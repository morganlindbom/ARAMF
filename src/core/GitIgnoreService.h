// GitIgnoreService.h

#pragma once

#include <QObject>
#include <QString>

struct GitIgnoreResult
{
    bool success = false;
    bool changed = false;
    bool selfProject = false;
    QString error;
};

class GitIgnoreService final : public QObject
{
    Q_OBJECT

public:
    explicit GitIgnoreService(QObject* parent = nullptr);
    GitIgnoreResult ensureProjectGitIgnore(const QString& projectRoot) const;
    static QString managedBlock(bool includeGeneratedRootAgents);
};
