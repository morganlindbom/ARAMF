#include "ConfigurationUpdateService.h"
#include "ProjectPersistence.h"
#include "AramfPaths.h"
#include "TemplateValidation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QCryptographicHash>
#include <algorithm>

namespace {
class WorkerNameScope final {
public:
    explicit WorkerNameScope(const QString& suffix)
        : previous_(AramfPaths::detail::workerSuffixOverride())
    { AramfPaths::setRuntimeWorkerNameSuffix(suffix); }
    ~WorkerNameScope() { AramfPaths::setRuntimeWorkerNameSuffix(previous_); }
private:
    QString previous_;
};

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

struct UpdateSnapshot {
    QString path;
    bool existed = false;
    QByteArray bytes;
    QByteArray digest;
};

bool safeParents(const QString& path)
{
    QFileInfo current(path);
    for (;;) {
        if (current.isSymLink()) return false;
        const auto parent = current.absolutePath();
        if (parent == current.absoluteFilePath()) break;
        current.setFile(parent);
    }
    return true;
}

bool capture(const QStringList& paths, QList<UpdateSnapshot>* snapshots, QString* error)
{
    for (const auto& path : paths) {
        const QFileInfo info(path);
        if (!safeParents(path) || (info.exists() && !info.isFile())) {
            *error = QStringLiteral("Update requires regular, non-linked targets: %1").arg(path); return false;
        }
        UpdateSnapshot snapshot{path, info.exists(), {}, {}};
        if (snapshot.existed) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) { *error = file.errorString(); return false; }
            snapshot.bytes = file.readAll();
            if (file.error() != QFileDevice::NoError || snapshot.bytes.size() != info.size() || file.size() != info.size()) {
                *error = QStringLiteral("Incomplete update snapshot: %1").arg(path); return false;
            }
        }
        snapshot.digest = QCryptographicHash::hash(snapshot.bytes, QCryptographicHash::Sha256);
        snapshots->append(snapshot);
    }
    return true;
}

bool rollback(const QList<UpdateSnapshot>& snapshots, QString* error)
{
    QStringList failures;
    for (const auto& s : snapshots) {
        if (!safeParents(s.path) || QCryptographicHash::hash(s.bytes, QCryptographicHash::Sha256) != s.digest) {
            failures.append(s.path); continue;
        }
        if (!s.existed) {
            // Remove only an exact, declared transaction-created file, never
            // directories, user content or a recursively discovered path.
            if (QFileInfo::exists(s.path) && (!QFileInfo(s.path).isFile() || !QFile::remove(s.path))) failures.append(s.path);
            continue;
        }
        QFile current(s.path);
        if (current.open(QIODevice::ReadOnly)) {
            const auto bytes = current.readAll();
            const bool identical = current.error() == QFileDevice::NoError && bytes == s.bytes;
            current.close();
            if (identical) continue;
        }
        QSaveFile restored(s.path);
        if (!restored.open(QIODevice::WriteOnly) || restored.write(s.bytes) != s.bytes.size() || !restored.commit()) {
            failures.append(s.path); continue;
        }
        QFile readback(s.path);
        if (!readback.open(QIODevice::ReadOnly)) { failures.append(s.path); continue; }
        const auto bytes = readback.readAll();
        if (readback.error() != QFileDevice::NoError || bytes != s.bytes
            || QCryptographicHash::hash(bytes, QCryptographicHash::Sha256) != s.digest) failures.append(s.path);
    }
    if (!failures.isEmpty()) *error += QStringLiteral("\nROLLBACK FAILED: %1").arg(failures.join(", "));
    return failures.isEmpty();
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
    return analyze(model, true);
}

