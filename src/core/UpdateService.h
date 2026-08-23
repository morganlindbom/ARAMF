#pragma once

#include "ProjectModel.h"
#include "FrameworkKnowledge.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>

struct UpdateAnalysisResult {
    bool success = false;
    bool blockedByAuthority = false;
    QString status;
    QString planPath;
    QString contractPath;
    QString error;
    QJsonObject plan;
};

class UpdateService final : public QObject
{
    Q_OBJECT
public:
    explicit UpdateService(QObject* parent = nullptr);

    QList<FrameworkKnowledgeEntry> applicableApprovedKnowledge(const QString& projectRoot,
                                                                const ProjectModel& model,
                                                                QString* error = nullptr) const;
    QList<FrameworkKnowledgeEntry> approvedKnowledgeForProject(const QString& projectRoot,
                                                               const ProjectModel& model,
                                                               QString* error = nullptr) const;
    UpdateAnalysisResult analyze(const QString& projectRoot,
                                 const ProjectModel& model,
                                 const QStringList& selectedKnowledgeIds) const;
    bool apply(const QString& projectRoot,
               const ProjectModel& model,
               QString* error = nullptr) const;
    QJsonObject currentPlan(const QString& projectRoot, QString* error = nullptr) const;
    bool isPlanCurrent(const QString& projectRoot,
                       const ProjectModel& model,
                       QString* error = nullptr) const;
};
