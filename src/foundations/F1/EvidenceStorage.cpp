#include "EvidenceStorage.h"
#include "../../core/AramfPaths.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSaveFile>
#include <QSet>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QStack>
#include <algorithm>
namespace f1_storage_detail {
QString absolutePath(const QString& projectRoot, const QString& relativePath)
{
    /**Resolve a canonical ARAMF-relative path.

    The project root is always treated as the owner of the generated ARAMF control plane.
    */
    return QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(relativePath));
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
        QStringLiteral("FRAMEWORK_KNOWLEDGE_SUPERSEDED"),
        QStringLiteral("ADMIN_OVERRIDE"),
        QStringLiteral("ADMIN_OVERRIDE_VALIDATION")
    };
    return controlPlaneEvents.contains(eventType);
}

QList<QJsonObject> readDecisionObjects(const QString& path, QString* error)
{
    QList<QJsonObject> records;
    QFile file(path);
    if (!file.exists()) return records;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return records;
    }

    QJsonObject current;
    bool inRecord = false;
    bool closedRecord = false;
    const auto lines = QString::fromUtf8(file.readAll()).split(QRegularExpression(QStringLiteral("\\r?\\n")));
    int lineNumber = 0;
    for (const QString& line : lines) {
        ++lineNumber;
        const QString trimmed = line.trimmed();
        if (trimmed == QStringLiteral("<!-- ARAMF-DECISION -->")) {
            if (inRecord) {
                if (error) *error = QStringLiteral("Nested durable decision marker at line %1.").arg(lineNumber);
                return {};
            }
            current = {};
            inRecord = true;
            closedRecord = false;
        } else if (trimmed == QStringLiteral("<!-- /ARAMF-DECISION -->")) {
            if (!inRecord) {
                if (error) *error = QStringLiteral("Unmatched durable decision terminator at line %1.").arg(lineNumber);
                return {};
            }
            if (current.value(QStringLiteral("decisionId")).toString().isEmpty()) {
                if (error) *error = QStringLiteral("Durable decision at line %1 has no Decision-ID.").arg(lineNumber);
                return {};
            }
            records.append(current);
            inRecord = false;
            closedRecord = true;
        } else if (inRecord) {
            const int separator = trimmed.indexOf(QLatin1Char(':'));
            if (separator < 0) continue;
            const QString key = trimmed.left(separator).remove(QLatin1Char('-')).trimmed().toLower();
            const QString value = trimmed.mid(separator + 1).trimmed();
            if (key == QStringLiteral("decisionid")) current.insert(QStringLiteral("decisionId"), value);
            else if (key == QStringLiteral("topic")) current.insert(QStringLiteral("topic"), value);
            else if (key == QStringLiteral("status")) current.insert(QStringLiteral("status"), value);
            else if (key == QStringLiteral("supersededby")) current.insert(QStringLiteral("supersededBy"), value);
            else if (key == QStringLiteral("summary")) current.insert(QStringLiteral("summary"), value);
            else if (key == QStringLiteral("scope")) current.insert(QStringLiteral("scope"), value);
            else if (key == QStringLiteral("scopes")) current.insert(QStringLiteral("scopes"), value);
        }
    }
    if (inRecord && !closedRecord) {
        if (error) *error = QStringLiteral("Durable decision file ends with an unclosed record.");
        return {};
    }
    return records;
}

QList<QJsonObject> readCheckpointObjects(const QString& path, QString* error)
{
    QList<QJsonObject> result;
    QFile file(path);
    if (!file.exists()) return result;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError
        || (!document.isObject() && !document.isArray())) {
        if (error) *error = parseError.errorString();
        return {};
    }
    const QJsonArray values = document.isArray()
        ? document.array()
        : document.object().value(QStringLiteral("checkpoints")).toArray();
    if (document.isObject() && !document.object().contains(QStringLiteral("checkpoints"))) {
        if (error) *error = QStringLiteral("Checkpoint file has no checkpoints array.");
        return {};
    }
    QSet<QString> ids;
    for (const auto& value : values) {
        const QJsonObject checkpoint = value.toObject();
        const QString id = checkpoint.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || ids.contains(id)
            || checkpoint.value(QStringLiteral("title")).toString().trimmed().isEmpty()
            || checkpoint.value(QStringLiteral("summary")).toString().trimmed().isEmpty()
            || !QDateTime::fromString(checkpoint.value(QStringLiteral("createdAt")).toString(), Qt::ISODate).isValid()
            || checkpoint.value(QStringLiteral("productionSequence")).toVariant().toLongLong() < 0) {
            if (error) *error = QStringLiteral("Checkpoint file contains an invalid record.");
            return {};
        }
        ids.insert(id);
        result.append(checkpoint);
    }
    return result;
}

