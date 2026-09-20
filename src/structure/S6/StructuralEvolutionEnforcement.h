#pragma once

#include "core/ProcessVersion.h"
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace S6 {

enum class Mode { Audit, Warn, Enforce };
enum class Maturity { Experimental, Observe, Warn, Enforce };
enum class Disposition { Ignore, Record, Warn, RequireAcknowledgement, Block, Remediate, MigrationRequired };
enum class ApprovalState { NotRequired, Required, Approved, Rejected };
enum class RollbackState { NotRequired, Available, Required, Executed, Failed };

struct Finding {
    QString sourceAuthority;
    QString sourceRuleCode;
    QString findingFingerprint;
    QString scopeResponsibilityId;
    QString severity;
    QString reason;
    QString evidenceReference;
    QJsonObject metadata;
};

struct EnforcementPolicy {
    QString id;
    QString stableKey;
    QString name;
    QString scopeResponsibilityId;
    Mode mode = Mode::Audit;
    Maturity maturity = Maturity::Observe;
    QStringList enforcementTargets;
    Disposition defaultDisposition = Disposition::Record;
    QString effectiveFrom;
    QJsonObject metadata;
};

struct RuleBinding {
    QString id;
    QString stableKey;
    QString sourceAuthority;
    QString sourceRuleCode;
    QString policyId;
    QString minimumSeverity = QStringLiteral("INFO");
    Disposition disposition = Disposition::Record;
    QStringList enforcementTargets;
    QString confidenceRequirement;
    QJsonObject metadata;
};

struct Exemption {
    QString id;
    QString stableKey;
    QString scopeResponsibilityId;
    QString ruleCode;
    QString reason;
    QString approvedBy;
    QString expiresAt;
    QString status = QStringLiteral("ACTIVE");
    QJsonObject metadata;
};

struct Waiver {
    QString id;
    QString stableKey;
    QString findingFingerprint;
    QString reason;
    QString approvedBy;
    QString validUntil;
    QString status = QStringLiteral("ACTIVE");
    QJsonObject metadata;
};

struct RemediationPlan {
    QString id;
    QString stableKey;
    QStringList findingReferences;
    QStringList proposedActions;
    bool requiresApproval = true;
    QString status = QStringLiteral("PLANNED");
    QJsonObject metadata;
};

struct MigrationOperation {
    QString id;
    QString operation;
    QString target;
    QStringList affectedArtifacts;
    bool destructive = false;
    ApprovalState approvalState = ApprovalState::Required;
    QJsonObject metadata;
};

struct MigrationPlan {
    QString id;
    QString stableKey;
    QString sourceStateFingerprint;
    QString targetStateDescription;
    QStringList affectedResponsibilities;
    QStringList affectedArtifacts;
    QList<MigrationOperation> proposedOperations;
    ApprovalState approvalState = ApprovalState::Required;
    QString executionState = QStringLiteral("PLANNED");
    RollbackState rollbackState = RollbackState::NotRequired;
    QJsonObject metadata;
};

struct Observation {
    QString id;
    QString findingFingerprint;
    QString status;
    QString evidenceReference;
    QJsonObject metadata;
};

struct EnforcementDecision {
    QString decisionId;
    QString findingAuthority;
    QString findingRuleCode;
    QString findingFingerprint;
    QString scopeResponsibilityId;
    QString policyId;
    QString ruleBindingId;
    Mode mode = Mode::Audit;
    Disposition disposition = Disposition::Record;
    QString enforcementTarget;
    bool blocked = false;
    QString reason;
    QString exemptionId;
    QString waiverId;
    QString remediationPlanId;
    QString evidenceReference;
    QJsonObject metadata;
};

struct Diagnostic {
    QString ruleCode;
    QString severity;
    QString sourceAuthority;
    QString sourceRuleCode;
    QString policyId;
    QString ruleBindingId;
    QString scopeResponsibilityId;
    QString reason;
    QString requiredAction;
    QString migrationStatus;
};

struct AuditResult {
    bool valid = true;
    QString modelFingerprint;
    QList<Diagnostic> diagnostics;
    QJsonObject toJson() const;
};

struct DryRunResult {
    bool valid = true;
    QString planFingerprint;
    QString expectedTargetState;
    QStringList affectedArtifacts;
    QStringList operations;
    QString reason;
    QJsonObject toJson() const;
};

struct Configuration {
    int schemaVersion = 1;
    QList<EnforcementPolicy> enforcementPolicies;
    QList<RuleBinding> ruleBindings;
    QList<Exemption> exemptions;
    QList<Waiver> waivers;
    QList<RemediationPlan> remediationPlans;
    QList<MigrationPlan> migrationPlans;
    QList<Observation> observations;
    ProcessVersion lifecycle = ProcessVersion(ProcessKind::Structure, 6, 1, 0, 0, 0);
    QList<ProcessVersion> lifecycleHistory;
    QJsonObject metadata;
    bool isEmpty() const { return enforcementPolicies.isEmpty() && ruleBindings.isEmpty() && exemptions.isEmpty() && waivers.isEmpty() && remediationPlans.isEmpty() && migrationPlans.isEmpty() && observations.isEmpty(); }
};

QString modeName(Mode value);
QString maturityName(Maturity value);
QString dispositionName(Disposition value);
QString approvalStateName(ApprovalState value);
QString rollbackStateName(RollbackState value);
bool modeFromName(const QString& value, Mode* result);
bool maturityFromName(const QString& value, Maturity* result);
bool dispositionFromName(const QString& value, Disposition* result);
bool approvalStateFromName(const QString& value, ApprovalState* result);
bool rollbackStateFromName(const QString& value, RollbackState* result);

QString findingFingerprint(const Finding& finding);
QJsonObject toJson(const Configuration& configuration);
bool fromJson(const QJsonValue& value, Configuration* configuration, QString* error = nullptr);
AuditResult audit(const Configuration& configuration, const QStringList& knownAuthorities = {"S1", "S2", "S3", "S4", "S5"});
EnforcementDecision evaluate(const Finding& finding, const Configuration& configuration,
                             const QString& enforcementTarget, const QDateTime& evaluationTime);
RemediationPlan planRemediation(const Finding& finding, const QStringList& actions, const QString& stableId = {});
MigrationPlan planMigration(const QString& sourceStateFingerprint, const QString& targetStateDescription,
                             const QStringList& responsibilities, const QStringList& artifacts,
                             const QList<MigrationOperation>& operations, const QString& stableId = {});
DryRunResult dryRunMigration(const MigrationPlan& plan);

} // namespace S6
