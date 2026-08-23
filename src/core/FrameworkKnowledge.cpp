// FrameworkKnowledge.cpp

#include "FrameworkKnowledge.h"

#include "AramfPaths.h"
#include "ProjectModel.h"
#include "ProjectMemory.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {
QString globalLibraryPathOverride;

QJsonArray strings(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

QStringList strings(const QJsonArray& values)
{
    QStringList result;
    for (const auto& value : values) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty() && !result.contains(text)) result.append(text);
    }
    return result;
}

QJsonObject emptyStore()
{
    return QJsonObject{
        {QStringLiteral("_file"), QStringLiteral("framework-knowledge.json")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("authority"), QJsonArray{
            QStringLiteral("explicit-current-user-instruction"),
            QStringLiteral("current-source-of-truth"),
            QStringLiteral("current-durable-project-decisions"),
            QStringLiteral("approved-framework-knowledge"),
            QStringLiteral("templates-and-defaults"),
            QStringLiteral("ai-inference")}},
        {QStringLiteral("entries"), QJsonArray{}}
    };
}

QString storePath(const QString& projectRoot)
{
    return QDir(projectRoot).filePath(AramfPaths::FrameworkKnowledge);
}

QString libraryPath()
{
    if (!globalLibraryPathOverride.isEmpty()) return globalLibraryPathOverride;
    return QDir(AramfPaths::programRoot())
        .filePath(QStringLiteral("ARAMF_DATA/framework-knowledge-library.json"));
}

QString legacyLibraryPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("framework-knowledge-library.json"));
}

QString legacyExecutableLibraryPath()
{
    return QDir(AramfPaths::applicationDirectory())
        .filePath(QStringLiteral("ARAMF_DATA/framework-knowledge-library.json"));
}

QString migrationMarkerPath()
{
    return QDir(QFileInfo(libraryPath()).absolutePath())
        .filePath(QStringLiteral(".framework-knowledge-library-migrated.json"));
}

QJsonObject emptyLibrary()
{
    return QJsonObject{{QStringLiteral("_file"), QStringLiteral("framework-knowledge-library.json")},
                       {QStringLiteral("version"), 1}, {QStringLiteral("entries"), QJsonArray{}}};
}

bool readObjectFile(const QString& path, QJsonObject* value, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return false;
    }
    *value = document.object();
    return true;
}

bool writeObjectFile(const QString& path, const QJsonObject& value, QString* error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = QStringLiteral("Cannot create directory for Framework Knowledge library: %1")
                                .arg(QFileInfo(path).absolutePath());
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const auto data = QJsonDocument(value).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool readStore(const QString& projectRoot, QJsonObject* store, QString* error)
{
    QFile file(storePath(projectRoot));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return false;
    }
    *store = document.object();
    if (!store->value(QStringLiteral("entries")).isArray()) {
        if (error) *error = QStringLiteral("framework-knowledge.json has no valid entries array.");
        return false;
    }
    return true;
}

