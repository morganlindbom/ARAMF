#pragma once

#include "core/ProjectModel.h"
#include <QWidget>

class CapabilityCheckGroup;
class ProjectDevelopmentToolsPage final : public QWidget
{
public:
    explicit ProjectDevelopmentToolsPage(ProjectModel* model, QWidget* parent = nullptr);
private:
    ProjectModel* model_;
    CapabilityCheckGroup* ides_;
    CapabilityCheckGroup* versionControl_;
    CapabilityCheckGroup* support_;
    void refresh();
};
