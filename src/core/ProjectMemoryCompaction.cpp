#include "ProjectMemoryCompaction.h"

#include "AramfPaths.h"
#include "ProjectMemory.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace {
QString path(const QString& root, const QString& relative) { return QDir(root).filePath(relative); }
QJsonObject readObject(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject() ? document.object() : QJsonObject{};
}
QList<QJsonObject> readEvents(const QString& fileName, QString* error)
{
    QList<QJsonObject> values;
    QFile file(fileName);
    if (!file.exists()) return values;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { if (error) *error = file.errorString(); return {}; }
    int line = 0;
    while (!file.atEnd()) {
        ++line; const auto bytes = file.readLine().trimmed(); if (bytes.isEmpty()) continue;
        QJsonParseError parseError; const auto document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (error) *error = QStringLiteral("Malformed event history at line %1.").arg(line); return {};
        }
        values.append(document.object());
    }
    return values;
}
bool writeObject(const QString& fileName, const QJsonObject& value, QString* error)
{
    QDir().mkpath(QFileInfo(fileName).absolutePath()); QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = file.errorString(); return false; }
    if (file.write(QJsonDocument(value).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) { if (error) *error = file.errorString(); return false; }
    return true;
}
QString fingerprint(const QJsonObject& event)
{
    QJsonObject normalized;
    for (const auto& key : {QStringLiteral("eventType"), QStringLiteral("task"), QStringLiteral("status"), QStringLiteral("summary"), QStringLiteral("detail"), QStringLiteral("category"), QStringLiteral("issue"), QStringLiteral("buildSystem"), QStringLiteral("suite")})
        if (event.contains(key)) normalized.insert(key, event.value(key));
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(normalized).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
bool protectedEvent(const QJsonObject& event, const QSet<QString>& referenced)
{
    const QString type = event.value(QStringLiteral("eventType")).toString();
    const QString id = event.value(QStringLiteral("eventId")).toString();
    const QString text = QJsonDocument(event).toJson(QJsonDocument::Compact).toLower();
    return referenced.contains(id) || type == QStringLiteral("ADMIN_OVERRIDE") || type == QStringLiteral("ADMIN_OVERRIDE_VALIDATION")
        || type.contains(QStringLiteral("DECISION")) || type.contains(QStringLiteral("CHECKPOINT"))
        || type.contains(QStringLiteral("CERTIFICATION")) || text.contains("\"unresolved\":true")
        || text.contains("\"blocker\":true") || text.contains("\"active\":true")
        || text.contains("security") || text.contains("destructive") || text.contains("unique");
}
}

ProjectMemoryCompaction::ProjectMemoryCompaction(QObject* parent) : QObject(parent) {}

int ProjectMemoryCompaction::reviewThreshold(const QString& projectRoot, QString* error)
{
    const auto config = readObject(path(projectRoot, AramfPaths::MemoryConfiguration));
    const int threshold = config.value(QStringLiteral("compactionReviewThreshold")).toInt(500);
    if (threshold < 1) { if (error) *error = QStringLiteral("compactionReviewThreshold must be positive."); return 500; }
    return threshold;
}

bool ProjectMemoryCompaction::reviewDue(const QString& projectRoot, QString* error)
{
    ProjectMemory memory; const auto events = memory.events(projectRoot, error);
    return error && !error->isEmpty() ? false : events.size() >= reviewThreshold(projectRoot, error);
}

