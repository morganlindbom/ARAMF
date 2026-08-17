#pragma once

#include "ProjectModel.h"
#include <QStringList>

class TemplateManager final : public QObject {
    Q_OBJECT
public:
    explicit TemplateManager(QObject* parent = nullptr);
    QStringList builtInTemplates() const;
    bool applyTemplate(ProjectModel* model, const QString& id) const;
};

class GenerationServices final : public QObject {
    Q_OBJECT
public:
    explicit GenerationServices(QObject* parent = nullptr) : QObject(parent) {}
    QString generate(const ProjectModel& model) const;
};
