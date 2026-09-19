#include "core/ContextCoordinationService.h"
#include "core/ExecutionOrchestrator.h"
#include "core/ProjectPersistence.h"
#include "core/RuntimeOwnershipService.h"
#include "core/Services.h"
#include "core/WorkerTaskServices.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <thread>

namespace {
bool writeFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

QString sha256(const QByteArray& content)
{
    return QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
}

void configureFixture(ProjectModel& model, const QString& path)
{
    model.setProjectId(QStringLiteral("p2-canonical-integration"));
    model.setProjectPath(path);
    // Production Worker projects use the resolved Worker directory name as
    // their project identity; keeping the fixture aligned makes save/reload
    // exercise the same canonical configuration fingerprint.
    model.setProjectName(QStringLiteral("ARAMF_WORKER"));
    auto ai = model.aiConfiguration();
    ai.permissions = {QStringLiteral("read-project-files"), QStringLiteral("modify-files"), QStringLiteral("create-files")};
    model.setAiConfiguration(ai);

    const QStringList scopes{
        QStringLiteral("database"), QStringLiteral("ui"), QStringLiteral("integration"),
        QStringLiteral("regression"), QStringLiteral("parallel-a"), QStringLiteral("parallel-b"),
        QStringLiteral("worker-loss"), QStringLiteral("shared"), QStringLiteral("retry"),
        QStringLiteral("checkpoint-1"), QStringLiteral("checkpoint-2"), QStringLiteral("canonical"),
        QStringLiteral("deadlock"), QStringLiteral("governance"), QStringLiteral("permanent"),
        QStringLiteral("validation-gate")};
    auto rules = model.ruleConfiguration();
    rules.projectScopes = scopes;
    rules.scopeMetadata = {};
    for (const auto& scope : scopes) {
        const QString file = QStringLiteral("source/") + scope + QStringLiteral(".cpp");
        writeFile(QDir(path).filePath(file), QByteArrayLiteral("baseline\n"));
        rules.scopeMetadata.insert(scope, QJsonObject{
            {QStringLiteral("files"), QJsonArray{file}},
            {QStringLiteral("tests"), QJsonArray{scope + QStringLiteral("-tests")}},
            {QStringLiteral("riskTraits"), QJsonArray{}},
            {QStringLiteral("affects"), QJsonArray{}},
            {QStringLiteral("generatedArtifacts"), QJsonArray{}}});
    }
    model.setRuleConfiguration(rules);
    const auto generated = GenerationServices().generate(model, model.generationOptions());
    if (!generated.success) std::cerr << "P2 fixture generation: " << generated.error.toStdString() << '\n';
    VerificationServices().verify(model, model.generationOptions());
}

ExecutionTask taskFor(const ProjectModel& model, const QString& id,
                      const QString& scope, const QStringList& dependencies = {},
                      int retryLimit = 0)
{
    const QString resource = QStringLiteral("source/") + scope + QStringLiteral(".cpp");
    WorkerTaskRequest request;
    request.goal = QStringLiteral("Execute canonical P2 task ") + id;
    request.type = QStringLiteral("coding");
    request.scopes = {scope};
    request.files = {resource};
    request.definitionOfDone = {QStringLiteral("canonical validation passes")};
    ExecutionTask result;
    result.id = id;
    result.dependencies = dependencies;
    result.resources = {resource};
    result.contract = WorkerTaskServices::prepare(model, request);
    result.contractId = result.contract.value(QStringLiteral("contractId")).toString();
    result.contextFingerprint = ContextCoordinationService::route(model, {scope})
        .value(QStringLiteral("indexFingerprint")).toString();
    result.retryLimit = retryLimit;
    return result;
}

ExecutionTask refreshTaskContract(const ProjectModel& model, ExecutionTask task)
{
    const auto request = WorkerTaskRequest::fromJson(task.contract.value(QStringLiteral("request")).toObject());
    task.contract = WorkerTaskServices::prepare(model, request);
    task.contractId = task.contract.value(QStringLiteral("contractId")).toString();
    task.contextFingerprint = ContextCoordinationService::route(model, request.scopes)
        .value(QStringLiteral("indexFingerprint")).toString();
    return task;
}

QList<ExecutionTask> refreshTaskContracts(const ProjectModel& model, QList<ExecutionTask> tasks)
{
    for (auto& task : tasks) task = refreshTaskContract(model, task);
    return tasks;
}

bool saveCanonicalDag(const ProjectModel& model, const QList<ExecutionTask>& tasks)
{
    QJsonArray nodes;
    for (const auto& task : tasks)
        nodes.append(QJsonObject{{QStringLiteral("id"), task.id},
                                 {QStringLiteral("dependencies"), QJsonArray::fromStringList(task.dependencies)},
                                 {QStringLiteral("state"), QStringLiteral("PENDING")}});
    return ContextCoordinationService::saveTaskDag(model, nodes, {})
        .value(QStringLiteral("valid")).toBool();
}

QJsonObject executionResultFor(const ProjectModel& model, const ExecutionTask& task,
                               const QString& artifactPrefix, bool passing = true)
{
    const auto initial = WorkerTaskServices::postflight(model, task.contract);
    QJsonArray evidence;
    for (const auto& required : task.contract.value(QStringLiteral("requiredEvidence")).toArray()) {
        const QString check = required.toString();
        if (check == QStringLiteral("diff-boundary") || check == QStringLiteral("physical-certification")) continue;
        const QString path = QStringLiteral("ARAMF_WORKER/verification/tasks/%1-%2.json").arg(artifactPrefix, check);
        const QJsonObject record{
            {QStringLiteral("check"), check}, {QStringLiteral("status"), passing ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("contractId"), task.contractId},
            {QStringLiteral("dependencyFingerprint"), initial.value(QStringLiteral("evidenceFingerprints")).toObject().value(check)},
            {QStringLiteral("resultFingerprint"), initial.value(QStringLiteral("resultFingerprint"))}};
        const QByteArray bytes = QJsonDocument(record).toJson(QJsonDocument::Compact);
        writeFile(QDir(model.projectPath()).filePath(path), bytes);
        evidence.append(QJsonObject{{QStringLiteral("check"), check}, {QStringLiteral("artifact"), path},
                                    {QStringLiteral("artifactFingerprint"), sha256(bytes)}});
    }
    return QJsonObject{{QStringLiteral("executionStatus"), QStringLiteral("PASS")},
                       {QStringLiteral("evidence"), evidence},
                       {QStringLiteral("producedArtifacts"), QJsonArray{}},
                       {QStringLiteral("continuationRequirements"), QJsonArray{QStringLiteral("preserve scoped P1 context")}}};
}

bool check(bool condition, const QString& message)
{
    if (!condition) std::cerr << "P2 FAIL: " << message.toStdString() << '\n';
    return condition;
}
}

