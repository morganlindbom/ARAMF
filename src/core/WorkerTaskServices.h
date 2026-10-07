// WorkerTaskServices.h
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
    QJsonArray relocations; // Explicit oldPath/newPath pairs; never inferred.
    QString protectedGrantId;
    QJsonObject toJson() const;
    static WorkerTaskRequest fromJson(const QJsonObject& value);
};

class ChangeImpactResolver final {
public:
    static QJsonObject resolve(const ProjectModel& model, const WorkerTaskRequest& task);
};

class WorkerTaskServices final {
public:
    // Issue an exact P6 build-registration grant using verified admin authority.

    // The immutable audit event binds the request, project, operation, base and
    // approved result bytes. Issuance never changes CMake or task permissions.
    // CLI: task grant-p6-build --config <project> --request <intent>
    // --instruction <verified-admin-text> --expires-at <ISO8601, within 24h>.
    // Copy only the returned grantId into request.protectedGrantId, regenerate
    // context via its service, and derive a new READY contract. The returned
    // registration is the exact append-only CMake edit bound by the grant.
    static QJsonObject issueP6BuildGrant(const ProjectModel& model, const WorkerTaskRequest& task,
                                        const QString& instruction, const QString& expiresAt);
    // Consume a grant only after a complete verified postflight.

    // Consumption is a locked append-only event. The read-only preparation and
    // postflight APIs reject consumed grants; callers cannot reset their use.
    // CLI: task consume-p6-build-grant --config <project> --contract <contract>
    // --evidence <evidence>. A complete verified postflight and exact CMake
    // result are mandatory. Expired/reused grants require administrator review;
    // issuance does not automatically replace any grant for the same intent.
    static QJsonObject consumeP6BuildGrant(const ProjectModel& model, const QJsonObject& contract,
                                          const QJsonArray& evidence);
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
