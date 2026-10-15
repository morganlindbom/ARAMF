// File: src/ui/workflows/project/languages/scripting/ProjectLanguageScriptingPage.h
#pragma once

#include "../ProjectLanguagesPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectLanguageScriptingPage final : public ProjectLanguagesPage
{
public:
    explicit ProjectLanguageScriptingPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized languages workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectLanguagesPage(model, parent, EnvironmentCatalog::languageScripting(), {QStringLiteral("bash"), QStringLiteral("powershell"), QStringLiteral("ruby"), QStringLiteral("lua")}) {}
};
