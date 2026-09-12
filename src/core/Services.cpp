// Services.cpp

#include "Services.h"
#include "TemplateValidation.h"
#include "DocumentTemplate.h"
#include "DocumentInstruction.h"
#include "DocumentTemplateInspector.h"

#include "AramfPaths.h"
#include "ControlPlaneMigration.h"
#include "AiCatalog.h"
#include "ProjectMemory.h"
#include "CertificationService.h"
#include "RuleCatalog.h"
#include "ValidationRouting.h"
#include "WorkerContextResolver.h"
#include "GitIgnoreService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QSaveFile>

#include <algorithm>

namespace
{
class WorkerNameScope final {
public:
    explicit WorkerNameScope(const QString& suffix) : previous_(AramfPaths::detail::workerSuffixOverride()) { AramfPaths::setRuntimeWorkerNameSuffix(suffix); }
    ~WorkerNameScope() { AramfPaths::setRuntimeWorkerNameSuffix(previous_); }
private:
    QString previous_;
};

QJsonArray toJsonArray(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

bool writeTextFile(const QString& path, const QByteArray& data, QString* error, bool onlyIfMissing = false)
{
    if (onlyIfMissing && QFile::exists(path)) return true;
    if (QFile existing(path); existing.exists() && existing.open(QIODevice::ReadOnly)
        && existing.readAll() == data) return true;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool writeJsonFile(const QString& path, const QJsonObject& object, QString* error, bool onlyIfMissing = false)
{
    QJsonObject value = object;
    if (!value.contains(QStringLiteral("_file"))) value.insert(QStringLiteral("_file"), QFileInfo(path).fileName());
    return writeTextFile(path, QJsonDocument(value).toJson(QJsonDocument::Indented), error, onlyIfMissing);
}

bool upsertManagedSection(const QString& path,
                          const QString& beginMarker,
                          const QString& endMarker,
                          const QString& section,
                          QString* error)
{
    QString content;
    QFile existing(path);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error) *error = existing.errorString();
            return false;
        }
        content = QString::fromUtf8(existing.readAll());
        existing.close();
    }
    const int begin = content.indexOf(beginMarker);
    if (begin >= 0) {
        const int end = content.indexOf(endMarker, begin);
        if (end >= 0) content.replace(begin, end + endMarker.size() - begin, section);
        else content = content.left(begin) + section;
    } else {
        while (content.endsWith(QStringLiteral("\n\n"))) content.chop(1);
        if (!content.isEmpty() && !content.endsWith(QLatin1Char('\n'))) content += QLatin1Char('\n');
        content += QLatin1Char('\n') + section;
    }
    return writeTextFile(path, content.toUtf8(), error);
}

QString ruleDisplayName(const QString& id, const QList<EnvironmentOption>& options)
{
    for (const auto& option : options) {
        if (option.second == id) return option.first;
    }
    return id;
}

QString projectTypeLabel(const ProjectModel& model)
{
    return model.context().isEmpty() ? QStringLiteral("not classified") : model.context();
}

QJsonObject projectConfiguration(const ProjectModel& model, const QString& fingerprint)
{
    const auto environment = model.developmentEnvironment();
    const auto capabilities = model.developmentCapabilities();
    const auto academic = model.academicConfiguration();
    const auto ai = model.aiConfiguration();
    const auto communication = model.communicationConfiguration();
    return QJsonObject{
        {QStringLiteral("schemaVersion"), 1}, {QStringLiteral("projectId"), model.projectId()},
        {QStringLiteral("projectName"), model.projectName()}, {QStringLiteral("workerIdentity"), AramfPaths::runtimeWorkerDirectoryName()},
        {QStringLiteral("workerSchemaVersion"), 1}, {QStringLiteral("configurationFingerprint"), fingerprint}, {QStringLiteral("context"), model.context()},
        {QStringLiteral("templateId"), model.templateId()}, {QStringLiteral("templateModules"), toJsonArray(model.templateModules())},
        {QStringLiteral("environment"), QJsonObject{{QStringLiteral("language"), environment.language}, {QStringLiteral("framework"), environment.framework},
            {QStringLiteral("ide"), environment.ide}, {QStringLiteral("compiler"), environment.compiler}, {QStringLiteral("operatingSystem"), environment.operatingSystem},
            {QStringLiteral("targetPlatform"), environment.targetPlatform}, {QStringLiteral("targetArchitecture"), environment.targetArchitecture}, {QStringLiteral("buildSystem"), environment.buildSystem},
            {QStringLiteral("packageManager"), environment.packageManager}, {QStringLiteral("versionControl"), environment.versionControl}}},
        {QStringLiteral("languages"), toJsonArray(capabilities.languages)}, {QStringLiteral("frameworks"), toJsonArray(capabilities.frameworks)},
        {QStringLiteral("tools"), toJsonArray(capabilities.developmentTools)}, {QStringLiteral("targetPlatforms"), toJsonArray(capabilities.targetPlatforms)},
        {QStringLiteral("hardware"), toJsonArray(capabilities.hardwareTargets)}, {QStringLiteral("architectures"), toJsonArray(capabilities.targetArchitectures)},
        {QStringLiteral("academic"), QJsonObject{{QStringLiteral("enabled"), academic.enabled}, {QStringLiteral("projectTypes"), toJsonArray(academic.projectTypes)}, {QStringLiteral("thesis"), academic.thesisDocumentation.enabled}, {QStringLiteral("report"), academic.reportDocumentation.enabled}}},
        {QStringLiteral("ai"), QJsonObject{{QStringLiteral("primaryAgent"), ai.primaryAgent}, {QStringLiteral("additionalAgents"), toJsonArray(ai.additionalAgents)}}},
        {QStringLiteral("communication"), QJsonObject{{QStringLiteral("enabled"), communication.enabled}, {QStringLiteral("transport"), communication.transport}, {QStringLiteral("protocol"), communication.protocol}}},
        {QStringLiteral("canonicalPaths"), QJsonObject{{QStringLiteral("worker"), AramfPaths::runtimeWorkerDirectoryName()}, {QStringLiteral("status"), AramfPaths::ProjectStatus}, {QStringLiteral("currentState"), AramfPaths::CurrentState}, {QStringLiteral("routing"), AramfPaths::TaskRoutes}, {QStringLiteral("validation"), AramfPaths::ColdStartValidation}}}
    };
}

QJsonObject workerManifest(const ProjectModel& model, const GenerationOptions& options, const QString& fingerprint)
{
    return QJsonObject{
        {QStringLiteral("workerSchemaVersion"), 1}, {QStringLiteral("workerIdentity"), AramfPaths::runtimeWorkerDirectoryName()},
        {QStringLiteral("taskContract"), QJsonObject{{"schemaVersion", 1}, {"authority", "DERIVED"}, {"policyOwner", "ProjectModel.rules.scopeMetadata"}, {"routingSource", AramfPaths::ScopeRoutes}, {"preflightRequired", true}}},
        {QStringLiteral("projectId"), model.projectId()}, {QStringLiteral("generatedFromFingerprint"), fingerprint},
        {QStringLiteral("canonicalFiles"), QJsonObject{{QStringLiteral("projectConfiguration"), AramfPaths::ProjectConfiguration}, {QStringLiteral("routing"), AramfPaths::TaskRoutes}, {QStringLiteral("currentState"), AramfPaths::CurrentState}, {QStringLiteral("validation"), AramfPaths::ColdStartValidation}, {QStringLiteral("decisions"), AramfPaths::Decisions}, {QStringLiteral("eventHistory"), AramfPaths::EventLog}}},
        {QStringLiteral("fileRoles"), QJsonObject{{QStringLiteral("projectConfiguration"), QStringLiteral("DERIVED")}, {QStringLiteral("workerManifest"), QStringLiteral("DERIVED")}, {QStringLiteral("routing"), QStringLiteral("DERIVED")}, {QStringLiteral("currentState"), QStringLiteral("CANONICAL")}, {QStringLiteral("decisions"), QStringLiteral("CANONICAL")}, {QStringLiteral("eventHistory"), QStringLiteral("HISTORICAL")}, {QStringLiteral("latestValidation"), QStringLiteral("DERIVED")}, {QStringLiteral("validationEvidence"), QStringLiteral("VALIDATION_EVIDENCE")}}},
        {QStringLiteral("derivedFiles"), QJsonObject{{QStringLiteral("status"), AramfPaths::ProjectStatus}, {QStringLiteral("coldStartValidation"), AramfPaths::ColdStartValidation}, {QStringLiteral("latestValidation"), AramfPaths::LatestValidation}}},
        {QStringLiteral("coldStart"), QJsonObject{{QStringLiteral("files"), QJsonArray{AramfPaths::ProjectConfiguration, AramfPaths::WorkerManifest, AramfPaths::AgentInstructions, AramfPaths::CurrentState, AramfPaths::ColdStartValidation, AramfPaths::LatestValidation, AramfPaths::ValidationPolicy}}, {QStringLiteral("historyExcluded"), true}}},
        {QStringLiteral("generationOptions"), QJsonObject{{QStringLiteral("agentRules"), options.generateAgentRules}, {QStringLiteral("routing"), options.generateRouting}, {QStringLiteral("memory"), options.generateMemory}}}
    };
}

void addGeneratedFiles(GenerationResult& result, const QStringList& files)
{
    for (const auto& file : files)
        result.generatedFiles.append(file == QStringLiteral("AGENTS.md") ? file : AramfPaths::resolveWorkerRelativePath(file));
}

QString statusName(VerificationStatus status)
{
    switch (status) {
    case VerificationStatus::Pass: return QStringLiteral("PASS");
    case VerificationStatus::Warning: return QStringLiteral("WARNING");
    case VerificationStatus::Fail: return QStringLiteral("FAIL");
    case VerificationStatus::NotApplicable: return QStringLiteral("NOT_APPLICABLE");
    }
    return QStringLiteral("FAIL");
}

bool readJson(const QString& path, QJsonObject* object, QString* error)
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
    if (object) *object = document.object();
    return true;
}

bool readableNonEmpty(const QString& path)
{
    return QFileInfo(path).isReadable() && QFileInfo(path).size() > 0;
}

void addCheck(VerificationResult& result, const QString& id, const QString& name,
             VerificationStatus status, const QString& details)
{
    result.checks.append({id, name, status, details});
}

QString managedBootstrapBlock(const QString& workerName = AramfPaths::ControlDirectory)
{
    return QStringLiteral("<!-- ARAMF-BEGIN -->\n"
                          "This project is managed by ARAMF.\n\n"
                          "Read and follow:\n\n"
                          ) + workerName + QStringLiteral("/AGENTS.md\n\n")
        + workerName + QStringLiteral(" contains the canonical project rules, routing, resources, memory and project status.\n"
                                      "<!-- ARAMF-END -->\n");
}

bool writeManagedFile(const QString& path, const QString& block,
                      QString* state, QString* error)
{
    QFile existing(path);
    QString original;
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error) *error = existing.errorString();
            return false;
        }
        original = QString::fromUtf8(existing.readAll());
        existing.close();
    }

    const QString begin = QStringLiteral("<!-- ARAMF-BEGIN -->");
    const QString end = QStringLiteral("<!-- ARAMF-END -->");
    const int beginIndex = original.indexOf(begin);
    const int endIndex = original.indexOf(end, beginIndex < 0 ? 0 : beginIndex);
    QString updated;
    if (beginIndex >= 0 && endIndex < beginIndex) {
        if (error) *error = QStringLiteral("Malformed ARAMF managed section in %1.").arg(path);
        return false;
    }
    if (beginIndex >= 0 && endIndex >= 0) {
        const int endLength = end.size();
        const QString existingBlock = original.mid(beginIndex, endIndex + endLength - beginIndex);
        QString normalizedExistingBlock = existingBlock;
        normalizedExistingBlock.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        normalizedExistingBlock.replace(QChar('\r'), QChar('\n'));
        const QString normalizedBlock = block;
        if (normalizedExistingBlock.trimmed() == normalizedBlock.trimmed()) {
            if (state) *state = QStringLiteral("unchanged");
            return true;
        }
        updated = original.left(beginIndex) + block + original.mid(endIndex + endLength);
    } else {
        updated = original;
        if (!updated.isEmpty() && !updated.endsWith(QLatin1Char('\n'))) updated += QLatin1Char('\n');
        if (!updated.isEmpty()) updated += QLatin1Char('\n');
        updated += block;
    }

    const QString normalizedOriginal = original.normalized(QString::NormalizationForm_C).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const QString normalizedUpdated = updated.normalized(QString::NormalizationForm_C).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (normalizedUpdated == normalizedOriginal) {
        if (state) *state = QStringLiteral("unchanged");
        return true;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)
        || output.write(updated.toUtf8()) != updated.toUtf8().size()
        || !output.commit()) {
        if (error) *error = output.errorString();
        return false;
    }
    if (state) *state = original.isEmpty() ? QStringLiteral("created") : QStringLiteral("updated");
    return true;
}

QString selectedAgentDisplayName(const QString& id)
{
    for (const auto& option : AiCatalog::agents()) {
        if (option.id == id) return option.displayName;
    }
    return id;
}
}

