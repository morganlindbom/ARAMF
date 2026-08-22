#include "ControlPlaneMigration.h"

#include "AramfPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

bool copyMissingTree(const QString& sourceRoot, const QString& targetRoot,
                     QStringList* conflicts, QString* error)
{
    const QDir source(sourceRoot);
    if (!source.exists()) return true;
    if (!QDir().mkpath(targetRoot)) {
        if (error) *error = QStringLiteral("Could not create %1").arg(targetRoot);
        return false;
    }
    for (const QFileInfo& entry : source.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        const QString target = QDir(targetRoot).filePath(entry.fileName());
        if (entry.isDir()) {
            if (entry.fileName().compare(AramfPaths::ControlDirectory, Qt::CaseInsensitive) == 0) {
                continue;
            }
            if (!copyMissingTree(entry.absoluteFilePath(), target, conflicts, error)) return false;
        } else if (QFile::exists(target)) {
            if (conflicts) conflicts->append(QDir(sourceRoot).relativeFilePath(entry.fileName()));
        } else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            if (error) *error = QStringLiteral("Could not preserve legacy file %1").arg(entry.absoluteFilePath());
            return false;
        }
    }
    return true;
}

bool writeMigrationReport(const QString& projectRoot, const ControlPlanePreparation& preparation)
{
    QJsonArray conflicts;
    for (const QString& warning : preparation.warnings) conflicts.append(warning);
    const QJsonObject report{
        {QStringLiteral("status"), QStringLiteral("legacy-preserved")},
        {QStringLiteral("legacyDirectory"), AramfPaths::LegacyControlDirectory},
        {QStringLiteral("canonicalDirectory"), AramfPaths::ControlDirectory},
        {QStringLiteral("migrated"), preparation.migrated},
        {QStringLiteral("bothDirectoriesPresent"), preparation.bothDetected},
        {QStringLiteral("warnings"), conflicts}
    };
    QSaveFile file(QDir(projectRoot).filePath(AramfPaths::LegacyMigrationReport));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    const QByteArray data = QJsonDocument(report).toJson(QJsonDocument::Indented);
    return file.write(data) == data.size() && file.commit();
}

}

ControlPlanePreparation prepareControlPlane(const QString& projectRoot)
{
    ControlPlanePreparation preparation;
    const QDir root(QDir::cleanPath(projectRoot));
    const QString legacyPath = root.filePath(AramfPaths::LegacyControlDirectory);
    const QString canonicalPath = root.filePath(AramfPaths::ControlDirectory);
    preparation.legacyDetected = QDir(legacyPath).exists();
    preparation.bothDetected = preparation.legacyDetected && QDir(canonicalPath).exists();

    if (!QDir().mkpath(canonicalPath)) {
        preparation.error = QStringLiteral("Could not create %1").arg(canonicalPath);
        return preparation;
    }
    if (preparation.legacyDetected) {
        QStringList conflicts;
        if (!copyMissingTree(legacyPath, canonicalPath, &conflicts, &preparation.error)) return preparation;
        preparation.migrated = !preparation.bothDetected;
        if (preparation.bothDetected) {
            preparation.warnings << QStringLiteral("Legacy ARAMF/ and canonical ARAMF_WORKER/ both exist; ARAMF_WORKER/ is authoritative and legacy content is preserved.");
        }
        for (const QString& conflict : conflicts) {
            preparation.warnings << QStringLiteral("Preserved conflicting legacy file without overwriting canonical content: %1").arg(conflict);
        }
        if (!writeMigrationReport(projectRoot, preparation)) {
            preparation.error = QStringLiteral("Could not write legacy migration report.");
            return preparation;
        }
    }
    preparation.success = true;
    return preparation;
}
