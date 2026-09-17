#include "ConfigurationUpdateService.h"
#include "ProjectPersistence.h"
#include "AramfPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QCryptographicHash>
#include <algorithm>

namespace {
QJsonObject readObject(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { if (error) *error = file.errorString(); return {}; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) { if (error) *error = parseError.errorString(); return {}; }
    return document.object();
}

bool writeObject(const QString& path, const QJsonObject& object, QString* error)
{
    QSaveFile file(path);
    const auto data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) *error = file.errorString(); return false;
    }
    return true;
}

QStringList sortedKeys(const QJsonObject& object) { auto keys = object.keys(); std::sort(keys.begin(), keys.end()); return keys; }

QString pointerPart(const QString& value) { QString escaped = value; escaped.replace('~', "~0"); escaped.replace('/', "~1"); return escaped; }
QString valueKey(const QJsonValue& value) { return QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact)); }
void change(QJsonArray* target, const QString& operation, const QString& path, const QJsonValue& before, const QJsonValue& after)
{
    target->append(QJsonObject{{"operation", operation}, {"path", path}, {"previous", before}, {"current", after}});
}
QString identity(const QJsonObject& object)
{
    for (const auto& key : {QStringLiteral("id"), QStringLiteral("key"), QStringLiteral("identifier"), QStringLiteral("name")})
        if (object.contains(key) && object.value(key).isString() && !object.value(key).toString().isEmpty()) return key + QStringLiteral("=") + object.value(key).toString();
    return {};
}
void diffValue(const QJsonValue& before, const QJsonValue& after, const QString& path,
              QJsonArray* added, QJsonArray* modified, QJsonArray* removed, QJsonArray* unchanged)
{
    if (before == after) { unchanged->append(path); return; }
    if (before.isObject() && after.isObject()) {
        QSet<QString> keys;
        for (const auto& key : before.toObject().keys()) keys.insert(key);
        for (const auto& key : after.toObject().keys()) keys.insert(key);
        auto ordered = keys.values(); std::sort(ordered.begin(), ordered.end());
        for (const auto& key : ordered) {
            const QString child = path + QStringLiteral("/") + pointerPart(key);
            if (!before.toObject().contains(key)) change(added, QStringLiteral("ADD"), child, QJsonValue(), after.toObject().value(key));
            else if (!after.toObject().contains(key)) change(removed, QStringLiteral("REMOVE"), child, before.toObject().value(key), QJsonValue());
            else diffValue(before.toObject().value(key), after.toObject().value(key), child, added, modified, removed, unchanged);
        }
        return;
    }
    if (before.isArray() && after.isArray()) {
        const auto oldArray = before.toArray(), newArray = after.toArray();
        QMap<QString, QJsonObject> oldObjects, newObjects;
        bool identifiable = !oldArray.isEmpty() || !newArray.isEmpty();
        for (const auto& value : oldArray) { if (!value.isObject() || identity(value.toObject()).isEmpty()) { identifiable = false; break; } oldObjects.insert(identity(value.toObject()), value.toObject()); }
        for (const auto& value : newArray) { if (!value.isObject() || identity(value.toObject()).isEmpty()) { identifiable = false; break; } newObjects.insert(identity(value.toObject()), value.toObject()); }
        if (identifiable) {
            QSet<QString> ids; for (auto it = oldObjects.cbegin(); it != oldObjects.cend(); ++it) ids.insert(it.key()); for (auto it = newObjects.cbegin(); it != newObjects.cend(); ++it) ids.insert(it.key());
            auto ordered = ids.values(); std::sort(ordered.begin(), ordered.end());
            for (const auto& id : ordered) {
                const QString child = path + QStringLiteral("/") + pointerPart(id.mid(id.indexOf('=') + 1));
                if (!oldObjects.contains(id)) change(added, QStringLiteral("ADD"), child, QJsonValue(), newObjects.value(id));
                else if (!newObjects.contains(id)) change(removed, QStringLiteral("REMOVE"), child, oldObjects.value(id), QJsonValue());
                else diffValue(oldObjects.value(id), newObjects.value(id), child, added, modified, removed, unchanged);
            }
            return;
        }
        // Primitive collections are set-like configuration values. Sorting
        // makes serialization order irrelevant while preserving ADD/REMOVE.
        for (const auto& value : oldArray) if (!newArray.contains(value)) change(removed, QStringLiteral("REMOVE"), path + QStringLiteral("/") + pointerPart(valueKey(value)), value, QJsonValue());
        for (const auto& value : newArray) if (!oldArray.contains(value)) change(added, QStringLiteral("ADD"), path + QStringLiteral("/") + pointerPart(valueKey(value)), QJsonValue(), value);
        bool sameSet = oldArray.size() == newArray.size();
        for (const auto& value : oldArray) sameSet = sameSet && newArray.contains(value);
        if (sameSet) unchanged->append(path);
        return;
    }
    change(modified, QStringLiteral("MODIFY"), path, before, after);
}
}

