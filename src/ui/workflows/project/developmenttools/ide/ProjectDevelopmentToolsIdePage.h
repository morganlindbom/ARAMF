// File: src/ui/workflows/project/developmenttools/ide/ProjectDevelopmentToolsIdePage.h
#pragma once

#include "../ProjectDevelopmentToolsPage.h"

class ProjectDevelopmentToolsIdePage final : public ProjectDevelopmentToolsPage
{
public:
    explicit ProjectDevelopmentToolsIdePage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectDevelopmentToolsIde workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectDevelopmentToolsPage(model, parent, ProjectDevelopmentToolsPage::Section::Ide) {}
};
