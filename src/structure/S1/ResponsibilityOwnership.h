#pragma once

#include "core/ProcessVersion.h"
#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace S1 {

enum class ResponsibilityKind { Project, Subsystem, Capability, Feature, Subfeature, Class, Document, ArtifactGroup, ResourceGroup };
enum class OwnershipMode { Exclusive, SharedExplicit, External, Generated, LegacyUnmapped, Ignored };

struct ResponsibilityNode {
    QString id;
    QString stableKey;
    QString name;
    ResponsibilityKind kind = ResponsibilityKind::Feature;
    QString description;
    QString parentId;
    QStringList childIds;
    QString ownerMetadata;
    QJsonObject metadata;
};

struct ResponsibilityArtifact {
    QString id;
    QString identity;
    QString artifactType;
    QString primaryOwnerId;
    OwnershipMode ownershipMode = OwnershipMode::LegacyUnmapped;
    QStringList participatingOwnerIds;
    QString sharedReason;
    bool generated = false;
    QString canonicalProducer;
    QString testOwnerId;
    QString documentationOwnerId;
    QString status;
    QString fingerprint;
};

struct Diagnostic {
    QString ruleCode;
    QString severity;
    QString responsibilityId;
    QString artifactId;
    QString artifactPath;
    QString expectedOwner;
    QString observedOwner;
    QString ownershipMode;
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
    QString rootId;
    QList<ResponsibilityNode> responsibilities;
    QList<ResponsibilityArtifact> artifacts;
    ProcessVersion lifecycle = ProcessVersion(ProcessKind::Structure, 1, 1, 0, 0, 0);
    QList<ProcessVersion> lifecycleHistory;

    bool isEmpty() const { return responsibilities.isEmpty() && artifacts.isEmpty(); }
};

QString responsibilityKindName(ResponsibilityKind kind);
QString ownershipModeName(OwnershipMode mode);
bool responsibilityKindFromName(const QString& value, ResponsibilityKind* kind);
bool ownershipModeFromName(const QString& value, OwnershipMode* mode);

QJsonObject toJson(const Configuration& configuration);
bool fromJson(const QJsonValue& value, Configuration* configuration, QString* error = nullptr);
AuditResult audit(const Configuration& configuration);

} // namespace S1
