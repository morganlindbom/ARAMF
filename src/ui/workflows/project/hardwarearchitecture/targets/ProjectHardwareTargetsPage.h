// File: src/ui/workflows/project/hardwarearchitecture/targets/ProjectHardwareTargetsPage.h
#pragma once

#include "../ProjectHardwareArchitecturePage.h"

class ProjectHardwareTargetsPage final : public ProjectHardwareArchitecturePage
{
public:
    explicit ProjectHardwareTargetsPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectHardwareTargets workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectHardwareArchitecturePage(model, parent, ProjectHardwareArchitecturePage::Section::Targets) {}
};
