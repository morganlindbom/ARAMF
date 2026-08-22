#pragma once

#include "EnvironmentCatalog.h"

class MemoryCatalog final
{
public:
    static QList<EnvironmentOption> captureCategories();
    static QList<EnvironmentOption> maintenanceOptions();
    static QList<EnvironmentOption> validationOptions();
    static QList<EnvironmentOption> historyOptions();
};
