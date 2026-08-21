// AramfPaths.h

#pragma once

#include <QString>

namespace AramfPaths {
inline const QString RootDirectory = QStringLiteral("ARAMF");
inline const QString AgentInstructions = QStringLiteral("ARAMF/AGENTS.md");
inline const QString ProjectStatus = QStringLiteral("ARAMF/PROJECT_STATUS.md");
inline const QString Profile = QStringLiteral("ARAMF/aramf-profile.json");
inline const QString GeneratedRules = QStringLiteral("ARAMF/rules/generated-rules.md");
inline const QString Decisions = QStringLiteral("ARAMF/memory/decisions.md");
inline const QString Checkpoints = QStringLiteral("ARAMF/memory/checkpoints.json");
inline const QString Metrics = QStringLiteral("ARAMF/memory/metrics.json");
inline const QString Manifest = QStringLiteral("ARAMF/memory/memory-manifest.json");
inline const QString EventLog = QStringLiteral("ARAMF/memory/event-log.jsonl");
inline const QString CurrentState = QStringLiteral("ARAMF/memory/current-state.md");
inline const QString ColdStartValidation = QStringLiteral("ARAMF/memory/cold-start-validation.json");
inline const QString ConsistencyValidation = QStringLiteral("ARAMF/memory/memory-consistency-validation.json");
inline const QString TaskRoutes = QStringLiteral("ARAMF/routing/task-routes.json");
inline const QString ScopeRoutes = QStringLiteral("ARAMF/routing/scope-routes.json");
inline const QString ResourceManifest = QStringLiteral("ARAMF/resources/resources.json");
inline const QString CustomTemplates = QStringLiteral("ARAMF/templates/custom-templates.json");
inline const QString Provenance = QStringLiteral("ARAMF/provenance.json");
inline const QString SelectionEffects = QStringLiteral("ARAMF/selection-effects.json");
} // namespace AramfPaths
