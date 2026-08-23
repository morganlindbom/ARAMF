// ProjectMemory.cpp

#include "ProjectMemory.h"

#include "AramfPaths.h"
#include "ControlPlaneMigration.h"
#include "ProjectModel.h"
#include "ValidationRouting.h"

#include <algorithm>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStack>
#include <QRegularExpression>
#include <QVariant>
#include <QUuid>

namespace {
QString absolutePath(const QString& projectRoot, const QString& relativePath)
{
    /**Resolve a canonical ARAMF-relative path.

    The project root is always treated as the owner of the generated ARAMF control plane.
    */
    return QDir(projectRoot).filePath(relativePath);
}

bool writeTextFile(const QString& path, const QByteArray& data, QString* error, bool onlyIfMissing = false)
{
    /**Write one complete text file atomically.

    QSaveFile avoids leaving partially written control-plane files after an interrupted write.
    */
    if (onlyIfMissing && QFile::exists(path)) {
        return true;
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    if (file.write(data) != data.size() || !file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

bool writeJsonFile(const QString& path, const QJsonObject& object, QString* error, bool onlyIfMissing = false)
{
    /**Serialize an object using stable indented JSON.

    A top-level _file field is used as filename metadata because JSON itself does not allow comments.
    */
    QJsonObject value = object;
    if (!value.contains(QStringLiteral("_file"))) {
        value.insert(QStringLiteral("_file"), QFileInfo(path).fileName());
    }
    return writeTextFile(path, QJsonDocument(value).toJson(QJsonDocument::Indented), error, onlyIfMissing);
}

QJsonObject readJsonObject(const QString& path, QString* error)
{
    /**Read a JSON object from disk.

    Parse errors are returned explicitly so memory validation never silently accepts damaged state.
    */
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return {};
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return {};
    }
    return document.object();
}

QList<QJsonObject> readEvents(const QString& path, QString* error)
{
    /**Read the append-only JSONL event stream.

    Each non-empty line must be a complete JSON object and malformed entries invalidate the stream.
    */
    QList<QJsonObject> events;
    QFile file(path);
    if (!file.exists()) {
        return events;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return {};
    }

    int lineNumber = 0;
    while (!file.atEnd()) {
        ++lineNumber;
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (error) {
                *error = QStringLiteral("Malformed JSONL at line %1: %2").arg(lineNumber).arg(parseError.errorString());
            }
            return {};
        }
        events.append(document.object());
    }
    return events;
}

bool isControlPlaneEvent(const QString& eventType)
{
    /**Classify events that update framework state rather than production code.

    Production and durable sequence concepts stay separate so control-plane maintenance cannot look like product progress.
    */
    static const QSet<QString> controlPlaneEvents {
        QStringLiteral("PROJECT_MEMORY_ACTIVATED"),
        QStringLiteral("PROJECT_CONTEXT_CHANGED"),
        QStringLiteral("DECISION_RECORDED"),
        QStringLiteral("CHECKPOINT_CREATED"),
        QStringLiteral("FRAMEWORK_KNOWLEDGE_CANDIDATE"),
        QStringLiteral("FRAMEWORK_KNOWLEDGE_APPROVED"),
        QStringLiteral("FRAMEWORK_KNOWLEDGE_SUPERSEDED")
    };
    return controlPlaneEvents.contains(eventType);
}

QJsonArray stringListToJson(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

bool validResultStatus(const QString& status)
{
    return status == QStringLiteral("PASS") || status == QStringLiteral("FAIL");
}

QString recordingOptionFor(const QString& operation)
{
    if (operation == QStringLiteral("task-start") || operation == QStringLiteral("task-complete")) return QStringLiteral("record-task-completion");
    if (operation == QStringLiteral("build-result")) return QStringLiteral("record-build-results");
    if (operation == QStringLiteral("test-result")) return QStringLiteral("record-test-results");
    if (operation == QStringLiteral("validation-result")) return QStringLiteral("record-validation");
    return {};
}

QString eventTypeFor(const QString& operation)
{
    if (operation == QStringLiteral("task-start")) return QStringLiteral("TASK_STARTED");
    if (operation == QStringLiteral("task-complete")) return QStringLiteral("TASK_COMPLETED");
    if (operation == QStringLiteral("build-result")) return QStringLiteral("BUILD_RESULT");
    if (operation == QStringLiteral("test-result")) return QStringLiteral("TEST_RESULT");
    if (operation == QStringLiteral("validation-result")) return QStringLiteral("VALIDATION_RESULT");
    return {};
}

struct FileSnapshot {
    QString path;
    bool existed = false;
    QByteArray contents;
};

QList<FileSnapshot> snapshotFiles(const QString& projectRoot, const QStringList& relativePaths)
{
    QList<FileSnapshot> snapshots;
    for (const auto& relative : relativePaths) {
        FileSnapshot snapshot;
        snapshot.path = absolutePath(projectRoot, relative);
        snapshot.existed = QFile::exists(snapshot.path);
        if (snapshot.existed) {
            QFile file(snapshot.path);
            if (file.open(QIODevice::ReadOnly)) snapshot.contents = file.readAll();
        }
        snapshots.append(snapshot);
    }
    return snapshots;
}

bool restoreSnapshots(const QList<FileSnapshot>& snapshots, QString* error)
{
    for (const auto& snapshot : snapshots) {
        if (!snapshot.existed) {
            if (QFile::exists(snapshot.path) && !QFile::remove(snapshot.path)) {
                if (error) *error = QStringLiteral("Could not roll back %1").arg(snapshot.path);
                return false;
            }
            continue;
        }
        if (!writeTextFile(snapshot.path, snapshot.contents, error)) return false;
    }
    return true;
}

bool restoreSnapshot(const FileSnapshot& snapshot, QString* error)
{
    if (!snapshot.existed) {
        if (QFile::exists(snapshot.path) && !QFile::remove(snapshot.path)) {
            if (error) *error = QStringLiteral("Could not roll back %1").arg(snapshot.path);
            return false;
        }
        return true;
    }
    QDir().mkpath(QFileInfo(snapshot.path).absolutePath());
    QSaveFile file(snapshot.path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(snapshot.contents) != snapshot.contents.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

struct DecisionRecord {
    QString id;
    QString topic;
    QString status;
    QString supersededBy;
};

QList<DecisionRecord> readDecisionRecords(const QString& path, QString* error)
{
    QList<DecisionRecord> records;
    QFile file(path);
    if (!file.exists()) return records;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return records;
    }
    DecisionRecord current;
    bool inRecord = false;
    const auto lines = QString::fromUtf8(file.readAll()).split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed == QStringLiteral("<!-- ARAMF-DECISION -->")) {
            current = {};
            inRecord = true;
        } else if (trimmed == QStringLiteral("<!-- /ARAMF-DECISION -->")) {
            if (inRecord && !current.id.isEmpty()) records.append(current);
            inRecord = false;
        } else if (inRecord) {
            const int separator = trimmed.indexOf(QLatin1Char(':'));
            if (separator < 0) continue;
            const QString key = trimmed.left(separator).remove(QLatin1Char('-')).trimmed().toLower();
            const QString value = trimmed.mid(separator + 1).trimmed();
            if (key == QStringLiteral("decisionid")) current.id = value;
            else if (key == QStringLiteral("topic")) current.topic = value;
            else if (key == QStringLiteral("status")) current.status = value;
            else if (key == QStringLiteral("supersededby")) current.supersededBy = value;
        }
    }
    return records;
}

QString coldStartFingerprint(const QString& projectRoot, const QStringList& relativePaths)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const QString& relative : relativePaths) {
        QFile file(absolutePath(projectRoot, relative));
        hash.addData(relative.toUtf8());
        hash.addData(QByteArray("\0", 1));
        if (file.open(QIODevice::ReadOnly)) hash.addData(file.readAll());
        hash.addData(QByteArray("\0", 1));
    }
    return QString::fromLatin1(hash.result().toHex());
}

QStringList coldStartPaths(bool requireControlPlane)
{
    QStringList paths {
        AramfPaths::Decisions,
        AramfPaths::FrameworkKnowledge,
        AramfPaths::CurrentState,
        AramfPaths::MemoryConfiguration,
        AramfPaths::MemoryContract,
        AramfPaths::Manifest
    };
    if (requireControlPlane) paths << AramfPaths::AgentInstructions << AramfPaths::ProjectStatus;
    return paths;
}
}

ProjectMemory::ProjectMemory(QObject* parent)
    : QObject(parent)
{
    /**Construct the project-memory service.

    The service is intentionally stateless between calls so one ARAMF process can work with different project roots safely.
    */
}

