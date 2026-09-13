#include "WorkflowPageMetadata.h"

namespace {

bool isSetupPage(const QString& stablePageId)
{
    return stablePageId == QStringLiteral("project.file-worker")
        || stablePageId == QStringLiteral("project.modules-templates")
        || stablePageId == QStringLiteral("project.academic")
        || stablePageId == QStringLiteral("project.languages")
        || stablePageId == QStringLiteral("project.frameworks")
        || stablePageId == QStringLiteral("project.development-tools")
        || stablePageId == QStringLiteral("project.platforms")
        || stablePageId == QStringLiteral("project.hardware-architecture")
        || stablePageId == QStringLiteral("project.build-delivery")
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
    if (isSetupPage(stablePageId)) return {true, true};
    return {};
}
