#include "PhysicalStructure.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

namespace S2 {
namespace {
void setError(QString* target, const QString& message) { if (target) *target = message; }
QJsonArray strings(const QStringList& values) { QJsonArray result; for (const auto& value : values) result.append(value); return result; }
QStringList readStrings(const QJsonValue& value) { QStringList result; for (const auto& entry : value.toArray()) if (entry.isString()) result.append(entry.toString()); return result; }
QString severityFor(const QString& code) { return code.startsWith(QStringLiteral("S2.BLOCKER")) ? QStringLiteral("BLOCKER") : code.startsWith(QStringLiteral("S2.ERROR")) ? QStringLiteral("ERROR") : code.startsWith(QStringLiteral("S2.INFO")) ? QStringLiteral("INFO") : QStringLiteral("WARNING"); }
void add(AuditResult* result, const QString& code, const QString& reason, const QString& node = {}, const QString& responsibility = {}, const QString& artifact = {}, const QString& actual = {}, const QString& expected = {}, const QString& boundary = {}, const QString& mode = {}, const QString& action = {}, const QString& migration = QStringLiteral("S2")) {
    result->diagnostics.append({code, severityFor(code), node, responsibility, artifact, actual, expected, boundary, mode, reason, action, migration});
    if (result->diagnostics.last().severity == QStringLiteral("ERROR") || result->diagnostics.last().severity == QStringLiteral("BLOCKER")) result->valid = false;
}
bool isAbsoluteOrEscaping(const QString& path) {
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    return normalized.startsWith('/') || normalized.startsWith("//") || (normalized.size() > 1 && normalized.at(1) == ':');
}
QString normalizedRelativePath(const QString& path, bool* safe = nullptr) {
    const QString source = QDir::fromNativeSeparators(path.trimmed());
    const bool initialSafe = !source.isEmpty() && !isAbsoluteOrEscaping(source);
    QStringList parts;
    bool valid = initialSafe;
    for (const auto& part : source.split('/', Qt::SkipEmptyParts)) {
        if (part == QStringLiteral(".")) continue;
        if (part == QStringLiteral("..")) { if (parts.isEmpty()) valid = false; else parts.removeLast(); }
        else parts.append(part);
    }
    if (safe) *safe = valid;
    return parts.join('/');
}
QString boundaryFor(const Configuration& configuration, const QString& id) { for (const auto& boundary : configuration.boundaries) if (boundary.id == id) return boundary.rootRelativePath; return {}; }
bool within(const QString& path, const QString& boundary) { return path == boundary || path.startsWith(boundary.endsWith('/') ? boundary : boundary + '/'); }
}

QString placementModeName(PlacementMode mode) {
    switch (mode) { case PlacementMode::Exact: return "EXACT"; case PlacementMode::WithinBoundary: return "WITHIN_BOUNDARY"; case PlacementMode::Mirrored: return "MIRRORED"; case PlacementMode::Generated: return "GENERATED"; case PlacementMode::External: return "EXTERNAL"; case PlacementMode::LegacyUnmapped: return "LEGACY_UNMAPPED"; case PlacementMode::Ignored: return "IGNORED"; }
    return {};
}

bool placementModeFromName(const QString& value, PlacementMode* mode) {
    const QStringList names = {"EXACT", "WITHIN_BOUNDARY", "MIRRORED", "GENERATED", "EXTERNAL", "LEGACY_UNMAPPED", "IGNORED"};
    const int index = names.indexOf(value); if (index < 0) return false; if (mode) *mode = static_cast<PlacementMode>(index); return true;
}

QJsonObject toJson(const Configuration& configuration) {
    QList<PhysicalStructureNode> nodes = configuration.nodes; std::sort(nodes.begin(), nodes.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    QJsonArray nodeArray; for (const auto& node : nodes) nodeArray.append(QJsonObject{{"id", node.id}, {"stableKey", node.stableKey}, {"name", node.name}, {"parentId", node.parentId}, {"childIds", strings(node.childIds)}, {"responsibilityId", node.responsibilityId}, {"relativePath", node.relativePath}, {"role", node.role}, {"metadata", node.metadata}});
    QList<PhysicalBoundary> boundaries = configuration.boundaries; std::sort(boundaries.begin(), boundaries.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    QJsonArray boundaryArray; for (const auto& boundary : boundaries) boundaryArray.append(QJsonObject{{"id", boundary.id}, {"responsibilityId", boundary.responsibilityId}, {"rootRelativePath", boundary.rootRelativePath}, {"containmentMode", boundary.containmentMode}, {"includePatterns", strings(boundary.includePatterns)}, {"excludePatterns", strings(boundary.excludePatterns)}, {"allowedChildDirectories", strings(boundary.allowedChildDirectories)}, {"generatedDirectories", strings(boundary.generatedDirectories)}, {"externalLocationAllowed", boundary.externalLocationAllowed}, {"metadata", boundary.metadata}});
    QList<ArtifactPlacement> placements = configuration.placements; std::sort(placements.begin(), placements.end(), [](const auto& a, const auto& b) { return a.artifactId < b.artifactId; });
    QJsonArray placementArray; for (const auto& placement : placements) placementArray.append(QJsonObject{{"artifactId", placement.artifactId}, {"responsibilityId", placement.responsibilityId}, {"expectedBoundaryId", placement.expectedBoundaryId}, {"expectedRelativePath", placement.expectedRelativePath}, {"placementMode", placementModeName(placement.placementMode)}, {"role", placement.role}, {"generated", placement.generated}, {"canonicalProducer", placement.canonicalProducer}, {"metadata", placement.metadata}});
    QJsonArray history; for (const auto& version : configuration.lifecycleHistory) history.append(processVersionToJson(version));
    return QJsonObject{{"schemaVersion", configuration.schemaVersion}, {"rootId", configuration.rootId}, {"nodes", nodeArray}, {"boundaries", boundaryArray}, {"placements", placementArray}, {"lifecycle", processVersionToJson(configuration.lifecycle)}, {"lifecycleHistory", history}};
}

bool fromJson(const QJsonValue& value, Configuration* configuration, QString* error) {
    if (!configuration || !value.isObject()) { setError(error, "S2 physical structure configuration must be an object."); return false; }
    const auto object = value.toObject(); Configuration next; next.schemaVersion = object.value("schemaVersion").toInt(1); next.rootId = object.value("rootId").toString();
    for (const auto& entry : object.value("nodes").toArray()) { const auto item = entry.toObject(); PhysicalStructureNode node; node.id = item.value("id").toString(); node.stableKey = item.value("stableKey").toString(); node.name = item.value("name").toString(); node.parentId = item.value("parentId").toString(); node.childIds = readStrings(item.value("childIds")); node.responsibilityId = item.value("responsibilityId").toString(); node.relativePath = item.value("relativePath").toString(); node.role = item.value("role").toString(); node.metadata = item.value("metadata").toObject(); next.nodes.append(node); }
    for (const auto& entry : object.value("boundaries").toArray()) { const auto item = entry.toObject(); PhysicalBoundary boundary; boundary.id = item.value("id").toString(); boundary.responsibilityId = item.value("responsibilityId").toString(); boundary.rootRelativePath = item.value("rootRelativePath").toString(); boundary.containmentMode = item.value("containmentMode").toString("EXCLUSIVE"); boundary.includePatterns = readStrings(item.value("includePatterns")); boundary.excludePatterns = readStrings(item.value("excludePatterns")); boundary.allowedChildDirectories = readStrings(item.value("allowedChildDirectories")); boundary.generatedDirectories = readStrings(item.value("generatedDirectories")); boundary.externalLocationAllowed = item.value("externalLocationAllowed").toBool(); boundary.metadata = item.value("metadata").toObject(); next.boundaries.append(boundary); }
    for (const auto& entry : object.value("placements").toArray()) { const auto item = entry.toObject(); ArtifactPlacement placement; placement.artifactId = item.value("artifactId").toString(); placement.responsibilityId = item.value("responsibilityId").toString(); placement.expectedBoundaryId = item.value("expectedBoundaryId").toString(); placement.expectedRelativePath = item.value("expectedRelativePath").toString(); if (!placementModeFromName(item.value("placementMode").toString(), &placement.placementMode)) { setError(error, "Unknown S2 placement mode."); return false; } placement.role = item.value("role").toString(); placement.generated = item.value("generated").toBool(); placement.canonicalProducer = item.value("canonicalProducer").toString(); placement.metadata = item.value("metadata").toObject(); next.placements.append(placement); }
    if (object.contains("lifecycle") && !processVersionFromJson(object.value("lifecycle"), &next.lifecycle, error)) return false;
    for (const auto& entry : object.value("lifecycleHistory").toArray()) { ProcessVersion version; if (!processVersionFromJson(entry, &version, error) || !version.isStructure() || version.number != 2 || version.done != 1) { setError(error, "S2 lifecycle history contains an invalid entry."); return false; } next.lifecycleHistory.append(version); }
    if (!next.lifecycle.isStructure() || next.lifecycle.number != 2) { setError(error, "S2 lifecycle must be an S2 structure version."); return false; }
    *configuration = next; return true;
}

AuditResult audit(const Configuration& configuration, const S1::Configuration& ownership, const QString&) {
    AuditResult result; result.modelFingerprint = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(toJson(configuration)).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    QSet<QString> nodeIds; QSet<QString> stableKeys; QHash<QString, PhysicalStructureNode> nodes;
    for (const auto& node : configuration.nodes) { if (node.id.isEmpty() || nodeIds.contains(node.id)) add(&result, "S2.ERROR.DUPLICATE_STRUCTURE_ID", "Physical structure IDs must be unique.", node.id); nodeIds.insert(node.id); if (node.stableKey.isEmpty() || stableKeys.contains(node.stableKey)) add(&result, "S2.ERROR.DUPLICATE_STABLE_KEY", "Physical structure stable keys must be unique.", node.id); stableKeys.insert(node.stableKey); nodes.insert(node.id, node); bool safe = false; normalizedRelativePath(node.relativePath, &safe); if (!node.relativePath.isEmpty() && !safe) add(&result, "S2.BLOCKER.PATH_TRAVERSAL", "Physical structure path escapes the project root.", node.id, node.responsibilityId, {}, node.relativePath); }
    if (configuration.rootId.isEmpty() || !nodes.contains(configuration.rootId)) add(&result, "S2.ERROR.ROOT_MISSING", "Exactly one physical structure root is required.", configuration.rootId);
    for (const auto& node : configuration.nodes) { if (!node.parentId.isEmpty() && !nodes.contains(node.parentId)) add(&result, "S2.ERROR.INVALID_PARENT", "Physical structure parent does not exist.", node.id); if (node.parentId == node.id) add(&result, "S2.ERROR.SELF_PARENT", "A physical structure node cannot parent itself.", node.id); QSet<QString> seen; QString current = node.id; while (!current.isEmpty() && nodes.contains(current)) { if (seen.contains(current)) { add(&result, "S2.ERROR.CYCLE", "Physical structure contains a cycle.", node.id); break; } seen.insert(current); current = nodes.value(current).parentId; } }
    QSet<QString> responsibilityIds; for (const auto& node : ownership.responsibilities) responsibilityIds.insert(node.id); QSet<QString> artifactIds; for (const auto& artifact : ownership.artifacts) artifactIds.insert(artifact.id);
    QSet<QString> boundaryIds; QStringList normalizedBoundaries;
    for (const auto& boundary : configuration.boundaries) { if (boundaryIds.contains(boundary.id)) add(&result, "S2.ERROR.DUPLICATE_BOUNDARY_ID", "Physical boundary IDs must be unique.", {}, boundary.responsibilityId, {}, {}, {}, boundary.rootRelativePath); boundaryIds.insert(boundary.id); bool safe = false; const QString normalized = normalizedRelativePath(boundary.rootRelativePath, &safe); if (!safe) add(&result, "S2.BLOCKER.PATH_ESCAPE", "Boundary path escapes the project root.", {}, boundary.responsibilityId, {}, boundary.rootRelativePath); if (!boundary.responsibilityId.isEmpty() && !responsibilityIds.contains(boundary.responsibilityId) && !boundary.externalLocationAllowed) add(&result, "S2.ERROR.UNKNOWN_RESPONSIBILITY", "Boundary references an unknown S1 responsibility.", {}, boundary.responsibilityId, {}, {}, {}, normalized); if (boundary.containmentMode == QStringLiteral("EXCLUSIVE")) { for (const auto& previous : normalizedBoundaries) if (within(normalized, previous) || within(previous, normalized)) add(&result, "S2.ERROR.OVERLAPPING_EXCLUSIVE_BOUNDARY", "Exclusive physical boundaries overlap.", {}, boundary.responsibilityId, {}, {}, {}, normalized); normalizedBoundaries.append(normalized); } }
    QSet<QString> normalizedPaths;
    for (const auto& placement : configuration.placements) { if (!artifactIds.contains(placement.artifactId) && placement.placementMode != PlacementMode::External && placement.placementMode != PlacementMode::Ignored && placement.placementMode != PlacementMode::LegacyUnmapped) add(&result, "S2.ERROR.UNKNOWN_ARTIFACT", "Placement references an unknown S1 artifact.", {}, placement.responsibilityId, placement.artifactId); if (!responsibilityIds.contains(placement.responsibilityId) && placement.placementMode != PlacementMode::External && placement.placementMode != PlacementMode::Ignored && placement.placementMode != PlacementMode::LegacyUnmapped) add(&result, "S2.ERROR.UNKNOWN_RESPONSIBILITY", "Placement references an unknown S1 responsibility.", {}, placement.responsibilityId, placement.artifactId); const auto boundary = std::find_if(configuration.boundaries.cbegin(), configuration.boundaries.cend(), [&](const auto& value) { return value.id == placement.expectedBoundaryId; }); if (placement.placementMode != PlacementMode::External && placement.placementMode != PlacementMode::Ignored && placement.placementMode != PlacementMode::LegacyUnmapped && boundary == configuration.boundaries.cend()) add(&result, "S2.ERROR.MISSING_BOUNDARY", "Placement references a missing physical boundary.", {}, placement.responsibilityId, placement.artifactId); bool safe = false; const QString expected = normalizedRelativePath(placement.expectedRelativePath, &safe); if (!safe && placement.placementMode != PlacementMode::External) add(&result, "S2.BLOCKER.PATH_TRAVERSAL", "Expected artifact path escapes the project root.", {}, placement.responsibilityId, placement.artifactId, placement.expectedRelativePath); if (!expected.isEmpty() && normalizedPaths.contains(expected)) add(&result, "S2.ERROR.DUPLICATE_NORMALIZED_PATH", "Two placements use the same normalized path.", {}, placement.responsibilityId, placement.artifactId, {}, expected); normalizedPaths.insert(expected); const auto owner = std::find_if(ownership.artifacts.cbegin(), ownership.artifacts.cend(), [&](const auto& value) { return value.id == placement.artifactId; }); if (owner != ownership.artifacts.cend() && !owner->primaryOwnerId.isEmpty() && owner->primaryOwnerId != placement.responsibilityId) add(&result, "S2.ERROR.OWNERSHIP_MISMATCH", "S2 cannot redefine the S1 primary owner.", {}, placement.responsibilityId, placement.artifactId, owner->primaryOwnerId, placement.responsibilityId); if (placement.placementMode == PlacementMode::Generated && placement.canonicalProducer.isEmpty()) add(&result, "S2.ERROR.GENERATED_PRODUCER_MISSING", "Generated placement requires a canonical producer.", {}, placement.responsibilityId, placement.artifactId); if (owner != ownership.artifacts.cend() && (owner->ownershipMode == S1::OwnershipMode::LegacyUnmapped || placement.placementMode == PlacementMode::LegacyUnmapped)) add(&result, "S2.WARNING.LEGACY_UNMAPPED", "Artifact placement remains legacy-unmapped.", {}, placement.responsibilityId, placement.artifactId, {}, expected); if (boundary != configuration.boundaries.cend() && (placement.placementMode == PlacementMode::WithinBoundary || placement.placementMode == PlacementMode::Exact || placement.placementMode == PlacementMode::Generated)) { const QString root = normalizedRelativePath(boundary->rootRelativePath); if (placement.placementMode != PlacementMode::External && !within(expected, root)) add(&result, "S2.ERROR.PLACEMENT_OUTSIDE_BOUNDARY", "Expected artifact path is outside its declared boundary.", {}, placement.responsibilityId, placement.artifactId, expected, expected, root, placementModeName(placement.placementMode)); } }
    return result;
}

QJsonObject AuditResult::toJson() const { QJsonArray diagnostics; for (const auto& diagnostic : this->diagnostics) diagnostics.append(QJsonObject{{"ruleCode", diagnostic.ruleCode}, {"severity", diagnostic.severity}, {"structureNodeId", diagnostic.structureNodeId}, {"responsibilityId", diagnostic.responsibilityId}, {"artifactId", diagnostic.artifactId}, {"actualPath", diagnostic.actualPath}, {"expectedPath", diagnostic.expectedPath}, {"expectedBoundary", diagnostic.expectedBoundary}, {"placementMode", diagnostic.placementMode}, {"reason", diagnostic.reason}, {"suggestedAction", diagnostic.suggestedAction}, {"migrationStatus", diagnostic.migrationStatus}}); return QJsonObject{{"schemaVersion", 1}, {"authority", "S2"}, {"valid", valid}, {"modelFingerprint", modelFingerprint}, {"diagnostics", diagnostics}}; }
}