bool runP2ExecutionTests()
{
    bool ok = true;
    QString error;
    QTemporaryDir project;
    ProjectModel model;
    ProjectPersistence persistence;
    configureFixture(model, project.path());

    const auto t1Draft = taskFor(model, QStringLiteral("T1"), QStringLiteral("database"));
    const auto t2Draft = taskFor(model, QStringLiteral("T2"), QStringLiteral("ui"));
    const auto t3Draft = taskFor(model, QStringLiteral("T3"), QStringLiteral("integration"), {QStringLiteral("T1"), QStringLiteral("T2")});
    const auto t4Draft = taskFor(model, QStringLiteral("T4"), QStringLiteral("regression"), {QStringLiteral("T3")});
    const QList<ExecutionTask> integrationDraft{t1Draft, t2Draft, t3Draft, t4Draft};
    ok &= check(saveCanonicalDag(model, integrationDraft), QStringLiteral("P1 canonical DAG is created"));
    const auto integrationTasks = refreshTaskContracts(model, integrationDraft);
    const auto t1 = integrationTasks.at(0);
    const auto t2 = integrationTasks.at(1);
    const auto t3 = integrationTasks.at(2);
    const auto t4 = integrationTasks.at(3);
    ExecutionOrchestrator orchestrator(&model);
    ok &= check(orchestrator.configure(QStringLiteral("integration"), integrationTasks,
                                       {ExecutionWorker{QStringLiteral("worker-a")}, ExecutionWorker{QStringLiteral("worker-b")}}, &error),
                QStringLiteral("integration DAG configures: %1").arg(error));
    ok &= check(orchestrator.start(&error) && orchestrator.readyTaskIds() == QStringList{QStringLiteral("T1"), QStringLiteral("T2")},
                QStringLiteral("P1 roots are deterministically ready"));

    const auto finish = [&](ExecutionOrchestrator& runner, const QString& workerId, const QString& taskId) {
        const auto snapshot = runner.snapshot();
        const auto it = std::find_if(snapshot.tasks.cbegin(), snapshot.tasks.cend(), [&taskId](const auto& value) { return value.id == taskId; });
        if (it == snapshot.tasks.cend()) return false;
        if (!runner.claimTask(workerId, taskId, &error)) return false;
        if (!runner.startTask(workerId, taskId, &error)) return false;
        if (!runner.completeTask(workerId, taskId, executionResultFor(model, *it, QStringLiteral("integration-%1").arg(taskId)), &error)) return false;
        return true;
    };
    ok &= check(finish(orchestrator, QStringLiteral("worker-a"), QStringLiteral("T1")), QStringLiteral("T1 crosses P0 and completes"));
    const auto t1Handoff = orchestrator.snapshot().tasks.first().handoff;
    ok &= check(t1Handoff.value(QStringLiteral("authority")) == QStringLiteral("DERIVED")
                    && t1Handoff.value(QStringLiteral("sourceTaskId")) == QStringLiteral("T1")
                    && t1Handoff.value(QStringLiteral("validationResult")).toObject().value(QStringLiteral("status")) == QStringLiteral("PASS")
                    && t1Handoff.value(QStringLiteral("execution")).toObject().value(QStringLiteral("contextFingerprint")) == t1.contextFingerprint,
                QStringLiteral("P1 createHandoff preserves canonical execution and provenance data"));
    ok &= check(finish(orchestrator, QStringLiteral("worker-b"), QStringLiteral("T2")), QStringLiteral("T2 completes"));
    ok &= check(orchestrator.readyTaskIds() == QStringList{QStringLiteral("T3")}, QStringLiteral("T3 is released by P1 dependencies"));
    ok &= check(finish(orchestrator, QStringLiteral("worker-a"), QStringLiteral("T3")), QStringLiteral("T3 completes"));
    ok &= check(finish(orchestrator, QStringLiteral("worker-b"), QStringLiteral("T4")) && orchestrator.isComplete(), QStringLiteral("T4 completes the production DAG"));

    const auto ownership = RuntimeOwnershipService::inspect(model);
    bool released = true;
    for (const auto& value : ownership.value(QStringLiteral("claims")).toArray())
        if (value.toObject().value(QStringLiteral("taskId")).toString().startsWith(QStringLiteral("T")))
            released &= value.toObject().value(QStringLiteral("state")).toString() == QStringLiteral("RELEASED");
    ok &= check(released, QStringLiteral("P0 owns and releases every completed resource claim"));

    // P0 validation controls success; a truthy caller field cannot bypass it.
    const auto gateDraft = taskFor(model, QStringLiteral("validation-gate"), QStringLiteral("validation-gate"));
    ok &= check(saveCanonicalDag(model, {gateDraft}), QStringLiteral("validation gate DAG persists"));
    const auto gateTask = refreshTaskContract(model, gateDraft);
    ExecutionOrchestrator validationGate(&model);
    ok &= check(validationGate.configure(QStringLiteral("validation-gate"), {gateTask}, {ExecutionWorker{QStringLiteral("validator")}}, &error)
                    && validationGate.start(&error) && validationGate.claimTask(QStringLiteral("validator"), QStringLiteral("validation-gate"), &error)
                    && validationGate.startTask(QStringLiteral("validator"), QStringLiteral("validation-gate"), &error),
                QStringLiteral("validation gate enters execution"));
    ok &= check(!validationGate.completeTask(QStringLiteral("validator"), QStringLiteral("validation-gate"),
                                             QJsonObject{{QStringLiteral("validationPassed"), true}}, &error)
                    && validationGate.snapshot().tasks.first().state == TaskExecutionState::Running
                    && error == QStringLiteral("REQUIRED_EVIDENCE_MISSING")
                    && !validationGate.snapshot().tasks.first().handoff.contains(QStringLiteral("handoffId")),
                QStringLiteral("caller validation boolean cannot bypass P0 postflight"));
    error.clear();
    ok &= check(!validationGate.completeTask(QStringLiteral("validator"), QStringLiteral("validation-gate"),
                                             executionResultFor(model, gateTask, QStringLiteral("validation-failure"), false), &error)
                    && error == QStringLiteral("VALIDATION_FAILED")
                    && validationGate.snapshot().tasks.first().state == TaskExecutionState::Running,
                QStringLiteral("P0 validation failure cannot enter SUCCEEDED"));
    ok &= check(validationGate.failTask(QStringLiteral("validator"), QStringLiteral("validation-gate"), ExecutionFailureCategory::Validation, &error),
                QStringLiteral("failed validation releases P0 ownership"));

    // T1-T5 production fixture: dependencies originate in P1 and each task
    // enters through the same contract/ownership/validation/handoff path.
    const auto d1Draft = taskFor(model, QStringLiteral("T1"), QStringLiteral("database"));
    const auto d2Draft = taskFor(model, QStringLiteral("T2"), QStringLiteral("ui"));
    const auto d3Draft = taskFor(model, QStringLiteral("T3"), QStringLiteral("integration"), {QStringLiteral("T1")});
    const auto d4Draft = taskFor(model, QStringLiteral("T4"), QStringLiteral("regression"), {QStringLiteral("T1"), QStringLiteral("T2")});
    const auto d5Draft = taskFor(model, QStringLiteral("T5"), QStringLiteral("canonical"), {QStringLiteral("T3"), QStringLiteral("T4")});
    const QList<ExecutionTask> t1ToT5Draft{d1Draft, d2Draft, d3Draft, d4Draft, d5Draft};
    ok &= check(saveCanonicalDag(model, t1ToT5Draft), QStringLiteral("T1-T5 DAG is P1-owned"));
    const QList<ExecutionTask> t1ToT5 = refreshTaskContracts(model, t1ToT5Draft);
    const auto d1 = t1ToT5.at(0);
    const auto d2 = t1ToT5.at(1);
    const auto d3 = t1ToT5.at(2);
    const auto d4 = t1ToT5.at(3);
    const auto d5 = t1ToT5.at(4);
    ExecutionOrchestrator t1ToT5Runner(&model);
    ok &= check(t1ToT5Runner.configure(QStringLiteral("t1-t5"), t1ToT5,
                                        {ExecutionWorker{QStringLiteral("database-worker")}, ExecutionWorker{QStringLiteral("ui-worker")}}, &error)
                    && t1ToT5Runner.start(&error)
                    && t1ToT5Runner.readyTaskIds() == QStringList{QStringLiteral("T1"), QStringLiteral("T2")}
                    && t1ToT5Runner.blockedTaskIds().size() == 3,
                QStringLiteral("T1-T5 initial frontier follows P1"));
    ok &= check(finish(t1ToT5Runner, QStringLiteral("database-worker"), QStringLiteral("T1"))
                    && t1ToT5Runner.readyTaskIds() == QStringList{QStringLiteral("T2"), QStringLiteral("T3")},
                QStringLiteral("T3 unlocks after T1"));
    ok &= check(finish(t1ToT5Runner, QStringLiteral("ui-worker"), QStringLiteral("T2"))
                    && t1ToT5Runner.readyTaskIds() == QStringList{QStringLiteral("T3"), QStringLiteral("T4")},
                QStringLiteral("T4 unlocks after T2"));
    ok &= check(finish(t1ToT5Runner, QStringLiteral("database-worker"), QStringLiteral("T3"))
                    && finish(t1ToT5Runner, QStringLiteral("ui-worker"), QStringLiteral("T4"))
                    && t1ToT5Runner.readyTaskIds() == QStringList{QStringLiteral("T5")}
                    && finish(t1ToT5Runner, QStringLiteral("database-worker"), QStringLiteral("T5"))
                    && t1ToT5Runner.isComplete(), QStringLiteral("T1-T5 canonical chain completes"));

    // P0 ownership, not P2's task map, decides a collision.
    const auto c1Draft = taskFor(model, QStringLiteral("collision-a"), QStringLiteral("shared"));
    const auto c2Draft = taskFor(model, QStringLiteral("collision-b"), QStringLiteral("shared"));
    ok &= check(saveCanonicalDag(model, {c1Draft, c2Draft}), QStringLiteral("collision DAG persists"));
    const auto collisionTasks = refreshTaskContracts(model, {c1Draft, c2Draft});
    const auto c1 = collisionTasks.at(0);
    const auto c2 = collisionTasks.at(1);
    ExecutionOrchestrator collision(&model);
    ok &= check(collision.configure(QStringLiteral("collision"), {c1, c2},
                                    {ExecutionWorker{QStringLiteral("one")}, ExecutionWorker{QStringLiteral("two")}}, &error)
                    && collision.start(&error) && collision.claimTask(QStringLiteral("one"), QStringLiteral("collision-a"), &error),
                QStringLiteral("first task claims through P0"));
    const auto p0Claims = RuntimeOwnershipService::inspect(model).value(QStringLiteral("claims")).toArray();
    ok &= check(!collision.claimTask(QStringLiteral("two"), QStringLiteral("collision-b"), &error)
                    && error == QStringLiteral("OWNERSHIP_CONFLICT")
                    && std::any_of(p0Claims.cbegin(), p0Claims.cend(), [](const auto& value) { return value.toObject().value(QStringLiteral("state")) == QStringLiteral("ACTIVE"); }),
                QStringLiteral("P0 blocks the same-resource collision"));
    ok &= check(collision.startTask(QStringLiteral("one"), QStringLiteral("collision-a"), &error)
                    && collision.failTask(QStringLiteral("one"), QStringLiteral("collision-a"), ExecutionFailureCategory::PermanentTask, &error)
                    && collision.claimTask(QStringLiteral("two"), QStringLiteral("collision-b"), &error),
                QStringLiteral("P0 release permits the waiting task after failure"));
    collision.cancelTask(QStringLiteral("collision-b"), &error);

    // Independent resources overlap in the actual execution window.
    const auto paDraft = taskFor(model, QStringLiteral("parallel-a"), QStringLiteral("parallel-a"));
    const auto pbDraft = taskFor(model, QStringLiteral("parallel-b"), QStringLiteral("parallel-b"));
    ok &= check(saveCanonicalDag(model, {paDraft, pbDraft}), QStringLiteral("parallel DAG persists"));
    const auto parallelTasks = refreshTaskContracts(model, {paDraft, pbDraft});
    const auto pa = parallelTasks.at(0);
    const auto pb = parallelTasks.at(1);
    ExecutionOrchestrator parallel(&model);
    ok &= check(parallel.configure(QStringLiteral("parallel"), {pa, pb},
                                   {ExecutionWorker{QStringLiteral("A")}, ExecutionWorker{QStringLiteral("B")}}, &error)
                    && parallel.start(&error), QStringLiteral("parallel runner starts"));
    const QJsonObject parallelA = executionResultFor(model, pa, QStringLiteral("parallel-a"));
    const QJsonObject parallelB = executionResultFor(model, pb, QStringLiteral("parallel-b"));
    std::atomic<int> running{0};
    std::atomic<bool> overlap{false};
    std::atomic<int> entered{0};
    std::atomic<int> parallelFailures{0};
    const auto runParallel = [&](const QString& workerId, const QString& taskId, const QJsonObject& result) {
        if (!parallel.claimTask(workerId, taskId) || !parallel.startTask(workerId, taskId)) {
            ++parallelFailures;
            ++entered;
            return;
        }
        ++entered;
        const int current = ++running;
        if (current == 2) overlap = true;
        while (entered.load() < 2) std::this_thread::yield();
        if (!parallel.completeTask(workerId, taskId, result)) ++parallelFailures;
        --running;
    };
    std::thread first(runParallel, QStringLiteral("A"), QStringLiteral("parallel-a"), parallelA);
    std::thread second(runParallel, QStringLiteral("B"), QStringLiteral("parallel-b"), parallelB);
    first.join();
    second.join();
    ok &= check(overlap.load() && parallelFailures.load() == 0, QStringLiteral("independent resources execute concurrently through P0"));

    // Worker loss transitions canonical P0 claims through recoverable state.
    const auto lossDraft = taskFor(model, QStringLiteral("worker-loss"), QStringLiteral("worker-loss"), {}, 1);
    ok &= check(saveCanonicalDag(model, {lossDraft}), QStringLiteral("worker-loss DAG persists"));
    const auto loss = refreshTaskContract(model, lossDraft);
    ExecutionOrchestrator workerLoss(&model);
    ok &= check(workerLoss.configure(QStringLiteral("worker-loss"), {loss}, {ExecutionWorker{QStringLiteral("lost")}}, &error)
                    && workerLoss.start(&error) && workerLoss.claimTask(QStringLiteral("lost"), QStringLiteral("worker-loss"), &error)
                    && workerLoss.startTask(QStringLiteral("lost"), QStringLiteral("worker-loss"), &error)
                    && workerLoss.markWorkerUnavailable(QStringLiteral("lost"), &error),
                QStringLiteral("worker loss reaches canonical recovery path"));
    ok &= check(workerLoss.snapshot().tasks.first().state == TaskExecutionState::RetryPending
                    && RuntimeOwnershipService::inspect(model).value(QStringLiteral("claims")).toArray().last().toObject().value(QStringLiteral("state")) != QStringLiteral("ACTIVE"),
                QStringLiteral("worker loss does not leave an active P0 claim"));
    ok &= check(workerLoss.recover(&error), QStringLiteral("worker-loss recovery completes through P0"));

    // Retry remains dependency-aware and re-enters the same governed path.
    const auto retryDraft = taskFor(model, QStringLiteral("retry"), QStringLiteral("retry"), {}, 1);
    ok &= check(saveCanonicalDag(model, {retryDraft}), QStringLiteral("retry DAG persists"));
    const auto retryTask = refreshTaskContract(model, retryDraft);
    ExecutionOrchestrator retry(&model);
    ok &= check(retry.configure(QStringLiteral("retry"), {retryTask}, {ExecutionWorker{QStringLiteral("retry-worker")}}, &error)
                    && retry.start(&error) && retry.claimTask(QStringLiteral("retry-worker"), QStringLiteral("retry"), &error)
                    && retry.startTask(QStringLiteral("retry-worker"), QStringLiteral("retry"), &error)
                    && retry.failTask(QStringLiteral("retry-worker"), QStringLiteral("retry"), ExecutionFailureCategory::TransientInfrastructure, &error),
                QStringLiteral("transient failure enters retry state"));
    ok &= check(retry.snapshot().tasks.first().state == TaskExecutionState::RetryPending
                    && retry.snapshot().tasks.first().retryCount == 1
                    && retry.retryTask(QStringLiteral("retry"), &error)
                    && retry.readyTaskIds() == QStringList{QStringLiteral("retry")},
                QStringLiteral("retry count and ready state are durable"));
    ok &= check(retry.claimTask(QStringLiteral("retry-worker"), QStringLiteral("retry"), &error)
                    && retry.startTask(QStringLiteral("retry-worker"), QStringLiteral("retry"), &error)
                    && retry.completeTask(QStringLiteral("retry-worker"), QStringLiteral("retry"),
                                          executionResultFor(model, retryTask, QStringLiteral("retry")), &error),
                QStringLiteral("retry completes through P0 validation and P1 handoff"));

    // Checkpoint/restart reconstructs P1/P2 state and reconciles P0 claims.
    const auto checkpointFirstDraft = taskFor(model, QStringLiteral("checkpoint-1"), QStringLiteral("checkpoint-1"), {}, 1);
    const auto checkpointSecondDraft = taskFor(model, QStringLiteral("checkpoint-2"), QStringLiteral("checkpoint-2"));
    const QList<ExecutionTask> checkpointDraft{checkpointFirstDraft, checkpointSecondDraft};
    ok &= check(saveCanonicalDag(model, checkpointDraft), QStringLiteral("checkpoint DAG persists"));
    const auto checkpointTasks = refreshTaskContracts(model, checkpointDraft);
    ExecutionOrchestrator checkpoint(&model);
    ok &= check(checkpoint.configure(QStringLiteral("checkpoint"), checkpointTasks, {ExecutionWorker{QStringLiteral("checkpoint-worker")}}, &error)
                    && checkpoint.start(&error) && checkpoint.claimTask(QStringLiteral("checkpoint-worker"), QStringLiteral("checkpoint-1"), &error)
                    && checkpoint.startTask(QStringLiteral("checkpoint-worker"), QStringLiteral("checkpoint-1"), &error)
                    && checkpoint.writeCheckpoint(&error), QStringLiteral("checkpoint captures active production state"));
    const auto checkpointSnapshot = checkpoint.snapshot();
    QTemporaryDir checkpointStorage;
    const QString checkpointPath = QDir(checkpointStorage.path()).filePath(QStringLiteral("checkpoint.aramf.json"));
    ok &= check(persistence.save(model, checkpointPath, &error), QStringLiteral("checkpoint persists through project save"));
    ProjectModel restartedModel;
    ok &= check(persistence.load(&restartedModel, checkpointPath, &error), QStringLiteral("checkpoint reloads: %1").arg(error));
    ExecutionOrchestrator restarted(&restartedModel);
    const bool restored = restarted.restoreCheckpoint(restartedModel.orchestrationState(), &error);
    error.clear(); const bool retryWorkerRegistered = restarted.registerWorker(QStringLiteral("checkpoint-recovery-worker"), {}, &error);
    error.clear(); const bool retryReady = restarted.retryTask(QStringLiteral("checkpoint-1"), &error);
    const bool reconciled = !restarted.snapshot().tasks.isEmpty()
        && restarted.snapshot().tasks.first().state == TaskExecutionState::Ready;
    ExecutionTask recoveryTask;
    for (const auto& task : restarted.snapshot().tasks)
        if (task.id == QStringLiteral("checkpoint-1")) { recoveryTask = task; break; }
    error.clear(); const bool retryClaimed = restarted.claimTask(QStringLiteral("checkpoint-recovery-worker"), QStringLiteral("checkpoint-1"), &error);
    error.clear(); const bool retryStarted = restarted.startTask(QStringLiteral("checkpoint-recovery-worker"), QStringLiteral("checkpoint-1"), &error);
    ok &= check(restored && reconciled
                    && retryWorkerRegistered && retryReady && retryClaimed && retryStarted,
                QStringLiteral("restart reconciles active claim before resume: %1").arg(error));
    ExecutionTask restartedFirst;
    for (const auto& task : restarted.snapshot().tasks)
        if (task.id == QStringLiteral("checkpoint-1")) { restartedFirst = task; break; }
    const auto recoveryExecution = executionResultFor(restartedModel, restartedFirst, QStringLiteral("checkpoint-recovery"));
    const bool resumed = !restartedFirst.id.isEmpty()
        && restarted.completeTask(QStringLiteral("checkpoint-recovery-worker"), QStringLiteral("checkpoint-1"),
                                  recoveryExecution, &error);
    const auto readyAfterResume = restarted.readyTaskIds();
    ok &= check(resumed && readyAfterResume == QStringList{QStringLiteral("checkpoint-2")} && checkpointSnapshot.tasks.size() == 2,
                QStringLiteral("resume does not repeat completed work and unlocks downstream"));
    // A fresh application session replaces the previous model. Keep the
    // remainder of this hermetic campaign on the reloaded canonical state so
    // later operations do not mix an old in-memory snapshot with the files
    // updated by the restarted session.
    model.setOrchestrationState(restartedModel.orchestrationState());
    model.setRuntimeOwnershipState(restartedModel.runtimeOwnershipState());

    // P1 remains the graph authority and emits deterministic deadlock errors.
    const auto deadlockRootDraft = taskFor(model, QStringLiteral("deadlock-root"), QStringLiteral("deadlock"));
    const auto deadlockChildDraft = taskFor(model, QStringLiteral("deadlock-child"), QStringLiteral("canonical"), {QStringLiteral("deadlock-root")});
    const QList<ExecutionTask> deadlockDraft{deadlockRootDraft, deadlockChildDraft};
    ok &= check(saveCanonicalDag(model, deadlockDraft), QStringLiteral("deadlock DAG persists"));
    const auto deadlockTasks = refreshTaskContracts(model, deadlockDraft);
    ExecutionOrchestrator deadlock(&model);
    const bool deadlockConfigured = deadlock.configure(QStringLiteral("deadlock"), deadlockTasks, {ExecutionWorker{QStringLiteral("deadlock-worker")}}, &error);
    const bool deadlockStarted = deadlockConfigured && deadlock.start(&error);
    const bool deadlockClaimed = deadlockStarted && deadlock.claimTask(QStringLiteral("deadlock-worker"), QStringLiteral("deadlock-root"), &error);
    const bool deadlockRunning = deadlockClaimed && deadlock.startTask(QStringLiteral("deadlock-worker"), QStringLiteral("deadlock-root"), &error);
    const bool deadlockFailed = deadlockRunning && deadlock.failTask(QStringLiteral("deadlock-worker"), QStringLiteral("deadlock-root"), ExecutionFailureCategory::PermanentTask, &error);
    const auto deadlockDiagnostics = deadlock.summary().value(QStringLiteral("deadlockDiagnostics")).toArray();
    const QString deadlockReason = deadlockDiagnostics.isEmpty() ? QString() : deadlockDiagnostics.first().toObject().value(QStringLiteral("reason")).toString();
    ok &= check(deadlockFailed && deadlockReason == QStringLiteral("FAILED_PREREQUISITE"),
                QStringLiteral("failed prerequisite produces deterministic deadlock diagnostics"));
    ok &= check(!ContextCoordinationService::saveTaskDag(model,
                                                         QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("missing-node")},
                                                                                {QStringLiteral("dependencies"), QJsonArray{QStringLiteral("absent")}}}},
                                                         {})
                    .value(QStringLiteral("valid")).toBool(),
                QStringLiteral("P1 rejects a DAG with a missing dependency"));
    ok &= check(!ContextCoordinationService::saveTaskDag(model,
                                                         QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("cycle-a")},
                                                                                {QStringLiteral("dependencies"), QJsonArray{QStringLiteral("cycle-b")}}},
                                                                    QJsonObject{{QStringLiteral("id"), QStringLiteral("cycle-b")},
                                                                                {QStringLiteral("dependencies"), QJsonArray{QStringLiteral("cycle-a")}}}},
                                                         {})
                    .value(QStringLiteral("valid")).toBool(),
                QStringLiteral("P1 rejects a dependency cycle"));

    // A changed P1 source fingerprint blocks dispatch until P1 is refreshed.
    QTemporaryDir staleProject;
    ProjectModel staleModel;
    configureFixture(staleModel, staleProject.path());
    const auto staleDraft = taskFor(staleModel, QStringLiteral("stale"), QStringLiteral("canonical"));
    saveCanonicalDag(staleModel, {staleDraft});
    const auto staleTask = refreshTaskContract(staleModel, staleDraft);
    ExecutionOrchestrator stale(&staleModel);
    ok &= check(stale.configure(QStringLiteral("stale"), {staleTask}, {ExecutionWorker{QStringLiteral("stale-worker")}}, &error)
                    && stale.start(&error), QStringLiteral("stale context runner starts"));
    QFile status(QDir(staleProject.path()).filePath(QStringLiteral("ARAMF_WORKER/PROJECT_STATUS.md")));
    if (status.open(QIODevice::Append | QIODevice::Text)) {
        status.write("material P1 context change\n");
        status.close();
    }
    ok &= check(!stale.claimTask(QStringLiteral("stale-worker"), QStringLiteral("stale"), &error)
                    && error == QStringLiteral("GOVERNANCE_BLOCKED"),
                QStringLiteral("stale P1 context is blocked by read-only PREPARE"));

    const QString savePath = QDir(project.path()).filePath(QStringLiteral("p2.aramf.json"));
    ok &= check(persistence.save(model, savePath, &error), QStringLiteral("canonical P2 state saves: %1").arg(error));
    ProjectModel loaded;
    ok &= check(persistence.load(&loaded, savePath, &error)
                    && !loaded.orchestrationState().isEmpty()
                    && RuntimeOwnershipService::inspect(loaded).value(QStringLiteral("claims")).isArray(),
                QStringLiteral("P0/P2 state reloads without losing semantic ownership history"));
    auto oldProject = persistence.toJson(model);
    oldProject.remove(QStringLiteral("orchestration"));
    ProjectModel oldLoaded;
    ok &= check(persistence.fromJson(&oldLoaded, oldProject, &error) && oldLoaded.orchestrationState().isEmpty(),
                QStringLiteral("projects without P2 state remain backward compatible"));

    std::cout << "P2-EXECUTION checks=" << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}
