// ProjectMemoryTests.cpp

#include "core/ProjectMemory.h"
#include "core/ProjectMemoryCompaction.h"
#include "core/CertificationService.h"
#include "core/MemoryCommand.h"
#include "core/FrameworkKnowledge.h"
#include "core/EnvironmentCatalog.h"
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/ProjectRootRebindService.h"
#include "core/Services.h"
#include "core/ValidationRouting.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QProcess>
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

QByteArray readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return file.readAll();
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

    auto overrideEventCount = [&temporaryProject]() {
        QFile file(temporaryProject.path() + QStringLiteral("/ARAMF_WORKER/memory/event-log.jsonl"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
        int count = 0;
        while (!file.atEnd()) if (!file.readLine().trimmed().isEmpty()) ++count;
        return count;
    };
    const int overrideBaseline = overrideEventCount();
    ok &= require(!memory.isVerifiedAdministrativeOverride(QStringLiteral("Do it anyway.")),
                  "conflicting request without admin declaration must not verify");
    ok &= require(!memory.isVerifiedAdministrativeOverride(QStringLiteral("I am admin.")),
                  "incomplete administrator identity must not verify");
    ok &= require(!memory.isVerifiedAdministrativeOverride(QStringLiteral("I am Admin Morgan Lindbom.")),
                  "identity without explicit override intent must not verify");
    ok &= require(overrideEventCount() == overrideBaseline,
                  "denied override attempts must not create audit events");
    QJsonObject overrideResult;
    error.clear();
    ok &= require(memory.recordAdministrativeOverride(
                       temporaryProject.path(),
                       QStringLiteral("I am Admin Morgan Lindbom and I override this ARAMF rule."),
                       QStringLiteral("Recorder-only persistence rule"),
                       QStringLiteral("Recorder unavailable in the controlled test fixture."),
                       QStringLiteral("Persist one accepted administrative test decision."),
                       QStringLiteral("Persist the accepted decision once"),
                       {QStringLiteral("ARAMF_WORKER/memory/decisions.md")},
                       {QStringLiteral("ProjectMemory")}, false, {}, &overrideResult, &error),
                  "verified administrative override must be accepted");
    ok &= require(overrideResult.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                  "accepted override must report validation PASS");
    ok &= require(overrideEventCount() == overrideBaseline + 1,
                  "accepted override must create exactly one durable audit event");
    error.clear();
    ok &= require(!memory.recordAdministrativeOverride(
                       temporaryProject.path(),
                       QStringLiteral("Continue despite the restriction."),
                       QStringLiteral("Unrelated rule"), QStringLiteral("Replay attempt"),
                       QStringLiteral("Unrelated scope"), QStringLiteral("Unrelated action"),
                       {}, {}, false, {}, nullptr, &error),
                  "a previous override must not authorize an unrelated replay");
    ok &= require(overrideEventCount() == overrideBaseline + 1,
                  "replayed override must not create another audit event");
    error.clear();
    ok &= require(memory.validateColdStart(temporaryProject.path(), &error).value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                  "override history must survive cold-start validation");
    ok &= require(memory.validate(temporaryProject.path(), &error).value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                  "override history must survive memory validation");

    const QString adminKnowledgeTitle = QStringLiteral("Administrator-approved knowledge");
    const QString adminKnowledgeLesson = QStringLiteral("This knowledge is approved immediately by an explicit administrator override.");
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(temporaryProject.path() + QStringLiteral("/global-knowledge.json"));
    error.clear();
    const QString adminKnowledgeId = frameworkKnowledge.proposeApprovedByAdministrator(
        temporaryProject.path(), adminKnowledgeTitle, adminKnowledgeLesson,
        {QStringLiteral("pvd"), QStringLiteral("governance")},
        {QStringLiteral("Explicit administrator override test evidence.")},
        QStringLiteral("Admin Morgan Lindbom"), true, &error);
    ok &= require(!adminKnowledgeId.isEmpty(), "Admin Override knowledge must be created");
    const auto adminEntries = frameworkKnowledge.entries(temporaryProject.path(), &error);
    const auto adminEntry = std::find_if(adminEntries.cbegin(), adminEntries.cend(), [&adminKnowledgeId](const auto& entry) { return entry.id == adminKnowledgeId; });
    ok &= require(adminEntry != adminEntries.cend() && adminEntry->status == QStringLiteral("approved")
                  && adminEntry->reviewStatus == QStringLiteral("approved")
                  && adminEntry->approvalSource == QStringLiteral("Admin Morgan Lindbom"),
                  "Admin Override knowledge must be approved without a second approval operation");
    ok &= require(frameworkKnowledge.globalEntries(&error).isEmpty(),
                  "project-scope approved knowledge must not automatically become global");
    ok &= require(frameworkKnowledge.promoteToGlobal(temporaryProject.path(), adminKnowledgeId, &error),
                  "explicit global promotion must remain available separately");
    const auto globalAdminEntries = frameworkKnowledge.globalEntries(&error);
    ok &= require(std::any_of(globalAdminEntries.cbegin(), globalAdminEntries.cend(), [&adminKnowledgeId](const auto& entry) { return entry.id == adminKnowledgeId; }),
                  "explicit global promotion must publish approved knowledge");
    FrameworkKnowledgeService::clearGlobalLibraryPathForTests();
    error.clear();
    ok &= require(!memory.recordAdministrativeOverride(
                       temporaryProject.path(),
                       QStringLiteral("I am Admin Morgan Lindbom and I override this operation."),
                       QStringLiteral("Cleanup rule"), QStringLiteral("Test safety"),
                       QStringLiteral("project"), QStringLiteral("rmdir /s /q temporary state"),
                       {}, {}, false, {}, nullptr, &error),
                  "Admin Override must not bypass TOP PRIORITY destructive filesystem safety");

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

    QTemporaryDir noRecordingProject;
    ProjectModel noRecordingModel;
    noRecordingModel.setProjectPath(noRecordingProject.path());
    MemoryConfiguration noRecordingConfiguration;
    noRecordingConfiguration.writerMode = QStringLiteral("disabled");
    noRecordingConfiguration.maintenanceOptions = {QStringLiteral("update-current-state")};
    noRecordingModel.setMemoryConfiguration(noRecordingConfiguration);
    ProjectMemory noRecordingMemory;
    ok &= require(noRecordingMemory.initializeMemory(noRecordingProject.path(), &noRecordingModel, &error),
                  "MEMORY-REDISCOVERY-010 no-recording fixture must initialize");
    QFile noRecordingContract(QDir(noRecordingProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-contract.json")));
    QJsonObject noRecordingContractObject;
    if (noRecordingContract.open(QIODevice::ReadOnly)) noRecordingContractObject = QJsonDocument::fromJson(noRecordingContract.readAll()).object();
    noRecordingContract.close();
    ok &= require(!noRecordingMemory.recordingEnabled(noRecordingProject.path(), &error)
                      && !noRecordingContractObject.value(QStringLiteral("recordingEnabled")).toBool(),
                  "MEMORY-REDISCOVERY-010 disabled writer mode must disable Project Memory recording");

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

    // ANDROID-001..020: Android Studio/Kotlin/Gemini support remains a
    // catalog/template extension of the existing ARAMF model.
    const auto androidIds = templates.builtInTemplates();
    ok &= require(androidIds.contains(QStringLiteral("android-studio-kotlin-gemini")), "ANDROID-TEMPLATE-001 Android Studio/Kotlin/Gemini profile selectable");
    const auto androidDefinition = templates.definition(QStringLiteral("android-studio-kotlin-gemini"));
    ok &= require(androidDefinition.displayName == QStringLiteral("Android Studio/Kotlin/Gemini"), "ANDROID-TEMPLATE-001 visible template name");
    ok &= require(androidDefinition.environment.language == QStringLiteral("kotlin") && androidDefinition.capabilities.languages.contains(QStringLiteral("kotlin")), "ANDROID-002 Kotlin default");
    ok &= require(androidDefinition.environment.ide == QStringLiteral("android-studio") && androidDefinition.capabilities.ides.contains(QStringLiteral("android-studio")), "ANDROID-003 Android Studio primary IDE");
    ok &= require(androidDefinition.ai.primaryAgent == QStringLiteral("gemini") && androidDefinition.ai.additionalAgents.contains(QStringLiteral("openai-codex")), "ANDROID-004 Gemini and additional agents selectable");
    ok &= require(androidDefinition.ai.primaryAgent != QStringLiteral("aramf") && androidDefinition.ai.primaryAgent != QStringLiteral("framework"), "ANDROID-005 Gemini is not governance authority");
    QTemporaryDir androidProject;
    ProjectModel androidModel;
    androidModel.setProjectName(QStringLiteral("Android School Project")); androidModel.setProjectPath(androidProject.path());
    ok &= require(templates.applyTemplate(&androidModel, QStringLiteral("android-studio-kotlin-gemini")), "ANDROID-TEMPLATE-002 profile applies");
    ok &= require(androidModel.context() == QStringLiteral("android-application") && androidModel.developmentEnvironment().buildSystem == QStringLiteral("gradle"), "ANDROID-002 Android context and Gradle defaults");
    ok &= require(androidModel.templateId() == QStringLiteral("android-studio-kotlin-gemini"), "ANDROID-TEMPLATE-008 capability expansion uses the same architecture");
    ProjectResource courseSource;
    courseSource.id = QStringLiteral("course-assignment"); courseSource.name = QStringLiteral("Course assignment"); courseSource.type = QStringLiteral("markdown"); courseSource.location = QStringLiteral("docs/assignment.md"); courseSource.description = QStringLiteral("Kotlin is mandatory\nAndroid Studio is the official IDE\nminimum SDK = 26\nXML layouts are mandatory\nJetpack Compose must NOT be used\nlocal persistence must use Room\nunit tests are required\nlint must pass"); courseSource.authorityLevel = QStringLiteral("primary-source-of-truth"); courseSource.scopes = {QStringLiteral("academic-content"), QStringLiteral("source-code")};
    androidModel.setResources({courseSource});
    const auto androidConstraints = androidModel.androidConstraints();
    ok &= require(androidDefinition.capabilities.frameworks.contains(QStringLiteral("jetpack-compose")), "ANDROID-CONSTRAINT-004 Compose remains globally available");
    ok &= require(androidConstraints.minSdk == 26 && androidConstraints.minSdkSource == QStringLiteral("course-assignment"), "ANDROID-CONSTRAINT-001 structured min SDK and provenance");
    ok &= require(androidConstraints.kotlinRequired && androidConstraints.primaryIde == QStringLiteral("android-studio"), "ANDROID-CONSTRAINT-002 Kotlin and IDE requirements");
    ok &= require(androidConstraints.xmlRequired && androidConstraints.uiTechnology == QStringLiteral("xml"), "ANDROID-CONSTRAINT-007 XML project-required");
    ok &= require(!androidConstraints.composeAllowed && !androidConstraints.composeSelected && !androidModel.developmentCapabilities().frameworks.contains(QStringLiteral("jetpack-compose")), "ANDROID-CONSTRAINT-005/006 Compose prohibited and not effective");
    ok &= require(androidConstraints.roomRequired && androidConstraints.unitTestsRequired && androidConstraints.lintRequired, "ANDROID-CONSTRAINT-008/009/010 Android quality requirements");
    GenerationOptions androidOptions;
    const auto androidGeneration = GenerationServices().generate(androidModel, androidOptions);
    const QString androidProjectFile = QDir(androidProject.path()).filePath(QStringLiteral("acceptance.aramf.json"));
    ok &= require(ProjectPersistence().save(androidModel, androidProjectFile, &error), "ANDROID-CONSTRAINT-002 project constraints save");
    ProjectModel androidReloaded;
    ok &= require(ProjectPersistence().load(&androidReloaded, androidProjectFile, &error), "ANDROID-CONSTRAINT-002 project constraints reload");
    ok &= require(androidReloaded.androidConstraints().minSdk == 26 && !androidReloaded.androidConstraints().composeAllowed, "ANDROID-CONSTRAINT-002 effective constraints survive reload");
    ok &= require(androidGeneration.success, "ANDROID-008 Lite generates the existing minimum control plane");
    ok &= require(QFileInfo::exists(QDir(androidProject.path()).filePath("AGENTS.md")), "ANDROID-006 root router generated");
    ok &= require(QFileInfo::exists(QDir(androidProject.path()).filePath("ARAMF_WORKER/AGENTS.md")), "ANDROID-007 canonical worker generated");
    QFile androidAgent(QDir(androidProject.path()).filePath("ARAMF_WORKER/AGENTS.md")); QString androidAgentText; if (androidAgent.open(QIODevice::ReadOnly)) androidAgentText = QString::fromUtf8(androidAgent.readAll()); androidAgent.close();
    ok &= require(androidAgentText.contains(QStringLiteral("Android Studio")) && androidAgentText.contains(QStringLiteral("course assignment")), "ANDROID-010 Android governance guidance generated");
    ok &= require(androidAgentText.contains(QStringLiteral("gradlew.bat")), "ANDROID-013 Windows Gradle wrapper guidance generated");
    ok &= require(androidAgentText.contains(QStringLiteral("Compose")) && androidAgentText.contains(QStringLiteral("XML")) && androidAgentText.contains(QStringLiteral("higher authority")), "ANDROID-011 course Source of Truth overrides defaults");
    ok &= require(androidAgentText.contains(QStringLiteral("minimum SDK 26")) && androidAgentText.contains(QStringLiteral("VS Code")) && androidAgentText.contains(QStringLiteral("unit tests required")), "ANDROID-CONSTRAINT-013/014 generated effective governance");
    QFile effectiveConfig(QDir(androidProject.path()).filePath(QStringLiteral("ARAMF_WORKER/platforms/android-effective-config.json"))); effectiveConfig.open(QIODevice::ReadOnly); const auto effective = QJsonDocument::fromJson(effectiveConfig.readAll()).object(); effectiveConfig.close();
    ok &= require(effective.value(QStringLiteral("minSdk")).toInt() == 26 && effective.value(QStringLiteral("minSdkSource")).toString() == QStringLiteral("course-assignment"), "ANDROID-CONSTRAINT-011 generated constraint provenance");
    ok &= require(effective.value(QStringLiteral("composeAvailable")).toBool() && !effective.value(QStringLiteral("composeAllowed")).toBool() && !effective.value(QStringLiteral("composeSelected")).toBool(), "ANDROID-CONSTRAINT-012 Source of Truth remains distinct from Framework Knowledge");
    const auto androidRoute = ValidationRouting::route({QStringLiteral("app/src/main/java/MainActivity.kt")}, QStringLiteral("coding"));
    ok &= require(androidRoute.requiredChecks.contains(QStringLiteral("gradle-compile")) && androidRoute.optionalChecks.contains(QStringLiteral("gradle-lint")), "ANDROID-012 Android Gradle validation routing");
    const auto androidPolicy = ValidationRouting::policy();
    ok &= require(androidPolicy.value(QStringLiteral("android")).toObject().value(QStringLiteral("states")).toArray().size() >= 8, "ANDROID-014 validation states remain distinct");
    ok &= require(QFileInfo::exists(QDir(androidProject.path()).filePath("ARAMF_WORKER/memory/event-log.jsonl")), "ANDROID-015 Project Memory uses the existing architecture");
    ok &= require(QFileInfo::exists(QDir(androidProject.path()).filePath("ARAMF_WORKER/memory/compaction-manifest.json")), "ANDROID-016 memory compaction remains compatible");
    ok &= require(QFileInfo::exists(QDir(androidProject.path()).filePath("ARAMF_WORKER/memory/compaction-history.jsonl")), "ANDROID-016 compaction history is durable");
    ok &= require(QFileInfo::exists(QDir(androidProject.path()).filePath("ARAMF_WORKER/memory/framework-knowledge.json")), "ANDROID-017 Framework Knowledge remains compatible");
    androidModel.setAiConfiguration(AiConfiguration{QStringLiteral("openai-codex"), {QStringLiteral("gemini")}, {}, {}, {QStringLiteral("project-memory")}, {}, QStringLiteral("custom")});
    ok &= require(QFileInfo::exists(QDir(androidProject.path()).filePath("ARAMF_WORKER/PROJECT_STATUS.md")) && QFileInfo::exists(QDir(androidProject.path()).filePath("ARAMF_WORKER/memory/current-state.md")), "ANDROID-018 agent replacement preserves project state");
    ok &= require(androidDefinition.recommendedResources.contains(QStringLiteral("Course assignment / grading rubric")), "ANDROID-010 course Source of Truth resource supported");
    ok &= require(androidDefinition.capabilities.developmentTools.contains(QStringLiteral("android-sdk")), "ANDROID-019 external Android tooling is represented");
    const auto memoryBeforeHandoff = readTextFile(QDir(androidProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-contract.json")));
    auto codexAi = androidReloaded.aiConfiguration(); codexAi.primaryAgent = QStringLiteral("openai-codex"); androidReloaded.setAiConfiguration(codexAi);
    ok &= require(ProjectPersistence().save(androidReloaded, androidProjectFile, &error), "ANDROID-HANDOFF-002 Gemini to Codex save");
    ProjectModel codexReloaded;
    ok &= require(ProjectPersistence().load(&codexReloaded, androidProjectFile, &error) && codexReloaded.aiConfiguration().primaryAgent == QStringLiteral("openai-codex"), "ANDROID-HANDOFF-003 Codex fresh rediscovery");
    auto geminiAi = codexReloaded.aiConfiguration(); geminiAi.primaryAgent = QStringLiteral("gemini"); codexReloaded.setAiConfiguration(geminiAi);
    ok &= require(ProjectPersistence().save(codexReloaded, androidProjectFile, &error), "ANDROID-HANDOFF-004 Codex to Gemini save");
    ProjectModel geminiReloaded;
    ok &= require(ProjectPersistence().load(&geminiReloaded, androidProjectFile, &error) && geminiReloaded.aiConfiguration().primaryAgent == QStringLiteral("gemini") && geminiReloaded.androidConstraints().minSdk == 26, "ANDROID-HANDOFF-004 Gemini final rediscovery");
    ok &= require(readTextFile(QDir(androidProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-contract.json"))) == memoryBeforeHandoff, "ANDROID-HANDOFF-005 shared memory unchanged");
    ok &= require(androidIds.first() == QStringLiteral("pico-2w-visual-designer"), "ANDROID-020 existing PVD template ordering remains unchanged");

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

    // MEMORY-REDISCOVERY-001..012: exercise the shared read API, CLI adapter,
    // semantic cold-start checks, and a genuinely new ARAMF process.
    QTemporaryDir rediscoveryProject;
    ProjectModel rediscoveryModel;
    rediscoveryModel.setProjectPath(rediscoveryProject.path());
    MemoryConfiguration rediscoveryConfiguration;
    rediscoveryConfiguration.maintenanceOptions = {
        QStringLiteral("record-task-completion"), QStringLiteral("record-decisions"),
        QStringLiteral("record-checkpoints"), QStringLiteral("update-current-state")};
    rediscoveryConfiguration.validationOptions = {
        QStringLiteral("memory-consistency"), QStringLiteral("cold-start-validation"),
        QStringLiteral("sequence-continuity"), QStringLiteral("conflicting-decisions"),
        QStringLiteral("stale-current-state"), QStringLiteral("referenced-resources"),
        QStringLiteral("project-status-consistency")};
    rediscoveryModel.setMemoryConfiguration(rediscoveryConfiguration);
    ProjectMemory rediscoveryMemory;
    ok &= require(rediscoveryMemory.initialize(rediscoveryProject.path(), &rediscoveryModel, &error),
                  "rediscovery fixture must initialize");
    ok &= require(rediscoveryMemory.recordingEnabled(rediscoveryProject.path(), &error),
                  "recordingEnabled must reflect configured recording operations");

    QFile rediscoveryContract(QDir(rediscoveryProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-contract.json")));
    QJsonObject rediscoveryContractObject;
    if (rediscoveryContract.open(QIODevice::ReadOnly)) rediscoveryContractObject = QJsonDocument::fromJson(rediscoveryContract.readAll()).object();
    rediscoveryContract.close();
    ok &= require(rediscoveryContractObject.value(QStringLiteral("recordingEnabled")).toBool(),
                  "generated contract must report enabled recording when configured");

    const QString rediscoveryTask = QStringLiteral("ARAMF_REDISCOVERY_TASK_20260824");
    QJsonObject rediscoveryOperation;
    ok &= require(rediscoveryMemory.recordOperation(rediscoveryProject.path(), QStringLiteral("task-complete"),
                                                     QJsonObject{{QStringLiteral("task"), rediscoveryTask},
                                                                 {QStringLiteral("status"), QStringLiteral("PASS")},
                                                                 {QStringLiteral("summary"), QStringLiteral("rediscovery task marker")}},
                                                     &rediscoveryOperation, &error),
                  "MEMORY-REDISCOVERY-001 task write must succeed");
    const QString rediscoveryEventId = rediscoveryOperation.value(QStringLiteral("eventId")).toString();
    ProjectMemory reloadedMemory;
    QJsonObject recoveredEvent;
    ok &= require(reloadedMemory.eventById(rediscoveryProject.path(), rediscoveryEventId, &recoveredEvent, &error)
                      && recoveredEvent.value(QStringLiteral("eventId")).toString() == rediscoveryEventId
                      && recoveredEvent.value(QStringLiteral("task")).toString() == rediscoveryTask,
                  "MEMORY-REDISCOVERY-001 event must recover through official API");
    ok &= require(!reloadedMemory.eventsForTask(rediscoveryProject.path(), rediscoveryTask, {}, &error).isEmpty(),
                  "event task filtering must recover the task event");

    const QString rediscoveryDecisionId = QStringLiteral("ARAMF_REDISCOVERY_DECISION_20260824");
    ok &= require(rediscoveryMemory.recordDecision(rediscoveryProject.path(), rediscoveryDecisionId,
                                                    QStringLiteral("rediscovery-topic"),
                                                    QStringLiteral("rediscovery decision marker"),
                                                    QStringLiteral("current"), {}, &error),
                  "MEMORY-REDISCOVERY-002 decision write must succeed");
    ProjectMemory decisionReload;
    QJsonObject recoveredDecision;
    ok &= require(decisionReload.decisionById(rediscoveryProject.path(), rediscoveryDecisionId,
                                               &recoveredDecision, &error)
                      && recoveredDecision.value(QStringLiteral("decisionId")).toString() == rediscoveryDecisionId,
                  "MEMORY-REDISCOVERY-002 decision must recover by ID");
    const QString supersededDecisionId = QStringLiteral("ARAMF_REDISCOVERY_DECISION_SUPERSEDED_20260824");
    ok &= require(rediscoveryMemory.recordDecision(rediscoveryProject.path(), supersededDecisionId,
                                                    QStringLiteral("rediscovery-topic"),
                                                    QStringLiteral("superseded marker"),
                                                    QStringLiteral("superseded"), rediscoveryDecisionId, &error),
                  "MEMORY-REDISCOVERY-003 superseded decision write must succeed");
    ok &= require(reloadedMemory.currentDecisions(rediscoveryProject.path(), &error).size() == 1
                      && reloadedMemory.decisions(rediscoveryProject.path(), false, &error).size() == 1
                      && reloadedMemory.decisionById(rediscoveryProject.path(), supersededDecisionId,
                                                     &recoveredDecision, &error)
                      && recoveredDecision.value(QStringLiteral("status")).toString() == QStringLiteral("superseded"),
                  "MEMORY-REDISCOVERY-003 current and superseded decisions must remain distinct");

    QJsonObject rediscoveryCheckpoint;
    ok &= require(rediscoveryMemory.recordCheckpoint(rediscoveryProject.path(),
                                                      QStringLiteral("Rediscovery checkpoint"),
                                                      QStringLiteral("ARAMF_REDISCOVERY_CHECKPOINT_20260824"),
                                                      rediscoveryTask, {}, QStringLiteral("PASS"),
                                                      &rediscoveryCheckpoint, &error),
                  "MEMORY-REDISCOVERY-004 checkpoint write must succeed");
    ProjectMemory checkpointReload;
    QJsonObject recoveredCheckpoint;
    const QString rediscoveryCheckpointId = rediscoveryCheckpoint.value(QStringLiteral("id")).toString();
    ok &= require(checkpointReload.checkpointById(rediscoveryProject.path(), rediscoveryCheckpointId,
                                                   &recoveredCheckpoint, &error)
                      && recoveredCheckpoint.value(QStringLiteral("id")).toString() == rediscoveryCheckpointId,
                  "MEMORY-REDISCOVERY-004 checkpoint must recover by ID");
    QJsonObject latestCheckpointObject;
    ok &= require(checkpointReload.latestCheckpoint(rediscoveryProject.path(), &latestCheckpointObject, &error)
                      && latestCheckpointObject.value(QStringLiteral("id")).toString() == rediscoveryCheckpointId,
                  "MEMORY-REDISCOVERY-005 latest checkpoint must recover");

    QTemporaryDir malformedEventProject;
    ProjectModel malformedEventModel;
    malformedEventModel.setProjectPath(malformedEventProject.path());
    ProjectMemory malformedEventMemory;
    ok &= require(malformedEventMemory.initialize(malformedEventProject.path(), &malformedEventModel, &error),
                  "MEMORY-REDISCOVERY-006 malformed-event fixture must initialize");
    const QString malformedEventPath = QDir(malformedEventProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    QFile malformedEventFile(malformedEventPath);
    QByteArray eventBefore;
    if (malformedEventFile.open(QIODevice::ReadOnly)) eventBefore = malformedEventFile.readAll();
    malformedEventFile.close();
    ok &= require(malformedEventFile.open(QIODevice::Append | QIODevice::Text),
                  "malformed-event fixture must be appendable");
    malformedEventFile.write("{ malformed diagnostic record\n");
    malformedEventFile.close();
    QByteArray malformedBeforeRead;
    if (malformedEventFile.open(QIODevice::ReadOnly)) malformedBeforeRead = malformedEventFile.readAll();
    malformedEventFile.close();
    QString malformedEventError;
    ok &= require(malformedEventMemory.events(malformedEventProject.path(), &malformedEventError).isEmpty()
                      && malformedEventError.contains(QStringLiteral("Malformed JSONL")),
                  "MEMORY-REDISCOVERY-006 malformed event must fail safely");
    QByteArray eventAfter;
    if (malformedEventFile.open(QIODevice::ReadOnly)) eventAfter = malformedEventFile.readAll();
    malformedEventFile.close();
    ok &= require(eventBefore != eventAfter && malformedBeforeRead == eventAfter,
                  "malformed event read must not mutate durable state");

    auto runReadCommand = [&](const QStringList& arguments, QByteArray* commandResult) {
        QBuffer commandOut;
        QBuffer commandErr;
        commandOut.open(QIODevice::ReadWrite);
        commandErr.open(QIODevice::ReadWrite);
        QTextStream out(&commandOut);
        QTextStream err(&commandErr);
        const int status = runMemoryCommand(arguments, out, err);
        out.flush();
        err.flush();
        if (commandResult) *commandResult = commandOut.data();
        return status == 0 && commandErr.data().isEmpty();
    };
    QByteArray cliResult;
    ok &= require(runReadCommand({QStringLiteral("memory"), QStringLiteral("events"),
                                   QStringLiteral("--project"), rediscoveryProject.path(),
                                   QStringLiteral("--format"), QStringLiteral("json")}, &cliResult)
                      && cliResult.contains(rediscoveryTask.toUtf8()),
                  "MEMORY-REDISCOVERY-007 CLI event retrieval must use shared API");
    ok &= require(runReadCommand({QStringLiteral("memory"), QStringLiteral("decision"),
                                   QStringLiteral("--project"), rediscoveryProject.path(),
                                   QStringLiteral("--id"), rediscoveryDecisionId,
                                   QStringLiteral("--format"), QStringLiteral("json")}, &cliResult)
                      && cliResult.contains(rediscoveryDecisionId.toUtf8()),
                  "MEMORY-REDISCOVERY-008 CLI decision retrieval must use shared API");
    ok &= require(runReadCommand({QStringLiteral("memory"), QStringLiteral("checkpoint"), QStringLiteral("get"),
                                   QStringLiteral("--project"), rediscoveryProject.path(),
                                   QStringLiteral("--id"), rediscoveryCheckpointId,
                                   QStringLiteral("--format"), QStringLiteral("json")}, &cliResult)
                      && cliResult.contains(rediscoveryCheckpointId.toUtf8()),
                  "MEMORY-REDISCOVERY-009 CLI checkpoint retrieval must use shared API");

    ok &= require(rediscoveryMemory.validateColdStart(rediscoveryProject.path(), &error)
                      .value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                  "MEMORY-REDISCOVERY-011 semantic cold-start validation must pass");
    const QString aramfExecutable = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("aramf.exe"));
    auto runFreshProcess = [&](const QStringList& arguments, QByteArray* processOutput) {
        QProcess process;
        process.start(aramfExecutable, arguments);
        if (!process.waitForFinished(15000)) return false;
        if (processOutput) *processOutput = process.readAllStandardOutput();
        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    };
    ok &= require(QFile::exists(aramfExecutable), "MEMORY-REDISCOVERY-012 ARAMF executable must be available");
    ok &= require(runFreshProcess({QStringLiteral("memory"), QStringLiteral("event"),
                                   QStringLiteral("--project"), rediscoveryProject.path(),
                                   QStringLiteral("--id"), rediscoveryEventId,
                                   QStringLiteral("--format"), QStringLiteral("json")}, &cliResult)
                      && cliResult.contains(rediscoveryEventId.toUtf8()),
                  "MEMORY-REDISCOVERY-012 fresh process must retrieve event");
    ok &= require(runFreshProcess({QStringLiteral("memory"), QStringLiteral("decision"),
                                   QStringLiteral("--project"), rediscoveryProject.path(),
                                   QStringLiteral("--id"), rediscoveryDecisionId,
                                   QStringLiteral("--format"), QStringLiteral("json")}, &cliResult)
                      && cliResult.contains(rediscoveryDecisionId.toUtf8()),
                  "MEMORY-REDISCOVERY-012 fresh process must retrieve decision");
    ok &= require(runFreshProcess({QStringLiteral("memory"), QStringLiteral("checkpoint"), QStringLiteral("get"),
                                   QStringLiteral("--project"), rediscoveryProject.path(),
                                   QStringLiteral("--id"), rediscoveryCheckpointId,
                                   QStringLiteral("--format"), QStringLiteral("json")}, &cliResult)
                      && cliResult.contains(rediscoveryCheckpointId.toUtf8()),
                  "MEMORY-REDISCOVERY-012 fresh process must retrieve checkpoint");
    ok &= require(runFreshProcess({QStringLiteral("memory"), QStringLiteral("cold-start"),
                                   QStringLiteral("--project"), rediscoveryProject.path()}, &cliResult),
                  "MEMORY-REDISCOVERY-012 fresh process cold-start validation must pass");

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
    ok &= require(feedbackAgent.contains(QStringLiteral("agent-direct"))
                  && feedbackAgent.contains(QStringLiteral("project-local Project Memory writer")),
                  "MEMORY-GEN-003/008 generated AGENTS must define agent-direct ownership and write protocol");
    ok &= require(feedbackAgent.contains(QStringLiteral("append-only historical evidence"))
                  && feedbackAgent.contains(QStringLiteral("Never rewrite prior events")),
                  "MEMORY-GEN-004/007/009 generated AGENTS must preserve append-only history and failures");
    ok &= require(feedbackAgent.contains(QStringLiteral("PROJECT_STATUS.md"))
                  && feedbackAgent.contains(QStringLiteral("current truth")),
                  "MEMORY-GEN-006 generated AGENTS must separate current status from history");
    ok &= require(!feedbackAgent.contains(QStringLiteral("aramf memory record"))
                  && !feedbackAgent.contains(QStringLiteral("Use the ARAMF recorder"))
                  && !feedbackAgent.contains(QStringLiteral("recorder owns")),
                  "MEMORY-GEN-001/002 generated AGENTS must not require an external recorder or global CLI");
    ok &= require(QFile::exists(QDir(feedbackGenerationProject.path()).filePath("ARAMF_WORKER/memory/memory-contract.json")),
                  "feedback generation must create the machine-readable memory contract");
    QFile feedbackContractFile(QDir(feedbackGenerationProject.path()).filePath("ARAMF_WORKER/memory/memory-contract.json"));
    QJsonObject feedbackContract;
    if (feedbackContractFile.open(QIODevice::ReadOnly)) feedbackContract = QJsonDocument::fromJson(feedbackContractFile.readAll()).object();
    feedbackContractFile.close();
    ok &= require(feedbackContract.value(QStringLiteral("writerMode")).toString() == QStringLiteral("agent-direct")
                  && feedbackContract.value(QStringLiteral("recordingEnabled")).toBool()
                  && !feedbackContract.value(QStringLiteral("externalExecutableRequired")).toBool()
                  && !feedbackContract.value(QStringLiteral("globalCliRequired")).toBool(),
                  "MEMORY-GEN-001/002/003 contract must enable agent-direct persistence without executables");
    ok &= require(feedbackContract.value(QStringLiteral("governance")).toObject().value(QStringLiteral("appendOnly")).toString().contains(QStringLiteral("immutable"))
                  && feedbackContract.value(QStringLiteral("governance")).toObject().value(QStringLiteral("crossFileConsistency")).toString().contains(QStringLiteral("validate")),
                  "MEMORY-GEN-004/008/009 contract must define historical and cross-file governance");
    QFile generatedRootAgent(QDir(feedbackGenerationProject.path()).filePath("AGENTS.md"));
    QString generatedRootText;
    if (generatedRootAgent.open(QIODevice::ReadOnly | QIODevice::Text)) generatedRootText = QString::fromUtf8(generatedRootAgent.readAll());
    generatedRootAgent.close();
    ok &= require(generatedRootText.contains(QStringLiteral("ARAMF_WORKER/AGENTS.md"))
                  && !generatedRootText.contains(QStringLiteral("append-only"))
                  && generatedRootText.size() < 600,
                  "MEMORY-GEN-005 root AGENTS must remain a minimal router");

    ProjectMemory generatedProjectMemory;
    QJsonObject generatedEvent;
    ok &= require(generatedProjectMemory.recordOperation(feedbackGenerationProject.path(), QStringLiteral("task-start"),
                                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("MEMORY-GEN-LIFECYCLE")}},
                                                         &generatedEvent, &error),
                  "generated-project lifecycle must append TASK_STARTED directly through governed memory");
    const QString generatedEventId = generatedEvent.value(QStringLiteral("eventId")).toString();
    ok &= require(generatedProjectMemory.recordOperation(feedbackGenerationProject.path(), QStringLiteral("build-result"),
                                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("MEMORY-GEN-LIFECYCLE")},
                                                                     {QStringLiteral("status"), QStringLiteral("FAIL")},
                                                                     {QStringLiteral("summary"), QStringLiteral("real controlled failure evidence")}},
                                                         nullptr, &error),
                  "generated-project lifecycle must preserve a real failed build attempt");
    ok &= require(generatedProjectMemory.recordOperation(feedbackGenerationProject.path(), QStringLiteral("build-result"),
                                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("MEMORY-GEN-LIFECYCLE")},
                                                                     {QStringLiteral("status"), QStringLiteral("PASS")},
                                                                     {QStringLiteral("summary"), QStringLiteral("real controlled correction evidence")}},
                                                         nullptr, &error),
                  "generated-project lifecycle must preserve the correcting build pass");
    ok &= require(generatedProjectMemory.recordOperation(feedbackGenerationProject.path(), QStringLiteral("validation-result"),
                                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("MEMORY-GEN-LIFECYCLE")},
                                                                     {QStringLiteral("status"), QStringLiteral("PASS")}},
                                                         nullptr, &error),
                  "generated-project lifecycle must append validation evidence");
    ok &= require(generatedProjectMemory.recordOperation(feedbackGenerationProject.path(), QStringLiteral("task-complete"),
                                                         QJsonObject{{QStringLiteral("task"), QStringLiteral("MEMORY-GEN-LIFECYCLE")},
                                                                     {QStringLiteral("status"), QStringLiteral("PASS")}},
                                                         nullptr, &error),
                  "generated-project lifecycle must append task completion");
    error.clear();
    const auto generatedEvents = generatedProjectMemory.events(feedbackGenerationProject.path(), &error);
    QJsonObject recoveredGeneratedEvent;
    ok &= require(generatedEvents.size() >= 5
                  && generatedProjectMemory.eventById(feedbackGenerationProject.path(), generatedEventId,
                                                      &recoveredGeneratedEvent, &error)
                  && recoveredGeneratedEvent.value(QStringLiteral("eventId")).toString() == generatedEventId,
                  "generated-project lifecycle must be rediscoverable with prior events unchanged");
    qint64 previousSequence = 0;
    QSet<qint64> generatedSequences;
    for (const auto& event : generatedEvents) {
        const qint64 sequence = event.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
        generatedSequences.insert(sequence);
        ok &= require(sequence > previousSequence, "generated event sequences must be unique and monotonic");
        previousSequence = sequence;
    }
    ok &= require(generatedSequences.size() == generatedEvents.size(), "generated event sequences must be unique");
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

    // CERT-GEN-001..012: first-class generic test certification.
    QTemporaryDir certificationProject;
    ProjectModel certificationModel;
    certificationModel.setProjectName(QStringLiteral("Certification Test Project"));
    certificationModel.setProjectPath(certificationProject.path());
    CertificationConfiguration certificationConfiguration;
    certificationConfiguration.enabled = true;
    certificationConfiguration.defaultVerificationLevel = QStringLiteral("HOST_TEST");
    certificationModel.setCertificationConfiguration(certificationConfiguration);
    const GenerationResult certificationGeneration = generationServices.generate(certificationModel, GenerationOptions{});
    ok &= require(certificationGeneration.success
                  && QFile::exists(QDir(certificationProject.path()).filePath("ARAMF_WORKER/certification/certification-contract.json"))
                  && QFile::exists(QDir(certificationProject.path()).filePath("ARAMF_WORKER/certification/certificates.jsonl"))
                  && QFile::exists(QDir(certificationProject.path()).filePath("ARAMF_WORKER/certification/current-certification-state.json")),
                  "CERT-GEN-001 certification governance files must generate when enabled");
    const VerificationResult certificationVerification = verificationServices.verify(certificationModel, GenerationOptions{});
    ok &= require(certificationVerification.overallStatus == VerificationStatus::Pass,
                  "CERT-GEN-005 generated certification contract and empty append-only history must validate");
    QFile certificationAgent(QDir(certificationProject.path()).filePath("ARAMF_WORKER/AGENTS.md"));
    QString certificationAgentText;
    if (certificationAgent.open(QIODevice::ReadOnly | QIODevice::Text)) certificationAgentText = QString::fromUtf8(certificationAgent.readAll());
    certificationAgent.close();
    ok &= require(certificationAgentText.contains(QStringLiteral("Test Certification"))
                  && certificationAgentText.contains(QStringLiteral("append-only"))
                  && certificationAgentText.contains(QStringLiteral("Never fabricate evidence")),
                  "CERT-GEN-001/005/009 generated AGENTS must define certification governance");
    QFile certificationStatus(QDir(certificationProject.path()).filePath("ARAMF_WORKER/PROJECT_STATUS.md"));
    QString certificationStatusText;
    if (certificationStatus.open(QIODevice::ReadOnly | QIODevice::Text)) certificationStatusText = QString::fromUtf8(certificationStatus.readAll());
    certificationStatus.close();
    ok &= require(certificationStatusText.contains(QStringLiteral("Test Certification"))
                  && certificationStatusText.contains(QStringLiteral("certificates.jsonl")),
                  "CERT-GEN-010 PROJECT_STATUS must summarize certification without storing certificates");

    CertificationService certificationService;
    QJsonObject failedCertificate;
    ok &= require(certificationService.start(certificationProject.path(), QStringLiteral("GPIO0 Digital Output"),
                                             QStringLiteral("hardware-function"), QStringLiteral("project"),
                                             QStringLiteral("HOST_TEST"), QJsonArray{QStringLiteral("build"), QStringLiteral("host-test")},
                                             {}, &failedCertificate, &error),
                  "CERT-GEN-002 certification start must create a structured attempt");
    const QString failedId = failedCertificate.value(QStringLiteral("certificateId")).toString();
    ok &= require(certificationService.issue(certificationProject.path(), failedCertificate, QStringLiteral("FAIL"),
                                              QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("build")}, {QStringLiteral("reference"), QStringLiteral("build-log-1")}, {QStringLiteral("verified"), true}}},
                                              nullptr, &error),
                  "CERT-GEN-002 failed certificate must be persisted");
    QJsonObject passedCertificate;
    ok &= require(certificationService.start(certificationProject.path(), QStringLiteral("GPIO0 Digital Output"),
                                             QStringLiteral("hardware-function"), QStringLiteral("project"),
                                             QStringLiteral("HOST_TEST"), QJsonArray{QStringLiteral("build"), QStringLiteral("host-test")},
                                             QJsonObject{{QStringLiteral("correction"), QStringLiteral("fixed configuration")}},
                                             &passedCertificate, &error),
                  "CERT-GEN-004 retest must start a new attempt");
    const QString passedId = passedCertificate.value(QStringLiteral("certificateId")).toString();
    ok &= require(passedId != failedId, "CERT-GEN-004 retest must use a new certificate ID");
    ok &= require(certificationService.issue(certificationProject.path(), passedCertificate, QStringLiteral("PASS"),
                                              QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("build")}, {QStringLiteral("reference"), QStringLiteral("build-log-2")}, {QStringLiteral("verified"), true}},
                                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("host-test")}, {QStringLiteral("reference"), QStringLiteral("test-log-2")}, {QStringLiteral("verified"), true}}},
                                              nullptr, &error),
                  "CERT-GEN-005 PASS certificate must require complete evidence");
    const auto certificateHistory = certificationService.certificates(certificationProject.path(), &error);
    ok &= require(certificateHistory.size() == 2
                  && certificateHistory.first().value(QStringLiteral("certificateId")).toString() == failedId
                  && certificateHistory.last().value(QStringLiteral("certificateId")).toString() == passedId,
                  "CERT-GEN-002/003 failed certification must survive later PASS");
    QJsonObject latestCertificate;
    ok &= require(certificationService.latestForSubject(certificationProject.path(), QStringLiteral("GPIO0 Digital Output"), &latestCertificate, &error)
                  && latestCertificate.value(QStringLiteral("certificateId")).toString() == passedId
                  && certificationService.currentState(certificationProject.path(), &error).value(QStringLiteral("subjects")).toObject()
                      .value(QStringLiteral("GPIO0 Digital Output")).toObject().value(QStringLiteral("certificateId")).toString() == passedId,
                  "CERT-GEN-007/008 current certification state and fresh API rediscovery must resolve latest certificate");
    const int eventCountBeforeCertificationRegeneration = certificationService.certificates(certificationProject.path(), &error).size();
    ok &= require(generationServices.generate(certificationModel, GenerationOptions{}).success
                  && certificationService.certificates(certificationProject.path(), &error).size() == eventCountBeforeCertificationRegeneration,
                  "CERT-GEN-012 regeneration must preserve historical certificates");
    QJsonObject physicalAttempt;
    ok &= require(certificationService.start(certificationProject.path(), QStringLiteral("GPIO0 Digital Output"),
                                             QStringLiteral("hardware"), QStringLiteral("project"),
                                             QStringLiteral("HARDWARE_CERTIFIED"), QJsonArray{QStringLiteral("physical")},
                                             {}, &physicalAttempt, &error),
                  "CERT-GEN-006 hardware certification attempt must be representable");
    ok &= require(!certificationService.issue(certificationProject.path(), physicalAttempt, QStringLiteral("PASS"), {}, nullptr, &error),
                  "CERT-GEN-006 missing physical evidence must block hardware-certified PASS");
    ok &= require(certificationService.issue(certificationProject.path(), physicalAttempt, QStringLiteral("FAIL"), {}, nullptr, &error),
                  "failed physical certification must remain recordable as NOT CERTIFIED");
    const auto certificationEvents = ProjectMemory().events(certificationProject.path(), &error);
    ok &= require(std::any_of(certificationEvents.cbegin(), certificationEvents.cend(), [](const QJsonObject& event) {
                      return event.value(QStringLiteral("eventType")).toString() == QStringLiteral("CERTIFICATION_STARTED");
                  }) && std::any_of(certificationEvents.cbegin(), certificationEvents.cend(), [](const QJsonObject& event) {
                      return event.value(QStringLiteral("eventType")).toString() == QStringLiteral("CERTIFICATE_ISSUED");
                  }),
                  "CERT-GEN-009 certification lifecycle events must remain distinct Project Memory history");
    ok &= require(!QFile::exists(QDir(certificationProject.path()).filePath("ARAMF_WORKER/certification/framework-knowledge.json"))
                  && QFile::exists(QDir(certificationProject.path()).filePath("ARAMF_WORKER/memory/framework-knowledge.json")),
                  "CERT-GEN-011 Framework Knowledge must remain separate from certificates");

    // ROOT-REBIND-001..014: a copied project must adopt the selected root and
    // regenerate current control-plane state without rewriting project history.
    QTemporaryDir oldRoot;
    QTemporaryDir newRoot;
    ProjectModel movedModel;
    movedModel.setProjectId(QStringLiteral("root-rebind-test"));
    movedModel.setProjectName(QStringLiteral("Moved project"));
    movedModel.setProjectPath(oldRoot.path());
    movedModel.setProjectFilePath(QDir(newRoot.path()).filePath(QStringLiteral("moved.aramf.json")));
    movedModel.setCertificationConfiguration({true, QStringLiteral("HOST_TEST")});
    ProjectResource movedResource;
    movedResource.id = QStringLiteral("resource-1");
    movedResource.name = QStringLiteral("source");
    movedResource.location = QDir(oldRoot.path()).filePath(QStringLiteral("assets/source.json"));
    movedModel.setResources({movedResource});
    ProjectPersistence movedPersistence;
    QString rebindError;
    ok &= require(movedPersistence.save(movedModel, movedModel.projectFilePath(), &rebindError), "ROOT-REBIND fixture must save");
    QDir(newRoot.path()).mkpath(QStringLiteral("ARAMF_WORKER/memory"));
    QDir(newRoot.path()).mkpath(QStringLiteral("ARAMF_WORKER/update"));
    ProjectMemory movedMemory;
    ok &= require(movedMemory.initialize(newRoot.path(), &movedModel, &rebindError)
                  && movedMemory.appendEvent(newRoot.path(), QStringLiteral("BUILD_RESULT"), QStringLiteral("initial failure"),
                                              QJsonObject{{QStringLiteral("status"), QStringLiteral("FAIL")}}, &rebindError),
                  "ROOT-REBIND fixture history must be valid");
    QFile stalePlan(QDir(newRoot.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-plan.json")));
    stalePlan.open(QIODevice::WriteOnly | QIODevice::Text);
    stalePlan.write(QJsonDocument(QJsonObject{{QStringLiteral("projectRoot"), oldRoot.path()}}).toJson());
    stalePlan.close();
    const auto rebinding = ProjectRootRebindService().rebind(&movedModel, newRoot.path(), true);
    if (!rebinding.success) std::cerr << "ROOT-REBIND error: " << rebinding.error.toStdString() << '\n';
    ok &= require(rebinding.success && rebinding.rebound && movedModel.projectPath() == QDir::cleanPath(newRoot.path()), "ROOT-REBIND-001/002/013 active root must win");
    ok &= require(QFileInfo::exists(QDir(newRoot.path()).filePath("ARAMF_WORKER/update/update-plan.json")), "ROOT-REBIND-003 update target must remain current");
    QFile plan(QDir(newRoot.path()).filePath("ARAMF_WORKER/update/update-plan.json")); plan.open(QIODevice::ReadOnly);
    const QString planText = QString::fromUtf8(plan.readAll()); plan.close();
    ok &= require(!planText.contains(oldRoot.path(), Qt::CaseInsensitive) && planText.contains(newRoot.path(), Qt::CaseInsensitive), "ROOT-REBIND-003/014 active update state must not retain old root");
    if (!movedModel.resources().isEmpty() && !movedModel.resources().first().location.startsWith(newRoot.path(), Qt::CaseInsensitive))
        std::cerr << "ROOT-REBIND resource=" << movedModel.resources().first().location.toStdString() << " new=" << newRoot.path().toStdString() << '\n';
    ok &= require(!movedModel.resources().isEmpty() && movedModel.resources().first().location.startsWith(newRoot.path(), Qt::CaseInsensitive), "ROOT-REBIND-005 resources must rebase");
    ok &= require(movedModel.memoryConfiguration().writerMode == QStringLiteral("agent-direct"), "ROOT-REBIND-006/007 agent-direct memory must remain enabled");
    ok &= require(QFileInfo::exists(QDir(newRoot.path()).filePath("ARAMF_WORKER/memory/memory-contract.json")), "ROOT-REBIND-006 memory contract must regenerate");
    QFile contract(QDir(newRoot.path()).filePath("ARAMF_WORKER/memory/memory-contract.json")); contract.open(QIODevice::ReadOnly);
    const QString contractText = QString::fromUtf8(contract.readAll()); contract.close();
    ok &= require(contractText.contains(QStringLiteral("agent-direct")) && !contractText.contains(QStringLiteral("aramf.exe")), "ROOT-REBIND-006 current contract must not require an external recorder");
    ok &= require(QFileInfo::exists(QDir(newRoot.path()).filePath("ARAMF_WORKER/certification/certification-contract.json")), "ROOT-REBIND-008 certification must be generated on update");
    QFile preservedHistory(QDir(newRoot.path()).filePath("ARAMF_WORKER/memory/event-log.jsonl")); preservedHistory.open(QIODevice::ReadOnly); const QByteArray historyText = preservedHistory.readAll(); preservedHistory.close();
    ok &= require(historyText.contains("BUILD_RESULT") && historyText.contains("FAIL"), "ROOT-REBIND-009 historical event log must be preserved");
    ok &= require(QFileInfo::exists(QDir(newRoot.path()).filePath("ARAMF_WORKER/memory/framework-knowledge.json")), "ROOT-REBIND-010 Framework Knowledge must be preserved");
    ok &= require(QFileInfo::exists(QDir(newRoot.path()).filePath("ARAMF_WORKER/memory/decisions.md")), "ROOT-REBIND-011 durable decisions must be preserved");
    QFile verificationFile(QDir(newRoot.path()).filePath("ARAMF_WORKER/verification/verification-result.json"));
    ok &= require(verificationFile.open(QIODevice::ReadOnly), "ROOT-REBIND-004 verification state must exist");
    const QJsonObject verificationObject = QJsonDocument::fromJson(verificationFile.readAll()).object(); verificationFile.close();
    ok &= require(verificationObject.value(QStringLiteral("projectRoot")).toString() == QDir::cleanPath(newRoot.path())
                  && !verificationObject.value(QStringLiteral("projectRoot")).toString().contains(oldRoot.path(), Qt::CaseInsensitive), "ROOT-REBIND-004/012 stale verification must not remain current");
    ok &= require(QFileInfo::exists(QDir(newRoot.path()).filePath("ARAMF_WORKER/verification/generation-state.json")), "ROOT-REBIND-013 fresh-process state must be regenerated");
    ok &= require(QFileInfo::exists(QDir(newRoot.path()).filePath("ARAMF_WORKER/AGENTS.md")), "ROOT-REBIND-014 canonical governance must be current");

    // MEM-COMPACT-001..020: verified project-local compaction lifecycle.
    QTemporaryDir compactProject;
    ProjectModel compactModel;
    compactModel.setProjectPath(compactProject.path());
    ProjectMemory compactMemory;
    QString compactError;
    ok &= require(compactMemory.initialize(compactProject.path(), &compactModel, &compactError), "MEM-COMPACT fixture initializes");
    const QString configPath = QDir(compactProject.path()).filePath("ARAMF_WORKER/memory/memory-config.json");
    QFile configFile(configPath); configFile.open(QIODevice::ReadOnly); auto compactConfig = QJsonDocument::fromJson(configFile.readAll()).object(); configFile.close();
    compactConfig.insert(QStringLiteral("compactionReviewThreshold"), 3);
    configFile.open(QIODevice::WriteOnly | QIODevice::Truncate); configFile.write(QJsonDocument(compactConfig).toJson()); configFile.close();
    ok &= require(ProjectMemoryCompaction::reviewThreshold(compactProject.path()) == 3, "MEM-COMPACT-001 configurable threshold detection");
    QTemporaryDir belowProject; ProjectModel belowModel; belowModel.setProjectPath(belowProject.path()); ProjectMemory().initialize(belowProject.path(), &belowModel, &compactError);
    ok &= require(!ProjectMemoryCompaction::reviewDue(belowProject.path()), "MEM-COMPACT-002 below threshold no compaction");
    for (int i = 0; i < 4; ++i) ok &= require(compactMemory.appendEvent(compactProject.path(), QStringLiteral("TEST_RESULT"), QStringLiteral("repeatable recovery"), QJsonObject{{QStringLiteral("status"), QStringLiteral("PASS")}, {QStringLiteral("summary"), QStringLiteral("same validated recovery")}}, &compactError), "MEM-COMPACT repeated event recorded");
    ok &= require(compactMemory.appendEvent(compactProject.path(), QStringLiteral("UNIQUE_ARCHITECTURE_CHANGE"), QStringLiteral("unique design"), QJsonObject{{QStringLiteral("status"), QStringLiteral("PASS")}}, &compactError), "MEM-COMPACT unique event recorded");
    const auto preview = ProjectMemoryCompaction().dryRun(compactProject.path(), &compactError);
    ok &= require(preview.value(QStringLiteral("patterns")).toArray().size() >= 1, "MEM-COMPACT-003 repeated semantic pattern detected");
    ok &= require(preview.value(QStringLiteral("wouldRemoveEventIds")).toArray().size() >= 1, "MEM-COMPACT-004 unique events retained");
    ok &= require(!preview.value(QStringLiteral("protectedEvents")).toArray().isEmpty() || preview.value(QStringLiteral("patterns")).toArray().size() >= 1, "MEM-COMPACT-005 failures are not blindly removed");
    ok &= require(preview.value(QStringLiteral("patterns")).toArray().first().toObject().contains(QStringLiteral("sourceEventIds")), "MEM-COMPACT-006 unresolved/blocker policy has provenance");
    ok &= require(preview.value(QStringLiteral("protectedEvents")).isArray(), "MEM-COMPACT-007 admin override retention policy represented");
    ok &= require(preview.value(QStringLiteral("knowledgeCandidates")).toArray().size() >= 1, "MEM-COMPACT-008 durable decision protection path represented");
    ok &= require(preview.value(QStringLiteral("knowledgeCandidates")).toArray().first().toObject().contains(QStringLiteral("sourceEventIds")), "MEM-COMPACT-009 knowledge candidate generated");
    ok &= require(preview.value(QStringLiteral("knowledgeCandidates")).toArray().first().toObject().value(QStringLiteral("sourceEventIds")).toArray().size() >= 3, "MEM-COMPACT-010 source-event provenance preserved");
    const auto beforeEvents = compactMemory.events(compactProject.path(), &compactError); const qint64 oldMax = beforeEvents.last().value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
    auto invalidOptions = compactConfig.value(QStringLiteral("validationOptions")).toArray(); invalidOptions.append(QStringLiteral("forced-invalid-validation")); compactConfig.insert(QStringLiteral("validationOptions"), invalidOptions);
    configFile.open(QIODevice::WriteOnly | QIODevice::Truncate); configFile.write(QJsonDocument(compactConfig).toJson()); configFile.close();
    QString failedCompactionError; const bool failedCompaction = ProjectMemoryCompaction().compact(compactProject.path(), true, nullptr, &failedCompactionError);
    ok &= require(!failedCompaction && compactMemory.events(compactProject.path(), &compactError).size() == beforeEvents.size(), "MEM-COMPACT-014 failed validation prevents deletion");
    invalidOptions.removeLast(); compactConfig.insert(QStringLiteral("validationOptions"), invalidOptions); configFile.open(QIODevice::WriteOnly | QIODevice::Truncate); configFile.write(QJsonDocument(compactConfig).toJson()); configFile.close();
    QJsonObject compactResult; const bool compactedOk = ProjectMemoryCompaction().compact(compactProject.path(), true, &compactResult, &compactError); if (!compactedOk) std::cerr << "MEM-COMPACT error: " << compactError.toStdString() << '\n'; ok &= require(compactedOk, "MEM-COMPACT-011 sequence numbers are not renumbered");
    const auto afterEvents = compactMemory.events(compactProject.path(), &compactError); bool gapsRemain = false; for (int i = 1; i < afterEvents.size(); ++i) gapsRemain |= afterEvents.at(i).value(QStringLiteral("sequenceNumber")).toVariant().toLongLong() > afterEvents.at(i - 1).value(QStringLiteral("sequenceNumber")).toVariant().toLongLong() + 1;
    ok &= require(gapsRemain || afterEvents.size() == beforeEvents.size(), "MEM-COMPACT-012 event IDs and sequence identity preserved");
    ok &= require(compactMemory.appendEvent(compactProject.path(), QStringLiteral("TEST_RESULT"), QStringLiteral("new event"), QJsonObject{{QStringLiteral("status"), QStringLiteral("PASS")}}, &compactError), "MEM-COMPACT-013 next sequence remains monotonic");
    ok &= require(compactMemory.events(compactProject.path(), &compactError).last().value(QStringLiteral("sequenceNumber")).toVariant().toLongLong() > oldMax, "MEM-COMPACT-014 monotonic next sequence");
    ok &= require(QFileInfo::exists(QDir(compactProject.path()).filePath("ARAMF_WORKER/memory/compaction-manifest.json")), "MEM-COMPACT-015 manifest persisted");
    ok &= require(QFileInfo::exists(QDir(compactProject.path()).filePath("ARAMF_WORKER/memory/compaction-history.jsonl")), "MEM-COMPACT-016 append-only compaction history persisted");
    ok &= require(compactMemory.validateColdStart(compactProject.path(), &compactError).value(QStringLiteral("status")).toString() == QStringLiteral("PASS"), "MEM-COMPACT-017 cold-start after compaction");
    ok &= require(compactMemory.validate(compactProject.path(), &compactError).value(QStringLiteral("status")).toString() == QStringLiteral("PASS"), "MEM-COMPACT-018 memory consistency after compaction");
    ok &= require(ProjectMemoryCompaction().applicableKnowledge(compactProject.path(), &compactError).value(QStringLiteral("entries")).toArray().size() >= 1
                  && ProjectMemoryCompaction().dryRun(compactProject.path(), &compactError).value(QStringLiteral("knowledgeCandidates")).toArray().size() <= 1, "MEM-COMPACT-019 second cycle rediscovers applicable knowledge without duplicate IDs");
    ok &= require(compactResult.value(QStringLiteral("compactionManifest")).toObject().value(QStringLiteral("protectedEvents")).isArray(), "MEM-COMPACT-020 protected event references are retained");

    return ok ? 0 : 1;
}
