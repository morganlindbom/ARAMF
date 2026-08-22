#include "AiCatalog.h"

namespace {

QList<AiOption> options(std::initializer_list<AiOption> values)
{
    return QList<AiOption>(values);
}

}

namespace AiCatalog {

QList<AiOption> agents()
{
    return options({
        {QStringLiteral("OpenAI Codex"), QStringLiteral("openai-codex"), QStringLiteral("OpenAI")},
        {QStringLiteral("ChatGPT"), QStringLiteral("chatgpt"), QStringLiteral("OpenAI")},
        {QStringLiteral("Claude Code"), QStringLiteral("claude-code"), QStringLiteral("Anthropic")},
        {QStringLiteral("Claude"), QStringLiteral("claude"), QStringLiteral("Anthropic")},
        {QStringLiteral("Gemini CLI"), QStringLiteral("gemini-cli"), QStringLiteral("Google")},
        {QStringLiteral("Gemini"), QStringLiteral("gemini"), QStringLiteral("Google")},
        {QStringLiteral("GitHub Copilot"), QStringLiteral("github-copilot"), QStringLiteral("GitHub")},
        {QStringLiteral("Microsoft Copilot"), QStringLiteral("microsoft-copilot"), QStringLiteral("Microsoft")},
        {QStringLiteral("JetBrains Junie"), QStringLiteral("jetbrains-junie"), QStringLiteral("JetBrains")},
        {QStringLiteral("Perplexity"), QStringLiteral("perplexity"), QStringLiteral("Search / Research AI")},
        {QStringLiteral("Phind"), QStringLiteral("phind"), QStringLiteral("Search / Research AI")},
        {QStringLiteral("Codeium"), QStringLiteral("codeium"), QStringLiteral("Coding / IDE")},
        {QStringLiteral("Tabby"), QStringLiteral("tabby"), QStringLiteral("Coding / IDE")},
        {QStringLiteral("Cursor"), QStringLiteral("cursor"), QStringLiteral("IDE / Editor Agents")},
        {QStringLiteral("Windsurf"), QStringLiteral("windsurf"), QStringLiteral("IDE / Editor Agents")},
        {QStringLiteral("JetBrains AI Assistant"), QStringLiteral("jetbrains-ai-assistant"), QStringLiteral("IDE / Editor Agents")},
        {QStringLiteral("Continue"), QStringLiteral("continue"), QStringLiteral("IDE / Editor Agents")},
        {QStringLiteral("Cline"), QStringLiteral("cline"), QStringLiteral("IDE / Editor Agents")},
        {QStringLiteral("Roo Code"), QStringLiteral("roo-code"), QStringLiteral("IDE / Editor Agents")},
        {QStringLiteral("Aider"), QStringLiteral("aider"), QStringLiteral("Terminal / Coding Agents")},
        {QStringLiteral("OpenCode"), QStringLiteral("opencode"), QStringLiteral("Terminal / Coding Agents")},
        {QStringLiteral("Goose"), QStringLiteral("goose"), QStringLiteral("Terminal / Coding Agents")},
        {QStringLiteral("Qwen Code"), QStringLiteral("qwen-code"), QStringLiteral("Terminal / Coding Agents")},
        {QStringLiteral("Blackbox AI"), QStringLiteral("blackbox-ai"), QStringLiteral("Other Coding Agents")},
        {QStringLiteral("Devin"), QStringLiteral("devin"), QStringLiteral("Autonomous / General Agents")},
        {QStringLiteral("OpenHands"), QStringLiteral("openhands"), QStringLiteral("Autonomous / General Agents")},
        {QStringLiteral("Replit Agent"), QStringLiteral("replit-agent"), QStringLiteral("Autonomous / General Agents")},
        {QStringLiteral("Amazon Q Developer"), QStringLiteral("amazon-q-developer"), QStringLiteral("Cloud / Vendor Assistants")},
        {QStringLiteral("Sourcegraph Cody"), QStringLiteral("sourcegraph-cody"), QStringLiteral("Cloud / Vendor Assistants")},
        {QStringLiteral("Tabnine"), QStringLiteral("tabnine"), QStringLiteral("Cloud / Vendor Assistants")},
        {QStringLiteral("Local / Self-hosted Agent"), QStringLiteral("local-self-hosted"), QStringLiteral("Other")},
        {QStringLiteral("Ollama"), QStringLiteral("ollama"), QStringLiteral("Local / Self-hosted")},
        {QStringLiteral("LM Studio"), QStringLiteral("lm-studio"), QStringLiteral("Local / Self-hosted")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom-agent"), QStringLiteral("Other")}
    });
}

QList<AiOption> responsibilities()
{
    return options({
        {QStringLiteral("Planning"), QStringLiteral("planning"), QStringLiteral("Planning / Design")},
        {QStringLiteral("Requirement Analysis"), QStringLiteral("requirement-analysis"), QStringLiteral("Planning / Design")},
        {QStringLiteral("Architecture"), QStringLiteral("architecture"), QStringLiteral("Planning / Design")},
        {QStringLiteral("Technical Design"), QStringLiteral("technical-design"), QStringLiteral("Planning / Design")},
        {QStringLiteral("Task Breakdown"), QStringLiteral("task-breakdown"), QStringLiteral("Planning / Design")},
        {QStringLiteral("Specification Writing"), QStringLiteral("specification-writing"), QStringLiteral("Planning / Design")},
        {QStringLiteral("API Design"), QStringLiteral("api-design"), QStringLiteral("Planning / Design")},
        {QStringLiteral("Database Design"), QStringLiteral("database-design"), QStringLiteral("Planning / Design")},
        {QStringLiteral("UI / UX Design"), QStringLiteral("ui-ux-design"), QStringLiteral("Planning / Design")},
        {QStringLiteral("Data Model Design"), QStringLiteral("data-model-design"), QStringLiteral("Planning / Design")},
        {QStringLiteral("Coding"), QStringLiteral("coding"), QStringLiteral("Implementation")},
        {QStringLiteral("Refactoring"), QStringLiteral("refactoring"), QStringLiteral("Implementation")},
        {QStringLiteral("Code Generation"), QStringLiteral("code-generation"), QStringLiteral("Implementation")},
        {QStringLiteral("Dependency Management"), QStringLiteral("dependency-management"), QStringLiteral("Implementation")},
        {QStringLiteral("Configuration"), QStringLiteral("configuration"), QStringLiteral("Implementation")},
        {QStringLiteral("Migration"), QStringLiteral("migration"), QStringLiteral("Implementation")},
        {QStringLiteral("Data Transformation"), QStringLiteral("data-transformation"), QStringLiteral("Implementation")},
        {QStringLiteral("Infrastructure Configuration"), QStringLiteral("infrastructure-configuration"), QStringLiteral("Implementation")},
        {QStringLiteral("Database Implementation"), QStringLiteral("database-implementation"), QStringLiteral("Implementation")},
        {QStringLiteral("API Implementation"), QStringLiteral("api-implementation"), QStringLiteral("Implementation")},
        {QStringLiteral("Code Review"), QStringLiteral("code-review"), QStringLiteral("Quality")},
        {QStringLiteral("Testing"), QStringLiteral("testing"), QStringLiteral("Quality")},
        {QStringLiteral("Debugging"), QStringLiteral("debugging"), QStringLiteral("Quality")},
        {QStringLiteral("Static Analysis"), QStringLiteral("static-analysis"), QStringLiteral("Quality")},
        {QStringLiteral("Performance Analysis"), QStringLiteral("performance-analysis"), QStringLiteral("Quality")},
        {QStringLiteral("Security Review"), QStringLiteral("security-review"), QStringLiteral("Quality")},
        {QStringLiteral("Accessibility Review"), QStringLiteral("accessibility-review"), QStringLiteral("Quality")},
        {QStringLiteral("Compatibility Review"), QStringLiteral("compatibility-review"), QStringLiteral("Quality")},
        {QStringLiteral("Regression Analysis"), QStringLiteral("regression-analysis"), QStringLiteral("Quality")},
        {QStringLiteral("Dependency Review"), QStringLiteral("dependency-review"), QStringLiteral("Quality")},
        {QStringLiteral("Documentation"), QStringLiteral("documentation"), QStringLiteral("Project Support")},
        {QStringLiteral("Research"), QStringLiteral("research"), QStringLiteral("Project Support")},
        {QStringLiteral("Build / CI"), QStringLiteral("build-ci"), QStringLiteral("Project Support")},
        {QStringLiteral("Release Preparation"), QStringLiteral("release-preparation"), QStringLiteral("Project Support")},
        {QStringLiteral("Resource Analysis"), QStringLiteral("resource-analysis"), QStringLiteral("Project Support")},
        {QStringLiteral("Project Memory Maintenance"), QStringLiteral("project-memory-maintenance"), QStringLiteral("Project Support")},
        {QStringLiteral("Project Status Maintenance"), QStringLiteral("project-status-maintenance"), QStringLiteral("Project Support")}
        ,{QStringLiteral("Dependency Research"), QStringLiteral("dependency-research"), QStringLiteral("Project Support")}
        ,{QStringLiteral("Changelog Maintenance"), QStringLiteral("changelog-maintenance"), QStringLiteral("Project Support")}
        ,{QStringLiteral("Issue / Task Management"), QStringLiteral("issue-task-management"), QStringLiteral("Project Support")}
        ,{QStringLiteral("Release Notes"), QStringLiteral("release-notes"), QStringLiteral("Project Support")}
        ,{QStringLiteral("Technical Investigation"), QStringLiteral("technical-investigation"), QStringLiteral("Project Support")}
    });
}

QList<AiOption> permissions()
{
    return options({
        {QStringLiteral("Read project files"), QStringLiteral("read-project-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Create files"), QStringLiteral("create-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Modify files"), QStringLiteral("modify-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Rename files"), QStringLiteral("rename-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Move files"), QStringLiteral("move-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Copy files"), QStringLiteral("copy-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Delete files"), QStringLiteral("delete-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Modify generated files"), QStringLiteral("modify-generated-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Modify protected files"), QStringLiteral("modify-protected-files"), QStringLiteral("File Operations")},
        {QStringLiteral("Run builds"), QStringLiteral("run-builds"), QStringLiteral("Execution")},
        {QStringLiteral("Run tests"), QStringLiteral("run-tests"), QStringLiteral("Execution")},
        {QStringLiteral("Run linters"), QStringLiteral("run-linters"), QStringLiteral("Execution")},
        {QStringLiteral("Run static analysis"), QStringLiteral("run-static-analysis"), QStringLiteral("Execution")},
        {QStringLiteral("Run project tools"), QStringLiteral("run-project-tools"), QStringLiteral("Execution")},
        {QStringLiteral("Run scripts"), QStringLiteral("run-scripts"), QStringLiteral("Execution")},
        {QStringLiteral("Run generated executables"), QStringLiteral("run-generated-executables"), QStringLiteral("Execution")},
        {QStringLiteral("Run debugging tools"), QStringLiteral("run-debugging-tools"), QStringLiteral("Execution")},
        {QStringLiteral("Run package managers"), QStringLiteral("run-package-managers"), QStringLiteral("Execution")},
        {QStringLiteral("Run database migrations"), QStringLiteral("run-database-migrations"), QStringLiteral("Execution")},
        {QStringLiteral("Start local services"), QStringLiteral("start-local-services"), QStringLiteral("Execution")},
        {QStringLiteral("Stop local services"), QStringLiteral("stop-local-services"), QStringLiteral("Execution")},
        {QStringLiteral("Modify build configuration"), QStringLiteral("modify-build-configuration"), QStringLiteral("Project Configuration")},
        {QStringLiteral("Modify dependency configuration"), QStringLiteral("modify-dependency-configuration"), QStringLiteral("Project Configuration")},
        {QStringLiteral("Modify project configuration"), QStringLiteral("modify-project-configuration"), QStringLiteral("Project Configuration")},
        {QStringLiteral("Modify CI/CD configuration"), QStringLiteral("modify-ci-cd-configuration"), QStringLiteral("Project Configuration")},
        {QStringLiteral("Modify ARAMF generated files"), QStringLiteral("modify-aramf-generated-files"), QStringLiteral("Project Configuration")},
        {QStringLiteral("Install dependencies"), QStringLiteral("install-dependencies"), QStringLiteral("Dependencies / Environment")},
        {QStringLiteral("Update dependencies"), QStringLiteral("update-dependencies"), QStringLiteral("Dependencies / Environment")},
        {QStringLiteral("Remove dependencies"), QStringLiteral("remove-dependencies"), QStringLiteral("Dependencies / Environment")},
        {QStringLiteral("Modify environment configuration"), QStringLiteral("modify-environment-configuration"), QStringLiteral("Dependencies / Environment")},
        {QStringLiteral("Create branch"), QStringLiteral("create-branch"), QStringLiteral("Version Control")},
        {QStringLiteral("Stage changes"), QStringLiteral("stage-changes"), QStringLiteral("Version Control")},
        {QStringLiteral("Create commit"), QStringLiteral("create-commit"), QStringLiteral("Version Control")},
        {QStringLiteral("Amend commit"), QStringLiteral("amend-commit"), QStringLiteral("Version Control")},
        {QStringLiteral("Merge changes"), QStringLiteral("merge-changes"), QStringLiteral("Version Control")},
        {QStringLiteral("Push changes"), QStringLiteral("push-changes"), QStringLiteral("Version Control")},
        {QStringLiteral("Create pull request"), QStringLiteral("create-pull-request"), QStringLiteral("Version Control")},
        {QStringLiteral("Rebase"), QStringLiteral("rebase"), QStringLiteral("Version Control")},
        {QStringLiteral("Resolve merge conflicts"), QStringLiteral("resolve-merge-conflicts"), QStringLiteral("Version Control")},
        {QStringLiteral("Create tag"), QStringLiteral("create-tag"), QStringLiteral("Version Control")},
        {QStringLiteral("Delete branch"), QStringLiteral("delete-branch"), QStringLiteral("Version Control")},
        {QStringLiteral("Force push"), QStringLiteral("force-push"), QStringLiteral("Version Control")},
        {QStringLiteral("Delete project files"), QStringLiteral("delete-project-files"), QStringLiteral("High-Risk Actions")},
        {QStringLiteral("Modify production configuration"), QStringLiteral("modify-production-configuration"), QStringLiteral("High-Risk Actions")},
        {QStringLiteral("Push to remote repository"), QStringLiteral("push-remote-repository"), QStringLiteral("High-Risk Actions")},
        {QStringLiteral("Publish release"), QStringLiteral("publish-release"), QStringLiteral("High-Risk Actions")},
        {QStringLiteral("Deploy project"), QStringLiteral("deploy-project"), QStringLiteral("High-Risk Actions")},
        {QStringLiteral("Execute destructive commands"), QStringLiteral("execute-destructive-commands"), QStringLiteral("High-Risk Actions")}
        ,{QStringLiteral("Modify secrets / secret configuration"), QStringLiteral("modify-secrets"), QStringLiteral("High-Risk Actions")}
        ,{QStringLiteral("Modify database schema"), QStringLiteral("modify-database-schema"), QStringLiteral("High-Risk Actions")}
        ,{QStringLiteral("Destructive database operations"), QStringLiteral("destructive-database-operations"), QStringLiteral("High-Risk Actions")}
        ,{QStringLiteral("Modify deployment environment"), QStringLiteral("modify-deployment-environment"), QStringLiteral("High-Risk Actions")}
        ,{QStringLiteral("Delete remote branch"), QStringLiteral("delete-remote-branch"), QStringLiteral("High-Risk Actions")}
    });
}

QList<AiOption> integrations()
{
    return options({
        {QStringLiteral("AGENTS.md"), QStringLiteral("agents-md"), QStringLiteral("Core Agent Integration")},
        {QStringLiteral("Task Lifecycle"), QStringLiteral("task-lifecycle"), QStringLiteral("Core Agent Integration")},
        {QStringLiteral("Rules"), QStringLiteral("rules"), QStringLiteral("Core Agent Integration")},
        {QStringLiteral("Routing"), QStringLiteral("routing"), QStringLiteral("Core Agent Integration")},
        {QStringLiteral("Project Memory"), QStringLiteral("project-memory"), QStringLiteral("Core Agent Integration")},
        {QStringLiteral("Project Status"), QStringLiteral("project-status"), QStringLiteral("Core Agent Integration")},
        {QStringLiteral("Project Resources"), QStringLiteral("project-resources"), QStringLiteral("Knowledge and Authority")},
        {QStringLiteral("Source of Truth"), QStringLiteral("source-of-truth"), QStringLiteral("Knowledge and Authority")},
        {QStringLiteral("Durable Decisions"), QStringLiteral("durable-decisions"), QStringLiteral("Knowledge and Authority")},
        {QStringLiteral("Framework Knowledge"), QStringLiteral("framework-knowledge"), QStringLiteral("Knowledge and Authority")},
        {QStringLiteral("Template Knowledge"), QStringLiteral("template-knowledge"), QStringLiteral("Knowledge and Authority")},
        {QStringLiteral("Source References"), QStringLiteral("source-references"), QStringLiteral("Knowledge and Authority")},
        {QStringLiteral("User-Approved Knowledge"), QStringLiteral("user-approved-knowledge"), QStringLiteral("Knowledge and Authority")},
        {QStringLiteral("Decision Recording"), QStringLiteral("decision-recording"), QStringLiteral("Validation and History")},
        {QStringLiteral("Checkpoint Recording"), QStringLiteral("checkpoint-recording"), QStringLiteral("Validation and History")},
        {QStringLiteral("Validation / Verification"), QStringLiteral("validation-verification"), QStringLiteral("Validation and History")},
        {QStringLiteral("Memory Consistency Validation"), QStringLiteral("memory-consistency-validation"), QStringLiteral("Validation and History")},
        {QStringLiteral("Cold-Start Validation"), QStringLiteral("cold-start-validation"), QStringLiteral("Validation and History")},
        {QStringLiteral("Certification Knowledge"), QStringLiteral("certification-knowledge"), QStringLiteral("Validation and History")},
        {QStringLiteral("Evidence Recording"), QStringLiteral("evidence-recording"), QStringLiteral("Validation and History")},
        {QStringLiteral("Test Result Recording"), QStringLiteral("test-result-recording"), QStringLiteral("Validation and History")},
        {QStringLiteral("Build Result Recording"), QStringLiteral("build-result-recording"), QStringLiteral("Validation and History")},
        {QStringLiteral("Update PROJECT_STATUS.md"), QStringLiteral("update-project-status"), QStringLiteral("Automatic Project Maintenance")},
        {QStringLiteral("Record durable decisions"), QStringLiteral("record-durable-decisions"), QStringLiteral("Automatic Project Maintenance")},
        {QStringLiteral("Update current-state memory"), QStringLiteral("update-current-state-memory"), QStringLiteral("Automatic Project Maintenance")},
        {QStringLiteral("Record validation results"), QStringLiteral("record-validation-results"), QStringLiteral("Automatic Project Maintenance")},
        {QStringLiteral("Record checkpoints"), QStringLiteral("record-checkpoints"), QStringLiteral("Automatic Project Maintenance")}
        ,{QStringLiteral("Update Evidence"), QStringLiteral("update-evidence"), QStringLiteral("Automatic Project Maintenance")}
        ,{QStringLiteral("Update Verification State"), QStringLiteral("update-verification-state"), QStringLiteral("Automatic Project Maintenance")}
    });
}

}