bool writeStore(const QString& projectRoot, const QJsonObject& store, QString* error)
{
    const QString path = storePath(projectRoot);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray data = QJsonDocument(store).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QJsonObject toJson(const FrameworkKnowledgeEntry& entry)
{
    return QJsonObject{
        {QStringLiteral("id"), entry.id},
        {QStringLiteral("title"), entry.title},
        {QStringLiteral("lesson"), entry.lesson},
        {QStringLiteral("status"), entry.status},
        {QStringLiteral("reviewStatus"), entry.reviewStatus},
        {QStringLiteral("scopes"), strings(entry.scopes)},
        {QStringLiteral("evidence"), strings(entry.evidence)},
        {QStringLiteral("portable"), entry.portable},
        {QStringLiteral("createdAt"), entry.createdAt},
        {QStringLiteral("approvedAt"), entry.approvedAt},
        {QStringLiteral("approvalSource"), entry.approvalSource},
        {QStringLiteral("supersededBy"), entry.supersededBy},
        {QStringLiteral("origin"), entry.origin},
        {QStringLiteral("originProjectId"), entry.originProjectId},
        {QStringLiteral("originalKnowledgeId"), entry.originalKnowledgeId},
        {QStringLiteral("promotedAt"), entry.promotedAt}
    };
}

FrameworkKnowledgeEntry fromJson(const QJsonObject& value)
{
    FrameworkKnowledgeEntry entry;
    entry.id = value.value(QStringLiteral("id")).toString();
    entry.title = value.value(QStringLiteral("title")).toString();
    entry.lesson = value.value(QStringLiteral("lesson")).toString();
    entry.status = value.value(QStringLiteral("status")).toString(QStringLiteral("candidate"));
    entry.reviewStatus = value.value(QStringLiteral("reviewStatus")).toString(
        entry.status == QStringLiteral("approved") ? QStringLiteral("approved")
        : entry.status == QStringLiteral("superseded") ? QStringLiteral("superseded")
        : QStringLiteral("more-evidence"));
    entry.scopes = strings(value.value(QStringLiteral("scopes")).toArray());
    entry.evidence = strings(value.value(QStringLiteral("evidence")).toArray());
    entry.portable = value.value(QStringLiteral("portable")).toBool(true);
    entry.createdAt = value.value(QStringLiteral("createdAt")).toString();
    entry.approvedAt = value.value(QStringLiteral("approvedAt")).toString();
    entry.approvalSource = value.value(QStringLiteral("approvalSource")).toString();
    entry.supersededBy = value.value(QStringLiteral("supersededBy")).toString();
    entry.origin = value.value(QStringLiteral("origin")).toString(QStringLiteral("project"));
    entry.originProjectId = value.value(QStringLiteral("originProjectId")).toString();
    entry.originalKnowledgeId = value.value(QStringLiteral("originalKnowledgeId")).toString();
    entry.promotedAt = value.value(QStringLiteral("promotedAt")).toString();
    return entry;
}

QString stableId(const QString& title, const QString& lesson, QStringList scopes)
{
    std::sort(scopes.begin(), scopes.end());
    const QByteArray source = (title.trimmed() + QLatin1Char('\n') + lesson.trimmed()
                               + QLatin1Char('\n') + scopes.join(QStringLiteral("|"))).toUtf8();
    return QStringLiteral("fk-%1").arg(QString::fromLatin1(
        QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex().left(16)));
}

bool scopeMatches(const FrameworkKnowledgeEntry& entry, const QStringList& requested)
{
    if (requested.isEmpty() || requested.contains(QStringLiteral("all")) || entry.scopes.isEmpty() || entry.scopes.contains(QStringLiteral("all"))) return true;
    for (const auto& scope : requested) if (entry.scopes.contains(scope)) return true;
    return false;
}

QList<FrameworkKnowledgeEntry> entriesFromStore(const QJsonObject& store)
{
    QList<FrameworkKnowledgeEntry> result;
    for (const auto& value : store.value(QStringLiteral("entries")).toArray()) result.append(fromJson(value.toObject()));
    return result;
}

QJsonObject storeFromEntries(const QList<FrameworkKnowledgeEntry>& entries, const QString& fileName)
{
    QJsonArray values;
    for (const auto& entry : entries) values.append(toJson(entry));
    return QJsonObject{{QStringLiteral("_file"), fileName}, {QStringLiteral("version"), 1}, {QStringLiteral("entries"), values}};
}

bool activeApplicable(const FrameworkKnowledgeEntry& entry, const QStringList& scopes)
{
    if (entry.status != QStringLiteral("approved") || entry.reviewStatus != QStringLiteral("approved") || !entry.supersededBy.isEmpty()) return false;
    return scopeMatches(entry, scopes);
}

int knowledgeStateRank(const FrameworkKnowledgeEntry& entry)
{
    if (entry.status == QStringLiteral("superseded") || !entry.supersededBy.isEmpty()) return 4;
    if (entry.status == QStringLiteral("approved")) return 3;
    if (entry.status == QStringLiteral("candidate")) return 2;
    return 1;
}

QList<FrameworkKnowledgeEntry> mergeLibraryEntries(const QList<FrameworkKnowledgeEntry>& primary,
                                                    const QList<FrameworkKnowledgeEntry>& secondary)
{
    QList<FrameworkKnowledgeEntry> merged = primary;
    for (const auto& incoming : secondary) {
        auto existing = std::find_if(merged.begin(), merged.end(), [&incoming](const auto& value) {
            return value.id == incoming.id;
        });
        if (existing == merged.end()) {
            merged.append(incoming);
            continue;
        }
        const bool replace = knowledgeStateRank(incoming) > knowledgeStateRank(*existing)
            || (knowledgeStateRank(incoming) == knowledgeStateRank(*existing)
                && existing->status != QStringLiteral("approved") && incoming.status == QStringLiteral("approved"));
        FrameworkKnowledgeEntry selected = replace ? incoming : *existing;
        for (const auto& evidence : existing->evidence) if (!selected.evidence.contains(evidence)) selected.evidence.append(evidence);
        for (const auto& evidence : incoming.evidence) if (!selected.evidence.contains(evidence)) selected.evidence.append(evidence);
        if (selected.originalKnowledgeId.isEmpty()) selected.originalKnowledgeId = existing->originalKnowledgeId;
        if (selected.originProjectId.isEmpty()) selected.originProjectId = existing->originProjectId;
        if (selected.promotedAt.isEmpty()) selected.promotedAt = existing->promotedAt;
        *existing = selected;
    }
    return merged;
}
}

