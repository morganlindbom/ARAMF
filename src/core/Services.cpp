// Services.cpp

#include "Services.h"

#include "AramfPaths.h"
#include "ControlPlaneMigration.h"
#include "AiCatalog.h"
#include "ProjectMemory.h"
#include "RuleCatalog.h"
#include "ValidationRouting.h"

#include <QDir>
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
QJsonArray toJsonArray(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

bool writeTextFile(const QString& path, const QByteArray& data, QString* error, bool onlyIfMissing = false)
{
    if (onlyIfMissing && QFile::exists(path)) return true;
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

void addGeneratedFiles(GenerationResult& result, const QStringList& files)
{
    result.generatedFiles.append(files);
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

QString managedBootstrapBlock()
{
    return QStringLiteral("<!-- ARAMF-BEGIN -->\n"
                          "This project is managed by ARAMF.\n\n"
                          "Read and follow:\n\n"
                          "ARAMF_WORKER/AGENTS.md\n\n"
                          "ARAMF_WORKER contains the canonical project rules, routing, resources, memory and project status.\n"
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
        {QStringLiteral("aiAgent"), ai.primaryAgent},
        {QStringLiteral("aiResponsibilities"), toJsonArray(ai.responsibilities)},
        {QStringLiteral("aiPermissions"), toJsonArray(ai.permissions)},
        {QStringLiteral("aiIntegrations"), toJsonArray(ai.aramfIntegrations)},
        {QStringLiteral("resources"), model.resources().size()},
        {QStringLiteral("resourceIds"), [&] { QJsonArray a; for (const auto& r : model.resources()) a.append(r.id); return a; }()},
        {QStringLiteral("rules"), toJsonArray(rules.activeCategories)},
        {QStringLiteral("ruleEnforcement"), rules.enforcementLevel},
        {QStringLiteral("memoryMaximum"), QString::number(memory.maximumSizeBytes)},
        {QStringLiteral("agentRules"), options.generateAgentRules},
        {QStringLiteral("routing"), options.generateRouting},
        {QStringLiteral("platformOutput"), options.generatePlatforms},
        {QStringLiteral("resourceOutput"), options.generateResources},
        {QStringLiteral("memoryOutput"), options.generateMemory},
        {QStringLiteral("provenance"), options.generateProvenance}};
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(value).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}

TemplateManager::TemplateManager(QObject* parent)
    : QObject(parent)
{
    /**Construct the template catalog service.

    Templates describe target projects; they do not change the fact that ARAMF itself is implemented entirely in C++.
    */
}

QStringList TemplateManager::builtInTemplates() const
{
    /**Return the protected built-in template order.

    Pico 2 W Visual Designer remains template number one while cross-language target templates stay available.
    */
    return {
        QStringLiteral("pico-2w-visual-designer"),
        QStringLiteral("qt-desktop-application"),
        QStringLiteral("cpp-command-line"),
        QStringLiteral("cmake-library"),
        QStringLiteral("raspberry-pi-pico-firmware"),
        QStringLiteral("react-frontend"),
        QStringLiteral("python-backend"),
        QStringLiteral("csharp-backend"),
        QStringLiteral("mobile-application"),
        QStringLiteral("full-stack-web-application"),
        QStringLiteral("bachelor-thesis")
    };
}

QList<TemplateDefinition> TemplateManager::definitions() const
{
    auto make = [](const QString& id, const QString& name, const QString& type, const QString& language, const QString& framework, const QString& compiler, const QString& target, const QString& build, const QStringList& supported, const QStringList& resources) {
        TemplateDefinition d; d.id=id; d.displayName=name; d.projectType=type; d.environment.language=language; d.environment.framework=framework; d.environment.ide=QStringLiteral("visual-studio-code"); d.environment.compiler=compiler; d.environment.operatingSystem=QStringLiteral("windows"); d.environment.targetPlatform=target; d.environment.targetArchitecture=target == QStringLiteral("embedded") ? QStringLiteral("cortex-m") : QStringLiteral("x86_64"); d.environment.buildSystem=build; d.environment.packageManager=QStringLiteral("none"); d.environment.versionControl=QStringLiteral("git"); d.supportedCapabilities=supported; d.recommendedResources=resources; d.recommendedAiConfiguration={QStringLiteral("codex"), QStringLiteral("planning"), QStringLiteral("coding"), QStringLiteral("review"), QStringLiteral("testing"), QStringLiteral("documentation")}; d.recommendedRules={QStringLiteral("Universal safety"), QStringLiteral("Project architecture")}; d.capabilities.languages={language}; d.capabilities.frameworks={framework}; d.capabilities.ides={d.environment.ide}; d.capabilities.versionControlSystems={QStringLiteral("git")}; d.capabilities.hostOperatingSystems={d.environment.operatingSystem}; d.capabilities.targetPlatforms={target}; d.capabilities.targetArchitectures={d.environment.targetArchitecture}; d.capabilities.toolchains={compiler}; d.capabilities.buildSystems={build}; d.capabilities.buildConfigurations={QStringLiteral("debug"), QStringLiteral("release")}; if (id == QStringLiteral("pico-2w-visual-designer")) { d.capabilities.languages={QStringLiteral("cpp"), QStringLiteral("c"), QStringLiteral("pio-assembly")}; d.capabilities.frameworks={QStringLiteral("qt"), QStringLiteral("pico-sdk")}; d.capabilities.ides={QStringLiteral("visual-studio-code")}; d.capabilities.versionControlSystems={QStringLiteral("git")}; d.capabilities.developmentTools={QStringLiteral("debugger"), QStringLiteral("hardware-debug-probe")}; d.capabilities.targetPlatforms={QStringLiteral("windows-desktop"), QStringLiteral("microcontroller")}; d.capabilities.targetArchitectures={QStringLiteral("x86_64"), QStringLiteral("rp2350"), QStringLiteral("cortex-m")}; d.capabilities.processorFamilies={QStringLiteral("rp2350")}; d.capabilities.hardwareTargets={QStringLiteral("raspberry-pi-pico-2-w")}; d.capabilities.toolchains={QStringLiteral("msys2-ucrt64-gcc"), QStringLiteral("arm-gnu"), QStringLiteral("pico-sdk-toolchain")}; d.capabilities.buildSystems={QStringLiteral("cmake"), QStringLiteral("ninja")}; d.capabilities.testingCapabilities={QStringLiteral("unit-testing")}; d.capabilities.buildConfigurations={QStringLiteral("debug"), QStringLiteral("release")}; } return d;
    };
    auto result = QList<TemplateDefinition>{
        make("pico-2w-visual-designer", "Pico 2 W Visual Designer", "embedded-firmware", "cpp", "pico-sdk", "msys2-ucrt64-gcc", "embedded", "cmake", {"Networking", "Testing", "Documentation"}, {"Pico 2 W datasheet", "Pico SDK documentation"}),
        make("qt-desktop-application", "Qt Desktop Application", "desktop-application", "cpp", "qt6", "msys2-ucrt64-gcc", "desktop", "cmake", {"SQLite", "Networking", "Testing", "Documentation"}, {"Qt documentation", "Architecture document"}),
        make("cpp-command-line", "C++ Command Line", "software-development", "cpp", "none", "msys2-ucrt64-gcc", "desktop", "cmake", {"Testing", "Documentation"}, {"Specification"}),
        make("cmake-library", "CMake Library", "reusable-library", "cpp", "none", "msys2-ucrt64-gcc", "desktop", "cmake", {"Testing", "Documentation"}, {"API specification"}),
        make("raspberry-pi-pico-firmware", "Raspberry Pi Pico Firmware", "embedded-firmware", "cpp", "pico-sdk", "msys2-ucrt64-gcc", "embedded", "cmake", {"Testing", "Documentation"}, {"Pico SDK documentation"}),
        make("react-frontend", "React Frontend", "web-application", "typescript", "react", "node", "web", "npm", {"Testing", "Documentation"}, {"Frontend specification"}),
        make("python-backend", "Python Backend", "backend-service", "python", "fastapi", "python", "server", "pyproject", {"SQLite", "Testing", "Documentation"}, {"API specification"}),
        make("csharp-backend", "C# Backend", "backend-service", "csharp", "aspnet", "dotnet", "server", "cmake", {"Testing", "Documentation"}, {"API specification"}),
        make("mobile-application", "Mobile Application", "software-development", "cpp", "qt6", "msvc", "desktop", "cmake", {"Testing", "Documentation"}, {"Mobile specification"}),
        make("full-stack-web-application", "Full Stack Web Application", "web-application", "typescript", "react", "node", "web", "npm", {"SQLite", "Networking", "Testing", "Documentation"}, {"Architecture document", "API specification"}),
        make("bachelor-thesis", "Bachelor Thesis", "thesis", "cpp", "none", "msys2-ucrt64-gcc", "desktop", "cmake", {"Documentation"}, {"Thesis specification", "Reference implementations"})
    };
    auto& pico = result.first();
    pico.ai.primaryAgent = QStringLiteral("openai-codex");
    pico.ai.additionalAgents = {QStringLiteral("chatgpt")};
    pico.ai.responsibilities = {
        QStringLiteral("planning"), QStringLiteral("architecture"),
        QStringLiteral("coding"), QStringLiteral("testing"),
        QStringLiteral("documentation")
    };
    pico.ai.aramfIntegrations = {
        QStringLiteral("agents-md"), QStringLiteral("rules"),
        QStringLiteral("routing"), QStringLiteral("project-memory"),
        QStringLiteral("project-status")
    };
    auto& bachelor = result.last();
    bachelor.academic.academicMode = QStringLiteral("thesis");
    bachelor.academic.thesisLevel = QStringLiteral("bachelor");
    bachelor.academic.thesisApproaches = {QStringLiteral("software-system-development")};
    bachelor.academic.academicRequirements = {
        QStringLiteral("source-citations"),
        QStringLiteral("reference-list"),
        QStringLiteral("methodology-section"),
        QStringLiteral("research-questions"),
        QStringLiteral("academic-formatting")
    };
    bachelor.academic.academicDeliverables = {
        QStringLiteral("written-thesis"),
        QStringLiteral("source-code")
    };
    return result;
}

TemplateDefinition TemplateManager::definition(const QString& id) const { for (const auto& definition : definitions()) if (definition.id == id) return definition; return {}; }

bool TemplateManager::applyTemplate(ProjectModel* model, const QString& id) const
{
    /**Apply one known built-in template to the project model.

    Template application changes project configuration only and is grouped as one model update.
    */
    if (!model || !builtInTemplates().contains(id)) {
        return false;
    }

    model->beginUpdate();
    model->setTemplateId(id);
    const auto selected = definition(id);
    model->setContext(selected.projectType);
    model->applyTemplateDefaults(selected.environment);
    model->applyTemplateCapabilities(selected.capabilities);
    model->setAcademicConfiguration(selected.academic);
    model->setAiConfiguration(selected.ai);
    model->applyTemplateDefaults(selected.environment);
    model->endUpdate();
    return true;
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
    GenerationResult result;
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
    const auto preparation = prepareControlPlane(projectRoot);
    if (!preparation.success) {
        result.error = QStringLiteral("Generation failed: %1").arg(preparation.error);
        return result;
    }
    result.warnings.append(preparation.warnings);

    auto fail = [&result](const QString& product, const QString& error) {
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
        const QString rootAgent = QStringLiteral("<!-- AGENTS.md -->\n\n") + managedBootstrapBlock();
        QString canonicalAgent = QStringLiteral(
            "<!-- AGENTS.md -->\n\n# Canonical ARAMF Agent Instructions\n\n"
            "Read `PROJECT_STATUS.md` and `memory/decisions.md` before project work.\n");
        if (options.generateMemory) {
            canonicalAgent += QStringLiteral(
                "Read `memory/framework-knowledge.json` and apply only entries whose status is `approved`.\n"
                "Approved Framework Knowledge is live: it applies immediately in this project without regeneration.\n");
        }
        canonicalAgent += QStringLiteral(
            "Read `rules/generated-rules.md` when rule output is present.\n\n"
            "Respect Sources of Truth, durable decisions, and the user-owned `custom/` directory.\n"
            "Authority order: explicit current user instruction, current Source of Truth, current durable project decisions, approved Framework Knowledge, templates/defaults, then AI inference.\n"
            "When a corrected approach is verified and reusable, record a Framework Knowledge candidate with evidence. Never self-approve it; explicit user approval is required before changing its status to `approved`. Superseded entries remain auditable but are not active.\n"
            "Keep project status current and use project memory when configured.\n"
            "The generated control directory is `ARAMF_WORKER/`.\n");
        canonicalAgent += QStringLiteral(
            "Run the minimum validation required by `routing/validation-policy.json`; do not run full regression campaigns for ordinary isolated changes. Escalate when scope, risk, failure, or explicit milestone policy requires it.\n");
        if (options.generateMemory) {
            const auto memory = model.memoryConfiguration();
            QString memorySection = QStringLiteral(
                "<!-- ARAMF-MEMORY-BEGIN -->\n"
                "\n## Project Memory Feedback\n\n"
                "Read `memory/memory-contract.json` before recording development results. "
                "Do not edit `memory/event-log.jsonl`, `memory/metrics.json`, `memory/current-state.md`, "
                "`memory/memory-manifest.json`, validation state, or `PROJECT_STATUS.md` bookkeeping fields directly. "
                "Use the ARAMF recorder described by the contract: `aramf memory record --project <project-root> "
                "--operation <operation> ...`.\n");
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
            memorySection += QStringLiteral("- Record durable decisions only for genuine architecture or policy choices through the decision workflow.\n");
            memorySection += QStringLiteral("- Follow current durable decisions; explicitly superseded decisions remain historical and inactive.\n");
            memorySection += QStringLiteral("\nThe recorder owns event IDs, timestamps, sequences, metrics, pruning, validation, and current-state pointers.\n\n<!-- ARAMF-MEMORY-END -->\n");
            canonicalAgent += memorySection;
        }
        if (!writeTextFile(QDir(projectRoot).filePath(QStringLiteral("AGENTS.md")), rootAgent.toUtf8(), &error, true)) {
            return fail(QStringLiteral("Agent rules"), error);
        }
        const QString agentInstructionsPath = QDir(projectRoot).filePath(AramfPaths::AgentInstructions);
        if (options.generateMemory) {
            const int memoryBegin = canonicalAgent.indexOf(QStringLiteral("<!-- ARAMF-MEMORY-BEGIN -->"));
            const QString memorySection = memoryBegin >= 0 ? canonicalAgent.mid(memoryBegin) : QString();
            if (!QFile::exists(agentInstructionsPath)) {
                if (!writeTextFile(agentInstructionsPath, canonicalAgent.toUtf8(), &error)) return fail(QStringLiteral("Agent rules"), error);
            } else if (!upsertManagedSection(agentInstructionsPath,
                                             QStringLiteral("<!-- ARAMF-MEMORY-BEGIN -->"),
                                             QStringLiteral("<!-- ARAMF-MEMORY-END -->"),
                                             memorySection, &error)) {
                return fail(QStringLiteral("Agent rules"), error);
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
        if (!writeTextFile(QDir(projectRoot).filePath(AramfPaths::GeneratedRules), markdown.toUtf8(), &error)) {
            return fail(QStringLiteral("Agent rules"), error);
        }

        const QString status = QStringLiteral(
            "<!-- PROJECT_STATUS.md -->\n\n# Project Status\n\n## Project\n\n"
            "- Name: %1\n- Project ID: %2\n- ARAMF state: Initialized\n\n"
            "## Implemented\n\n- ARAMF control-plane structure generated.\n\n"
            "## Verified\n\n- Generation completed for the selected output products.\n")
                                   .arg(model.projectName(), model.projectId());
        if (!writeTextFile(QDir(projectRoot).filePath(AramfPaths::ProjectStatus), status.toUtf8(), &error, true)) {
            return fail(QStringLiteral("Agent rules"), error);
        }
        addGeneratedFiles(result, {QStringLiteral("AGENTS.md"), AramfPaths::AgentInstructions,
                                   AramfPaths::ProjectStatus, AramfPaths::GeneratedRules});
    }

    if (options.generateRouting) {
        const auto rules = model.ruleConfiguration();
        const QJsonObject taskRoutes{
            {QStringLiteral("strategy"), rules.loadingStrategy},
            {QStringLiteral("workTypes"), toJsonArray(rules.workScopes)},
            {QStringLiteral("contextPolicies"), toJsonArray(rules.contextPolicies)},
            {QStringLiteral("conflictPolicy"), rules.conflictPolicy}
        };
        const QJsonObject scopeRoutes{{QStringLiteral("scopes"), toJsonArray(rules.projectScopes)}};
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::TaskRoutes), taskRoutes, &error)
            || !writeJsonFile(QDir(projectRoot).filePath(AramfPaths::ScopeRoutes), scopeRoutes, &error)
            || !writeJsonFile(QDir(projectRoot).filePath(AramfPaths::ValidationPolicy), ValidationRouting::policy(), &error)) {
            return fail(QStringLiteral("Routing"), error);
        }
        const QString readme = QStringLiteral(
            "# Routing\n\n"
            "Strategy: %1\n\n"
            "Task routing selects relevant work types: %2\n\n"
            "Scope routing selects relevant project areas: %3\n\n"
            "Routing minimizes irrelevant AI context by loading only applicable rules and scopes.\n")
                                  .arg(rules.loadingStrategy, rules.workScopes.join(QStringLiteral(", ")), rules.projectScopes.join(QStringLiteral(", ")));
        if (!writeTextFile(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/README.md")), readme.toUtf8(), &error)) {
            return fail(QStringLiteral("Routing"), error);
        }
        addGeneratedFiles(result, {AramfPaths::TaskRoutes, AramfPaths::ScopeRoutes, AramfPaths::ValidationPolicy, QStringLiteral("ARAMF_WORKER/routing/README.md")});
    }

    if (options.generatePlatforms) {
        const auto environment = model.developmentEnvironment();
        const auto capabilities = model.developmentCapabilities();
        const QJsonObject platform{
            {QStringLiteral("projectType"), projectTypeLabel(model)},
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
        const QString platformPath = QStringLiteral("ARAMF_WORKER/platforms/platform-metadata.json");
        if (!writeJsonFile(QDir(projectRoot).filePath(platformPath), platform, &error)) return fail(QStringLiteral("Platform metadata"), error);
        addGeneratedFiles(result, {platformPath});
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
                {QStringLiteral("locationMode"), resource.locationMode}, {QStringLiteral("authority"), resource.authorityLevel},
                {QStringLiteral("scopes"), toJsonArray(resource.scopes)}, {QStringLiteral("status"), resource.status},
                {QStringLiteral("loadingPolicyOverride"), resource.loadingStrategyOverride}
            });
        }
        const QJsonObject manifest{{QStringLiteral("resources"), resources},
                                   {QStringLiteral("loadingStrategy"), model.resourcePolicy().loadingStrategy}};
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::ResourceManifest), manifest, &error)) return fail(QStringLiteral("Resource manifest"), error);
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

    if (options.generateProvenance) {
        const auto capabilities = model.developmentCapabilities();
        const auto ai = model.aiConfiguration();
        const QJsonObject provenance{
            {QStringLiteral("status"), QStringLiteral("managed")}, {QStringLiteral("implementation"), QStringLiteral("C++")},
            {QStringLiteral("projectId"), model.projectId()}, {QStringLiteral("projectName"), model.projectName()},
            {QStringLiteral("template"), model.templateId()}, {QStringLiteral("projectType"), projectTypeLabel(model)}};
        const QJsonObject effects{
            {QStringLiteral("template"), model.templateId()}, {QStringLiteral("languages"), toJsonArray(capabilities.languages)},
            {QStringLiteral("frameworks"), toJsonArray(capabilities.frameworks)}, {QStringLiteral("platforms"), toJsonArray(capabilities.targetPlatforms)},
            {QStringLiteral("hardware"), toJsonArray(capabilities.hardwareTargets)}, {QStringLiteral("primaryAiAgent"), ai.primaryAgent},
            {QStringLiteral("resources"), model.resources().size()}, {QStringLiteral("rules"), toJsonArray(model.ruleConfiguration().activeCategories)},
            {QStringLiteral("memoryMaximumSizeBytes"), model.memoryConfiguration().maximumSizeBytes}};
        if (!writeJsonFile(QDir(projectRoot).filePath(AramfPaths::Provenance), provenance, &error)
            || !writeJsonFile(QDir(projectRoot).filePath(AramfPaths::SelectionEffects), effects, &error)) {
            return fail(QStringLiteral("Provenance"), error);
        }
        addGeneratedFiles(result, {AramfPaths::Provenance, AramfPaths::SelectionEffects});
    }

    const QJsonObject generationState{
        {QStringLiteral("fingerprint"), result.fingerprint},
        {QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("agentRules"), options.generateAgentRules},
        {QStringLiteral("routing"), options.generateRouting},
        {QStringLiteral("platforms"), options.generatePlatforms},
        {QStringLiteral("resources"), options.generateResources},
        {QStringLiteral("memory"), options.generateMemory},
        {QStringLiteral("provenance"), options.generateProvenance}};
    if (!writeJsonFile(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/verification/generation-state.json")),
                       generationState, &error)) {
        return fail(QStringLiteral("Generation state"), error);
    }
    addGeneratedFiles(result, {QStringLiteral("ARAMF_WORKER/verification/generation-state.json")});

    result.success = true;
    return result;
}

