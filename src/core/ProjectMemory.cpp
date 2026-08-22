// ProjectMemory.cpp

#include "ProjectMemory.h"

#include "AramfPaths.h"
#include "ControlPlaneMigration.h"
#include "ProjectModel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStack>
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

    QJsonObject report;
    report.insert(QStringLiteral("_file"), QStringLiteral("memory-consistency-validation.json"));
    report.insert(QStringLiteral("status"), errors.isEmpty() ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
    report.insert(QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    report.insert(QStringLiteral("durableSequence"), maximum);
    report.insert(QStringLiteral("productionSequence"), productionMaximum);
    report.insert(QStringLiteral("checks"), checks);
    report.insert(QStringLiteral("errors"), errors);
    report.insert(QStringLiteral("warnings"), QJsonArray {});

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
    const QList<QPair<QString, QJsonObject>> defaults {
        {AramfPaths::MemoryConfiguration, QJsonObject {
            {QStringLiteral("maximumSizeBytes"), model ? model->memoryConfiguration().maximumSizeBytes : 10LL * 1024LL * 1024LL * 1024LL}
        }},
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
        const bool preserveExisting = relativePath != AramfPaths::MemoryConfiguration;
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
    /**Write a lightweight cold-start validation report.

    This confirms the canonical ARAMF root and mandatory startup files are present for a fresh agent session.
    */
    QStringList mandatory {
        AramfPaths::Decisions,
        AramfPaths::FrameworkKnowledge,
        AramfPaths::Manifest
    };
    if (requireControlPlane) {
        mandatory << AramfPaths::AgentInstructions
                  << AramfPaths::ProjectStatus
                  << AramfPaths::GeneratedRules;
    }

    QJsonArray checks;
    QJsonArray errors;
    for (const QString& relativePath : mandatory) {
        const bool exists = QFile::exists(absolutePath(projectRoot, relativePath));
        checks.append(QJsonObject {
            {QStringLiteral("name"), relativePath},
            {QStringLiteral("status"), exists ? QStringLiteral("PASS") : QStringLiteral("FAIL")}
        });
        if (!exists) {
            errors.append(QStringLiteral("Missing %1").arg(relativePath));
        }
    }

    QJsonObject report {
        {QStringLiteral("status"), errors.isEmpty() ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
        {QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("checks"), checks},
        {QStringLiteral("errors"), errors},
        {QStringLiteral("warnings"), QJsonArray {}}
    };
    return writeJsonFile(absolutePath(projectRoot, AramfPaths::ColdStartValidation), report, error);
}

bool ProjectMemory::writeValidationReport(const QString& projectRoot, const QJsonObject& report, QString* error) const
{
    /**Persist the canonical memory-consistency report.

    Keeping validation output inside ARAMF makes the evidence portable with the project.
    */
    return writeJsonFile(absolutePath(projectRoot, AramfPaths::ConsistencyValidation), report, error);
}