FrameworkKnowledgeService::FrameworkKnowledgeService(QObject* parent)
    : QObject(parent)
{
    /**Construct the live Framework Knowledge service.

    The service stores candidates and approved reusable lessons inside the managed project's ARAMF memory.
    */
}

void FrameworkKnowledgeService::setGlobalLibraryPathForTests(const QString& path)
{
    globalLibraryPathOverride = QDir::cleanPath(path);
}

void FrameworkKnowledgeService::clearGlobalLibraryPathForTests()
{
    globalLibraryPathOverride.clear();
}

bool FrameworkKnowledgeService::ensureFile(const QString& projectRoot, QString* error) const
{
    /**Ensure that the project has a valid Framework Knowledge store.

    Existing knowledge is never replaced merely because initialization runs again.
    */
    const QString root = QDir::cleanPath(projectRoot.trimmed());
    if (root.isEmpty() || root == QStringLiteral(".")) {
        if (error) *error = QStringLiteral("Project path is empty.");
        return false;
    }
    const QString path = storePath(root);
    if (QFile::exists(path)) {
        QJsonObject existing;
        return readStore(root, &existing, error);
    }
    return writeStore(root, emptyStore(), error);
}

QString FrameworkKnowledgeService::propose(const QString& projectRoot,
                                            const QString& title,
                                            const QString& lesson,
                                            const QStringList& scopes,
                                            const QStringList& evidence,
                                            bool portable,
                                            QString* error) const
{
    /**Create or enrich one evidence-backed knowledge candidate.

    Proposing never approves knowledge; explicit user approval is a separate operation.
    */
    if (title.trimmed().isEmpty() || lesson.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Framework Knowledge candidates require a title and lesson.");
        return {};
    }
    if (!ensureFile(projectRoot, error)) return {};

    QJsonObject store;
    if (!readStore(projectRoot, &store, error)) return {};
    QJsonArray values = store.value(QStringLiteral("entries")).toArray();
    const QString id = stableId(title, lesson, scopes);
    for (qsizetype i = 0; i < values.size(); ++i) {
        QJsonObject value = values.at(i).toObject();
        if (value.value(QStringLiteral("id")).toString() != id) continue;
        QStringList mergedEvidence = strings(value.value(QStringLiteral("evidence")).toArray());
        for (const auto& item : evidence) if (!item.trimmed().isEmpty() && !mergedEvidence.contains(item.trimmed())) mergedEvidence.append(item.trimmed());
        value.insert(QStringLiteral("evidence"), strings(mergedEvidence));
        values.replace(i, value);
        store.insert(QStringLiteral("entries"), values);
        if (!writeStore(projectRoot, store, error)) return {};
        return id;
    }

    FrameworkKnowledgeEntry entry;
    entry.id = id;
    entry.title = title.trimmed();
    entry.lesson = lesson.trimmed();
    entry.scopes = scopes;
    entry.evidence = evidence;
    entry.portable = portable;
    entry.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    values.append(toJson(entry));
    store.insert(QStringLiteral("entries"), values);
    if (!writeStore(projectRoot, store, error)) return {};

    ProjectMemory memory;
    memory.appendEvent(projectRoot, QStringLiteral("FRAMEWORK_KNOWLEDGE_CANDIDATE"),
                       QStringLiteral("Framework Knowledge candidate proposed"),
                       QJsonObject{{QStringLiteral("knowledgeId"), id}, {QStringLiteral("title"), entry.title}}, nullptr);
    return id;
}

