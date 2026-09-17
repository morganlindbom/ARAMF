#pragma once

#include "ProjectModel.h"

#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QStringList>

enum class TaskExecutionState {
    Pending,
    Blocked,
    Ready,
    Claimed,
    Running,
    Waiting,
    Succeeded,
    Failed,
    RetryPending,
    Cancelled,
    GovernanceBlocked
};

enum class ExecutionFailureCategory {
    None,
    Tool,
    Build,
    Test,
    Validation,
    Governance,
    Dependency,
    OwnershipConflict,
    StaleContext,
    WorkerUnavailable,
    TransientInfrastructure,
    PermanentTask
};

struct ExecutionTask final
{
    QString id;
    QStringList dependencies;
    QStringList resources;
    QString contractId;
    QJsonObject contract;
    TaskExecutionState state = TaskExecutionState::Pending;
    ExecutionFailureCategory failure = ExecutionFailureCategory::None;
    int retryCount = 0;
    int retryLimit = 0;
    QString workerId;
    QString handoffId;
    QString contextFingerprint;
    QJsonObject handoff;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& value, ExecutionTask* result,
                         QString* error = nullptr);
};

struct ExecutionWorker final
{
    QString id;
    bool available = true;
    QString taskId;
    QStringList resources;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& value, ExecutionWorker* result,
                         QString* error = nullptr);
};

struct ExecutionSnapshot final
{
    QString runId;
    QList<ExecutionTask> tasks;
    QList<ExecutionWorker> workers;
    QString checkpointId;
    bool interrupted = false;
    QString lastError;

    bool isValid(QString* error = nullptr) const;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& value, ExecutionSnapshot* result,
                         QString* error = nullptr);
};

// P2's single in-memory execution-state authority. ProjectModel owns the
// persisted JSON view. P1 remains authoritative for dependencies/context and
// P0 remains authoritative for contracts, validation and runtime ownership.
class ExecutionOrchestrator final
{
public:
    explicit ExecutionOrchestrator(ProjectModel* model = nullptr);

    bool configure(const QString& runId, QList<ExecutionTask> tasks,
                   QList<ExecutionWorker> workers, QString* error = nullptr);
    bool registerWorker(const QString& workerId, QStringList resources = {},
                        QString* error = nullptr);
    bool addTask(const ExecutionTask& task, QString* error = nullptr);
    bool start(QString* error = nullptr);

    bool claimTask(const QString& workerId, const QString& taskId,
                   QString* error = nullptr);
    bool startTask(const QString& workerId, const QString& taskId,
                   QString* error = nullptr);
    // executionResult is the worker's result and may contain evidence plus
    // execution-specific handoff fields. Completion is accepted only after
    // P0 postflight validation and P1 canonical handoff creation succeed.
    // There is deliberately no caller-supplied validation boolean.
    bool completeTask(const QString& workerId, const QString& taskId,
                      const QJsonObject& executionResult = {},
                      QString* error = nullptr);
    bool failTask(const QString& workerId, const QString& taskId,
                  ExecutionFailureCategory category, QString* error = nullptr);
    bool retryTask(const QString& taskId, QString* error = nullptr);
    bool cancelTask(const QString& taskId, QString* error = nullptr);

    // A worker disappearing never leaves a claim behind. Retryability is
    // decided by the central failure policy, not by the adapter.
    bool markWorkerUnavailable(const QString& workerId, QString* error = nullptr);
    bool recover(QString* error = nullptr);
    bool writeCheckpoint(QString* error = nullptr);
    bool restoreCheckpoint(const QJsonObject& checkpoint,
                           QString* error = nullptr);

    QStringList readyTaskIds() const;
    QStringList blockedTaskIds() const;
    QStringList activeTaskIds() const;
    bool isComplete() const;
    QJsonObject summary() const;
    ExecutionSnapshot snapshot() const;

    static QString stateName(TaskExecutionState state);
    static QString failureName(ExecutionFailureCategory category);
    static bool parseState(const QString& value, TaskExecutionState* result);
    static bool parseFailure(const QString& value, ExecutionFailureCategory* result);

private:
    bool refreshReadyLocked(QString* error = nullptr);
    bool validateTaskContractLocked(const ExecutionTask& task, QString* error) const;
    bool transitionLocked(ExecutionTask* task, TaskExecutionState next,
                          QString* error);
    bool releaseWorkerLocked(ExecutionTask& task, QString* error = nullptr);
    bool dependenciesSatisfiedLocked(const ExecutionTask& task) const;
    bool dependenciesFailedLocked(const ExecutionTask& task) const;
    // Diagnostic only. The decision is read from P0 RuntimeOwnershipService;
    // this is never a second ownership authority.
    bool resourceAvailableLocked(const ExecutionTask& task,
                                 const QString& workerId) const;
    ExecutionTask* findTaskLocked(const QString& taskId);
    ExecutionWorker* findWorkerLocked(const QString& workerId);
    bool persistLocked(QString* error = nullptr);

    ProjectModel* model_ = nullptr;
    mutable QMutex mutex_;
    ExecutionSnapshot state_;
};
