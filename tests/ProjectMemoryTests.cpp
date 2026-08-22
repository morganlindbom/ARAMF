// ProjectMemoryTests.cpp

#include "core/ProjectMemory.h"
#include "core/FrameworkKnowledge.h"
#include "core/EnvironmentCatalog.h"
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <iostream>

namespace {
bool require(bool condition, const char* message)
{
    /**Report one test assertion without an external test framework.

    Returning false keeps the test binary dependency-free beyond Qt Core and makes CTest integration straightforward.
    */
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}
}

int main(int argc, char** argv)
{
    /**Exercise the native C++ project-memory lifecycle.

    The test proves initialization, uppercase ARAMF layout, root agent bootstrap, and consistency validation without Python.
    */
    QCoreApplication app(argc, argv);
    QTemporaryDir temporaryProject;
    if (!require(temporaryProject.isValid(), "temporary project directory must be valid")) {
        return 1;
    }

    ProjectModel model;
    model.setProjectName(QStringLiteral("Memory Test Project"));
    model.setProjectPath(temporaryProject.path());

    ProjectMemory memory;
    QString error;
    if (!require(memory.initialize(temporaryProject.path(), &model, &error), qPrintable(error))) {
        return 1;
    }

    const QDir root(temporaryProject.path());
    bool ok = true;
    ok &= require(root.exists(QStringLiteral("ARAMF/AGENTS.md")), "canonical ARAMF/AGENTS.md must exist");
    ok &= require(root.exists(QStringLiteral("ARAMF/PROJECT_STATUS.md")), "ARAMF/PROJECT_STATUS.md must exist");
    ok &= require(root.exists(QStringLiteral("ARAMF/memory/decisions.md")), "durable decisions must live under ARAMF/memory");
    ok &= require(root.exists(QStringLiteral("AGENTS.md")), "root agent bootstrap must exist");
    ok &= require(!root.exists(QStringLiteral("aramf.py")), "no Python backend file may be generated");
    QFile generatedBootstrap(root.filePath(QStringLiteral("AGENTS.md")));
    ok &= require(generatedBootstrap.open(QIODevice::ReadOnly | QIODevice::Text), "generated root bootstrap must be readable");
    const QString bootstrapText = QString::fromUtf8(generatedBootstrap.readAll());
    ok &= require(bootstrapText.contains(QStringLiteral("ARAMF/AGENTS.md")), "generated bootstrap must route to uppercase ARAMF control plane");
    ok &= require(!bootstrapText.contains(QStringLiteral("aramf_setup")), "generated bootstrap must not reference repository setup");
    ok &= require(!root.exists(QStringLiteral("aramf_setup")), "generated project must not contain repository setup directory");
    ok &= require(!root.exists(QStringLiteral("bootstrap")), "generated project must not contain repository bootstrap directory");
    ok &= require(root.exists(QStringLiteral("ARAMF/memory/framework-knowledge.json")), "Framework Knowledge store must exist");
    QFile canonicalAgentFile(root.filePath(QStringLiteral("ARAMF/AGENTS.md")));
    ok &= require(canonicalAgentFile.open(QIODevice::ReadOnly | QIODevice::Text), "canonical agent file must be readable");
    const QString canonicalAgentText = QString::fromUtf8(canonicalAgentFile.readAll());
    ok &= require(canonicalAgentText.contains(QStringLiteral("framework-knowledge.json")), "canonical agent instructions must load live Framework Knowledge");

    FrameworkKnowledgeService frameworkKnowledge;
    const QString candidateId = frameworkKnowledge.propose(
        temporaryProject.path(),
        QStringLiteral("Preserve verified corrections"),
        QStringLiteral("Do not restore a superseded implementation after a verified correction unless newer evidence requires it."),
        {QStringLiteral("implementation"), QStringLiteral("regression")},
        {QStringLiteral("Corrected behavior passed verification.")},
        true, &error);
    ok &= require(!candidateId.isEmpty(), "Framework Knowledge candidate must be created");
    const QString repeatedCandidateId = frameworkKnowledge.propose(
        temporaryProject.path(),
        QStringLiteral("Preserve verified corrections"),
        QStringLiteral("Do not restore a superseded implementation after a verified correction unless newer evidence requires it."),
        {QStringLiteral("implementation"), QStringLiteral("regression")},
        {QStringLiteral("Second evidence item.")},
        true, &error);
    ok &= require(repeatedCandidateId == candidateId, "matching Framework Knowledge proposals must deduplicate");
    ok &= require(frameworkKnowledge.approvedEntries(temporaryProject.path(), {QStringLiteral("implementation")}, &error).isEmpty(),
                  "candidate Framework Knowledge must not be active before approval");
    ok &= require(!frameworkKnowledge.approve(temporaryProject.path(), candidateId, QString(), &error),
                  "Framework Knowledge approval must require an explicit approval source");
    error.clear();
    ok &= require(frameworkKnowledge.approve(temporaryProject.path(), candidateId, QStringLiteral("explicit-user-approval"), &error),
                  "Framework Knowledge candidate must be approvable after explicit user approval");
    const auto activeKnowledge = frameworkKnowledge.approvedEntries(temporaryProject.path(), {QStringLiteral("implementation")}, &error);
    ok &= require(activeKnowledge.size() == 1 && activeKnowledge.first().id == candidateId,
                  "approved Framework Knowledge must become active immediately");
    ok &= require(frameworkKnowledge.supersede(temporaryProject.path(), candidateId, QStringLiteral("replacement-entry"), &error),
                  "Framework Knowledge must support non-destructive superseding");
    ok &= require(frameworkKnowledge.approvedEntries(temporaryProject.path(), {QStringLiteral("implementation")}, &error).isEmpty(),
                  "superseded Framework Knowledge must no longer be active");

    TemplateManager templates;
    ok &= require(templates.builtInTemplates().first() == QStringLiteral("pico-2w-visual-designer"), "Pico visual designer must remain the first template");
    ok &= require(templates.applyTemplate(&model, QStringLiteral("qt-desktop-application")), "Qt template must apply");
    ok &= require(model.developmentEnvironment().framework == QStringLiteral("qt6"), "template must derive framework");
    ok &= require(model.context() == QStringLiteral("desktop-application"), "project context must be derived from capabilities");
    ok &= require(model.academicConfiguration().academicMode == QStringLiteral("disabled"), "ordinary templates must leave Academic disabled");
    auto overridden = model.developmentEnvironment(); overridden.language = QStringLiteral("python"); model.setDevelopmentEnvironment(overridden);
    ok &= require(templates.applyTemplate(&model, QStringLiteral("pico-2w-visual-designer")), "Pico template must apply");
    ok &= require(model.developmentEnvironment().language == QStringLiteral("python"), "compatible user override must survive template change");
    ok &= require(model.developmentEnvironment().framework == QStringLiteral("pico-sdk"), "non-overridden framework must follow template");
    ok &= require(model.context() == QStringLiteral("embedded-firmware"), "embedded project context must be derived from capabilities");
    ok &= require(model.aiConfiguration().primaryAgent == QStringLiteral("openai-codex"), "Pico template must provide AI agent defaults");

    const auto bachelor = templates.definition(QStringLiteral("bachelor-thesis"));
    ok &= require(bachelor.academic.academicMode == QStringLiteral("thesis"), "thesis template must provide Academic defaults");
    ok &= require(bachelor.academic.thesisLevel == QStringLiteral("bachelor"), "thesis template must provide thesis level");

    model.setProjectPath(QStringLiteral("C:/managed-target"));
    model.setDescription(QStringLiteral("Persistence round-trip"));
    model.setAiPlatforms({QStringLiteral("openai-codex")});
    ProjectResource resource;
    resource.id = QStringLiteral("pico-datasheet");
    resource.name = QStringLiteral("Pico Datasheet");
    resource.type = QStringLiteral("datasheet");
    resource.location = QStringLiteral("C:/docs/pico-datasheet.pdf");
    resource.description = QStringLiteral("Authoritative hardware reference");
    resource.authorityLevel = QStringLiteral("primary-source-of-truth");
    resource.scopes = {QStringLiteral("hardware"), QStringLiteral("firmware")};
    resource.status = QStringLiteral("available");
    model.setResources({resource});
    ResourcePolicy resourcePolicy;
    resourcePolicy.options = {QStringLiteral("read-relevant"), QStringLiteral("prefer-authoritative")};
    resourcePolicy.loadingStrategy = QStringLiteral("relevant");
    model.setResourcePolicy(resourcePolicy);
    RuleConfiguration rules;
    rules.activeCategories = {QStringLiteral("coding-standards"), QStringLiteral("build-must-pass")};
    rules.enforcementLevel = QStringLiteral("strict");
    rules.loadingStrategy = QStringLiteral("metadata-first");
    rules.workScopes = {QStringLiteral("coding")};
    rules.projectScopes = {QStringLiteral("source-code")};
    rules.conflictPolicy = QStringLiteral("manual-resolution");
    model.setRuleConfiguration(rules);
    MemoryConfiguration memoryConfiguration;
    memoryConfiguration.captureCategories = {QStringLiteral("durable-decisions"), QStringLiteral("current-project-status")};
    memoryConfiguration.retentionLevel = QStringLiteral("detailed");
    memoryConfiguration.maintenanceOptions = {QStringLiteral("record-decisions")};
    memoryConfiguration.validationOptions = {QStringLiteral("memory-consistency")};
    memoryConfiguration.maximumSizeBytes = 750LL * 1024LL * 1024LL;
    model.setMemoryConfiguration(memoryConfiguration);
    model.setOptionValues(QStringLiteral("rules-routing"), {QStringLiteral("Project architecture")});
    AiConfiguration ai;
    ai.primaryAgent = QStringLiteral("openai-codex");
    ai.additionalAgents = {QStringLiteral("chatgpt"), QStringLiteral("claude")};
    ai.responsibilities = {QStringLiteral("planning"), QStringLiteral("testing")};
    ai.permissions = {QStringLiteral("read-project-files"), QStringLiteral("run-tests")};
    ai.aramfIntegrations = {QStringLiteral("agents-md"), QStringLiteral("project-memory")};
    model.setAiConfiguration(ai);
    AcademicConfiguration academic;
    academic.academicMode = QStringLiteral("thesis");
    academic.thesisLevel = QStringLiteral("master");
    academic.thesisApproaches = {QStringLiteral("software-system-development"), QStringLiteral("case-study")};
    academic.researchMethods = {QStringLiteral("mixed-methods")};
    academic.institution = QStringLiteral("HKR");
    academic.programmeOrCourse = QStringLiteral("Computer Science");
    academic.supervisor = QStringLiteral("Supervisor");
    academic.examiner = QStringLiteral("Examiner");
    academic.citationStyle = QStringLiteral("ieee");
    academic.academicLanguage = QStringLiteral("swedish");
    academic.academicRequirements = {QStringLiteral("source-citations")};
    academic.academicDeliverables = {QStringLiteral("written-thesis"), QStringLiteral("source-code")};
    model.setAcademicConfiguration(academic);
    const QString projectFile = root.filePath(QStringLiteral("project.aramf.json"));
    ProjectPersistence persistence;
    ok &= require(persistence.save(model, projectFile, &error), "project save must succeed");

    ProjectModel loaded;
    ok &= require(persistence.load(&loaded, projectFile, &error), "project open must succeed");
    ok &= require(loaded.projectId() == model.projectId(), "project ID must survive persistence");
    ok &= require(loaded.projectName() == model.projectName(), "project name must survive persistence");
    ok &= require(loaded.projectPath() == model.projectPath(), "managed target path must survive persistence");
    ok &= require(loaded.templateId() == model.templateId(), "template selection must survive persistence");
    ok &= require(loaded.academicConfiguration().academicMode == QStringLiteral("thesis"), "Academic mode must survive persistence");
    ok &= require(loaded.academicConfiguration().thesisLevel == QStringLiteral("master"), "thesis level must survive persistence");
    ok &= require(loaded.academicConfiguration().institution == QStringLiteral("HKR"), "academic information must survive persistence");
    ok &= require(loaded.academicConfiguration().academicDeliverables == academic.academicDeliverables, "academic deliverables must survive persistence");
    ok &= require(loaded.aiConfiguration().primaryAgent == QStringLiteral("openai-codex"), "primary AI agent must survive persistence");
    ok &= require(loaded.aiConfiguration().additionalAgents == ai.additionalAgents, "additional AI agents must survive persistence");
    ok &= require(loaded.aiConfiguration().permissions == ai.permissions, "AI permissions must survive persistence");
    ok &= require(loaded.resourceNames() == model.resourceNames(), "resource names must survive persistence");
    ok &= require(loaded.resources().size() == 1, "structured resources must survive persistence");
    ok &= require(loaded.resources().first().authorityLevel == QStringLiteral("primary-source-of-truth"), "resource authority must survive persistence");
    ok &= require(loaded.resources().first().scopes == resource.scopes, "resource scopes must survive persistence");
    ok &= require(loaded.resourcePolicy().options == resourcePolicy.options, "resource policy options must survive persistence");
    ok &= require(loaded.resourcePolicy().loadingStrategy == QStringLiteral("relevant"), "resource loading strategy must survive persistence");
    ok &= require(loaded.ruleConfiguration().activeCategories == rules.activeCategories, "rule categories must survive persistence");
    ok &= require(loaded.ruleConfiguration().enforcementLevel == QStringLiteral("strict"), "rule enforcement must survive persistence");
    ok &= require(loaded.memoryConfiguration().maximumSizeBytes == memoryConfiguration.maximumSizeBytes, "memory size limit must survive persistence");

    const QString legacyProjectFile = root.filePath(QStringLiteral("legacy-project.aramf.json"));
    QJsonObject legacyRoot;
    legacyRoot.insert(QStringLiteral("projectName"), QStringLiteral("Legacy Resources"));
    legacyRoot.insert(QStringLiteral("resources"), QJsonArray{QStringLiteral("Legacy Datasheet")});
    QFile legacyFile(legacyProjectFile);
    if (legacyFile.open(QIODevice::WriteOnly)) {
        legacyFile.write(QJsonDocument(legacyRoot).toJson(QJsonDocument::Indented));
        legacyFile.close();
    }
    ProjectModel legacyLoaded;
    ok &= require(persistence.load(&legacyLoaded, legacyProjectFile, &error), "legacy project open must succeed");
    ok &= require(legacyLoaded.resources().size() == 1, "legacy resource names must migrate to structured resources");
    ok &= require(legacyLoaded.resources().first().name == QStringLiteral("Legacy Datasheet"), "legacy resource name must be retained");
    ok &= require(loaded.optionValues(QStringLiteral("rules-routing")) == model.optionValues(QStringLiteral("rules-routing")), "rules must survive persistence");
    ok &= require(!loaded.isModified(), "loaded project must start clean");
    ok &= require(!EnvironmentCatalog::ides().isEmpty(), "environment IDE catalog must be available");
    ok &= require(EnvironmentCatalog::toolchains().size() >= 20, "environment toolchain catalog must be broad");
    ok &= require(EnvironmentCatalog::languages().size() >= 18, "language catalog must support multi-language projects");
    ok &= require(EnvironmentCatalog::hardwareTargets().size() >= 10, "hardware catalog must be available");
    auto capabilities = loaded.developmentCapabilities();
    capabilities.languages = {QStringLiteral("cpp"), QStringLiteral("c"), QStringLiteral("custom:zig")};
    capabilities.frameworks = {QStringLiteral("qt"), QStringLiteral("pico-sdk")};
    capabilities.ides = {QStringLiteral("visual-studio-code"), QStringLiteral("clion")};
    capabilities.hostOperatingSystems = {QStringLiteral("windows"), QStringLiteral("linux")};
    capabilities.targetPlatforms = {QStringLiteral("desktop"), QStringLiteral("microcontroller")};
    capabilities.targetArchitectures = {QStringLiteral("x86_64"), QStringLiteral("rp2350")};
    capabilities.hardwareTargets = {QStringLiteral("raspberry-pi-pico-2-w")};
    capabilities.toolchains = {QStringLiteral("gcc"), QStringLiteral("arm-gnu")};
    capabilities.buildSystems = {QStringLiteral("cmake"), QStringLiteral("ninja")};
    capabilities.buildConfigurations = {QStringLiteral("debug"), QStringLiteral("release")};
    capabilities.testingCapabilities = {QStringLiteral("unit-testing")};
    capabilities.qualityCapabilities = {QStringLiteral("static-analysis")};
    loaded.setDevelopmentCapabilities(capabilities);
    auto customEnvironment = loaded.developmentEnvironment();
    customEnvironment.compiler = QStringLiteral("custom:zig-toolchain");
    customEnvironment.targetArchitecture = QStringLiteral("risc-v");
    loaded.setDevelopmentEnvironment(customEnvironment);
    const QString customProjectFile = root.filePath(QStringLiteral("custom-project.aramf.json"));
    ok &= require(persistence.save(loaded, customProjectFile, &error), "custom environment save must succeed");
    ProjectModel customLoaded;
    ok &= require(persistence.load(&customLoaded, customProjectFile, &error), "custom environment open must succeed");
    ok &= require(customLoaded.developmentEnvironment().compiler == QStringLiteral("custom:zig-toolchain"), "custom toolchain must survive persistence");
    ok &= require(customLoaded.developmentEnvironment().targetArchitecture == QStringLiteral("risc-v"), "target architecture must survive persistence");
    ok &= require(customLoaded.developmentCapabilities().buildSystems == capabilities.buildSystems, "multi-select build systems must survive persistence");
    ok &= require(customLoaded.developmentCapabilities().targetArchitectures == capabilities.targetArchitectures, "multi-select architectures must survive persistence");
    ok &= require(!customLoaded.isModified(), "custom loaded project must start clean");

    const QJsonObject report = memory.validate(temporaryProject.path(), &error);
    ok &= require(report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"), "memory validation must pass");

    QTemporaryDir limitedProject;
    ProjectModel limitedModel;
    auto limitedMemory = limitedModel.memoryConfiguration();
    limitedMemory.maximumSizeBytes = 1024 * 1024;
    limitedModel.setMemoryConfiguration(limitedMemory);
    ProjectMemory limitedMemoryService;
    ok &= require(limitedMemoryService.initialize(limitedProject.path(), &limitedModel, &error), "limited memory project must initialize");
    const bool oversizedWrite = limitedMemoryService.appendEvent(
        limitedProject.path(), QStringLiteral("TEST_OVERSIZED_WRITE"), QStringLiteral("limit test"),
        QJsonObject{{QStringLiteral("payload"), QString(2 * 1024 * 1024, QLatin1Char('x'))}}, &error);
    ok &= require(!oversizedWrite, "memory service must reject writes over the configured limit");

    QTemporaryDir generationProject;
    ProjectModel generationModel;
    generationModel.setProjectName(QStringLiteral("Generation Test Project"));
    generationModel.setProjectPath(generationProject.path());
    RuleConfiguration generationRules;
    generationRules.activeCategories = {QStringLiteral("coding-standards")};
    generationRules.enforcementLevel = QStringLiteral("strict");
    generationRules.loadingStrategy = QStringLiteral("relevant");
    generationRules.workScopes = {QStringLiteral("coding")};
    generationRules.projectScopes = {QStringLiteral("source-code")};
    generationRules.contextPolicies = {QStringLiteral("matching-categories")};
    generationModel.setRuleConfiguration(generationRules);
    ProjectResource generationResource;
    generationResource.id = QStringLiteral("generation-resource");
    generationResource.name = QStringLiteral("Generation Datasheet");
    generationResource.type = QStringLiteral("file");
    generationResource.location = QStringLiteral("C:/docs/datasheet.pdf");
    generationResource.authorityLevel = QStringLiteral("primary-source-of-truth");
    generationResource.scopes = {QStringLiteral("hardware")};
    generationModel.setResources({generationResource});

    GenerationServices generationServices;
    GenerationOptions generationOptions;
    generationOptions.generateAgentRules = true;
    generationOptions.generateRouting = true;
    generationOptions.generatePlatforms = false;
    generationOptions.generateResources = true;
    generationOptions.generateMemory = false;
    generationOptions.generateProvenance = true;
    const GenerationResult generationResult = generationServices.generate(generationModel, generationOptions);
    ok &= require(generationResult.success, "selective generation must succeed");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF/rules/generated-rules.md")), "generated rules must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF/routing/task-routes.json")), "task routes must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF/resources/resources.json")), "resource manifest must exist");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("ARAMF/memory/memory-config.json")), "disabled memory must not initialize memory");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("ARAMF/platforms/platform-metadata.json")), "disabled platforms must not generate metadata");
    VerificationServices verificationServices;
    FinalizationServices finalizationServices;
    const VerificationResult selectiveVerification = verificationServices.verify(generationModel, generationOptions);
    ok &= require(selectiveVerification.overallStatus == VerificationStatus::Pass,
                  "selective verification must pass without the memory product");
    const FinalizationResult selectiveFinalized = finalizationServices.finalize(generationModel, generationOptions);
    ok &= require(selectiveFinalized.success && !selectiveFinalized.alreadyFinalized,
                  "finalization must allow a verified selective generation without the memory product");
    const FinalizationResult repeatedSelectiveFinalization = finalizationServices.finalize(generationModel, generationOptions);
    ok &= require(repeatedSelectiveFinalization.success && repeatedSelectiveFinalization.alreadyFinalized,
                  "selective finalization must remain idempotent");

    QFile generatedRules(QDir(generationProject.path()).filePath("ARAMF/rules/generated-rules.md"));
    ok &= require(generatedRules.open(QIODevice::ReadOnly | QIODevice::Text), "generated rules must be readable");
    ok &= require(QString::fromUtf8(generatedRules.readAll()).contains(QStringLiteral("Coding Standards")), "generated rules must use catalog display names");
    generatedRules.close();

    const QString routingPath = QDir(generationProject.path()).filePath("ARAMF/routing/task-routes.json");
    QFile routingSentinel(routingPath);
    ok &= require(routingSentinel.open(QIODevice::WriteOnly | QIODevice::Text), "routing sentinel must be writable");
    routingSentinel.write("sentinel");
    routingSentinel.close();
    generationOptions.generateRouting = false;
    const GenerationResult skippedRouting = generationServices.generate(generationModel, generationOptions);
    ok &= require(skippedRouting.success, "generation without routing must succeed");
    ok &= require(QFile(routingPath).open(QIODevice::ReadOnly | QIODevice::Text), "unselected routing output must remain available");

    generationOptions.generateMemory = true;
    const GenerationResult memoryGeneration = generationServices.generate(generationModel, generationOptions);
    ok &= require(memoryGeneration.success, "memory generation must succeed when selected");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF/memory/memory-consistency-validation.json")), "memory validation must be generated");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("aramf_setup")), "generated output must never use aramf_setup");

    AiConfiguration entryPointAi;
    entryPointAi.primaryAgent = QStringLiteral("openai-codex");
    entryPointAi.additionalAgents = {QStringLiteral("claude-code"), QStringLiteral("github-copilot"), QStringLiteral("claude-code")};
    generationModel.setAiConfiguration(entryPointAi);
    AgentEntryPointService entryPointService;
    const AgentEntryPointResult entryPoints = entryPointService.createEntryPoints(generationModel);
    ok &= require(entryPoints.success, "AI agent entry-point creation must succeed");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("AGENTS.md")), "generic root entry point must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("CLAUDE.md")), "Claude entry point must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath(".github/copilot-instructions.md")), "Copilot entry point must exist");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("CODEX.md")), "Codex must use generic AGENTS.md");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("bootstrap")), "entry-point generation must not create bootstrap directory");
    for (const auto& relative : {QStringLiteral("AGENTS.md"), QStringLiteral("CLAUDE.md"), QStringLiteral(".github/copilot-instructions.md")}) {
        QFile entryFile(QDir(generationProject.path()).filePath(relative));
        ok &= require(entryFile.open(QIODevice::ReadOnly | QIODevice::Text), "entry point must be readable");
        const QString entryText = QString::fromUtf8(entryFile.readAll());
        ok &= require(entryText.contains(QStringLiteral("ARAMF/AGENTS.md")), "entry point must route to canonical ARAMF instructions");
        ok &= require(entryText.count(QStringLiteral("ARAMF-BEGIN")) == 1, "entry point must contain one managed section");
    }
    const AgentEntryPointResult repeatedEntryPoints = entryPointService.createEntryPoints(generationModel);
    ok &= require(repeatedEntryPoints.success && repeatedEntryPoints.createdFiles.isEmpty()
                  && repeatedEntryPoints.updatedFiles.isEmpty()
                  && repeatedEntryPoints.unchangedFiles.contains(QStringLiteral("AGENTS.md"))
                  && repeatedEntryPoints.unchangedFiles.contains(QStringLiteral("CLAUDE.md"))
                  && repeatedEntryPoints.unchangedFiles.contains(QStringLiteral(".github/copilot-instructions.md")),
                  "repeated entry-point creation must be idempotent");
    entryPointAi.additionalAgents = {QStringLiteral("ollama")};
    generationModel.setAiConfiguration(entryPointAi);
    const AgentEntryPointResult unsupportedEntryPoint = entryPointService.createEntryPoints(generationModel);
    ok &= require(unsupportedEntryPoint.success && !unsupportedEntryPoint.genericAgents.isEmpty(), "unsupported agents must use generic entry point");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("OLLAMA.md")), "unsupported agents must not get invented files");

    QTemporaryDir userOwnedProject;
    QFile userAgents(QDir(userOwnedProject.path()).filePath("AGENTS.md"));
    ok &= require(userAgents.open(QIODevice::WriteOnly | QIODevice::Text), "user-owned AGENTS.md must be writable");
    userAgents.write("User instructions\n");
    userAgents.close();
    ProjectModel userOwnedModel;
    userOwnedModel.setProjectPath(userOwnedProject.path());
    const AgentEntryPointResult userOwnedEntryPoint = entryPointService.createEntryPoints(userOwnedModel);
    ok &= require(userOwnedEntryPoint.success, "entry point must preserve user-owned root content");
    ok &= require(userAgents.open(QIODevice::ReadOnly | QIODevice::Text), "updated user-owned AGENTS.md must be readable");
    const QString userAgentsText = QString::fromUtf8(userAgents.readAll());
    ok &= require(userAgentsText.contains(QStringLiteral("User instructions")), "user-owned root content must be preserved");
    ok &= require(userAgentsText.count(QStringLiteral("ARAMF-BEGIN")) == 1, "user-owned root must receive one managed section");
    QFile eventLog(QDir(generationProject.path()).filePath("ARAMF/memory/event-log.jsonl"));
    ok &= require(eventLog.open(QIODevice::ReadOnly | QIODevice::Text), "memory event log must be readable");
    const QByteArray firstEvents = eventLog.readAll();
    eventLog.close();
    const GenerationResult repeatedGeneration = generationServices.generate(generationModel, generationOptions);
    ok &= require(repeatedGeneration.success, "repeated generation must succeed");
    ok &= require(eventLog.open(QIODevice::ReadOnly | QIODevice::Text), "repeated memory event log must be readable");
    const QByteArray repeatedEvents = eventLog.readAll();
    eventLog.close();
    ok &= require(firstEvents.count("PROJECT_MEMORY_ACTIVATED") == repeatedEvents.count("PROJECT_MEMORY_ACTIVATED"),
                  "repeated memory generation must not duplicate activation events");

    const VerificationResult verification = verificationServices.verify(generationModel, generationOptions);
    ok &= require(verification.overallStatus == VerificationStatus::Pass,
                  "verification must pass for the generated selected products");
    const FinalizationResult finalized = finalizationServices.finalize(generationModel, generationOptions);
    ok &= require(finalized.success && !finalized.alreadyFinalized,
                  "finalization must record a verified lifecycle completion");
    const FinalizationResult repeatedFinalization = finalizationServices.finalize(generationModel, generationOptions);
    ok &= require(repeatedFinalization.success && repeatedFinalization.alreadyFinalized,
                  "finalization must be idempotent for an unchanged configuration");
    auto changedCapabilities = generationModel.developmentCapabilities();
    changedCapabilities.languages.append(QStringLiteral("c"));
    generationModel.setDevelopmentCapabilities(changedCapabilities);
    const VerificationResult staleVerification = verificationServices.verify(generationModel, generationOptions);
    ok &= require(staleVerification.overallStatus == VerificationStatus::Warning,
                  "verification must report stale generated configuration after model changes");
    const FinalizationResult blockedFinalization = finalizationServices.finalize(generationModel, generationOptions);
    ok &= require(!blockedFinalization.success && !blockedFinalization.blockers.isEmpty(),
                  "stale verification must block finalization");

    GenerationOptions noProducts;
    noProducts.generateAgentRules = false;
    noProducts.generateRouting = false;
    noProducts.generatePlatforms = false;
    noProducts.generateResources = false;
    noProducts.generateMemory = false;
    noProducts.generateProvenance = false;
    const GenerationResult noProductResult = generationServices.generate(generationModel, noProducts);
    ok &= require(!noProductResult.success && noProductResult.error.contains(QStringLiteral("select at least one")), "empty generation must be rejected");
    return ok ? 0 : 1;
}
