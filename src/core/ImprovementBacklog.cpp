#include "ImprovementBacklog.h"

#include "AramfPaths.h"
#include "ProjectModel.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>

namespace {
QString pathOverride;
bool writeFailureForTests = false;

QString now()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QString normalized(const QString& value)
{
    return value.simplified().toCaseFolded();
}

QStringList uniqueStrings(const QJsonArray& values)
{
    QStringList result;
    for (const auto& value : values) {
        const auto text = value.toString().trimmed();
        if (!text.isEmpty() && !result.contains(text)) result.append(text);
    }
    return result;
}

QJsonArray array(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

QJsonObject emptyStore()
{
    return QJsonObject{{QStringLiteral("_file"), QStringLiteral("aramf-improvement-backlog.json")},
                       {QStringLiteral("version"), 1},
                       {QStringLiteral("nextTodoNumber"), 1},
                       {QStringLiteral("items"), QJsonArray{}}};
}

ImprovementBacklogProjectIdentity readProjectIdentity(const QString& projectRoot)
{
    ImprovementBacklogProjectIdentity identity;
    identity.projectPath = QDir::cleanPath(QFileInfo(projectRoot).absoluteFilePath());
    QFile profile(QDir(projectRoot).filePath(AramfPaths::Profile));
    if (profile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError parseError;
        const auto object = QJsonDocument::fromJson(profile.readAll(), &parseError).object();
        identity.projectId = object.value(QStringLiteral("projectId")).toString().trimmed();
        identity.projectName = object.value(QStringLiteral("projectName")).toString().trimmed();
    }
    if (identity.projectName.isEmpty()) identity.projectName = QFileInfo(identity.projectPath).fileName();
    return identity;
}

QString stableGapIdentity(const QString& title, const QString& observation, const QString& expected, const QString& area)
{
    const auto source = normalized(title) + QChar('\n') + normalized(observation) + QChar('\n')
        + normalized(expected) + QChar('\n') + normalized(area);
    const auto digest = QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha256).toHex().left(20);
    return QStringLiteral("gap-%1").arg(QString::fromLatin1(digest));
}

QJsonObject occurrence(const ImprovementBacklogProjectIdentity& identity, const QString& task, const QString& agent,
                       const QString& summary, const QStringList& evidence)
{
    return QJsonObject{{QStringLiteral("projectId"), identity.projectId},
                       {QStringLiteral("projectName"), identity.projectName},
                       {QStringLiteral("projectPath"), identity.projectPath},
                       {QStringLiteral("observedAt"), now()},
                       {QStringLiteral("agentId"), agent},
                       {QStringLiteral("task"), task},
                       {QStringLiteral("summary"), summary},
                       {QStringLiteral("evidence"), array(evidence)}};
}

bool sameProjectPath(const QJsonObject& object, const ImprovementBacklogProjectIdentity& identity)
{
    const auto equivalent = [](const QString& left, const QString& right) {
        if (left.trimmed().isEmpty() || right.trimmed().isEmpty()) return false;
        const auto normalizedLeft = QDir::cleanPath(QFileInfo(left).absoluteFilePath());
        const auto normalizedRight = QDir::cleanPath(QFileInfo(right).absoluteFilePath());
        return QDir::fromNativeSeparators(normalizedLeft).compare(
            QDir::fromNativeSeparators(normalizedRight), Qt::CaseInsensitive) == 0;
    };
    return equivalent(object.value(QStringLiteral("projectPath")).toString(), identity.projectPath)
        || equivalent(object.value(QStringLiteral("projectId")).toString(), identity.projectPath);
}

bool normalizeIdentityObject(QJsonObject& object, const ImprovementBacklogProjectIdentity& identity)
{
    if (!sameProjectPath(object, identity))
        return false;
    bool changed = false;
    if (object.value(QStringLiteral("projectId")).toString() != identity.projectId) {
        object.insert(QStringLiteral("projectId"), identity.projectId);
        changed = true;
    }
    if (object.value(QStringLiteral("projectName")).toString() != identity.projectName) {
        object.insert(QStringLiteral("projectName"), identity.projectName);
        changed = true;
    }
    if (object.value(QStringLiteral("projectPath")).toString() != identity.projectPath) {
        object.insert(QStringLiteral("projectPath"), identity.projectPath);
        changed = true;
    }
    return changed;
}
}

ImprovementBacklogService::ImprovementBacklogService(QObject* parent)
    : QObject(parent)
{
}

void ImprovementBacklogService::setPathForTests(const QString& path)
{
    pathOverride = QDir::cleanPath(path);
}

void ImprovementBacklogService::clearPathForTests()
{
    pathOverride.clear();
}

void ImprovementBacklogService::setWriteFailureForTests(bool enabled)
{
    writeFailureForTests = enabled;
}