QString projectConfigurationFingerprint(const ProjectModel& model,
                                        const GenerationOptions& options)
{
    const auto capabilities = model.developmentCapabilities();
    const auto ai = model.aiConfiguration();
    const auto rules = model.ruleConfiguration();
    const auto memory = model.memoryConfiguration();
    const auto certification = model.certificationConfiguration();
    QJsonObject value{
        {QStringLiteral("projectId"), model.projectId()},
        {QStringLiteral("projectName"), model.projectName()},
        {QStringLiteral("projectPath"), QDir::cleanPath(model.projectPath())},
        {QStringLiteral("template"), model.templateId()},
        {QStringLiteral("context"), model.context()},
        {QStringLiteral("languages"), toJsonArray(capabilities.languages)},
        {QStringLiteral("frameworks"), toJsonArray(capabilities.frameworks)},
        {QStringLiteral("tools"), toJsonArray(capabilities.developmentTools)},
        {QStringLiteral("platforms"), toJsonArray(capabilities.targetPlatforms)},
        {QStringLiteral("hardware"), toJsonArray(capabilities.hardwareTargets)},
        {QStringLiteral("architectures"), toJsonArray(capabilities.targetArchitectures)},
        {QStringLiteral("toolchains"), toJsonArray(capabilities.toolchains)},
        {QStringLiteral("buildSystems"), toJsonArray(capabilities.buildSystems)},
        {QStringLiteral("dependencyManagers"), toJsonArray(capabilities.dependencyManagers)},
        {QStringLiteral("buildConfigurations"), toJsonArray(capabilities.buildConfigurations)},
        {QStringLiteral("testingCapabilities"), toJsonArray(capabilities.testingCapabilities)},
        {QStringLiteral("qualityCapabilities"), toJsonArray(capabilities.qualityCapabilities)},
        {QStringLiteral("automationCapabilities"), toJsonArray(capabilities.automationCapabilities)},
        {QStringLiteral("deliveryCapabilities"), toJsonArray(capabilities.deliveryCapabilities)},
        {QStringLiteral("aiAgent"), ai.primaryAgent},
        {QStringLiteral("aiResponsibilities"), toJsonArray(ai.responsibilities)},
        {QStringLiteral("aiPermissions"), toJsonArray(ai.permissions)},
        {QStringLiteral("aiIntegrations"), toJsonArray(ai.aramfIntegrations)},
        {QStringLiteral("resources"), model.resources().size()},
        {QStringLiteral("resourceIds"), [&] { QJsonArray a; for (const auto& r : model.resources()) a.append(r.id); return a; }()},
        {QStringLiteral("resourceRoles"), [&] { QJsonArray a; for (const auto& r : model.resources()) a.append(r.role); return a; }()},
        {QStringLiteral("rules"), toJsonArray(rules.activeCategories)},
        {QStringLiteral("ruleEnforcement"), rules.enforcementLevel},
        {QStringLiteral("scopeMetadata"), rules.scopeMetadata},
        {QStringLiteral("projectScopes"), [&] { auto scopes = rules.projectScopes; scopes.removeDuplicates(); scopes.sort(); return toJsonArray(scopes); }()},
        {QStringLiteral("memoryMaximum"), QString::number(memory.maximumSizeBytes)},
        {QStringLiteral("certificationEnabled"), certification.enabled},
        {QStringLiteral("certificationLevel"), certification.defaultVerificationLevel},
        {QStringLiteral("agentRules"), options.generateAgentRules},
        {QStringLiteral("routing"), options.generateRouting},
        {QStringLiteral("platformOutput"), options.generatePlatforms},
        {QStringLiteral("resourceOutput"), options.generateResources},
        {QStringLiteral("memoryOutput"), options.generateMemory},
        {QStringLiteral("provenance"), options.generateProvenance}};
    const auto communication = model.communicationConfiguration();
    value.insert(QStringLiteral("communication"), QJsonObject{{QStringLiteral("enabled"), communication.enabled}, {QStringLiteral("sourceTarget"), communication.sourceTarget}, {QStringLiteral("destinationTarget"), communication.destinationTarget}, {QStringLiteral("transport"), communication.transport}, {QStringLiteral("protocol"), communication.protocol}, {QStringLiteral("protocolVersion"), communication.protocolVersion}});
    value.insert(QStringLiteral("workerNameSuffix"), model.workerNameSuffix());
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(value).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}

GenerationServices::GenerationServices(QObject* parent)
    : QObject(parent)
{
    /**Construct the project generation service.

    Generation is implemented through the native C++ ProjectMemory service with no Python or Node runtime dependency.
    */
}

