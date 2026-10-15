// File: src/ui/workflows/project/frameworks/embedded/ProjectFrameworkEmbeddedPage.h
#pragma once

#include "../ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectFrameworkEmbeddedPage final : public ProjectFrameworksPage
{
public:
    explicit ProjectFrameworkEmbeddedPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized frameworks workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectFrameworksPage(model, parent, EnvironmentCatalog::frameworkEmbeddedDevice(), {QStringLiteral("pico-sdk"), QStringLiteral("arduino"), QStringLiteral("platformio"), QStringLiteral("esp-idf"), QStringLiteral("zephyr"), QStringLiteral("freertos"), QStringLiteral("stm32-hal"), QStringLiteral("cmsis"), QStringLiteral("tinyusb")}) {}
};
