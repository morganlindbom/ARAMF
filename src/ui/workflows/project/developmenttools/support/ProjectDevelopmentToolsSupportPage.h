// File: src/ui/workflows/project/developmenttools/support/ProjectDevelopmentToolsSupportPage.h
#pragma once

#include "../ProjectDevelopmentToolsPage.h"

class ProjectDevelopmentToolsSupportPage final : public ProjectDevelopmentToolsPage
{
public:
    explicit ProjectDevelopmentToolsSupportPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectDevelopmentToolsSupport workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectDevelopmentToolsPage(model, parent, ProjectDevelopmentToolsPage::Section::Support) {}
};
