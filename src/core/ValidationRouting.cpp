#include "ValidationRouting.h"

#include <QJsonArray>
#include <QSet>

namespace {
bool containsAny(const QStringList& files, const QStringList& needles)
{
    for (const auto& file : files) {
        for (const auto& needle : needles) {
            if (file.contains(needle, Qt::CaseInsensitive)) return true;
        }
    }
    return false;
}

QStringList unique(const QStringList& values)
{
    QStringList result;
    QSet<QString> seen;
    for (const auto& value : values) {
        if (!seen.contains(value)) {
            seen.insert(value);
            result.append(value);
        }
    }
    return result;
}
}

QString ValidationRouting::levelName(ValidationLevel level)
{
    switch (level) {
    case ValidationLevel::Focused: return QStringLiteral("FOCUSED");
    case ValidationLevel::Subsystem: return QStringLiteral("SUBSYSTEM");
    case ValidationLevel::FullRegression: return QStringLiteral("FULL REGRESSION");
    }
    return QStringLiteral("FOCUSED");
}

ValidationPlan ValidationRouting::route(const QStringList& changedFiles,
                                        const QString& taskType,
                                        bool explicitFullRegression,
                                        bool focusedValidationFailed,
                                        bool broadImpactUncertain)
{
    const QString type = taskType.toLower();
    const bool releaseTask = explicitFullRegression || type.contains(QStringLiteral("release"))
        || type.contains(QStringLiteral("milestone")) || type.contains(QStringLiteral("certification"));
    const bool migration = containsAny(changedFiles, {QStringLiteral("migration"), QStringLiteral("ControlPlaneMigration"),
                                                       QStringLiteral("ARAMF_WORKER")});
    const bool broadArchitecture = containsAny(changedFiles, {QStringLiteral("CMakeLists.txt"), QStringLiteral("src/core/Services"),
                                                               QStringLiteral("AramfPaths"), QStringLiteral("bootstrap")});
    const bool memoryContract = containsAny(changedFiles, {QStringLiteral("ProjectMemory"), QStringLiteral("MemoryCommand"),
                                                            QStringLiteral("memory-contract"), QStringLiteral("AGENTS.md")});
    const bool subsystem = containsAny(changedFiles, {QStringLiteral("src/core"), QStringLiteral("persistence"),
                                                       QStringLiteral("resources"), QStringLiteral("generation"), QStringLiteral("verification"),
                                                       QStringLiteral("workflow"), QStringLiteral("tests")});
    const bool isolatedUi = containsAny(changedFiles, {QStringLiteral("src/ui")})
        && !containsAny(changedFiles, {QStringLiteral("src/core"), QStringLiteral("persistence"), QStringLiteral("generation"),
                                       QStringLiteral("verification"), QStringLiteral("migration"), QStringLiteral("memory")});

    ValidationPlan plan;
    plan.escalationTriggers = {QStringLiteral("focused validation failure with cross-system evidence"),
                               QStringLiteral("broad or unbounded shared impact"),
                               QStringLiteral("explicit release, milestone, or certification request")};
    if (releaseTask || migration || broadImpactUncertain || (broadArchitecture && type.contains(QStringLiteral("architecture")))) {
        plan.level = ValidationLevel::FullRegression;
        plan.requiredChecks = {QStringLiteral("production-build"), QStringLiteral("ctest"), QStringLiteral("test_250"),
                               QStringLiteral("test_550-automated"), QStringLiteral("aramf-worker-migration"),
                               QStringLiteral("application-startup"), QStringLiteral("memory-consistency"),
                               QStringLiteral("cold-start"), QStringLiteral("git-diff-check")};
        plan.rationale = QStringLiteral("Broad architecture, migration, release, milestone, certification, or explicitly unbounded risk requires full regression.");
    } else if (focusedValidationFailed || (!isolatedUi && (subsystem || memoryContract))) {
        plan.level = ValidationLevel::Subsystem;
        plan.requiredChecks = {QStringLiteral("affected-focused-tests"), QStringLiteral("ctest"), QStringLiteral("memory-consistency")};
        plan.optionalChecks = {QStringLiteral("cold-start when bootstrap, contract, discovery, or generated memory files are affected"),
                               QStringLiteral("relevant generation, verification, or startup tests")};
        plan.rationale = QStringLiteral("Shared subsystem behavior requires subsystem confidence without unrelated historical campaigns.");
    } else {
        plan.level = ValidationLevel::Focused;
        plan.requiredChecks = {QStringLiteral("affected-focused-tests"), QStringLiteral("affected-target-build"), QStringLiteral("git-diff-check")};
        plan.optionalChecks = {QStringLiteral("ctest when affected tests are part of the normal suite")};
        plan.rationale = QStringLiteral("Small isolated changes use the minimum relevant validation set.");
    }
    return plan;
}