bool projectMemoryRecordingEnabled(const QJsonObject& config)
{
    const QString writerMode = config.value(QStringLiteral("writerMode"))
                                   .toString(QStringLiteral("agent-direct"));
    return writerMode == QStringLiteral("agent-direct")
        || writerMode == QStringLiteral("project-local-tool");
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

QStringList coldStartPaths(const QString& projectRoot, bool requireControlPlane)
{
    QStringList paths {
        AramfPaths::Decisions,
        AramfPaths::FrameworkKnowledge,
        AramfPaths::CurrentState,
        AramfPaths::MemoryConfiguration,
        AramfPaths::MemoryContract,
        AramfPaths::Manifest,
        AramfPaths::EventIdIntegrityExceptions,
        AramfPaths::ProjectKnowledge,
        AramfPaths::CompactionManifest
    };
    if (requireControlPlane) paths << AramfPaths::AgentInstructions << AramfPaths::ProjectStatus;
    // These topology files are optional during bare ProjectMemory bootstrap,
    // but become part of the validated cold-start contract once generation
    // has published them.
    if (QFileInfo::exists(absolutePath(projectRoot, AramfPaths::ProjectConfiguration)))
        paths << AramfPaths::ProjectConfiguration;
    if (QFileInfo::exists(absolutePath(projectRoot, AramfPaths::WorkerManifest)))
        paths << AramfPaths::WorkerManifest;
    return paths;
}

}
using namespace f1_storage_detail;
QList<QJsonObject> EvidenceStorage::events(const QString& projectRoot, QString* error) const
{
    return readEvents(absolutePath(projectRoot, AramfPaths::EventLog), error);
}

bool EvidenceStorage::appendEvidence(const QString& projectRoot, QJsonObject event, QString* error) const
{
    if (error) error->clear();
    const QString manifestPath = absolutePath(projectRoot, AramfPaths::Manifest);
    QJsonObject manifest = readJsonObject(manifestPath, error);
    if (manifest.isEmpty() && QFile::exists(manifestPath)) return false;
    if (event.value(QStringLiteral("eventId")).toString().isEmpty()) {
        if (error) *error = QStringLiteral("Physical evidence requires a non-empty record ID.");
        return false;
    }
    const qint64 nextSequence = manifest.value(QStringLiteral("nextSequenceNumber")).toVariant().toLongLong();
    const qint64 sequence = nextSequence > 0 ? nextSequence : 1;
    event.insert(QStringLiteral("sequenceNumber"), sequence);
    QFile ledger(absolutePath(projectRoot, AramfPaths::EventLog));
    if (!ledger.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (error) *error = ledger.errorString();
        return false;
    }
    const auto record = QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
    if (ledger.write(record) != record.size() || !ledger.flush()) {
        if (error) *error = ledger.errorString();
        return false;
    }
    ledger.close();
    manifest.insert(QStringLiteral("_file"), QStringLiteral("memory-manifest.json"));
    manifest.insert(QStringLiteral("memoryVersion"), QStringLiteral("3"));
    manifest.insert(QStringLiteral("nextSequenceNumber"), sequence + 1);
    manifest.insert(QStringLiteral("eventCount"), manifest.value(QStringLiteral("eventCount")).toInt() + 1);
    manifest.insert(QStringLiteral("latestEventId"), event.value(QStringLiteral("eventId")));
    if (!writeJsonFile(manifestPath, manifest, error)) return false;
    QJsonObject metrics = readJsonObject(absolutePath(projectRoot, AramfPaths::Metrics), nullptr);
    metrics.insert(QStringLiteral("totalEventsCreated"), metrics.value(QStringLiteral("totalEventsCreated")).toInt() + 1);
    metrics.insert(QStringLiteral("activeEvents"), manifest.value(QStringLiteral("eventCount")));
    return writeJsonFile(absolutePath(projectRoot, AramfPaths::Metrics), metrics, error)
        && generateCurrentState(projectRoot, error);
}

