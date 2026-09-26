#pragma once
#include "../foundations/F2/IdentityTrustFoundation.h"
#include "../foundations/F3/ScopeIntegrityFoundation.h"
class ProjectModel;
// External project adapter: imports persisted evidence and combines domain reports.
// Individual Foundations never call this adapter or read each other's stores.
class FoundationProjectValidation final {
public:
 static F2TrustReport validateF2(const QString&, QString* = nullptr);
 static F3IntegrityReport validateF3(const QString&, const ProjectModel* = nullptr, QString* = nullptr);
 static bool validateProjectIsolation(const QString&, QString* = nullptr);
};
