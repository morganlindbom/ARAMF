#pragma once

#include <QJsonObject>
#include <QStringList>

// Resolves the generated routing model. It is intentionally stateless: the
// Worker files remain the canonical routing source and no second route store
// is maintained in memory.
class WorkerContextResolver final
{
public:
    static QJsonObject resolve(const QString& workerRoot, QStringList scopes);
};
