// File: src/ui/workflows/project/builddelivery/toolchains/ProjectBuildDeliveryToolchainsPage.h
#pragma once

#include "../ProjectBuildDeliveryPage.h"

class ProjectBuildDeliveryToolchainsPage final : public ProjectBuildDeliveryPage
{
public:
    explicit ProjectBuildDeliveryToolchainsPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectBuildDeliveryToolchains workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectBuildDeliveryPage(model, parent, ProjectBuildDeliveryPage::Section::Toolchains) {}
};