GenerationResult GenerationServices::generate(const ProjectModel& model,
                                               const GenerationOptions& options) const
{
    WorkerNameScope workerNameScope(model.workerNameSuffix());
    GenerationResult result;
    const auto configurationErrors = TemplateValidation::readiness(model);
    if (!configurationErrors.isEmpty()) {
        result.error = configurationErrors.join('\n');
        return result;
    }
    result.fingerprint = projectConfigurationFingerprint(model, options);
    if (!options.generateAgentRules && !options.generateRouting && !options.generatePlatforms
        && !options.generateResources && !options.generateMemory && !options.generateProvenance) {
        result.error = QStringLiteral("Generation stopped: select at least one output product.");
        return result;
    }

    const QString projectRoot = QDir::cleanPath(model.projectPath().trimmed());
    if (projectRoot.isEmpty() || projectRoot == QStringLiteral(".")) {
        result.error = QStringLiteral("Generation stopped: choose a project path first.");
        return result;
    }
    const QString workerSuffix = AramfPaths::normalizeWorkerNameSuffix(model.workerNameSuffix());
    if (!workerSuffix.isEmpty()
        && QDir(projectRoot).exists(AramfPaths::workerDirectoryName(workerSuffix))) {
        result.error = QStringLiteral("Generation stopped: worker directory already exists: %1")
            .arg(QDir(projectRoot).filePath(AramfPaths::workerDirectoryName(workerSuffix)));
        return result;
    }
    const auto preparation = prepareControlPlane(projectRoot);
    if (!preparation.success) {
        result.error = QStringLiteral("Generation failed: %1").arg(preparation.error);
        return result;
    }
    result.warnings.append(preparation.warnings);

    const QList<QPair<bool, QString>> productOrder{
        {options.generateAgentRules, QStringLiteral("Agent rules")},
        {options.generateRouting, QStringLiteral("Routing")},
        {options.generatePlatforms, QStringLiteral("Platform and environment metadata")},
        {options.generateResources, QStringLiteral("Resource manifest")},
        {options.generateMemory, QStringLiteral("Project Memory")},
        {options.generateProvenance, QStringLiteral("Provenance and selection effects")}};
    auto fail = [&result, &productOrder](const QString& product, const QString& error) {
        result.failedProduct = product;
        result.partial = !result.generatedFiles.isEmpty();
        bool afterFailure = false;
        for (const auto& [selected, name] : productOrder) {
            if (name == product) {
                afterFailure = true;
                continue;
            }
            if (afterFailure && selected) result.notAttemptedProducts.append(name);
        }
        result.error = QStringLiteral("Generation failed in %1: %2")
                           .arg(product, error.isEmpty() ? QStringLiteral("unknown error") : error);
        return result;
    };

    if (!options.generateAgentRules) result.skippedProducts << QStringLiteral("Agent rules");
    if (!options.generateRouting) result.skippedProducts << QStringLiteral("Routing");
    if (!options.generatePlatforms) result.skippedProducts << QStringLiteral("Platform and environment metadata");
    if (!options.generateResources) result.skippedProducts << QStringLiteral("Resource manifest");
    if (!options.generateMemory) result.skippedProducts << QStringLiteral("Project Memory");
    if (!options.generateProvenance) result.skippedProducts << QStringLiteral("Provenance and selection effects");

    QString error;
    if (options.generateAgentRules) {
        const QString workerName = AramfPaths::runtimeWorkerDirectoryName();
        const QString rootAgent = QStringLiteral("<!-- AGENTS.md -->\n\n") + managedBootstrapBlock(workerName);
        QString canonicalAgent = QStringLiteral(
            "<!-- AGENTS.md -->\n\n# Canonical ARAMF Agent Instructions\n\n"
            "## Mandatory startup router\n\n"
            "1. Read `project.json` to identify the project and stable capabilities.\n"
            "2. Read `worker-manifest.json` to resolve canonical ownership and the cold-start read set.\n"
            "3. Read `memory/current-state.md` and the latest validation summary.\n"
            "4. Determine the task scope, then follow `routing/task-routes.json` and `routing/scope-routes.json`.\n"
            "5. Read only the selected scope's instructions/resources; defer `memory/event-log.jsonl` unless history is explicitly required.\n\n"
            "Before scoped AI edits, derive a task contract with `aramf task prepare --config <saved-project> --request <task.json>`. "
            "Continue only from READY or READY_WITH_WARNINGS; unmapped source files require canonical scopeMetadata. "
            "After edits, use `aramf task postflight --config <saved-project> --contract <prepared.json> --evidence <evidence.json>` "
            "and satisfy its evidence requirements before claiming VERIFIED. Task contracts are derived views, not independent authority.\n\n"
            "Read `PROJECT_STATUS.md` and `memory/decisions.md` when the task requires current project detail or durable architectural context.\n");
        canonicalAgent += QStringLiteral(
            "<!-- ARAMF-TASK-GOVERNANCE-BEGIN -->\n"
            "\n## Governed Task Execution Contract\n\n"
            "Every governed task follows ANALYZE -> PREPARE -> EXECUTE -> VALIDATE. ANALYZE resolves the applicable scope and dependencies without mutating the project. PREPARE produces a READY TaskContract with exact permitted files/resources, ownership, canonical producers, ChangeImpact, ValidationRouting, and required evidence; PREPARE must not perform Execute. EXECUTE is the only implementation mutation boundary. VALIDATE performs postflight and evidence checks before completion.\n"
            "Modify only files and resources explicitly permitted by the TaskContract. Unmapped files, path traversal, protected files, user-owned Sources of Truth, and ownership conflicts are blocked. Permission to write generated output never overrides its canonical producer or resource ownership. Generated/service-owned files must be produced or repaired by their authoritative ARAMF service, not recreated manually.\n"
            "Project isolation is mandatory: preserve pre-existing dirty and unrelated files, exclude them from current-task attribution, and fail on new out-of-scope edits. Never broaden a task to the whole project or full ARAMF_WORKER because precise scope resolution is inconvenient. Use declared ChangeImpact and dependency scope to determine affected validation and evidence. Evidence is fresh only for the dependencies it covers; later relevant changes stale that evidence.\n"
            "Follow the authoritative route in `routing/validation-policy.json`. VERIFIED requires all applicable valid software evidence and fresh fingerprints. CERTIFIED is a separate claim requiring its applicable certification evidence; software verification must not imply physical certification. HARDWARE_CERTIFIED or other physical claims require valid physical/on-target evidence and must never be fabricated.\n"
            "Persist governed state through the canonical ARAMF services, save/reload it, and verify readback and cross-file consistency. Governance events use the append-only recorder and its current-state, manifest, metrics, PROJECT_STATUS, memory-consistency, and cold-start mechanisms; do not invent recorder files or rewrite history. Keep the generated Worker topology coherent and treat `ARAMF_WORKER/` as orchestration while the managed project root remains the implementation target.\n"
            "<!-- ARAMF-TASK-GOVERNANCE-END -->\n");
        if (options.generateMemory) {
            canonicalAgent += QStringLiteral(
                "Read `memory/framework-knowledge.json` and apply only entries whose status is `approved`.\n"
                "Approved Framework Knowledge is live: it applies immediately in this project without regeneration.\n");
        }
        canonicalAgent += QStringLiteral(
            "Read `rules/generated-rules.md` when rule output is present.\n\n"
            "Respect Sources of Truth, durable decisions, and the user-owned `custom/` directory.\n"
            "Project resources have explicit governance roles in resources/resources.json: source-of-truth is authoritative project fact/requirement, instruction is a directive within its authority and scope, reference is informational and does not override governing sources, and supporting-material is contextual with lower governance authority. Ignore disabled resources. Do not infer roles from filenames or file types. If active instructions conflict at equal effective authority, surface the conflict and require governance resolution; never silently choose or merge them.\n"
            "Thesis Template and Report Template are distinct canonical resource roles. Thesis and Report may both be enabled. Use the ARAMF Default Thesis Template or ARAMF Default Report Template automatically when their document is enabled without a custom source; custom sources must resolve by resource ID and matching role from resources/resources.json.\n"
            "Authority order: explicit current user instruction, current Source of Truth, current durable project decisions, approved Framework Knowledge, templates/defaults, then AI inference.\n"
            "When a corrected approach is verified and reusable, record a Framework Knowledge candidate with evidence. Never self-approve it; explicit user approval is required before changing its status to `approved`. Superseded entries remain auditable but are not active.\n"
            "Keep PROJECT_STATUS.md current as human-readable present state; it is distinct from append-only historical evidence. Project Memory ownership is explicit in memory/memory-contract.json.\n"
            "The generated control directory is `%1/`.\n"
            "When communication-contract.json is present, it is the canonical communication Source of Truth. Do not invent message IDs, rename contract fields, change logical field types or protocol versions independently, or change wire encoding for only one endpoint; update the shared contract first and run compatibility validation for every affected endpoint.\n"
            "Communication commands use symbolic hardware resource IDs. Resolve physical pins only through `hardware/hardware-resources.json`; validate endpoint ownership and capabilities, and never invent or access arbitrary numeric GPIOs in communication code.\n"
            "Framework Knowledge has distinct built-in, global, and project-local layers. The global user library is stored under `ARAMF_DATA/` at the resolved ARAMF program root; build directories are disposable. Only explicitly approved portable knowledge may be promoted there; use the memory knowledge promotion command and never edit knowledge stores directly. New projects seed approved global knowledge without replacing project-local authority.\n"
            "UPDATE is a separate human-controlled workflow: review approved Framework Knowledge, analyze the whole project, prepare a plan, then explicitly execute it through the configured agent. Read `update/update-plan.json` and `update/update-contract.json` when present; the managed project root is the implementation target and `%1/` is orchestration only. `READY_FOR_EXTERNAL_AGENT` is an incomplete handoff, not completion; actual project changes are required. Preserve higher-authority instructions and use the scope-aware validation policy.\n").arg(workerName);
        canonicalAgent += QStringLiteral("\n## Governed Project Resources\n\n");
        if (model.resources().isEmpty()) {
            canonicalAgent += QStringLiteral("No project resources are registered.\n");
        } else {
            for (const auto& resource : model.resources()) {
                canonicalAgent += QStringLiteral("- `%1` — role `%2`, authority `%3`, scope `%4`, enabled `%5`; resolve the source from `resources/resources.json`.\n")
                    .arg(resource.name, resource.role, resource.authorityLevel,
                         resource.scopes.isEmpty() ? QStringLiteral("all") : resource.scopes.join(QStringLiteral(", ")),
                         resource.enabled ? QStringLiteral("yes") : QStringLiteral("no"));
            }
        }
        const auto academic = model.academicConfiguration();
        const auto documentLine = [](const QString& name, const AcademicConfiguration::DocumentationConfiguration& document, const QString& instructionId) {
            const QString structure = document.templateMode == QStringLiteral("source") ? QStringLiteral("selected custom source controls structure; do not inject or map default headings") : QStringLiteral("ARAMF built-in structure is active");
            return QStringLiteral("- %1: enabled=%2, templateMode=%3, templateSourceId=%4, instruction=%5/v%6; %7. Preserve the original source, apply only this document-type instruction, and treat guidance as authoring assistance rather than final prose.\n")
                .arg(name, document.enabled ? QStringLiteral("yes") : QStringLiteral("no"), document.templateMode,
                     document.templateSourceId.isEmpty() ? QStringLiteral("ARAMF default") : document.templateSourceId,
                     instructionId, QString::number(document.instructionVersion), structure);
        };
        canonicalAgent += QStringLiteral("\n## Documentation Template Routing\n\n")
            + QStringLiteral("Academic project types are independently selected: ")
            + (academic.projectTypes.isEmpty() ? QStringLiteral("none") : academic.projectTypes.join(QStringLiteral(", "))) + QStringLiteral(". Do not treat them as mutually exclusive.\n")
            + documentLine(QStringLiteral("Thesis"), academic.thesisDocumentation, QStringLiteral("aramf-thesis-instruction"))
            + documentLine(QStringLiteral("Report"), academic.reportDocumentation, QStringLiteral("aramf-report-instruction"))
            + QStringLiteral("Thesis never uses the Report instruction; Report never uses the Thesis instruction. Custom template structure remains custom and external instructions retain their governed authority.\n")
            + QStringLiteral("The canonical built-in section hierarchy and bilingual authoring guidance are in `documentation/documentation-manifest.json` when documentation is enabled. Guidance is authoring assistance, not final document prose.\n");
        if (model.context() == QStringLiteral("android-application") || model.templateId() == QStringLiteral("android-studio-kotlin-gemini")
            || model.templateId() == QStringLiteral("official-android-arduino-smart-home") || model.templateId() == QStringLiteral("android-arduino-smart-home")) {
            const auto android = model.androidConstraints();
            canonicalAgent += QStringLiteral(
                "\n## Android / Kotlin project guidance\n\n"
                "Template: Android Studio/Kotlin/Gemini. This is an ARAMF-managed Android project whose active control-plane capabilities come from the selected configuration.\n"
                "Android Studio, Kotlin, Gradle, Android SDK, and Gemini are project tools/profiles; ARAMF governance remains agent-independent and canonical in ")
                + workerName + QStringLiteral("/.\n"
                "Android Studio is the primary IDE for Android application development. Project-support work may also use governed VS Code, command-line tools, Python utilities, SQLite tools, generators, scripts, and validation utilities when they affect this same managed project; the primary IDE is not exclusive.\n"
                "Effective Android constraints: Kotlin required; primary IDE %1; minimum SDK %2; UI technology %3; Compose allowed=%4 and effectively selected=%5; Room required=%6; unit tests required=%7; lint required=%8. These values are derived from the authoritative Source of Truth and are not Gradle source claims.\n"
                "Read `memory/project-knowledge.json` when present and apply only approved, applicable project-local lessons; project knowledge remains distinct from Framework Knowledge and is not automatically global.\n"
                "Treat an explicitly configured course assignment, rubric, teacher specification, or submission requirement recorded as a primary Source of Truth as higher authority than Android defaults. For example, course-required XML overrides a Compose preference, and a course-required persistence technology overrides Room.\n"
                "Prefer clear UI, state/ViewModel, domain, and data/repository responsibilities when the project benefits; use lifecycle-aware coroutines and Android resources/configuration. Do not add ceremony a small school project does not need.\n"
                "Use the existing validation profile: Gradle sync/configuration and compile are baseline where applicable; use `./gradlew build`, `./gradlew test`, `./gradlew lint`, or their `gradlew.bat` equivalents only when relevant and available. Keep BUILD PASS, TEST PASS, LINT PASS, EMULATOR VERIFIED, DEVICE VERIFIED, APPLICATION VERIFIED, and CERTIFIED distinct; never fabricate emulator/device/runtime evidence.\n"
                "Gemini, Codex, Claude, Copilot, and other agents follow this same governance and may be replaced without losing project state.\n");
            canonicalAgent = canonicalAgent.arg(android.primaryIde)
                .arg(android.minSdk)
                .arg(android.uiTechnology)
                .arg(android.composeAllowed ? QStringLiteral("YES") : QStringLiteral("NO"))
                .arg(android.composeSelected ? QStringLiteral("YES") : QStringLiteral("NO"))
                .arg(android.roomRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                .arg(android.unitTestsRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                .arg(android.lintRequired ? QStringLiteral("YES") : QStringLiteral("NO"));
            if (!android.courseName.isEmpty()) {
                canonicalAgent += QStringLiteral(
                    "\n## Course Source of Truth (derived constraints)\n\n"
                    "Course: %1 (source resource: %2). Project domain: %3. Architecture: %4.\n"
                    "Declared technologies: %5.\n"
                    "Compose required=%6; Retrofit required=%7; Gson required=%8; INTERNET permission required=%9; GitHub API required=%10.\n"
                    "GitHub PAT required=%11; PAT type=%12; scope=%13; hardcoded token prohibited=%14; local.properties to Gradle BuildConfig required=%15.\n"
                    "Repository: %16; visibility private required=%17; instructor collaborator required=%18; initial branch=%19; initial file=%20.\n"
                    "Application runtime state Source of Truth: %21; state model=%22; workflow=%23. This is distinct from the project Source of Truth.\n"
                    "Domain agents: %24. Development agents such as Gemini and Codex remain separate from the application simulation agents.\n"
                    "Polling: %25 seconds in %26 using %27; analysis uses %28. StateFlow=%29; collectAsState=%30; DeceptionDetector=%31; Regex/weighted heuristics=%32; confidence=%33-%34%%.\n"
                    "Normal green=%35; SecurityAlert red=%36; raw adversarial text=%37; human-in-the-loop=%38.\n"
                    "Course validation: %39. Submission: %40; video %41; segments: %42.\n"
                    "Unresolved / future lab detail (not inferred): %43.\n")
                    .arg(android.courseName, android.sourceOfTruthResource, android.projectDomain, android.architecture)
                    .arg(android.declaredTechnologies.join(QStringLiteral(", ")))
                    .arg(android.composeRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.retrofitRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.gsonRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.internetPermissionRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.githubApiRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.githubPatRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.classicPatRequired ? QStringLiteral("Classic") : QStringLiteral("UNSPECIFIED"))
                    .arg(android.patScope.isEmpty() ? QStringLiteral("UNSPECIFIED") : android.patScope)
                    .arg(android.hardcodedTokenProhibited ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.localPropertiesRequired && android.buildConfigRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.repositoryName.isEmpty() ? QStringLiteral("UNSPECIFIED") : android.repositoryName)
                    .arg(android.privateRepositoryRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.instructorCollaboratorRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.initialBranch.isEmpty() ? QStringLiteral("UNSPECIFIED") : android.initialBranch)
                    .arg(android.requiredInitialFile.isEmpty() ? QStringLiteral("UNSPECIFIED") : android.requiredInitialFile)
                    .arg(android.applicationStateSource.isEmpty() ? QStringLiteral("UNSPECIFIED") : android.applicationStateSource)
                    .arg(android.stateModel.isEmpty() ? QStringLiteral("UNSPECIFIED") : android.stateModel)
                    .arg(android.workflow.isEmpty() ? QStringLiteral("UNSPECIFIED") : android.workflow)
                    .arg(android.domainAgents.join(QStringLiteral(", ")))
                    .arg(android.pollingIntervalSeconds)
                    .arg(android.pollingExecutionLocation)
                    .arg(android.networkDispatcher)
                    .arg(android.analysisDispatcher)
                    .arg(android.stateFlowRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.collectAsStateRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.deceptionDetectorRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.regexOrHeuristicsRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.confidenceMinPercent < 0 ? QStringLiteral("UNSPECIFIED") : QString::number(android.confidenceMinPercent))
                    .arg(android.confidenceMaxPercent < 0 ? QStringLiteral("UNSPECIFIED") : QString::number(android.confidenceMaxPercent))
                    .arg(android.normalGreenRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.securityAlertRedRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.rawAdversarialTextRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.humanInTheLoopRequired ? QStringLiteral("YES") : QStringLiteral("NO"))
                    .arg(android.validationRequirements.join(QStringLiteral(", ")))
                    .arg(android.submissionRequirements.join(QStringLiteral(", ")))
                    .arg(android.submissionVideoDuration.isEmpty() ? QStringLiteral("UNSPECIFIED") : android.submissionVideoDuration)
                    .arg(android.submissionSegments.join(QStringLiteral(", ")))
                    .arg(android.unresolvedRequirements.join(QStringLiteral(", ")));
            }
        }
        if (model.templateId() == QStringLiteral("official-android-arduino-smart-home")
            || model.templateId() == QStringLiteral("android-arduino-smart-home")) {
            canonicalAgent += QStringLiteral(
                "\n## Android / Arduino KS0085 Smart Home guidance\n\n"
                "Android Studio is the primary IDE and Kotlin is the Android language. The embedded side is an Arduino-compatible Keyestudio KS0085 Smart Home platform using readable C/C++. Android communicates with the HM-10 Bluetooth module, which bridges to Arduino UART/serial. Wi-Fi and Raspberry Pi Pico are not part of this template.\n"
                "Keep Android connection/discovery state, command transmission, telemetry reception, dashboard UI, lifecycle-safe reconnection and error handling separated into understandable responsibilities. Keep Arduino initialization, sensor acquisition, actuator control, command parsing, response generation, safe hardware states and symbolic pin ownership explicit.\n"
                "Use `hardware/hardware-resources.json` as the hardware authority. Keep LED, relay, buzzer, fan, servo, LCD, PIR, MQ-2, photocell, soil-moisture, water/steam and push-button resources symbolic and replaceable; do not invent fixed GPIO numbers in communication code.\n"
                "Recommended progression: validate existing KS0085 hardware and firmware; test sensors and actuators individually; validate Bluetooth with the original implementation; connect a minimal Kotlin Bluetooth client; send one command; control one actuator; read one sensor; build the dashboard; integrate remaining devices; then improve protocol robustness and validate the complete Android-to-hardware workflow.\n"
                "The communication contract defines message structure, not a frozen legacy one-character protocol. Preserve UI/communication separation and make each increment observable and testable for a school project.\n");
        }
        if (model.templateId() == QStringLiteral("machine-learning")
            || model.context() == QStringLiteral("ai-machine-learning")) {
            canonicalAgent += QStringLiteral(
                "\n## Python Machine Learning guidance\n\n"
                "Use the selected Python environment and keep dependencies explicit. Keep source code separate from generated or runtime training output, avoid committing temporary caches and large artifacts, and preserve reproducibility-related configuration when it is available. Treat the saved ProjectModel capabilities as the authoritative description of the Python Machine Learning stack.\n");
        }
        canonicalAgent += QStringLiteral(
            "Run the minimum validation required by `routing/validation-policy.json`; do not run full regression campaigns for ordinary isolated changes. Escalate when scope, risk, failure, or explicit milestone policy requires it.\n");
        if (options.generateMemory) {
            const auto memory = model.memoryConfiguration();
            QString memorySection = QStringLiteral(
                "<!-- ARAMF-MEMORY-BEGIN -->\n"
                "\n## Project Memory Feedback\n\n"
                "Read `memory/memory-contract.json` before recording development results. In `agent-direct` mode the active coding agent is the project-local Project Memory writer; no aramf.exe, global `aramf` command, recorder daemon, or external service is required. "
                "Do not edit `memory/event-log.jsonl`, `memory/metrics.json`, `memory/current-state.md`, "
                "`memory/memory-manifest.json`, validation state, or `PROJECT_STATUS.md` bookkeeping fields outside the governed protocol. "
                "Identify canonical targets, read current files and schemas, preserve unrelated state, perform the narrowest valid mutation, write the existing schema, reload from disk, parse/validate, and verify uniqueness, ordering, and cross-file consistency.\n"
                "`memory/event-log.jsonl` is append-only historical evidence: preserve failed attempts, successful corrections, and their original IDs, sequences, timestamps, ordering, and PASS/FAIL results. Never rewrite prior events or regenerate history from current state.\n");
            const auto addInstruction = [&memorySection, &memory](const QString& option, const QString& text) {
                if (memory.maintenanceOptions.contains(option)) memorySection += QStringLiteral("- %1\n").arg(text);
            };
            addInstruction(QStringLiteral("record-task-completion"), QStringLiteral("Record meaningful task starts and completions."));
            addInstruction(QStringLiteral("record-build-results"), QStringLiteral("Record completed build attempts and their PASS/FAIL result."));
            addInstruction(QStringLiteral("record-test-results"), QStringLiteral("Record completed test attempts and their PASS/FAIL result."));
            addInstruction(QStringLiteral("record-validation"), QStringLiteral("Record meaningful validation outcomes."));
            addInstruction(QStringLiteral("update-current-state"), QStringLiteral("Let ProjectMemory refresh current-state from accepted events."));
            addInstruction(QStringLiteral("update-project-status"), QStringLiteral("Allow meaningful completed tasks to update PROJECT_STATUS through the recorder policy."));
            addInstruction(QStringLiteral("record-checkpoints"), QStringLiteral("Record a checkpoint only when an actual stable checkpoint is warranted."));
            memorySection += QStringLiteral("\n- Record durable decisions only for genuine architecture or policy choices through the decision workflow.\n");
            memorySection += QStringLiteral("- Follow current durable decisions; explicitly superseded decisions remain historical and inactive.\n");
            memorySection += QStringLiteral("\nThe active agent owns governed writes in `agent-direct` mode. `PROJECT_STATUS.md` and current-state files describe current truth; `memory/event-log.jsonl` preserves historical truth. Corrections and durable decision changes are represented as new evidence with explicit supersession.\n\n<!-- ARAMF-MEMORY-END -->\n");
            canonicalAgent += memorySection;
        }
        if (model.certificationConfiguration().enabled) {
            canonicalAgent += QStringLiteral(
                "\n## Test Certification\n\n"
                "Use certification for claims that a capability, component, workflow, integration, release, or hardware configuration has been verified under defined conditions. A test result records what happened; a certificate is the durable structured claim and evidence record.\n"
                "Certification history at `certification/certificates.jsonl` is append-only. Failed certification attempts remain historical; a retest creates a new certificate ID and may explicitly supersede or relate to an earlier certificate.\n"
                "Issue PASS only when every applicable required build, test, validation, runtime, on-target, physical, or other evidence requirement is present and verified. Never fabricate evidence or issue `HARDWARE_CERTIFIED` without physical/on-target evidence.\n"
                "Use `certification/current-certification-state.json` to rediscover current subject state; it is derived current state, not a replacement for certificate history. Project Memory records certification lifecycle events separately, and Framework Knowledge remains separate approved reusable knowledge.\n");
        }
        if (!writeTextFile(QDir(projectRoot).filePath(QStringLiteral("AGENTS.md")), rootAgent.toUtf8(), &error, true)) {
            return fail(QStringLiteral("Agent rules"), error);
        }
        const QString agentInstructionsPath = QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::AgentInstructions));
        if (options.generateMemory) {
            const int memoryBegin = canonicalAgent.indexOf(QStringLiteral("<!-- ARAMF-MEMORY-BEGIN -->"));
            const QString memorySection = memoryBegin >= 0 ? canonicalAgent.mid(memoryBegin) : QString();
            const int taskBegin = canonicalAgent.indexOf(QStringLiteral("<!-- ARAMF-TASK-GOVERNANCE-BEGIN -->"));
            const int taskEnd = canonicalAgent.indexOf(QStringLiteral("<!-- ARAMF-TASK-GOVERNANCE-END -->"), taskBegin);
            const QString taskSection = taskBegin >= 0 && taskEnd >= taskBegin
                ? canonicalAgent.mid(taskBegin, taskEnd + QStringLiteral("<!-- ARAMF-TASK-GOVERNANCE-END -->").size() - taskBegin)
                : QString();
            if (!QFile::exists(agentInstructionsPath)) {
                if (!writeTextFile(agentInstructionsPath, canonicalAgent.toUtf8(), &error)) return fail(QStringLiteral("Agent rules"), error);
            } else {
                if (!taskSection.isEmpty() && !upsertManagedSection(agentInstructionsPath,
                                                                      QStringLiteral("<!-- ARAMF-TASK-GOVERNANCE-BEGIN -->"),
                                                                      QStringLiteral("<!-- ARAMF-TASK-GOVERNANCE-END -->"),
                                                                      taskSection, &error)) {
                    return fail(QStringLiteral("Agent rules"), error);
                }
                if (!upsertManagedSection(agentInstructionsPath,
                                          QStringLiteral("<!-- ARAMF-MEMORY-BEGIN -->"),
                                          QStringLiteral("<!-- ARAMF-MEMORY-END -->"),
                                          memorySection, &error)) {
                    return fail(QStringLiteral("Agent rules"), error);
                }
            }
        } else if (!writeTextFile(agentInstructionsPath, canonicalAgent.toUtf8(), &error, true)) {
            return fail(QStringLiteral("Agent rules"), error);
        }

        const auto rules = model.ruleConfiguration();
        QString markdown = QStringLiteral("# Generated Project Rules\n\n");
        markdown += QStringLiteral("## Enforcement Level\n\n%1\n\n").arg(rules.enforcementLevel);
        markdown += QStringLiteral("## Active Rule Categories\n\n");
        if (rules.activeCategories.isEmpty()) {
            markdown += QStringLiteral("No active rule categories.\n\n");
        } else {
            for (const auto& id : rules.activeCategories) {
                markdown += QStringLiteral("- %1\n").arg(ruleDisplayName(id, RuleCatalog::categories()));
            }
            markdown += QLatin1Char('\n');
        }
        markdown += QStringLiteral("## Rule Loading Strategy\n\n%1\n\n").arg(rules.loadingStrategy);
        markdown += QStringLiteral("## Work-Type Routing\n\n");
        for (const auto& id : rules.workScopes) markdown += QStringLiteral("- %1\n").arg(ruleDisplayName(id, RuleCatalog::workScopes()));
        markdown += QStringLiteral("\n## Project-Area Routing\n\n");
        for (const auto& id : rules.projectScopes) markdown += QStringLiteral("- %1\n").arg(ruleDisplayName(id, RuleCatalog::projectScopes()));
        markdown += QStringLiteral("\n## Context / Token Efficiency\n\n");
        for (const auto& id : rules.contextPolicies) markdown += QStringLiteral("- %1\n").arg(ruleDisplayName(id, RuleCatalog::contextPolicies()));
        markdown += QStringLiteral("\n## Conflict Policy\n\n%1\n").arg(rules.conflictPolicy);
        if (!writeTextFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::GeneratedRules)), markdown.toUtf8(), &error)) {
            return fail(QStringLiteral("Agent rules"), error);
        }

        QString status = QStringLiteral(
            "<!-- PROJECT_STATUS.md -->\n\n# Project Status\n\n## Project\n\n"
            "- Name: %1\n- Project ID: %2\n- ARAMF state: Initialized\n\n"
            "## Implemented\n\n- ARAMF control-plane structure generated.\n\n"
            "## Verified\n\n- Generation completed for the selected output products.\n")
                                   .arg(model.projectName(), model.projectId());
        if (model.context() == QStringLiteral("android-application") || model.templateId() == QStringLiteral("android-studio-kotlin-gemini")
            || model.templateId() == QStringLiteral("official-android-arduino-smart-home") || model.templateId() == QStringLiteral("android-arduino-smart-home")) {
            status += QStringLiteral(
                "\n## Android Validation States\n\n"
                "Track IMPLEMENTED, BUILD PASS, TEST PASS, LINT PASS, EMULATOR VERIFIED, DEVICE VERIFIED, APPLICATION VERIFIED, and CERTIFIED separately.\n"
                "Unrun synchronization, emulator, device, lifecycle, permission, navigation, persistence, and runtime checks remain not verified.\n");
            if (model.templateId() == QStringLiteral("official-android-arduino-smart-home") || model.templateId() == QStringLiteral("android-arduino-smart-home"))
                status += QStringLiteral("\n## Arduino / KS0085 Validation States\n\nTrack Bluetooth pairing, UART/serial protocol, sensor acquisition, actuator control, safe-state behavior, and complete Android-to-hardware integration separately. Physical hardware evidence is required for on-target claims.\n");
            if (!model.androidConstraints().courseName.isEmpty()) {
                status += QStringLiteral(
                    "\n## Course Source of Truth\n\n"
                    "- Active: %1\n- Android application implementation: not implemented; implementation remains pending.\n"
                    "- GitHub Lab 0 completion: not independently verified.\n- No real PAT or other secret is stored by ARAMF.\n")
                               .arg(model.androidConstraints().courseName);
            }
        }
        if (!writeTextFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ProjectStatus)), status.toUtf8(), &error, true)) {
            return fail(QStringLiteral("Agent rules"), error);
        }
        addGeneratedFiles(result, {QStringLiteral("AGENTS.md"), AramfPaths::AgentInstructions,
                                   AramfPaths::ProjectStatus, AramfPaths::GeneratedRules});
    }

    {
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ProjectConfiguration)), projectConfiguration(model, result.fingerprint), &error))
            return fail(QStringLiteral("Project configuration"), error);
        addGeneratedFiles(result, {AramfPaths::ProjectConfiguration});
    }

    const auto gitIgnore = GitIgnoreService().ensureProjectGitIgnore(projectRoot);
    if (!gitIgnore.success) return fail(QStringLiteral("Project privacy (.gitignore)"), gitIgnore.error);
    if (gitIgnore.changed) result.generatedFiles.append(QStringLiteral(".gitignore"));

    if (options.generateRouting) {
        const auto rules = model.ruleConfiguration();
        const auto taskRoutes = WorkerContextResolver::taskRoutes(rules, result.fingerprint);
        const QJsonObject scopeRoutes = WorkerContextResolver::scopeRoutes(rules, result.fingerprint);
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::TaskRoutes)), taskRoutes, &error)
            || !writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ScopeRoutes)), scopeRoutes, &error)
            || !writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ValidationPolicy)), ValidationRouting::policy(), &error)) {
            return fail(QStringLiteral("Routing"), error);
        }
        const QString readme = QStringLiteral(
            "# Routing\n\n"
            "Strategy: %1\n\n"
            "Task routing selects relevant work types: %2\n\n"
            "Scope routing selects relevant project areas: %3\n\n"
            "Routing minimizes irrelevant AI context by loading only applicable rules and scopes.\n")
                                  .arg(rules.loadingStrategy, rules.workScopes.join(QStringLiteral(", ")), rules.projectScopes.join(QStringLiteral(", ")));
        const QString routingReadmePath = AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/routing/README.md"));
        if (!writeTextFile(QDir(projectRoot).filePath(routingReadmePath), readme.toUtf8(), &error)) {
            return fail(QStringLiteral("Routing"), error);
        }
        addGeneratedFiles(result, {AramfPaths::TaskRoutes, AramfPaths::ScopeRoutes, AramfPaths::ValidationPolicy, routingReadmePath});
    }

    if (options.generatePlatforms) {
        const auto environment = model.developmentEnvironment();
        const auto capabilities = model.developmentCapabilities();
        QJsonObject platform{
            {QStringLiteral("projectType"), projectTypeLabel(model)},
            {QStringLiteral("templateId"), model.templateId()},
            {QStringLiteral("environment"), QJsonObject{
                {QStringLiteral("language"), environment.language}, {QStringLiteral("framework"), environment.framework},
                {QStringLiteral("ide"), environment.ide}, {QStringLiteral("compiler"), environment.compiler},
                {QStringLiteral("operatingSystem"), environment.operatingSystem}, {QStringLiteral("targetPlatform"), environment.targetPlatform},
                {QStringLiteral("targetArchitecture"), environment.targetArchitecture}, {QStringLiteral("buildSystem"), environment.buildSystem},
                {QStringLiteral("packageManager"), environment.packageManager}, {QStringLiteral("versionControl"), environment.versionControl}}},
            {QStringLiteral("languages"), toJsonArray(capabilities.languages)},
            {QStringLiteral("frameworks"), toJsonArray(capabilities.frameworks)},
            {QStringLiteral("developmentTools"), toJsonArray(capabilities.developmentTools)},
            {QStringLiteral("targetPlatforms"), toJsonArray(capabilities.targetPlatforms)},
            {QStringLiteral("targetArchitectures"), toJsonArray(capabilities.targetArchitectures)},
            {QStringLiteral("processorFamilies"), toJsonArray(capabilities.processorFamilies)},
            {QStringLiteral("hardwareTargets"), toJsonArray(capabilities.hardwareTargets)},
            {QStringLiteral("toolchains"), toJsonArray(capabilities.toolchains)},
            {QStringLiteral("buildSystems"), toJsonArray(capabilities.buildSystems)}
        };
        if (model.context() == QStringLiteral("android-application") || model.templateId() == QStringLiteral("android-studio-kotlin-gemini")) {
            platform.insert(QStringLiteral("validationProfile"), QJsonObject{
                {QStringLiteral("requiredBaseline"), QJsonArray{QStringLiteral("gradle-sync-or-configuration"), QStringLiteral("gradle-compile")}},
                {QStringLiteral("optionalChecks"), QJsonArray{QStringLiteral("gradle-test"), QStringLiteral("gradle-lint"), QStringLiteral("instrumentation-test"), QStringLiteral("emulator-verification"), QStringLiteral("device-verification")}},
                {QStringLiteral("wrapperCommands"), QJsonArray{QStringLiteral("./gradlew build"), QStringLiteral("./gradlew test"), QStringLiteral("./gradlew lint"), QStringLiteral("gradlew.bat build"), QStringLiteral("gradlew.bat test"), QStringLiteral("gradlew.bat lint")}},
                {QStringLiteral("verificationPolicy"), QStringLiteral("Runtime, emulator, device, lifecycle, permission, navigation, and persistence claims require observed evidence and remain distinct from build/test/lint PASS.")}});
            platform.insert(QStringLiteral("architectureDefaults"), QJsonArray{QStringLiteral("Kotlin"), QStringLiteral("Android lifecycle awareness"), QStringLiteral("ViewModel where appropriate"), QStringLiteral("lifecycle-aware coroutines"), QStringLiteral("Android resources/configuration"), QStringLiteral("Room when structured local persistence is appropriate")});
            platform.insert(QStringLiteral("courseSourceOfTruth"), QStringLiteral("Explicit primary project resources such as assignment specifications and rubrics override these defaults."));
            const auto android = model.androidConstraints();
            QJsonObject effective{
                {QStringLiteral("language"), environment.language},
                {QStringLiteral("primaryIde"), android.primaryIde},
                {QStringLiteral("buildSystem"), environment.buildSystem},
                {QStringLiteral("minSdk"), android.minSdkSpecified ? QJsonValue(android.minSdk) : QJsonValue(QStringLiteral("UNSPECIFIED"))},
                {QStringLiteral("minSdkSource"), android.minSdkSource},
                {QStringLiteral("uiTechnology"), android.uiTechnology},
                {QStringLiteral("xmlRequired"), android.xmlRequired},
                {QStringLiteral("composeAvailable"), android.composeAvailable},
                {QStringLiteral("composeAllowed"), android.composeAllowed},
                {QStringLiteral("composeSelected"), android.composeSelected},
                {QStringLiteral("composeSource"), android.composeSource},
                {QStringLiteral("roomRequired"), android.roomRequired},
                {QStringLiteral("roomSource"), android.roomSource},
                {QStringLiteral("unitTestsRequired"), android.unitTestsRequired},
                {QStringLiteral("unitTestsSource"), android.unitTestsSource},
                {QStringLiteral("lintRequired"), android.lintRequired},
                {QStringLiteral("lintSource"), android.lintSource}};
            effective.insert(QStringLiteral("courseName"), android.courseName);
            effective.insert(QStringLiteral("courseNameSource"), android.courseNameSource);
            effective.insert(QStringLiteral("projectDomain"), android.projectDomain);
            effective.insert(QStringLiteral("projectDomainSource"), android.projectDomainSource);
            effective.insert(QStringLiteral("architecture"), android.architecture);
            effective.insert(QStringLiteral("architectureSource"), android.architectureSource);
            effective.insert(QStringLiteral("declaredTechnologies"), toJsonArray(android.declaredTechnologies));
            effective.insert(QStringLiteral("declaredTechnologiesSource"), android.declaredTechnologiesSource);
            effective.insert(QStringLiteral("composeRequired"), android.composeRequired);
            effective.insert(QStringLiteral("composeRequiredSource"), android.composeRequiredSource);
            effective.insert(QStringLiteral("retrofitRequired"), android.retrofitRequired);
            effective.insert(QStringLiteral("retrofitSource"), android.retrofitSource);
            effective.insert(QStringLiteral("gsonRequired"), android.gsonRequired);
            effective.insert(QStringLiteral("gsonSource"), android.gsonSource);
            effective.insert(QStringLiteral("internetPermissionRequired"), android.internetPermissionRequired);
            effective.insert(QStringLiteral("internetPermissionSource"), android.internetPermissionSource);
            effective.insert(QStringLiteral("githubApiRequired"), android.githubApiRequired);
            effective.insert(QStringLiteral("githubPatRequired"), android.githubPatRequired);
            effective.insert(QStringLiteral("classicPatRequired"), android.classicPatRequired);
            effective.insert(QStringLiteral("patScope"), android.patScope);
            effective.insert(QStringLiteral("hardcodedTokenProhibited"), android.hardcodedTokenProhibited);
            effective.insert(QStringLiteral("localPropertiesRequired"), android.localPropertiesRequired);
            effective.insert(QStringLiteral("buildConfigRequired"), android.buildConfigRequired);
            effective.insert(QStringLiteral("privateRepositoryRequired"), android.privateRepositoryRequired);
            effective.insert(QStringLiteral("instructorCollaboratorRequired"), android.instructorCollaboratorRequired);
            effective.insert(QStringLiteral("repositoryName"), android.repositoryName);
            effective.insert(QStringLiteral("initialBranch"), android.initialBranch);
            effective.insert(QStringLiteral("requiredInitialFile"), android.requiredInitialFile);
            effective.insert(QStringLiteral("applicationStateSource"), android.applicationStateSource);
            effective.insert(QStringLiteral("applicationStateFields"), toJsonArray(android.applicationStateFields));
            effective.insert(QStringLiteral("stateModel"), android.stateModel);
            effective.insert(QStringLiteral("workflow"), android.workflow);
            effective.insert(QStringLiteral("domainAgents"), toJsonArray(android.domainAgents));
            effective.insert(QStringLiteral("domainAgentDistinction"), android.domainAgentDistinction);
            effective.insert(QStringLiteral("pollingRequired"), android.pollingRequired);
            effective.insert(QStringLiteral("pollingIntervalSeconds"), android.pollingIntervalSeconds);
            effective.insert(QStringLiteral("pollingExecutionLocation"), android.pollingExecutionLocation);
            effective.insert(QStringLiteral("networkDispatcher"), android.networkDispatcher);
            effective.insert(QStringLiteral("analysisDispatcher"), android.analysisDispatcher);
            effective.insert(QStringLiteral("stateFlowRequired"), android.stateFlowRequired);
            effective.insert(QStringLiteral("collectAsStateRequired"), android.collectAsStateRequired);
            effective.insert(QStringLiteral("deceptionDetectorRequired"), android.deceptionDetectorRequired);
            effective.insert(QStringLiteral("regexOrHeuristicsRequired"), android.regexOrHeuristicsRequired);
            effective.insert(QStringLiteral("confidenceMinPercent"), android.confidenceMinPercent);
            effective.insert(QStringLiteral("confidenceMaxPercent"), android.confidenceMaxPercent);
            effective.insert(QStringLiteral("normalGreenRequired"), android.normalGreenRequired);
            effective.insert(QStringLiteral("securityAlertRedRequired"), android.securityAlertRedRequired);
            effective.insert(QStringLiteral("rawAdversarialTextRequired"), android.rawAdversarialTextRequired);
            effective.insert(QStringLiteral("humanInTheLoopRequired"), android.humanInTheLoopRequired);
            effective.insert(QStringLiteral("validationRequirements"), toJsonArray(android.validationRequirements));
            effective.insert(QStringLiteral("submissionRequirements"), toJsonArray(android.submissionRequirements));
            effective.insert(QStringLiteral("submissionVideoDuration"), android.submissionVideoDuration);
            effective.insert(QStringLiteral("submissionSegments"), toJsonArray(android.submissionSegments));
            effective.insert(QStringLiteral("unresolvedRequirements"), toJsonArray(android.unresolvedRequirements));
            effective.insert(QStringLiteral("sourceOfTruthTitle"), android.sourceOfTruthTitle);
            effective.insert(QStringLiteral("sourceOfTruthResource"), android.sourceOfTruthResource);
            if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::AndroidEffectiveConfig)), effective, &error)) return fail(QStringLiteral("Platform metadata"), error);
            addGeneratedFiles(result, {AramfPaths::AndroidEffectiveConfig});
        }
        const QString platformPath = AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/platforms/platform-metadata.json"));
        if (!writeJsonFile(QDir(projectRoot).filePath(platformPath), platform, &error)) return fail(QStringLiteral("Platform metadata"), error);
        addGeneratedFiles(result, {platformPath});
        const auto communication = model.communicationConfiguration();
        if (communication.enabled) {
            QJsonArray endpoints;
            for (const auto& endpoint : communication.endpoints)
                endpoints.append(QJsonObject{{QStringLiteral("id"), endpoint.id}, {QStringLiteral("targetId"), endpoint.targetId}, {QStringLiteral("displayName"), endpoint.displayName}, {QStringLiteral("role"), endpoint.role}, {QStringLiteral("capabilities"), toJsonArray(endpoint.capabilities)}, {QStringLiteral("address"), endpoint.address}});
            QJsonArray links;
            for (const auto& link : communication.links)
                links.append(QJsonObject{{QStringLiteral("id"), link.id}, {QStringLiteral("endpointA"), link.endpointA}, {QStringLiteral("endpointB"), link.endpointB}, {QStringLiteral("direction"), link.direction}, {QStringLiteral("transport"), link.transport}, {QStringLiteral("protocol"), link.protocol}, {QStringLiteral("frameType"), link.frameType}, {QStringLiteral("logicalDataModel"), link.logicalDataModel}, {QStringLiteral("wireEncoding"), link.wireEncoding}, {QStringLiteral("protocolVersion"), link.protocolVersion}, {QStringLiteral("byteOrder"), link.byteOrder}, {QStringLiteral("timeout"), link.timeout}, {QStringLiteral("reconnectPolicy"), link.reconnectPolicy}, {QStringLiteral("errorHandling"), link.errorHandling}, {QStringLiteral("acknowledgement"), link.acknowledgement}, {QStringLiteral("maximumPacketSize"), link.maximumPacketSize}});
            QJsonArray messages;
            for (const auto& message : communication.messages) {
                QJsonArray fields;
                for (const auto& field : message.fields)
                    fields.append(QJsonObject{{QStringLiteral("name"), field.name}, {QStringLiteral("type"), field.type}, {QStringLiteral("required"), field.required}, {QStringLiteral("description"), field.description}, {QStringLiteral("array"), field.array}, {QStringLiteral("min"), field.min}, {QStringLiteral("max"), field.max}, {QStringLiteral("defaultValue"), field.defaultValue}, {QStringLiteral("enumValues"), toJsonArray(field.enumValues)}});
                messages.append(QJsonObject{{QStringLiteral("id"), static_cast<qint64>(message.id)}, {QStringLiteral("name"), message.name}, {QStringLiteral("type"), message.type}, {QStringLiteral("direction"), message.direction}, {QStringLiteral("sourceEndpointId"), message.sourceEndpointId}, {QStringLiteral("destinationEndpointId"), message.destinationEndpointId}, {QStringLiteral("requestMessageId"), static_cast<qint64>(message.requestMessageId)}, {QStringLiteral("responseMessageId"), static_cast<qint64>(message.responseMessageId)}, {QStringLiteral("fields"), fields}, {QStringLiteral("description"), message.description}});
            }
            const QJsonObject contract{
                {QStringLiteral("endpoints"), endpoints},
                {QStringLiteral("links"), links},
                {QStringLiteral("messages"), messages},
                {QStringLiteral("authenticationRequired"), communication.authenticationRequired},
                {QStringLiteral("encryptionRequired"), communication.encryptionRequired},
                {QStringLiteral("reconnectPolicy"), communication.reconnectPolicy},
                {QStringLiteral("errorHandling"), communication.errorHandling},
                {QStringLiteral("integrationRequirements"), toJsonArray(communication.integrationRequirements)}};
            const QString contractPath = AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/communication/communication-contract.json"));
            if (!writeJsonFile(QDir(projectRoot).filePath(contractPath), contract, &error)) return fail(QStringLiteral("Communication contract"), error);
            addGeneratedFiles(result, {contractPath});
            const bool hasAndroid = capabilities.targetPlatforms.contains(QStringLiteral("android"));
            const bool hasPico = capabilities.hardwareTargets.contains(QStringLiteral("raspberry-pi-pico-2-w"))
                || capabilities.frameworks.contains(QStringLiteral("pico-sdk"));
            const bool hasArduino = capabilities.hardwareTargets.contains(QStringLiteral("arduino-mcu"))
                || capabilities.frameworks.contains(QStringLiteral("arduino"));
            if (hasAndroid && (hasPico || hasArduino)) {
                const QJsonObject embeddedTarget = hasArduino
                    ? QJsonObject{{QStringLiteral("id"), QStringLiteral("arduino")}, {QStringLiteral("name"), QStringLiteral("Arduino-compatible KS0085 Smart Home firmware")}, {QStringLiteral("buildSystem"), QStringLiteral("arduino-build")}, {QStringLiteral("environment"), QStringLiteral("Arduino IDE")}, {QStringLiteral("outputs"), QJsonArray{"HEX", "BIN"}}, {QStringLiteral("tests"), QJsonArray{"host/unit", "hardware when available"}}}
                    : QJsonObject{{QStringLiteral("id"), QStringLiteral("pico-2w")}, {QStringLiteral("name"), QStringLiteral("Raspberry Pi Pico 2 W firmware")}, {QStringLiteral("buildSystem"), QStringLiteral("cmake")}, {QStringLiteral("sdk"), QStringLiteral("pico-sdk")}, {QStringLiteral("outputs"), QJsonArray{"UF2", "ELF", "BIN"}}, {QStringLiteral("tests"), QJsonArray{"host/unit", "hardware when available"}}};
                const QJsonObject targets{
                    {QStringLiteral("projectScope"), hasArduino ? QStringLiteral("one coordinated Android + Arduino KS0085 Smart Home system") : QStringLiteral("one coordinated Android + Raspberry Pi Pico 2 W system")},
                    {QStringLiteral("targets"), QJsonArray{
                        QJsonObject{{QStringLiteral("id"), QStringLiteral("android")}, {QStringLiteral("name"), QStringLiteral("Android application")}, {QStringLiteral("buildSystem"), QStringLiteral("gradle")}, {QStringLiteral("environment"), QStringLiteral("Android Studio")}, {QStringLiteral("outputs"), QJsonArray{"APK", "AAB"}}, {QStringLiteral("tests"), QJsonArray{"JVM/unit", "instrumentation/UI"}}},
                        embeddedTarget}},
                    {QStringLiteral("orchestration"), hasArduino ? QJsonArray{"validate shared configuration", "build Arduino firmware", "run Arduino tests", "build Android application", "run Android tests", "run Bluetooth/serial contract tests", "verify combined system"} : QJsonArray{"validate shared configuration", "build Pico firmware", "run Pico tests", "build Android application", "run Android tests", "run communication contract tests", "verify combined system"}}};
                const QString targetsPath = AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/communication/multi-target-build.json"));
                if (!writeJsonFile(QDir(projectRoot).filePath(targetsPath), targets, &error)) return fail(QStringLiteral("Multi-target build model"), error);
                addGeneratedFiles(result, {targetsPath});
            }
        }
        if (!model.hardwareResources().isEmpty()) {
            QJsonArray hardware;
            for (const auto& resource : model.hardwareResources())
                hardware.append(QJsonObject{{QStringLiteral("id"), resource.id}, {QStringLiteral("endpointId"), resource.endpointId}, {QStringLiteral("resourceType"), resource.resourceType}, {QStringLiteral("physicalResource"), resource.physicalResource}, {QStringLiteral("direction"), resource.direction}, {QStringLiteral("logicalMode"), resource.logicalMode}, {QStringLiteral("activeLevel"), resource.activeLevel}, {QStringLiteral("purpose"), resource.purpose}, {QStringLiteral("ownership"), resource.ownership}, {QStringLiteral("capabilities"), toJsonArray(resource.capabilities)}});
            const QString hardwarePath = AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/hardware/hardware-resources.json"));
            if (!writeJsonFile(QDir(projectRoot).filePath(hardwarePath), QJsonObject{{QStringLiteral("resources"), hardware}}, &error)) return fail(QStringLiteral("Hardware resources"), error);
            addGeneratedFiles(result, {hardwarePath});
        }
    }

    if (options.generateResources) {
        QJsonArray resources;
        QHash<QString, int> resourceIdentities;
        const auto modelResources = model.resources();
        for (int resourceIndex = 0; resourceIndex < modelResources.size(); ++resourceIndex) {
            const auto& resource = modelResources.at(resourceIndex);
            const QString identity = canonicalResourceIdentity(resource, model.projectPath());
            if (!identity.isEmpty() && resourceIdentities.contains(identity)) {
                const auto& existing = modelResources.at(resourceIdentities.value(identity));
                const bool sameMetadata = existing.name == resource.name
                    && existing.type == resource.type
                    && existing.description == resource.description
                    && existing.enabled == resource.enabled
                    && existing.locationMode == resource.locationMode
                    && existing.role == resource.role
                    && existing.authorityLevel == resource.authorityLevel
                    && existing.scopes == resource.scopes
                    && existing.status == resource.status
                    && existing.loadingStrategyOverride == resource.loadingStrategyOverride
                    && existing.lastModified == resource.lastModified
                    && existing.fingerprint == resource.fingerprint;
                if (sameMetadata) continue;
                return fail(QStringLiteral("Resource manifest"),
                            QStringLiteral("Conflicting duplicate resource identity: %1").arg(identity));
            }
            if (!identity.isEmpty()) resourceIdentities.insert(identity, resourceIndex);
            resources.append(QJsonObject{
                {QStringLiteral("id"), resource.id}, {QStringLiteral("name"), resource.name},
                {QStringLiteral("type"), resource.type}, {QStringLiteral("location"), resource.location},
                {QStringLiteral("description"), resource.description}, {QStringLiteral("enabled"), resource.enabled},
                {QStringLiteral("locationMode"), resource.locationMode}, {QStringLiteral("role"), resource.role}, {QStringLiteral("authority"), resource.authorityLevel},
                {QStringLiteral("scopes"), toJsonArray(resource.scopes)}, {QStringLiteral("status"), resource.status},
                {QStringLiteral("loadingPolicyOverride"), resource.loadingStrategyOverride}
            });
        }
        const QJsonObject manifest{{QStringLiteral("resources"), resources},
                                   {QStringLiteral("loadingStrategy"), model.resourcePolicy().loadingStrategy}};
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ResourceManifest)), manifest, &error)) return fail(QStringLiteral("Resource manifest"), error);
        addGeneratedFiles(result, {AramfPaths::ResourceManifest});
    }

    if (options.generateMemory) {
        ProjectMemory memory;
        if (!memory.initializeMemory(projectRoot, &model, &error)) return fail(QStringLiteral("Project Memory"), error);
        addGeneratedFiles(result, {AramfPaths::MemoryConfiguration, AramfPaths::Manifest,
                                   AramfPaths::EventLog, AramfPaths::CurrentState,
                                   AramfPaths::ColdStartValidation, AramfPaths::ConsistencyValidation,
                                   AramfPaths::Checkpoints, AramfPaths::Metrics, AramfPaths::Decisions,
                                   AramfPaths::MemoryContract});
    }

    if (model.certificationConfiguration().enabled) {
        CertificationService certification;
        if (!certification.initialize(projectRoot, &model, &error)) return fail(QStringLiteral("Test Certification"), error);
        addGeneratedFiles(result, {AramfPaths::CertificationContract, AramfPaths::Certificates,
                                   AramfPaths::CurrentCertificationState});
    }

    if (options.generateProvenance) {
        const auto capabilities = model.developmentCapabilities();
        const auto ai = model.aiConfiguration();
        const QJsonObject provenance{
            {QStringLiteral("status"), QStringLiteral("managed")}, {QStringLiteral("implementation"), QStringLiteral("C++")},
            {QStringLiteral("projectId"), model.projectId()}, {QStringLiteral("projectName"), model.projectName()},
            {QStringLiteral("template"), model.templateId()}, {QStringLiteral("projectType"), projectTypeLabel(model)}};
        const QJsonObject effects{
            {QStringLiteral("template"), model.templateId()}, {QStringLiteral("templateModules"), toJsonArray(model.templateModules())}, {QStringLiteral("languages"), toJsonArray(capabilities.languages)},
            {QStringLiteral("frameworks"), toJsonArray(capabilities.frameworks)}, {QStringLiteral("platforms"), toJsonArray(capabilities.targetPlatforms)},
            {QStringLiteral("hardware"), toJsonArray(capabilities.hardwareTargets)}, {QStringLiteral("primaryAiAgent"), ai.primaryAgent},
            {QStringLiteral("resources"), model.resources().size()}, {QStringLiteral("rules"), toJsonArray(model.ruleConfiguration().activeCategories)},
            {QStringLiteral("memoryMaximumSizeBytes"), model.memoryConfiguration().maximumSizeBytes},
            {QStringLiteral("academic"), QJsonObject{{QStringLiteral("enabled"), model.academicConfiguration().enabled}, {QStringLiteral("projectTypes"), toJsonArray(model.academicConfiguration().projectTypes)}}},
            {QStringLiteral("thesisDocumentation"), QJsonObject{{QStringLiteral("enabled"), model.academicConfiguration().thesisDocumentation.enabled}, {QStringLiteral("templateMode"), model.academicConfiguration().thesisDocumentation.templateMode}, {QStringLiteral("templateSourceId"), model.academicConfiguration().thesisDocumentation.templateSourceId}}},
            {QStringLiteral("reportDocumentation"), QJsonObject{{QStringLiteral("enabled"), model.academicConfiguration().reportDocumentation.enabled}, {QStringLiteral("templateMode"), model.academicConfiguration().reportDocumentation.templateMode}, {QStringLiteral("templateSourceId"), model.academicConfiguration().reportDocumentation.templateSourceId}}}};
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::Provenance)), provenance, &error)
            || !writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::SelectionEffects)), effects, &error)) {
            return fail(QStringLiteral("Provenance"), error);
        }
        addGeneratedFiles(result, {AramfPaths::Provenance, AramfPaths::SelectionEffects});
    }

    const auto academic = model.academicConfiguration();
    if (options.generateAgentRules && (academic.thesisDocumentation.enabled || academic.reportDocumentation.enabled)) {
        const auto validateSelectedSource = [&](const AcademicConfiguration::DocumentationConfiguration& configuration, const QString& role, const QString& label) {
            if (!configuration.enabled) return QString();
            if (configuration.templateMode != QStringLiteral("source")) {
                int candidates = 0;
                for (const auto& resource : model.resources()) if (resource.enabled && resource.role == role) ++candidates;
                return candidates > 1 ? label + QStringLiteral(" custom template resources are ambiguous; resolve authority in Resources") : QString();
            }
            const auto selected = std::find_if(model.resources().cbegin(), model.resources().cend(), [&](const ProjectResource& resource) { return resource.id == configuration.templateSourceId; });
            if (selected == model.resources().cend()) return label + QStringLiteral(" custom template resource is missing");
            if (selected->role != role) return label + QStringLiteral(" custom template resource has the wrong role");
            if (!selected->enabled) return label + QStringLiteral(" custom template resource is disabled");
            const auto inspection = DocumentTemplateInspector::inspect(*selected, projectRoot);
            if (!inspection.exists) return label + QStringLiteral(" custom template source file is missing");
            if (!inspection.formatRecognized) return label + QStringLiteral(" custom template format is unsupported");
            return QString();
        };
        const QString thesisSourceError = validateSelectedSource(academic.thesisDocumentation, QStringLiteral("thesis-template"), QStringLiteral("Thesis"));
        const QString reportSourceError = validateSelectedSource(academic.reportDocumentation, QStringLiteral("report-template"), QStringLiteral("Report"));
        if (!thesisSourceError.isEmpty()) return fail(QStringLiteral("Thesis documentation template"), thesisSourceError);
        if (!reportSourceError.isEmpty()) return fail(QStringLiteral("Report documentation template"), reportSourceError);
        auto documentationManifest = DocumentTemplates::manifest(
            academic.thesisDocumentation.enabled, academic.thesisDocumentation.templateMode, academic.thesisDocumentation.templateSourceId,
            academic.reportDocumentation.enabled,
            academic.reportDocumentation.templateMode, academic.reportDocumentation.templateSourceId,
            academic.thesisDocumentation.language, academic.reportDocumentation.language);
        QJsonArray documents = documentationManifest.value(QStringLiteral("documents")).toArray();
        const auto enrichDocument = [&](QJsonObject document, const AcademicConfiguration::DocumentationConfiguration& configuration, const QString& role, const DocumentInstruction& instruction) {
            document.insert(QStringLiteral("instructionId"), instruction.id);
            document.insert(QStringLiteral("instructionVersion"), instruction.version);
            document.insert(QStringLiteral("templateRole"), role);
            document.insert(QStringLiteral("validationState"), QStringLiteral("valid"));
            if (configuration.templateMode == QStringLiteral("source")) {
                ProjectResource selected;
                bool found = false;
                for (const auto& resource : model.resources()) if (resource.id == configuration.templateSourceId && resource.role == role) { selected = resource; found = true; break; }
                if (!found) {
                    document.insert(QStringLiteral("validationState"), QStringLiteral("invalid-resource"));
                    document.insert(QStringLiteral("sourceId"), configuration.templateSourceId);
                    document.insert(QStringLiteral("capability"), QStringLiteral("unknown"));
                } else {
                    const auto inspection = DocumentTemplateInspector::inspect(selected, projectRoot);
                    document.insert(QStringLiteral("sourceIdentity"), canonicalResourceIdentity(selected, projectRoot));
                    document.insert(QStringLiteral("sourcePath"), selected.location);
                    document.insert(QStringLiteral("fileName"), inspection.fileName);
                    document.insert(QStringLiteral("format"), inspection.format);
                    document.insert(QStringLiteral("capability"), inspection.capability);
                    document.insert(QStringLiteral("structurallyParsed"), inspection.structurallyParsed);
                    document.insert(QStringLiteral("present"), inspection.exists);
                    document.insert(QStringLiteral("sourceEnabled"), selected.enabled);
                    document.insert(QStringLiteral("contentHash"), inspection.contentHash);
                    document.insert(QStringLiteral("changed"), !selected.fingerprint.isEmpty() && selected.fingerprint != inspection.contentHash);
                    document.insert(QStringLiteral("parsedSectionCount"), inspection.structurallyParsed ? inspection.sections.size() : 0);
                    document.insert(QStringLiteral("warnings"), QJsonArray::fromStringList(inspection.warnings));
                    if (!selected.enabled) document.insert(QStringLiteral("validationState"), QStringLiteral("disabled-resource"));
                    else if (!inspection.exists) document.insert(QStringLiteral("validationState"), QStringLiteral("missing-source"));
                    else if (!inspection.formatRecognized) document.insert(QStringLiteral("validationState"), QStringLiteral("unsupported-format"));
                }
            }
            return document;
        };
        documents[0] = enrichDocument(documents[0].toObject(), academic.thesisDocumentation, QStringLiteral("thesis-template"), DocumentInstructions::thesis());
        documents[1] = enrichDocument(documents[1].toObject(), academic.reportDocumentation, QStringLiteral("report-template"), DocumentInstructions::report());
        documentationManifest.insert(QStringLiteral("documents"), documents);
        const QString documentationPath = QStringLiteral("ARAMF_WORKER/documentation/documentation-manifest.json");
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(documentationPath)), documentationManifest, &error))
            return fail(QStringLiteral("Documentation template manifest"), error);
        addGeneratedFiles(result, {documentationPath});
    }

    const QJsonObject generationState{
        {QStringLiteral("fingerprint"), result.fingerprint},
        {QStringLiteral("projectRoot"), projectRoot},
        {QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("agentRules"), options.generateAgentRules},
        {QStringLiteral("routing"), options.generateRouting},
        {QStringLiteral("platforms"), options.generatePlatforms},
        {QStringLiteral("resources"), options.generateResources},
        {QStringLiteral("memory"), options.generateMemory},
        {QStringLiteral("provenance"), options.generateProvenance}};
    const QString generationStatePath = AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/verification/generation-state.json"));
    if (!writeJsonFile(QDir(projectRoot).filePath(generationStatePath),
                       generationState, &error)) {
        return fail(QStringLiteral("Generation state"), error);
    }
    addGeneratedFiles(result, {generationStatePath});

    if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::WorkerManifest)), workerManifest(model, options, result.fingerprint), &error))
        return fail(QStringLiteral("Worker manifest"), error);
    addGeneratedFiles(result, {AramfPaths::WorkerManifest});

    // Generation writes the complete selected product set after ProjectMemory
    // initialization. Refresh derived state last so cold-start validation
    // describes the final generated control plane, not the pre-generation tree.
    if (options.generateMemory) {
        ProjectMemory memory;
        QString memoryError;
        if (!memory.refreshDerivedState(projectRoot, &memoryError))
            return fail(QStringLiteral("Project Memory derived state"), memoryError);
        addGeneratedFiles(result, {AramfPaths::CurrentState, AramfPaths::ColdStartValidation});
    }
    // The worker identity JSON uses the same resolved basename as its
    // containing directory. Keep any legacy profile handling independent,
    // while publishing the canonical identity filename.
    const QString resolvedWorkerName = AramfPaths::runtimeWorkerDirectoryName();
    const QString identityPath = QDir(projectRoot).filePath(
        resolvedWorkerName + QStringLiteral("/") + resolvedWorkerName + QStringLiteral(".json"));
    const QJsonObject identity{
        {QStringLiteral("name"), QStringLiteral("AI Rules & Memory Framework")},
        {QStringLiteral("version"), 2},
        {QStringLiteral("projectId"), model.projectId()},
        {QStringLiteral("projectName"), model.projectName()},
        {QStringLiteral("implementationLanguage"), QStringLiteral("C++")},
        {QStringLiteral("controlDirectory"), resolvedWorkerName}
    };
    if (!writeJsonFile(identityPath, identity, &error))
        return fail(QStringLiteral("Worker identity"), error);
    result.generatedFiles.append(resolvedWorkerName + QStringLiteral("/") + resolvedWorkerName + QStringLiteral(".json"));
    result.success = true;
    return result;
}