QJsonObject ValidationRouting::policy()
{
    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("principle"), QStringLiteral("Validation is proportional to changed scope, task type, affected subsystem, and risk; use the minimum sufficient validation and escalate when evidence requires it.")},
        {QStringLiteral("levels"), QJsonObject{
            {QStringLiteral("focused"), QJsonObject{
                {QStringLiteral("default"), true},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("affected-focused-tests"), QStringLiteral("affected-target-build"), QStringLiteral("git-diff-check")}},
                {QStringLiteral("excludesByDefault"), QJsonArray{QStringLiteral("test_250"), QStringLiteral("test_550-automated"), QStringLiteral("aramf-worker-migration"), QStringLiteral("cold-start")}}}},
            {QStringLiteral("subsystem"), QJsonObject{
                {QStringLiteral("required"), QJsonArray{QStringLiteral("affected-focused-tests"), QStringLiteral("ctest"), QStringLiteral("memory-consistency")}},
                {QStringLiteral("conditional"), QJsonArray{QStringLiteral("cold-start when bootstrap, contract, or discovery changes")}},
                {QStringLiteral("excludesByDefault"), QJsonArray{QStringLiteral("test_250"), QStringLiteral("test_550-automated"), QStringLiteral("aramf-worker-migration")}}}},
            {QStringLiteral("full-regression"), QJsonObject{
                {QStringLiteral("required"), QJsonArray{QStringLiteral("production-build"), QStringLiteral("ctest"), QStringLiteral("test_250"),
                                                          QStringLiteral("test_550-automated"), QStringLiteral("aramf-worker-migration"), QStringLiteral("application-startup"),
                                                          QStringLiteral("memory-consistency"), QStringLiteral("cold-start"), QStringLiteral("git-diff-check")}}}}}},
        {QStringLiteral("routes"), QJsonObject{
            {QStringLiteral("ui"), QStringLiteral("focused unless shared workflow behavior changes")},
            {QStringLiteral("project-memory"), QStringLiteral("subsystem; cold-start only for contract/bootstrap/discovery impact")},
            {QStringLiteral("resources"), QStringLiteral("subsystem when persistence, generation, or verification is affected")},
            {QStringLiteral("persistence"), QStringLiteral("subsystem")},
            {QStringLiteral("generation"), QStringLiteral("subsystem; full only for broad architecture or explicit certification")},
            {QStringLiteral("migration"), QStringLiteral("full-regression")},
            {QStringLiteral("workflow"), QStringLiteral("focused or subsystem according to shared impact")},
            {QStringLiteral("control-plane"), QStringLiteral("full-regression")}}},
        {QStringLiteral("fullRegressionTriggers"), QJsonArray{QStringLiteral("release preparation"), QStringLiteral("explicit milestone or certification"),
                                                               QStringLiteral("major architecture or control-plane migration"), QStringLiteral("broad cross-system impact"),
                                                               QStringLiteral("unbounded shared impact"), QStringLiteral("failed lower-level validation with wider risk")}},
        {QStringLiteral("escalation"), QJsonArray{QStringLiteral("FOCUSED -> SUBSYSTEM on failure or cross-impact"),
                                                    QStringLiteral("SUBSYSTEM -> FULL REGRESSION on failure or broad uncertainty")}}
    };
}
