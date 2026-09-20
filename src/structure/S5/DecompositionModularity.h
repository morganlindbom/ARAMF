#pragma once

#include "core/ProcessVersion.h"
#include "structure/S1/ResponsibilityOwnership.h"
#include "structure/S2/PhysicalStructure.h"
#include "structure/S3/DependencyInterfaces.h"
#include "structure/S4/CompositionEncapsulation.h"
#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace S5 {

enum class PolicyMode { Observe, Assess, Explicit };
enum class AssessmentStatus { Draft, Reviewed, Candidate, Accepted, Ignored };
enum class ProposalStatus { Proposed, Reviewed, Accepted, Rejected, Deferred };

struct ModularityPolicy {
    QString id;
    QString stableKey;
    QString name;
    QString scopeResponsibilityId;
    PolicyMode policyMode = PolicyMode::Assess;
    bool recursive = true;
    bool semanticCohesionRequired = true;
    QStringList permittedConcernCategories;
    QJsonObject optionalThresholds;
    QJsonObject metadata;
};

struct ModularityAssessment {
    QString id;
    QString stableKey;
    QString responsibilityId;
    QStringList semanticConcerns;
    QStringList independentChangeReasons;
    QStringList stateDomains;
    QStringList lifecycleDomains;
    QStringList testDomains;
    QStringList dependencySignals;
    QStringList compositionSignals;
    QStringList artifactSignals;
    QJsonObject optionalSizeMetrics;
    AssessmentStatus assessmentStatus = AssessmentStatus::Draft;
    QString rationale;
    QJsonObject metadata;
};

struct CandidateModule {
    QString proposalLocalId;
    QString name;
    QString intendedResponsibility;
    QString semanticConcern;
    QStringList candidateArtifactIds;
    QStringList candidateInterfaceIds;
    QStringList candidateDependencyIds;
    QStringList candidateCompositionIds;
    QJsonObject metadata;
};

struct DecompositionProposal {
    QString id;
    QString stableKey;
    QString sourceResponsibilityId;
    QList<CandidateModule> candidateModules;
    QString rationale;
    QStringList evidenceReferences;
    ProposalStatus status = ProposalStatus::Proposed;
    QJsonObject metadata;
};

struct ModularityObservation {
    QString id;
    QString responsibilityId;
    QString observationKind;
    QString evidenceReference;
    QString value;
    QJsonObject metadata;
};

struct Diagnostic {
    QString ruleCode;
    QString severity;
    QString responsibilityId;
    QString assessmentId;
    QString proposalId;
    QString concern;
    QString evidenceReference;
    QString reason;
    QString suggestedAction;
    QString confidence;
    QString evidenceStrength;
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
    QList<ModularityPolicy> modularityPolicies;
    QList<ModularityAssessment> assessments;
    QList<DecompositionProposal> decompositionProposals;
    QList<ModularityObservation> observations;
    ProcessVersion lifecycle = ProcessVersion(ProcessKind::Structure, 5, 1, 0, 0, 0);
    QList<ProcessVersion> lifecycleHistory;
    QJsonObject metadata;
    bool isEmpty() const { return modularityPolicies.isEmpty() && assessments.isEmpty() && decompositionProposals.isEmpty() && observations.isEmpty(); }
};

QString policyModeName(PolicyMode value);
QString assessmentStatusName(AssessmentStatus value);
QString proposalStatusName(ProposalStatus value);
bool policyModeFromName(const QString& value, PolicyMode* result);
bool assessmentStatusFromName(const QString& value, AssessmentStatus* result);
bool proposalStatusFromName(const QString& value, ProposalStatus* result);

QJsonObject toJson(const Configuration& configuration);
bool fromJson(const QJsonValue& value, Configuration* configuration, QString* error = nullptr);
AuditResult audit(const Configuration& configuration, const S1::Configuration& ownership,
                  const S2::Configuration& physical, const S3::Configuration& dependencies,
                  const S4::Configuration& composition);

} // namespace S5
