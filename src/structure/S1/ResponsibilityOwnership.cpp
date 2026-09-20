#include "ResponsibilityOwnership.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

namespace S1 {
namespace {
void setError(QString* target, const QString& message) { if (target) *target = message; }
QJsonArray strings(const QStringList& values) { QJsonArray a; for (const auto& v : values) a.append(v); return a; }
QStringList stringList(const QJsonValue& value) { QStringList r; for (const auto& v : value.toArray()) if (v.isString()) r << v.toString(); return r; }
QString severityFor(const QString& code) { return code.startsWith("S1.ERROR") ? "ERROR" : code.startsWith("S1.BLOCKER") ? "BLOCKER" : "WARNING"; }
void add(AuditResult* result, const QString& code, const QString& reason, const QString& id = {}, const QString& artifact = {}, const QString& action = {}) {
    result->diagnostics.append({code, severityFor(code), id, artifact, artifact, {}, {}, {}, reason, action, QStringLiteral("S1")});
    if (result->diagnostics.last().severity == QStringLiteral("ERROR") || result->diagnostics.last().severity == QStringLiteral("BLOCKER")) result->valid = false;
}
}

QString responsibilityKindName(ResponsibilityKind kind) {
    switch (kind) { case ResponsibilityKind::Project: return "PROJECT"; case ResponsibilityKind::Subsystem: return "SUBSYSTEM"; case ResponsibilityKind::Capability: return "CAPABILITY"; case ResponsibilityKind::Feature: return "FEATURE"; case ResponsibilityKind::Subfeature: return "SUBFEATURE"; case ResponsibilityKind::Class: return "CLASS"; case ResponsibilityKind::Document: return "DOCUMENT"; case ResponsibilityKind::ArtifactGroup: return "ARTIFACT_GROUP"; case ResponsibilityKind::ResourceGroup: return "RESOURCE_GROUP"; }
    return {};
}
QString ownershipModeName(OwnershipMode mode) {
    switch (mode) { case OwnershipMode::Exclusive: return "EXCLUSIVE"; case OwnershipMode::SharedExplicit: return "SHARED_EXPLICIT"; case OwnershipMode::External: return "EXTERNAL"; case OwnershipMode::Generated: return "GENERATED"; case OwnershipMode::LegacyUnmapped: return "LEGACY_UNMAPPED"; case OwnershipMode::Ignored: return "IGNORED"; }
    return {};
}
bool responsibilityKindFromName(const QString& value, ResponsibilityKind* kind) {
    const QStringList names = {"PROJECT","SUBSYSTEM","CAPABILITY","FEATURE","SUBFEATURE","CLASS","DOCUMENT","ARTIFACT_GROUP","RESOURCE_GROUP"}; int i = names.indexOf(value); if (i < 0) return false; if (kind) *kind = static_cast<ResponsibilityKind>(i); return true;
}
bool ownershipModeFromName(const QString& value, OwnershipMode* mode) {
    const QStringList names = {"EXCLUSIVE","SHARED_EXPLICIT","EXTERNAL","GENERATED","LEGACY_UNMAPPED","IGNORED"}; int i = names.indexOf(value); if (i < 0) return false; if (mode) *mode = static_cast<OwnershipMode>(i); return true;
}

QJsonObject toJson(const Configuration& c) {
    QJsonArray nodes;
    QList<ResponsibilityNode> ordered = c.responsibilities;
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    for (const auto& n : ordered) nodes.append(QJsonObject{{"id",n.id},{"stableKey",n.stableKey},{"name",n.name},{"kind",responsibilityKindName(n.kind)},{"description",n.description},{"parentId",n.parentId},{"childIds",strings(n.childIds)},{"ownerMetadata",n.ownerMetadata},{"metadata",n.metadata}});
    QJsonArray artifacts;
    QList<ResponsibilityArtifact> sorted = c.artifacts;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    for (const auto& a : sorted) artifacts.append(QJsonObject{{"id",a.id},{"identity",a.identity},{"artifactType",a.artifactType},{"primaryOwnerId",a.primaryOwnerId},{"ownershipMode",ownershipModeName(a.ownershipMode)},{"participatingOwnerIds",strings(a.participatingOwnerIds)},{"sharedReason",a.sharedReason},{"generated",a.generated},{"canonicalProducer",a.canonicalProducer},{"testOwnerId",a.testOwnerId},{"documentationOwnerId",a.documentationOwnerId},{"status",a.status},{"fingerprint",a.fingerprint}});
    QJsonArray history; for (const auto& version : c.lifecycleHistory) history.append(processVersionToJson(version));
    return QJsonObject{{"schemaVersion",c.schemaVersion},{"rootId",c.rootId},{"responsibilities",nodes},{"artifacts",artifacts},{"lifecycle",processVersionToJson(c.lifecycle)},{"lifecycleHistory",history}};
}

bool fromJson(const QJsonValue& value, Configuration* c, QString* error) {
    if (!c || !value.isObject()) { setError(error, "S1 responsibility configuration must be an object."); return false; }
    const auto o = value.toObject(); Configuration next; next.schemaVersion = o.value("schemaVersion").toInt(1); next.rootId = o.value("rootId").toString();
    for (const auto& value : o.value("responsibilities").toArray()) { auto n = value.toObject(); ResponsibilityNode node; node.id=n.value("id").toString(); node.stableKey=n.value("stableKey").toString(); node.name=n.value("name").toString(); if (!responsibilityKindFromName(n.value("kind").toString(), &node.kind)) { setError(error,"Unknown S1 responsibility kind."); return false; } node.description=n.value("description").toString(); node.parentId=n.value("parentId").toString(); node.childIds=stringList(n.value("childIds")); node.ownerMetadata=n.value("ownerMetadata").toString(); node.metadata=n.value("metadata").toObject(); next.responsibilities.append(node); }
    for (const auto& value : o.value("artifacts").toArray()) { auto a=value.toObject(); ResponsibilityArtifact artifact; artifact.id=a.value("id").toString(); artifact.identity=a.value("identity").toString(); artifact.artifactType=a.value("artifactType").toString(); artifact.primaryOwnerId=a.value("primaryOwnerId").toString(); if (!ownershipModeFromName(a.value("ownershipMode").toString(), &artifact.ownershipMode)) { setError(error,"Unknown S1 ownership mode."); return false; } artifact.participatingOwnerIds=stringList(a.value("participatingOwnerIds")); artifact.sharedReason=a.value("sharedReason").toString(); artifact.generated=a.value("generated").toBool(); artifact.canonicalProducer=a.value("canonicalProducer").toString(); artifact.testOwnerId=a.value("testOwnerId").toString(); artifact.documentationOwnerId=a.value("documentationOwnerId").toString(); artifact.status=a.value("status").toString(); artifact.fingerprint=a.value("fingerprint").toString(); next.artifacts.append(artifact); }
    if (o.contains("lifecycle") && !processVersionFromJson(o.value("lifecycle"), &next.lifecycle, error)) return false;
    for (const auto& value : o.value("lifecycleHistory").toArray()) { ProcessVersion version; if (!processVersionFromJson(value, &version, error) || !version.isStructure() || version.done != 1) { setError(error,"S1 lifecycle history contains an invalid entry."); return false; } next.lifecycleHistory.append(version); }
    if (!next.lifecycle.isStructure() || next.lifecycle.number != 1) { setError(error,"S1 lifecycle must be an S1 structure version."); return false; }
    *c = next; return true;
}

AuditResult audit(const Configuration& c) {
    AuditResult r; const auto serialized = QJsonDocument(toJson(c)).toJson(QJsonDocument::Compact); r.modelFingerprint = QString::fromLatin1(QCryptographicHash::hash(serialized,QCryptographicHash::Sha256).toHex());
    QSet<QString> ids; QSet<QString> stableKeys; QHash<QString, ResponsibilityNode> nodes;
    for (const auto& n : c.responsibilities) { if (n.id.isEmpty() || ids.contains(n.id)) add(&r,"S1.ERROR.DUPLICATE_RESPONSIBILITY_ID","Responsibility IDs must be unique.",n.id); ids.insert(n.id); if (n.stableKey.isEmpty() || stableKeys.contains(n.stableKey)) add(&r,"S1.ERROR.DUPLICATE_STABLE_KEY","Stable keys must be unique.",n.id); stableKeys.insert(n.stableKey); nodes.insert(n.id,n); }
    if (c.rootId.isEmpty() || !nodes.contains(c.rootId)) add(&r,"S1.ERROR.ROOT_MISSING","Exactly one canonical S1 root responsibility is required.",c.rootId,{},"Declare one PROJECT root.");
    int projectRoots=0; for (const auto& n:c.responsibilities) if (n.kind==ResponsibilityKind::Project && n.parentId.isEmpty()) ++projectRoots; if (projectRoots!=1) add(&r,"S1.ERROR.ROOT_COUNT","Exactly one root PROJECT responsibility is required.");
    for (const auto& n:c.responsibilities) { if (!n.parentId.isEmpty() && !nodes.contains(n.parentId)) add(&r,"S1.ERROR.INVALID_PARENT","Parent responsibility does not exist.",n.id); if (n.parentId==n.id) add(&r,"S1.ERROR.SELF_PARENT","A responsibility cannot parent itself.",n.id); QSet<QString> seen; QString current=n.id; while (!current.isEmpty() && nodes.contains(current)) { if (seen.contains(current)) { add(&r,"S1.ERROR.CYCLE","Responsibility hierarchy contains a cycle.",n.id); break; } seen.insert(current); current=nodes.value(current).parentId; } }
    QSet<QString> artifactIds; for (const auto& a:c.artifacts) { if (a.id.isEmpty() || artifactIds.contains(a.id)) add(&r,"S1.ERROR.DUPLICATE_ARTIFACT_ID","Artifact IDs must be unique.",{},a.id); artifactIds.insert(a.id); const bool exempt=a.ownershipMode==OwnershipMode::External||a.ownershipMode==OwnershipMode::Generated||a.ownershipMode==OwnershipMode::LegacyUnmapped||a.ownershipMode==OwnershipMode::Ignored; if (!exempt && a.primaryOwnerId.isEmpty()) add(&r,"S1.BLOCKER.MISSING_PRIMARY_OWNER","Artifact requires one primary owner.",{},a.identity,"Declare an explicit owner."); if (!a.primaryOwnerId.isEmpty() && !nodes.contains(a.primaryOwnerId)) add(&r,"S1.ERROR.UNKNOWN_OWNER","Artifact owner does not exist.",a.primaryOwnerId,a.identity); if (a.ownershipMode==OwnershipMode::SharedExplicit && (a.participatingOwnerIds.size()<2 || a.sharedReason.trimmed().isEmpty() || a.primaryOwnerId.isEmpty())) add(&r,"S1.BLOCKER.INVALID_SHARED_OWNERSHIP","Shared ownership requires coordinating owner, participants, and reason.",a.primaryOwnerId,a.identity); for (const auto& owner:a.participatingOwnerIds) if (!nodes.contains(owner)) add(&r,"S1.ERROR.INVALID_SHARED_PARTICIPANT","Shared participant does not exist.",owner,a.identity); if (a.ownershipMode==OwnershipMode::Generated && a.canonicalProducer.isEmpty()) add(&r,"S1.ERROR.GENERATED_PRODUCER_MISSING","Generated artifact requires a canonical producer.",{},a.identity); if (!a.testOwnerId.isEmpty() && !nodes.contains(a.testOwnerId)) add(&r,"S1.ERROR.INVALID_TEST_OWNER","Test owner does not exist.",a.testOwnerId,a.identity); if (!a.documentationOwnerId.isEmpty() && !nodes.contains(a.documentationOwnerId)) add(&r,"S1.ERROR.INVALID_DOCUMENT_OWNER","Document owner does not exist.",a.documentationOwnerId,a.identity); }
    return r;
}

QJsonObject AuditResult::toJson() const { QJsonArray d; for (const auto& x:diagnostics) d.append(QJsonObject{{"ruleCode",x.ruleCode},{"severity",x.severity},{"responsibilityId",x.responsibilityId},{"artifactId",x.artifactId},{"artifactPath",x.artifactPath},{"expectedOwner",x.expectedOwner},{"observedOwner",x.observedOwner},{"ownershipMode",x.ownershipMode},{"reason",x.reason},{"suggestedAction",x.suggestedAction},{"migrationStatus",x.migrationStatus}}); return QJsonObject{{"schemaVersion",1},{"authority","S1"},{"valid",valid},{"modelFingerprint",modelFingerprint},{"diagnostics",d}}; }
}