ConfigurationUpdateResult ConfigurationUpdateService::analyze(const ProjectModel& model, bool persistPlan) const
{
    WorkerNameScope workerScope(model.workerNameSuffix());
    ConfigurationUpdateResult result;
    const QString root = QDir::cleanPath(model.projectPath());
    const QString workerName = AramfPaths::workerDirectoryName(model.workerNameSuffix());
    const QString worker = QDir(root).filePath(workerName);
    if (root.isEmpty() || root == QStringLiteral(".") || !QDir(worker).exists()) {
        result.error = QStringLiteral("An existing %1 is required.").arg(workerName); return result;
    }
    if (QFileInfo(worker).canonicalFilePath() != QDir(QFileInfo(root).canonicalFilePath()).filePath(workerName)) {
        result.error = QStringLiteral("Worker directory must belong directly to the selected project root."); return result;
    }
    QString error;
    const auto state = readObject(QDir(worker).filePath(QStringLiteral("verification/generation-state.json")), &error);
    if (state.isEmpty() || !state.contains(QStringLiteral("canonicalState"))) {
        result.error = QStringLiteral("LEGACY GENERATED STATE: BASELINE UNAVAILABLE: REGENERATION REQUIRED."); return result;
    }
    const auto previous = state.value(QStringLiteral("canonicalState")).toObject();
    if (previous.value(QStringLiteral("projectId")).toString() != model.projectId()
        || AramfPaths::workerDirectoryName(previous.value(QStringLiteral("workerNameSuffix")).toString()) != workerName
        || (state.contains(QStringLiteral("projectRoot"))
            && QFileInfo(state.value(QStringLiteral("projectRoot")).toString()).canonicalFilePath() != QFileInfo(root).canonicalFilePath())) {
        result.error = QStringLiteral("Existing worker baseline does not match the selected project and worker identity."); return result;
    }
    const QString manifestPath = QDir(worker).filePath(QStringLiteral("worker-manifest.json"));
    if (QFileInfo::exists(manifestPath)) {
        const auto manifest = readObject(manifestPath, &error);
        if (manifest.value(QStringLiteral("projectId")).toString() != model.projectId()
            || manifest.value(QStringLiteral("workerIdentity")).toString() != workerName) {
            result.error = QStringLiteral("Worker manifest identity conflicts with the selected project."); return result;
        }
    }
    const auto current = canonicalState(model);
    QJsonArray added, modified, removed, unchanged;
    diffValue(previous, current, QString(), &added, &modified, &removed, &unchanged);
    result.removalBlocked = !removed.isEmpty();
    result.noChange = added.isEmpty() && modified.isEmpty() && removed.isEmpty();
    const QString currentFingerprint = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(current).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    result.plan = QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("status"), result.removalBlocked ? QStringLiteral("REMOVAL_BLOCKED") : QStringLiteral("VALID")},
        {QStringLiteral("added"), added}, {QStringLiteral("modified"), modified}, {QStringLiteral("removalBlocked"), removed}, {QStringLiteral("unchanged"), unchanged},
        {QStringLiteral("currentState"), current}, {QStringLiteral("baselineState"), previous}, {QStringLiteral("validationFingerprint"), currentFingerprint}};
    if (persistPlan && (!QDir(worker).mkpath(QStringLiteral("update"))
        || !writeObject(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::UpdatePlan)), result.plan, &error))) {
        result.success = false;
        result.error = QStringLiteral("Could not write update plan: %1").arg(error);
        return result;
    }
    result.success = true;
    return result;
}