bool ProjectMemory::initialize(const QString& projectRoot, const ProjectModel* model, QString* error)
{
    /**Create or repair the canonical ARAMF control-plane skeleton.

    Existing user-owned files are preserved; only missing managed files are created automatically.
    */
    if (projectRoot.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Project path is empty.");
        }
        return false;
    }
    const auto preparation = prepareControlPlane(projectRoot);
    if (!preparation.success) {
        if (error) *error = preparation.error;
        return false;
    }
    if (!ensureDirectories(projectRoot, error) || !writeInitialFiles(projectRoot, model, error)) {
        return false;
    }

    const QString eventPath = absolutePath(projectRoot, AramfPaths::EventLog);
    if (!QFile::exists(eventPath) || QFileInfo(eventPath).size() == 0) {
        if (!appendEvent(projectRoot,
                         QStringLiteral("PROJECT_MEMORY_ACTIVATED"),
                         QStringLiteral("Project Memory initialized"),
                         {},
                         error)) {
            return false;
        }
    }

    if (!generateCurrentState(projectRoot, error) || !generateColdStartValidation(projectRoot, error)) {
        return false;
    }
    const QJsonObject report = validate(projectRoot, error);
    return report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
}

bool ProjectMemory::initializeMemory(const QString& projectRoot, const ProjectModel* model, QString* error)
{
    if (projectRoot.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Project path is empty.");
        return false;
    }
    const auto preparation = prepareControlPlane(projectRoot);
    if (!preparation.success) {
        if (error) *error = preparation.error;
        return false;
    }
    if (!ensureMemoryDirectories(projectRoot, error)
        || !writeMemoryFiles(projectRoot, model, error)) {
        return false;
    }

    const QString eventPath = absolutePath(projectRoot, AramfPaths::EventLog);
    if (!QFile::exists(eventPath) || QFileInfo(eventPath).size() == 0) {
        if (!appendEvent(projectRoot, QStringLiteral("PROJECT_MEMORY_ACTIVATED"),
                         QStringLiteral("Project Memory initialized"), {}, error)) {
            return false;
        }
    }

    if (!generateCurrentState(projectRoot, error)
        || !generateColdStartValidation(projectRoot, error, false)) {
        return false;
    }
    const QJsonObject report = validate(projectRoot, error);
    return report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
}

bool ProjectMemory::appendEvent(const QString& projectRoot,
                                const QString& eventType,
                                const QString& task,
                                const QJsonObject& fields,
                                QString* error)
{
    /**Append one durable event and advance the manifest sequence.

    The event log remains append-only while current-state.md is regenerated as a derived snapshot.
    */
    const QByteArray serializedEvent = QJsonDocument(fields).toJson(QJsonDocument::Compact);
    if (!withinConfiguredLimit(projectRoot, serializedEvent.size() + 1, error)) return false;

    const QString manifestPath = absolutePath(projectRoot, AramfPaths::Manifest);
    QJsonObject manifest = readJsonObject(manifestPath, error);
    if (manifest.isEmpty() && QFile::exists(manifestPath)) {
        return false;
    }

    const qint64 sequence = manifest.value(QStringLiteral("nextSequenceNumber")).toVariant().toLongLong() > 0
                                 ? manifest.value(QStringLiteral("nextSequenceNumber")).toVariant().toLongLong()
                                 : 1;
    QJsonObject event = fields;
    event.insert(QStringLiteral("eventId"), QStringLiteral("event-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    event.insert(QStringLiteral("eventType"), eventType);
    event.insert(QStringLiteral("sequenceNumber"), sequence);
    event.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    event.insert(QStringLiteral("task"), task);

    QFile eventFile(absolutePath(projectRoot, AramfPaths::EventLog));
    if (!eventFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (error) {
            *error = eventFile.errorString();
        }
        return false;
    }
    eventFile.write(QJsonDocument(event).toJson(QJsonDocument::Compact));
    eventFile.write("\n");
    eventFile.close();

    manifest.insert(QStringLiteral("_file"), QStringLiteral("memory-manifest.json"));
    manifest.insert(QStringLiteral("memoryVersion"), QStringLiteral("3"));
    manifest.insert(QStringLiteral("nextSequenceNumber"), sequence + 1);
    manifest.insert(QStringLiteral("eventCount"), manifest.value(QStringLiteral("eventCount")).toInt() + 1);
    manifest.insert(QStringLiteral("latestEventId"), event.value(QStringLiteral("eventId")));
    if (!writeJsonFile(manifestPath, manifest, error)) {
        return false;
    }

    return generateCurrentState(projectRoot, error);
}

QStringList ProjectMemory::supportedRecordOperations()
{
    return {QStringLiteral("task-start"), QStringLiteral("task-complete"), QStringLiteral("build-result"),
            QStringLiteral("test-result"), QStringLiteral("validation-result")};
}

bool ProjectMemory::recordDecision(const QString& projectRoot,
                                   const QString& decisionId,
                                   const QString& topic,
                                   const QString& summary,
                                   const QString& status,
                                   const QString& supersededBy,
                                   QString* error)
{
    if (decisionId.trimmed().isEmpty() || decisionId.size() > 128
        || topic.trimmed().isEmpty() || topic.size() > 128
        || summary.trimmed().isEmpty() || summary.size() > 2048
        || (status != QStringLiteral("current") && status != QStringLiteral("superseded")
            && status != QStringLiteral("historical"))) {
        if (error) *error = QStringLiteral("Decision fields are invalid.");
        return false;
    }
    QString configError;
    const QJsonObject config = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryConfiguration), &configError);
    if (!configError.isEmpty() || !config.value(QStringLiteral("maintenanceOptions")).toArray().contains(QStringLiteral("record-decisions"))) {
        if (error) *error = QStringLiteral("Decision recording is disabled by configuration.");
        return false;
    }
    QString decisionsError;
    const QString decisionsPath = absolutePath(projectRoot, AramfPaths::Decisions);
    const auto existing = readDecisionRecords(decisionsPath, &decisionsError);
    if (!decisionsError.isEmpty()) {
        if (error) *error = decisionsError;
        return false;
    }
    for (const auto& decision : existing) {
        if (decision.id == decisionId) {
            if (error) *error = QStringLiteral("Decision ID already exists: %1").arg(decisionId);
            return false;
        }
    }

    QFile original(decisionsPath);
    if (!original.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = original.errorString();
        return false;
    }
    const QByteArray previous = original.readAll();
    original.close();
    const auto snapshots = snapshotFiles(projectRoot, {
        AramfPaths::Decisions, AramfPaths::EventLog, AramfPaths::Manifest,
        AramfPaths::CurrentState, AramfPaths::ConsistencyValidation
    });
    const QByteArray block = QStringLiteral(
        "\n## Decision Record: %1\n\n"
        "<!-- ARAMF-DECISION -->\n"
        "- Decision-ID: %1\n"
        "- Topic: %2\n"
        "- Status: %3\n"
        "- Superseded-By: %4\n"
        "- Summary: %5\n"
        "<!-- /ARAMF-DECISION -->\n")
                                  .arg(decisionId, topic, status, supersededBy.isEmpty() ? QStringLiteral("none") : supersededBy, summary)
                                  .toUtf8();
    if (!writeTextFile(decisionsPath, previous + block, error)) return false;

    const QJsonObject fields {
        {QStringLiteral("decisionId"), decisionId},
        {QStringLiteral("topic"), topic},
        {QStringLiteral("decisionStatus"), status},
        {QStringLiteral("supersededBy"), supersededBy}
    };
    if (!appendEvent(projectRoot, QStringLiteral("DECISION_RECORDED"), summary, fields, error)) {
        restoreSnapshots(snapshots, nullptr);
        return false;
    }
    generateColdStartValidation(projectRoot, error);
    const auto report = validate(projectRoot, error);
    if (report.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")) {
        restoreSnapshots(snapshots, nullptr);
        return false;
    }
    return true;
}

