#include "ExecutionOrchestrator.h"

#include "WorkerTaskServices.h"
#include "ContextCoordinationService.h"
#include "RuntimeOwnershipService.h"
#include "Services.h"

#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace {
void setError(QString* error, const QString& value)
{
    if (error) *error = value;
}

QStringList stringArray(const QJsonValue& value)
{
    QStringList result;
    for (const auto& entry : value.toArray()) if (entry.isString() && !entry.toString().isEmpty()) result << entry.toString();
    result.removeDuplicates();
    std::sort(result.begin(), result.end());
    return result;
}

QJsonArray toArray(QStringList values)
{
    values.removeDuplicates();
    std::sort(values.begin(), values.end());
    return QJsonArray::fromStringList(values);
}

bool overlaps(const QStringList& left, const QStringList& right)
{
    for (const auto& a : left) for (const auto& b : right) {
        const QString first = a.trimmed().replace('\\', '/');
        const QString second = b.trimmed().replace('\\', '/');
        if (first == second || first.startsWith(second + '/') || second.startsWith(first + '/')) return true;
    }
    return false;
}

bool hasCycle(const QHash<QString, ExecutionTask>& tasks, const QString& id,
              QSet<QString>& active, QSet<QString>& visited)
{
    if (active.contains(id)) return true;
    if (visited.contains(id)) return false;
    active.insert(id);
    for (const auto& dependency : tasks.value(id).dependencies)
        if (hasCycle(tasks, dependency, active, visited)) return true;
    active.remove(id);
    visited.insert(id);
    return false;
}
}

QString ExecutionOrchestrator::stateName(TaskExecutionState state)
{
    switch (state) {
    case TaskExecutionState::Pending: return QStringLiteral("PENDING");
    case TaskExecutionState::Blocked: return QStringLiteral("BLOCKED");
    case TaskExecutionState::Ready: return QStringLiteral("READY");
    case TaskExecutionState::Claimed: return QStringLiteral("CLAIMED");
    case TaskExecutionState::Running: return QStringLiteral("RUNNING");
    case TaskExecutionState::Waiting: return QStringLiteral("WAITING");
    case TaskExecutionState::Succeeded: return QStringLiteral("SUCCEEDED");
    case TaskExecutionState::Failed: return QStringLiteral("FAILED");
    case TaskExecutionState::RetryPending: return QStringLiteral("RETRY_PENDING");
    case TaskExecutionState::Cancelled: return QStringLiteral("CANCELLED");
    case TaskExecutionState::GovernanceBlocked: return QStringLiteral("GOVERNANCE_BLOCKED");
    }
    return QStringLiteral("PENDING");
}

QString ExecutionOrchestrator::failureName(ExecutionFailureCategory category)
{
    switch (category) {
    case ExecutionFailureCategory::None: return QStringLiteral("NONE");
    case ExecutionFailureCategory::Tool: return QStringLiteral("TOOL");
    case ExecutionFailureCategory::Build: return QStringLiteral("BUILD");
    case ExecutionFailureCategory::Test: return QStringLiteral("TEST");
    case ExecutionFailureCategory::Validation: return QStringLiteral("VALIDATION");
    case ExecutionFailureCategory::Governance: return QStringLiteral("GOVERNANCE");
    case ExecutionFailureCategory::Dependency: return QStringLiteral("DEPENDENCY");
    case ExecutionFailureCategory::OwnershipConflict: return QStringLiteral("OWNERSHIP_CONFLICT");
    case ExecutionFailureCategory::StaleContext: return QStringLiteral("STALE_CONTEXT");
    case ExecutionFailureCategory::WorkerUnavailable: return QStringLiteral("WORKER_UNAVAILABLE");
    case ExecutionFailureCategory::TransientInfrastructure: return QStringLiteral("TRANSIENT_INFRASTRUCTURE");
    case ExecutionFailureCategory::PermanentTask: return QStringLiteral("PERMANENT_TASK");
    }
    return QStringLiteral("NONE");
}

bool ExecutionOrchestrator::parseState(const QString& value, TaskExecutionState* result)
{
    if (!result) return false;
    const QString normalized = value.trimmed().toUpper();
    const QList<QPair<QString, TaskExecutionState>> values{
        {"PENDING", TaskExecutionState::Pending}, {"BLOCKED", TaskExecutionState::Blocked},
        {"READY", TaskExecutionState::Ready}, {"CLAIMED", TaskExecutionState::Claimed},
        {"RUNNING", TaskExecutionState::Running}, {"WAITING", TaskExecutionState::Waiting},
        {"SUCCEEDED", TaskExecutionState::Succeeded}, {"FAILED", TaskExecutionState::Failed},
        {"RETRY_PENDING", TaskExecutionState::RetryPending}, {"CANCELLED", TaskExecutionState::Cancelled},
        {"GOVERNANCE_BLOCKED", TaskExecutionState::GovernanceBlocked}};
    for (const auto& valuePair : values) if (valuePair.first == normalized) { *result = valuePair.second; return true; }
    return false;
}

