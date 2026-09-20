#pragma once

#include "core/ProcessVersion.h"
#include "structure/S1/ResponsibilityOwnership.h"
#include "structure/S2/PhysicalStructure.h"
#include "structure/S3/DependencyInterfaces.h"
#include <QJsonObject>
#include <QList>

namespace S4 {

enum class CompositionKind { Hosts, Contains, Embeds, Attaches, Mounts, Presents, Orchestrates };
enum class HostOperation { Mount, Unmount, Attach, Detach, Activate, Deactivate, Position, Resize, Show, Hide, Focus, SetExternalConfiguration, ConnectPublicContract };
enum class ChildIsolationMode { Encapsulated, ContractOnly, ExplicitSharedSurface };
enum class Multiplicity { Single, Multiple, Unbounded };

struct CompositionContract {
    QString id;
    QString stableKey;
    QString name;
    QString hostResponsibilityId;
    QString childResponsibilityId;
    QString hostBoundaryId;
    QString childBoundaryId;
    CompositionKind compositionKind = CompositionKind::Hosts;
    QList<HostOperation> allowedOperations;
    QStringList allowedInterfaceIds;
    ChildIsolationMode childIsolationMode = ChildIsolationMode::ContractOnly;
    Multiplicity multiplicity = Multiplicity::Single;
    QString description;
    QJsonObject metadata;
};

struct EncapsulationPolicy {
    QString id;
    QString stableKey;
    QString name;
    QString responsibilityId;
    QStringList compositionContractIds;
    bool hostAgnostic = false;
    QStringList publicSurfaceInterfaceIds;
    QList<HostOperation> allowedExternalOperations;
    QStringList protectedInternalArtifactIds;
    QString lifecycleSurface;
    QJsonObject metadata;
};

struct CompositionObservation {
    QString id;
    QString hostResponsibilityId;
    QString childResponsibilityId;
    HostOperation operation = HostOperation::Mount;
    QString interfaceId;
    QString sourceArtifactId;
    QString targetArtifactId;
    QString evidenceType;
    QString evidenceReference;
    QString status = QStringLiteral("OBSERVED");
    QJsonObject metadata;
};

struct Diagnostic {
    QString ruleCode;
    QString severity;
    QString compositionContractId;
    QString hostResponsibilityId;
    QString childResponsibilityId;
    QString operation;
    QString interfaceId;
    QString sourceArtifactId;
    QString targetArtifactId;
    QString evidenceReference;
    QString reason;
    QString suggestedAction;
    QString migrationStatus;
};

struct AuditResult {
    bool valid = true;
    QString modelFingerprint;
    QList<Diagnostic> diagnostics;
    QJsonObject toJson() const;
};

struct Configuration {
    int schemaVersion = 1;
    QList<CompositionContract> compositionContracts;
    QList<EncapsulationPolicy> encapsulationPolicies;
    QList<CompositionObservation> observations;
    ProcessVersion lifecycle = ProcessVersion(ProcessKind::Structure, 4, 1, 0, 0, 0);
    QList<ProcessVersion> lifecycleHistory;
    QJsonObject metadata;
    bool isEmpty() const { return compositionContracts.isEmpty() && encapsulationPolicies.isEmpty() && observations.isEmpty(); }
};

QString compositionKindName(CompositionKind value);
QString hostOperationName(HostOperation value);
QString isolationModeName(ChildIsolationMode value);
QString multiplicityName(Multiplicity value);
bool compositionKindFromName(const QString& value, CompositionKind* result);
bool hostOperationFromName(const QString& value, HostOperation* result);
bool isolationModeFromName(const QString& value, ChildIsolationMode* result);
bool multiplicityFromName(const QString& value, Multiplicity* result);

QJsonObject toJson(const Configuration& configuration);
bool fromJson(const QJsonValue& value, Configuration* configuration, QString* error = nullptr);
AuditResult audit(const Configuration& configuration, const S1::Configuration& ownership,
                  const S2::Configuration& physical, const S3::Configuration& dependencies);

} // namespace S4
