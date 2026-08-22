#pragma once
#include "core/ProjectModel.h"
#include <QWidget>
class CapabilityCheckGroup;
class ProjectPlatformsPage final : public QWidget
{
public:
    explicit ProjectPlatformsPage(ProjectModel* model, QWidget* parent = nullptr);
private:
    ProjectModel* model_;
    CapabilityCheckGroup* hosts_;
    CapabilityCheckGroup* targets_;
    void refresh();
};