QJsonObject ProjectMemoryCompaction::dryRun(const QString& projectRoot, QString* error) const
{
    const auto events = readEvents(path(projectRoot, AramfPaths::EventLog), error);
    QHash<QString, QList<QJsonObject>> clusters; QSet<QString> protectedIds;
    const auto checkpoints = readObject(path(projectRoot, AramfPaths::Checkpoints)).value(QStringLiteral("checkpoints")).toArray();
    for (const auto& value : checkpoints) protectedIds.insert(value.toObject().value(QStringLiteral("latestEventId")).toString());
    for (const auto& event : events) if (!protectedEvent(event, protectedIds)) clusters[fingerprint(event)].append(event);
    QJsonArray patterns, candidates, retained, removed;
    int compacted = 0;
    for (auto it = clusters.cbegin(); it != clusters.cend(); ++it) {
        if (it.value().size() < 3) continue;
        const auto& group = it.value(); QJsonArray sourceIds;
        for (const auto& event : group) sourceIds.append(event.value(QStringLiteral("eventId")));
        const QString knowledgeId = QStringLiteral("pk-%1").arg(it.key().left(16));
        const auto first = group.first();
        QJsonObject candidate{{QStringLiteral("id"), knowledgeId}, {QStringLiteral("title"), QStringLiteral("Recurring lesson: %1").arg(first.value(QStringLiteral("eventType")).toString())},
            {QStringLiteral("lesson"), QStringLiteral("Repeated %1 times: %2").arg(group.size()).arg(first.value(QStringLiteral("task")).toString())},
            {QStringLiteral("scope"), QStringLiteral("project")}, {QStringLiteral("sourceProject"), QFileInfo(projectRoot).fileName()},
            {QStringLiteral("evidenceSummary"), QStringLiteral("Semantic fingerprint %1 matched equivalent event context.").arg(it.key())},
            {QStringLiteral("sourceEventIds"), sourceIds}, {QStringLiteral("occurrenceCount"), group.size()},
            {QStringLiteral("firstObserved"), first.value(QStringLiteral("timestamp"))}, {QStringLiteral("lastObserved"), group.last().value(QStringLiteral("timestamp"))},
            {QStringLiteral("confidence"), group.size() >= 5 ? QStringLiteral("high") : QStringLiteral("medium")}, {QStringLiteral("reasonForExtraction"), QStringLiteral("Recurring semantically equivalent operational experience")},
            {QStringLiteral("expectedFutureEfficiencyBenefit"), QStringLiteral("Avoid rediscovery and repeated equivalent work")}, {QStringLiteral("approvalState"), QStringLiteral("candidate")}, {QStringLiteral("portability"), QStringLiteral("project-local")}};
        candidates.append(candidate); patterns.append(QJsonObject{{QStringLiteral("fingerprint"), it.key()}, {QStringLiteral("occurrenceCount"), group.size()}, {QStringLiteral("sourceEventIds"), sourceIds}});
        for (int i = 0; i < group.size(); ++i) { if (i < 2) retained.append(group.at(i).value(QStringLiteral("eventId"))); else { removed.append(group.at(i).value(QStringLiteral("eventId"))); ++compacted; } }
    }
    QJsonObject result{{QStringLiteral("status"), QStringLiteral("DRY_RUN")}, {QStringLiteral("reviewDue"), events.size() >= reviewThreshold(projectRoot, nullptr)},
        {QStringLiteral("threshold"), reviewThreshold(projectRoot, nullptr)}, {QStringLiteral("eventsBefore"), events.size()}, {QStringLiteral("eventsRetained"), events.size() - compacted},
        {QStringLiteral("eventsCompacted"), compacted}, {QStringLiteral("patterns"), patterns}, {QStringLiteral("knowledgeCandidates"), candidates},
        {QStringLiteral("retainedRepresentativeEvents"), retained}, {QStringLiteral("wouldRemoveEventIds"), removed}, {QStringLiteral("protectedEvents"), QJsonArray::fromStringList(protectedIds.values())}};
    return result;
}