bool ProjectMemory::recordCheckpoint(const QString& projectRoot,
                                     const QString& title,
                                     const QString& summary,
                                     const QString& relatedTask,
                                     const QString& commit,
                                     const QString& verificationStatus,
                                     QJsonObject* result,
                                     QString* error)
{
    if (title.trimmed().isEmpty() || title.size() > 256
        || summary.trimmed().isEmpty() || summary.size() > 2048
        || relatedTask.size() > 512 || commit.size() > 128 || verificationStatus.size() > 64) {
        if (error) *error = QStringLiteral("Checkpoint fields are invalid.");
        return false;
    }
    QString configError;
    const QJsonObject config = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryConfiguration), &configError);
    if (!configError.isEmpty() || !config.value(QStringLiteral("maintenanceOptions")).toArray().contains(QStringLiteral("record-checkpoints"))) {
        if (error) *error = QStringLiteral("Checkpoint recording is disabled by configuration.");
        return false;
    }

    QString checkpointError;
    QJsonObject checkpointFile;
    QJsonArray existing;
    QFile checkpointInput(absolutePath(projectRoot, AramfPaths::Checkpoints));
    if (!checkpointInput.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = checkpointInput.errorString();
        return false;
    }
    QJsonParseError checkpointParseError;
    const QJsonDocument checkpointDocument = QJsonDocument::fromJson(checkpointInput.readAll(), &checkpointParseError);
    if (checkpointParseError.error != QJsonParseError::NoError
        || (!checkpointDocument.isArray() && !checkpointDocument.isObject())) {
        if (error) *error = checkpointParseError.errorString();
        return false;
    }
    if (checkpointDocument.isArray()) {
        existing = checkpointDocument.array();
        checkpointFile.insert(QStringLiteral("_file"), QStringLiteral("checkpoints.json"));
    } else {
        checkpointFile = checkpointDocument.object();
        if (!checkpointFile.value(QStringLiteral("checkpoints")).isArray()) {
            if (error) *error = QStringLiteral("Checkpoint file is malformed.");
            return false;
        }
        existing = checkpointFile.value(QStringLiteral("checkpoints")).toArray();
    }
    checkpointInput.close();
    QSet<QString> checkpointIds;
    for (const auto& value : existing) {
        const QString id = value.toObject().value(QStringLiteral("id")).toString();
        if (id.isEmpty() || checkpointIds.contains(id)) {
            if (error) *error = QStringLiteral("Checkpoint file contains duplicate or invalid IDs.");
            return false;
        }
        checkpointIds.insert(id);
    }

    QString eventError;
    const QList<QJsonObject> events = readEvents(absolutePath(projectRoot, AramfPaths::EventLog), &eventError);
    if (!eventError.isEmpty()) {
        if (error) *error = eventError;
        return false;
    }
    qint64 productionSequence = 0;
    QString latestProductionEvent;
    for (const auto& event : events) {
        const qint64 sequence = event.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
        if (!isControlPlaneEvent(event.value(QStringLiteral("eventType")).toString()) && sequence >= productionSequence) {
            productionSequence = sequence;
            latestProductionEvent = event.value(QStringLiteral("eventId")).toString();
        }
    }

    const QString checkpointId = QStringLiteral("checkpoint-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QJsonObject fields {
        {QStringLiteral("checkpointId"), checkpointId},
        {QStringLiteral("productionSequence"), productionSequence},
        {QStringLiteral("latestProductionEventId"), latestProductionEvent}
    };
    if (!relatedTask.isEmpty()) fields.insert(QStringLiteral("relatedTask"), relatedTask);
    if (!commit.isEmpty()) fields.insert(QStringLiteral("commit"), commit);
    if (!verificationStatus.isEmpty()) fields.insert(QStringLiteral("verificationStatus"), verificationStatus);

    const auto snapshots = snapshotFiles(projectRoot, {
        AramfPaths::Checkpoints, AramfPaths::EventLog, AramfPaths::Manifest,
        AramfPaths::CurrentState, AramfPaths::ConsistencyValidation, AramfPaths::ColdStartValidation
    });
    if (!appendEvent(projectRoot, QStringLiteral("CHECKPOINT_CREATED"), summary, fields, error)) {
        restoreSnapshots(snapshots, nullptr);
        return false;
    }

    const QList<QJsonObject> updatedEvents = readEvents(absolutePath(projectRoot, AramfPaths::EventLog), error);
    if (error && !error->isEmpty()) {
        restoreSnapshots(snapshots, nullptr);
        return false;
    }
    QString checkpointEventId;
    for (auto it = updatedEvents.crbegin(); it != updatedEvents.crend(); ++it) {
        if (it->value(QStringLiteral("eventType")).toString() == QStringLiteral("CHECKPOINT_CREATED")
            && it->value(QStringLiteral("checkpointId")).toString() == checkpointId) {
            checkpointEventId = it->value(QStringLiteral("eventId")).toString();
            break;
        }
    }
    if (checkpointEventId.isEmpty()) {
        restoreSnapshots(snapshots, nullptr);
        if (error) *error = QStringLiteral("Checkpoint event was not written.");
        return false;
    }

    QJsonObject checkpoint {
        {QStringLiteral("id"), checkpointId},
        {QStringLiteral("title"), title.trimmed()},
        {QStringLiteral("summary"), summary.trimmed()},
        {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("productionSequence"), productionSequence},
        {QStringLiteral("latestEventId"), checkpointEventId}
    };
    if (!relatedTask.isEmpty()) checkpoint.insert(QStringLiteral("relatedTask"), relatedTask);
    if (!commit.isEmpty()) checkpoint.insert(QStringLiteral("commit"), commit);
    if (!verificationStatus.isEmpty()) checkpoint.insert(QStringLiteral("verificationStatus"), verificationStatus);
    QJsonArray checkpoints = existing;
    checkpoints.append(checkpoint);
    checkpointFile.insert(QStringLiteral("checkpoints"), checkpoints);
    checkpointFile.insert(QStringLiteral("_file"), QStringLiteral("checkpoints.json"));
    if (!withinConfiguredLimit(projectRoot, QJsonDocument(checkpoint).toJson(QJsonDocument::Compact).size(), error)
        || !writeJsonFile(absolutePath(projectRoot, AramfPaths::Checkpoints), checkpointFile, error)) {
        restoreSnapshots(snapshots, nullptr);
        return false;
    }
    generateColdStartValidation(projectRoot, error);
    const auto report = validate(projectRoot, error);
    if (report.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")) {
        restoreSnapshots(snapshots, nullptr);
        return false;
    }
    if (result) {
        *result = checkpoint;
    }
    return true;
}

bool ProjectMemory::recordOperation(const QString& projectRoot,
                                    const QString& operation,
                                    const QJsonObject& fields,
                                    QJsonObject* result,
                                    QString* error)
{
    if (!supportedRecordOperations().contains(operation)) {
        if (error) *error = QStringLiteral("Unknown recording operation: %1").arg(operation);
        return false;
    }
    const QString option = recordingOptionFor(operation);
    QString configError;
    const QJsonObject config = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryConfiguration), &configError);
    if (!configError.isEmpty() || config.isEmpty()) {
        if (error) *error = QStringLiteral("Memory configuration is unavailable.");
        return false;
    }
    const QJsonArray configured = config.value(QStringLiteral("maintenanceOptions")).toArray();
    bool enabled = false;
    for (const auto& value : configured) enabled |= value.toString() == option;
    if (!enabled) {
        if (error) *error = QStringLiteral("Recording disabled by configuration: %1").arg(option);
        return false;
    }
    if (operation != QStringLiteral("task-start")) {
        const QString status = fields.value(QStringLiteral("status")).toString();
        if (!validResultStatus(status)) {
            if (error) *error = QStringLiteral("status must be PASS or FAIL.");
            return false;
        }
    }
    const QString task = fields.value(QStringLiteral("task")).toString().trimmed();
    if (task.isEmpty() || task.size() > 512) {
        if (error) *error = QStringLiteral("task is required and must be at most 512 characters.");
        return false;
    }
    const QStringList protectedFiles {
        AramfPaths::EventLog, AramfPaths::Manifest, AramfPaths::CurrentState,
        AramfPaths::Metrics, AramfPaths::ConsistencyValidation, AramfPaths::ColdStartValidation,
        AramfPaths::ProjectStatus
    };
    const auto snapshots = snapshotFiles(projectRoot, protectedFiles);
    QJsonObject eventFields = fields;
    eventFields.insert(QStringLiteral("task"), task);
    eventFields.remove(QStringLiteral("operation"));
    const QString eventType = eventTypeFor(operation);
    if (!appendEvent(projectRoot, eventType, task, eventFields, error)) {
        QString rollbackError;
        restoreSnapshots(snapshots, &rollbackError);
        return false;
    }

    const bool updateCurrentState = configured.contains(QStringLiteral("update-current-state"));
    if (!updateCurrentState) {
        const auto currentStateSnapshot = std::find_if(
            snapshots.cbegin(), snapshots.cend(), [](const FileSnapshot& snapshot) {
                return snapshot.path.endsWith(AramfPaths::CurrentState);
            });
        if (currentStateSnapshot != snapshots.cend()) {
            QString restoreError;
            if (!restoreSnapshot(*currentStateSnapshot, &restoreError)) {
                restoreSnapshots(snapshots, nullptr);
                if (error) *error = restoreError;
                return false;
            }
        }
    }

    QJsonObject metrics = readJsonObject(absolutePath(projectRoot, AramfPaths::Metrics), error);
    if (metrics.isEmpty()) {
        QString rollbackError;
        restoreSnapshots(snapshots, &rollbackError);
        if (error && error->isEmpty()) *error = QStringLiteral("Metrics file is unavailable.");
        return false;
    }
    auto increment = [&metrics](const QString& key) {
        metrics.insert(key, metrics.value(key).toInt() + 1);
    };
    if (operation == QStringLiteral("task-complete")) increment(QStringLiteral("iterations"));
    if (operation == QStringLiteral("build-result")) increment(QStringLiteral("buildAttempts"));
    if (operation == QStringLiteral("test-result")) increment(QStringLiteral("testAttempts"));
    if (fields.value(QStringLiteral("status")).toString() == QStringLiteral("FAIL")) increment(QStringLiteral("failures"));
    if (!writeJsonFile(absolutePath(projectRoot, AramfPaths::Metrics), metrics, error)) {
        QString rollbackError;
        restoreSnapshots(snapshots, &rollbackError);
        return false;
    }

    if (operation == QStringLiteral("task-complete")
        && config.value(QStringLiteral("maintenanceOptions")).toArray().contains(QStringLiteral("update-project-status"))) {
        QFile statusFile(absolutePath(projectRoot, AramfPaths::ProjectStatus));
        if (!statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString rollbackError;
            restoreSnapshots(snapshots, &rollbackError);
            if (error) *error = statusFile.errorString();
            return false;
        }
        QString status = QString::fromUtf8(statusFile.readAll());
        statusFile.close();
        const QString marker = QStringLiteral("## Latest Agent Task");
        const QString section = QStringLiteral("%1\n\n- Task: %2\n- Status: %3\n")
                                    .arg(marker, task, fields.value(QStringLiteral("status")).toString());
        const int markerAt = status.indexOf(marker);
        if (markerAt >= 0) status.truncate(markerAt);
        status += QLatin1Char('\n') + section;
        if (!writeTextFile(absolutePath(projectRoot, AramfPaths::ProjectStatus), status.toUtf8(), error)) {
            QString rollbackError;
            restoreSnapshots(snapshots, &rollbackError);
            return false;
        }
    }

    if (!generateColdStartValidation(projectRoot, error)) {
        QString rollbackError;
        restoreSnapshots(snapshots, &rollbackError);
        return false;
    }
    QString validationError;
    const auto validation = validate(projectRoot, &validationError);
    if (!validationError.isEmpty() || validation.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")) {
        QString rollbackError;
        restoreSnapshots(snapshots, &rollbackError);
        if (error) *error = validationError.isEmpty() ? QStringLiteral("Memory consistency validation failed.") : validationError;
        return false;
    }
    if (result) {
        const QJsonObject manifest = readJsonObject(absolutePath(projectRoot, AramfPaths::Manifest), nullptr);
        result->insert(QStringLiteral("operation"), operation);
        result->insert(QStringLiteral("eventType"), eventType);
        result->insert(QStringLiteral("eventId"), manifest.value(QStringLiteral("latestEventId")));
        result->insert(QStringLiteral("sequenceNumber"), manifest.value(QStringLiteral("nextSequenceNumber")).toInt() - 1);
        result->insert(QStringLiteral("status"), QStringLiteral("PASS"));
    }
    return true;
}

