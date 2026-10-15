// File: src/ui/workflows/project/frameworks/graphicsgame/ProjectFrameworkGraphicsGamePage.h
#pragma once

#include "../ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectFrameworkGraphicsGamePage final : public ProjectFrameworksPage
{
public:
    explicit ProjectFrameworkGraphicsGamePage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized frameworks workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectFrameworksPage(model, parent, EnvironmentCatalog::frameworkGraphicsGame(), {QStringLiteral("sdl"), QStringLiteral("sfml"), QStringLiteral("unreal"), QStringLiteral("unity"), QStringLiteral("godot"), QStringLiteral("opengl"), QStringLiteral("vulkan")}) {}
};