QList<QJsonObject> EvidenceStorage::decisions(const QString& projectRoot,
                                            bool includeSuperseded,
                                            QString* error) const
{
    QList<QJsonObject> result;
    for (const auto& value : readDecisionObjects(absolutePath(projectRoot, AramfPaths::Decisions), error)) {
        if (!includeSuperseded && value.value(QStringLiteral("status")).toString() == QStringLiteral("superseded")) continue;
        result.append(value);
    }
    return result;
}

QList<QJsonObject> EvidenceStorage::currentDecisions(const QString& projectRoot, QString* error) const
{
    QList<QJsonObject> result;
    for (const auto& value : decisions(projectRoot, false, error)) {
        if (value.value(QStringLiteral("status")).toString() == QStringLiteral("current")) result.append(value);
    }
    return result;
}

QList<QJsonObject> EvidenceStorage::checkpoints(const QString& projectRoot, QString* error) const
{
    return readCheckpointObjects(absolutePath(projectRoot, AramfPaths::Checkpoints), error);
}

bool EvidenceStorage::generateCurrentState(const QString& projectRoot, QString* error) const
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

bool EvidenceStorage::requiresControlPlane(const QString& projectRoot)
{
    // Legacy/full projects fail closed. Only an explicit persisted false
    // narrows the read set; absence of an expected file never does.
    return readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryConfiguration), nullptr)
        .value(QStringLiteral("requireControlPlane")).toBool(true);
}

bool EvidenceStorage::generateColdStartValidation(const QString& projectRoot, QString* error) const
{
    return generateColdStartValidation(projectRoot, error, requiresControlPlane(projectRoot));
}

