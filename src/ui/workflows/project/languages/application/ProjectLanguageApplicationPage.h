// File: src/ui/workflows/project/languages/application/ProjectLanguageApplicationPage.h
#pragma once

#include "../ProjectLanguagesPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectLanguageApplicationPage final : public ProjectLanguagesPage
{
public:
    explicit ProjectLanguageApplicationPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized languages workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectLanguagesPage(model, parent, EnvironmentCatalog::languageApplication(), {QStringLiteral("python"), QStringLiteral("sql"), QStringLiteral("dart"), QStringLiteral("php"), QStringLiteral("custom")}) {}
};
