#include "RuleCatalog.h"

namespace {
QList<EnvironmentOption> options(std::initializer_list<std::pair<const char*, const char*>> values)
{
    QList<EnvironmentOption> result;
    for (const auto& value : values) result.append({QString::fromUtf8(value.first), QString::fromUtf8(value.second)});
    return result;
}
}

QList<EnvironmentOption> RuleCatalog::categories()
{
    return options({
        {"Coding Standards", "coding-standards"}, {"Naming Conventions", "naming-conventions"}, {"Formatting", "formatting"}, {"Static Analysis", "static-analysis"}, {"Warning Policy", "warning-policy"}, {"Refactoring Rules", "refactoring-rules"}, {"Complexity Limits", "complexity-limits"}, {"Error Handling", "error-handling"}, {"Logging", "logging"}, {"Documentation Comments", "documentation-comments"},
        {"Architecture Boundaries", "architecture-boundaries"}, {"Module Ownership", "module-ownership"}, {"Dependency Direction", "dependency-direction"}, {"Separation of Concerns", "separation-of-concerns"}, {"Interface Contracts", "interface-contracts"}, {"Data Ownership", "data-ownership"}, {"Generated Code Boundaries", "generated-code-boundaries"}, {"Shared Component Rules", "shared-component-rules"}, {"File / Folder Ownership", "file-folder-ownership"},
        {"Unit Testing", "unit-testing"}, {"Integration Testing", "integration-testing"}, {"Regression Testing", "regression-testing"}, {"Test Naming", "test-naming"}, {"Test Coverage", "test-coverage"}, {"Hardware-in-the-loop Testing", "hardware-in-the-loop"}, {"Test Evidence", "test-evidence"}, {"Failure Reproduction", "failure-reproduction"}, {"Verification Before Completion", "verification-before-completion"},
        {"Build Must Pass", "build-must-pass"}, {"Warnings Must Be Reviewed", "warnings-reviewed"}, {"CMake Rules", "cmake-rules"}, {"Dependency Rules", "dependency-rules"}, {"Toolchain Rules", "toolchain-rules"}, {"CI Rules", "ci-rules"}, {"Release Build Rules", "release-build-rules"}, {"Generated Artifact Rules", "generated-artifact-rules"},
        {"Secret Handling", "secret-handling"}, {"Input Validation", "input-validation"}, {"Dependency Security", "dependency-security"}, {"Unsafe Operation Rules", "unsafe-operation-rules"}, {"Production Safety", "production-safety"}, {"Destructive Command Protection", "destructive-command-protection"}, {"Authentication / Authorization Rules", "authentication-authorization"}, {"Data Protection", "data-protection"},
        {"Follow AGENTS.md", "follow-agents"}, {"Respect Source of Truth", "respect-source-of-truth"}, {"Respect Project Resources", "respect-project-resources"}, {"Respect Durable Decisions", "respect-durable-decisions"}, {"Do Not Modify Protected Files", "protected-files"}, {"Do Not Invent Verification Results", "no-invented-verification"}, {"Ask Before High-Risk Actions", "ask-high-risk"}, {"Record Important Decisions", "record-decisions"}, {"Update Project Status", "update-project-status"}, {"Use Project Memory", "use-project-memory"},
        {"README Maintenance", "readme-maintenance"}, {"Architecture Documentation", "architecture-documentation"}, {"PROJECT_STATUS.md Maintenance", "project-status-maintenance"}, {"Changelog Maintenance", "changelog-maintenance"}, {"API Documentation", "api-documentation"}, {"Academic Documentation", "academic-documentation"}, {"Source References", "source-references"}
    });
}

QList<EnvironmentOption> RuleCatalog::workScopes()
{
    return options({{"Planning", "planning"}, {"Architecture", "architecture"}, {"Coding", "coding"}, {"Refactoring", "refactoring"}, {"Testing", "testing"}, {"Debugging", "debugging"}, {"Documentation", "documentation"}, {"Build / CI", "build-ci"}, {"Release", "release"}, {"Research", "research"}, {"Academic Work", "academic"}, {"Resource Analysis", "resource-analysis"}, {"Project Memory", "project-memory"}, {"Project Status", "project-status"}});
}

QList<EnvironmentOption> RuleCatalog::projectScopes()
{
    return options({{"Entire Project", "entire-project"}, {"Source Code", "source-code"}, {"Tests", "tests"}, {"Build System", "build-system"}, {"Documentation", "documentation"}, {"Resources", "resources"}, {"Generated Files", "generated-files"}, {"Configuration", "configuration"}, {"CI / CD", "ci-cd"}, {"Academic Content", "academic-content"}, {"Hardware / Firmware", "hardware-firmware"}, {"Database / Data", "database-data"}, {"UI / UX", "ui-ux"}});
}

QList<EnvironmentOption> RuleCatalog::contextPolicies()
{
    return options({{"Load only matching rule categories", "matching-categories"}, {"Avoid duplicate rule content", "avoid-duplicates"}, {"Prefer project-specific rules", "prefer-project-specific"}, {"Load summaries before full rules", "summaries-first"}, {"Avoid inactive rules", "avoid-inactive"}, {"Avoid unrelated language/framework rules", "avoid-unrelated"}, {"Reuse valid loaded context", "reuse-context"}});
}
