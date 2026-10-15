// File: src/ui/workflows/project/hardwarearchitecture/processors/ProjectHardwareProcessorsPage.h
#pragma once

#include "../ProjectHardwareArchitecturePage.h"

class ProjectHardwareProcessorsPage final : public ProjectHardwareArchitecturePage
{
public:
    explicit ProjectHardwareProcessorsPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectHardwareProcessors workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectHardwareArchitecturePage(model, parent, ProjectHardwareArchitecturePage::Section::Processors) {}
};
