#include "CodexExecutableResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace {
CodexExecutableResolution unavailable(const QString& source, const QString& error)
{
    CodexExecutableResolution result;
    result.source = source;
    result.error = error;
    return result;
}
}

CodexExecutableResolution CodexExecutableResolver::validate(const QString& path)
{
    const QFileInfo file(path);
    if (!file.exists() || !file.isFile()) return unavailable(QStringLiteral("invalid"), QStringLiteral("Codex executable is not a regular file: %1").arg(path));
    QProcess process;
    process.setProgram(file.absoluteFilePath());
    process.setArguments({QStringLiteral("--version")});
    process.start();
    if (!process.waitForStarted(1500)) return unavailable(QStringLiteral("invalid"), QStringLiteral("Codex executable could not be started: %1").arg(process.errorString()));
    if (!process.waitForFinished(5000)) {
        process.kill();
        return unavailable(QStringLiteral("invalid"), QStringLiteral("Codex version check timed out."));
    }
    const QString version = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || version.isEmpty()) {
        return unavailable(QStringLiteral("invalid"), QStringLiteral("Codex version check failed for %1.").arg(path));
    }
    CodexExecutableResolution result;
    result.available = true;
    result.path = file.absoluteFilePath();
    result.version = version.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts).value(0).trimmed();
    return result;
}

QString CodexExecutableResolver::localCodexDirectory()
{
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty()) localAppData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(localAppData).filePath(QStringLiteral("OpenAI/Codex/bin"));
}

QStringList CodexExecutableResolver::localCandidates()
{
    const QDir root(localCodexDirectory());
    if (!root.exists()) return {};
    QStringList candidates;
    QFileInfoList directories = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(directories.begin(), directories.end(), [](const QFileInfo& left, const QFileInfo& right) {
        if (left.lastModified() != right.lastModified()) return left.lastModified() > right.lastModified();
        return left.absoluteFilePath() < right.absoluteFilePath();
    });
    for (const auto& directory : directories) {
        const QString candidate = QDir(directory.absoluteFilePath()).filePath(QStringLiteral("codex.exe"));
        if (QFileInfo(candidate).isFile()) candidates.append(candidate);
    }
    return candidates;
}

CodexExecutableResolution CodexExecutableResolver::resolve(const QString& explicitPath)
{
    if (!explicitPath.isEmpty()) {
        auto result = validate(explicitPath);
        result.source = QStringLiteral("explicit");
        if (!result.available && result.error.isEmpty()) result.error = QStringLiteral("Configured Codex path is invalid.");
        return result;
    }

    const QString environmentPath = qEnvironmentVariable("CODEX_CLI_PATH");
    if (!environmentPath.isEmpty()) {
        auto result = validate(environmentPath);
        result.source = QStringLiteral("CODEX_CLI_PATH");
        if (!result.available) result.error = QStringLiteral("CODEX_CLI_PATH is invalid: %1").arg(result.error);
        return result;
    }

    const QString pathCandidate = QStandardPaths::findExecutable(QStringLiteral("codex"));
    if (!pathCandidate.isEmpty()) {
        auto result = validate(pathCandidate);
        result.source = QStringLiteral("PATH");
        if (result.available) return result;
    }

    CodexExecutableResolution firstFailure = unavailable(QStringLiteral("LOCALAPPDATA"), QStringLiteral("No valid Codex installation was found."));
    for (const auto& candidate : localCandidates()) {
        auto result = validate(candidate);
        result.source = QStringLiteral("LOCALAPPDATA");
        if (result.available) return result;
        if (firstFailure.error == QStringLiteral("No valid Codex installation was found.")) firstFailure = result;
    }
    return firstFailure;
}