VerificationServices::VerificationServices(QObject* parent)
    : QObject(parent)
{
}

VerificationResult VerificationServices::verify(const ProjectModel& model,
                                                const GenerationOptions& expectedOptions) const
{
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
    const QString worker = QDir(root).filePath(AramfPaths::ControlDirectory);
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
        const QString path = QDir(root).filePath(relative);
        addCheck(result, id, name, readableNonEmpty(path) ? VerificationStatus::Pass : VerificationStatus::Fail,
                 readableNonEmpty(path) ? relative : QStringLiteral("Missing or empty: %1").arg(relative));
    };
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

    const QStringList jsonFiles{AramfPaths::TaskRoutes, AramfPaths::ScopeRoutes,
        QStringLiteral("ARAMF_WORKER/platforms/platform-metadata.json"), AramfPaths::ResourceManifest,
        AramfPaths::MemoryConfiguration, AramfPaths::FrameworkKnowledge, AramfPaths::Provenance, AramfPaths::SelectionEffects};
    for (const auto& relative : jsonFiles) {
        const bool expected = (relative == AramfPaths::TaskRoutes || relative == AramfPaths::ScopeRoutes) ? expectedOptions.generateRouting
            : relative == QStringLiteral("ARAMF_WORKER/platforms/platform-metadata.json") ? expectedOptions.generatePlatforms
            : relative == AramfPaths::ResourceManifest ? expectedOptions.generateResources
            : (relative == AramfPaths::MemoryConfiguration || relative == AramfPaths::FrameworkKnowledge) ? expectedOptions.generateMemory
            : expectedOptions.generateProvenance;
        if (!expected) continue;
        QJsonObject object; QString error;
        addCheck(result, QStringLiteral("json-%1").arg(QFileInfo(relative).fileName()),
                 QStringLiteral("Valid JSON: %1").arg(relative),
                 readJson(QDir(root).filePath(relative), &object, &error) ? VerificationStatus::Pass : VerificationStatus::Fail,
                 error.isEmpty() ? QStringLiteral("Parsed successfully.") : error);
    }

    if (expectedOptions.generateMemory) {
        ProjectMemory memory;
        QString error;
        const auto report = memory.validate(root, &error);
        addCheck(result, QStringLiteral("memory-consistency"), QStringLiteral("Memory consistency"),
                 report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS") ? VerificationStatus::Pass : VerificationStatus::Fail,
                 error.isEmpty() ? report.value(QStringLiteral("status")).toString() : error);
        QJsonObject cold;
        const bool coldPass = readJson(QDir(root).filePath(AramfPaths::ColdStartValidation), &cold, &error)
            && cold.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
        addCheck(result, QStringLiteral("cold-start"), QStringLiteral("Cold-start validation"),
                 coldPass ? VerificationStatus::Pass : VerificationStatus::Fail,
                 coldPass ? QStringLiteral("PASS") : (error.isEmpty() ? QStringLiteral("Cold-start validation failed.") : error));
    }

    QJsonObject state; QString stateError;
    const bool stateReadable = readJson(QDir(root).filePath(QStringLiteral("ARAMF_WORKER/verification/generation-state.json")), &state, &stateError);
    const bool current = stateReadable && state.value(QStringLiteral("fingerprint")).toString() == result.fingerprint;
    addCheck(result, QStringLiteral("freshness"), QStringLiteral("Generated configuration is current"),
             current ? VerificationStatus::Pass : VerificationStatus::Warning,
             current ? QStringLiteral("Fingerprint matches ProjectModel.") : QStringLiteral("Generated output is stale; run Generate again."));

    bool hasFail = false; bool hasWarning = false;
    for (const auto& check : result.checks) {
        hasFail |= check.status == VerificationStatus::Fail;
        hasWarning |= check.status == VerificationStatus::Warning;
    }
    result.overallStatus = hasFail ? VerificationStatus::Fail : hasWarning ? VerificationStatus::Warning : VerificationStatus::Pass;
    QJsonArray checks;
    for (const auto& check : result.checks) checks.append(QJsonObject{{QStringLiteral("id"), check.id}, {QStringLiteral("name"), check.name}, {QStringLiteral("status"), statusName(check.status)}, {QStringLiteral("details"), check.details}});
    writeJsonFile(QDir(root).filePath(QStringLiteral("ARAMF_WORKER/verification/verification-result.json")),
                  QJsonObject{{QStringLiteral("fingerprint"), result.fingerprint}, {QStringLiteral("overallStatus"), statusName(result.overallStatus)}, {QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {QStringLiteral("checks"), checks}}, nullptr);
    return result;
}

