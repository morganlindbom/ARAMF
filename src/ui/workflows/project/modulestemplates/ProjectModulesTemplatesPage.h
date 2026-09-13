#pragma once

#include <QWidget>

#include "core/ProjectModel.h"
#include "core/Services.h"

class TemplateSelector;

class ProjectModulesTemplatesPage final : public QWidget
{
public:
    ProjectModulesTemplatesPage(ProjectModel* model, TemplateManager* manager,
                                 QWidget* parent = nullptr);
};
