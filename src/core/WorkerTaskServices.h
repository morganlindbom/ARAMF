#pragma once

#include "ProjectModel.h"
#include <QJsonArray>
#include <QTextStream>

// Task intent is supplied by the caller. Permissions, dependencies, risk and
// evidence requirements are derived only from the saved ProjectModel/Worker.
struct WorkerTaskRequest {
    QString goal;
    QString type;
    QStringList scopes;
    QStringList files;
    QStringList definitionOfDone;
    bool history = false;
    bool destructive = false;
    QJsonObject toJson() const;
    static WorkerTaskRequest fromJson(const QJsonObject& value);
};

class ChangeImpactResolver final {
public:
    static QJsonObject resolve(const ProjectModel& model, const WorkerTaskRequest& task);
};

class WorkerTaskServices final {
public:
    static QJsonObject mutationPolicy(const ProjectModel& model);
    // Read-only. Includes an observed repository baseline and a gated preflight.
    static QJsonObject prepare(const ProjectModel& model, const WorkerTaskRequest& task);
    // Observes actual disk changes; never accepts a caller's changed-file list.
    // Evidence is bound to both contractId and current resultFingerprint.
    static QJsonObject postflight(const ProjectModel& model, const QJsonObject& contract,
                                 const QJsonArray& evidence = {});
};

// aramf task prepare|postflight --config <saved-project> --request/--contract <json>
// JSON goes to stdout; callers choose whether/where to retain derived evidence.
int runWorkerTaskCommand(const QStringList& arguments, QTextStream& output, QTextStream& error);