FinalizationServices::FinalizationServices(QObject* parent)
    : QObject(parent)
{
}

FinalizationResult FinalizationServices::finalize(const ProjectModel& model,
                                                  const GenerationOptions& expectedOptions) const
{
    FinalizationResult result;
    result.fingerprint = projectConfigurationFingerprint(model, expectedOptions);
    const QString root = QDir::cleanPath(model.projectPath());
    if (root.isEmpty() || root == QStringLiteral(".")) {
        result.blockers << QStringLiteral("Project Path is not configured.");
        return result;
    }
    QJsonObject verification;
    QString error;
    const QString verificationPath = QDir(root).filePath(QStringLiteral("ARAMF_WORKER/verification/verification-result.json"));
    if (!readJson(verificationPath, &verification, &error)) {
        result.blockers << QStringLiteral("Run Verify before finalizing.");
        return result;
    }
    if (verification.value(QStringLiteral("overallStatus")).toString() != QStringLiteral("PASS")
        || verification.value(QStringLiteral("fingerprint")).toString() != result.fingerprint) {
        result.blockers << QStringLiteral("Verification is missing, failed, or stale. Run Verify again.");
        return result;
    }
    QJsonObject existing;
    const QString finalizationPath = QDir(root).filePath(QStringLiteral("ARAMF_WORKER/verification/finalization-state.json"));
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
    const QString statusPath = QDir(root).filePath(AramfPaths::ProjectStatus);
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
    if (!writeManagedFile(rootBootstrapPath, managedBootstrapBlock(), &state, &error)) {
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
        if (!writeManagedFile(targetPath, managedBootstrapBlock(), &state, &error)) {
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
