#pragma once

#include "core/ProcessVersion.h"
#include "structure/S1/ResponsibilityOwnership.h"
#include "structure/S2/PhysicalStructure.h"
#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace S3 {

enum class InterfaceVisibility { Public, Internal, Private, External };
enum class InterfaceKind { Api, Data, Resource, Persistence, Generation, Build, Test, Documentation, ExternalService, Hardware, Protocol };
enum class DependencyKind { UsesApi, ReadsData, WritesData, UsesResource, Persists, Generates, BuildsAgainst, Tests, Documents, CallsExternal, UsesProtocol };

struct InterfaceContract {
    QString id;
    QString stableKey;
    QString name;
    QString ownerResponsibilityId;
    InterfaceVisibility visibility = InterfaceVisibility::Public;
    InterfaceKind interfaceKind = InterfaceKind::Api;
    QStringList artifactIds;
    QString contractVersion;
    QString description;
    QJsonObject metadata;
};

struct DependencyEdge {
    QString id;
    QString stableKey;
    QString sourceResponsibilityId;
    QString targetResponsibilityId;
    QString interfaceId;
    DependencyKind dependencyKind = DependencyKind::UsesApi;
    QString direction = QStringLiteral("FORWARD");
    QString status = QStringLiteral("DECLARED");
    QString rationale;
    QString sourceArtifactId;
    QString targetArtifactId;
    QString sourceBoundaryId;
    QString targetBoundaryId;
    QJsonObject metadata;
};

struct DependencyObservation {
    QString id;
    QString sourceResponsibilityId;
    QString targetResponsibilityId;
    QString interfaceId;
    QString sourceArtifactId;
    QString targetArtifactId;
    QString evidenceType;
    QString evidenceReference;
    DependencyKind observedKind = DependencyKind::UsesApi;
    QString confidence;
    QString status = QStringLiteral("OBSERVED");
    QJsonObject metadata;
};

struct Diagnostic {
    QString ruleCode;
    QString severity;
    QString dependencyId;
    QString sourceResponsibilityId;
    QString targetResponsibilityId;
    QString interfaceId;
    QString dependencyKind;
    QString visibility;
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
    QList<InterfaceContract> interfaces;
    QList<DependencyEdge> dependencies;
    QList<DependencyObservation> observations;
    ProcessVersion lifecycle = ProcessVersion(ProcessKind::Structure, 3, 1, 0, 0, 0);
    QList<ProcessVersion> lifecycleHistory;
    QJsonObject metadata;

    bool isEmpty() const { return interfaces.isEmpty() && dependencies.isEmpty() && observations.isEmpty(); }
};

QString visibilityName(InterfaceVisibility value);
QString interfaceKindName(InterfaceKind value);
QString dependencyKindName(DependencyKind value);
bool visibilityFromName(const QString& value, InterfaceVisibility* result);
bool interfaceKindFromName(const QString& value, InterfaceKind* result);
bool dependencyKindFromName(const QString& value, DependencyKind* result);

QJsonObject toJson(const Configuration& configuration);
bool fromJson(const QJsonValue& value, Configuration* configuration, QString* error = nullptr);
AuditResult audit(const Configuration& configuration, const S1::Configuration& ownership,
                  const S2::Configuration& physicalStructure);

} // namespace S3
