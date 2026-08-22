// FrameworkKnowledge.cpp

#include "FrameworkKnowledge.h"

#include "AramfPaths.h"
#include "ProjectMemory.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace {
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
        {QStringLiteral("supersededBy"), entry.supersededBy}
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
    if (requested.isEmpty() || entry.scopes.isEmpty() || entry.scopes.contains(QStringLiteral("all"))) return true;
    for (const auto& scope : requested) if (entry.scopes.contains(scope)) return true;
    return false;
}
}

FrameworkKnowledgeService::FrameworkKnowledgeService(QObject* parent)
    : QObject(parent)
{
    /**Construct the live Framework Knowledge service.

    The service stores candidates and approved reusable lessons inside the managed project's ARAMF memory.
    */
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
    for (const auto& entry : entries(projectRoot, error)) {
        if (entry.status == QStringLiteral("approved") && scopeMatches(entry, scopes)) result.append(entry);
    }
    return result;
}
