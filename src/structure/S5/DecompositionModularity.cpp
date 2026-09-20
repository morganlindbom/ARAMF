#include "DecompositionModularity.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>
#include <functional>
#include <tuple>

namespace S5 {
namespace {
QStringList strings(const QJsonValue& value)
{
    QStringList result;
    for (const auto& item : value.toArray()) result.append(item.toString());
    return result;
}

QJsonArray array(QStringList values)
{
    std::sort(values.begin(), values.end());
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

QString enumName(int value, const QStringList& names)
{
    return value >= 0 && value < names.size() ? names.at(value) : QStringLiteral("UNKNOWN");
}

bool enumValue(const QString& value, const QStringList& names, int* result)
{
    const int index = names.indexOf(value.trimmed().toUpper());
    if (index < 0 || !result) return false;
    *result = index;
    return true;
}

QJsonObject candidateToJson(const CandidateModule& c)
{
    return {{"proposalLocalId", c.proposalLocalId}, {"name", c.name},
            {"intendedResponsibility", c.intendedResponsibility}, {"semanticConcern", c.semanticConcern},
            {"candidateArtifactIds", array(c.candidateArtifactIds)},
            {"candidateInterfaceIds", array(c.candidateInterfaceIds)},
            {"candidateDependencyIds", array(c.candidateDependencyIds)},
            {"candidateCompositionIds", array(c.candidateCompositionIds)}, {"metadata", c.metadata}};
}

CandidateModule candidateFromJson(const QJsonObject& o)
{
    CandidateModule c;
    c.proposalLocalId = o.value("proposalLocalId").toString();
    c.name = o.value("name").toString();
    c.intendedResponsibility = o.value("intendedResponsibility").toString();
    c.semanticConcern = o.value("semanticConcern").toString();
    c.candidateArtifactIds = strings(o.value("candidateArtifactIds"));
    c.candidateInterfaceIds = strings(o.value("candidateInterfaceIds"));
    c.candidateDependencyIds = strings(o.value("candidateDependencyIds"));
    c.candidateCompositionIds = strings(o.value("candidateCompositionIds"));
    c.metadata = o.value("metadata").toObject();
    return c;
}

void add(Diagnostic* d, AuditResult* result)
{
    result->diagnostics.append(*d);
    if (d->severity == QStringLiteral("ERROR") || d->severity == QStringLiteral("BLOCKER")) result->valid = false;
}

void missing(const QString& rule, const QString& id, const QString& kind, AuditResult* result,
             const QString& assessment = {}, const QString& proposal = {})
{
    Diagnostic d{rule, "ERROR", id, assessment, proposal, {}, {},
                 QStringLiteral("Unknown %1 reference: %2.").arg(kind, id),
                 QStringLiteral("Declare the referenced %1 before using it.").arg(kind),
                 "HIGH", "STRONG", "UNMAPPED"};
    add(&d, result);
}

template <typename T, typename F> bool duplicateIds(const QList<T>& values, F id,
                                        const QString& rule, AuditResult* result)
{
    QSet<QString> seen;
    bool ok = true;
    for (const auto& value : values) {
        const QString key = id(value);
        if (key.isEmpty()) { missing(rule, key, "stable ID", result); ok = false; }
        else if (seen.contains(key)) { missing(rule, key, "duplicate stable ID", result); ok = false; }
        seen.insert(key);
    }
    return ok;
}
}

QString policyModeName(PolicyMode value) { return enumName(static_cast<int>(value), {"OBSERVE", "ASSESS", "EXPLICIT"}); }
QString assessmentStatusName(AssessmentStatus value) { return enumName(static_cast<int>(value), {"DRAFT", "REVIEWED", "CANDIDATE", "ACCEPTED", "IGNORED"}); }
QString proposalStatusName(ProposalStatus value) { return enumName(static_cast<int>(value), {"PROPOSED", "REVIEWED", "ACCEPTED", "REJECTED", "DEFERRED"}); }
bool policyModeFromName(const QString& value, PolicyMode* result) { int i; const bool ok = enumValue(value.toUpper(), {"OBSERVE", "ASSESS", "EXPLICIT"}, &i); if (ok && result) *result = static_cast<PolicyMode>(i); return ok; }
bool assessmentStatusFromName(const QString& value, AssessmentStatus* result) { int i; const bool ok = enumValue(value.toUpper(), {"DRAFT", "REVIEWED", "CANDIDATE", "ACCEPTED", "IGNORED"}, &i); if (ok && result) *result = static_cast<AssessmentStatus>(i); return ok; }
bool proposalStatusFromName(const QString& value, ProposalStatus* result) { int i; const bool ok = enumValue(value.toUpper(), {"PROPOSED", "REVIEWED", "ACCEPTED", "REJECTED", "DEFERRED"}, &i); if (ok && result) *result = static_cast<ProposalStatus>(i); return ok; }

QJsonObject toJson(const Configuration& c)
{
    QList<ModularityPolicy> policies = c.modularityPolicies;
    QList<ModularityAssessment> assessments = c.assessments;
    QList<DecompositionProposal> proposals = c.decompositionProposals;
    QList<ModularityObservation> observations = c.observations;
    auto byId = [](const auto& a, const auto& b) { return a.id < b.id; };
    std::sort(policies.begin(), policies.end(), byId); std::sort(assessments.begin(), assessments.end(), byId);
    std::sort(proposals.begin(), proposals.end(), byId); std::sort(observations.begin(), observations.end(), byId);
    QJsonArray p, a, d, o;
    for (const auto& x : policies) p.append(QJsonObject{{"id",x.id},{"stableKey",x.stableKey},{"name",x.name},{"scopeResponsibilityId",x.scopeResponsibilityId},{"policyMode",policyModeName(x.policyMode)},{"recursive",x.recursive},{"semanticCohesionRequired",x.semanticCohesionRequired},{"permittedConcernCategories",array(x.permittedConcernCategories)},{"optionalThresholds",x.optionalThresholds},{"metadata",x.metadata}});
    for (const auto& x : assessments) a.append(QJsonObject{{"id",x.id},{"stableKey",x.stableKey},{"responsibilityId",x.responsibilityId},{"semanticConcerns",array(x.semanticConcerns)},{"independentChangeReasons",array(x.independentChangeReasons)},{"stateDomains",array(x.stateDomains)},{"lifecycleDomains",array(x.lifecycleDomains)},{"testDomains",array(x.testDomains)},{"dependencySignals",array(x.dependencySignals)},{"compositionSignals",array(x.compositionSignals)},{"artifactSignals",array(x.artifactSignals)},{"optionalSizeMetrics",x.optionalSizeMetrics},{"assessmentStatus",assessmentStatusName(x.assessmentStatus)},{"rationale",x.rationale},{"metadata",x.metadata}});
    for (auto& x : proposals) { std::sort(x.candidateModules.begin(), x.candidateModules.end(), [](const auto& l,const auto& r){return l.proposalLocalId<r.proposalLocalId;}); QJsonArray candidates; for (const auto& candidate : x.candidateModules) candidates.append(candidateToJson(candidate)); d.append(QJsonObject{{"id",x.id},{"stableKey",x.stableKey},{"sourceResponsibilityId",x.sourceResponsibilityId},{"candidateModules",candidates},{"rationale",x.rationale},{"evidenceReferences",array(x.evidenceReferences)},{"status",proposalStatusName(x.status)},{"metadata",x.metadata}}); }
    for (const auto& x : observations) o.append(QJsonObject{{"id",x.id},{"responsibilityId",x.responsibilityId},{"observationKind",x.observationKind},{"evidenceReference",x.evidenceReference},{"value",x.value},{"metadata",x.metadata}});
    QJsonArray history; for (const auto& v : c.lifecycleHistory) history.append(processVersionToJson(v));
    return {{"schemaVersion",c.schemaVersion},{"modularityPolicies",p},{"assessments",a},{"decompositionProposals",d},{"observations",o},{"lifecycle",processVersionToJson(c.lifecycle)},{"lifecycleHistory",history},{"metadata",c.metadata}};
}

bool fromJson(const QJsonValue& value, Configuration* c, QString* error)
{
    if (!c || !value.isObject()) { if (error) *error = QStringLiteral("S5 configuration must be an object."); return false; }
    const auto o = value.toObject(); Configuration result; result.schemaVersion = o.value("schemaVersion").toInt(1);
    for (const auto& v : o.value("modularityPolicies").toArray()) { const auto x=v.toObject(); ModularityPolicy p; p.id=x.value("id").toString(); p.stableKey=x.value("stableKey").toString(); p.name=x.value("name").toString(); p.scopeResponsibilityId=x.value("scopeResponsibilityId").toString(); if (!policyModeFromName(x.value("policyMode").toString("ASSESS"), &p.policyMode)) { if(error)*error="Invalid S5 policy mode."; return false; } p.recursive=x.value("recursive").toBool(true); p.semanticCohesionRequired=x.value("semanticCohesionRequired").toBool(true); p.permittedConcernCategories=strings(x.value("permittedConcernCategories")); p.optionalThresholds=x.value("optionalThresholds").toObject(); p.metadata=x.value("metadata").toObject(); result.modularityPolicies.append(p); }
    for (const auto& v : o.value("assessments").toArray()) { const auto x=v.toObject(); ModularityAssessment a; a.id=x.value("id").toString(); a.stableKey=x.value("stableKey").toString(); a.responsibilityId=x.value("responsibilityId").toString(); a.semanticConcerns=strings(x.value("semanticConcerns")); a.independentChangeReasons=strings(x.value("independentChangeReasons")); a.stateDomains=strings(x.value("stateDomains")); a.lifecycleDomains=strings(x.value("lifecycleDomains")); a.testDomains=strings(x.value("testDomains")); a.dependencySignals=strings(x.value("dependencySignals")); a.compositionSignals=strings(x.value("compositionSignals")); a.artifactSignals=strings(x.value("artifactSignals")); a.optionalSizeMetrics=x.value("optionalSizeMetrics").toObject(); if (!assessmentStatusFromName(x.value("assessmentStatus").toString("DRAFT"), &a.assessmentStatus)) { if(error)*error="Invalid S5 assessment status."; return false; } a.rationale=x.value("rationale").toString(); a.metadata=x.value("metadata").toObject(); result.assessments.append(a); }
    for (const auto& v : o.value("decompositionProposals").toArray()) { const auto x=v.toObject(); DecompositionProposal p; p.id=x.value("id").toString(); p.stableKey=x.value("stableKey").toString(); p.sourceResponsibilityId=x.value("sourceResponsibilityId").toString(); for (const auto& candidate : x.value("candidateModules").toArray()) p.candidateModules.append(candidateFromJson(candidate.toObject())); p.rationale=x.value("rationale").toString(); p.evidenceReferences=strings(x.value("evidenceReferences")); if (!proposalStatusFromName(x.value("status").toString("PROPOSED"), &p.status)) { if(error)*error="Invalid S5 proposal status."; return false; } p.metadata=x.value("metadata").toObject(); result.decompositionProposals.append(p); }
    for (const auto& v : o.value("observations").toArray()) { const auto x=v.toObject(); ModularityObservation q; q.id=x.value("id").toString(); q.responsibilityId=x.value("responsibilityId").toString(); q.observationKind=x.value("observationKind").toString(); q.evidenceReference=x.value("evidenceReference").toString(); q.value=x.value("value").toString(); q.metadata=x.value("metadata").toObject(); result.observations.append(q); }
    if (o.contains("lifecycle") && !processVersionFromJson(o.value("lifecycle"), &result.lifecycle, error)) return false;
    for (const auto& v : o.value("lifecycleHistory").toArray()) { ProcessVersion version; if (!processVersionFromJson(v, &version, error)) return false; result.lifecycleHistory.append(version); }
    result.metadata=o.value("metadata").toObject(); *c=result; return true;
}

QJsonObject AuditResult::toJson() const
{
    QJsonArray values; for (const auto& d : diagnostics) values.append(QJsonObject{{"ruleCode",d.ruleCode},{"severity",d.severity},{"responsibilityId",d.responsibilityId},{"assessmentId",d.assessmentId},{"proposalId",d.proposalId},{"concern",d.concern},{"evidenceReference",d.evidenceReference},{"reason",d.reason},{"suggestedAction",d.suggestedAction},{"confidence",d.confidence},{"evidenceStrength",d.evidenceStrength},{"migrationStatus",d.migrationStatus}});
    return {{"valid",valid},{"modelFingerprint",modelFingerprint},{"diagnostics",values}};
}

AuditResult audit(const Configuration& c, const S1::Configuration& s1, const S2::Configuration& s2, const S3::Configuration& s3, const S4::Configuration& s4)
{
    AuditResult result; result.modelFingerprint=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(toJson(c)).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
    QSet<QString> responsibilities, artifacts, boundaries, interfaces, dependencies, compositions;
    for (const auto& x : s1.responsibilities) responsibilities.insert(x.id);
    for (const auto& x : s1.artifacts) artifacts.insert(x.id);
    for (const auto& x : s2.boundaries) boundaries.insert(x.id);
    for (const auto& x : s3.interfaces) interfaces.insert(x.id);
    for (const auto& x : s3.dependencies) dependencies.insert(x.id);
    for (const auto& x : s4.compositionContracts) compositions.insert(x.id);
    duplicateIds(c.modularityPolicies, [](const auto& x){return x.id;}, "S5.DUPLICATE_POLICY_ID", &result);
    duplicateIds(c.assessments, [](const auto& x){return x.id;}, "S5.DUPLICATE_ASSESSMENT_ID", &result);
    duplicateIds(c.decompositionProposals, [](const auto& x){return x.id;}, "S5.DUPLICATE_PROPOSAL_ID", &result);
    duplicateIds(c.observations, [](const auto& x){return x.id;}, "S5.DUPLICATE_OBSERVATION_ID", &result);
    for (const auto& p : c.modularityPolicies) if (!p.scopeResponsibilityId.isEmpty() && !responsibilities.contains(p.scopeResponsibilityId)) missing("S5.UNKNOWN_RESPONSIBILITY",p.scopeResponsibilityId,"responsibility",&result,{},p.id);
    for (const auto& a : c.assessments) {
        if (!responsibilities.contains(a.responsibilityId)) missing("S5.UNKNOWN_RESPONSIBILITY",a.responsibilityId,"responsibility",&result,a.id);
        const int evidence = a.semanticConcerns.size()+a.independentChangeReasons.size()+a.stateDomains.size()+a.testDomains.size()+a.dependencySignals.size()+a.compositionSignals.size()+a.artifactSignals.size();
        const bool unrelated = a.metadata.value("unrelatedConcerns").toBool(false) || a.metadata.value("unrelatedStateDomains").toBool(false) || a.metadata.value("unrelatedTestDomains").toBool(false);
        if (a.semanticConcerns.size()>1 && unrelated) { Diagnostic d{"S5.MULTIPLE_UNRELATED_CONCERNS","WARNING",a.responsibilityId,a.id,{},a.semanticConcerns.join(","),{},"Assessment identifies multiple unrelated semantic concerns.","Review semantic responsibility boundaries and record a proposal if justified.","HIGH","STRONG","UNMAPPED"}; add(&d,&result); }
        if (a.independentChangeReasons.size()>1 && !a.metadata.value("relatedChangeReasons").toBool(false)) { Diagnostic d{"S5.INDEPENDENT_CHANGE_REASONS","WARNING",a.responsibilityId,a.id,{}, {},{},"Assessment identifies multiple independent reasons to change.","Evaluate a semantic decomposition proposal; do not split on size alone.","HIGH","STRONG","UNMAPPED"}; add(&d,&result); }
        if (evidence >= 4 && (unrelated || a.independentChangeReasons.size()>1)) { Diagnostic d{"S5.SUSPECTED_UNDER_DECOMPOSITION","WARNING",a.responsibilityId,a.id,{}, {},{},"Several independent modularity signals converge on this responsibility.","Review the responsibility recursively and document meaningful child boundaries if needed.","HIGH","STRONG","UNMAPPED"}; add(&d,&result); }
        if (a.metadata.value("fragmentationSignal").toBool(false) && !a.metadata.value("independentMeaning").toBool(true)) { Diagnostic d{"S5.SUSPECTED_OVER_FRAGMENTATION","WARNING",a.responsibilityId,a.id,{}, {},{},"The assessment indicates a mechanically fragmented responsibility without independent meaning.","Consolidate only after explicit semantic review.","MEDIUM","MODERATE","UNMAPPED"}; add(&d,&result); }
        const auto refs = a.metadata;
        for (const auto& id : strings(refs.value("physicalBoundaryIds"))) if (!boundaries.contains(id)) missing("S5.UNKNOWN_S2_REFERENCE",id,"physical boundary",&result,a.id);
        for (const auto& id : strings(refs.value("dependencyIds"))) if (!dependencies.contains(id)) missing("S5.UNKNOWN_S3_REFERENCE",id,"dependency",&result,a.id);
        for (const auto& id : strings(refs.value("interfaceIds"))) if (!interfaces.contains(id)) missing("S5.UNKNOWN_S3_REFERENCE",id,"interface",&result,a.id);
        for (const auto& id : strings(refs.value("compositionIds"))) if (!compositions.contains(id)) missing("S5.UNKNOWN_S4_REFERENCE",id,"composition",&result,a.id);
        for (const auto& p : c.modularityPolicies) for (auto it=p.optionalThresholds.begin(); it!=p.optionalThresholds.end(); ++it) if (!it.value().isDouble() || it.value().toDouble()<0) { Diagnostic d{"S5.INVALID_OPTIONAL_THRESHOLD","ERROR",p.scopeResponsibilityId,{},p.id,it.key(),{},"Optional threshold must be a non-negative number.","Remove the malformed threshold or provide a project-specific numeric value.","HIGH","STRONG","UNMAPPED"}; add(&d,&result); }
    }
    for (const auto& p : c.decompositionProposals) {
        if (!responsibilities.contains(p.sourceResponsibilityId)) missing("S5.UNKNOWN_RESPONSIBILITY",p.sourceResponsibilityId,"source responsibility",&result,{},p.id);
        QSet<QString> local;
        for (const auto& candidate : p.candidateModules) {
            if (candidate.proposalLocalId.isEmpty() || local.contains(candidate.proposalLocalId)) { Diagnostic d{"S5.NONDETERMINISTIC_CANDIDATE_ID","ERROR",p.sourceResponsibilityId,{},p.id,{}, {},"Candidate module IDs must be present and unique within the proposal.","Assign deterministic proposal-local candidate IDs.","HIGH","STRONG","UNMAPPED"}; add(&d,&result); }
            local.insert(candidate.proposalLocalId);
            if (candidate.metadata.value("authoritativeS1ResponsibilityId").isString()) { Diagnostic d{"S5.PROPOSAL_PRETENDS_S1_AUTHORITY","ERROR",p.sourceResponsibilityId,{},p.id,{}, {},"A decomposition proposal cannot create authoritative S1 responsibility identity.","Keep the candidate proposal-local and use a later governed S1 change for adoption.","HIGH","STRONG","UNMAPPED"}; add(&d,&result); }
            for (const auto& id : candidate.candidateArtifactIds) if (!artifacts.contains(id)) missing("S5.UNKNOWN_ARTIFACT",id,"artifact",&result,{},p.id);
            for (const auto& id : candidate.candidateInterfaceIds) if (!interfaces.contains(id)) missing("S5.UNKNOWN_S3_REFERENCE",id,"interface",&result,{},p.id);
            for (const auto& id : candidate.candidateDependencyIds) if (!dependencies.contains(id)) missing("S5.UNKNOWN_S3_REFERENCE",id,"dependency",&result,{},p.id);
            for (const auto& id : candidate.candidateCompositionIds) if (!compositions.contains(id)) missing("S5.UNKNOWN_S4_REFERENCE",id,"composition",&result,{},p.id);
        }
    }
    for (const auto& o : c.observations) if (!o.responsibilityId.isEmpty() && !responsibilities.contains(o.responsibilityId)) missing("S5.UNKNOWN_RESPONSIBILITY",o.responsibilityId,"responsibility",&result,{},o.id);
    std::sort(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& a,const auto& b){return std::tie(a.ruleCode,a.responsibilityId,a.assessmentId,a.proposalId,a.evidenceReference)<std::tie(b.ruleCode,b.responsibilityId,b.assessmentId,b.proposalId,b.evidenceReference);});
    return result;
}
} // namespace S5
