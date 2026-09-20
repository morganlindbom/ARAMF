#include "structure/S5/DecompositionModularity.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <iostream>

namespace {
bool check(bool value, const char* message) { if (!value) std::cerr << "FAIL: " << message << '\n'; return value; }
S1::Configuration ownership()
{
    S1::Configuration c; c.rootId = "project";
    S1::ResponsibilityNode root; root.id="project"; root.stableKey="project"; root.name="Project"; root.kind=S1::ResponsibilityKind::Project;
    S1::ResponsibilityNode feature; feature.id="feature"; feature.stableKey="feature"; feature.name="Feature"; feature.parentId="project";
    c.responsibilities={root,feature}; return c;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); Q_UNUSED(app);
    bool ok=true; const auto s1=ownership(); S2::Configuration s2; S3::Configuration s3; S4::Configuration s4;
    S5::Configuration c; ok &= check(c.lifecycle.identifier()=="S5.1.0.0.0", "default S5 lifecycle");
    S5::ModularityAssessment assessment; assessment.id="assessment"; assessment.stableKey="assessment"; assessment.responsibilityId="feature"; assessment.semanticConcerns={"UI_PRESENTATION","PERSISTENCE"}; assessment.independentChangeReasons={"layout","format"}; assessment.metadata.insert("unrelatedConcerns",true); assessment.optionalSizeMetrics.insert("loc",1000); c.assessments.append(assessment);
    const auto audit=S5::audit(c,s1,s2,s3,s4); ok &= check(audit.valid, "warning-level modularity assessment remains auditable"); ok &= check(audit.modelFingerprint.size()==64, "audit fingerprint is SHA-256"); ok &= check(!audit.diagnostics.isEmpty(), "audit has structured diagnostics");
    S5::Configuration sizeOnly; auto sizeAssessment=assessment; sizeAssessment.id="size-only"; sizeAssessment.semanticConcerns={"UI_PRESENTATION"}; sizeAssessment.independentChangeReasons.clear(); sizeAssessment.metadata=QJsonObject{}; sizeOnly.assessments.append(sizeAssessment); const auto sizeAudit=S5::audit(sizeOnly,s1,s2,s3,s4); ok &= check(sizeAudit.valid, "size alone does not fail modularity");
    S5::DecompositionProposal proposal; proposal.id="proposal"; proposal.stableKey="proposal"; proposal.sourceResponsibilityId="feature"; S5::CandidateModule candidate; candidate.proposalLocalId="candidate-1"; candidate.name="Search"; candidate.intendedResponsibility="proposed-search"; candidate.semanticConcern="DOMAIN_LOGIC"; proposal.candidateModules.append(candidate); c.decompositionProposals.append(proposal);
    const auto serialized=S5::toJson(c); S5::Configuration roundTrip; QString error; ok &= check(S5::fromJson(serialized,&roundTrip,&error), "S5 round trip loads"); ok &= check(QJsonDocument(serialized).toJson(QJsonDocument::Compact)==QJsonDocument(S5::toJson(roundTrip)).toJson(QJsonDocument::Compact), "S5 serialization is deterministic");
    S5::DecompositionProposal bad=proposal; bad.id="bad"; bad.candidateModules.first().metadata.insert("authoritativeS1ResponsibilityId","new"); S5::Configuration invalid; invalid.decompositionProposals.append(bad); ok &= check(!S5::audit(invalid,s1,s2,s3,s4).valid, "proposal cannot create S1 authority");
    return ok ? 0 : 1;
}
