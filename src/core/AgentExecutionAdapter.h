#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

struct AgentExecutionRequest
{
    QString projectRoot;
    QString controlRoot;
    QString planPath;
    QString contractPath;
    QString prompt;
};

struct AgentExecutionResult
{
    bool launched = false;
    bool succeeded = false;
    qint64 processId = 0;
    int exitCode = -1;
    QString standardOutput;
    QString standardError;
    QString failureReason;
};

class AgentExecutionAdapter : public QObject
{
    Q_OBJECT
public:
    explicit AgentExecutionAdapter(QObject* parent = nullptr) : QObject(parent) {}
    ~AgentExecutionAdapter() override = default;

    virtual QString providerId() const = 0;
    virtual bool available(QString* error = nullptr) const = 0;
    virtual void start(const AgentExecutionRequest& request) = 0;
    virtual void cancel() = 0;

signals:
    void started(qint64 processId);
    void outputReceived(const QString& output);
    void finished(const AgentExecutionResult& result);
};
