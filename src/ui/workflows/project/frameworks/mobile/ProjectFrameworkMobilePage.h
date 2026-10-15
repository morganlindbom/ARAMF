// File: src/ui/workflows/project/frameworks/mobile/ProjectFrameworkMobilePage.h
#pragma once

#include "../ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectFrameworkMobilePage final : public ProjectFrameworksPage
{
public:
    explicit ProjectFrameworkMobilePage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized frameworks workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectFrameworksPage(model, parent, EnvironmentCatalog::frameworkMobile(), {QStringLiteral("flutter"), QStringLiteral("react-native"), QStringLiteral("dotnet-maui"), QStringLiteral("android-sdk"), QStringLiteral("android-emulator"), QStringLiteral("android-device-testing"), QStringLiteral("jetpack-compose"), QStringLiteral("room")}) {}
};
