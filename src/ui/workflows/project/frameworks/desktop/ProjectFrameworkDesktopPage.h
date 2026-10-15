// File: src/ui/workflows/project/frameworks/desktop/ProjectFrameworkDesktopPage.h
#pragma once

#include "../ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectFrameworkDesktopPage final : public ProjectFrameworksPage
{
public:
    explicit ProjectFrameworkDesktopPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized frameworks workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectFrameworksPage(model, parent, EnvironmentCatalog::frameworkDesktopUi(), {QStringLiteral("qt"), QStringLiteral("wxwidgets"), QStringLiteral("gtk"), QStringLiteral("dotnet"), QStringLiteral("winui"), QStringLiteral("custom")}) {}
};