VerificationServices::VerificationServices(QObject* parent)
    : QObject(parent)
{
}

QJsonObject GenerationServices::derivedTaskArtifacts(const ProjectModel& model, const GenerationOptions& options)
{
    WorkerNameScope scope(model.workerNameSuffix());
    const QString fingerprint = projectConfigurationFingerprint(model, options);
    QJsonObject outputs;
    outputs.insert(AramfPaths::resolveWorkerRelativePath(AramfPaths::ProjectConfiguration), projectConfiguration(model, fingerprint));
    outputs.insert(AramfPaths::resolveWorkerRelativePath(AramfPaths::WorkerManifest), workerManifest(model, options, fingerprint));
    if (options.generateRouting) {
        outputs.insert(AramfPaths::resolveWorkerRelativePath(AramfPaths::TaskRoutes), WorkerContextResolver::taskRoutes(model.ruleConfiguration(), fingerprint));
        outputs.insert(AramfPaths::resolveWorkerRelativePath(AramfPaths::ScopeRoutes), WorkerContextResolver::scopeRoutes(model.ruleConfiguration(), fingerprint));
    }
    return outputs;
}

VerificationResult VerificationServices::verify(const ProjectModel& model,
                                                const GenerationOptions& expectedOptions, bool persistEvidence) const
{
    WorkerNameScope workerScope(model.workerNameSuffix());
    VerificationResult result;
    result.fingerprint = projectConfigurationFingerprint(model, expectedOptions);
    const QString root = QDir::cleanPath(model.projectPath());
    if (root.isEmpty() || root == QStringLiteral(".")) {
        result.error = QStringLiteral("Project path is not configured.");
        addCheck(result, QStringLiteral("project-path"), QStringLiteral("Project Path"), VerificationStatus::Fail, result.error);
        result.overallStatus = VerificationStatus::Fail;
        return result;
    }
    addCheck(result, QStringLiteral("project-path"), QStringLiteral("Project Path"),
             QDir(root).exists() ? VerificationStatus::Pass : VerificationStatus::Fail,
             QDir(root).exists() ? root : QStringLiteral("Project path does not exist."));
    const QString worker = QDir(root).filePath(AramfPaths::runtimeWorkerDirectoryName());
    const QString legacy = QDir(root).filePath(AramfPaths::LegacyControlDirectory);
    const bool workerExists = QDir(worker).exists();
    addCheck(result, QStringLiteral("control-plane"), QStringLiteral("ARAMF_WORKER control plane"),
             workerExists ? VerificationStatus::Pass : VerificationStatus::Fail,
             workerExists ? QStringLiteral("ARAMF_WORKER directory exists.") : QStringLiteral("ARAMF_WORKER control plane has not been generated."));
    if (legacy != worker && QDir(legacy).exists()) {
        addCheck(result, QStringLiteral("legacy-control-plane"), QStringLiteral("Legacy ARAMF control plane"),
                 VerificationStatus::Warning,
                 workerExists ? QStringLiteral("Legacy ARAMF/ preserved; ARAMF_WORKER/ is authoritative.") : QStringLiteral("Legacy ARAMF/ detected; migrate before use."));
    }
    if (!workerExists) {
        result.overallStatus = VerificationStatus::Fail;
        return result;
    }

    auto checkFile = [&](const QString& id, const QString& name, const QString& relative, bool expected) {
        if (!expected) {
            addCheck(result, id, name, VerificationStatus::NotApplicable, QStringLiteral("Product was not selected."));
            return;
        }
        const QString path = QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(relative));
        addCheck(result, id, name, readableNonEmpty(path) ? VerificationStatus::Pass : VerificationStatus::Fail,
                 readableNonEmpty(path) ? relative : QStringLiteral("Missing or empty: %1").arg(relative));
    };
    checkFile(QStringLiteral("project-configuration"), QStringLiteral("Canonical project configuration"), AramfPaths::ProjectConfiguration, true);
    checkFile(QStringLiteral("worker-manifest"), QStringLiteral("Worker topology manifest"), AramfPaths::WorkerManifest, true);
    QJsonObject topologyManifest;
    QString topologyError;
    const bool topologyReadable = readJson(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::WorkerManifest)), &topologyManifest, &topologyError);
    const bool supportedTopology = topologyReadable
        && topologyManifest.value(QStringLiteral("workerSchemaVersion")).toInt(-1) == 1
        && topologyManifest.value(QStringLiteral("canonicalFiles")).toObject().contains(QStringLiteral("projectConfiguration"));
    addCheck(result, QStringLiteral("worker-topology"), QStringLiteral("Worker topology schema"),
             supportedTopology ? VerificationStatus::Pass : VerificationStatus::Fail,
             supportedTopology ? QStringLiteral("Worker schema version 1 and canonical ownership are valid.")
                               : (topologyError.isEmpty() ? QStringLiteral("Unsupported or incomplete Worker schema.") : topologyError));
    const auto canonical = topologyManifest.value(QStringLiteral("canonicalFiles")).toObject();
    QStringList canonicalPaths;
    for (const auto& value : canonical) canonicalPaths.append(value.toString());
    const bool duplicateOwners = canonicalPaths.size() != QSet<QString>(canonicalPaths.cbegin(), canonicalPaths.cend()).size();
    addCheck(result, QStringLiteral("canonical-owners"), QStringLiteral("Unique canonical file ownership"),
             duplicateOwners ? VerificationStatus::Fail : VerificationStatus::Pass,
             duplicateOwners ? QStringLiteral("Worker manifest assigns one path to multiple canonical owners.") : QStringLiteral("Canonical ownership is unique."));
    QHash<QString, int> canonicalFileCounts;
    const QSet<QString> canonicalNames{QStringLiteral("project.json"), QStringLiteral("worker-manifest.json"), QStringLiteral("scope-routes.json"), QStringLiteral("task-routes.json"), QStringLiteral("current-state.md"), QStringLiteral("latest-validation.json"), QStringLiteral("resources.json")};
    QStringList parallelStateFiles;
    int routingRoots = 0;
    int memoryRoots = 0;
    QDirIterator workerFiles(worker, QDir::Files, QDirIterator::Subdirectories);
    while (workerFiles.hasNext()) {
        const QFileInfo file(workerFiles.next());
        const QString relative = QDir(worker).relativeFilePath(file.absoluteFilePath());
        if (relative.startsWith(QStringLiteral("custom/"), Qt::CaseInsensitive)) continue;
        if (canonicalNames.contains(file.fileName())) {
            ++canonicalFileCounts[file.fileName()];
            if (canonicalFileCounts[file.fileName()] > 1) parallelStateFiles.append(relative);
        }
        const QString lowerName = file.fileName().toLower();
        if (lowerName == QStringLiteral("agent-status.json") || lowerName == QStringLiteral("agent-memory.json")
            || lowerName == QStringLiteral("memory-store.json") || lowerName == QStringLiteral("status.db")) parallelStateFiles.append(relative);
    }
    QDirIterator workerDirectories(worker, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (workerDirectories.hasNext()) {
        const QFileInfo directory(workerDirectories.next());
        const QString relative = QDir(worker).relativeFilePath(directory.absoluteFilePath());
        if (relative.startsWith(QStringLiteral("custom/"), Qt::CaseInsensitive)) continue;
        if (directory.fileName().compare(QStringLiteral("routing"), Qt::CaseInsensitive) == 0) ++routingRoots;
        if (directory.fileName().compare(QStringLiteral("memory"), Qt::CaseInsensitive) == 0) ++memoryRoots;
    }
    const auto expectedCount = [&canonicalFileCounts](const QString& name, bool expected) {
        return !expected || canonicalFileCounts.value(name) == 1;
    };
    const bool parallelStateValid = parallelStateFiles.isEmpty()
        && expectedCount(QStringLiteral("project.json"), true)
        && expectedCount(QStringLiteral("worker-manifest.json"), true)
        && expectedCount(QStringLiteral("scope-routes.json"), expectedOptions.generateRouting)
        && expectedCount(QStringLiteral("task-routes.json"), expectedOptions.generateRouting)
        && expectedCount(QStringLiteral("current-state.md"), expectedOptions.generateMemory)
        && canonicalFileCounts.value(QStringLiteral("latest-validation.json")) <= 1
        && expectedCount(QStringLiteral("resources.json"), expectedOptions.generateResources)
        && (!expectedOptions.generateRouting || routingRoots == 1)
        && (!expectedOptions.generateMemory || memoryRoots == 1);
    addCheck(result, QStringLiteral("parallel-state"), QStringLiteral("No competing canonical state systems"),
             parallelStateValid ? VerificationStatus::Pass : VerificationStatus::Fail,
             parallelStateValid ? QStringLiteral("Canonical Worker state roots are unique.") : QStringLiteral("Competing canonical or ad-hoc Worker state detected: %1").arg(parallelStateFiles.join(QStringLiteral(", "))));
    QJsonObject projectConfig;
    QString projectConfigError;
    const bool projectConfigReadable = readJson(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ProjectConfiguration)), &projectConfig, &projectConfigError);
    const bool projectIdentityCurrent = projectConfigReadable
        && projectConfig.value(QStringLiteral("projectId")).toString() == model.projectId()
        && projectConfig.value(QStringLiteral("workerIdentity")).toString() == AramfPaths::runtimeWorkerDirectoryName();
    addCheck(result, QStringLiteral("project-identity"), QStringLiteral("Project identity is current"),
             projectIdentityCurrent ? VerificationStatus::Pass : VerificationStatus::Fail,
             projectIdentityCurrent ? QStringLiteral("project.json matches the active ProjectModel.") : (projectConfigError.isEmpty() ? QStringLiteral("project.json is stale or belongs to another project.") : projectConfigError));
    const bool projectFingerprintCurrent = projectConfigReadable
        && projectConfig.value(QStringLiteral("configurationFingerprint")).toString() == result.fingerprint;
    addCheck(result, QStringLiteral("stale-project-configuration"), QStringLiteral("Project configuration fingerprint"),
             projectFingerprintCurrent ? VerificationStatus::Pass : VerificationStatus::Warning,
             projectFingerprintCurrent ? QStringLiteral("project.json dependencies are current.") : QStringLiteral("project.json is stale; regenerate the Worker."));
    const bool manifestFingerprintCurrent = supportedTopology
        && topologyManifest.value(QStringLiteral("generatedFromFingerprint")).toString() == result.fingerprint;
    addCheck(result, QStringLiteral("stale-worker-manifest"), QStringLiteral("Worker manifest fingerprint"),
             manifestFingerprintCurrent ? VerificationStatus::Pass : VerificationStatus::Warning,
             manifestFingerprintCurrent ? QStringLiteral("Worker topology dependencies are current.") : QStringLiteral("worker-manifest.json is stale; regenerate the Worker."));
    if (expectedOptions.generateRouting) {
        QJsonObject routes; QString routeError;
        const bool routesReadable = readJson(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::TaskRoutes)), &routes, &routeError);
        const bool routesCurrent = routesReadable && routes.value(QStringLiteral("inputFingerprint")).toString() == result.fingerprint;
        addCheck(result, QStringLiteral("stale-routing"), QStringLiteral("Routing dependency fingerprint"),
                 routesCurrent ? VerificationStatus::Pass : VerificationStatus::Warning,
                 routesCurrent ? QStringLiteral("Routing dependencies are current.") : QStringLiteral("Routing metadata is stale; regenerate the Worker."));
    }
    checkFile(QStringLiteral("root-agents"), QStringLiteral("Root AGENTS.md"), QStringLiteral("AGENTS.md"), expectedOptions.generateAgentRules);
    checkFile(QStringLiteral("aramf-worker-agents"), QStringLiteral("ARAMF_WORKER/AGENTS.md"), AramfPaths::AgentInstructions, expectedOptions.generateAgentRules);
    checkFile(QStringLiteral("project-status"), QStringLiteral("PROJECT_STATUS.md"), AramfPaths::ProjectStatus, expectedOptions.generateAgentRules);
    checkFile(QStringLiteral("generated-rules"), QStringLiteral("Generated rules"), AramfPaths::GeneratedRules, expectedOptions.generateAgentRules);
    checkFile(QStringLiteral("task-routes"), QStringLiteral("Task routes"), AramfPaths::TaskRoutes, expectedOptions.generateRouting);
    checkFile(QStringLiteral("scope-routes"), QStringLiteral("Scope routes"), AramfPaths::ScopeRoutes, expectedOptions.generateRouting);
    checkFile(QStringLiteral("platforms"), QStringLiteral("Platform metadata"), QStringLiteral("ARAMF_WORKER/platforms/platform-metadata.json"), expectedOptions.generatePlatforms);
    checkFile(QStringLiteral("resources"), QStringLiteral("Resource manifest"), AramfPaths::ResourceManifest, expectedOptions.generateResources);
    checkFile(QStringLiteral("memory"), QStringLiteral("Memory configuration"), AramfPaths::MemoryConfiguration, expectedOptions.generateMemory);
    checkFile(QStringLiteral("framework-knowledge"), QStringLiteral("Framework Knowledge"), AramfPaths::FrameworkKnowledge, expectedOptions.generateMemory);
    checkFile(QStringLiteral("provenance"), QStringLiteral("Provenance"), AramfPaths::Provenance, expectedOptions.generateProvenance);
    checkFile(QStringLiteral("selection-effects"), QStringLiteral("Selection effects"), AramfPaths::SelectionEffects, expectedOptions.generateProvenance);
    checkFile(QStringLiteral("communication-contract"), QStringLiteral("Communication contract"), QStringLiteral("ARAMF_WORKER/communication/communication-contract.json"), model.communicationConfiguration().enabled && expectedOptions.generatePlatforms);
    const auto capabilitiesForVerification = model.developmentCapabilities();
    const bool combinedTargets = capabilitiesForVerification.targetPlatforms.contains(QStringLiteral("android"))
        && (capabilitiesForVerification.hardwareTargets.contains(QStringLiteral("raspberry-pi-pico-2-w")) || capabilitiesForVerification.frameworks.contains(QStringLiteral("pico-sdk")));
    const bool arduinoTargets = capabilitiesForVerification.targetPlatforms.contains(QStringLiteral("android"))
        && (capabilitiesForVerification.hardwareTargets.contains(QStringLiteral("arduino-mcu")) || capabilitiesForVerification.frameworks.contains(QStringLiteral("arduino")));
    checkFile(QStringLiteral("multi-target-build"), QStringLiteral("Multi-target build model"), QStringLiteral("ARAMF_WORKER/communication/multi-target-build.json"), (combinedTargets || arduinoTargets) && model.communicationConfiguration().enabled && expectedOptions.generatePlatforms);
    const bool certificationEnabled = model.certificationConfiguration().enabled;
    checkFile(QStringLiteral("certification-contract"), QStringLiteral("Certification contract"), AramfPaths::CertificationContract, certificationEnabled);
    checkFile(QStringLiteral("current-certification-state"), QStringLiteral("Current certification state"), AramfPaths::CurrentCertificationState, certificationEnabled);
    if (certificationEnabled) {
        CertificationService certification;
        QString certificationError;
        QFile certificateHistoryFile(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::Certificates)));
        const bool certificateHistoryReadable = certificateHistoryFile.exists()
            && certificateHistoryFile.open(QIODevice::ReadOnly | QIODevice::Text);
        certificateHistoryFile.close();
        addCheck(result, QStringLiteral("certificates"), QStringLiteral("Certificate history"),
                 certificateHistoryReadable ? VerificationStatus::Pass : VerificationStatus::Fail,
                 certificateHistoryReadable ? QStringLiteral("Append-only certificate history exists.") : QStringLiteral("Missing certificate history."));
        const auto history = certification.certificates(root, &certificationError);
        addCheck(result, QStringLiteral("certification-history"), QStringLiteral("Certificate history parses"),
                 certificationError.isEmpty() ? VerificationStatus::Pass : VerificationStatus::Fail,
                 certificationError.isEmpty() ? QStringLiteral("Append-only certificate history is readable.") : certificationError);
        const auto contract = certification.contract(root, &certificationError);
        addCheck(result, QStringLiteral("certification-policy"), QStringLiteral("Certification evidence policy"),
                 !contract.isEmpty() && contract.value(QStringLiteral("passRule")).toString().contains(QStringLiteral("evidence"))
                     ? VerificationStatus::Pass : VerificationStatus::Fail,
                 contract.isEmpty() ? certificationError : QStringLiteral("Certification contract defines evidence gating."));
        Q_UNUSED(history);
    }

    if (expectedOptions.generateResources) {
        QHash<QString, int> identities;
        bool duplicate = false;
        QString duplicateDetails;
        const auto resources = model.resources();
        for (int index = 0; index < resources.size(); ++index) {
            const QString identity = canonicalResourceIdentity(resources.at(index), model.projectPath());
            if (identity.isEmpty()) continue;
            if (identities.contains(identity)) {
                duplicate = true;
                duplicateDetails = QStringLiteral("Duplicate canonical resource identity: %1").arg(identity);
                break;
            }
            identities.insert(identity, index);
        }
        addCheck(result, QStringLiteral("resource-identities"), QStringLiteral("Unique resource identities"),
                 duplicate ? VerificationStatus::Fail : VerificationStatus::Pass,
                 duplicate ? duplicateDetails : QStringLiteral("All configured resources have unique canonical identities."));
    }

    const QStringList jsonFiles{AramfPaths::ProjectConfiguration, AramfPaths::WorkerManifest, AramfPaths::TaskRoutes, AramfPaths::ScopeRoutes,
        QStringLiteral("ARAMF_WORKER/platforms/platform-metadata.json"), AramfPaths::ResourceManifest,
        QStringLiteral("ARAMF_WORKER/communication/communication-contract.json"), QStringLiteral("ARAMF_WORKER/communication/multi-target-build.json"),
        AramfPaths::MemoryConfiguration, AramfPaths::FrameworkKnowledge, AramfPaths::Provenance, AramfPaths::SelectionEffects};
    for (const auto& relative : jsonFiles) {
        const bool expected = (relative == AramfPaths::ProjectConfiguration || relative == AramfPaths::WorkerManifest) ? true
            : (relative == AramfPaths::TaskRoutes || relative == AramfPaths::ScopeRoutes) ? expectedOptions.generateRouting
            : relative == QStringLiteral("ARAMF_WORKER/platforms/platform-metadata.json") ? expectedOptions.generatePlatforms
            : relative == AramfPaths::ResourceManifest ? expectedOptions.generateResources
            : (relative == QStringLiteral("ARAMF_WORKER/communication/communication-contract.json")
               || relative == QStringLiteral("ARAMF_WORKER/communication/multi-target-build.json"))
                ? (model.communicationConfiguration().enabled && expectedOptions.generatePlatforms)
            : (relative == AramfPaths::MemoryConfiguration || relative == AramfPaths::FrameworkKnowledge) ? expectedOptions.generateMemory
            : expectedOptions.generateProvenance;
        if (!expected) continue;
        QJsonObject object; QString error;
        addCheck(result, QStringLiteral("json-%1").arg(QFileInfo(relative).fileName()),
                 QStringLiteral("Valid JSON: %1").arg(relative),
                 readJson(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(relative)), &object, &error) ? VerificationStatus::Pass : VerificationStatus::Fail,
                 error.isEmpty() ? QStringLiteral("Parsed successfully.") : error);
    }

    if (expectedOptions.generateMemory) {
        ProjectMemory memory;
        QString error;
        const auto report = memory.validate(root, &error, persistEvidence);
        addCheck(result, QStringLiteral("memory-consistency"), QStringLiteral("Memory consistency"),
                 report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS") ? VerificationStatus::Pass : VerificationStatus::Fail,
                 error.isEmpty() ? report.value(QStringLiteral("status")).toString() : error);
        QJsonObject cold;
        const bool coldPass = readJson(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ColdStartValidation)), &cold, &error)
            && cold.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
        addCheck(result, QStringLiteral("cold-start"), QStringLiteral("Cold-start validation"),
                 coldPass ? VerificationStatus::Pass : VerificationStatus::Fail,
                 coldPass ? QStringLiteral("PASS") : (error.isEmpty() ? QStringLiteral("Cold-start validation failed.") : error));
    }

    QJsonObject state; QString stateError;
    const bool stateReadable = readJson(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/verification/generation-state.json"))), &state, &stateError);
    const bool current = stateReadable && state.value(QStringLiteral("fingerprint")).toString() == result.fingerprint;
    addCheck(result, QStringLiteral("freshness"), QStringLiteral("Generated configuration is current"),
             current ? VerificationStatus::Pass : VerificationStatus::Warning,
             current ? QStringLiteral("Fingerprint matches ProjectModel.") : QStringLiteral("Generated output is stale; run Generate again."));
    const QString latestPath = QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::LatestValidation));
    if (QFileInfo::exists(latestPath)) {
        QJsonObject priorSummary;
        QString summaryError;
        const bool summaryReadable = readJson(latestPath, &priorSummary, &summaryError);
        const bool summarySchemaValid = summaryReadable && priorSummary.value(QStringLiteral("schemaVersion")).toInt(-1) == 1;
        const bool summaryCurrent = summarySchemaValid && priorSummary.value(QStringLiteral("fingerprint")).toString() == result.fingerprint;
        addCheck(result, QStringLiteral("validation-summary-integrity"), QStringLiteral("Latest validation summary integrity"),
                 !summaryReadable || !summarySchemaValid ? VerificationStatus::Fail : VerificationStatus::Pass,
                 !summaryReadable ? summaryError : (!summarySchemaValid ? QStringLiteral("latest-validation.json has an unsupported schema.") : (!summaryCurrent ? QStringLiteral("latest-validation.json is stale and will be regenerated.") : QStringLiteral("Latest validation summary is structurally valid."))));
    }

    bool hasFail = false; bool hasWarning = false;
    for (const auto& check : result.checks) {
        hasFail |= check.status == VerificationStatus::Fail;
        hasWarning |= check.status == VerificationStatus::Warning;
    }
    result.overallStatus = hasFail ? VerificationStatus::Fail : hasWarning ? VerificationStatus::Warning : VerificationStatus::Pass;
    QJsonArray checks;
    for (const auto& check : result.checks) checks.append(QJsonObject{{QStringLiteral("id"), check.id}, {QStringLiteral("name"), check.name}, {QStringLiteral("status"), statusName(check.status)}, {QStringLiteral("details"), check.details}});
    result.evidence = QJsonObject{{QStringLiteral("fingerprint"), result.fingerprint}, {QStringLiteral("projectRoot"), root}, {QStringLiteral("overallStatus"), statusName(result.overallStatus)}, {QStringLiteral("checks"), checks}};
    if (persistEvidence) {
        auto evidence = result.evidence;
        evidence.insert(QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        writeJsonFile(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/verification/verification-result.json"))), evidence, nullptr);
    }
    const auto checkPassed = [&result](const QString& id) {
        for (const auto& check : result.checks) if (check.id == id) return check.status == VerificationStatus::Pass;
        return false;
    };
    int staleCount = 0;
    for (const auto& check : result.checks) if (check.id.contains(QStringLiteral("stale")) && check.status != VerificationStatus::Pass) ++staleCount;
    QJsonObject summary{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("fingerprint"), result.fingerprint},
        {QStringLiteral("overallStatus"), statusName(result.overallStatus)}, {QStringLiteral("workerSchemaValid"), supportedTopology},
        {QStringLiteral("topologyValid"), supportedTopology && checkPassed(QStringLiteral("parallel-state"))}, {QStringLiteral("routingValid"), checkPassed(QStringLiteral("task-routes")) && checkPassed(QStringLiteral("scope-routes"))},
        {QStringLiteral("coldStartValid"), checkPassed(QStringLiteral("cold-start"))}, {QStringLiteral("memoryConsistent"), checkPassed(QStringLiteral("memory-consistency"))},
        {QStringLiteral("instructionsReachable"), checkPassed(QStringLiteral("aramf-worker-agents"))}, {QStringLiteral("resourcesReachable"), checkPassed(QStringLiteral("resources"))},
        {QStringLiteral("documentationRoutingValid"), !model.academicConfiguration().enabled || checkPassed(QStringLiteral("project-status"))}, {QStringLiteral("staleArtifactCount"), staleCount},
        {QStringLiteral("routingConflictCount"), 0}, {QStringLiteral("checks"), checks}};
    result.summary = summary;
    if (persistEvidence) writeJsonFile(QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::LatestValidation)), summary, nullptr);
    return result;
}

