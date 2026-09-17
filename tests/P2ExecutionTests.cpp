#include "core/ExecutionOrchestrator.h"
#include "core/ProjectPersistence.h"

#include <QDir>
#include <QTemporaryDir>

#include <atomic>
#include <iostream>
#include <thread>

namespace {
bool check(bool condition, const QString& message)
{
    if (!condition) std::cerr << "P2 FAIL: " << message.toStdString() << '\n';
    return condition;
}

ExecutionTask task(const QString& id, QStringList dependencies = {}, QStringList resources = {}, int retryLimit = 0)
{
    ExecutionTask result;
    result.id = id;
    result.dependencies = dependencies;
    result.resources = resources;
    result.contractId = QStringLiteral("contract-") + id;
    result.contract = QJsonObject{{"contractId", result.contractId},
                                  {"preflight", QJsonObject{{"status", "READY"}}},
                                  {"permittedFiles", QJsonArray::fromStringList(resources)}};
    result.retryLimit = retryLimit;
    return result;
}

ExecutionWorker worker(const QString& id)
{
    return ExecutionWorker{id, true, {}, {}};
}
}

bool runP2ExecutionTests()
{
    bool ok = true;
    QString error;
    ProjectModel model;
    QTemporaryDir project;
    model.setProjectId(QStringLiteral("p2-test"));
    model.setProjectPath(project.path());

    ProjectPersistence persistence;
    auto authoritativeJson = persistence.toJson(model);
    authoritativeJson.insert(QStringLiteral("processVersion"), QJsonObject{
        {QStringLiteral("completedHistory"), QJsonArray{
            QJsonObject{{"process", 0}, {"loop", 1}, {"iteration", 1}, {"certification", 1}, {"done", 1}},
            QJsonObject{{"process", 1}, {"loop", 1}, {"iteration", 1}, {"certification", 1}, {"done", 1}}}},
        {QStringLiteral("active"), QJsonValue(QJsonValue::Null)},
        {QStringLiteral("next"), QJsonObject{{"process", 2}, {"loop", 1}, {"iteration", 0}, {"certification", 0}, {"done", 0}}}});
    ProjectModel processModel;
    ok &= check(persistence.fromJson(&processModel, authoritativeJson, &error), QStringLiteral("authoritative P2 baseline loads: %1").arg(error));
    ok &= check(processModel.startNextProcess(&error) && processModel.processVersionState().activeIdentifier() == QStringLiteral("P2.1.1.0.0"), QStringLiteral("P2 starts only through lifecycle authority: %1").arg(error));
    ok &= check(processModel.processVersionState().completedIdentifiers() == QStringList{"P0.1.1.1.1", "P1.1.1.1.1"}, QStringLiteral("P0 and P1 remain immutable when P2 starts"));

    ExecutionOrchestrator orchestrator(&model);
    ok &= check(orchestrator.configure(QStringLiteral("integration"),
        {task("T1", {}, {"db.cpp"}), task("T2", {}, {"ui.cpp"}),
         task("T3", {"T1", "T2"}, {"integration.cpp"}), task("T4", {"T3"}, {"regression.cpp"})},
        {worker("worker-a"), worker("worker-b")}, &error), QStringLiteral("integration DAG configures: %1").arg(error));
    ok &= check(orchestrator.start(&error), QStringLiteral("orchestration starts"));
    ok &= check(orchestrator.readyTaskIds() == QStringList{"T1", "T2"}, QStringLiteral("independent roots are deterministic and ready"));
    ok &= check(orchestrator.blockedTaskIds().contains("T3"), QStringLiteral("dependent task is initially blocked"));
    ok &= check(orchestrator.claimTask("worker-a", "T1", &error) && orchestrator.startTask("worker-a", "T1", &error)
                    && orchestrator.completeTask("worker-a", "T1", QJsonObject{{"handoffId", "handoff-t1"}}, &error), QStringLiteral("first root completes: %1").arg(error));
    ok &= check(orchestrator.readyTaskIds() == QStringList{"T2"}, QStringLiteral("only remaining root is ready"));
    ok &= check(orchestrator.claimTask("worker-b", "T2", &error) && orchestrator.startTask("worker-b", "T2", &error)
                    && orchestrator.completeTask("worker-b", "T2", {}, &error), QStringLiteral("second root completes: %1").arg(error));
    ok &= check(orchestrator.readyTaskIds() == QStringList{"T3"}, QStringLiteral("downstream task activates after dependencies"));
    ok &= check(orchestrator.claimTask("worker-a", "T3", &error) && orchestrator.startTask("worker-a", "T3", &error)
                    && orchestrator.completeTask("worker-a", "T3", {}, &error), QStringLiteral("integration task completes: %1").arg(error));
    ok &= check(orchestrator.readyTaskIds() == QStringList{"T4"}, QStringLiteral("final downstream task activates"));
    ok &= check(orchestrator.claimTask("worker-b", "T4", &error) && orchestrator.startTask("worker-b", "T4", &error)
                    && orchestrator.completeTask("worker-b", "T4", {}, &error)
                    && orchestrator.isComplete(), QStringLiteral("integration DAG completes"));
    ok &= check(orchestrator.snapshot().tasks.last().handoff.value("completionState") == QStringLiteral("SUCCEEDED")
                    && orchestrator.snapshot().tasks.last().handoff.value("taskId") == QStringLiteral("T4"),
                QStringLiteral("successful task produces a machine-readable handoff"));

    ExecutionOrchestrator requiredDAG(&model);
    ok &= check(requiredDAG.configure(QStringLiteral("required-t1-t5"),
        {task("T1"), task("T2"), task("T3", {"T1"}), task("T4", {"T1", "T2"}), task("T5", {"T3", "T4"})},
        {worker("database-worker"), worker("ui-worker")}, &error) && requiredDAG.start(&error),
        QStringLiteral("required T1-T5 DAG starts"));
    ok &= check(requiredDAG.readyTaskIds() == QStringList{"T1", "T2"}
                    && requiredDAG.blockedTaskIds().size() == 3, QStringLiteral("required DAG initial frontier"));
    auto finish = [&requiredDAG, &error](const QString& workerId, const QString& taskId) {
        return requiredDAG.claimTask(workerId, taskId, &error) && requiredDAG.startTask(workerId, taskId, &error)
            && requiredDAG.completeTask(workerId, taskId, {}, &error);
    };
    ok &= check(finish("database-worker", "T1") && requiredDAG.readyTaskIds() == QStringList{"T2", "T3"},
                QStringLiteral("required DAG activates T3 after T1"));
    ok &= check(finish("ui-worker", "T2") && requiredDAG.readyTaskIds() == QStringList{"T3", "T4"},
                QStringLiteral("required DAG activates T4 after T2"));
    ok &= check(finish("database-worker", "T3") && finish("ui-worker", "T4")
                    && requiredDAG.readyTaskIds() == QStringList{"T5"} && finish("database-worker", "T5")
                    && requiredDAG.isComplete(), QStringLiteral("required T1-T5 DAG completes"));

    ExecutionOrchestrator parallel(&model);
    ok &= check(parallel.configure(QStringLiteral("parallel"), {task("PA", {}, {"a.cpp"}), task("PB", {}, {"b.cpp"})},
                                    {worker("A"), worker("B")}, &error) && parallel.start(&error),
                QStringLiteral("parallel fixture starts"));
    std::atomic<int> running{0};
    std::atomic<bool> overlap{false};
    auto runParallel = [&parallel, &running, &overlap](const QString& workerId, const QString& taskId) {
        if (!parallel.claimTask(workerId, taskId) || !parallel.startTask(workerId, taskId)) return;
        const int count = ++running;
        if (count == 2) overlap = true;
        while (running.load() < 2) std::this_thread::yield();
            parallel.completeTask(workerId, taskId);
        --running;
    };
    std::thread first(runParallel, QStringLiteral("A"), QStringLiteral("PA"));
    std::thread second(runParallel, QStringLiteral("B"), QStringLiteral("PB"));
    first.join(); second.join();
    ok &= check(overlap.load(), QStringLiteral("independent tasks have overlapping execution windows"));

    ExecutionOrchestrator workerLoss(&model);
    ok &= check(workerLoss.configure(QStringLiteral("worker-loss"), {task("WL", {}, {}, 1)}, {worker("lost")}, &error)
                    && workerLoss.start(&error) && workerLoss.claimTask("lost", "WL", &error)
                    && workerLoss.startTask("lost", "WL", &error)
                    && workerLoss.markWorkerUnavailable("lost", &error),
                QStringLiteral("worker loss is recovered through central state"));
    ok &= check(workerLoss.snapshot().tasks.first().state == TaskExecutionState::RetryPending
                    && !workerLoss.snapshot().workers.first().available,
                QStringLiteral("lost worker remains unavailable and task is retryable"));

    ExecutionOrchestrator collision(&model);
    ok &= check(collision.configure(QStringLiteral("collision"), {task("A", {}, {"shared.cpp"}), task("B", {}, {"shared.cpp"})}, {worker("one"), worker("two")}, &error), QStringLiteral("collision DAG configures"));
    ok &= check(collision.start(&error) && collision.claimTask("one", "A", &error), QStringLiteral("first owner claims shared resource"));
    ok &= check(!collision.claimTask("two", "B", &error) && error == QStringLiteral("OWNERSHIP_CONFLICT"), QStringLiteral("overlapping ownership is blocked"));
    ok &= check(collision.startTask("one", "A", &error) && collision.completeTask("one", "A", {}, &error)
                    && collision.claimTask("two", "B", &error), QStringLiteral("ownership releases after success"));

    ExecutionOrchestrator retry(&model);
    ok &= check(retry.configure(QStringLiteral("retry"), {task("R", {}, {"retry.cpp"}, 1)}, {worker("retry-worker")}, &error) && retry.start(&error), QStringLiteral("retry DAG starts"));
    ok &= check(retry.claimTask("retry-worker", "R", &error) && retry.startTask("retry-worker", "R", &error)
                    && retry.failTask("retry-worker", "R", ExecutionFailureCategory::TransientInfrastructure, &error), QStringLiteral("transient failure is classified"));
    ok &= check(retry.snapshot().tasks.first().state == TaskExecutionState::RetryPending && retry.snapshot().tasks.first().retryCount == 1, QStringLiteral("retry state and count persist in memory"));
    ok &= check(retry.retryTask("R", &error) && retry.readyTaskIds() == QStringList{"R"}, QStringLiteral("retry returns task to ready"));
    ok &= check(!retry.failTask("retry-worker", "R", ExecutionFailureCategory::PermanentTask, &error), QStringLiteral("unstarted task cannot be failed"));

    ExecutionOrchestrator checkpoint(&model);
    ok &= check(checkpoint.configure(QStringLiteral("resume"), {task("C1", {}, {"c1.cpp"}, 1), task("C2", {"C1"}, {"c2.cpp"})}, {worker("resume-worker")}, &error)
                    && checkpoint.start(&error) && checkpoint.claimTask("resume-worker", "C1", &error)
                    && checkpoint.startTask("resume-worker", "C1", &error) && checkpoint.writeCheckpoint(&error), QStringLiteral("checkpoint captures active DAG: %1").arg(error));
    const auto persistedSnapshot = checkpoint.snapshot();
    const QString path = QDir(project.path()).filePath(QStringLiteral("p2.aramf.json"));
    ok &= check(persistence.save(model, path, &error), QStringLiteral("orchestration state saves through ProjectPersistence: %1").arg(error));
    ProjectModel loaded;
    ok &= check(persistence.load(&loaded, path, &error) && !loaded.orchestrationState().isEmpty(), QStringLiteral("orchestration state reloads: %1").arg(error));
    ExecutionOrchestrator recovered(&loaded);
    ok &= check(recovered.restoreCheckpoint(loaded.orchestrationState(), &error), QStringLiteral("checkpoint recovery succeeds: %1").arg(error));
    ok &= check(recovered.snapshot().tasks.first().state == TaskExecutionState::RetryPending, QStringLiteral("interrupted running task becomes retryable"));
    ok &= check(recovered.retryTask("C1", &error) && recovered.claimTask("resume-worker", "C1", &error)
                    && recovered.startTask("resume-worker", "C1", &error) && recovered.completeTask("resume-worker", "C1", {}, &error)
                    && recovered.readyTaskIds() == QStringList{"C2"}, QStringLiteral("recovered task is not duplicated and unlocks downstream: %1").arg(error));
    ok &= check(persistedSnapshot.tasks.size() == 2 && recovered.summary().value("taskCount").toInt() == 2, QStringLiteral("checkpoint retains complete DAG shape"));

    auto oldProject = persistence.toJson(model);
    oldProject.remove(QStringLiteral("orchestration"));
    ProjectModel oldLoaded;
    ok &= check(persistence.fromJson(&oldLoaded, oldProject, &error) && oldLoaded.orchestrationState().isEmpty(), QStringLiteral("projects without P2 state remain compatible"));
    ok &= check(!checkpoint.isComplete(), QStringLiteral("incomplete DAG is not reported complete"));

    ExecutionOrchestrator invalid(&model);
    ok &= check(!invalid.configure(QStringLiteral("missing"), {task("orphan", {"missing"})}, {worker("w")}, &error), QStringLiteral("missing dependency is rejected"));
    ok &= check(!invalid.configure(QStringLiteral("cycle"), {task("a", {"b"}), task("b", {"a"})}, {worker("w")}, &error), QStringLiteral("dependency cycle is rejected"));

    ExecutionTask blocked = task("blocked", {}, {"blocked.cpp"});
    blocked.contract.insert(QStringLiteral("preflight"), QJsonObject{{"status", "BLOCKED"}});
    ExecutionOrchestrator governance(&model);
    ok &= check(governance.configure(QStringLiteral("governance"), {blocked}, {worker("w")}, &error) && governance.start(&error), QStringLiteral("governance fixture starts"));
    ok &= check(!governance.claimTask("w", "blocked", &error) && governance.snapshot().tasks.first().state == TaskExecutionState::GovernanceBlocked, QStringLiteral("governance-blocked task cannot be claimed"));

    ExecutionOrchestrator permanent(&model);
    ok &= check(permanent.configure(QStringLiteral("permanent"), {task("permanent", {}, {"permanent.cpp"}, 3)}, {worker("w")}, &error) && permanent.start(&error)
                    && permanent.claimTask("w", "permanent", &error) && permanent.startTask("w", "permanent", &error)
                    && permanent.failTask("w", "permanent", ExecutionFailureCategory::PermanentTask, &error), QStringLiteral("permanent failure is recorded"));
    ok &= check(permanent.snapshot().tasks.first().state == TaskExecutionState::Failed && permanent.snapshot().tasks.first().retryCount == 0, QStringLiteral("permanent failure does not retry"));
    std::cout << "P2-EXECUTION checks=" << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}
