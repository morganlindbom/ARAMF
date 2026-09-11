// GitIgnoreService.cpp

#include "GitIgnoreService.h"
#include "AramfPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace
{
constexpr auto BeginMarker = "# BEGIN ARAMF MANAGED IGNORE";
constexpr auto EndMarker = "# END ARAMF MANAGED IGNORE";

bool writeFile(const QString& path, const QString& content, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray bytes = content.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool generatedRootAgent(const QString& projectRoot)
{
    QFile file(QDir(projectRoot).filePath(QStringLiteral("AGENTS.md")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString content = QString::fromUtf8(file.readAll());
    return content.contains(QStringLiteral("<!-- AGENTS.md -->"))
        && content.contains(QStringLiteral("ARAMF_WORKER"))
        && content.contains(QStringLiteral("/AGENTS.md"));
}
}

GitIgnoreService::GitIgnoreService(QObject* parent)
    : QObject(parent)
{
}

QString GitIgnoreService::managedBlock(bool includeGeneratedRootAgents)
{
    QString block = QStringLiteral(
        "# BEGIN ARAMF MANAGED IGNORE\n"
        "# ARAMF private/generated project artifacts\n"
        "/ARAMF_WORKER/\n"
        "/ARAMF_WORKER_*/\n"
        "/ARAMF/\n"
        "/aramf/\n");
    if (includeGeneratedRootAgents) block += QStringLiteral("/AGENTS.md\n");
    block += QStringLiteral("# END ARAMF MANAGED IGNORE\n");
    return block;
}

GitIgnoreResult GitIgnoreService::ensureProjectGitIgnore(const QString& projectRoot) const
{
    GitIgnoreResult result;
    const QString root = QDir::cleanPath(projectRoot.trimmed());
    if (root.isEmpty() || root == QStringLiteral(".") || !QDir(root).exists()) {
        result.error = QStringLiteral("Cannot update .gitignore: project root does not exist.");
        return result;
    }

    result.selfProject = AramfPaths::isAramfSelfProject(root);
    if (result.selfProject) {
        result.success = true;
        return result;
    }

    const QString path = QDir(root).filePath(QStringLiteral(".gitignore"));
    QString original;
    QFile existing(path);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.error = existing.errorString();
            return result;
        }
        original = QString::fromUtf8(existing.readAll());
        existing.close();
    }

    const QString block = managedBlock(generatedRootAgent(root));
    const QString begin = QString::fromLatin1(BeginMarker);
    const QString end = QString::fromLatin1(EndMarker);
    QString updated = original;
    const int beginIndex = original.indexOf(begin);
    if (beginIndex >= 0) {
        const int endIndex = original.indexOf(end, beginIndex + begin.size());
        if (endIndex < 0) {
            result.error = QStringLiteral("Malformed ARAMF managed .gitignore section.");
            return result;
        }
        const QString existingBlock = original.mid(beginIndex, endIndex + end.size() - beginIndex);
        QString normalizedExistingBlock = existingBlock;
        normalizedExistingBlock.remove(QLatin1Char('\r'));
        if (normalizedExistingBlock == block) {
            result.success = true;
            return result;
        }
        QString replacement = block;
        if (endIndex + end.size() < original.size() && original.at(endIndex + end.size()) == QLatin1Char('\n'))
            replacement.chop(1);
        updated.replace(beginIndex, endIndex + end.size() - beginIndex, replacement);
    } else {
        if (!updated.isEmpty() && !updated.endsWith(QLatin1Char('\n'))) updated += QLatin1Char('\n');
        if (!updated.isEmpty()) updated += QLatin1Char('\n');
        updated += block;
    }

    result.changed = updated != original;
    if (result.changed && !writeFile(path, updated, &result.error)) return result;
    result.success = true;
    return result;
}