QString FrameworkKnowledgeService::proposeApprovedByAdministrator(const QString& projectRoot,
                                                                    const QString& title,
                                                                    const QString& lesson,
                                                                    const QStringList& scopes,
                                                                    const QStringList& evidence,
                                                                    const QString& administrator,
                                                                    bool portable,
                                                                    QString* error) const
{
    if (administrator != QStringLiteral("Admin Morgan Lindbom")) {
        if (error) *error = QStringLiteral("Administrative knowledge approval requires Admin Morgan Lindbom.");
        return {};
    }
    if (title.trimmed().isEmpty() || lesson.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Framework Knowledge requires a title and lesson.");
        return {};
    }
    if (!ensureFile(projectRoot, error)) return {};
    QJsonObject store;
    if (!readStore(projectRoot, &store, error)) return {};
    QJsonArray values = store.value(QStringLiteral("entries")).toArray();
    const QString id = stableId(title, lesson, scopes);
    for (qsizetype i = 0; i < values.size(); ++i) {
        QJsonObject value = values.at(i).toObject();
        if (value.value(QStringLiteral("id")).toString() != id) continue;
        if (value.value(QStringLiteral("status")).toString() == QStringLiteral("superseded")) {
            if (error) *error = QStringLiteral("Superseded Framework Knowledge cannot be approved.");
            return {};
        }
        QJsonArray mergedEvidence = value.value(QStringLiteral("evidence")).toArray();
        for (const auto& item : evidence) if (!item.trimmed().isEmpty() && !strings(mergedEvidence).contains(item.trimmed())) mergedEvidence.append(item.trimmed());
        value.insert(QStringLiteral("evidence"), mergedEvidence);
        value.insert(QStringLiteral("status"), QStringLiteral("approved"));
        value.insert(QStringLiteral("reviewStatus"), QStringLiteral("approved"));
        value.insert(QStringLiteral("approvedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        value.insert(QStringLiteral("approvalSource"), administrator);
        values.replace(i, value);
        store.insert(QStringLiteral("entries"), values);
        return writeStore(projectRoot, store, error) ? id : QString();
    }
    FrameworkKnowledgeEntry entry;
    entry.id = id;
    entry.title = title.trimmed();
    entry.lesson = lesson.trimmed();
    entry.status = QStringLiteral("approved");
    entry.reviewStatus = QStringLiteral("approved");
    entry.scopes = scopes;
    entry.evidence = evidence;
    entry.portable = portable;
    entry.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry.approvedAt = entry.createdAt;
    entry.approvalSource = administrator;
    values.append(toJson(entry));
    store.insert(QStringLiteral("entries"), values);
    return writeStore(projectRoot, store, error) ? id : QString();
}

bool FrameworkKnowledgeService::approve(const QString& projectRoot,
                                         const QString& candidateId,
                                         const QString& approvalSource,
                                         QString* error) const
{
    /**Promote one candidate to approved live knowledge.

    Approval requires an explicit approval source so agents cannot silently self-promote observations.
    */
    if (approvalSource.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Explicit approval source is required.");
        return false;
    }
    QJsonObject store;
    if (!readStore(projectRoot, &store, error)) return false;
    QJsonArray values = store.value(QStringLiteral("entries")).toArray();
    bool found = false;
    for (qsizetype i = 0; i < values.size(); ++i) {
        QJsonObject value = values.at(i).toObject();
        if (value.value(QStringLiteral("id")).toString() != candidateId) continue;
        found = true;
        if (value.value(QStringLiteral("status")).toString() == QStringLiteral("superseded")) {
            if (error) *error = QStringLiteral("Superseded Framework Knowledge cannot be approved.");
            return false;
        }
        value.insert(QStringLiteral("status"), QStringLiteral("approved"));
        value.insert(QStringLiteral("reviewStatus"), QStringLiteral("approved"));
        value.insert(QStringLiteral("approvedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        value.insert(QStringLiteral("approvalSource"), approvalSource.trimmed());
        values.replace(i, value);
        break;
    }
    if (!found) {
        if (error) *error = QStringLiteral("Framework Knowledge candidate not found: %1").arg(candidateId);
        return false;
    }
    store.insert(QStringLiteral("entries"), values);
    if (!writeStore(projectRoot, store, error)) return false;

    ProjectMemory memory;
    return memory.appendEvent(projectRoot, QStringLiteral("FRAMEWORK_KNOWLEDGE_APPROVED"),
                              QStringLiteral("Framework Knowledge approved"),
                              QJsonObject{{QStringLiteral("knowledgeId"), candidateId},
                                          {QStringLiteral("approvalSource"), approvalSource.trimmed()}}, error);
}

bool FrameworkKnowledgeService::markMoreEvidence(const QString& projectRoot,
                                                 const QString& entryId,
                                                 QString* error) const
{
    QJsonObject store;
    if (!readStore(projectRoot, &store, error)) return false;
    QJsonArray values = store.value(QStringLiteral("entries")).toArray();
    bool found = false;
    for (qsizetype i = 0; i < values.size(); ++i) {
        QJsonObject value = values.at(i).toObject();
        if (value.value(QStringLiteral("id")).toString() != entryId) continue;
        found = true;
        if (value.value(QStringLiteral("status")).toString() != QStringLiteral("candidate")) {
            if (error) *error = QStringLiteral("Only candidate Framework Knowledge can request more evidence.");
            return false;
        }
        value.insert(QStringLiteral("reviewStatus"), QStringLiteral("more-evidence"));
        values.replace(i, value);
        break;
    }
    if (!found) {
        if (error) *error = QStringLiteral("Framework Knowledge entry not found: %1").arg(entryId);
        return false;
    }
    store.insert(QStringLiteral("entries"), values);
    return writeStore(projectRoot, store, error);
}

bool FrameworkKnowledgeService::supersede(const QString& projectRoot,
                                           const QString& entryId,
                                           const QString& replacementId,
                                           QString* error) const
{
    /**Mark obsolete knowledge as superseded without deleting historical evidence.

    Superseded entries remain auditable but are excluded from active knowledge loading.
    */
    QJsonObject store;
    if (!readStore(projectRoot, &store, error)) return false;
    QJsonArray values = store.value(QStringLiteral("entries")).toArray();
    bool found = false;
    for (qsizetype i = 0; i < values.size(); ++i) {
        QJsonObject value = values.at(i).toObject();
        if (value.value(QStringLiteral("id")).toString() != entryId) continue;
        value.insert(QStringLiteral("status"), QStringLiteral("superseded"));
        value.insert(QStringLiteral("reviewStatus"), QStringLiteral("superseded"));
        value.insert(QStringLiteral("supersededBy"), replacementId.trimmed());
        values.replace(i, value);
        found = true;
        break;
    }
    if (!found) {
        if (error) *error = QStringLiteral("Framework Knowledge entry not found: %1").arg(entryId);
        return false;
    }
    store.insert(QStringLiteral("entries"), values);
    if (!writeStore(projectRoot, store, error)) return false;
    ProjectMemory memory;
    return memory.appendEvent(projectRoot, QStringLiteral("FRAMEWORK_KNOWLEDGE_SUPERSEDED"),
                              QStringLiteral("Framework Knowledge superseded"),
                              QJsonObject{{QStringLiteral("knowledgeId"), entryId},
                                          {QStringLiteral("replacementId"), replacementId.trimmed()}}, error);
}

QList<FrameworkKnowledgeEntry> FrameworkKnowledgeService::entries(const QString& projectRoot,
                                                                   QString* error) const
{
    /**Return all knowledge entries in stored order.

    Candidates and superseded entries are included for auditability.
    */
    QJsonObject store;
    if (!readStore(projectRoot, &store, error)) return {};
    QList<FrameworkKnowledgeEntry> result;
    for (const auto& value : store.value(QStringLiteral("entries")).toArray()) result.append(fromJson(value.toObject()));
    return result;
}

QList<FrameworkKnowledgeEntry> FrameworkKnowledgeService::approvedEntries(const QString& projectRoot,
                                                                           const QStringList& scopes,
                                                                           QString* error) const
{
    /**Return only approved knowledge applicable to the requested scopes.

    This is the runtime read path used when consumers need live approved framework lessons.
    */
    QList<FrameworkKnowledgeEntry> result;
    for (const auto& entry : effectiveKnowledgeForProject(projectRoot, error)) {
        if (entry.status == QStringLiteral("approved") && entry.reviewStatus == QStringLiteral("approved") && scopeMatches(entry, scopes)) result.append(entry);
    }
    return result;
}

QList<FrameworkKnowledgeEntry> FrameworkKnowledgeService::effectiveKnowledgeForProject(const QString& projectRoot,
                                                                                         QString* error) const
{
    const auto builtin = builtInEntries(error);
    if (error && !error->isEmpty()) return {};
    const auto global = globalEntries(error);
    if (error && !error->isEmpty()) return {};
    const auto project = entries(projectRoot, error);
    if (error && !error->isEmpty()) return {};

    QList<FrameworkKnowledgeEntry> result;
    QStringList builtinIds;
    QStringList globalIds;
    QStringList projectIds;
    auto mergeLayer = [&result](const QList<FrameworkKnowledgeEntry>& layer, const QString& origin,
                                QStringList* ids) {
        for (const auto& incoming : layer) {
            if (ids) ids->append(incoming.id);
            auto existing = std::find_if(result.begin(), result.end(), [&incoming](const auto& value) {
                return value.id == incoming.id;
            });
            if (existing == result.end()) {
                auto added = incoming;
                added.origin = origin;
                result.append(added);
                continue;
            }
            const auto statePriority = [](const FrameworkKnowledgeEntry& value) {
                if (value.status == QStringLiteral("approved") && value.supersededBy.isEmpty()) return 3;
                if (value.status == QStringLiteral("candidate")) return 2;
                if (value.status == QStringLiteral("superseded") || !value.supersededBy.isEmpty()) return 1;
                return 0;
            };
            const bool projectSuppression = origin == QStringLiteral("project")
                && (incoming.status == QStringLiteral("superseded") || !incoming.supersededBy.isEmpty());
            const bool replace = projectSuppression
                || statePriority(incoming) > statePriority(*existing)
                || statePriority(incoming) == statePriority(*existing);
            FrameworkKnowledgeEntry selected = replace ? incoming : *existing;
            for (const auto& evidence : existing->evidence) if (!selected.evidence.contains(evidence)) selected.evidence.append(evidence);
            for (const auto& evidence : incoming.evidence) if (!selected.evidence.contains(evidence)) selected.evidence.append(evidence);
            if (selected.originalKnowledgeId.isEmpty()) selected.originalKnowledgeId = existing->originalKnowledgeId;
            if (selected.originProjectId.isEmpty()) selected.originProjectId = existing->originProjectId;
            if (selected.promotedAt.isEmpty()) selected.promotedAt = existing->promotedAt;
            selected.origin = existing->origin;
            if (origin == QStringLiteral("global") && (existing->origin == QStringLiteral("builtin") || existing->origin == QStringLiteral("global")))
                selected.origin = QStringLiteral("global");
            if (origin == QStringLiteral("project") && (existing->origin == QStringLiteral("global") || existing->origin == QStringLiteral("project")))
                selected.origin = QStringLiteral("project+global");
            *existing = selected;
        }
    };
    mergeLayer(builtin, QStringLiteral("builtin"), &builtinIds);
    mergeLayer(global, QStringLiteral("global"), &globalIds);
    mergeLayer(project, QStringLiteral("project"), &projectIds);
    for (auto& entry : result) {
        const bool hasGlobal = globalIds.contains(entry.id);
        const bool hasProject = projectIds.contains(entry.id);
        if (hasProject && hasGlobal) entry.origin = QStringLiteral("project+global");
        else if (hasGlobal) entry.origin = QStringLiteral("global");
        else if (hasProject) entry.origin = QStringLiteral("project");
        else entry.origin = QStringLiteral("builtin");
    }
    return result;
}

bool FrameworkKnowledgeService::adoptKnowledgeForProject(const QString& projectRoot,
                                                          const QStringList& entryIds,
                                                          QString* error) const
{
    QJsonObject store;
    if (!readStore(projectRoot, &store, error)) return false;
    const auto effective = effectiveKnowledgeForProject(projectRoot, error);
    if (error && !error->isEmpty()) return false;
    QList<FrameworkKnowledgeEntry> projectEntries = entriesFromStore(store);
    for (const auto& id : entryIds) {
        const auto source = std::find_if(effective.cbegin(), effective.cend(), [&id](const auto& entry) { return entry.id == id; });
        if (source == effective.cend()) {
            if (error) *error = QStringLiteral("Framework Knowledge entry is not available for adoption: %1").arg(id);
            return false;
        }
        if (source->status != QStringLiteral("approved") || source->reviewStatus != QStringLiteral("approved") || !source->supersededBy.isEmpty()) {
            if (error) *error = QStringLiteral("Only active approved Framework Knowledge can be adopted: %1").arg(id);
            return false;
        }
        auto existing = std::find_if(projectEntries.begin(), projectEntries.end(), [&id](const auto& entry) { return entry.id == id; });
        if (existing == projectEntries.end()) {
            auto adopted = *source;
            adopted.origin = source->origin == QStringLiteral("global") ? QStringLiteral("project+global") : QStringLiteral("project");
            projectEntries.append(adopted);
            continue;
        }
        for (const auto& evidence : source->evidence) if (!existing->evidence.contains(evidence)) existing->evidence.append(evidence);
        if (existing->status != QStringLiteral("approved") || existing->reviewStatus != QStringLiteral("approved")) {
            existing->status = source->status;
            existing->reviewStatus = source->reviewStatus;
        }
        if (existing->title.isEmpty()) existing->title = source->title;
        if (existing->lesson.isEmpty()) existing->lesson = source->lesson;
        if (existing->scopes.isEmpty()) existing->scopes = source->scopes;
        if (existing->approvedAt.isEmpty()) existing->approvedAt = source->approvedAt;
        if (existing->approvalSource.isEmpty()) existing->approvalSource = source->approvalSource;
        if (existing->originalKnowledgeId.isEmpty()) existing->originalKnowledgeId = source->originalKnowledgeId;
        if (existing->originProjectId.isEmpty()) existing->originProjectId = source->originProjectId;
        if (existing->promotedAt.isEmpty()) existing->promotedAt = source->promotedAt;
        if (existing->origin == QStringLiteral("project") || source->origin == QStringLiteral("global")) existing->origin = QStringLiteral("project+global");
    }
    return writeStore(projectRoot, storeFromEntries(projectEntries, QStringLiteral("framework-knowledge.json")), error);
}

QString FrameworkKnowledgeService::globalLibraryPath() const
{
    return libraryPath();
}

QString FrameworkKnowledgeService::legacyGlobalLibraryPath() const
{
    return legacyLibraryPath();
}

QString FrameworkKnowledgeService::legacyExecutableGlobalLibraryPath() const
{
    return legacyExecutableLibraryPath();
}

bool FrameworkKnowledgeService::ensureGlobalLibrary(QString* error) const
{
    const QString path = libraryPath();
    QJsonObject canonicalStore;
    const bool canonicalExists = QFile::exists(path);
    if (canonicalExists) {
        if (!readObjectFile(path, &canonicalStore, error) || !canonicalStore.value(QStringLiteral("entries")).isArray()) {
            if (error && error->isEmpty()) *error = QStringLiteral("Program-local Framework Knowledge library is invalid.");
            return false;
        }
    }

    const QStringList legacyPaths{legacyLibraryPath(), legacyExecutableLibraryPath()};
    const QString markerPath = migrationMarkerPath();
    if (!canonicalExists && QFile::exists(markerPath)) {
        if (error) *error = QStringLiteral("Framework Knowledge migration marker exists but the program-local library is missing.");
        return false;
    }
    bool hasLegacy = false;
    QList<FrameworkKnowledgeEntry> legacyEntries;
    QStringList migratedFrom;
    if (!QFile::exists(markerPath)) {
        for (const auto& legacyPath : legacyPaths) {
            if (legacyPath == path || !QFile::exists(legacyPath)) continue;
            hasLegacy = true;
            QJsonObject legacyStore;
            if (!readObjectFile(legacyPath, &legacyStore, error) || !legacyStore.value(QStringLiteral("entries")).isArray()) {
                if (error && error->isEmpty()) *error = QStringLiteral("Legacy Framework Knowledge library is invalid: %1").arg(legacyPath);
                return false;
            }
            legacyEntries = mergeLibraryEntries(legacyEntries, entriesFromStore(legacyStore));
            migratedFrom.append(legacyPath);
        }
    }
    if (hasLegacy) {
        const auto merged = mergeLibraryEntries(canonicalExists ? entriesFromStore(canonicalStore) : QList<FrameworkKnowledgeEntry>{}, legacyEntries);
        const auto mergedStore = storeFromEntries(merged, QStringLiteral("framework-knowledge-library.json"));
        if (!writeObjectFile(path, mergedStore, error)) return false;
        if (!writeObjectFile(markerPath, QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("migratedFrom"), strings(migratedFrom)}}, error)) return false;
        // The legacy file is intentionally retained as non-authoritative
        // recovery evidence. The marker ensures all future reads use only the
        // program-local file.
        return true;
    }

    if (canonicalExists) return true;
    return writeObjectFile(path, emptyLibrary(), error);
}

QList<FrameworkKnowledgeEntry> FrameworkKnowledgeService::builtInEntries(QString* error) const
{
    QStringList candidates;
    candidates << QDir(AramfPaths::programRoot()).filePath(QStringLiteral("aramf_setup/memory/framework-knowledge.json"));
    candidates << QDir(AramfPaths::applicationDirectory()).filePath(QStringLiteral("aramf_setup/memory/framework-knowledge.json"));
    for (const auto& path : candidates) {
        QJsonObject store;
        if (!QFile::exists(path)) continue;
        if (!readObjectFile(path, &store, error)) return {};
        auto result = entriesFromStore(store);
        for (auto& entry : result) entry.origin = QStringLiteral("builtin");
        return result;
    }
    return {};
}

QList<FrameworkKnowledgeEntry> FrameworkKnowledgeService::globalEntries(QString* error) const
{
    if (!ensureGlobalLibrary(error)) return {};
    QJsonObject store;
    if (!readObjectFile(libraryPath(), &store, error)) return {};
    auto result = entriesFromStore(store);
    for (auto& entry : result) entry.origin = QStringLiteral("global");
    return result;
}

QList<FrameworkKnowledgeEntry> FrameworkKnowledgeService::approvedGlobalEntries(const QStringList& scopes,
                                                                                 QString* error) const
{
    QList<FrameworkKnowledgeEntry> result;
    for (const auto& entry : globalEntries(error)) if (activeApplicable(entry, scopes)) result.append(entry);
    return result;
}

bool FrameworkKnowledgeService::promoteToGlobal(const QString& projectRoot,
                                                const QString& entryId,
                                                QString* error) const
{
    const auto projectEntries = entries(projectRoot, error);
    const auto it = std::find_if(projectEntries.cbegin(), projectEntries.cend(), [&entryId](const auto& entry) { return entry.id == entryId; });
    if (it == projectEntries.cend()) {
        if (error) *error = QStringLiteral("Framework Knowledge entry not found: %1").arg(entryId);
        return false;
    }
    if (it->status != QStringLiteral("approved") || !it->supersededBy.isEmpty()) {
        if (error) *error = QStringLiteral("Only approved, non-superseded knowledge can be promoted.");
        return false;
    }
    if (!it->portable) {
        if (error) *error = QStringLiteral("This knowledge is project-specific and is not portable.");
        return false;
    }
    if (!ensureGlobalLibrary(error)) return false;
    QJsonObject store;
    if (!readObjectFile(libraryPath(), &store, error)) return false;
    auto global = entriesFromStore(store);
    auto existing = std::find_if(global.begin(), global.end(), [&entryId](const auto& entry) { return entry.id == entryId; });
    if (existing != global.end()) {
        if (existing->status == QStringLiteral("superseded")) {
            if (error) *error = QStringLiteral("A superseded global entry cannot be reactivated automatically.");
            return false;
        }
        for (const auto& evidence : it->evidence) if (!existing->evidence.contains(evidence)) existing->evidence.append(evidence);
    } else {
        FrameworkKnowledgeEntry promoted = *it;
        promoted.origin = QStringLiteral("global");
        promoted.originProjectId = projectRoot.isEmpty() ? QString() : QFileInfo(projectRoot).fileName();
        promoted.originalKnowledgeId = it->id;
        promoted.promotedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        global.append(promoted);
    }
    return writeObjectFile(libraryPath(), storeFromEntries(global, QStringLiteral("framework-knowledge-library.json")), error);
}

bool FrameworkKnowledgeService::supersedeGlobal(const QString& entryId,
                                                const QString& replacementId,
                                                QString* error) const
{
    if (!ensureGlobalLibrary(error)) return false;
    QJsonObject store;
    if (!readObjectFile(libraryPath(), &store, error)) return false;
    auto global = entriesFromStore(store);
    auto it = std::find_if(global.begin(), global.end(), [&entryId](const auto& entry) { return entry.id == entryId; });
    if (it == global.end()) {
        if (error) *error = QStringLiteral("Global Framework Knowledge entry not found: %1").arg(entryId);
        return false;
    }
    it->status = QStringLiteral("superseded");
    it->reviewStatus = QStringLiteral("superseded");
    it->supersededBy = replacementId.trimmed();
    return writeObjectFile(libraryPath(), storeFromEntries(global, QStringLiteral("framework-knowledge-library.json")), error);
}

bool FrameworkKnowledgeService::seedProject(const QString& projectRoot,
                                            const ProjectModel* model,
                                            QString* error) const
{
    if (!ensureFile(projectRoot, error)) return false;
    QJsonObject store;
    if (!readObjectFile(storePath(projectRoot), &store, error)) return false;
    auto projectEntries = entriesFromStore(store);
    QList<FrameworkKnowledgeEntry> candidates = builtInEntries(error);
    // A new project may not yet expose enough semantic subsystem scopes to
    // decide applicability. Seed approved global knowledge conservatively;
    // runtime UPDATE applicability remains responsible for filtering it.
    const auto global = approvedGlobalEntries({}, error);
    for (const auto& entry : global) candidates.append(entry);
    bool changed = false;
    for (auto entry : candidates) {
        if (entry.status != QStringLiteral("approved") || !entry.supersededBy.isEmpty()) continue;
        if (std::any_of(projectEntries.cbegin(), projectEntries.cend(), [&entry](const auto& current) { return current.id == entry.id; })) continue;
        entry.origin = entry.origin == QStringLiteral("builtin") ? QStringLiteral("builtin") : QStringLiteral("global");
        projectEntries.append(entry);
        changed = true;
    }
    return !changed || writeStore(projectRoot, storeFromEntries(projectEntries, QStringLiteral("framework-knowledge.json")), error);
}