qint64 ProjectMemory::managedMemoryUsage(const QString& projectRoot) const
{
    qint64 total = 0;
    QStack<QString> directories;
    directories.push(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/memory")));
    while (!directories.isEmpty()) {
        const QDir directory(directories.pop());
        for (const auto& entry : directory.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.isDir()) directories.push(entry.absoluteFilePath());
            else total += entry.size();
        }
    }
    return total;
}

qint64 ProjectMemory::memoryUsageBytes(const QString& projectRoot) const
{
    return managedMemoryUsage(projectRoot);
}

bool ProjectMemory::withinConfiguredLimit(const QString& projectRoot, qint64 additionalBytes, QString* error)
{
    const QJsonObject configuration = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryConfiguration), nullptr);
    const qint64 maximum = configuration.value(QStringLiteral("maximumSizeBytes")).toVariant().toLongLong() > 0
        ? configuration.value(QStringLiteral("maximumSizeBytes")).toVariant().toLongLong()
        : 10LL * 1024LL * 1024LL * 1024LL;
    const qint64 current = managedMemoryUsage(projectRoot);
    if (additionalBytes < 0 || current <= maximum && additionalBytes <= maximum - current) return true;

    const qint64 target = maximum - maximum / 10;
    QList<QJsonObject> events = readEvents(absolutePath(projectRoot, AramfPaths::EventLog), error);
    if (events.isEmpty() && current > target) {
        if (error) *error = QStringLiteral("Protected project memory exceeds the configured limit.");
        return false;
    }
    auto protectedEvent = [](const QJsonObject& event) {
        const QString type = event.value(QStringLiteral("eventType")).toString();
        return isControlPlaneEvent(type) || type.contains(QStringLiteral("DECISION"), Qt::CaseInsensitive)
            || type.contains(QStringLiteral("STATUS"), Qt::CaseInsensitive)
            || type.contains(QStringLiteral("SOURCE"), Qt::CaseInsensitive);
    };
    QList<QJsonObject> retained;
    qint64 reclaimed = 0;
    const auto exceedsTarget = [&]() {
        const qint64 remaining = current - qMin(current, reclaimed);
        return remaining > target || additionalBytes > target - remaining;
    };
    for (const auto& event : events) {
        const qint64 eventSize = QJsonDocument(event).toJson(QJsonDocument::Compact).size() + 1;
        if (exceedsTarget() && !protectedEvent(event)) {
            reclaimed += eventSize;
        } else {
            retained.append(event);
        }
    }
    if (reclaimed == 0) {
        if (error) *error = QStringLiteral("Protected project memory exceeds the configured limit.");
        return false;
    }
    QSaveFile eventFile(absolutePath(projectRoot, AramfPaths::EventLog));
    if (!eventFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = eventFile.errorString();
        return false;
    }
    for (const auto& event : retained) {
        eventFile.write(QJsonDocument(event).toJson(QJsonDocument::Compact));
        eventFile.write("\n");
    }
    if (!eventFile.commit()) {
        if (error) *error = eventFile.errorString();
        return false;
    }
    QJsonObject manifest = readJsonObject(absolutePath(projectRoot, AramfPaths::Manifest), error);
    manifest.insert(QStringLiteral("eventCount"), retained.size());
    if (!writeJsonFile(absolutePath(projectRoot, AramfPaths::Manifest), manifest, error)) return false;
    QString validationError;
    if (validate(projectRoot, &validationError).value(QStringLiteral("status")).toString() != QStringLiteral("PASS")) {
        if (error) *error = validationError.isEmpty() ? QStringLiteral("Memory consistency validation failed after automatic cleanup.") : validationError;
        return false;
    }
    const qint64 remaining = managedMemoryUsage(projectRoot);
    if (remaining <= maximum && additionalBytes <= maximum - remaining) return true;
    if (error) *error = QStringLiteral("Protected project memory exceeds the configured limit.");
    return false;
}

