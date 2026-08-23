#pragma once

#include "AgentExecutionAdapter.h"

#include <QObject>
#include <QString>

class ProjectModel;
class QProcess;

class UpdateExecutionService final : public QObject
{
    Q_OBJECT
public:
    explicit UpdateExecutionService(QObject* parent = nullptr);

    bool execute(const QString& projectRoot, const ProjectModel& model, QString* error = nullptr);
    bool validate(const QString& projectRoot, QString* error = nullptr);
    bool canExecute(const ProjectModel& model, QString* error = nullptr) const;
    bool completeAfterValidation(const QString& projectRoot,
                                 bool validationPassed,
                                 bool noChangeRequired,
                                 const QString& validationSummary,
                                 QString* error = nullptr);
    QString executionState(const QString& projectRoot, QString* error = nullptr) const;
    static bool hasRequiredPermissions(const ProjectModel& model, QString* error = nullptr);

signals:
    void stateChanged(const QString& state);
    void outputReceived(const QString& output);
    void finished(const AgentExecutionResult& result);
    void validationFinished(bool passed, const QString& summary);

private:
    void writeResult(const AgentExecutionResult& result, const QString& state);
    QString projectRoot_;
    QString preExecutionStatus_;
    QString executionStartedAt_;
    AgentExecutionAdapter* adapter_ = nullptr;
    QProcess* validationProcess_ = nullptr;
};