bool ExecutionOrchestrator::parseFailure(const QString& value, ExecutionFailureCategory* result)
{
    if (!result) return false;
    const QString normalized = value.trimmed().toUpper();
    const QList<QPair<QString, ExecutionFailureCategory>> values{
        {"NONE", ExecutionFailureCategory::None}, {"TOOL", ExecutionFailureCategory::Tool},
        {"BUILD", ExecutionFailureCategory::Build}, {"TEST", ExecutionFailureCategory::Test},
        {"VALIDATION", ExecutionFailureCategory::Validation}, {"GOVERNANCE", ExecutionFailureCategory::Governance},
        {"DEPENDENCY", ExecutionFailureCategory::Dependency}, {"OWNERSHIP_CONFLICT", ExecutionFailureCategory::OwnershipConflict},
        {"STALE_CONTEXT", ExecutionFailureCategory::StaleContext}, {"WORKER_UNAVAILABLE", ExecutionFailureCategory::WorkerUnavailable},
        {"TRANSIENT_INFRASTRUCTURE", ExecutionFailureCategory::TransientInfrastructure}, {"PERMANENT_TASK", ExecutionFailureCategory::PermanentTask}};
    for (const auto& valuePair : values) if (valuePair.first == normalized) { *result = valuePair.second; return true; }
    return false;
}

QJsonObject ExecutionTask::toJson() const
{
    return {{"id", id}, {"dependencies", toArray(dependencies)}, {"resources", toArray(resources)},
            {"contractId", contractId}, {"contract", contract}, {"state", ExecutionOrchestrator::stateName(state)},
            {"failure", ExecutionOrchestrator::failureName(failure)}, {"retryCount", retryCount},
            {"retryLimit", retryLimit}, {"workerId", workerId}, {"handoffId", handoffId},
            {"contextFingerprint", contextFingerprint}, {"handoff", handoff}};
}

bool ExecutionTask::fromJson(const QJsonObject& value, ExecutionTask* result, QString* error)
{
    if (!result || value.value("id").toString().trimmed().isEmpty()) { setError(error, QStringLiteral("Execution task requires an id.")); return false; }
    ExecutionTask candidate;
    candidate.id = value.value("id").toString().trimmed();
    candidate.dependencies = stringArray(value.value("dependencies"));
    candidate.resources = stringArray(value.value("resources"));
    candidate.contractId = value.value("contractId").toString();
    candidate.contract = value.value("contract").toObject();
    if (!value.value("state").toString().isEmpty() && !ExecutionOrchestrator::parseState(value.value("state").toString(), &candidate.state)) { setError(error, QStringLiteral("Unknown task state.")); return false; }
    if (!value.value("failure").toString().isEmpty() && !ExecutionOrchestrator::parseFailure(value.value("failure").toString(), &candidate.failure)) { setError(error, QStringLiteral("Unknown task failure category.")); return false; }
    candidate.retryCount = qMax(0, value.value("retryCount").toInt());
    candidate.retryLimit = qMax(0, value.value("retryLimit").toInt());
    candidate.workerId = value.value("workerId").toString();
    candidate.handoffId = value.value("handoffId").toString();
    candidate.contextFingerprint = value.value("contextFingerprint").toString();
    candidate.handoff = value.value("handoff").toObject();
    *result = candidate;
    return true;
}

QJsonObject ExecutionWorker::toJson() const
{
    return {{"id", id}, {"available", available}, {"taskId", taskId}, {"resources", toArray(resources)}};
}

bool ExecutionWorker::fromJson(const QJsonObject& value, ExecutionWorker* result, QString* error)
{
    if (!result || value.value("id").toString().trimmed().isEmpty()) { setError(error, QStringLiteral("Execution worker requires an id.")); return false; }
    ExecutionWorker candidate;
    candidate.id = value.value("id").toString().trimmed();
    candidate.available = value.value("available").toBool(true);
    candidate.taskId = value.value("taskId").toString();
    candidate.resources = stringArray(value.value("resources"));
    *result = candidate;
    return true;
}