QJsonObject ProjectMemory::validate(const QString& projectRoot, QString* error) const
{
    /**Validate ordering, uniqueness, counts, and manifest sequence integrity.

    The returned report is also persisted as the canonical memory-consistency validation artifact.
    */
    QString localError;
    QJsonObject manifest = readJsonObject(absolutePath(projectRoot, AramfPaths::Manifest), &localError);
    QList<QJsonObject> events = readEvents(absolutePath(projectRoot, AramfPaths::EventLog), &localError);

    QJsonArray checks;
    QJsonArray errors;
    auto addCheck = [&checks, &errors](const QString& name, bool pass, const QString& message) {
        QJsonObject check;
        check.insert(QStringLiteral("name"), name);
        check.insert(QStringLiteral("status"), pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
        checks.append(check);
        if (!pass) {
            errors.append(message);
        }
    };

    if (!localError.isEmpty()) {
        errors.append(localError);
    }

    QSet<QString> ids;
    QSet<qint64> sequences;
    qint64 previous = 0;
    qint64 maximum = 0;
    qint64 productionMaximum = 0;
    bool uniqueIds = true;
    bool orderedSequences = true;

    for (const QJsonObject& event : events) {
        const QString id = event.value(QStringLiteral("eventId")).toString();
        const qint64 sequence = event.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
        if (id.isEmpty() || ids.contains(id)) {
            uniqueIds = false;
        }
        ids.insert(id);
        if (sequence <= previous || sequences.contains(sequence)) {
            orderedSequences = false;
        }
        previous = sequence;
        sequences.insert(sequence);
        maximum = qMax(maximum, sequence);
        if (!isControlPlaneEvent(event.value(QStringLiteral("eventType")).toString())) {
            productionMaximum = qMax(productionMaximum, sequence);
        }
    }

    addCheck(QStringLiteral("event-identifiers-unique"), uniqueIds, QStringLiteral("Event IDs must be present and unique."));
    addCheck(QStringLiteral("sequence-order"), orderedSequences, QStringLiteral("Event sequences must be strictly increasing and unique."));
    addCheck(QStringLiteral("manifest-next-sequence"),
             manifest.value(QStringLiteral("nextSequenceNumber")).toVariant().toLongLong() == maximum + 1,
             QStringLiteral("Manifest nextSequenceNumber is stale."));
    addCheck(QStringLiteral("manifest-event-count"),
             manifest.value(QStringLiteral("eventCount")).toInt() == events.size(),
             QStringLiteral("Manifest eventCount does not match the event log."));

    QString checkpointReadError;
    QJsonValue checkpointValue;
    QFile checkpointInput(absolutePath(projectRoot, AramfPaths::Checkpoints));
    QJsonParseError checkpointParseError;
    if (checkpointInput.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument checkpointDocument = QJsonDocument::fromJson(checkpointInput.readAll(), &checkpointParseError);
        checkpointValue = checkpointDocument.isArray() ? QJsonValue(checkpointDocument.array())
                                                        : checkpointDocument.object().value(QStringLiteral("checkpoints"));
        checkpointInput.close();
    } else {
        checkpointReadError = checkpointInput.errorString();
    }
    bool checkpointShapeOk = checkpointReadError.isEmpty()
        && checkpointParseError.error == QJsonParseError::NoError && checkpointValue.isArray();
    bool checkpointIdsOk = checkpointShapeOk;
    bool checkpointFieldsOk = checkpointShapeOk;
    bool checkpointReferencesOk = checkpointShapeOk;
    QSet<QString> checkpointIds;
    if (checkpointShapeOk) {
        for (const auto& value : checkpointValue.toArray()) {
            const QJsonObject checkpoint = value.toObject();
            const QString id = checkpoint.value(QStringLiteral("id")).toString();
            const QString title = checkpoint.value(QStringLiteral("title")).toString();
            const QString summary = checkpoint.value(QStringLiteral("summary")).toString();
            const QString createdAt = checkpoint.value(QStringLiteral("createdAt")).toString();
            const qint64 productionSequenceReference = checkpoint.value(QStringLiteral("productionSequence")).toVariant().toLongLong();
            const QString eventId = checkpoint.value(QStringLiteral("latestEventId")).toString();
            if (id.isEmpty() || checkpointIds.contains(id)) checkpointIdsOk = false;
            checkpointIds.insert(id);
            if (title.trimmed().isEmpty() || summary.trimmed().isEmpty()
                || QDateTime::fromString(createdAt, Qt::ISODate).isValid()
                == false || productionSequenceReference < 0) checkpointFieldsOk = false;
            if (productionSequenceReference > productionMaximum || (!eventId.isEmpty() && !ids.contains(eventId))) {
                checkpointReferencesOk = false;
            }
        }
    }
    addCheck(QStringLiteral("checkpoint-identifiers-unique"), checkpointIdsOk,
             QStringLiteral("Checkpoint IDs must be present and unique."));
    addCheck(QStringLiteral("checkpoint-fields-valid"), checkpointFieldsOk,
             QStringLiteral("Checkpoint records contain invalid required fields."));
    addCheck(QStringLiteral("checkpoint-references-valid"), checkpointReferencesOk,
             QStringLiteral("Checkpoint references must point to current production state and existing events."));

    QString configError;
    const QJsonObject config = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryConfiguration), &configError);
    const QJsonArray validationOptions = config.value(QStringLiteral("validationOptions")).toArray();
    QJsonArray warnings;
    auto addWarning = [&warnings](const QString& name, const QString& message) {
        warnings.append(QJsonObject{{QStringLiteral("name"), name},
                                    {QStringLiteral("status"), QStringLiteral("WARN")},
                                    {QStringLiteral("message"), message}});
    };

    if (validationOptions.contains(QStringLiteral("conflicting-decisions"))) {
        QString decisionsError;
        const auto decisions = readDecisionRecords(absolutePath(projectRoot, AramfPaths::Decisions), &decisionsError);
        QHash<QString, QString> activeTopics;
        bool conflictFree = decisionsError.isEmpty();
        QString conflictMessage;
        for (const auto& decision : decisions) {
            if (decision.status != QStringLiteral("current")) continue;
            if (activeTopics.contains(decision.topic) && activeTopics.value(decision.topic) != decision.id) {
                conflictFree = false;
                conflictMessage = QStringLiteral("Active decisions conflict for topic '%1': %2 and %3.")
                                      .arg(decision.topic, activeTopics.value(decision.topic), decision.id);
                break;
            }
            activeTopics.insert(decision.topic, decision.id);
        }
        addCheck(QStringLiteral("conflicting-decisions"), conflictFree,
                 conflictMessage.isEmpty() ? decisionsError : conflictMessage);
    }

    if (validationOptions.contains(QStringLiteral("stale-current-state"))) {
        QFile stateFile(absolutePath(projectRoot, AramfPaths::CurrentState));
        QString stateText;
        if (stateFile.open(QIODevice::ReadOnly | QIODevice::Text)) stateText = QString::fromUtf8(stateFile.readAll());
        const auto stateField = [&stateText](const QString& heading) {
            const QString marker = QStringLiteral("## ") + heading;
            const int start = stateText.indexOf(marker);
            if (start < 0) return QString();
            const int valueStart = stateText.indexOf(QLatin1Char('\n'), start);
            if (valueStart < 0) return QString();
            const int nextHeading = stateText.indexOf(QStringLiteral("\n## "), valueStart + 1);
            const int end = nextHeading >= 0 ? nextHeading : stateText.size();
            return stateText.mid(valueStart + 1, end - valueStart - 1).trimmed();
        };
        qint64 expectedProduction = 0;
        QString expectedProductionId;
        for (const auto& event : events) {
            if (isControlPlaneEvent(event.value(QStringLiteral("eventType")).toString())) continue;
            const qint64 sequence = event.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
            if (sequence >= expectedProduction) {
                expectedProduction = sequence;
                expectedProductionId = event.value(QStringLiteral("eventId")).toString();
            }
        }
        const QString durableValue = stateField(QStringLiteral("Latest Durable Sequence"));
        const QString productionValue = stateField(QStringLiteral("Latest Production Sequence"));
        const QString stateEvent = stateField(QStringLiteral("Latest Production Development Event"));
        const qint64 stateDurable = durableValue.toLongLong();
        const qint64 stateProduction = productionValue.toLongLong();
        const bool currentStateOk = stateDurable == maximum && stateProduction == expectedProduction
            && stateEvent == expectedProductionId;
        addCheck(QStringLiteral("stale-current-state"), currentStateOk,
                 QStringLiteral("current-state.md does not match the latest accepted event state."));
    }

    if (validationOptions.contains(QStringLiteral("project-status-consistency"))) {
        QFile statusFile(absolutePath(projectRoot, AramfPaths::ProjectStatus));
        QString statusText;
        const bool readable = statusFile.open(QIODevice::ReadOnly | QIODevice::Text);
        if (readable) statusText = QString::fromUtf8(statusFile.readAll());
        const bool taskMetadataValid = !statusText.contains(QStringLiteral("## Latest Agent Task"))
            || (statusText.contains(QStringLiteral("- Task:")) && statusText.contains(QStringLiteral("- Status:")));
        const bool staleAuthority = statusText.contains(QStringLiteral("aramf_setup/PROJECT_STATUS.md is the live"), Qt::CaseInsensitive)
            || statusText.contains(QStringLiteral("aramf_setup/AGENTS.md is the canonical"), Qt::CaseInsensitive);
        addCheck(QStringLiteral("project-status-consistency"), readable && taskMetadataValid && !staleAuthority,
                 QStringLiteral("PROJECT_STATUS.md is missing, malformed, or contains superseded authority wording."));
    }

    if (validationOptions.contains(QStringLiteral("referenced-resources"))) {
        bool referencesOk = true;
        for (const QString& relative : coldStartPaths(true)) {
            if (!QFileInfo::exists(absolutePath(projectRoot, relative))) referencesOk = false;
        }
        addCheck(QStringLiteral("referenced-resources"), referencesOk,
                 QStringLiteral("A required worker memory/control-plane reference is missing."));
    }

    QJsonArray unsupportedOptions;
    if (!configError.isEmpty()) {
        addCheck(QStringLiteral("configured-validation-coverage"), false, configError);
    } else {
        const QSet<QString> supported {
            QStringLiteral("memory-consistency"), QStringLiteral("cold-start-validation"),
            QStringLiteral("sequence-continuity"), QStringLiteral("conflicting-decisions"),
            QStringLiteral("stale-current-state"), QStringLiteral("referenced-resources"),
            QStringLiteral("project-status-consistency")};
        for (const auto& option : validationOptions) {
            if (!supported.contains(option.toString())) unsupportedOptions.append(option);
        }
        if (!unsupportedOptions.isEmpty()) {
            addCheck(QStringLiteral("configured-validation-coverage"), false,
                     QStringLiteral("Unsupported validation options are configured."));
        } else {
            addCheck(QStringLiteral("configured-validation-coverage"), true,
                     QStringLiteral("All configured validation options are executed."));
        }
    }

    if (validationOptions.contains(QStringLiteral("cold-start-validation"))) {
        const QString coldPath = absolutePath(projectRoot, AramfPaths::ColdStartValidation);
        const QJsonObject cold = readJsonObject(coldPath, nullptr);
        const QString currentFingerprint = coldStartFingerprint(projectRoot, coldStartPaths(true));
        addCheck(QStringLiteral("cold-start-fresh"), cold.value(QStringLiteral("status")).toString() == QStringLiteral("PASS")
                     && cold.value(QStringLiteral("fingerprint")).toString() == currentFingerprint,
                 QStringLiteral("Persisted cold-start validation is missing, failed, or stale."));
    }

    auto mirrorConfiguredCheck = [&checks](const QString& option, const QString& checkName, const QString& message) {
        bool found = false;
        QString status = QStringLiteral("FAIL");
        for (const auto& value : checks) {
            const auto check = value.toObject();
            if (check.value(QStringLiteral("name")).toString() == checkName) {
                found = true;
                status = check.value(QStringLiteral("status")).toString();
                break;
            }
        }
        if (found) checks.append(QJsonObject{{QStringLiteral("name"), option},
                                             {QStringLiteral("status"), status},
                                             {QStringLiteral("message"), message}});
    };
    if (validationOptions.contains(QStringLiteral("memory-consistency"))) {
        mirrorConfiguredCheck(QStringLiteral("memory-consistency"), QStringLiteral("manifest-event-count"),
                              QStringLiteral("Structural memory consistency checks executed."));
    }
    if (validationOptions.contains(QStringLiteral("sequence-continuity"))) {
        mirrorConfiguredCheck(QStringLiteral("sequence-continuity"), QStringLiteral("sequence-order"),
                              QStringLiteral("Sequence continuity check executed."));
    }

    QJsonObject report;
    report.insert(QStringLiteral("_file"), QStringLiteral("memory-consistency-validation.json"));
    report.insert(QStringLiteral("status"), errors.isEmpty() ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
    report.insert(QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    report.insert(QStringLiteral("durableSequence"), maximum);
    report.insert(QStringLiteral("productionSequence"), productionMaximum);
    report.insert(QStringLiteral("checks"), checks);
    report.insert(QStringLiteral("errors"), errors);
    report.insert(QStringLiteral("warnings"), warnings);
    report.insert(QStringLiteral("unsupportedValidations"), unsupportedOptions);

    QString writeError;
    writeValidationReport(projectRoot, report, &writeError);
    if (!writeError.isEmpty() && error) {
        *error = writeError;
    } else if (!localError.isEmpty() && error) {
        *error = localError;
    }
    return report;
}

bool ProjectMemory::ensureDirectories(const QString& projectRoot, QString* error) const
{
    /**Create the complete canonical ARAMF directory hierarchy.

    Custom content is isolated from generated and memory data so automation can protect user ownership boundaries.
    */
    const QStringList directories {
        AramfPaths::ControlDirectory,
        QStringLiteral("ARAMF_WORKER/memory"),
        QStringLiteral("ARAMF_WORKER/rules"),
        QStringLiteral("ARAMF_WORKER/routing"),
        QStringLiteral("ARAMF_WORKER/resources"),
        QStringLiteral("ARAMF_WORKER/templates"),
        QStringLiteral("ARAMF_WORKER/platforms"),
        QStringLiteral("ARAMF_WORKER/verification"),
        QStringLiteral("ARAMF_WORKER/custom"),
        QStringLiteral("ARAMF_WORKER/docs")
    };

    QDir root(projectRoot);
    for (const QString& directory : directories) {
        if (!root.mkpath(directory)) {
            if (error) {
                *error = QStringLiteral("Could not create %1").arg(root.filePath(directory));
            }
            return false;
        }
    }
    return true;
}

bool ProjectMemory::ensureMemoryDirectories(const QString& projectRoot, QString* error) const
{
    if (!QDir(projectRoot).mkpath(QStringLiteral("ARAMF_WORKER/memory"))) {
        if (error) *error = QStringLiteral("Could not create %1").arg(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/memory")));
        return false;
    }
    return true;
}

bool ProjectMemory::writeMemoryFiles(const QString& projectRoot, const ProjectModel* model, QString* error) const
{
    const MemoryConfiguration memory = model ? model->memoryConfiguration() : MemoryConfiguration{};
    const auto operations = supportedRecordOperations();
    QJsonObject memoryConfiguration{
        {QStringLiteral("captureCategories"), stringListToJson(memory.captureCategories)},
        {QStringLiteral("historyOptions"), stringListToJson(memory.historyOptions)},
        {QStringLiteral("maintenanceOptions"), stringListToJson(memory.maintenanceOptions)},
        {QStringLiteral("validationOptions"), stringListToJson(memory.validationOptions)},
        {QStringLiteral("retentionLevel"), memory.retentionLevel},
        {QStringLiteral("updateStrategy"), memory.updateStrategy},
        {QStringLiteral("maximumSizeBytes"), memory.maximumSizeBytes}
    };
    QJsonArray operationNames;
    for (const auto& operation : operations) operationNames.append(operation);
    const QJsonObject contract{
        {QStringLiteral("version"), 1},
        {QStringLiteral("recordingEnabled"), !memory.maintenanceOptions.isEmpty()},
        {QStringLiteral("command"), QJsonObject{
            {QStringLiteral("executable"), QStringLiteral("aramf")},
            {QStringLiteral("syntax"), QStringLiteral("aramf memory record --project <project-root> --operation <operation> ...")},
            {QStringLiteral("projectArgument"), QStringLiteral("--project <project-root>")}}},
        {QStringLiteral("supportedOperations"), operationNames},
        {QStringLiteral("separateOperations"), QJsonObject{
            {QStringLiteral("checkpoints"), QStringLiteral("deliberate stable recovery points; use the checkpoint operation; not created by routine feedback")},
            {QStringLiteral("durableDecisions"), QStringLiteral("deliberate architecture/policy records through the decision workflow")}}},
        {QStringLiteral("checkpointOperation"), QJsonObject{
            {QStringLiteral("command"), QStringLiteral("aramf memory checkpoint --project <project-root> --title <title> --summary <summary> [--task <task>] [--commit <sha>] [--verification-status <status>]")},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("--project"), QStringLiteral("--title"), QStringLiteral("--summary")}},
            {QStringLiteral("optional"), QJsonArray{QStringLiteral("--task"), QStringLiteral("--commit"), QStringLiteral("--verification-status")}},
            {QStringLiteral("deliberate"), true},
            {QStringLiteral("configuredOption"), QStringLiteral("record-checkpoints")}}},
        {QStringLiteral("configuredMaintenanceOptions"), stringListToJson(memory.maintenanceOptions)},
        {QStringLiteral("arguments"), QJsonObject{
            {QStringLiteral("required"), QJsonArray{QStringLiteral("--project"), QStringLiteral("--operation"), QStringLiteral("--task")}},
            {QStringLiteral("optional"), QJsonArray{QStringLiteral("--status"), QStringLiteral("--summary"), QStringLiteral("--detail"),
                                                     QStringLiteral("--category"), QStringLiteral("--issue"), QStringLiteral("--build-system"),
                                                     QStringLiteral("--configuration"), QStringLiteral("--suite"), QStringLiteral("--passed"),
                                                     QStringLiteral("--failed"), QStringLiteral("--total")}}}},
        {QStringLiteral("ownedFiles"), QJsonArray{
            QStringLiteral("memory/event-log.jsonl"), QStringLiteral("memory/metrics.json"),
            QStringLiteral("memory/current-state.md"), QStringLiteral("memory/memory-manifest.json"),
            QStringLiteral("memory/memory-consistency-validation.json"), QStringLiteral("PROJECT_STATUS.md")}},
        {QStringLiteral("directEditing"), QStringLiteral("forbidden: use the ARAMF memory recorder; do not edit owned files directly.")}
    };
    const QList<QPair<QString, QJsonObject>> defaults {
        {AramfPaths::MemoryConfiguration, memoryConfiguration},
        {AramfPaths::MemoryContract, contract},
        {AramfPaths::FrameworkKnowledge, QJsonObject {
            {QStringLiteral("version"), 1},
            {QStringLiteral("authority"), QJsonArray {
                QStringLiteral("explicit-current-user-instruction"),
                QStringLiteral("current-source-of-truth"),
                QStringLiteral("current-durable-project-decisions"),
                QStringLiteral("approved-framework-knowledge"),
                QStringLiteral("templates-and-defaults"),
                QStringLiteral("ai-inference")}},
            {QStringLiteral("entries"), QJsonArray {}}
        }},
        {AramfPaths::Checkpoints, QJsonObject {{QStringLiteral("checkpoints"), QJsonArray {}}}},
        {AramfPaths::Metrics, QJsonObject {{QStringLiteral("iterations"), 0}, {QStringLiteral("buildAttempts"), 0}, {QStringLiteral("testAttempts"), 0}, {QStringLiteral("failures"), 0}}}
    };
    for (const auto& [relativePath, object] : defaults) {
        const bool preserveExisting = relativePath != AramfPaths::MemoryConfiguration
            && relativePath != AramfPaths::MemoryContract;
        if (!writeJsonFile(absolutePath(projectRoot, relativePath), object, error, preserveExisting)) return false;
    }
    if (!writeTextFile(absolutePath(projectRoot, AramfPaths::Decisions),
                      QByteArrayLiteral("<!-- decisions.md -->\n\n# Durable Decisions\n\n"),
                      error, true)) return false;

    QJsonObject manifest {
        {QStringLiteral("memoryVersion"), QStringLiteral("3")},
        {QStringLiteral("nextSequenceNumber"), 1},
        {QStringLiteral("workEntryCount"), 0},
        {QStringLiteral("eventCount"), 0}
    };
    return writeJsonFile(absolutePath(projectRoot, AramfPaths::Manifest), manifest, error, true);
}

