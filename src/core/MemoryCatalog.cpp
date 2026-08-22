#include "MemoryCatalog.h"

namespace {
QList<EnvironmentOption> options(std::initializer_list<std::pair<const char*, const char*>> values)
{
    QList<EnvironmentOption> result;
    for (const auto& value : values) result.append({QString::fromUtf8(value.first), QString::fromUtf8(value.second)});
    return result;
}
}

QList<EnvironmentOption> MemoryCatalog::captureCategories()
{
    return options({{"Current Project Status", "current-project-status"}, {"Current Workflow State", "current-workflow-state"}, {"Selected Template", "selected-template"}, {"Project Capabilities", "project-capabilities"}, {"Current Build State", "current-build-state"}, {"Current Test State", "current-test-state"}, {"Current Verification State", "current-verification-state"}, {"Durable Decisions", "durable-decisions"}, {"Architecture Decisions", "architecture-decisions"}, {"Tooling Decisions", "tooling-decisions"}, {"Dependency Decisions", "dependency-decisions"}, {"Resource Decisions", "resource-decisions"}, {"AI Configuration Decisions", "ai-configuration-decisions"}, {"User-Approved Exceptions", "user-approved-exceptions"}, {"Completed Tasks", "completed-tasks"}, {"Failed Attempts", "failed-attempts"}, {"Corrective Actions", "corrective-actions"}, {"Build Attempts", "build-attempts"}, {"Test Attempts", "test-attempts"}, {"Validation Results", "validation-results"}, {"Checkpoints", "checkpoints"}, {"Reusable Project Knowledge", "reusable-project-knowledge"}, {"Certification Knowledge", "certification-knowledge"}, {"Source References", "source-references"}, {"Learned Constraints", "learned-constraints"}, {"Known Limitations", "known-limitations"}, {"Known Workarounds", "known-workarounds"}, {"Human Time", "human-time"}, {"AI Time", "ai-time"}, {"Autonomous Time", "autonomous-time"}, {"Waiting Time", "waiting-time"}, {"Diagnosis Time", "diagnosis-time"}, {"Iteration Count", "iteration-count"}, {"Build Count", "build-count"}, {"Test Count", "test-count"}, {"Failure Count", "failure-count"}});
}

QList<EnvironmentOption> MemoryCatalog::maintenanceOptions()
{
    return options({{"Update current-state after meaningful work", "update-current-state"}, {"Record durable decisions when approved", "record-decisions"}, {"Record checkpoints", "record-checkpoints"}, {"Record validation results", "record-validation"}, {"Record build results", "record-build-results"}, {"Record test results", "record-test-results"}, {"Record task completion", "record-task-completion"}, {"Update PROJECT_STATUS.md", "update-project-status"}, {"Remove stale current-state entries", "remove-stale-state"}, {"Preserve append-only history", "preserve-append-only"}});
}

QList<EnvironmentOption> MemoryCatalog::validationOptions()
{
    return options({{"Validate Memory Consistency", "memory-consistency"}, {"Run Cold-Start Validation", "cold-start-validation"}, {"Validate Sequence Continuity", "sequence-continuity"}, {"Detect Conflicting Durable Decisions", "conflicting-decisions"}, {"Detect Stale Current State", "stale-current-state"}, {"Validate Referenced Resources", "referenced-resources"}, {"Validate Project Status Consistency", "project-status-consistency"}});
}

QList<EnvironmentOption> MemoryCatalog::historyOptions()
{
    return options({{"Preserve append-only event history", "event-history"}, {"Preserve durable decision history", "decision-history"}, {"Preserve validation history", "validation-history"}, {"Preserve task history", "task-history"}, {"Preserve failure / correction history", "failure-history"}, {"Preserve milestone history", "milestone-history"}});
}
