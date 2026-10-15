// File: src/ui/workflows/project/builddelivery/buildsystems/ProjectBuildDeliveryBuildSystemsPage.h
#pragma once

#include "../ProjectBuildDeliveryPage.h"

class ProjectBuildDeliveryBuildSystemsPage final : public ProjectBuildDeliveryPage
{
public:
    explicit ProjectBuildDeliveryBuildSystemsPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectBuildDeliveryBuildSystems workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectBuildDeliveryPage(model, parent, ProjectBuildDeliveryPage::Section::BuildSystems) {}
};