bool ProjectMemory::writeInitialFiles(const QString& projectRoot, const ProjectModel* model, QString* error) const
{
    /**Create missing managed framework files with conservative defaults.

    The root AGENTS.md is only a bootstrap; every referenced instruction and state file lives inside ARAMF_WORKER/.
    */
    const QString projectName = model ? model->projectName() : QStringLiteral("Unnamed Project");
    const QString projectId = model ? model->projectId() : QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QByteArray rootAgent = QByteArrayLiteral(
        "<!-- AGENTS.md -->\n\n"
        "# ARAMF Agent Entry Point\n\n"
        "Read and follow `ARAMF_WORKER/AGENTS.md` before making project changes.\n"
        "All ARAMF rule, memory, status, routing, resource, and verification context is stored under `ARAMF_WORKER/`.\n");
    if (!writeTextFile(QDir(projectRoot).filePath(QStringLiteral("AGENTS.md")), rootAgent, error, true)) {
        return false;
    }

    const QByteArray canonicalAgent = QByteArrayLiteral(
        "<!-- AGENTS.md -->\n\n"
        "# Canonical ARAMF Agent Instructions\n\n"
        "## Required startup order\n\n"
        "1. Read `PROJECT_STATUS.md`.\n"
        "2. Read `memory/decisions.md`.\n"
        "3. Read `memory/framework-knowledge.json` and apply only entries whose status is `approved`.\n"
        "4. Read `rules/generated-rules.md`.\n"
        "5. Load only task-relevant files from `routing/`, `resources/`, `platforms/`, and `verification/`.\n"
        "6. Treat `custom/` as user-owned content and never modify it automatically.\n\n"
        "## Project status contract\n\n"
        "Update `PROJECT_STATUS.md` after every meaningful implementation task. Keep it current with what exists, what was changed, verified results, known issues, and the next concrete work. Do not use it as an append-only history.\n\n"
        "## Memory contract\n\n"
        "Record durable architectural choices in `memory/decisions.md`. Keep observations, TODOs, decisions, implementation, and validation separate. Never claim validation without evidence.\n\n"
        "For routine task, build, test, and validation feedback, read `memory/memory-contract.json` and use `aramf memory record --project <project-root> --operation <operation> ...`. Do not edit ProjectMemory-owned bookkeeping files directly. Durable decisions and checkpoints are deliberate separate workflows. Follow current decisions and ignore explicitly superseded decisions.\n\n"
        "## Live Framework Knowledge contract\n\n"
        "`memory/framework-knowledge.json` is live project memory. Approved entries apply immediately in this project and do not require ARAMF regeneration. The authority order is: explicit current user instruction, current Source of Truth, current durable project decisions, approved Framework Knowledge, templates/defaults, then AI inference. When a corrected approach is verified and appears reusable, add or enrich a `candidate` entry with evidence instead of silently changing framework behavior. Never self-approve a candidate. Only after explicit user approval may its status become `approved`; once approved, use it immediately. Keep superseded entries for auditability but do not apply them.\n\n"
        "## Scope\n\n"
        "All paths in this file are relative to the `ARAMF_WORKER/` directory. Do not depend on rule or memory files outside `ARAMF_WORKER/`.\n");
    if (!writeTextFile(absolutePath(projectRoot, AramfPaths::AgentInstructions), canonicalAgent, error, true)) {
        return false;
    }

    const QByteArray status = QStringLiteral(
        "<!-- PROJECT_STATUS.md -->\n\n"
        "# Project Status\n\n"
        "## Project\n\n"
        "- Name: %1\n"
        "- Project ID: %2\n"
        "- ARAMF state: Initialized\n\n"
        "## Implemented\n\n"
        "- ARAMF control-plane structure created.\n\n"
        "## Verified\n\n"
        "- Memory structure initialized.\n\n"
        "## Known Issues\n\n"
        "- None recorded.\n\n"
        "## Next Work\n\n"
        "- Continue project implementation and keep this file synchronized.\n")
                                  .arg(projectName, projectId)
                                  .toUtf8();
    if (!writeTextFile(absolutePath(projectRoot, AramfPaths::ProjectStatus), status, error, true)) {
        return false;
    }

    const QByteArray rules = QByteArrayLiteral(
        "<!-- generated-rules.md -->\n\n"
        "# Generated Rules\n\n"
        "No active rule categories are configured in this control-plane skeleton.\n");
    if (!writeTextFile(absolutePath(projectRoot, AramfPaths::GeneratedRules), rules, error, true)) {
        return false;
    }

    const QByteArray decisions = QByteArrayLiteral("<!-- decisions.md -->\n\n# Durable Decisions\n\n");
    if (!writeTextFile(absolutePath(projectRoot, AramfPaths::Decisions), decisions, error, true)) {
        return false;
    }

    QJsonObject profile {
        {QStringLiteral("name"), QStringLiteral("AI Rules & Memory Framework")},
        {QStringLiteral("version"), 2},
        {QStringLiteral("projectId"), projectId},
        {QStringLiteral("projectName"), projectName},
        {QStringLiteral("implementationLanguage"), QStringLiteral("C++")},
        {QStringLiteral("controlDirectory"), AramfPaths::ControlDirectory}
    };
    if (!writeJsonFile(absolutePath(projectRoot, AramfPaths::Profile), profile, error, true)) {
        return false;
    }

    if (!writeMemoryFiles(projectRoot, model, error)) return false;
    const QList<QPair<QString, QJsonObject>> defaults {
        {AramfPaths::TaskRoutes, QJsonObject {{QStringLiteral("routes"), QJsonArray {}}}},
        {AramfPaths::ScopeRoutes, QJsonObject {{QStringLiteral("routes"), QJsonArray {}}}},
        {AramfPaths::ValidationPolicy, ValidationRouting::policy()},
        {AramfPaths::ResourceManifest, QJsonObject {{QStringLiteral("resources"), QJsonArray {}}}},
        {AramfPaths::CustomTemplates, QJsonObject {{QStringLiteral("templates"), QJsonArray {}}}},
        {AramfPaths::Provenance, QJsonObject {{QStringLiteral("status"), QStringLiteral("managed")}, {QStringLiteral("implementation"), QStringLiteral("C++")}}},
        {AramfPaths::SelectionEffects, QJsonObject {{QStringLiteral("rulesSelected"), QJsonArray {}}, {QStringLiteral("resourcesSelected"), QJsonArray {}}}}
    };
    for (const auto& [relativePath, object] : defaults) {
        if (!writeJsonFile(absolutePath(projectRoot, relativePath), object, error, true)) {
            return false;
        }
    }

    const QString routingReadme = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/README.md"));
    const QByteArray routing = QByteArrayLiteral(
        "<!-- README.md -->\n\n"
        "# Routing\n\n"
        "Task and scope routes select the smallest relevant ARAMF context for the active work.\n");
    return writeTextFile(routingReadme, routing, error, true);
}

