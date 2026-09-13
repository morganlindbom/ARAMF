#pragma once

#include "ProjectModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

// P1 context and coordination is one derived, agent-independent service. It
// indexes existing canonical Worker sources, but never becomes a source of
// truth for project facts, decisions, permissions, or validation.
class ContextCoordinationService final
{
public:
    static QStringList derivedPaths();
    static QJsonObject buildIndex(const ProjectModel& model);
    static QJsonObject route(const ProjectModel& model, QStringList scopes,
                             const QString& section = {});
    static QJsonObject compress(const ProjectModel& model);
    static QJsonObject freshness(const ProjectModel& model);
    static QJsonObject retrieveDecisions(const ProjectModel& model,
                                         QStringList scopes,
                                         bool includeHistory = false);

    static QJsonObject saveTaskDag(const ProjectModel& model,
                                   QJsonArray nodes,
                                   const QString& contractId);
    static QJsonObject taskDag(const ProjectModel& model);

    static QJsonObject createHandoff(const ProjectModel& model,
                                     const QJsonObject& contract,
                                     const QString& sourceAgent,
                                     const QString& targetAgent,
                                     QStringList destinationScopes,
                                     bool persist = true);

    static QJsonObject adapterDescriptors();
    static QJsonObject adaptContract(const QJsonObject& contract,
                                     const QString& adapterId);

    // Writes only derived P1 files. Existing task DAG and handoff records are
    // preserved; they are coordination state, not regenerateable project
    // configuration.
    static QJsonObject generate(const ProjectModel& model);
    static QJsonObject validate(const ProjectModel& model);
};