ConfigurationUpdateService::ConfigurationUpdateService(QObject* parent) : QObject(parent) {}

QJsonObject ConfigurationUpdateService::canonicalState(const ProjectModel& model)
{
    // Use the complete persistence serializer so new ProjectModel fields are
    // automatically included. Only storage location and UI progress are not
    // generated configuration.
    auto state = ProjectPersistence().toJson(model);
    state.remove(QStringLiteral("projectPath"));
    state.remove(QStringLiteral("projectFilePath"));
    state.remove(QStringLiteral("workflowProgress"));
    return state;
}

ConfigurationUpdateResult ConfigurationUpdateService::validate(const ProjectModel& model) const
{
    ConfigurationUpdateResult result;
    const QString root = QDir::cleanPath(model.projectPath());
    const QString worker = QDir(root).filePath(QStringLiteral("ARAMF_WORKER"));
    if (root.isEmpty() || root == QStringLiteral(".") || !QDir(worker).exists()) {
        result.error = QStringLiteral("An existing ARAMF_WORKER is required."); return result;
    }
    QString error;
    QDir(worker).mkpath(QStringLiteral("update"));
    const auto state = readObject(QDir(worker).filePath(QStringLiteral("verification/generation-state.json")), &error);
    if (state.isEmpty() || !state.contains(QStringLiteral("canonicalState"))) {
        result.error = QStringLiteral("LEGACY GENERATED STATE: BASELINE UNAVAILABLE: REGENERATION REQUIRED."); return result;
    }
    const auto previous = state.value(QStringLiteral("canonicalState")).toObject();
    const auto current = canonicalState(model);
    QJsonArray added, modified, removed, unchanged;
    diffValue(previous, current, QString(), &added, &modified, &removed, &unchanged);
    result.removalBlocked = !removed.isEmpty();
    result.noChange = added.isEmpty() && modified.isEmpty() && removed.isEmpty();
    const QString currentFingerprint = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(current).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    result.plan = QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("status"), result.removalBlocked ? QStringLiteral("REMOVAL_BLOCKED") : QStringLiteral("VALID")},
        {QStringLiteral("added"), added}, {QStringLiteral("modified"), modified}, {QStringLiteral("removalBlocked"), removed}, {QStringLiteral("unchanged"), unchanged},
        {QStringLiteral("currentState"), current}, {QStringLiteral("baselineState"), previous}, {QStringLiteral("validationFingerprint"), currentFingerprint}};
    if (!writeObject(QDir(root).filePath(AramfPaths::UpdatePlan), result.plan, &error)) {
        result.success = false;
        result.error = QStringLiteral("Could not write update plan: %1").arg(error);
        return result;
    }
    result.success = true;
    return result;
}

ConfigurationUpdateResult ConfigurationUpdateService::apply(const ProjectModel& model, const QString& validationFingerprint) const
{
    auto result = validate(model);
    if (!validationFingerprint.isEmpty() && result.plan.value(QStringLiteral("validationFingerprint")).toString() != validationFingerprint) {
        result.success = false; result.error = QStringLiteral("Validation is stale; validate the current project configuration again."); return result;
    }
    if (!result.success || result.removalBlocked) { if (result.removalBlocked) result.error = QStringLiteral("Removal detected - currently blocked."); return result; }
    if (result.noChange) return result;
    GenerationOptions all; all.generateAgentRules = all.generateRouting = all.generatePlatforms = true;
    all.generateResources = all.generateMemory = all.generateProvenance = true;
    const auto generated = GenerationServices().repairDerivedArtifacts(model, all);
    if (!generated.success) { result.success = false; result.error = generated.error; return result; }
    QString error;
    const auto statePath = QDir(model.projectPath()).filePath(QStringLiteral("ARAMF_WORKER/verification/generation-state.json"));
    QJsonObject state{{QStringLiteral("fingerprint"), projectConfigurationFingerprint(model, all)},
        {QStringLiteral("canonicalState"), canonicalState(model)},
        {QStringLiteral("projectRoot"), QDir::cleanPath(model.projectPath())}};
    if (!writeObject(statePath, state, &error)) { result.success = false; result.error = QStringLiteral("Baseline commit failed: %1").arg(error); return result; }
    result.success = true;
    return result;
}
