#include "RuntimeOwnershipService.h"
#include "WorkerTaskServices.h"

#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

#include <algorithm>

namespace {
QMutex& mutex()
{
    static QMutex value;
    return value;
}

QJsonArray resources(const QStringList& values)
{
    QStringList normalized = values;
    normalized.removeDuplicates();
    normalized.sort();
    return QJsonArray::fromStringList(normalized);
}

QJsonObject result(bool granted, const QString& code, const QString& message = {})
{
    QJsonObject value{{QStringLiteral("granted"), granted}, {QStringLiteral("code"), code}};
    if (!message.isEmpty()) value.insert(QStringLiteral("message"), message);
    return value;
}

QJsonArray claims(const ProjectModel& model)
{ return model.runtimeOwnershipState().value(QStringLiteral("claims")).toArray(); }
}

QString RuntimeOwnershipService::canonicalResource(const ProjectModel& model,
                                                   const QString& resource,
                                                   bool* valid)
{
    const QString candidate = resource.trimmed().replace('\\', '/');
    const QString root = QDir::cleanPath(model.projectPath());
    if (candidate.isEmpty() || root.isEmpty() || candidate.startsWith(QStringLiteral("external:"))) {
        if (valid) *valid = false;
        return {};
    }
    const QString absolute = QFileInfo(QDir(root).filePath(candidate)).absoluteFilePath();
    const QString canonicalRoot = QFileInfo(root).absoluteFilePath();
    const QString normalized = QDir::cleanPath(absolute).replace('\\', '/');
    const QString rootNormalized = QDir::cleanPath(canonicalRoot).replace('\\', '/');
    const bool inside = normalized == rootNormalized || normalized.startsWith(rootNormalized + '/');
    if (valid) *valid = inside;
    return inside ? QDir(rootNormalized).relativeFilePath(normalized).replace('\\', '/') : QString();
}

QJsonObject RuntimeOwnershipService::claim(ProjectModel* model, const QJsonObject& contract,
                                            const QString& taskId, const QString& workerId,
                                            QStringList requestedResources)
{
    if (!model || taskId.trimmed().isEmpty() || workerId.trimmed().isEmpty())
        return result(false, QStringLiteral("OWNERSHIP_REQUEST_INVALID"));
    QMutexLocker locker(&mutex());
    if (contract.value(QStringLiteral("contractId")).toString().isEmpty()
        || contract.value(QStringLiteral("binding")).toObject().value(QStringLiteral("projectId")).toString() != model->projectId()
        || !QStringList{QStringLiteral("READY"), QStringLiteral("READY_WITH_WARNINGS")}.contains(
            contract.value(QStringLiteral("preflight")).toObject().value(QStringLiteral("status")).toString()))
        return result(false, QStringLiteral("TASK_CONTRACT_INVALID"));
    if (contract.contains(QStringLiteral("request"))) {
        const auto authoritative = WorkerTaskServices::prepare(*model,
            WorkerTaskRequest::fromJson(contract.value(QStringLiteral("request")).toObject()));
        const QString status = authoritative.value(QStringLiteral("preflight")).toObject().value(QStringLiteral("status")).toString();
        if (status != QStringLiteral("READY") && status != QStringLiteral("READY_WITH_WARNINGS"))
            return result(false, QStringLiteral("TASK_CONTRACT_INVALID"));
        if (authoritative.value(QStringLiteral("permittedFiles")) != contract.value(QStringLiteral("permittedFiles")))
            return result(false, QStringLiteral("TASK_CONTRACT_INVALID"));
    }

    QStringList permitted;
    for (const auto& value : contract.value(QStringLiteral("permittedFiles")).toArray()) {
        bool permittedValid = false;
        const QString canonical = canonicalResource(*model, value.toString(), &permittedValid);
        if (permittedValid) permitted << canonical;
    }
    QStringList normalized;
    for (const auto& resource : requestedResources) {
        bool valid = false;
        const QString canonical = canonicalResource(*model, resource, &valid);
        if (!valid || !permitted.contains(canonical))
            return result(false, QStringLiteral("OWNERSHIP_NOT_PERMITTED"));
        normalized << canonical;
    }
    normalized.removeDuplicates();
    auto state = model->runtimeOwnershipState();
    QJsonArray current = state.value(QStringLiteral("claims")).toArray();
    for (const auto& value : current) {
        const auto existing = value.toObject();
        if (existing.value(QStringLiteral("state")).toString() != QStringLiteral("ACTIVE")) continue;
        for (const auto& resource : normalized)
            for (const auto& owned : existing.value(QStringLiteral("resources")).toArray())
                if (owned.toString() == resource)
                    return result(false, QStringLiteral("OWNERSHIP_CONFLICT"));
    }
    current.append(QJsonObject{{QStringLiteral("projectId"), model->projectId()},
                               {QStringLiteral("taskId"), taskId},
                               {QStringLiteral("workerId"), workerId},
                               {QStringLiteral("contractId"), contract.value(QStringLiteral("contractId"))},
                               {QStringLiteral("resources"), resources(normalized)},
                               {QStringLiteral("state"), QStringLiteral("ACTIVE")}});
    state.insert(QStringLiteral("schemaVersion"), 1);
    state.insert(QStringLiteral("claims"), current);
    model->setRuntimeOwnershipState(state);
    return QJsonObject{{QStringLiteral("granted"), true}, {QStringLiteral("code"), QStringLiteral("OWNERSHIP_GRANTED")},
                       {QStringLiteral("resources"), resources(normalized)}};
}

