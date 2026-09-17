#pragma once

#include "ProjectModel.h"

#include <QJsonObject>
#include <QStringList>

// P0 runtime ownership authority. ProjectModel owns the durable semantic
// record; this service owns the synchronized claim/release decisions.
class RuntimeOwnershipService final
{
public:
    static QJsonObject claim(ProjectModel* model, const QJsonObject& contract,
                             const QString& taskId, const QString& workerId,
                             QStringList resources);
    static QJsonObject release(ProjectModel* model, const QString& taskId,
                               const QString& workerId);
    static QJsonObject markWorkerUnavailable(ProjectModel* model,
                                              const QString& workerId);
    static QJsonObject recoverWorker(ProjectModel* model, const QString& workerId);
    static QJsonObject inspect(const ProjectModel& model);
    static QString canonicalResource(const ProjectModel& model,
                                     const QString& resource,
                                     bool* valid = nullptr);
};