QString ImprovementBacklogService::backlogPath() const
{
    if (!pathOverride.isEmpty()) return pathOverride;
    return QDir(AramfPaths::programRoot()).filePath(QStringLiteral("ARAMF_DATA/aramf-improvement-backlog.json"));
}

bool ImprovementBacklogService::ensure(QString* error) const
{
    if (QFileInfo::exists(backlogPath())) return true;
    if (!QDir().mkpath(QFileInfo(backlogPath()).absolutePath())) {
        if (error) *error = QStringLiteral("Cannot create ARAMF improvement backlog directory: %1").arg(QFileInfo(backlogPath()).absolutePath());
        return false;
    }
    return write(emptyStore(), error);
}

bool ImprovementBacklogService::read(QJsonObject* store, QString* error) const
{
    if (!QFileInfo::exists(backlogPath())) {
        *store = emptyStore();
        return true;
    }
    QFile file(backlogPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().value(QStringLiteral("version")).toInt() != 1
        || !document.object().value(QStringLiteral("items")).isArray()) {
        if (error) *error = QStringLiteral("ARAMF improvement backlog is malformed or unsupported; history was not replaced.");
        return false;
    }
    *store = document.object();
    return true;
}

bool ImprovementBacklogService::write(const QJsonObject& store, QString* error) const
{
    if (writeFailureForTests) {
        if (error) *error = QStringLiteral("Test write failure: backlog was not persisted.");
        return false;
    }
    if (!QDir().mkpath(QFileInfo(backlogPath()).absolutePath())) {
        if (error) *error = QStringLiteral("Cannot create ARAMF improvement backlog directory: %1").arg(QFileInfo(backlogPath()).absolutePath());
        return false;
    }
    QSaveFile file(backlogPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const auto data = QJsonDocument(store).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QList<QJsonObject> ImprovementBacklogService::items(QString* error) const
{
    QJsonObject store;
    if (!read(&store, error)) return {};
    QList<QJsonObject> result;
    for (const auto& value : store.value(QStringLiteral("items")).toArray()) result.append(value.toObject());
    return result;
}

bool ImprovementBacklogService::report(const QString& projectRoot, const QString& title,
                                       const QString& observation, const QString& expected,
                                       const QString& area, const QStringList& evidence,
                                       const QString& task, const QString& agent,
                                       QJsonObject* result, QString* error) const
{
    return reportWithIdentity(projectIdentity(projectRoot), title, observation, expected, area,
                              evidence, task, agent, result, error);
}

ImprovementBacklogProjectIdentity ImprovementBacklogService::projectIdentity(const QString& projectRoot) const
{
    return readProjectIdentity(projectRoot);
}

bool ImprovementBacklogService::reportWithIdentity(const ImprovementBacklogProjectIdentity& identity,
                                       const QString& title, const QString& observation,
                                       const QString& expected, const QString& area,
                                       const QStringList& evidence, const QString& task,
                                       const QString& agent, QJsonObject* result, QString* error) const
{
    if (identity.projectPath.trimmed().isEmpty() || !QDir(identity.projectPath).exists()) {
        if (error) *error = QStringLiteral("Managed project path is required and must exist.");
        return false;
    }
    if (title.trimmed().isEmpty() || observation.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Title and observation are required.");
        return false;
    }
    QJsonObject store;
    if (!read(&store, error)) return false;
    const auto id = stableGapIdentity(title, observation, expected, area);
    auto values = store.value(QStringLiteral("items")).toArray();
    int index = -1;
    for (int i = 0; i < values.size(); ++i) if (values.at(i).toObject().value(QStringLiteral("id")).toString() == id) { index = i; break; }
    const auto observed = occurrence(identity, task, agent, observation, evidence);
    if (index < 0) {
        const auto timestamp = now();
        values.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("todoId"), QString()},
                                  {QStringLiteral("stage"), QStringLiteral("observation")}, {QStringLiteral("status"), QStringLiteral("open")},
                                  {QStringLiteral("title"), title.trimmed()}, {QStringLiteral("observation"), observation.trimmed()},
                                  {QStringLiteral("frameworkArea"), area.trimmed()}, {QStringLiteral("severity"), QStringLiteral("unknown")},
                                  {QStringLiteral("priority"), QStringLiteral("unassigned")},
                                  {QStringLiteral("originProjects"), QJsonArray{QJsonObject{{QStringLiteral("projectId"), identity.projectId}, {QStringLiteral("projectName"), identity.projectName}, {QStringLiteral("projectPath"), identity.projectPath}}}},
                                  {QStringLiteral("occurrences"), QJsonArray{observed}}, {QStringLiteral("evidence"), array(evidence)},
                                  {QStringLiteral("expectedFrameworkBehavior"), expected.trimmed()}, {QStringLiteral("suggestedDirection"), QString()},
                                  {QStringLiteral("duplicateOf"), QString()}, {QStringLiteral("relatedItems"), QJsonArray{}},
                                  {QStringLiteral("createdAt"), timestamp}, {QStringLiteral("updatedAt"), timestamp},
                                  {QStringLiteral("triagedAt"), QString()}, {QStringLiteral("triagedBy"), QString()},
                                  {QStringLiteral("implementedAt"), QString()}, {QStringLiteral("validatedAt"), QString()}, {QStringLiteral("completedAt"), QString()}});
        if (result) *result = QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("outcome"), QStringLiteral("NEW")}};
    } else {
        auto item = values.at(index).toObject();
        auto origins = item.value(QStringLiteral("originProjects")).toArray();
        for (int i = 0; i < origins.size(); ++i) {
            auto origin = origins.at(i).toObject();
            if (normalizeIdentityObject(origin, identity)) origins[i] = origin;
        }
        item.insert(QStringLiteral("originProjects"), origins);
        auto occurrences = item.value(QStringLiteral("occurrences")).toArray();
        for (int i = 0; i < occurrences.size(); ++i) {
            auto prior = occurrences.at(i).toObject();
            if (normalizeIdentityObject(prior, identity)) occurrences[i] = prior;
        }
        occurrences.append(observed);
        item.insert(QStringLiteral("occurrences"), occurrences);
        auto projects = origins;
        const auto pid = identity.projectId;
        bool known = false;
        for (const auto& project : projects) if (project.toObject().value(QStringLiteral("projectId")).toString() == pid) known = true;
        if (!known) projects.append(QJsonObject{{QStringLiteral("projectId"), pid}, {QStringLiteral("projectName"), identity.projectName}, {QStringLiteral("projectPath"), identity.projectPath}});
        item.insert(QStringLiteral("originProjects"), projects);
        QStringList mergedEvidence = uniqueStrings(item.value(QStringLiteral("evidence")).toArray());
        for (const auto& evidenceValue : evidence) if (!mergedEvidence.contains(evidenceValue)) mergedEvidence.append(evidenceValue);
        item.insert(QStringLiteral("evidence"), array(mergedEvidence));
        item.insert(QStringLiteral("updatedAt"), now());
        values[index] = item;
        if (result) *result = QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("outcome"), QStringLiteral("EXISTING_OCCURRENCE_APPENDED")}};
    }
    store.insert(QStringLiteral("items"), values);
    return write(store, error);
}

