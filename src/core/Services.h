// Services.h

#pragma once

#include "ProjectModel.h"

#include <QObject>
#include <QStringList>

struct TemplateDefinition {
    QString id;
    QString displayName;
    QString projectType;
    DevelopmentEnvironment environment;
    DevelopmentCapabilities capabilities;
    AcademicConfiguration academic;
    AiConfiguration ai;
    QStringList recommendedRules;
    QStringList recommendedResources;
    QStringList recommendedAiConfiguration;
    QStringList supportedCapabilities;
};

class TemplateManager final : public QObject
{
    Q_OBJECT

public:
    explicit TemplateManager(QObject* parent = nullptr);
    QStringList builtInTemplates() const;
    TemplateDefinition definition(const QString& id) const;
    QList<TemplateDefinition> definitions() const;
    bool applyTemplate(ProjectModel* model, const QString& id) const;
};

class GenerationServices final : public QObject
{
    Q_OBJECT

public:
    explicit GenerationServices(QObject* parent = nullptr);
    QString generate(const ProjectModel& model) const;
};