bool ExecutionSnapshot::isValid(QString* error) const
{
    if (runId.trimmed().isEmpty()) { setError(error, QStringLiteral("Orchestration run requires an id.")); return false; }
    QHash<QString, ExecutionTask> byId;
    for (const auto& task : tasks) {
        if (task.id.trimmed().isEmpty() || byId.contains(task.id)) { setError(error, QStringLiteral("Task ids must be unique and nonempty.")); return false; }
        if (task.retryCount < 0 || task.retryLimit < 0 || task.retryCount > task.retryLimit) { setError(error, QStringLiteral("Task retry state is invalid.")); return false; }
        byId.insert(task.id, task);
    }
    QSet<QString> workerIds;
    for (const auto& worker : this->workers) {
        if (worker.id.trimmed().isEmpty() || workerIds.contains(worker.id)) { setError(error, QStringLiteral("Worker ids must be unique and nonempty.")); return false; }
        workerIds.insert(worker.id);
    }
    QSet<QString> claimedTasks;
    for (const auto& worker : this->workers) {
        if (worker.taskId.isEmpty()) continue;
        if (!byId.contains(worker.taskId) || claimedTasks.contains(worker.taskId)) {
            setError(error, QStringLiteral("Worker ownership references an unknown or multiply claimed task."));
            return false;
        }
        claimedTasks.insert(worker.taskId);
        const auto& task = byId.value(worker.taskId);
        if (task.workerId != worker.id
            || (task.state != TaskExecutionState::Claimed
                && task.state != TaskExecutionState::Running
                && task.state != TaskExecutionState::Waiting)) {
            setError(error, QStringLiteral("Worker and task ownership state is inconsistent."));
            return false;
        }
    }
    for (const auto& task : tasks) {
        if (task.workerId.isEmpty()) {
            if (task.state == TaskExecutionState::Claimed || task.state == TaskExecutionState::Running
                || task.state == TaskExecutionState::Waiting) {
                setError(error, QStringLiteral("Active task has no worker owner."));
                return false;
            }
        } else if (!workerIds.contains(task.workerId) || !claimedTasks.contains(task.id)) {
            setError(error, QStringLiteral("Task ownership references an unknown or inconsistent worker."));
            return false;
        }
    }
    for (const auto& task : tasks) for (const auto& dependency : task.dependencies)
        if (!byId.contains(dependency)) { setError(error, QStringLiteral("Task dependency '%1' is missing.").arg(dependency)); return false; }
    QSet<QString> active, visited;
    for (const auto& id : byId.keys()) if (hasCycle(byId, id, active, visited)) { setError(error, QStringLiteral("Task DAG contains a cycle.")); return false; }
    return true;
}

QJsonObject ExecutionSnapshot::toJson() const
{
    QJsonArray taskArray, workerArray;
    for (const auto& task : tasks) taskArray.append(task.toJson());
    for (const auto& worker : workers) workerArray.append(worker.toJson());
    return {{"schemaVersion", 1}, {"runId", runId}, {"tasks", taskArray}, {"workers", workerArray},
            {"checkpointId", checkpointId}, {"interrupted", interrupted}, {"lastError", lastError}};
}

bool ExecutionSnapshot::fromJson(const QJsonObject& value, ExecutionSnapshot* result, QString* error)
{
    if (!result || !value.value("tasks").isArray() || !value.value("workers").isArray()) { setError(error, QStringLiteral("Orchestration state requires task and worker arrays.")); return false; }
    ExecutionSnapshot candidate;
    candidate.runId = value.value("runId").toString();
    candidate.checkpointId = value.value("checkpointId").toString();
    candidate.interrupted = value.value("interrupted").toBool(false);
    candidate.lastError = value.value("lastError").toString();
    for (const auto& entry : value.value("tasks").toArray()) { ExecutionTask task; if (!ExecutionTask::fromJson(entry.toObject(), &task, error)) return false; candidate.tasks << task; }
    for (const auto& entry : value.value("workers").toArray()) { ExecutionWorker worker; if (!ExecutionWorker::fromJson(entry.toObject(), &worker, error)) return false; candidate.workers << worker; }
    if (!candidate.isValid(error)) return false;
    *result = candidate;
    return true;
}

ExecutionOrchestrator::ExecutionOrchestrator(ProjectModel* model) : model_(model) {}

bool ExecutionOrchestrator::persistLocked(QString* error)
{
    if (!model_) {
        Q_UNUSED(error);
        return true;
    }

    model_->setOrchestrationState(state_.toJson());
    const auto repair = GenerationServices().repairDerivedArtifacts(*model_, model_->generationOptions());
    if (!repair.success) {
        setError(error, QStringLiteral("P2_DERIVED_STATE_SYNC_FAILED: ") + repair.error);
        return false;
    }
    return true;
}

