// File: src/ui/workflows/project/platforms/targets/ProjectPlatformsTargetsPage.h
#pragma once

#include "../ProjectPlatformsPage.h"

class ProjectPlatformsTargetsPage final : public ProjectPlatformsPage
{
public:
    explicit ProjectPlatformsTargetsPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectPlatformsTargets workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectPlatformsPage(model, parent, ProjectPlatformsPage::Section::Targets) {}
};
