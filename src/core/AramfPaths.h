// AramfPaths.h

#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace AramfPaths {
// Repository-local development material lives under ./aramf_setup/.
// These runtime paths intentionally describe only the generated target
// control plane below ProjectModel::projectPath().
inline const QString RepositorySetupDirectory = QStringLiteral("aramf_setup");
inline const QString BootstrapDirectory = QStringLiteral("bootstrap");
inline const QString ControlDirectory = QStringLiteral("ARAMF_WORKER");
inline const QString LegacyControlDirectory = QStringLiteral("ARAMF");
inline const QString RootDirectory = ControlDirectory;
inline const QString AgentInstructions = QStringLiteral("ARAMF_WORKER/AGENTS.md");
inline const QString ProjectStatus = QStringLiteral("ARAMF_WORKER/PROJECT_STATUS.md");
inline const QString Profile = QStringLiteral("ARAMF_WORKER/aramf-profile.json");
inline const QString GeneratedRules = QStringLiteral("ARAMF_WORKER/rules/generated-rules.md");
inline const QString Decisions = QStringLiteral("ARAMF_WORKER/memory/decisions.md");
inline const QString Checkpoints = QStringLiteral("ARAMF_WORKER/memory/checkpoints.json");
inline const QString Metrics = QStringLiteral("ARAMF_WORKER/memory/metrics.json");
inline const QString Manifest = QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json");
inline const QString EventLog = QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl");
inline const QString CurrentState = QStringLiteral("ARAMF_WORKER/memory/current-state.md");
inline const QString ColdStartValidation = QStringLiteral("ARAMF_WORKER/memory/cold-start-validation.json");
inline const QString ConsistencyValidation = QStringLiteral("ARAMF_WORKER/memory/memory-consistency-validation.json");
inline const QString MemoryConfiguration = QStringLiteral("ARAMF_WORKER/memory/memory-config.json");
inline const QString MemoryContract = QStringLiteral("ARAMF_WORKER/memory/memory-contract.json");
inline const QString FrameworkKnowledge = QStringLiteral("ARAMF_WORKER/memory/framework-knowledge.json");
inline const QString TaskRoutes = QStringLiteral("ARAMF_WORKER/routing/task-routes.json");
inline const QString ScopeRoutes = QStringLiteral("ARAMF_WORKER/routing/scope-routes.json");
inline const QString ValidationPolicy = QStringLiteral("ARAMF_WORKER/routing/validation-policy.json");
inline const QString ResourceManifest = QStringLiteral("ARAMF_WORKER/resources/resources.json");
inline const QString CustomTemplates = QStringLiteral("ARAMF_WORKER/templates/custom-templates.json");
inline const QString Provenance = QStringLiteral("ARAMF_WORKER/provenance.json");
inline const QString SelectionEffects = QStringLiteral("ARAMF_WORKER/selection-effects.json");
inline const QString LegacyMigrationReport = QStringLiteral("ARAMF_WORKER/legacy-migration.json");
inline const QString UpdateDirectory = QStringLiteral("ARAMF_WORKER/update");
inline const QString UpdatePlan = QStringLiteral("ARAMF_WORKER/update/update-plan.json");
inline const QString UpdateContract = QStringLiteral("ARAMF_WORKER/update/update-contract.json");
inline const QString UpdateHistoryDirectory = QStringLiteral("ARAMF_WORKER/update/history");

namespace detail {
inline QString& programRootOverride()
{
    static QString value;
    return value;
}

inline QString& applicationDirectoryOverride()
{
    static QString value;
    return value;
}
}

inline void setProgramRootForTests(const QString& root)
{
    detail::programRootOverride() = QDir::cleanPath(root);
}

inline void clearProgramRootForTests()
{
    detail::programRootOverride().clear();
}

inline void setApplicationDirectoryForTests(const QString& directory)
{
    detail::applicationDirectoryOverride() = QDir::cleanPath(directory);
}

inline void clearApplicationDirectoryForTests()
{
    detail::applicationDirectoryOverride().clear();
}

inline QString applicationDirectory()
{
    if (!detail::applicationDirectoryOverride().isEmpty()) return detail::applicationDirectoryOverride();
    return QDir::cleanPath(QCoreApplication::applicationDirPath());
}

inline bool isDevelopmentRoot(const QString& directory)
{
    return QFileInfo(QDir(directory).filePath(RepositorySetupDirectory)).isDir()
        && QFileInfo(QDir(directory).filePath(QStringLiteral("src"))).isDir()
        && QFileInfo(QDir(directory).filePath(QStringLiteral("CMakeLists.txt"))).isFile();
}

inline QString programRoot()
{
    if (!detail::programRootOverride().isEmpty()) return detail::programRootOverride();

    QDir candidate(applicationDirectory());
    while (!candidate.isRoot()) {
        if (isDevelopmentRoot(candidate.absolutePath())) return candidate.absolutePath();
        candidate.cdUp();
    }
    if (isDevelopmentRoot(candidate.absolutePath())) return candidate.absolutePath();

    // A packaged portable build may put the executable directly beside its
    // ARAMF_DATA directory without development source markers.
    const QString executableDirectory = applicationDirectory();
    if (QFileInfo(QDir(executableDirectory).filePath(QStringLiteral("ARAMF_DATA"))).isDir())
        return executableDirectory;
    return executableDirectory;
}
} // namespace AramfPaths
