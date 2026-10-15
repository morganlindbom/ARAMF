// File: src/ui/workflows/project/builddelivery/automation/ProjectBuildDeliveryAutomationPage.h
#pragma once

#include "../ProjectBuildDeliveryPage.h"

class ProjectBuildDeliveryAutomationPage final : public ProjectBuildDeliveryPage
{
public:
    explicit ProjectBuildDeliveryAutomationPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectBuildDeliveryAutomation workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectBuildDeliveryPage(model, parent, ProjectBuildDeliveryPage::Section::Automation) {}
};
