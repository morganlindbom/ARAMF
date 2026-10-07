#include "WorkflowPageMetadata.h"

namespace {

bool isSetupPage(const QString& stablePageId)
{
    return stablePageId == QStringLiteral("project.file-worker")
        || stablePageId == QStringLiteral("project.modules-templates")
        || stablePageId == QStringLiteral("project.communication")
        || stablePageId == QStringLiteral("project.compatibility")
        || stablePageId == QStringLiteral("project.academic")
        || stablePageId == QStringLiteral("project.academic.documentation")
        || stablePageId == QStringLiteral("project.academic.research")
        || stablePageId == QStringLiteral("project.academic.information")
        || stablePageId == QStringLiteral("project.academic.standards")
        || stablePageId == QStringLiteral("project.academic.deliverables")
        || stablePageId == QStringLiteral("project.languages")
        || stablePageId == QStringLiteral("project.frameworks")
        || stablePageId == QStringLiteral("project.development-tools.ide")
        || stablePageId == QStringLiteral("project.development-tools.version-control")
        || stablePageId == QStringLiteral("project.development-tools.support")
        || stablePageId == QStringLiteral("project.platforms.hosts")
        || stablePageId == QStringLiteral("project.platforms.targets")
        || stablePageId == QStringLiteral("project.hardware-architecture.architectures")
        || stablePageId == QStringLiteral("project.hardware-architecture.processors")
        || stablePageId == QStringLiteral("project.hardware-architecture.targets")
        || stablePageId == QStringLiteral("project.build-delivery.toolchains")
        || stablePageId == QStringLiteral("project.build-delivery.build-systems")
        || stablePageId == QStringLiteral("project.build-delivery.testing")
        || stablePageId == QStringLiteral("project.build-delivery.automation")
        || stablePageId == QStringLiteral("ai.agents")
        || stablePageId == QStringLiteral("ai.responsibilities")
        || stablePageId == QStringLiteral("ai.autonomy")
        || stablePageId == QStringLiteral("ai.integration")
        || stablePageId == QStringLiteral("resources.inventory")
        || stablePageId == QStringLiteral("resources.authority")
        || stablePageId == QStringLiteral("resources.policy")
        || stablePageId == QStringLiteral("rules.selection")
        || stablePageId == QStringLiteral("rules.routing")
        || stablePageId == QStringLiteral("memory.capture")
        || stablePageId == QStringLiteral("memory.maintenance");
}

}

WorkflowPageCapabilities workflowPageCapabilities(const QString& stablePageId)
{
    if (stablePageId == QStringLiteral("project.academic")) return {};
    if (stablePageId == QStringLiteral("project.development-tools")
        || stablePageId == QStringLiteral("project.platforms")
        || stablePageId == QStringLiteral("project.hardware-architecture")
        || stablePageId == QStringLiteral("project.build-delivery")) return {};
    if (isSetupPage(stablePageId)) return {true, true};
    return {};
}
