#pragma once

#include <QString>

enum class WorkflowPageId {
    Setup,
    ProjectIdentity,
    ProjectModulesTemplates,
    ProjectCommunication,
    ProjectCompatibility,
    Academic,
    AcademicDocumentation,
    AcademicResearch,
    AcademicInformation,
    AcademicStandards,
    AcademicDeliverables,
    Languages,
    Frameworks,
    DevelopmentTools,
    DevelopmentToolsIde,
    DevelopmentToolsVersionControl,
    DevelopmentToolsSupport,
    Platforms,
    PlatformsHosts,
    PlatformsTargets,
    HardwareArchitecture,
    HardwareArchitectures,
    HardwareProcessors,
    HardwareTargets,
    BuildDelivery,
    BuildDeliveryToolchains,
    BuildDeliveryBuildSystems,
    BuildDeliveryTesting,
    BuildDeliveryAutomation,
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
    UpdateConfiguration,
    ImprovementBacklog
};

inline QString workflowPageKey(WorkflowPageId page)
{
    switch (page) {
    case WorkflowPageId::Setup: return QStringLiteral("project.overview");
    case WorkflowPageId::ProjectIdentity: return QStringLiteral("project.file-worker");
    case WorkflowPageId::ProjectModulesTemplates: return QStringLiteral("project.modules-templates");
    case WorkflowPageId::ProjectCommunication: return QStringLiteral("project.communication");
    case WorkflowPageId::ProjectCompatibility: return QStringLiteral("project.compatibility");
    case WorkflowPageId::Academic: return QStringLiteral("project.academic");
    case WorkflowPageId::AcademicDocumentation: return QStringLiteral("project.academic.documentation");
    case WorkflowPageId::AcademicResearch: return QStringLiteral("project.academic.research");
    case WorkflowPageId::AcademicInformation: return QStringLiteral("project.academic.information");
    case WorkflowPageId::AcademicStandards: return QStringLiteral("project.academic.standards");
    case WorkflowPageId::AcademicDeliverables: return QStringLiteral("project.academic.deliverables");
    case WorkflowPageId::Languages: return QStringLiteral("project.languages");
    case WorkflowPageId::Frameworks: return QStringLiteral("project.frameworks");
    case WorkflowPageId::DevelopmentTools: return QStringLiteral("project.development-tools");
    case WorkflowPageId::DevelopmentToolsIde: return QStringLiteral("project.development-tools.ide");
    case WorkflowPageId::DevelopmentToolsVersionControl: return QStringLiteral("project.development-tools.version-control");
    case WorkflowPageId::DevelopmentToolsSupport: return QStringLiteral("project.development-tools.support");
    case WorkflowPageId::Platforms: return QStringLiteral("project.platforms");
    case WorkflowPageId::PlatformsHosts: return QStringLiteral("project.platforms.hosts");
    case WorkflowPageId::PlatformsTargets: return QStringLiteral("project.platforms.targets");
    case WorkflowPageId::HardwareArchitecture: return QStringLiteral("project.hardware-architecture");
    case WorkflowPageId::HardwareArchitectures: return QStringLiteral("project.hardware-architecture.architectures");
    case WorkflowPageId::HardwareProcessors: return QStringLiteral("project.hardware-architecture.processors");
    case WorkflowPageId::HardwareTargets: return QStringLiteral("project.hardware-architecture.targets");
    case WorkflowPageId::BuildDelivery: return QStringLiteral("project.build-delivery");
    case WorkflowPageId::BuildDeliveryToolchains: return QStringLiteral("project.build-delivery.toolchains");
    case WorkflowPageId::BuildDeliveryBuildSystems: return QStringLiteral("project.build-delivery.build-systems");
    case WorkflowPageId::BuildDeliveryTesting: return QStringLiteral("project.build-delivery.testing");
    case WorkflowPageId::BuildDeliveryAutomation: return QStringLiteral("project.build-delivery.automation");
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
    case WorkflowPageId::UpdateConfiguration: return QStringLiteral("update.configuration");
    case WorkflowPageId::ImprovementBacklog: return QStringLiteral("update.improvement-backlog");
    }
    return {};
}
