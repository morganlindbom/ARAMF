#pragma once
#include "core/ProjectModel.h"
#include <QWidget>
class CapabilityCheckGroup;
class ProjectHardwareArchitecturePage final : public QWidget
{
public:
    explicit ProjectHardwareArchitecturePage(ProjectModel* model, QWidget* parent = nullptr);
private:
    ProjectModel* model_;
    CapabilityCheckGroup* architectures_;
    CapabilityCheckGroup* processors_;
    CapabilityCheckGroup* hardware_;
    void refresh();
};