bool ImprovementBacklogService::normalizeProjectIdentity(const ImprovementBacklogProjectIdentity& identity,
                                                          QString* error) const
{
    if (identity.projectPath.trimmed().isEmpty() || !QDir(identity.projectPath).exists()) {
        if (error) *error = QStringLiteral("Managed project path is required and must exist.");
        return false;
    }
    QJsonObject store;
    if (!read(&store, error)) return false;
    auto values = store.value(QStringLiteral("items")).toArray();
    bool changed = false;
    for (int itemIndex = 0; itemIndex < values.size(); ++itemIndex) {
        auto item = values.at(itemIndex).toObject();
        auto origins = item.value(QStringLiteral("originProjects")).toArray();
        for (int i = 0; i < origins.size(); ++i) { auto value = origins.at(i).toObject(); if (normalizeIdentityObject(value, identity)) { origins[i] = value; changed = true; } }
        item.insert(QStringLiteral("originProjects"), origins);
        auto occurrences = item.value(QStringLiteral("occurrences")).toArray();
        for (int i = 0; i < occurrences.size(); ++i) { auto value = occurrences.at(i).toObject(); if (normalizeIdentityObject(value, identity)) { occurrences[i] = value; changed = true; } }
        item.insert(QStringLiteral("occurrences"), occurrences);
        values[itemIndex] = item;
    }
    if (!changed) return true;
    store.insert(QStringLiteral("items"), values);
    return write(store, error);
}

