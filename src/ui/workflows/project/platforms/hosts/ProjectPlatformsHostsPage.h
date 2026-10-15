// File: src/ui/workflows/project/platforms/hosts/ProjectPlatformsHostsPage.h
#pragma once

#include "../ProjectPlatformsPage.h"

class ProjectPlatformsHostsPage final : public ProjectPlatformsPage
{
public:
    explicit ProjectPlatformsHostsPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectPlatformsHosts workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectPlatformsPage(model, parent, ProjectPlatformsPage::Section::Hosts) {}
};