QJsonObject RuntimeOwnershipService::release(ProjectModel* model, const QString& taskId,
                                              const QString& workerId)
{
    if (!model) return result(false, QStringLiteral("OWNERSHIP_REQUEST_INVALID"));
    QMutexLocker locker(&mutex());
    auto state = model->runtimeOwnershipState();
    auto current = state.value(QStringLiteral("claims")).toArray();
    bool found = false;
    for (int i = 0; i < current.size(); ++i) {
        auto value = current.at(i);
        auto claim = value.toObject();
        if (claim.value(QStringLiteral("taskId")).toString() == taskId
            && claim.value(QStringLiteral("workerId")).toString() == workerId
            && claim.value(QStringLiteral("state")).toString() == QStringLiteral("ACTIVE")) {
            claim.insert(QStringLiteral("state"), QStringLiteral("RELEASED"));
            current[i] = claim;
            found = true;
        }
    }
    if (!found) return result(true, QStringLiteral("OWNERSHIP_ALREADY_RELEASED"));
    state.insert(QStringLiteral("claims"), current);
    model->setRuntimeOwnershipState(state);
    return result(true, QStringLiteral("OWNERSHIP_RELEASED"));
}

QJsonObject RuntimeOwnershipService::markWorkerUnavailable(ProjectModel* model, const QString& workerId)
{
    if (!model) return result(false, QStringLiteral("OWNERSHIP_REQUEST_INVALID"));
    QMutexLocker locker(&mutex());
    auto state = model->runtimeOwnershipState();
    auto current = state.value(QStringLiteral("claims")).toArray();
    bool found = false;
    for (int i = 0; i < current.size(); ++i) {
        auto value = current.at(i);
        auto claim = value.toObject();
        if (claim.value(QStringLiteral("workerId")).toString() == workerId
            && claim.value(QStringLiteral("state")).toString() == QStringLiteral("ACTIVE")) {
            claim.insert(QStringLiteral("state"), QStringLiteral("RECOVERABLE"));
            current[i] = claim;
            found = true;
        }
    }
    if (found) { state.insert(QStringLiteral("claims"), current); model->setRuntimeOwnershipState(state); }
    return result(true, found ? QStringLiteral("OWNERSHIP_RECOVERABLE") : QStringLiteral("WORKER_NO_ACTIVE_CLAIM"));
}

QJsonObject RuntimeOwnershipService::recoverWorker(ProjectModel* model, const QString& workerId)
{
    if (!model) return result(false, QStringLiteral("OWNERSHIP_REQUEST_INVALID"));
    QMutexLocker locker(&mutex());
    auto state = model->runtimeOwnershipState();
    auto current = state.value(QStringLiteral("claims")).toArray();
    bool found = false;
    for (int i = 0; i < current.size(); ++i) {
        auto value = current.at(i);
        auto claim = value.toObject();
        if (claim.value(QStringLiteral("workerId")).toString() == workerId
            && claim.value(QStringLiteral("state")).toString() == QStringLiteral("RECOVERABLE")) {
            claim.insert(QStringLiteral("state"), QStringLiteral("RELEASED"));
            current[i] = claim;
            found = true;
        }
    }
    if (found) { state.insert(QStringLiteral("claims"), current); model->setRuntimeOwnershipState(state); }
    return result(true, found ? QStringLiteral("OWNERSHIP_RECOVERED") : QStringLiteral("NO_RECOVERABLE_CLAIM"));
}

QJsonObject RuntimeOwnershipService::inspect(const ProjectModel& model)
{
    QMutexLocker locker(&mutex());
    return model.runtimeOwnershipState();
}
