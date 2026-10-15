// File: src/ui/workflows/project/developmenttools/versioncontrol/ProjectDevelopmentToolsVersionControlPage.h
#pragma once

#include "../ProjectDevelopmentToolsPage.h"

class ProjectDevelopmentToolsVersionControlPage final : public ProjectDevelopmentToolsPage
{
public:
    explicit ProjectDevelopmentToolsVersionControlPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectDevelopmentToolsVersionControl workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectDevelopmentToolsPage(model, parent, ProjectDevelopmentToolsPage::Section::VersionControl) {}
};