ConfigurationUpdateResult ConfigurationUpdateService::apply(const ProjectModel& model, const QString& validationFingerprint) const
{
    WorkerNameScope workerScope(model.workerNameSuffix());
    // Analysis here is read-only: even a rejected request must not replace
    // the previous update-plan or verification artifacts.
    auto result = analyze(model, false);
    if (!validationFingerprint.isEmpty() && result.plan.value(QStringLiteral("validationFingerprint")).toString() != validationFingerprint) {
        result.success = false; result.error = QStringLiteral("Validation is stale; validate the current project configuration again."); return result;
    }
    if (!result.success || result.removalBlocked) {
        result.success = false;
        if (result.removalBlocked) result.error = QStringLiteral("Removal detected - currently blocked.");
        return result;
    }
    result.success = false;
    const auto readiness = TemplateValidation::readiness(model);
    if (!readiness.isEmpty()) { result.error = readiness.join('\n'); return result; }
    QString error;
    ProjectPersistence persistence;
    ProjectModel prior;
    const QString configPath = QFileInfo(model.projectFilePath()).absoluteFilePath();
    if (model.projectFilePath().isEmpty() || !persistence.load(&prior, configPath, &error)
        || prior.projectId() != model.projectId() || prior.workerNameSuffix() != model.workerNameSuffix()
        || QFileInfo(prior.projectPath()).canonicalFilePath() != QFileInfo(model.projectPath()).canonicalFilePath()) {
        result.error = QStringLiteral("Update requires the saved configuration of this exact project/worker: %1").arg(error); return result;
    }
    const auto options = model.generationOptions();
    auto relativeTargets = GenerationServices::configurationUpdateFiles(model, options);
    QStringList paths;
    for (const auto& path : relativeTargets) paths.append(QDir(model.projectPath()).absoluteFilePath(path));
    if (paths.contains(configPath)) { result.error = QStringLiteral("Project persistence must be distinct from derived worker files."); return result; }
    paths.prepend(configPath);
    QList<UpdateSnapshot> snapshots;
    if (!capture(paths, &snapshots, &error)) { result.error = error; return result; }
    auto failed = [&](const QString& message) {
        result.error = message;
        result.rolledBack = rollback(snapshots, &result.error);
        result.configurationSaved = result.derivedSynchronized = result.verified = false;
        return result;
    };
    if (!persistence.save(model, configPath, &error)) return failed(QStringLiteral("Configuration save failed: %1").arg(error));
    result.configurationSaved = true;
    const auto generated = GenerationServices().regenerateConfiguration(model, options);
    if (!generated.success) return failed(generated.error);
    ProjectModel reloaded;
    if (!persistence.load(&reloaded, configPath, &error) || canonicalState(reloaded) != canonicalState(model)
        || QFileInfo(reloaded.projectPath()).canonicalFilePath() != QFileInfo(model.projectPath()).canonicalFilePath())
        return failed(QStringLiteral("Saved configuration readback failed: %1").arg(error));
    const auto expected = GenerationServices::derivedTaskArtifacts(reloaded, options);
    for (auto it = expected.begin(); it != expected.end(); ++it) {
        auto actual = readObject(QDir(model.projectPath()).filePath(it.key()), &error);
        auto wanted = it.value().toObject(); actual.remove("_file"); wanted.remove("_file");
        if (actual != wanted) return failed(QStringLiteral("Derived configuration readback mismatch: %1").arg(it.key()));
    }
    const auto state = readObject(QDir(model.projectPath()).filePath(AramfPaths::resolveWorkerRelativePath("ARAMF_WORKER/verification/generation-state.json")), &error);
    if (state.value("canonicalState").toObject() != canonicalState(reloaded)
        || state.value("configurationArtifactHashes").toObject().isEmpty()) return failed(QStringLiteral("Canonical generation binding missing or stale."));
    result.derivedSynchronized = true;
    const auto verification = VerificationServices().verify(reloaded, options);
    if (verification.overallStatus != VerificationStatus::Pass) {
        return failed(QStringLiteral("Updated worker configuration did not pass final verification."));
    }
    // Persistence errors in the verifier must not be mistaken for in-memory
    // PASS. Both current verification artifacts are mandatory readback.
    auto verifyDisk = readObject(QDir(model.projectPath()).filePath(AramfPaths::resolveWorkerRelativePath("ARAMF_WORKER/verification/verification-result.json")), &error);
    auto summaryDisk = readObject(QDir(model.projectPath()).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::LatestValidation)), &error);
    verifyDisk.remove("_file"); verifyDisk.remove("checkedAt"); summaryDisk.remove("_file");
    if (verifyDisk != verification.evidence || summaryDisk != verification.summary)
        return failed(QStringLiteral("Verification persistence/readback failed."));
    result.verified = true;
    result.success = true;
    return result;
}