bool ProjectMemoryCompaction::compact(const QString& projectRoot, bool approveCandidates, QJsonObject* result, QString* error) const
{
    const auto plan = dryRun(projectRoot, error); if (error && !error->isEmpty()) return false;
    if (!plan.value(QStringLiteral("reviewDue")).toBool()) { if (result) *result = plan; return true; }
    if (!approveCandidates) { if (error) *error = QStringLiteral("Compaction requires explicit candidate approval; use dry-run or approve candidates."); return false; }
    const auto events = readEvents(path(projectRoot, AramfPaths::EventLog), error); if (error && !error->isEmpty()) return false;
    QSet<QString> removed; for (const auto& value : plan.value(QStringLiteral("wouldRemoveEventIds")).toArray()) removed.insert(value.toString());
    QList<QJsonObject> retained; for (const auto& event : events) if (!removed.contains(event.value(QStringLiteral("eventId")).toString())) retained.append(event);
    const QString stamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate); const QString id = QStringLiteral("compact-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QFile sourceFile(path(projectRoot, AramfPaths::EventLog)); QByteArray sourceBytes;
    if (sourceFile.open(QIODevice::ReadOnly)) sourceBytes = sourceFile.readAll();
    sourceFile.close();
    const QByteArray oldManifestBytes = QFile(path(projectRoot, AramfPaths::Manifest)).exists() ? [&]() { QFile f(path(projectRoot, AramfPaths::Manifest)); f.open(QIODevice::ReadOnly); const auto b = f.readAll(); f.close(); return b; }() : QByteArray{};
    const QByteArray oldKnowledgeBytes = QFile(path(projectRoot, AramfPaths::ProjectKnowledge)).exists() ? [&]() { QFile f(path(projectRoot, AramfPaths::ProjectKnowledge)); f.open(QIODevice::ReadOnly); const auto b = f.readAll(); f.close(); return b; }() : QByteArray{};
    QJsonObject manifest{{QStringLiteral("compactionId"), id}, {QStringLiteral("timestamp"), stamp}, {QStringLiteral("sourceSequenceStart"), events.isEmpty() ? 0 : events.first().value(QStringLiteral("sequenceNumber"))}, {QStringLiteral("sourceSequenceEnd"), events.isEmpty() ? 0 : events.last().value(QStringLiteral("sequenceNumber"))}, {QStringLiteral("originalEventCount"), events.size()}, {QStringLiteral("retainedEventCount"), retained.size()}, {QStringLiteral("removedEventCount"), removed.size()}, {QStringLiteral("knowledgeCreated"), plan.value(QStringLiteral("knowledgeCandidates"))}, {QStringLiteral("retainedRepresentativeEvents"), plan.value(QStringLiteral("retainedRepresentativeEvents"))}, {QStringLiteral("protectedEvents"), plan.value(QStringLiteral("protectedEvents"))}, {QStringLiteral("sourceFingerprint"), QString::fromLatin1(QCryptographicHash::hash(sourceBytes, QCryptographicHash::Sha256).toHex())}, {QStringLiteral("agentMode"), QStringLiteral("agent-direct")}, {QStringLiteral("writerMode"), QStringLiteral("agent-direct")}, {QStringLiteral("validationStatus"), QStringLiteral("PENDING")}};
    // Persist provenance and knowledge before the atomic event-log replacement.
    QJsonObject knowledge = readObject(path(projectRoot, AramfPaths::ProjectKnowledge)); QJsonArray entries = knowledge.value(QStringLiteral("entries")).toArray();
    QSet<QString> existingKnowledgeIds;
    for (const auto& value : entries) existingKnowledgeIds.insert(value.toObject().value(QStringLiteral("id")).toString());
    for (const auto& value : plan.value(QStringLiteral("knowledgeCandidates")).toArray()) {
        auto candidate = value.toObject();
        if (existingKnowledgeIds.contains(candidate.value(QStringLiteral("id")).toString())) continue;
        candidate.insert(QStringLiteral("approvalState"), QStringLiteral("approved")); entries.append(candidate);
        existingKnowledgeIds.insert(candidate.value(QStringLiteral("id")).toString());
    }
    knowledge.insert(QStringLiteral("_file"), QStringLiteral("project-knowledge.json")); knowledge.insert(QStringLiteral("entries"), entries);
    if (!writeObject(path(projectRoot, AramfPaths::ProjectKnowledge), knowledge, error) || !writeObject(path(projectRoot, AramfPaths::CompactionManifest), manifest, error)) return false;
    QSaveFile log(path(projectRoot, AramfPaths::EventLog)); if (!log.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = log.errorString(); return false; }
    for (const auto& event : retained) { log.write(QJsonDocument(event).toJson(QJsonDocument::Compact)); log.write("\n"); }
    if (!log.commit()) { if (error) *error = log.errorString(); return false; }
    QJsonObject memoryManifest = readObject(path(projectRoot, AramfPaths::Manifest));
    memoryManifest.insert(QStringLiteral("eventCount"), retained.size());
    if (!writeObject(path(projectRoot, AramfPaths::Manifest), memoryManifest, error)) return false;
    QFile history(path(projectRoot, AramfPaths::CompactionHistory)); if (!history.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) { if (error) *error = history.errorString(); return false; }
    history.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact)); history.write("\n"); history.close();
    ProjectMemory memory; memory.refreshDerivedState(projectRoot, error);
    auto validation = memory.validate(projectRoot, error); if (validation.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")) {
        // A failed post-write validation revokes deletion: restore the exact
        // source and derived inputs from the in-memory snapshots.
        auto restore = [](const QString& fileName, const QByteArray& bytes) { QSaveFile f(fileName); if (!f.open(QIODevice::WriteOnly)) return false; if (f.write(bytes) != bytes.size()) return false; return f.commit(); };
        restore(path(projectRoot, AramfPaths::EventLog), sourceBytes);
        restore(path(projectRoot, AramfPaths::Manifest), oldManifestBytes);
        if (!oldKnowledgeBytes.isEmpty()) restore(path(projectRoot, AramfPaths::ProjectKnowledge), oldKnowledgeBytes);
        memory.refreshDerivedState(projectRoot, nullptr);
        if (error && error->isEmpty()) *error = QStringLiteral("Compacted history failed consistency validation; source history was restored."); return false;
    }
    manifest.insert(QStringLiteral("resultingEventLogFingerprint"), QStringLiteral("validated-after-rewrite")); manifest.insert(QStringLiteral("validationStatus"), QStringLiteral("PASS")); writeObject(path(projectRoot, AramfPaths::CompactionManifest), manifest, error);
    QJsonObject metrics = readObject(path(projectRoot, AramfPaths::Metrics));
    metrics.insert(QStringLiteral("activeEvents"), retained.size());
    metrics.insert(QStringLiteral("eventsCompacted"), metrics.value(QStringLiteral("eventsCompacted")).toInt() + removed.size());
    metrics.insert(QStringLiteral("compactionRuns"), metrics.value(QStringLiteral("compactionRuns")).toInt() + 1);
    metrics.insert(QStringLiteral("knowledgeCandidatesCreated"), metrics.value(QStringLiteral("knowledgeCandidatesCreated")).toInt() + plan.value(QStringLiteral("knowledgeCandidates")).toArray().size());
    metrics.insert(QStringLiteral("knowledgeEntriesApproved"), metrics.value(QStringLiteral("knowledgeEntriesApproved")).toInt() + plan.value(QStringLiteral("knowledgeCandidates")).toArray().size());
    metrics.insert(QStringLiteral("repeatedPatternsDetected"), metrics.value(QStringLiteral("repeatedPatternsDetected")).toInt() + plan.value(QStringLiteral("patterns")).toArray().size());
    metrics.insert(QStringLiteral("estimatedAvoidedRepetitions"), metrics.value(QStringLiteral("estimatedAvoidedRepetitions")).toInt() + removed.size());
    writeObject(path(projectRoot, AramfPaths::Metrics), metrics, error);
    memory.validateColdStart(projectRoot, error); if (result) { *result = plan; result->insert(QStringLiteral("status"), QStringLiteral("PASS")); result->insert(QStringLiteral("compactionManifest"), manifest); } return !(error && !error->isEmpty());
}

QJsonObject ProjectMemoryCompaction::applicableKnowledge(const QString& projectRoot, QString* error) const
{
    Q_UNUSED(error); return readObject(path(projectRoot, AramfPaths::ProjectKnowledge));
}