GenerationResult GenerationServices::repairDerivedArtifacts(const ProjectModel& model,
                                                              const GenerationOptions& options) const
{
    WorkerNameScope workerNameScope(model.workerNameSuffix());
    GenerationResult result;
    result.fingerprint = projectConfigurationFingerprint(model, options);
    const QString projectRoot = QDir::cleanPath(model.projectPath().trimmed());
    if (projectRoot.isEmpty() || projectRoot == QStringLiteral(".")) {
        result.error = QStringLiteral("Repair stopped: choose a project path first.");
        return result;
    }
    const QString workerRoot = QDir(projectRoot).filePath(AramfPaths::runtimeWorkerDirectoryName());
    if (!QDir(workerRoot).exists()) {
        result.error = QStringLiteral("Repair stopped: ARAMF_WORKER does not exist.");
        return result;
    }
    QString error;
    if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ProjectConfiguration)), projectConfiguration(model, result.fingerprint), &error)) {
        result.error = QStringLiteral("Repair failed in project configuration: %1").arg(error);
        return result;
    }
    addGeneratedFiles(result, {AramfPaths::ProjectConfiguration});
    if (options.generateRouting) {
        const auto rules = model.ruleConfiguration();
        const auto taskRoutes = WorkerContextResolver::taskRoutes(rules, result.fingerprint);
        const QJsonObject scopeRoutes = WorkerContextResolver::scopeRoutes(rules, result.fingerprint);
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::TaskRoutes)), taskRoutes, &error)
            || !writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ScopeRoutes)), scopeRoutes, &error)) {
            result.error = QStringLiteral("Repair failed in routing: %1").arg(error);
            return result;
        }
        addGeneratedFiles(result, {AramfPaths::TaskRoutes, AramfPaths::ScopeRoutes});
    }
    if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::WorkerManifest)), workerManifest(model, options, result.fingerprint), &error)) {
        result.error = QStringLiteral("Repair failed in Worker manifest: %1").arg(error);
        return result;
    }
    addGeneratedFiles(result, {AramfPaths::WorkerManifest});
    const auto verification = VerificationServices().verify(model, options);
    bool nonSummaryFailure = false;
    for (const auto& check : verification.checks) {
        if (check.status == VerificationStatus::Fail && check.id != QStringLiteral("validation-summary-integrity")) nonSummaryFailure = true;
    }
    if (verification.overallStatus == VerificationStatus::Fail && nonSummaryFailure) {
        result.error = QStringLiteral("Repair completed derived files, but validation remains failed.");
        return result;
    }
    addGeneratedFiles(result, {AramfPaths::LatestValidation});
    result.success = true;
    return result;
}

