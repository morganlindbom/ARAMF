// File: src/ui/workflows/project/hardwarearchitecture/architectures/ProjectHardwareArchitecturesPage.h
#pragma once

#include "../ProjectHardwareArchitecturePage.h"

class ProjectHardwareArchitecturesPage final : public ProjectHardwareArchitecturePage
{
public:
    explicit ProjectHardwareArchitecturesPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectHardwareArchitectures workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectHardwareArchitecturePage(model, parent, ProjectHardwareArchitecturePage::Section::Architectures) {}
};
