#pragma once

#include <QString>

struct WorkflowPageCapabilities
{
    bool countsTowardSetupProgress = false;
    bool userCheckableCompletion = false;
};

// Stable workflow-page keys are the canonical identity used by persistence.
// Unknown keys intentionally receive the safe non-setup defaults above.
WorkflowPageCapabilities workflowPageCapabilities(const QString& stablePageId);