FinalizationServices::FinalizationServices(QObject* parent)
    : QObject(parent)
{
}

FinalizationResult FinalizationServices::finalize(const ProjectModel& model,
                                                  const GenerationOptions& expectedOptions) const
{
    AramfPaths::setRuntimeWorkerNameSuffix(model.workerNameSuffix());
    FinalizationResult result;
    result.fingerprint = projectConfigurationFingerprint(model, expectedOptions);
    const QString root = QDir::cleanPath(model.projectPath());
    if (root.isEmpty() || root == QStringLiteral(".")) {
        result.blockers << QStringLiteral("Project Path is not configured.");
        return result;
    }
    QJsonObject verification;
    QString error;
    const QString verificationPath = QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/verification/verification-result.json")));
    if (!readJson(verificationPath, &verification, &error)) {
        result.blockers << QStringLiteral("Run Verify before finalizing.");
        return result;
    }
    if (verification.value(QStringLiteral("projectRoot")).toString().trimmed().isEmpty()
        || QDir::cleanPath(verification.value(QStringLiteral("projectRoot")).toString())
               .compare(QDir::cleanPath(root), Qt::CaseInsensitive) != 0) {
        result.blockers << QStringLiteral("Verification belongs to another project root. Run Verify again.");
        return result;
    }
    if (verification.value(QStringLiteral("overallStatus")).toString() != QStringLiteral("PASS")
        || verification.value(QStringLiteral("fingerprint")).toString() != result.fingerprint) {
        result.blockers << QStringLiteral("Verification is missing, failed, or stale. Run Verify again.");
        return result;
    }
    QJsonObject existing;
    const QString finalizationPath = QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/verification/finalization-state.json")));
    if (readJson(finalizationPath, &existing, nullptr)
        && existing.value(QStringLiteral("fingerprint")).toString() == result.fingerprint) {
        result.success = true;
        result.alreadyFinalized = true;
        return result;
    }
    ProjectMemory memory;
    if (expectedOptions.generateMemory
        && memory.validate(root, &error).value(QStringLiteral("status")).toString() != QStringLiteral("PASS")) {
        result.blockers << QStringLiteral("Memory consistency validation did not pass.");
        return result;
    }
    const QJsonObject eventFields{{QStringLiteral("projectId"), model.projectId()}, {QStringLiteral("fingerprint"), result.fingerprint}, {QStringLiteral("verificationStatus"), QStringLiteral("PASS")}};
    if (expectedOptions.generateMemory
        && !memory.appendEvent(root, QStringLiteral("PROJECT_FINALIZED"), QStringLiteral("Project lifecycle finalized"), eventFields, &error)) {
        result.error = error;
        return result;
    }
    if (!writeJsonFile(finalizationPath, QJsonObject{{QStringLiteral("fingerprint"), result.fingerprint}, {QStringLiteral("projectId"), model.projectId()}, {QStringLiteral("finalizedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}}, &error)) {
        result.error = error;
        return result;
    }
    const QString statusPath = QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::ProjectStatus));
    QFile statusFile(statusPath);
    QString status;
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) status = statusFile.readAll();
    statusFile.close();
    if (!status.contains(QStringLiteral("## ARAMF Lifecycle"))) status += QStringLiteral("\n## ARAMF Lifecycle\n\nCurrent state: Finalized\n");
    else {
        const int stateStart = status.indexOf(QStringLiteral("Current state:"));
        if (stateStart >= 0) {
            const int lineEnd = status.indexOf(QLatin1Char('\n'), stateStart);
            status.replace(stateStart, lineEnd < 0 ? status.size() - stateStart : lineEnd - stateStart,
                          QStringLiteral("Current state: Finalized"));
        }
    }
    if (!writeTextFile(statusPath, status.toUtf8(), &error)) { result.error = error; return result; }
    if (expectedOptions.generateMemory && !memory.refreshDerivedState(root, &error)) {
        result.error = error;
        return result;
    }

    result.success = true;
    return result;
}

AgentEntryPointService::AgentEntryPointService(QObject* parent)
    : QObject(parent)
{
}

QList<AgentEntryPointDefinition> AgentEntryPointService::definitions() const
{
    return {
        {QStringLiteral("claude-code"), QStringLiteral("Claude Code"),
         QStringLiteral("aramf_setup/bootstrap/claude/CLAUDE.md"), QStringLiteral("CLAUDE.md"), false},
        {QStringLiteral("github-copilot"), QStringLiteral("GitHub Copilot"),
         QStringLiteral("aramf_setup/bootstrap/github-copilot/copilot-instructions.md"),
         QStringLiteral(".github/copilot-instructions.md"), false},
        {QStringLiteral("gemini"), QStringLiteral("Gemini"),
         QStringLiteral("aramf_setup/bootstrap/gemini/GEMINI.md"), QStringLiteral("GEMINI.md"), false},
        {QStringLiteral("gemini-cli"), QStringLiteral("Gemini CLI"),
         QStringLiteral("aramf_setup/bootstrap/gemini/GEMINI.md"), QStringLiteral("GEMINI.md"), false}
    };
}

AgentEntryPointResult AgentEntryPointService::createEntryPoints(const ProjectModel& model) const
{
    AgentEntryPointResult result;
    const QString projectRoot = QDir::cleanPath(model.projectPath().trimmed());
    if (projectRoot.isEmpty() || projectRoot == QStringLiteral(".") || !QDir(projectRoot).exists()) {
        result.errors << QStringLiteral("Cannot create AI agent entry points: choose a valid Project Path first.");
        return result;
    }

    QString state;
    QString error;
    const QString rootBootstrapPath = QDir(projectRoot).filePath(QStringLiteral("AGENTS.md"));
    const QString workerName = AramfPaths::workerDirectoryName(model.workerNameSuffix());
    if (!writeManagedFile(rootBootstrapPath, managedBootstrapBlock(workerName), &state, &error)) {
        result.errors << error;
        return result;
    }
    if (state == QStringLiteral("created")) result.createdFiles << QStringLiteral("AGENTS.md");
    else if (state == QStringLiteral("updated")) result.updatedFiles << QStringLiteral("AGENTS.md");
    else result.unchangedFiles << QStringLiteral("AGENTS.md");

    QStringList selected;
    const auto ai = model.aiConfiguration();
    if (!ai.primaryAgent.isEmpty() && ai.primaryAgent != QStringLiteral("none")) selected << ai.primaryAgent;
    for (const auto& agent : ai.additionalAgents) {
        if (!agent.isEmpty() && agent != QStringLiteral("none") && !selected.contains(agent)) selected << agent;
    }

    const auto knownDefinitions = definitions();
    for (const auto& agentId : selected) {
        auto definition = std::find_if(knownDefinitions.cbegin(), knownDefinitions.cend(),
                                       [&](const auto& value) { return value.agentId == agentId; });
        if (definition == knownDefinitions.cend()) {
            result.genericAgents << QStringLiteral("%1 uses generic AGENTS.md").arg(selectedAgentDisplayName(agentId));
            continue;
        }
        const QString cleanTarget = QDir::cleanPath(definition->targetPath);
        if (cleanTarget == QStringLiteral(".") || cleanTarget == QStringLiteral("..")
            || cleanTarget.startsWith(QStringLiteral("../")) || QDir::isAbsolutePath(cleanTarget)) {
            result.conflicts << QStringLiteral("%1: unsafe target path %2").arg(definition->displayName, definition->targetPath);
            continue;
        }
        const QString targetPath = QDir(projectRoot).filePath(cleanTarget);
        const QString relative = QDir(projectRoot).relativeFilePath(targetPath);
        if (relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../"))) {
            result.conflicts << QStringLiteral("%1: target escapes Project Path").arg(definition->displayName);
            continue;
        }
        if (!writeManagedFile(targetPath, managedBootstrapBlock(workerName), &state, &error)) {
            result.conflicts << QStringLiteral("%1: %2").arg(definition->displayName, error);
            continue;
        }
        if (state == QStringLiteral("created")) result.createdFiles << definition->targetPath;
        else if (state == QStringLiteral("updated")) result.updatedFiles << definition->targetPath;
        else result.unchangedFiles << definition->targetPath;
    }
    result.success = result.errors.isEmpty() && result.conflicts.isEmpty();
    return result;
}