bool ExecutionOrchestrator::configure(const QString& runId, QList<ExecutionTask> tasks, QList<ExecutionWorker> workers, QString* error)
{
    QMutexLocker locker(&mutex_);
    ExecutionSnapshot candidate{runId.trimmed(), tasks, workers};
    std::sort(candidate.tasks.begin(), candidate.tasks.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    std::sort(candidate.workers.begin(), candidate.workers.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    if (!candidate.isValid(error)) return false;
    state_ = candidate;
    if (!validateCanonicalDependenciesLocked(error)) { state_ = {}; return false; }
    return persistLocked(error) && refreshReadyLocked(error);
}

bool ExecutionOrchestrator::registerWorker(const QString& workerId, QStringList resources, QString* error)
{
    QMutexLocker locker(&mutex_);
    const QString id = workerId.trimmed();
    if (id.isEmpty() || findWorkerLocked(id)) { setError(error, QStringLiteral("Worker id is empty or already registered.")); return false; }
    state_.workers.append(ExecutionWorker{id, true, {}, resources});
    std::sort(state_.workers.begin(), state_.workers.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    return persistLocked(error);
}

bool ExecutionOrchestrator::addTask(const ExecutionTask& task, QString* error)
{
    QMutexLocker locker(&mutex_);
    if (task.id.trimmed().isEmpty() || findTaskLocked(task.id)) { setError(error, QStringLiteral("Task id is empty or already registered.")); return false; }
    state_.tasks.append(task);
    std::sort(state_.tasks.begin(), state_.tasks.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    if (!state_.isValid(error)) { state_.tasks.removeIf([&task](const auto& value) { return value.id == task.id; }); return false; }
    return persistLocked(error);
}

bool ExecutionOrchestrator::refreshReadyLocked(QString* error)
{
    Q_UNUSED(error);
    for (auto& task : state_.tasks) {
        if (task.state == TaskExecutionState::Pending || task.state == TaskExecutionState::Blocked) {
            if (dependenciesFailedLocked(task)) task.state = TaskExecutionState::Blocked;
            else if (dependenciesSatisfiedLocked(task)) task.state = TaskExecutionState::Ready;
            else task.state = TaskExecutionState::Blocked;
        }
    }
    return persistLocked(error);
}

bool ExecutionOrchestrator::start(QString* error)
{
    QMutexLocker locker(&mutex_);
    if (state_.runId.isEmpty()) { setError(error, QStringLiteral("Orchestration is not configured.")); return false; }
    state_.interrupted = false;
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::validateTaskContractLocked(const ExecutionTask& task, QString* error) const
{
    if (!model_) { setError(error, QStringLiteral("P0_GOVERNANCE_UNAVAILABLE")); return false; }
    if (task.contractId.trimmed().isEmpty() || task.contract.value("contractId").toString() != task.contractId) { setError(error, QStringLiteral("TASK_CONTRACT_INVALID")); return false; }
    const QString status = task.contract.value("preflight").toObject().value("status").toString();
    if (status != QStringLiteral("READY") && status != QStringLiteral("READY_WITH_WARNINGS")) { setError(error, QStringLiteral("GOVERNANCE_BLOCKED")); return false; }
    const auto binding = task.contract.value("binding").toObject();
    if (binding.value("projectId").toString() != model_->projectId()) {
        setError(error, QStringLiteral("STALE_TASK_CONTRACT"));
        return false;
    }
    const QStringList permitted = stringArray(task.contract.value("permittedFiles"));
    for (const auto& resource : task.resources) if (!permitted.contains(resource)) { setError(error, QStringLiteral("TASK_SCOPE_VIOLATION")); return false; }
    if (task.contract.contains(QStringLiteral("request"))) {
        const auto authoritative = WorkerTaskServices::prepare(*model_, WorkerTaskRequest::fromJson(task.contract.value(QStringLiteral("request")).toObject()));
        const auto status = authoritative.value(QStringLiteral("preflight")).toObject().value(QStringLiteral("status")).toString();
        if (status != QStringLiteral("READY") && status != QStringLiteral("READY_WITH_WARNINGS")) {
            setError(error, QStringLiteral("GOVERNANCE_BLOCKED"));
            return false;
        }
        for (const auto& field : {QStringLiteral("permittedFiles"), QStringLiteral("requiredEvidence"), QStringLiteral("risk"), QStringLiteral("impact"), QStringLiteral("evidenceDependencies"), QStringLiteral("taskDependencies")}) {
            if (task.contract.value(field) != authoritative.value(field)) {
                setError(error, QStringLiteral("TASK_CONTRACT_INVALID"));
                return false;
            }
        }
    }
    const auto impact = task.contract.value(QStringLiteral("impact")).toObject();
    const QStringList scopes = stringArray(impact.value(QStringLiteral("affectedScopes")));
    if (scopes.isEmpty()) { setError(error, QStringLiteral("P1_CONTEXT_INVALID")); return false; }
    const auto routed = ContextCoordinationService::route(*model_, scopes);
    if (!routed.value(QStringLiteral("valid")).toBool()) { setError(error, QStringLiteral("P1_CONTEXT_INVALID")); return false; }
    if (ContextCoordinationService::freshness(*model_).value(QStringLiteral("staleCount")).toInt() != 0) {
        setError(error, QStringLiteral("STALE_CONTEXT"));
        return false;
    }
    if (task.contextFingerprint.isEmpty()
        || task.contextFingerprint != routed.value(QStringLiteral("indexFingerprint")).toString()) {
        setError(error, QStringLiteral("STALE_CONTEXT"));
        return false;
    }
    return true;
}

bool ExecutionOrchestrator::validateCanonicalDependenciesLocked(QString* error) const
{
    if (!model_) { setError(error, QStringLiteral("P1_DAG_UNAVAILABLE")); return false; }
    const QString dagPath = QDir(model_->projectPath()).filePath(QStringLiteral("ARAMF_WORKER/context/task-dag.json"));
    if (!QFileInfo::exists(dagPath)) { setError(error, QStringLiteral("P1_DAG_UNAVAILABLE")); return false; }
    const auto dag = ContextCoordinationService::taskDag(*model_);
    if (!dag.value(QStringLiteral("valid")).toBool()) { setError(error, QStringLiteral("P1_DAG_UNAVAILABLE")); return false; }
    QHash<QString, QStringList> canonical;
    for (const auto& value : dag.value(QStringLiteral("nodes")).toArray()) {
        const auto node = value.toObject();
        QStringList dependencies;
        for (const auto& dependency : node.value(QStringLiteral("dependencies")).toArray()) dependencies << dependency.toString();
        dependencies.removeDuplicates(); dependencies.sort();
        canonical.insert(node.value(QStringLiteral("id")).toString(), dependencies);
    }
    if (canonical.size() != state_.tasks.size()) { setError(error, QStringLiteral("P1_DAG_MISMATCH")); return false; }
    for (const auto& task : state_.tasks) {
        auto dependencies = task.dependencies;
        dependencies.removeDuplicates(); dependencies.sort();
        if (!canonical.contains(task.id) || canonical.value(task.id) != dependencies) { setError(error, QStringLiteral("P1_DAG_MISMATCH")); return false; }
    }
    return true;
}

ExecutionTask* ExecutionOrchestrator::findTaskLocked(const QString& taskId)
{
    for (auto& task : state_.tasks) if (task.id == taskId) return &task;
    return nullptr;
}

ExecutionWorker* ExecutionOrchestrator::findWorkerLocked(const QString& workerId)
{
    for (auto& worker : state_.workers) if (worker.id == workerId) return &worker;
    return nullptr;
}

bool ExecutionOrchestrator::dependenciesSatisfiedLocked(const ExecutionTask& task) const
{
    for (const auto& dependency : task.dependencies) {
        const auto it = std::find_if(state_.tasks.cbegin(), state_.tasks.cend(), [&dependency](const auto& candidate) { return candidate.id == dependency; });
        if (it == state_.tasks.cend() || it->state != TaskExecutionState::Succeeded) return false;
    }
    return true;
}

bool ExecutionOrchestrator::dependenciesFailedLocked(const ExecutionTask& task) const
{
    for (const auto& dependency : task.dependencies) {
        const auto it = std::find_if(state_.tasks.cbegin(), state_.tasks.cend(), [&dependency](const auto& candidate) { return candidate.id == dependency; });
        if (it != state_.tasks.cend() && (it->state == TaskExecutionState::Failed || it->state == TaskExecutionState::Cancelled || it->state == TaskExecutionState::GovernanceBlocked)) return true;
    }
    return false;
}

bool ExecutionOrchestrator::resourceAvailableLocked(const ExecutionTask& task, const QString& workerId) const
{
    Q_UNUSED(workerId);
    if (!model_) return false;
    const auto ownership = RuntimeOwnershipService::inspect(*model_);
    for (const auto& value : ownership.value(QStringLiteral("claims")).toArray()) {
        const auto claim = value.toObject();
        if (claim.value(QStringLiteral("state")).toString() != QStringLiteral("ACTIVE")) continue;
        for (const auto& requested : task.resources) {
            bool valid = false;
            const QString canonical = RuntimeOwnershipService::canonicalResource(*model_, requested, &valid);
            if (!valid) return false;
            for (const auto& owned : claim.value(QStringLiteral("resources")).toArray())
                if (owned.toString() == canonical) return false;
        }
    }
    return true;
}

bool ExecutionOrchestrator::transitionLocked(ExecutionTask* task, TaskExecutionState next, QString* error)
{
    if (!task) { setError(error, QStringLiteral("Task is not available.")); return false; }
    const auto current = task->state;
    const bool allowed = (current == TaskExecutionState::Ready && next == TaskExecutionState::Claimed)
        || (current == TaskExecutionState::Claimed && next == TaskExecutionState::Running)
        || (current == TaskExecutionState::Running && (next == TaskExecutionState::Succeeded || next == TaskExecutionState::Failed || next == TaskExecutionState::Waiting || next == TaskExecutionState::Cancelled))
        || (current == TaskExecutionState::Failed && next == TaskExecutionState::RetryPending)
        || (current == TaskExecutionState::RetryPending && next == TaskExecutionState::Ready)
        || ((current == TaskExecutionState::Pending || current == TaskExecutionState::Blocked || current == TaskExecutionState::Ready || current == TaskExecutionState::RetryPending || current == TaskExecutionState::Claimed || current == TaskExecutionState::Waiting) && next == TaskExecutionState::Cancelled)
        || ((current == TaskExecutionState::Ready || current == TaskExecutionState::Pending) && next == TaskExecutionState::GovernanceBlocked);
    if (!allowed) { setError(error, QStringLiteral("Illegal task state transition %1 -> %2.").arg(stateName(current), stateName(next))); return false; }
    task->state = next;
    return true;
}

bool ExecutionOrchestrator::claimTask(const QString& workerId, const QString& taskId, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* worker = findWorkerLocked(workerId);
    auto* task = findTaskLocked(taskId);
    if (!worker || !worker->available || !worker->taskId.isEmpty()) { setError(error, QStringLiteral("WORKER_UNAVAILABLE")); return false; }
    if (!task || task->state != TaskExecutionState::Ready) { setError(error, QStringLiteral("TASK_NOT_READY")); return false; }
    if (!validateTaskContractLocked(*task, error)) { task->state = TaskExecutionState::GovernanceBlocked; persistLocked(nullptr); return false; }
    const auto ownership = RuntimeOwnershipService::claim(model_, task->contract, task->id, workerId, task->resources);
    if (!ownership.value(QStringLiteral("granted")).toBool()) {
        const QString code = ownership.value(QStringLiteral("code")).toString(QStringLiteral("OWNERSHIP_DENIED"));
        setError(error, code);
        if (code == QStringLiteral("OWNERSHIP_CONFLICT")) {
            task->failure = ExecutionFailureCategory::OwnershipConflict;
            task->state = TaskExecutionState::Blocked;
        } else {
            task->failure = ExecutionFailureCategory::Governance;
            task->state = TaskExecutionState::GovernanceBlocked;
        }
        persistLocked(nullptr);
        return false;
    }
    if (!transitionLocked(task, TaskExecutionState::Claimed, error)) {
        RuntimeOwnershipService::release(model_, task->id, workerId);
        return false;
    }
    task->workerId = workerId;
    worker->taskId = taskId;
    worker->available = false;
    return persistLocked(error);
}

bool ExecutionOrchestrator::startTask(const QString& workerId, const QString& taskId, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* task = findTaskLocked(taskId);
    if (!task || task->workerId != workerId) { setError(error, QStringLiteral("TASK_OWNERSHIP_INVALID")); return false; }
    if (!transitionLocked(task, TaskExecutionState::Running, error)) return false;
    return persistLocked(error);
}

bool ExecutionOrchestrator::releaseWorkerLocked(ExecutionTask& task, QString* error)
{
    if (model_ && !task.workerId.isEmpty()) {
        const auto released = RuntimeOwnershipService::release(model_, task.id, task.workerId);
        if (!released.value(QStringLiteral("granted")).toBool()) {
            setError(error, released.value(QStringLiteral("code")).toString(QStringLiteral("OWNERSHIP_RELEASE_FAILED")));
            return false;
        }
    }
    if (auto* worker = findWorkerLocked(task.workerId)) { worker->taskId.clear(); worker->available = true; }
    task.workerId.clear();
    return true;
}

bool ExecutionOrchestrator::completeTask(const QString& workerId, const QString& taskId,
                                         const QJsonObject& executionResult, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* task = findTaskLocked(taskId);
    if (!task || task->workerId != workerId || task->state != TaskExecutionState::Running) { setError(error, QStringLiteral("TASK_NOT_RUNNING")); return false; }
    if (!model_) { setError(error, QStringLiteral("P0_GOVERNANCE_UNAVAILABLE")); return false; }

    const auto validation = WorkerTaskServices::postflight(*model_, task->contract,
                                                            executionResult.value(QStringLiteral("evidence")).toArray());
    const QString completion = validation.value(QStringLiteral("completionState")).toString();
    if (validation.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")
        || !QStringList{QStringLiteral("VERIFIED"), QStringLiteral("CERTIFIED")}.contains(completion)) {
        task->failure = ExecutionFailureCategory::Validation;
        QString validationCode;
        QString warningCode;
        for (const auto& item : validation.value(QStringLiteral("errors")).toArray()) {
            const QString code = item.toObject().value(QStringLiteral("code")).toString();
            if (code.isEmpty()) continue;
            if (item.toObject().value(QStringLiteral("severity")).toString() == QStringLiteral("WARNING")) {
                if (warningCode.isEmpty() || warningCode == QStringLiteral("REPOSITORY_WITHOUT_GIT")) warningCode = code;
            } else {
                validationCode = code;
                break;
            }
        }
        setError(error, validationCode.isEmpty() ? (warningCode.isEmpty() ? QStringLiteral("VALIDATION_FAILED") : warningCode) : validationCode);
        persistLocked(nullptr);
        return false;
    }

    QStringList downstream;
    for (const auto& candidate : state_.tasks)
        if (candidate.dependencies.contains(taskId)) downstream << candidate.id;
    downstream.sort();
    QString destinationTaskId = executionResult.value(QStringLiteral("destinationTaskId")).toString();
    if (!destinationTaskId.isEmpty() && !downstream.contains(destinationTaskId)) {
        setError(error, QStringLiteral("HANDOFF_TARGET_INVALID"));
        return false;
    }
    if (destinationTaskId.isEmpty() && !downstream.isEmpty()) destinationTaskId = downstream.first();
    if (destinationTaskId.isEmpty()) destinationTaskId = QStringLiteral("process-complete");
    const QString targetAgent = destinationTaskId;
    const QStringList destinationScopes = stringArray(task->contract.value(QStringLiteral("impact")).toObject().value(QStringLiteral("affectedScopes")));
    QJsonObject metadata{
        {QStringLiteral("sourceTaskId"), taskId},
        {QStringLiteral("destinationTaskId"), destinationTaskId},
        {QStringLiteral("workerId"), workerId},
        {QStringLiteral("executionResult"), executionResult},
        {QStringLiteral("changedResources"), toArray(task->resources)},
        {QStringLiteral("producedArtifacts"), executionResult.value(QStringLiteral("producedArtifacts")).toArray()},
        {QStringLiteral("validationResult"), validation},
        {QStringLiteral("validationEvidence"), executionResult.value(QStringLiteral("evidence")).toArray()},
        {QStringLiteral("contextFingerprint"), task->contextFingerprint},
        {QStringLiteral("provenance"), task->contract.value(QStringLiteral("binding"))},
        {QStringLiteral("dependencyState"), QStringLiteral("SATISFIED")},
        {QStringLiteral("continuationRequirements"), executionResult.value(QStringLiteral("continuationRequirements"))},
        {QStringLiteral("completionState"), QStringLiteral("SUCCEEDED")}};
    const auto canonicalHandoff = ContextCoordinationService::createHandoff(
        *model_, task->contract, workerId, targetAgent, destinationScopes, true, metadata);
    if (!canonicalHandoff.value(QStringLiteral("valid")).toBool()) {
        setError(error, canonicalHandoff.value(QStringLiteral("errorCode")).toString(QStringLiteral("HANDOFF_FAILED")));
        return false;
    }
    if (!releaseWorkerLocked(*task, error)) return false;
    task->handoff = canonicalHandoff;
    task->handoffId = canonicalHandoff.value(QStringLiteral("handoffId")).toString();
    if (!transitionLocked(task, TaskExecutionState::Succeeded, error)) return false;
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::failTask(const QString& workerId, const QString& taskId, ExecutionFailureCategory category, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* task = findTaskLocked(taskId);
    if (!task || task->workerId != workerId || task->state != TaskExecutionState::Running) { setError(error, QStringLiteral("TASK_NOT_RUNNING")); return false; }
    task->failure = category;
    if (!releaseWorkerLocked(*task, error)) return false;
    if (!transitionLocked(task, TaskExecutionState::Failed, error)) return false;
    const bool retryable = category == ExecutionFailureCategory::Tool || category == ExecutionFailureCategory::Build
        || category == ExecutionFailureCategory::Test || category == ExecutionFailureCategory::Validation
        || category == ExecutionFailureCategory::TransientInfrastructure || category == ExecutionFailureCategory::WorkerUnavailable;
    if (retryable && task->retryCount < task->retryLimit) {
        ++task->retryCount;
        if (!transitionLocked(task, TaskExecutionState::RetryPending, error)) return false;
    }
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::retryTask(const QString& taskId, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* task = findTaskLocked(taskId);
    if (!task || task->state != TaskExecutionState::RetryPending || task->retryCount > task->retryLimit) { setError(error, QStringLiteral("TASK_NOT_RETRYABLE")); return false; }
    if (!dependenciesSatisfiedLocked(*task)) { setError(error, QStringLiteral("DEPENDENCY_BLOCKED")); return false; }
    if (!transitionLocked(task, TaskExecutionState::Ready, error)) return false;
    return persistLocked(error);
}

bool ExecutionOrchestrator::cancelTask(const QString& taskId, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* task = findTaskLocked(taskId);
    if (task && !task->workerId.isEmpty() && !releaseWorkerLocked(*task, error)) return false;
    if (!transitionLocked(task, TaskExecutionState::Cancelled, error)) return false;
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::markWorkerUnavailable(const QString& workerId, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* worker = findWorkerLocked(workerId);
    if (!worker) { setError(error, QStringLiteral("WORKER_UNKNOWN")); return false; }
    if (model_) {
        const auto ownership = RuntimeOwnershipService::markWorkerUnavailable(model_, workerId);
        if (!ownership.value(QStringLiteral("granted")).toBool()) { setError(error, ownership.value(QStringLiteral("code")).toString()); return false; }
    }
    worker->available = false;
    if (!worker->taskId.isEmpty()) {
        const QString taskId = worker->taskId;
        auto* task = findTaskLocked(taskId);
        if (task && (task->state == TaskExecutionState::Claimed || task->state == TaskExecutionState::Running)) {
            task->failure = ExecutionFailureCategory::WorkerUnavailable;
            task->state = TaskExecutionState::Failed;
            if (!releaseWorkerLocked(*task, error)) return false;
            if (task->retryCount < task->retryLimit) { ++task->retryCount; task->state = TaskExecutionState::RetryPending; }
        }
        if (auto* unavailable = findWorkerLocked(workerId)) unavailable->available = false;
    }
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::recover(QString* error)
{
    QMutexLocker locker(&mutex_);
    QSet<QString> workersRequiringOwnershipRecovery;
    for (auto& worker : state_.workers) {
        if (worker.taskId.isEmpty()) continue;
        auto* task = findTaskLocked(worker.taskId);
        if (!task) { worker.taskId.clear(); continue; }
        if (task->state == TaskExecutionState::Claimed || task->state == TaskExecutionState::Running
            || task->state == TaskExecutionState::Waiting) {
            task->failure = ExecutionFailureCategory::WorkerUnavailable;
            task->state = TaskExecutionState::Failed;
            if (model_) {
                RuntimeOwnershipService::markWorkerUnavailable(model_, worker.id);
                workersRequiringOwnershipRecovery.insert(worker.id);
            }
            if (!releaseWorkerLocked(*task, error)) return false;
            if (task->retryCount < task->retryLimit) { ++task->retryCount; task->state = TaskExecutionState::RetryPending; }
        }
    }
    if (model_) {
        for (const auto& worker : state_.workers) {
            if (!worker.available) workersRequiringOwnershipRecovery.insert(worker.id);
        }
        auto recoveryIds = workersRequiringOwnershipRecovery.values();
        std::sort(recoveryIds.begin(), recoveryIds.end());
        for (const auto& workerId : recoveryIds)
            RuntimeOwnershipService::recoverWorker(model_, workerId);
    }
    state_.interrupted = false;
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::writeCheckpoint(QString* error)
{
    QMutexLocker locker(&mutex_);
    state_.checkpointId = state_.runId + QStringLiteral("/") + QString::number(state_.tasks.size());
    state_.interrupted = true;
    return persistLocked(error);
}

bool ExecutionOrchestrator::restoreCheckpoint(const QJsonObject& checkpoint, QString* error)
{
    ExecutionSnapshot restored;
    if (!ExecutionSnapshot::fromJson(checkpoint, &restored, error)) return false;
    {
        QMutexLocker locker(&mutex_);
        state_ = restored;
        if (!persistLocked(error)) return false;
    }
    return recover(error);
}

QStringList ExecutionOrchestrator::readyTaskIds() const
{
    QMutexLocker locker(&mutex_); QStringList result;
    for (const auto& task : state_.tasks) if (task.state == TaskExecutionState::Ready) result << task.id;
    return result;
}

QStringList ExecutionOrchestrator::blockedTaskIds() const
{
    QMutexLocker locker(&mutex_); QStringList result;
    for (const auto& task : state_.tasks) if (task.state == TaskExecutionState::Blocked || task.state == TaskExecutionState::GovernanceBlocked) result << task.id;
    return result;
}

QStringList ExecutionOrchestrator::activeTaskIds() const
{
    QMutexLocker locker(&mutex_); QStringList result;
    for (const auto& task : state_.tasks) if (task.state == TaskExecutionState::Claimed || task.state == TaskExecutionState::Running || task.state == TaskExecutionState::Waiting) result << task.id;
    return result;
}

bool ExecutionOrchestrator::isComplete() const
{
    QMutexLocker locker(&mutex_);
    return !state_.tasks.isEmpty() && std::all_of(state_.tasks.cbegin(), state_.tasks.cend(), [](const auto& task) { return task.state == TaskExecutionState::Succeeded || task.state == TaskExecutionState::Cancelled; });
}

QJsonObject ExecutionOrchestrator::summary() const
{
    QMutexLocker locker(&mutex_);
    QJsonObject counts;
    for (const auto& task : state_.tasks) counts.insert(stateName(task.state), counts.value(stateName(task.state)).toInt() + 1);
    const bool complete = !state_.tasks.isEmpty() && std::all_of(state_.tasks.cbegin(), state_.tasks.cend(), [](const auto& task) { return task.state == TaskExecutionState::Succeeded || task.state == TaskExecutionState::Cancelled; });
    const int ready = static_cast<int>(std::count_if(state_.tasks.cbegin(), state_.tasks.cend(), [](const auto& task) { return task.state == TaskExecutionState::Ready; }));
    const int blocked = static_cast<int>(std::count_if(state_.tasks.cbegin(), state_.tasks.cend(), [](const auto& task) { return task.state == TaskExecutionState::Blocked || task.state == TaskExecutionState::GovernanceBlocked; }));
    const int active = static_cast<int>(std::count_if(state_.tasks.cbegin(), state_.tasks.cend(), [](const auto& task) { return task.state == TaskExecutionState::Claimed || task.state == TaskExecutionState::Running || task.state == TaskExecutionState::Waiting; }));
    QJsonArray diagnostics;
    for (const auto& task : state_.tasks) {
        if (task.state == TaskExecutionState::Blocked && dependenciesFailedLocked(task))
            diagnostics.append(QJsonObject{{QStringLiteral("taskId"), task.id}, {QStringLiteral("reason"), QStringLiteral("FAILED_PREREQUISITE")}});
        else if (task.state == TaskExecutionState::Ready && !resourceAvailableLocked(task, QString{}))
            diagnostics.append(QJsonObject{{QStringLiteral("taskId"), task.id}, {QStringLiteral("reason"), QStringLiteral("RESOURCE_BLOCKED")}});
    }
    if (!complete && ready == 0 && active == 0 && !state_.tasks.isEmpty() && diagnostics.isEmpty())
        diagnostics.append(QJsonObject{{QStringLiteral("reason"), QStringLiteral("NO_RUNNABLE_TASKS")} });
    const QString status = complete ? QStringLiteral("COMPLETE") : ready > 0 ? QStringLiteral("RUNNABLE") : active > 0 ? QStringLiteral("ACTIVE") : QStringLiteral("BLOCKED");
    return {{"runId", state_.runId}, {"status", status}, {"taskCount", state_.tasks.size()}, {"workerCount", state_.workers.size()},
            {"readyCount", ready}, {"blockedCount", blocked}, {"activeCount", active},
            {"checkpointId", state_.checkpointId}, {"interrupted", state_.interrupted}, {"counts", counts}, {"deadlockDiagnostics", diagnostics}};
}

ExecutionSnapshot ExecutionOrchestrator::snapshot() const
{
    QMutexLocker locker(&mutex_);
    return state_;
}
