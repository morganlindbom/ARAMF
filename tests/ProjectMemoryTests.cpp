// ProjectMemoryTests.cpp

#include "core/ProjectMemory.h"
#include "core/MemoryCommand.h"
#include "core/FrameworkKnowledge.h"
#include "core/EnvironmentCatalog.h"
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "core/ValidationRouting.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <algorithm>
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
    QTemporaryDir globalData;
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(QDir(globalData.path()).filePath(QStringLiteral("ARAMF_DATA/framework-knowledge-library.json")));
    QTemporaryDir temporaryProject;
    if (!require(temporaryProject.isValid(), "temporary project directory must be valid")) {
        return 1;
    }

    ProjectModel model;
    model.setProjectName(QStringLiteral("Memory Test Project"));
    model.setProjectPath(temporaryProject.path());
    MemoryConfiguration feedbackConfiguration;
    feedbackConfiguration.captureCategories = {QStringLiteral("completed-tasks"), QStringLiteral("build-attempts"), QStringLiteral("test-attempts")};
    feedbackConfiguration.historyOptions = {QStringLiteral("event-history"), QStringLiteral("task-history")};
    feedbackConfiguration.maintenanceOptions = {
        QStringLiteral("update-current-state"), QStringLiteral("record-validation"),
        QStringLiteral("record-build-results"), QStringLiteral("record-test-results"),
        QStringLiteral("record-task-completion"), QStringLiteral("update-project-status"),
        QStringLiteral("preserve-append-only")};
    feedbackConfiguration.validationOptions = {QStringLiteral("memory-consistency")};
    model.setMemoryConfiguration(feedbackConfiguration);

    ProjectMemory memory;
    QString error;
    if (!require(memory.initialize(temporaryProject.path(), &model, &error), qPrintable(error))) {
        return 1;
    }

    const QDir root(temporaryProject.path());
    bool ok = true;

    const auto focusedPlan = ValidationRouting::route({QStringLiteral("src/ui/workflows/resources/authority/ResourceAuthorityPage.cpp"),
                                                       QStringLiteral("tests/WorkflowNavigationTests.cpp")},
                                                      QStringLiteral("ordinary UI maintenance"));
    ok &= require(focusedPlan.level == ValidationLevel::Focused, "isolated UI help-text work must route to FOCUSED");
    ok &= require(!focusedPlan.requiredChecks.contains(QStringLiteral("test_250"))
                  && !focusedPlan.requiredChecks.contains(QStringLiteral("test_550-automated")),
                  "focused routing must exclude broad historical campaigns");
    const auto projectMemoryPlan = ValidationRouting::route({QStringLiteral("src/core/ProjectMemory.cpp"),
                                                             QStringLiteral("tests/ProjectMemoryTests.cpp")},
                                                            QStringLiteral("bug fix"));
    ok &= require(projectMemoryPlan.level == ValidationLevel::Subsystem
                  && projectMemoryPlan.requiredChecks.contains(QStringLiteral("ctest"))
                  && projectMemoryPlan.requiredChecks.contains(QStringLiteral("memory-consistency")),
                  "ProjectMemory behavior must route to SUBSYSTEM validation");
    const auto contractPlan = ValidationRouting::route({QStringLiteral("src/core/ProjectMemory.cpp"),
                                                        QStringLiteral("src/core/Services.cpp")},
                                                       QStringLiteral("memory contract change"));
    ok &= require(contractPlan.level == ValidationLevel::Subsystem
                  && contractPlan.optionalChecks.join(QStringLiteral(" ")).contains(QStringLiteral("cold-start")),
                  "memory contract changes must make cold-start validation conditional");
    const auto migrationPlan = ValidationRouting::route({QStringLiteral("src/core/ControlPlaneMigration.cpp")},
                                                        QStringLiteral("architecture migration"));
    ok &= require(migrationPlan.level == ValidationLevel::FullRegression
                  && migrationPlan.requiredChecks.contains(QStringLiteral("test_250"))
                  && migrationPlan.requiredChecks.contains(QStringLiteral("test_550-automated")),
                  "control-plane migration must route to FULL REGRESSION");
    const auto explicitPlan = ValidationRouting::route({QStringLiteral("docs/release.md")}, QStringLiteral("milestone"));
    ok &= require(explicitPlan.level == ValidationLevel::FullRegression, "explicit milestones must route to FULL REGRESSION");
    const auto escalatedPlan = ValidationRouting::route({QStringLiteral("src/ui/Widget.cpp")}, {}, false, true);
    ok &= require(escalatedPlan.level == ValidationLevel::Subsystem, "focused failure must escalate to SUBSYSTEM");
    const auto policy = ValidationRouting::policy();
    ok &= require(policy.value(QStringLiteral("levels")).toObject().contains(QStringLiteral("focused"))
                  && policy.value(QStringLiteral("routes")).toObject().contains(QStringLiteral("project-memory")),
                  "validation policy must expose machine-readable levels and routes");
    ok &= require(root.exists(QStringLiteral("ARAMF_WORKER/AGENTS.md")), "canonical ARAMF_WORKER/AGENTS.md must exist");
    ok &= require(root.exists(QStringLiteral("ARAMF_WORKER/PROJECT_STATUS.md")), "ARAMF_WORKER/PROJECT_STATUS.md must exist");
    ok &= require(root.exists(QStringLiteral("ARAMF_WORKER/memory/decisions.md")), "durable decisions must live under ARAMF_WORKER/memory");
    ok &= require(root.exists(QStringLiteral("AGENTS.md")), "root agent bootstrap must exist");
    ok &= require(!root.exists(QStringLiteral("aramf.py")), "no Python backend file may be generated");
    QFile generatedBootstrap(root.filePath(QStringLiteral("AGENTS.md")));
    ok &= require(generatedBootstrap.open(QIODevice::ReadOnly | QIODevice::Text), "generated root bootstrap must be readable");
    const QString bootstrapText = QString::fromUtf8(generatedBootstrap.readAll());
    ok &= require(bootstrapText.contains(QStringLiteral("ARAMF_WORKER/AGENTS.md")), "generated bootstrap must route to ARAMF_WORKER control plane");
    ok &= require(!bootstrapText.contains(QStringLiteral("aramf_setup")), "generated bootstrap must not reference repository setup");
    ok &= require(!root.exists(QStringLiteral("aramf_setup")), "generated project must not contain repository setup directory");
    ok &= require(!root.exists(QStringLiteral("bootstrap")), "generated project must not contain repository bootstrap directory");
    ok &= require(root.exists(QStringLiteral("ARAMF_WORKER/memory/framework-knowledge.json")), "Framework Knowledge store must exist");
    ok &= require(root.exists(QStringLiteral("ARAMF_WORKER/memory/memory-contract.json")), "memory contract must exist");
    QJsonObject memoryConfig;
    QFile memoryConfigFile(root.filePath(QStringLiteral("ARAMF_WORKER/memory/memory-config.json")));
    ok &= require(memoryConfigFile.open(QIODevice::ReadOnly), "memory config must be readable");
    memoryConfig = QJsonDocument::fromJson(memoryConfigFile.readAll()).object();
    memoryConfigFile.close();
    ok &= require(memoryConfig.value(QStringLiteral("maintenanceOptions")).toArray().contains(QStringLiteral("record-build-results")),
                  "memory config must preserve maintenance options");
    QJsonObject memoryContract;
    QFile memoryContractFile(root.filePath(QStringLiteral("ARAMF_WORKER/memory/memory-contract.json")));
    ok &= require(memoryContractFile.open(QIODevice::ReadOnly), "memory contract must be readable");
    memoryContract = QJsonDocument::fromJson(memoryContractFile.readAll()).object();
    memoryContractFile.close();
    ok &= require(memoryContract.value(QStringLiteral("supportedOperations")).toArray().contains(QStringLiteral("test-result")),
                  "memory contract must advertise test recording");
    QFile canonicalAgentFile(root.filePath(QStringLiteral("ARAMF_WORKER/AGENTS.md")));
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
    const auto proposedEntries = frameworkKnowledge.entries(temporaryProject.path(), &error);
    const auto proposedCandidate = std::find_if(proposedEntries.cbegin(), proposedEntries.cend(), [&candidateId](const auto& entry) { return entry.id == candidateId; });
    ok &= require(proposedCandidate != proposedEntries.cend()
                  && proposedCandidate->status == QStringLiteral("candidate")
                  && proposedCandidate->reviewStatus == QStringLiteral("more-evidence"),
                  "Framework Knowledge candidate must remain reviewable and inactive");
    const QString repeatedCandidateId = frameworkKnowledge.propose(
        temporaryProject.path(),
        QStringLiteral("Preserve verified corrections"),
        QStringLiteral("Do not restore a superseded implementation after a verified correction unless newer evidence requires it."),
        {QStringLiteral("implementation"), QStringLiteral("regression")},
        {QStringLiteral("Second evidence item.")},
        true, &error);
    ok &= require(repeatedCandidateId == candidateId, "matching Framework Knowledge proposals must deduplicate");
    const auto beforeApproval = frameworkKnowledge.approvedEntries(temporaryProject.path(), {QStringLiteral("implementation")}, &error);
    ok &= require(std::none_of(beforeApproval.cbegin(), beforeApproval.cend(), [&candidateId](const auto& entry) { return entry.id == candidateId; }),
                  "candidate Framework Knowledge must not be active before approval");
    ok &= require(!frameworkKnowledge.approve(temporaryProject.path(), candidateId, QString(), &error),
                  "Framework Knowledge approval must require an explicit approval source");
    error.clear();
    ok &= require(frameworkKnowledge.approve(temporaryProject.path(), candidateId, QStringLiteral("explicit-user-approval"), &error),
                  "Framework Knowledge candidate must be approvable after explicit user approval");
    const auto approvedEntries = frameworkKnowledge.entries(temporaryProject.path(), &error);
    const auto approvedCandidate = std::find_if(approvedEntries.cbegin(), approvedEntries.cend(), [&candidateId](const auto& entry) { return entry.id == candidateId; });
    ok &= require(approvedCandidate != approvedEntries.cend() && approvedCandidate->reviewStatus == QStringLiteral("approved"),
                  "Framework Knowledge approval must update review status");
    const auto activeKnowledge = frameworkKnowledge.approvedEntries(temporaryProject.path(), {QStringLiteral("implementation")}, &error);
    ok &= require(std::any_of(activeKnowledge.cbegin(), activeKnowledge.cend(), [&candidateId](const auto& entry) { return entry.id == candidateId; }),
                  "approved Framework Knowledge must become active immediately");
    ok &= require(frameworkKnowledge.supersede(temporaryProject.path(), candidateId, QStringLiteral("replacement-entry"), &error),
                  "Framework Knowledge must support non-destructive superseding");
    const auto afterSupersede = frameworkKnowledge.approvedEntries(temporaryProject.path(), {QStringLiteral("implementation")}, &error);
    ok &= require(std::none_of(afterSupersede.cbegin(), afterSupersede.cend(), [&candidateId](const auto& entry) { return entry.id == candidateId; }),
                  "superseded Framework Knowledge must no longer be active");

    QJsonObject recordingResult;
    ok &= require(memory.recordOperation(temporaryProject.path(), QStringLiteral("task-start"),
                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("Feedback bridge test")},
                                                     {QStringLiteral("category"), QStringLiteral("testing")}},
                                         &recordingResult, &error),
                  "task start must be recordable through ProjectMemory");
    ok &= require(memory.recordOperation(temporaryProject.path(), QStringLiteral("build-result"),
                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("Feedback bridge test")},
                                                     {QStringLiteral("status"), QStringLiteral("PASS")},
                                                     {QStringLiteral("configuration"), QStringLiteral("Debug")}},
                                         nullptr, &error),
                  "build result must be recordable through ProjectMemory");
    ok &= require(memory.recordOperation(temporaryProject.path(), QStringLiteral("test-result"),
                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("Feedback bridge test")},
                                                     {QStringLiteral("status"), QStringLiteral("PASS")},
                                                     {QStringLiteral("suite"), QStringLiteral("CTest")},
                                                     {QStringLiteral("passed"), 2}, {QStringLiteral("total"), 2}},
                                         nullptr, &error),
                  "test result must be recordable through ProjectMemory");
    ok &= require(memory.recordOperation(temporaryProject.path(), QStringLiteral("task-complete"),
                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("Feedback bridge test")},
                                                     {QStringLiteral("status"), QStringLiteral("PASS")},
                                                     {QStringLiteral("summary"), QStringLiteral("Completed successfully.")}},
                                         nullptr, &error),
                  "task completion must be recordable through ProjectMemory");
    QFile metricsFile(root.filePath(QStringLiteral("ARAMF_WORKER/memory/metrics.json")));
    ok &= require(metricsFile.open(QIODevice::ReadOnly), "metrics must remain readable after recording");
    const auto metrics = QJsonDocument::fromJson(metricsFile.readAll()).object();
    metricsFile.close();
    ok &= require(metrics.value(QStringLiteral("iterations")).toInt() == 1
                  && metrics.value(QStringLiteral("buildAttempts")).toInt() == 1
                  && metrics.value(QStringLiteral("testAttempts")).toInt() == 1
                  && metrics.value(QStringLiteral("failures")).toInt() == 0,
                  "feedback recording must update metrics deterministically");
    QFile checkpointsFile(root.filePath(QStringLiteral("ARAMF_WORKER/memory/checkpoints.json")));
    QJsonDocument checkpointDocument;
    if (checkpointsFile.open(QIODevice::ReadOnly)) checkpointDocument = QJsonDocument::fromJson(checkpointsFile.readAll());
    checkpointsFile.close();
    const bool noCheckpoints = checkpointDocument.isArray()
        ? checkpointDocument.array().isEmpty()
        : checkpointDocument.object().value(QStringLiteral("checkpoints")).toArray().isEmpty();
    ok &= require(noCheckpoints, "ordinary task feedback must not create a checkpoint");
    QFile currentStateFile(root.filePath(QStringLiteral("ARAMF_WORKER/memory/current-state.md")));
    QString currentState;
    if (currentStateFile.open(QIODevice::ReadOnly)) currentState = QString::fromUtf8(currentStateFile.readAll());
    currentStateFile.close();
    ok &= require(!currentState.isEmpty()
                  && currentState.contains(QStringLiteral("Latest Production Development Event"))
                  && currentState.contains(QStringLiteral("event-")),
                  "current state must advance to a recorded production event");

    QBuffer commandOutput;
    QBuffer commandError;
    commandOutput.open(QIODevice::ReadWrite);
    commandError.open(QIODevice::ReadWrite);
    QTextStream commandOut(&commandOutput);
    QTextStream commandErr(&commandError);
    const int commandStatus = runMemoryCommand({QStringLiteral("memory"), QStringLiteral("record"),
                                                 QStringLiteral("--project"), temporaryProject.path(),
                                                 QStringLiteral("--operation"), QStringLiteral("validation-result"),
                                                 QStringLiteral("--task"), QStringLiteral("Feedback bridge CLI test"),
                                                 QStringLiteral("--status"), QStringLiteral("PASS")}, commandOut, commandErr);
    commandOut.flush();
    commandErr.flush();
    if (commandStatus != 0 || !commandOutput.data().contains("recorded operation=validation-result")) {
        std::cerr << "memory command output: " << commandOutput.data().constData()
                  << " error: " << commandError.data().constData() << '\n';
    }
    ok &= require(commandStatus == 0 && commandOutput.data().contains("recorded operation=validation-result"),
                  "headless memory command must record without GUI");
    ok &= require(memory.recordOperation(temporaryProject.path(), QStringLiteral("build-result"),
                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("Feedback bridge failure test")},
                                                     {QStringLiteral("status"), QStringLiteral("FAIL")},
                                                     {QStringLiteral("detail"), QStringLiteral("controlled failure fixture")}},
                                         nullptr, &error),
                  "failed build result must still be recordable");
    QFile failedMetricsFile(root.filePath(QStringLiteral("ARAMF_WORKER/memory/metrics.json")));
    QJsonObject failedMetrics;
    if (failedMetricsFile.open(QIODevice::ReadOnly)) failedMetrics = QJsonDocument::fromJson(failedMetricsFile.readAll()).object();
    failedMetricsFile.close();
    ok &= require(failedMetrics.value(QStringLiteral("buildAttempts")).toInt() == 2
                  && failedMetrics.value(QStringLiteral("failures")).toInt() == 1,
                  "failed build must increment build attempts and failure metrics");
    QFile statusFile(root.filePath(QStringLiteral("ARAMF_WORKER/PROJECT_STATUS.md")));
    QString statusText;
    if (statusFile.open(QIODevice::ReadOnly)) statusText = QString::fromUtf8(statusFile.readAll());
    statusFile.close();
    ok &= require(statusText.contains(QStringLiteral("Latest Agent Task")),
                  "meaningful task completion must update project status through policy");

    QTemporaryDir disabledProject;
    ProjectModel disabledModel;
    disabledModel.setProjectPath(disabledProject.path());
    MemoryConfiguration disabledConfiguration;
    disabledConfiguration.maintenanceOptions = {QStringLiteral("record-task-completion")};
    disabledModel.setMemoryConfiguration(disabledConfiguration);
    ProjectMemory disabledMemory;
    ok &= require(disabledMemory.initializeMemory(disabledProject.path(), &disabledModel, &error),
                  "disabled recording fixture must initialize");
    ok &= require(!disabledMemory.recordOperation(disabledProject.path(), QStringLiteral("build-result"),
                                                  QJsonObject{{QStringLiteral("task"), QStringLiteral("Disabled build")},
                                                              {QStringLiteral("status"), QStringLiteral("PASS")}},
                                                  nullptr, &error)
                  && error.contains(QStringLiteral("Recording disabled")),
                  "disabled build recording must be rejected cleanly");
    error.clear();
    ok &= require(!disabledMemory.recordOperation(disabledProject.path(), QStringLiteral("test-result"),
                                                  QJsonObject{{QStringLiteral("task"), QStringLiteral("Disabled test")},
                                                              {QStringLiteral("status"), QStringLiteral("PASS")}},
                                                  nullptr, &error)
                  && error.contains(QStringLiteral("Recording disabled")),
                  "disabled test recording must be rejected cleanly");

    QTemporaryDir taskDisabledProject;
    ProjectModel taskDisabledModel;
    taskDisabledModel.setProjectPath(taskDisabledProject.path());
    MemoryConfiguration taskDisabledConfiguration;
    taskDisabledConfiguration.maintenanceOptions = {QStringLiteral("record-build-results")};
    taskDisabledModel.setMemoryConfiguration(taskDisabledConfiguration);
    ProjectMemory taskDisabledMemory;
    ok &= require(taskDisabledMemory.initializeMemory(taskDisabledProject.path(), &taskDisabledModel, &error),
                  "task-disabled recording fixture must initialize");
    error.clear();
    ok &= require(!taskDisabledMemory.recordOperation(taskDisabledProject.path(), QStringLiteral("task-complete"),
                                                      QJsonObject{{QStringLiteral("task"), QStringLiteral("Disabled task")},
                                                                  {QStringLiteral("status"), QStringLiteral("PASS")}},
                                                      nullptr, &error)
                  && error.contains(QStringLiteral("Recording disabled")),
                  "disabled task completion must be rejected cleanly");

    QTemporaryDir stateDisabledProject;
    ProjectModel stateDisabledModel;
    stateDisabledModel.setProjectPath(stateDisabledProject.path());
    MemoryConfiguration stateDisabledConfiguration;
    stateDisabledConfiguration.maintenanceOptions = {QStringLiteral("record-task-completion")};
    stateDisabledModel.setMemoryConfiguration(stateDisabledConfiguration);
    ProjectMemory stateDisabledMemory;
    ok &= require(stateDisabledMemory.initializeMemory(stateDisabledProject.path(), &stateDisabledModel, &error),
                  "current-state disabled fixture must initialize");
    QFile stateBeforeFile(QDir(stateDisabledProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/current-state.md")));
    QString stateBefore;
    if (stateBeforeFile.open(QIODevice::ReadOnly)) stateBefore = QString::fromUtf8(stateBeforeFile.readAll());
    stateBeforeFile.close();
    ok &= require(stateDisabledMemory.recordOperation(stateDisabledProject.path(), QStringLiteral("task-complete"),
                                                      QJsonObject{{QStringLiteral("task"), QStringLiteral("No snapshot update")},
                                                                  {QStringLiteral("status"), QStringLiteral("PASS")} },
                                                      nullptr, &error),
                  "task recording must work when current-state updates are disabled");
    QFile stateAfterFile(QDir(stateDisabledProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/current-state.md")));
    QString stateAfter;
    if (stateAfterFile.open(QIODevice::ReadOnly)) stateAfter = QString::fromUtf8(stateAfterFile.readAll());
    stateAfterFile.close();
    ok &= require(stateBefore == stateAfter, "disabled current-state maintenance must preserve the snapshot");

    QBuffer malformedOutput;
    QBuffer malformedError;
    malformedOutput.open(QIODevice::ReadWrite);
    malformedError.open(QIODevice::ReadWrite);
    QTextStream malformedOut(&malformedOutput);
    QTextStream malformedErr(&malformedError);
    const int malformedStatus = runMemoryCommand({QStringLiteral("memory"), QStringLiteral("record"),
                                                   QStringLiteral("--project"), temporaryProject.path(),
                                                   QStringLiteral("--operation"), QStringLiteral("unknown"),
                                                   QStringLiteral("--task"), QStringLiteral("Malformed request")},
                                                  malformedOut, malformedErr);
    malformedOut.flush();
    malformedErr.flush();
    ok &= require(malformedStatus != 0 && malformedError.data().contains("Unknown recording operation"),
                  "malformed memory requests must fail without writing state");

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
    ok &= require(loaded.resources().first().description == resource.description, "resource description must survive persistence");

    ProjectResource secondaryResource;
    secondaryResource.id = QStringLiteral("secondary-resource");
    secondaryResource.name = QStringLiteral("Secondary Resource");
    secondaryResource.type = QStringLiteral("folder");
    secondaryResource.location = QStringLiteral("C:/docs/secondary");
    secondaryResource.description = QStringLiteral("Supporting project reference");
    secondaryResource.authorityLevel = QStringLiteral("trusted-reference");
    secondaryResource.scopes = {QStringLiteral("architecture")};
    secondaryResource.status = QStringLiteral("available");
    model.setResources({resource, secondaryResource});
    const QString reorderedProjectFile = root.filePath(QStringLiteral("reordered-resources.aramf.json"));
    ok &= require(persistence.save(model, reorderedProjectFile, &error), "reordered resource save must succeed");
    ProjectModel reorderedLoaded;
    ok &= require(persistence.load(&reorderedLoaded, reorderedProjectFile, &error), "reordered resource open must succeed");
    auto reordered = reorderedLoaded.resources();
    std::reverse(reordered.begin(), reordered.end());
    reorderedLoaded.setResources(reordered);
    ok &= require(reorderedLoaded.resources().first().id == secondaryResource.id, "resource reorder must preserve resource identity");
    ok &= require(reorderedLoaded.resources().first().authorityLevel == secondaryResource.authorityLevel
                  && reorderedLoaded.resources().first().description == secondaryResource.description
                  && reorderedLoaded.resources().first().scopes == secondaryResource.scopes,
                  "authority, description, and scopes must remain attached after resource reorder");
    ok &= require(reorderedLoaded.resources().last().id == resource.id
                  && reorderedLoaded.resources().last().authorityLevel == resource.authorityLevel
                  && reorderedLoaded.resources().last().description == resource.description
                  && reorderedLoaded.resources().last().scopes == resource.scopes,
                  "reordered primary resource metadata must remain attached by ID");

    const QString identityRoot = QDir(root.path()).filePath(QStringLiteral("identity"));
    ok &= require(QDir().mkpath(identityRoot), "resource identity fixture directory must be creatable");
    QFile identityFile(QDir(identityRoot).filePath(QStringLiteral("same.txt")));
    ok &= require(identityFile.open(QIODevice::WriteOnly), "resource identity fixture file must be writable");
    identityFile.write("identity\n");
    identityFile.close();
    ProjectResource identityA;
    identityA.type = QStringLiteral("file");
    identityA.location = identityFile.fileName();
    ProjectResource identityB = identityA;
    identityB.location = QDir(root.path()).relativeFilePath(identityFile.fileName());
    ok &= require(sameResourceIdentity(identityA, identityB, root.path()), "absolute and project-relative paths must share resource identity");
    identityB.location = QDir(identityRoot).filePath(QStringLiteral("./same.txt"));
    ok &= require(sameResourceIdentity(identityA, identityB, root.path()), "dot path variants must share resource identity");
    identityB.location = identityFile.fileName() + QDir::separator();
    ok &= require(sameResourceIdentity(identityA, identityB, root.path()), "trailing separator variants must share resource identity");
    ProjectResource differentFile = identityA;
    differentFile.location = QDir(identityRoot).filePath(QStringLiteral("other/same.txt"));
    ok &= require(!sameResourceIdentity(identityA, differentFile, root.path()), "different files with the same name must remain distinct");
    ProjectResource urlA;
    urlA.type = QStringLiteral("url"); urlA.location = QStringLiteral("HTTPS://Example.com/docs/");
    ProjectResource urlB = urlA; urlB.location = QStringLiteral("https://example.com/docs");
    ok &= require(sameResourceIdentity(urlA, urlB, root.path()), "equivalent URLs must share resource identity");
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

    QTemporaryDir decisionProject;
    ProjectModel decisionModel;
    decisionModel.setProjectPath(decisionProject.path());
    MemoryConfiguration decisionConfiguration;
    decisionConfiguration.maintenanceOptions = {QStringLiteral("record-decisions"), QStringLiteral("update-current-state")};
    decisionConfiguration.validationOptions = {QStringLiteral("memory-consistency"), QStringLiteral("conflicting-decisions"),
                                               QStringLiteral("stale-current-state"), QStringLiteral("referenced-resources"),
                                               QStringLiteral("project-status-consistency")};
    decisionModel.setMemoryConfiguration(decisionConfiguration);
    ProjectMemory decisionMemory;
    const bool decisionInitialized = decisionMemory.initialize(decisionProject.path(), &decisionModel, &error);
    ok &= require(decisionInitialized,
                  "decision validation fixture must initialize");
    ok &= require(decisionMemory.recordDecision(decisionProject.path(), QStringLiteral("legacy-worker-authority"),
                                                QStringLiteral("worker-authority"),
                                                QStringLiteral("Legacy path authority retained for historical context."),
                                                QStringLiteral("superseded"), QStringLiteral("self-host-worker-authority"), &error),
                  "superseded durable decision must be recordable");
    ok &= require(decisionMemory.recordDecision(decisionProject.path(), QStringLiteral("self-host-worker-authority"),
                                                QStringLiteral("worker-authority"),
                                                QStringLiteral("ARAMF_WORKER is the live self-host control plane; aramf_setup is product source."),
                                                QStringLiteral("current"), {}, &error),
                  "replacement durable decision must be recordable");
    error.clear();
    ok &= require(!decisionMemory.recordDecision(decisionProject.path(), QStringLiteral("conflicting-worker-authority"),
                                                 QStringLiteral("worker-authority"),
                                                 QStringLiteral("A conflicting active authority."),
                                                 QStringLiteral("current"), {}, &error),
                  "conflicting active durable decisions must be rejected");
    const QJsonObject decisionReport = decisionMemory.validate(decisionProject.path(), &error);
    ok &= require(decisionReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS")
                  && !decisionReport.value(QStringLiteral("checks")).toArray().isEmpty(),
                  "superseded decisions must be excluded from active conflict validation");
    QFile decisionFile(decisionProject.path() + QStringLiteral("/ARAMF_WORKER/memory/decisions.md"));
    QString decisionText;
    if (decisionFile.open(QIODevice::ReadOnly | QIODevice::Text)) decisionText = QString::fromUtf8(decisionFile.readAll());
    decisionFile.close();
    ok &= require(decisionText.contains(QStringLiteral("Status: superseded")), "superseded decision history must remain visible");

    QTemporaryDir coldProject;
    ProjectModel coldModel;
    coldModel.setProjectPath(coldProject.path());
    MemoryConfiguration coldConfiguration;
    coldConfiguration.maintenanceOptions = {QStringLiteral("update-current-state")};
    coldConfiguration.validationOptions = {QStringLiteral("memory-consistency"), QStringLiteral("cold-start-validation"),
                                           QStringLiteral("sequence-continuity"), QStringLiteral("stale-current-state"),
                                           QStringLiteral("referenced-resources"), QStringLiteral("project-status-consistency")};
    coldModel.setMemoryConfiguration(coldConfiguration);
    ProjectMemory coldMemory;
    const bool coldInitialized = coldMemory.initialize(coldProject.path(), &coldModel, &error);
    ok &= require(coldInitialized, "cold-start fixture must initialize");
    const auto coldPass = coldMemory.validate(coldProject.path(), &error);
    ok &= require(coldPass.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                  "fresh cold-start fixture must validate");
    QFile coldAgent(coldProject.path() + QStringLiteral("/ARAMF_WORKER/AGENTS.md"));
    ok &= require(coldAgent.open(QIODevice::Append | QIODevice::Text), "cold-start fixture agent must be writable");
    coldAgent.write("\nchanged-for-stale-test\n");
    coldAgent.close();
    const auto staleCold = coldMemory.validate(coldProject.path(), &error);
    ok &= require(staleCold.value(QStringLiteral("status")).toString() == QStringLiteral("FAIL"),
                  "changed cold-start input must invalidate the persisted validation");
    ok &= require(coldMemory.validateColdStart(coldProject.path(), &error).value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                  "cold-start validation must refresh its fingerprint");

    QTemporaryDir checkpointProject;
    ProjectModel checkpointModel;
    checkpointModel.setProjectPath(checkpointProject.path());
    MemoryConfiguration checkpointConfiguration;
    checkpointConfiguration.maintenanceOptions = {QStringLiteral("record-checkpoints"), QStringLiteral("update-current-state")};
    checkpointConfiguration.validationOptions = {QStringLiteral("memory-consistency"), QStringLiteral("sequence-continuity")};
    checkpointModel.setMemoryConfiguration(checkpointConfiguration);
    ProjectMemory checkpointMemory;
    ok &= require(checkpointMemory.initialize(checkpointProject.path(), &checkpointModel, &error),
                  "checkpoint fixture must initialize with empty checkpoint history");
    const QJsonObject emptyCheckpointReport = checkpointMemory.validate(checkpointProject.path(), &error);
    ok &= require(emptyCheckpointReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                  "empty checkpoint history must validate");
    QJsonObject checkpointResult;
    const bool checkpointRecorded = checkpointMemory.recordCheckpoint(checkpointProject.path(),
                                                                       QStringLiteral("Stable memory baseline"),
                                                                       QStringLiteral("Verified Project Memory baseline."),
                                                                       QStringLiteral("checkpoint capability task"), {}, QStringLiteral("PASS"),
                                                                       &checkpointResult, &error);
    ok &= require(checkpointRecorded, QStringLiteral("deliberate checkpoint must be recordable: %1").arg(error).toUtf8().constData());
    ok &= require(checkpointResult.value(QStringLiteral("id")).toString().startsWith(QStringLiteral("checkpoint-"))
                  && !checkpointResult.value(QStringLiteral("createdAt")).toString().isEmpty()
                  && checkpointResult.value(QStringLiteral("productionSequence")).toInt() == 0
                  && !checkpointResult.value(QStringLiteral("latestEventId")).toString().isEmpty(),
                  "checkpoint identity and state references must be ARAMF-generated");
    QJsonObject secondCheckpoint;
    ok &= require(checkpointMemory.recordCheckpoint(checkpointProject.path(),
                                                    QStringLiteral("Second stable baseline"),
                                                    QStringLiteral("A second deliberate recovery point."), {}, {}, {},
                                                    &secondCheckpoint, &error),
                  "multiple deliberate checkpoints must append");
    ok &= require(secondCheckpoint.value(QStringLiteral("id")).toString() != checkpointResult.value(QStringLiteral("id")).toString(),
                  "checkpoint IDs must be unique");
    QFile checkpointFile(checkpointProject.path() + QStringLiteral("/ARAMF_WORKER/memory/checkpoints.json"));
    QJsonDocument checkpointHistoryDocument;
    if (checkpointFile.open(QIODevice::ReadOnly)) checkpointHistoryDocument = QJsonDocument::fromJson(checkpointFile.readAll());
    checkpointFile.close();
    ok &= require(checkpointHistoryDocument.object().value(QStringLiteral("checkpoints")).toArray().size() == 2,
                  "checkpoint history must preserve both records");
    QTemporaryDir disabledCheckpointProject;
    ProjectModel disabledCheckpointModel;
    disabledCheckpointModel.setProjectPath(disabledCheckpointProject.path());
    ProjectMemory disabledCheckpointMemory;
    ok &= require(disabledCheckpointMemory.initialize(disabledCheckpointProject.path(), &disabledCheckpointModel, &error),
                  "disabled checkpoint fixture must initialize");
    error.clear();
    ok &= require(!disabledCheckpointMemory.recordCheckpoint(disabledCheckpointProject.path(),
                                                             QStringLiteral("Should be rejected"),
                                                             QStringLiteral("Checkpoint recording is disabled."), {}, {}, {}, nullptr, &error)
                  && error.contains(QStringLiteral("disabled")),
                  "disabled checkpoint recording must be rejected");
    QTemporaryDir malformedCheckpointProject;
    ProjectModel malformedCheckpointModel;
    malformedCheckpointModel.setProjectPath(malformedCheckpointProject.path());
    ProjectMemory malformedCheckpointMemory;
    ok &= require(malformedCheckpointMemory.initialize(malformedCheckpointProject.path(), &malformedCheckpointModel, &error),
                  "malformed checkpoint fixture must initialize");
    QFile malformedCheckpointFile(malformedCheckpointProject.path() + QStringLiteral("/ARAMF_WORKER/memory/checkpoints.json"));
    ok &= require(malformedCheckpointFile.open(QIODevice::WriteOnly | QIODevice::Text),
                  "malformed checkpoint fixture must be writable");
    malformedCheckpointFile.write("{\"checkpoints\":[{\"id\":\"duplicate\"},{\"id\":\"duplicate\"}]}\n");
    malformedCheckpointFile.close();
    ok &= require(malformedCheckpointMemory.validate(malformedCheckpointProject.path(), &error)
                      .value(QStringLiteral("status")).toString() == QStringLiteral("FAIL"),
                  "malformed checkpoint history must fail validation");

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
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF_WORKER/rules/generated-rules.md")), "generated rules must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF_WORKER/routing/task-routes.json")), "task routes must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF_WORKER/routing/validation-policy.json")), "validation policy must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF_WORKER/resources/resources.json")), "resource manifest must exist");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("ARAMF_WORKER/memory/memory-config.json")), "disabled memory must not initialize memory");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("ARAMF_WORKER/platforms/platform-metadata.json")), "disabled platforms must not generate metadata");
    QFile generationManifest(QDir(generationProject.path()).filePath("ARAMF_WORKER/resources/resources.json"));
    ok &= require(generationManifest.open(QIODevice::ReadOnly | QIODevice::Text), "generated resource metadata must be readable");
    const auto generationManifestJson = QJsonDocument::fromJson(generationManifest.readAll()).object();
    generationManifest.close();
    const auto generatedResources = generationManifestJson.value(QStringLiteral("resources")).toArray();
    ok &= require(generatedResources.size() == 1, "generated resource metadata must contain the configured resource");
    if (!generatedResources.isEmpty()) {
        const auto generatedResource = generatedResources.first().toObject();
        ok &= require(generatedResource.value(QStringLiteral("authority")).toString() == generationResource.authorityLevel,
                      "generated resource authority must match the project resource");
        const auto generatedScopes = generatedResource.value(QStringLiteral("scopes")).toArray();
        ok &= require(generatedScopes.size() == generationResource.scopes.size()
                      && generatedScopes.first().toString() == generationResource.scopes.first(),
                      "generated resource scopes must match the project resource");
    }
    QTemporaryDir feedbackGenerationProject;
    ProjectModel feedbackGenerationModel;
    feedbackGenerationModel.setProjectName(QStringLiteral("Feedback Generation Project"));
    feedbackGenerationModel.setProjectPath(feedbackGenerationProject.path());
    feedbackGenerationModel.setMemoryConfiguration(feedbackConfiguration);
    GenerationOptions feedbackGenerationOptions;
    const GenerationResult feedbackGeneration = generationServices.generate(feedbackGenerationModel, feedbackGenerationOptions);
    ok &= require(feedbackGeneration.success, "feedback generation must succeed");
    QFile feedbackAgentFile(QDir(feedbackGenerationProject.path()).filePath("ARAMF_WORKER/AGENTS.md"));
    QString feedbackAgent;
    if (feedbackAgentFile.open(QIODevice::ReadOnly | QIODevice::Text)) feedbackAgent = QString::fromUtf8(feedbackAgentFile.readAll());
    feedbackAgentFile.close();
    ok &= require(feedbackAgent.contains(QStringLiteral("memory/memory-contract.json"))
                  && feedbackAgent.contains(QStringLiteral("Record completed build attempts")),
                  "generated agent instructions must describe configured memory feedback");
    ok &= require(QFile::exists(QDir(feedbackGenerationProject.path()).filePath("ARAMF_WORKER/memory/memory-contract.json")),
                  "feedback generation must create the machine-readable memory contract");
    VerificationServices verificationServices;
    FinalizationServices finalizationServices;

    QTemporaryDir duplicateProject;
    ProjectModel duplicateModel;
    duplicateModel.setProjectName(QStringLiteral("Duplicate Resource Test"));
    duplicateModel.setProjectPath(duplicateProject.path());
    const QString duplicateFilePath = QDir(duplicateProject.path()).filePath(QStringLiteral("src/same.txt"));
    ok &= require(QDir().mkpath(QFileInfo(duplicateFilePath).absolutePath()), "duplicate resource fixture directory must be creatable");
    QFile duplicateFile(duplicateFilePath);
    ok &= require(duplicateFile.open(QIODevice::WriteOnly), "duplicate resource fixture must be writable");
    duplicateFile.write("duplicate\n");
    duplicateFile.close();
    ProjectResource duplicateA;
    duplicateA.id = QStringLiteral("duplicate-a");
    duplicateA.name = QStringLiteral("same.txt");
    duplicateA.type = QStringLiteral("file");
    duplicateA.location = duplicateFilePath;
    duplicateA.status = QStringLiteral("available");
    ProjectResource duplicateB = duplicateA;
    duplicateB.id = QStringLiteral("duplicate-b");
    duplicateB.location = QDir(duplicateProject.path()).relativeFilePath(duplicateFilePath);
    duplicateModel.setResources({duplicateA, duplicateB});
    GenerationOptions duplicateOptions;
    duplicateOptions.generateAgentRules = false;
    duplicateOptions.generateRouting = false;
    duplicateOptions.generatePlatforms = false;
    duplicateOptions.generateResources = true;
    duplicateOptions.generateMemory = false;
    duplicateOptions.generateProvenance = false;
    const GenerationResult duplicateGeneration = generationServices.generate(duplicateModel, duplicateOptions);
    ok &= require(duplicateGeneration.success, "identical duplicate resources must be safely deduplicated during generation");
    QFile duplicateManifest(QDir(duplicateProject.path()).filePath("ARAMF_WORKER/resources/resources.json"));
    ok &= require(duplicateManifest.open(QIODevice::ReadOnly | QIODevice::Text), "deduplicated resource manifest must be readable");
    const auto duplicateManifestJson = QJsonDocument::fromJson(duplicateManifest.readAll()).object();
    duplicateManifest.close();
    ok &= require(duplicateManifestJson.value(QStringLiteral("resources")).toArray().size() == 1,
                  "generation must emit one resource entry for an identical duplicate identity");
    const VerificationResult duplicateVerification = verificationServices.verify(duplicateModel, duplicateOptions);
    ok &= require(duplicateVerification.overallStatus == VerificationStatus::Fail,
                  "Verify must reject duplicate canonical resource identities in the project configuration");
    ProjectResource conflictingDuplicate = duplicateB;
    conflictingDuplicate.id = QStringLiteral("duplicate-conflict");
    conflictingDuplicate.authorityLevel = QStringLiteral("primary-source-of-truth");
    duplicateModel.setResources({duplicateA, conflictingDuplicate});
    const GenerationResult conflictingGeneration = generationServices.generate(duplicateModel, duplicateOptions);
    ok &= require(!conflictingGeneration.success, "generation must reject conflicting duplicate resource metadata");

    const VerificationResult selectiveVerification = verificationServices.verify(generationModel, generationOptions);
    ok &= require(selectiveVerification.overallStatus == VerificationStatus::Pass,
                  "selective verification must pass without the memory product");
    const FinalizationResult selectiveFinalized = finalizationServices.finalize(generationModel, generationOptions);
    ok &= require(selectiveFinalized.success && !selectiveFinalized.alreadyFinalized,
                  "finalization must allow a verified selective generation without the memory product");
    const FinalizationResult repeatedSelectiveFinalization = finalizationServices.finalize(generationModel, generationOptions);
    ok &= require(repeatedSelectiveFinalization.success && repeatedSelectiveFinalization.alreadyFinalized,
                  "selective finalization must remain idempotent");

    QFile generatedRules(QDir(generationProject.path()).filePath("ARAMF_WORKER/rules/generated-rules.md"));
    ok &= require(generatedRules.open(QIODevice::ReadOnly | QIODevice::Text), "generated rules must be readable");
    ok &= require(QString::fromUtf8(generatedRules.readAll()).contains(QStringLiteral("Coding Standards")), "generated rules must use catalog display names");
    generatedRules.close();

    const QString routingPath = QDir(generationProject.path()).filePath("ARAMF_WORKER/routing/task-routes.json");
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
    ok &= require(memoryGeneration.success, qPrintable(QStringLiteral("memory generation must succeed when selected: %1").arg(memoryGeneration.error)));
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("ARAMF_WORKER/memory/memory-consistency-validation.json")), "memory validation must be generated");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("aramf_setup")), "generated output must never use aramf_setup");

    AiConfiguration entryPointAi;
    entryPointAi.primaryAgent = QStringLiteral("openai-codex");
    entryPointAi.additionalAgents = {QStringLiteral("claude-code"), QStringLiteral("gemini"), QStringLiteral("github-copilot"), QStringLiteral("claude-code")};
    generationModel.setAiConfiguration(entryPointAi);
    AgentEntryPointService entryPointService;
    const AgentEntryPointResult entryPoints = entryPointService.createEntryPoints(generationModel);
    ok &= require(entryPoints.success, "AI agent entry-point creation must succeed");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("AGENTS.md")), "generic root entry point must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("CLAUDE.md")), "Claude entry point must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath("GEMINI.md")), "Gemini entry point must exist");
    ok &= require(QFile::exists(QDir(generationProject.path()).filePath(".github/copilot-instructions.md")), "Copilot entry point must exist");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("CODEX.md")), "Codex must use generic AGENTS.md");
    ok &= require(!QFile::exists(QDir(generationProject.path()).filePath("bootstrap")), "entry-point generation must not create bootstrap directory");
    for (const auto& relative : {QStringLiteral("AGENTS.md"), QStringLiteral("CLAUDE.md"), QStringLiteral("GEMINI.md"), QStringLiteral(".github/copilot-instructions.md")}) {
        QFile entryFile(QDir(generationProject.path()).filePath(relative));
        ok &= require(entryFile.open(QIODevice::ReadOnly | QIODevice::Text), "entry point must be readable");
        const QString entryText = QString::fromUtf8(entryFile.readAll());
        ok &= require(entryText.contains(QStringLiteral("ARAMF_WORKER/AGENTS.md")), "entry point must route to canonical ARAMF instructions");
        ok &= require(entryText.count(QStringLiteral("ARAMF-BEGIN")) == 1, "entry point must contain one managed section");
    }
    const AgentEntryPointResult repeatedEntryPoints = entryPointService.createEntryPoints(generationModel);
    ok &= require(repeatedEntryPoints.success && repeatedEntryPoints.createdFiles.isEmpty()
                  && repeatedEntryPoints.updatedFiles.isEmpty()
                  && repeatedEntryPoints.unchangedFiles.contains(QStringLiteral("AGENTS.md"))
                  && repeatedEntryPoints.unchangedFiles.contains(QStringLiteral("CLAUDE.md"))
                  && repeatedEntryPoints.unchangedFiles.contains(QStringLiteral("GEMINI.md"))
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
    QFile eventLog(QDir(generationProject.path()).filePath("ARAMF_WORKER/memory/event-log.jsonl"));
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

    QTemporaryDir migrationRoot;
    const QString spacedProject = QDir(migrationRoot.path()).filePath(QStringLiteral("Project With Spaces"));
    ok &= require(QDir().mkpath(QDir(spacedProject).filePath("ARAMF/custom")), "legacy project custom directory must be creatable");
    QFile legacyCustom(QDir(spacedProject).filePath("ARAMF/custom/user-note.md"));
    ok &= require(legacyCustom.open(QIODevice::WriteOnly | QIODevice::Text), "legacy user content must be writable");
    legacyCustom.write("user-owned legacy content\n");
    legacyCustom.close();
    ProjectModel legacyModel;
    legacyModel.setProjectName(QStringLiteral("Legacy Migration Project"));
    legacyModel.setProjectPath(spacedProject);
    GenerationOptions migrationOptions;
    migrationOptions.generateAgentRules = true;
    migrationOptions.generateRouting = true;
    migrationOptions.generateMemory = true;
    migrationOptions.generateProvenance = true;
    const GenerationResult migrated = generationServices.generate(legacyModel, migrationOptions);
    ok &= require(migrated.success, "legacy project migration generation must succeed");
    ok &= require(QDir(spacedProject).exists(QStringLiteral("ARAMF_WORKER")), "new projects and migrated projects must use ARAMF_WORKER");
    ok &= require(QDir(spacedProject).exists(QStringLiteral("ARAMF")), "legacy ARAMF directory must be preserved");
    ok &= require(QFile::exists(QDir(spacedProject).filePath("ARAMF_WORKER/custom/user-note.md")), "legacy user content must be preserved in the worker");
    ok &= require(QFile::exists(QDir(spacedProject).filePath("ARAMF_WORKER/memory/framework-knowledge.json")), "Framework Knowledge must live under ARAMF_WORKER/memory");
    ok &= require(!QDir(spacedProject).exists(QStringLiteral("ARAMF_WORKER/ARAMF_WORKER")), "worker generation must not recurse into a nested worker");
    ok &= require(!QDir(spacedProject).exists(QStringLiteral("aramf_setup")) && !QDir(spacedProject).exists(QStringLiteral("bootstrap")), "migration must not generate setup or bootstrap directories");
    ok &= require(QFile::exists(QDir(spacedProject).filePath("ARAMF_WORKER/legacy-migration.json")), "legacy migration report must exist");
    QFile generatedRoot(QDir(spacedProject).filePath("AGENTS.md"));
    ok &= require(generatedRoot.open(QIODevice::ReadOnly | QIODevice::Text), "migrated root AGENTS.md must be readable");
    ok &= require(QString::fromUtf8(generatedRoot.readAll()).contains(QStringLiteral("ARAMF_WORKER/AGENTS.md")), "root AGENTS.md must route to ARAMF_WORKER");
    generatedRoot.close();

    const QString bothRoot = QDir(migrationRoot.path()).filePath(QStringLiteral("Both Directories"));
    ok &= require(QDir().mkpath(QDir(bothRoot).filePath("ARAMF_WORKER/custom")), "canonical worker fixture must be creatable");
    ok &= require(QDir().mkpath(QDir(bothRoot).filePath("ARAMF/custom")), "legacy fixture must be creatable");
    QFile canonicalSentinel(QDir(bothRoot).filePath("ARAMF_WORKER/custom/sentinel.txt"));
    ok &= require(canonicalSentinel.open(QIODevice::WriteOnly | QIODevice::Text), "canonical sentinel must be writable");
    canonicalSentinel.write("canonical\n");
    canonicalSentinel.close();
    QFile legacyConflict(QDir(bothRoot).filePath("ARAMF/custom/sentinel.txt"));
    ok &= require(legacyConflict.open(QIODevice::WriteOnly | QIODevice::Text), "legacy conflict must be writable");
    legacyConflict.write("legacy\n");
    legacyConflict.close();
    ProjectModel bothModel;
    bothModel.setProjectPath(bothRoot);
    const GenerationResult bothResult = generationServices.generate(bothModel, migrationOptions);
    ok &= require(bothResult.success && !bothResult.warnings.isEmpty(), "both-directory migration must warn and succeed");
    QFile canonicalReadback(QDir(bothRoot).filePath("ARAMF_WORKER/custom/sentinel.txt"));
    ok &= require(canonicalReadback.open(QIODevice::ReadOnly), "canonical sentinel must remain readable");
    ok &= require(QString::fromUtf8(canonicalReadback.readAll()).contains("canonical"), "canonical content must remain authoritative");
    ok &= require(QFile::exists(QDir(bothRoot).filePath("ARAMF/custom/sentinel.txt")), "legacy conflicting content must remain preserved");

    return ok ? 0 : 1;
}
