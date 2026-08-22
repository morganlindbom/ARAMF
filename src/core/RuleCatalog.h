#pragma once

#include "EnvironmentCatalog.h"

class RuleCatalog final
{
public:
    static QList<EnvironmentOption> categories();
    static QList<EnvironmentOption> workScopes();
    static QList<EnvironmentOption> projectScopes();
    static QList<EnvironmentOption> contextPolicies();
};
