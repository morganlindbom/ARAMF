#pragma once

#include "core/ProcessVersion.h"
#include "structure/S1/ResponsibilityOwnership.h"
#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace S2 {

enum class PlacementMode { Exact, WithinBoundary, Mirrored, Generated, External, LegacyUnmapped, Ignored };

struct PhysicalStructureNode {
    QString id;
    QString stableKey;
    QString name;
    QString parentId;
    QStringList childIds;
    QString responsibilityId;
    QString relativePath;
    QString role;
    QJsonObject metadata;
};

struct PhysicalBoundary {
    QString id;
    QString responsibilityId;
    QString rootRelativePath;
    QString containmentMode = QStringLiteral("EXCLUSIVE");
    QStringList includePatterns;
    QStringList excludePatterns;
    QStringList allowedChildDirectories;
    QStringList generatedDirectories;
    bool externalLocationAllowed = false;
    QJsonObject metadata;
};

struct ArtifactPlacement {
    QString artifactId;
    QString responsibilityId;
    QString expectedBoundaryId;
    QString expectedRelativePath;
    PlacementMode placementMode = PlacementMode::LegacyUnmapped;
    QString role;
    bool generated = false;
    QString canonicalProducer;
    QJsonObject metadata;
};

struct Diagnostic {
    QString ruleCode;
    QString severity;
    QString structureNodeId;
    QString responsibilityId;
    QString artifactId;
    QString actualPath;
    QString expectedPath;
    QString expectedBoundary;
    QString placementMode;
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
    QList<PhysicalStructureNode> nodes;
    QList<PhysicalBoundary> boundaries;
    QList<ArtifactPlacement> placements;
    ProcessVersion lifecycle = ProcessVersion(ProcessKind::Structure, 2, 1, 0, 0, 0);
    QList<ProcessVersion> lifecycleHistory;

    bool isEmpty() const { return nodes.isEmpty() && boundaries.isEmpty() && placements.isEmpty(); }
};

QString placementModeName(PlacementMode mode);
bool placementModeFromName(const QString& value, PlacementMode* mode);

QJsonObject toJson(const Configuration& configuration);
bool fromJson(const QJsonValue& value, Configuration* configuration, QString* error = nullptr);
AuditResult audit(const Configuration& configuration, const S1::Configuration& ownership,
                  const QString& projectRoot = {});

} // namespace S2
