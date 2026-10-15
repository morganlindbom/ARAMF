// File: src/ui/workflows/project/frameworks/webserver/ProjectFrameworkWebServerPage.h
#pragma once

#include "../ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectFrameworkWebServerPage final : public ProjectFrameworksPage
{
public:
    explicit ProjectFrameworkWebServerPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized frameworks workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectFrameworksPage(model, parent, EnvironmentCatalog::frameworkWebServer(), {QStringLiteral("react"), QStringLiteral("vue"), QStringLiteral("angular"), QStringLiteral("svelte"), QStringLiteral("express"), QStringLiteral("nodejs"), QStringLiteral("django"), QStringLiteral("flask"), QStringLiteral("fastapi"), QStringLiteral("nextjs"), QStringLiteral("nuxt"), QStringLiteral("nestjs"), QStringLiteral("aspnet"), QStringLiteral("custom:Retrofit")}) {}
};