bool ProjectMemory::generateCurrentState(const QString& projectRoot, QString* error) const
{
    /**Regenerate the compact current-state snapshot from durable events.

    The snapshot is derived data and may be overwritten; PROJECT_STATUS.md remains the human/agent-maintained live project summary.
    */
    QString eventError;
    const QList<QJsonObject> events = readEvents(absolutePath(projectRoot, AramfPaths::EventLog), &eventError);
    if (!eventError.isEmpty()) {
        if (error) {
            *error = eventError;
        }
        return false;
    }

    qint64 durableSequence = 0;
    qint64 productionSequence = 0;
    QString latestProductionEvent;
    for (const QJsonObject& event : events) {
        const qint64 sequence = event.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
        durableSequence = qMax(durableSequence, sequence);
        if (!isControlPlaneEvent(event.value(QStringLiteral("eventType")).toString()) && sequence >= productionSequence) {
            productionSequence = sequence;
            latestProductionEvent = event.value(QStringLiteral("eventId")).toString();
        }
    }

    const QByteArray content = QStringLiteral(
        "<!-- current-state.md -->\n\n"
        "# Current Project State\n\n"
        "## Latest Durable Sequence\n\n"
        "%1\n\n"
        "## Latest Production Development Event\n\n"
        "%2\n\n"
        "## Latest Production Sequence\n\n"
        "%3\n")
                                   .arg(durableSequence)
                                   .arg(latestProductionEvent)
                                   .arg(productionSequence)
                                   .toUtf8();
    return writeTextFile(absolutePath(projectRoot, AramfPaths::CurrentState), content, error);
}

