// ProjectMemoryTests.cpp

#include "core/ProjectMemory.h"
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
    memoryConfiguration.maximumSizeBytes = 250LL * 1024LL * 1024LL;
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
    return ok ? 0 : 1;
}