bool ImprovementBacklogService::triage(const QString& id, const QString& action,
                                       const QString& duplicateOf, const QString& priority,
                                       QString* error) const
{
    QJsonObject store;
    if (!read(&store, error)) return false;
    auto values = store.value(QStringLiteral("items")).toArray();
    int index = -1;
    for (int i = 0; i < values.size(); ++i) if (values.at(i).toObject().value(QStringLiteral("id")).toString() == id) { index = i; break; }
    if (index < 0) { if (error) *error = QStringLiteral("Backlog item not found: %1").arg(id); return false; }
    auto item = values.at(index).toObject();
    const auto timestamp = now();
    const auto normalizedAction = action.trimmed().toLower();
    if (normalizedAction == QStringLiteral("promote") || normalizedAction == QStringLiteral("todo")) {
        int maxTodo = 0;
        for (const auto& value : values) {
            const auto todoId = value.toObject().value(QStringLiteral("todoId")).toString();
            if (todoId.startsWith(QStringLiteral("TODO-"))) maxTodo = qMax(maxTodo, todoId.mid(5).toInt());
        }
        int nextTodo = qMax(store.value(QStringLiteral("nextTodoNumber")).toInt(0), maxTodo + 1);
        if (nextTodo < 1) nextTodo = 1;
        item.insert(QStringLiteral("stage"), QStringLiteral("todo"));
        item.insert(QStringLiteral("todoId"), QStringLiteral("TODO-%1").arg(nextTodo, 3, 10, QChar('0')));
        item.insert(QStringLiteral("status"), QStringLiteral("OPEN"));
        store.insert(QStringLiteral("nextTodoNumber"), nextTodo + 1);
    } else if (normalizedAction == QStringLiteral("project-specific")) {
        item.insert(QStringLiteral("stage"), QStringLiteral("project-specific")); item.insert(QStringLiteral("status"), QStringLiteral("closed"));
    } else if (normalizedAction == QStringLiteral("duplicate")) {
        if (duplicateOf.isEmpty() || duplicateOf == id) { if (error) *error = QStringLiteral("A different duplicate target is required."); return false; }
        item.insert(QStringLiteral("stage"), QStringLiteral("duplicate")); item.insert(QStringLiteral("status"), QStringLiteral("closed")); item.insert(QStringLiteral("duplicateOf"), duplicateOf);
    } else if (normalizedAction == QStringLiteral("needs-evidence")) {
        item.insert(QStringLiteral("stage"), QStringLiteral("observation")); item.insert(QStringLiteral("status"), QStringLiteral("needs-more-evidence"));
    } else if (normalizedAction == QStringLiteral("already-resolved")) {
        item.insert(QStringLiteral("stage"), QStringLiteral("already-resolved")); item.insert(QStringLiteral("status"), QStringLiteral("completed"));
    } else if (normalizedAction == QStringLiteral("reject")) {
        item.insert(QStringLiteral("stage"), QStringLiteral("rejected")); item.insert(QStringLiteral("status"), QStringLiteral("closed"));
    } else { if (error) *error = QStringLiteral("Unsupported triage action: %1").arg(action); return false; }
    if (!priority.isEmpty()) item.insert(QStringLiteral("priority"), priority.toLower());
    item.insert(QStringLiteral("triagedAt"), timestamp);
    item.insert(QStringLiteral("updatedAt"), timestamp);
    values[index] = item;
    store.insert(QStringLiteral("items"), values);
    return write(store, error);
}

bool ImprovementBacklogService::setStatus(const QString& id, const QString& status, QString* error) const
{
    QJsonObject store;
    if (!read(&store, error)) return false;
    auto values = store.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < values.size(); ++i) {
        auto item = values.at(i).toObject();
        if (item.value(QStringLiteral("id")).toString() != id) continue;
        const auto next = status.toUpper();
        if (next == QStringLiteral("COMPLETED") && item.value(QStringLiteral("status")).toString() != QStringLiteral("VALIDATED")) {
            if (error) *error = QStringLiteral("COMPLETED requires VALIDATED status.");
            return false;
        }
        item.insert(QStringLiteral("status"), next);
        item.insert(QStringLiteral("updatedAt"), now());
        if (next == QStringLiteral("IMPLEMENTED")) item.insert(QStringLiteral("implementedAt"), now());
        if (next == QStringLiteral("VALIDATED")) item.insert(QStringLiteral("validatedAt"), now());
        if (next == QStringLiteral("COMPLETED")) item.insert(QStringLiteral("completedAt"), now());
        values[i] = item;
        store.insert(QStringLiteral("items"), values);
        return write(store, error);
    }
    if (error) *error = QStringLiteral("Backlog item not found: %1").arg(id);
    return false;
}

bool ImprovementBacklogService::removeItem(const QString& id, QString* error) const
{
    const auto stableId = id.trimmed();
    if (stableId.isEmpty()) {
        if (error) *error = QStringLiteral("A stable backlog item ID is required.");
        return false;
    }
    QJsonObject store;
    if (!read(&store, error)) return false;
    auto values = store.value(QStringLiteral("items")).toArray();
    int index = -1;
    for (int i = 0; i < values.size(); ++i) {
        if (values.at(i).toObject().value(QStringLiteral("id")).toString() == stableId) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        if (error) *error = QStringLiteral("Backlog item not found: %1").arg(stableId);
        return false;
    }
    values.removeAt(index);
    store.insert(QStringLiteral("items"), values);
    return write(store, error);
}