bool ProjectMemory::generateColdStartValidation(const QString& projectRoot, QString* error, bool requireControlPlane) const
{
    const QStringList mandatory = coldStartPaths(requireControlPlane);
    QJsonArray checks;
    QJsonArray errors;
    for (const QString& relativePath : mandatory) {
        QFile file(absolutePath(projectRoot, relativePath));
        const bool exists = file.exists() && file.open(QIODevice::ReadOnly);
        checks.append(QJsonObject {
            {QStringLiteral("name"), relativePath},
            {QStringLiteral("status"), exists ? QStringLiteral("PASS") : QStringLiteral("FAIL")}
        });
        if (!exists) {
            errors.append(QStringLiteral("Missing %1").arg(relativePath));
        }
    }

    const QJsonObject config = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryConfiguration), nullptr);
    const QJsonObject contract = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryContract), nullptr);
    const bool recordingEnabled = !config.value(QStringLiteral("maintenanceOptions")).toArray().isEmpty();
    QFile agentFile(absolutePath(projectRoot, AramfPaths::AgentInstructions));
    QString agentText;
    if (agentFile.open(QIODevice::ReadOnly | QIODevice::Text)) agentText = QString::fromUtf8(agentFile.readAll());
    const bool contractDiscoverable = !recordingEnabled || !requireControlPlane
        || (agentText.contains(QStringLiteral("memory/memory-contract.json"))
            && agentText.contains(QStringLiteral("aramf memory record")));
    if (!contractDiscoverable) errors.append(QStringLiteral("Memory contract is not discoverable from AGENTS.md."));

    const QSet<QString> supportedOperations {
        QStringLiteral("task-start"), QStringLiteral("task-complete"), QStringLiteral("build-result"),
        QStringLiteral("test-result"), QStringLiteral("validation-result")};
    const QJsonArray contractOperations = contract.value(QStringLiteral("supportedOperations")).toArray();
    QSet<QString> configuredOptions;
    for (const auto& option : config.value(QStringLiteral("maintenanceOptions")).toArray()) {
        configuredOptions.insert(option.toString());
    }
    const bool contractConfigConsistent = !recordingEnabled || (!contract.isEmpty()
        && ((configuredOptions.contains(QStringLiteral("record-task-completion")) && contractOperations.contains(QStringLiteral("task-complete")))
            || !configuredOptions.contains(QStringLiteral("record-task-completion")))
        && ((!configuredOptions.contains(QStringLiteral("record-build-results")))
            || contractOperations.contains(QStringLiteral("build-result")))
        && ((!configuredOptions.contains(QStringLiteral("record-test-results")))
            || contractOperations.contains(QStringLiteral("test-result")))
        && ((!configuredOptions.contains(QStringLiteral("record-validation")))
            || contractOperations.contains(QStringLiteral("validation-result")))
        && supportedOperations.contains(QStringLiteral("task-start"))
        && (!configuredOptions.contains(QStringLiteral("record-checkpoints"))
            || contract.value(QStringLiteral("checkpointOperation")).toObject().value(QStringLiteral("deliberate")).toBool()));
    if (!contractConfigConsistent) errors.append(QStringLiteral("Memory configuration and contract disagree."));

    QJsonObject report {
        {QStringLiteral("status"), errors.isEmpty() ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
        {QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("fingerprint"), coldStartFingerprint(projectRoot, mandatory)},
        {QStringLiteral("checks"), checks},
        {QStringLiteral("errors"), errors},
        {QStringLiteral("warnings"), QJsonArray {}},
        {QStringLiteral("recordingEnabled"), recordingEnabled},
        {QStringLiteral("contractConfigConsistent"), contractConfigConsistent}
    };
    return writeJsonFile(absolutePath(projectRoot, AramfPaths::ColdStartValidation), report, error);
}

QJsonObject ProjectMemory::validateColdStart(const QString& projectRoot, QString* error) const
{
    if (!generateColdStartValidation(projectRoot, error)) return {};
    return readJsonObject(absolutePath(projectRoot, AramfPaths::ColdStartValidation), error);
}

bool ProjectMemory::refreshMemoryContract(const QString& projectRoot, QString* error) const
{
    const QString path = absolutePath(projectRoot, AramfPaths::MemoryContract);
    QJsonObject contract = readJsonObject(path, error);
    if (contract.isEmpty()) {
        if (error && error->isEmpty()) *error = QStringLiteral("Memory contract is unavailable.");
        return false;
    }
    contract.insert(QStringLiteral("separateOperations"), QJsonObject{
        {QStringLiteral("checkpoints"), QStringLiteral("deliberate stable recovery points; use aramf memory checkpoint --project <project-root> --title <title> --summary <summary>; not created by routine feedback")},
        {QStringLiteral("durableDecisions"), QStringLiteral("deliberate architecture/policy records through the decision workflow")}});
    contract.insert(QStringLiteral("checkpointOperation"), QJsonObject{
        {QStringLiteral("command"), QStringLiteral("aramf memory checkpoint --project <project-root> --title <title> --summary <summary> [--task <task>] [--commit <sha>] [--verification-status <status>]")},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("--project"), QStringLiteral("--title"), QStringLiteral("--summary")}},
        {QStringLiteral("optional"), QJsonArray{QStringLiteral("--task"), QStringLiteral("--commit"), QStringLiteral("--verification-status")}},
        {QStringLiteral("deliberate"), true},
        {QStringLiteral("configuredOption"), QStringLiteral("record-checkpoints")}});
    return writeJsonFile(path, contract, error);
}

bool ProjectMemory::refreshMemoryInstructions(const QString& projectRoot, QString* error) const
{
    const QString path = absolutePath(projectRoot, AramfPaths::AgentInstructions);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    const QString begin = QStringLiteral("<!-- ARAMF-MEMORY-BEGIN -->");
    const QString end = QStringLiteral("<!-- ARAMF-MEMORY-END -->");
    const int beginAt = content.indexOf(begin);
    const int endAt = content.indexOf(end, beginAt + begin.size());
    if (beginAt < 0 || endAt < 0) {
        if (error) *error = QStringLiteral("Managed memory instruction section is missing.");
        return false;
    }
    const QString section = QStringLiteral(
        "<!-- ARAMF-MEMORY-BEGIN -->\n\n"
        "## Project Memory Feedback\n\n"
        "Read `memory/memory-contract.json` before recording development results. Do not edit ProjectMemory-owned "
        "bookkeeping files directly. Use `aramf memory record --project <project-root> --operation <operation> ...`.\n"
        "- Record task starts/completions, build results, test results, and validation outcomes when configured.\n"
        "- Record durable decisions only for genuine architecture or policy choices through the decision workflow.\n"
        "- Record a checkpoint only for a genuine stable recovery point with `aramf memory checkpoint --project <project-root> --title <title> --summary <summary>`; routine feedback does not create one.\n"
        "- Run the minimum validation required by `routing/validation-policy.json`; do not run full regression campaigns for ordinary isolated changes. Escalate when scope, risk, failure, or explicit milestone policy requires it.\n"
        "- Follow current durable decisions; explicitly superseded decisions remain historical and inactive.\n\n"
        "The recorder owns event IDs, timestamps, sequences, metrics, pruning, validation, and current-state pointers.\n\n"
        "<!-- ARAMF-MEMORY-END -->");
    content.replace(beginAt, endAt + end.size() - beginAt, section);
    if (!writeTextFile(path, content.toUtf8(), error)) return false;
    return writeJsonFile(absolutePath(projectRoot, AramfPaths::ValidationPolicy), ValidationRouting::policy(), error);
}

bool ProjectMemory::writeValidationReport(const QString& projectRoot, const QJsonObject& report, QString* error) const
{
    /**Persist the canonical memory-consistency report.

    Keeping validation output inside ARAMF makes the evidence portable with the project.
    */
    return writeJsonFile(absolutePath(projectRoot, AramfPaths::ConsistencyValidation), report, error);
}
