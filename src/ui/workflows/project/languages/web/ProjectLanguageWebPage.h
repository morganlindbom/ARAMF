// File: src/ui/workflows/project/languages/web/ProjectLanguageWebPage.h
#pragma once

#include "../ProjectLanguagesPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectLanguageWebPage final : public ProjectLanguagesPage
{
public:
    explicit ProjectLanguageWebPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized languages workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectLanguagesPage(model, parent, EnvironmentCatalog::languageWeb(), {QStringLiteral("javascript"), QStringLiteral("typescript"), QStringLiteral("html"), QStringLiteral("css")}) {}
};
