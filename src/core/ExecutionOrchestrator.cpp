#include "ExecutionOrchestrator.h"

#include "WorkerTaskServices.h"

#include <QJsonArray>
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
    if (model_) model_->setOrchestrationState(state_.toJson());
    Q_UNUSED(error);
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
    if (task.contractId.trimmed().isEmpty() || task.contract.value("contractId").toString() != task.contractId) { setError(error, QStringLiteral("TASK_CONTRACT_INVALID")); return false; }
    const QString status = task.contract.value("preflight").toObject().value("status").toString();
    if (status != QStringLiteral("READY") && status != QStringLiteral("READY_WITH_WARNINGS")) { setError(error, QStringLiteral("GOVERNANCE_BLOCKED")); return false; }
    const auto binding = task.contract.value("binding").toObject();
    if (model_ && !binding.isEmpty() && binding.value("projectId").toString() != model_->projectId()) {
        setError(error, QStringLiteral("STALE_TASK_CONTRACT"));
        return false;
    }
    const QStringList permitted = stringArray(task.contract.value("permittedFiles"));
    for (const auto& resource : task.resources) if (!permitted.contains(resource)) { setError(error, QStringLiteral("TASK_SCOPE_VIOLATION")); return false; }
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
    for (const auto& candidate : state_.tasks) if (candidate.id != task.id && (candidate.state == TaskExecutionState::Claimed || candidate.state == TaskExecutionState::Running) && overlaps(task.resources, candidate.resources)) return false;
    const auto worker = std::find_if(state_.workers.cbegin(), state_.workers.cend(), [&workerId](const auto& candidate) { return candidate.id == workerId; });
    return worker != state_.workers.cend();
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
    if (!resourceAvailableLocked(*task, workerId)) { setError(error, QStringLiteral("OWNERSHIP_CONFLICT")); return false; }
    if (!transitionLocked(task, TaskExecutionState::Claimed, error)) return false;
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
    Q_UNUSED(error);
    if (auto* worker = findWorkerLocked(task.workerId)) { worker->taskId.clear(); worker->available = true; }
    task.workerId.clear();
    return true;
}

bool ExecutionOrchestrator::completeTask(const QString& workerId, const QString& taskId, const QJsonObject& executionResult, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* task = findTaskLocked(taskId);
    if (!task || task->workerId != workerId || task->state != TaskExecutionState::Running) { setError(error, QStringLiteral("TASK_NOT_RUNNING")); return false; }
    QJsonObject canonicalHandoff{
        {QStringLiteral("taskId"), taskId},
        {QStringLiteral("workerId"), workerId},
        {QStringLiteral("completionState"), QStringLiteral("SUCCEEDED")},
        {QStringLiteral("changedResources"), toArray(task->resources)},
        {QStringLiteral("producedArtifacts"), QJsonArray{}},
        {QStringLiteral("validationState"), QStringLiteral("PASS")},
        {QStringLiteral("dependencyState"), QStringLiteral("SATISFIED")},
        {QStringLiteral("contextFingerprint"), task->contextFingerprint},
        {QStringLiteral("provenance"), task->contract.value(QStringLiteral("binding"))}};
    for (auto it = executionResult.constBegin(); it != executionResult.constEnd(); ++it) canonicalHandoff.insert(it.key(), it.value());
    task->handoff = canonicalHandoff;
    task->handoffId = canonicalHandoff.value(QStringLiteral("handoffId")).toString();
    if (task->handoffId.isEmpty()) task->handoffId = taskId + QStringLiteral("/handoff");
    task->handoff.insert(QStringLiteral("handoffId"), task->handoffId);
    task->contextFingerprint = task->handoff.value(QStringLiteral("contextFingerprint")).toString();
    if (!transitionLocked(task, TaskExecutionState::Succeeded, error)) return false;
    releaseWorkerLocked(*task);
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::failTask(const QString& workerId, const QString& taskId, ExecutionFailureCategory category, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* task = findTaskLocked(taskId);
    if (!task || task->workerId != workerId || task->state != TaskExecutionState::Running) { setError(error, QStringLiteral("TASK_NOT_RUNNING")); return false; }
    task->failure = category;
    if (!transitionLocked(task, TaskExecutionState::Failed, error)) return false;
    releaseWorkerLocked(*task);
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
    if (!transitionLocked(task, TaskExecutionState::Cancelled, error)) return false;
    releaseWorkerLocked(*task);
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::markWorkerUnavailable(const QString& workerId, QString* error)
{
    QMutexLocker locker(&mutex_);
    auto* worker = findWorkerLocked(workerId);
    if (!worker) { setError(error, QStringLiteral("WORKER_UNKNOWN")); return false; }
    worker->available = false;
    if (!worker->taskId.isEmpty()) {
        const QString taskId = worker->taskId;
        auto* task = findTaskLocked(taskId);
        if (task && (task->state == TaskExecutionState::Claimed || task->state == TaskExecutionState::Running)) {
            task->failure = ExecutionFailureCategory::WorkerUnavailable;
            task->state = TaskExecutionState::Failed;
            releaseWorkerLocked(*task);
            if (task->retryCount < task->retryLimit) { ++task->retryCount; task->state = TaskExecutionState::RetryPending; }
        }
        if (auto* unavailable = findWorkerLocked(workerId)) unavailable->available = false;
    }
    return refreshReadyLocked(error);
}

bool ExecutionOrchestrator::recover(QString* error)
{
    QMutexLocker locker(&mutex_);
    for (auto& worker : state_.workers) {
        if (worker.taskId.isEmpty()) continue;
        auto* task = findTaskLocked(worker.taskId);
        if (!task) { worker.taskId.clear(); continue; }
        if (task->state == TaskExecutionState::Claimed || task->state == TaskExecutionState::Running
            || task->state == TaskExecutionState::Waiting) {
            task->failure = ExecutionFailureCategory::WorkerUnavailable;
            task->state = TaskExecutionState::Failed;
            releaseWorkerLocked(*task);
            if (task->retryCount < task->retryLimit) { ++task->retryCount; task->state = TaskExecutionState::RetryPending; }
        }
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
    const QString status = complete ? QStringLiteral("COMPLETE") : ready > 0 ? QStringLiteral("RUNNABLE") : active > 0 ? QStringLiteral("ACTIVE") : QStringLiteral("BLOCKED");
    return {{"runId", state_.runId}, {"status", status}, {"taskCount", state_.tasks.size()}, {"workerCount", state_.workers.size()},
            {"readyCount", ready}, {"blockedCount", blocked}, {"activeCount", active},
            {"checkpointId", state_.checkpointId}, {"interrupted", state_.interrupted}, {"counts", counts}};
}

ExecutionSnapshot ExecutionOrchestrator::snapshot() const
{
    QMutexLocker locker(&mutex_);
    return state_;
}
