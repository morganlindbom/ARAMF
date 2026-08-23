#pragma once

#include <QString>
#include <QStringList>

struct CodexExecutableResolution
{
    bool available = false;
    QString path;
    QString version;
    QString source;
    QString error;
};

class CodexExecutableResolver final
{
public:
    static CodexExecutableResolution resolve(const QString& explicitPath = {});
    static CodexExecutableResolution validate(const QString& path);
    static QString localCodexDirectory();
    static QStringList localCandidates();
};
