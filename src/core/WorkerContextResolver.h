#pragma once

#include <QJsonObject>
#include <QStringList>

struct RuleConfiguration;

// Resolves the generated routing model. It is intentionally stateless: the
// Worker files remain the canonical routing source and no second route store
// is maintained in memory.
class WorkerContextResolver final
{
public:
    static QJsonObject resolve(const QString& workerRoot, QStringList scopes);
    // Directional change propagation is owned by the same canonical routes.
    static QJsonObject resolveImpact(const QString& workerRoot, QStringList scopes);
    static QJsonObject scopeRoutes(const RuleConfiguration& rules, const QString& fingerprint);
    static QJsonObject taskRoutes(const RuleConfiguration& rules, const QString& fingerprint);
};
