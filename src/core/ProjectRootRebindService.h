#pragma once

#include <QString>
#include <QStringList>

class ProjectModel;
class QTextStream;

struct ProjectRootRebindResult
{
    bool success = false;
    bool rebound = false;
    QString savedRoot;
    QString currentRoot;
    QStringList changedFiles;
    QStringList warnings;
    QString error;
};

class ProjectRootRebindService final
{
public:
    ProjectRootRebindResult rebind(ProjectModel* model,
                                   const QString& openedProjectRoot,
                                   bool regenerate = true) const;
};

int runProjectRootRebindCommand(const QStringList& arguments,
                                QTextStream& output,
                                QTextStream& error);
