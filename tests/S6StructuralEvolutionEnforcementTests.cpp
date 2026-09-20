#include "structure/S6/StructuralEvolutionEnforcement.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <iostream>

namespace { bool check(bool value,const char* message){if(!value)std::cerr<<"FAIL: "<<message<<'\n';return value;} }

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);Q_UNUSED(app);bool ok=true;S6::Configuration empty;
    ok&=check(empty.lifecycle.identifier()=="S6.1.0.0.0","default S6 lifecycle");
    S6::Finding finding;finding.sourceAuthority="S3";finding.sourceRuleCode="S3.PRIVATE_ACCESS";finding.scopeResponsibilityId="feature";finding.severity="BLOCKER";finding.evidenceReference="evidence/s3.json";
    auto auditDecision=S6::evaluate(finding,empty,"FINALIZE",QDateTime::fromString("2026-09-20T00:00:00Z",Qt::ISODate));ok&=check(!auditDecision.blocked&&auditDecision.disposition==S6::Disposition::Record,"safe default audit is non-blocking");
    S6::EnforcementPolicy policy;policy.id="policy";policy.stableKey="policy";policy.mode=S6::Mode::Enforce;policy.maturity=S6::Maturity::Enforce;policy.enforcementTargets={"FINALIZE"};policy.defaultDisposition=S6::Disposition::Record;
    S6::RuleBinding binding;binding.id="binding";binding.stableKey="binding";binding.sourceAuthority="S3";binding.sourceRuleCode=finding.sourceRuleCode;binding.policyId="policy";binding.minimumSeverity="WARNING";binding.disposition=S6::Disposition::Block;binding.enforcementTargets={"FINALIZE"};
    S6::Configuration enforce;enforce.enforcementPolicies.append(policy);enforce.ruleBindings.append(binding);auto blocked=S6::evaluate(finding,enforce,"FINALIZE",QDateTime::fromString("2026-09-20T00:00:00Z",Qt::ISODate));ok&=check(blocked.blocked,"explicit enforce binding blocks configured target");auto other=S6::evaluate(finding,enforce,"BUILD",QDateTime::fromString("2026-09-20T00:00:00Z",Qt::ISODate));ok&=check(!other.blocked,"binding does not block unrelated target");
    auto warningPolicy=policy;warningPolicy.id="warn";warningPolicy.mode=S6::Mode::Warn;warningPolicy.maturity=S6::Maturity::Warn;warningPolicy.defaultDisposition=S6::Disposition::Warn;auto warningBinding=binding;warningBinding.id="warn-binding";warningBinding.policyId="warn";warningBinding.disposition=S6::Disposition::Warn;S6::Configuration warning;warning.enforcementPolicies.append(warningPolicy);warning.ruleBindings.append(warningBinding);auto warned=S6::evaluate(finding,warning,"FINALIZE",QDateTime::fromString("2026-09-20T00:00:00Z",Qt::ISODate));ok&=check(!warned.blocked&&warned.disposition==S6::Disposition::Warn,"warn mode surfaces without blocking");
    S6::Waiver waiver;waiver.id="waiver";waiver.findingFingerprint=S6::findingFingerprint(finding);waiver.reason="approved migration";waiver.approvedBy="reviewer";waiver.validUntil="2026-12-31T00:00:00Z";enforce.waivers.append(waiver);auto waived=S6::evaluate(finding,enforce,"FINALIZE",QDateTime::fromString("2026-09-20T00:00:00Z",Qt::ISODate));ok&=check(!waived.blocked&&waived.waiverId=="waiver","specific waiver suppresses enforcement");
    S6::MigrationOperation operation;operation.id="move";operation.operation="MOVE";operation.target="src/Feature/A.cpp";operation.affectedArtifacts={"artifact"};operation.destructive=true;operation.approvalState=S6::ApprovalState::Required;auto plan=S6::planMigration("source-fingerprint","target-state",{"feature"},{"artifact"},{operation});auto dry=S6::dryRunMigration(plan);ok&=check(dry.valid&&dry.reason.contains("DRY_RUN"),"migration dry run is non-destructive");
    S6::Configuration invalid;auto unsafePlan=plan;unsafePlan.rollbackState=S6::RollbackState::NotRequired;invalid.migrationPlans.append(unsafePlan);auto invalidAudit=S6::audit(invalid);ok&=check(!invalidAudit.valid,"multi-file/destructive migration requires rollback declaration");
    const auto json=S6::toJson(enforce);S6::Configuration round;QString error;ok&=check(S6::fromJson(json,&round,&error),"S6 persistence round trip");ok&=check(QJsonDocument(json).toJson(QJsonDocument::Compact)==QJsonDocument(S6::toJson(round)).toJson(QJsonDocument::Compact),"S6 serialization deterministic");
    return ok?0:1;
}