bool EvidenceStorage::generateColdStartValidation(const QString& projectRoot, QString* error, bool requireControlPlane) const
{
    const QStringList mandatory = coldStartPaths(projectRoot, requireControlPlane);
    QJsonArray checks;
    QJsonArray errors;
    auto addCheck = [&checks, &errors](const QString& name, bool pass, const QString& message) {
        checks.append(QJsonObject {
            {QStringLiteral("name"), name},
            {QStringLiteral("status"), pass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("message"), message}
        });
        if (!pass) errors.append(message);
    };
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

    QString eventError;
    const auto recoveredEvents = events(projectRoot, &eventError);
    const bool eventsRecovered = QFile::exists(absolutePath(projectRoot, AramfPaths::EventLog)) && eventError.isEmpty();
    QString decisionError;
    const auto recoveredDecisions = decisions(projectRoot, true, &decisionError);
    const bool decisionsRecovered = QFile::exists(absolutePath(projectRoot, AramfPaths::Decisions)) && decisionError.isEmpty();
    QString checkpointError;
    const auto recoveredCheckpoints = checkpoints(projectRoot, &checkpointError);
    const bool checkpointsRecovered = QFile::exists(absolutePath(projectRoot, AramfPaths::Checkpoints)) && checkpointError.isEmpty();

    const QJsonObject config = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryConfiguration), nullptr);
    const QJsonObject contract = readJsonObject(absolutePath(projectRoot, AramfPaths::MemoryContract), nullptr);
    const bool recordingEnabled = projectMemoryRecordingEnabled(config);
    QFile agentFile(absolutePath(projectRoot, AramfPaths::AgentInstructions));
    QString agentText;
    if (agentFile.open(QIODevice::ReadOnly | QIODevice::Text)) agentText = QString::fromUtf8(agentFile.readAll());
    const bool contractDiscoverable = !recordingEnabled || !requireControlPlane
        || (agentText.contains(QStringLiteral("memory/memory-contract.json"))
            && agentText.contains(QStringLiteral("append-only"))
            && (agentText.contains(QStringLiteral("narrow mutation"))
                || agentText.contains(QStringLiteral("narrowest valid mutation"))));
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

    addCheck(QStringLiteral("event-log-semantic-recovery"), eventsRecovered,
             eventsRecovered ? QStringLiteral("Event log parsed through ProjectMemory read API.") : eventError);
    addCheck(QStringLiteral("decisions-semantic-recovery"), decisionsRecovered,
             decisionsRecovered ? QStringLiteral("Durable decisions parsed through ProjectMemory read API.") : decisionError);
    addCheck(QStringLiteral("checkpoints-semantic-recovery"), checkpointsRecovered,
             checkpointsRecovered ? QStringLiteral("Checkpoints parsed through ProjectMemory read API.") : checkpointError);

    const qint64 maximumEventSequence = [&recoveredEvents] {
        qint64 maximum = 0;
        for (const auto& event : recoveredEvents) maximum = qMax(maximum, event.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong());
        return maximum;
    }();
    const qint64 manifestSequence = readJsonObject(absolutePath(projectRoot, AramfPaths::Manifest), nullptr)
                                        .value(QStringLiteral("nextSequenceNumber")).toVariant().toLongLong();
    addCheck(QStringLiteral("manifest-event-recovery"),
             eventsRecovered && manifestSequence == maximumEventSequence + 1,
             QStringLiteral("Manifest sequence does not agree with recovered event history."));
    addCheck(QStringLiteral("current-decisions-recovery"),
             decisionsRecovered && std::all_of(recoveredDecisions.cbegin(), recoveredDecisions.cend(), [](const QJsonObject& decision) {
                 const QString status = decision.value(QStringLiteral("status")).toString();
                 return status == QStringLiteral("current") || status == QStringLiteral("superseded") || status == QStringLiteral("historical");
             }),
             QStringLiteral("Current decision state could not be reconstructed."));
    addCheck(QStringLiteral("latest-checkpoint-recovery"),
             checkpointsRecovered && (recoveredCheckpoints.isEmpty() || !recoveredCheckpoints.last().value(QStringLiteral("id")).toString().isEmpty()),
             QStringLiteral("Latest checkpoint could not be recovered."));

    QJsonObject report {
        {QStringLiteral("status"), errors.isEmpty() ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
        {QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("fingerprint"), coldStartFingerprint(projectRoot, mandatory)},
        {QStringLiteral("checks"), checks},
        {QStringLiteral("errors"), errors},
        {QStringLiteral("warnings"), QJsonArray {}},
        {QStringLiteral("recordingEnabled"), recordingEnabled},
        {QStringLiteral("requireControlPlane"), requireControlPlane},
        {QStringLiteral("contractConfigConsistent"), contractConfigConsistent}
    };
    return writeJsonFile(absolutePath(projectRoot, AramfPaths::ColdStartValidation), report, error);
}

QJsonObject EvidenceStorage::validateColdStart(const QString& projectRoot, QString* error) const
{
    if (!generateColdStartValidation(projectRoot, error)) return {};
    return readJsonObject(absolutePath(projectRoot, AramfPaths::ColdStartValidation), error);
}

bool EvidenceStorage::refreshDerivedState(const QString& projectRoot, QString* error) const
{
    return generateCurrentState(projectRoot, error) && generateColdStartValidation(projectRoot, error);
}

qint64 EvidenceStorage::managedMemoryUsage(const QString& projectRoot) const
{
    qint64 total = 0;
    QStack<QString> directories;
    directories.push(absolutePath(projectRoot, QStringLiteral("ARAMF_WORKER/memory")));
    while (!directories.isEmpty()) {
        const QDir directory(directories.pop());
        for (const auto& entry : directory.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.isDir()) directories.push(entry.absoluteFilePath());
            else total += entry.size();
        }
    }
    return total;
}

qint64 EvidenceStorage::memoryUsageBytes(const QString& projectRoot) const
{
    return managedMemoryUsage(projectRoot);
}
