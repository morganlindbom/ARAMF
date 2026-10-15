// File: src/ui/workflows/project/languages/systems/ProjectLanguageSystemsPage.h
#pragma once

#include "../ProjectLanguagesPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectLanguageSystemsPage final : public ProjectLanguagesPage
{
public:
    explicit ProjectLanguageSystemsPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized languages workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectLanguagesPage(model, parent, EnvironmentCatalog::languageSystems(), {QStringLiteral("cpp"), QStringLiteral("c"), QStringLiteral("java"), QStringLiteral("kotlin"), QStringLiteral("csharp"), QStringLiteral("rust"), QStringLiteral("go"), QStringLiteral("swift"), QStringLiteral("assembly"), QStringLiteral("pio-assembly"), QStringLiteral("zig"), QStringLiteral("objective-c")}) {}
};
