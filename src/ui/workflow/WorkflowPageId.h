#pragma once

#include <QString>

enum class WorkflowPageId {
    Setup,
    ProjectIdentity,
    ProjectModulesTemplates,
    Academic,
    Languages,
    Frameworks,
    DevelopmentTools,
    Platforms,
    HardwareArchitecture,
    BuildDelivery,
    AiAgents,
    AiResponsibilities,
    AiAutonomy,
    AiIntegration,
    ResourceInventory,
    ResourceAuthority,
    ResourcePolicy,
    RuleSelection,
    RuleRouting,
    MemoryCapture,
    MemoryMaintenance,
    ReleaseOverview,
    ProductVersion,
    ComponentVersions,
    SchemaCompatibility,
    ReleaseReadiness,
    ApprovalHistory,
    Review,
    Generate,
    Verify,
    Finalize,
    UpdateReview,
    UpdateApply,
    ImprovementBacklog
};

inline QString workflowPageKey(WorkflowPageId page)
{
    switch (page) {
    case WorkflowPageId::Setup: return QStringLiteral("project.overview");
    case WorkflowPageId::ProjectIdentity: return QStringLiteral("project.file-worker");
    case WorkflowPageId::ProjectModulesTemplates: return QStringLiteral("project.modules-templates");
    case WorkflowPageId::Academic: return QStringLiteral("project.academic");
    case WorkflowPageId::Languages: return QStringLiteral("project.languages");
    case WorkflowPageId::Frameworks: return QStringLiteral("project.frameworks");
    case WorkflowPageId::DevelopmentTools: return QStringLiteral("project.development-tools");
    case WorkflowPageId::Platforms: return QStringLiteral("project.platforms");
    case WorkflowPageId::HardwareArchitecture: return QStringLiteral("project.hardware-architecture");
    case WorkflowPageId::BuildDelivery: return QStringLiteral("project.build-delivery");
    case WorkflowPageId::AiAgents: return QStringLiteral("ai.agents");
    case WorkflowPageId::AiResponsibilities: return QStringLiteral("ai.responsibilities");
    case WorkflowPageId::AiAutonomy: return QStringLiteral("ai.autonomy");
    case WorkflowPageId::AiIntegration: return QStringLiteral("ai.integration");
    case WorkflowPageId::ResourceInventory: return QStringLiteral("resources.inventory");
    case WorkflowPageId::ResourceAuthority: return QStringLiteral("resources.authority");
    case WorkflowPageId::ResourcePolicy: return QStringLiteral("resources.policy");
    case WorkflowPageId::RuleSelection: return QStringLiteral("rules.selection");
    case WorkflowPageId::RuleRouting: return QStringLiteral("rules.routing");
    case WorkflowPageId::MemoryCapture: return QStringLiteral("memory.capture");
    case WorkflowPageId::MemoryMaintenance: return QStringLiteral("memory.maintenance");
    case WorkflowPageId::ReleaseOverview: return QStringLiteral("release.overview");
    case WorkflowPageId::ProductVersion: return QStringLiteral("release.product-version");
    case WorkflowPageId::ComponentVersions: return QStringLiteral("release.component-versions");
    case WorkflowPageId::SchemaCompatibility: return QStringLiteral("release.schema-compatibility");
    case WorkflowPageId::ReleaseReadiness: return QStringLiteral("release.readiness");
    case WorkflowPageId::ApprovalHistory: return QStringLiteral("release.approval-history");
    case WorkflowPageId::Review: return QStringLiteral("generate.review");
    case WorkflowPageId::Generate: return QStringLiteral("generate.generate");
    case WorkflowPageId::Verify: return QStringLiteral("generate.verify");
    case WorkflowPageId::Finalize: return QStringLiteral("generate.finalize");
    case WorkflowPageId::UpdateReview: return QStringLiteral("update.review");
    case WorkflowPageId::UpdateApply: return QStringLiteral("update.apply");
    case WorkflowPageId::ImprovementBacklog: return QStringLiteral("update.improvement-backlog");
    }
    return {};
}
