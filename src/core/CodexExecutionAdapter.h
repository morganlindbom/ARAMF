#pragma once

#include "AgentExecutionAdapter.h"
#include "CodexExecutableResolver.h"

class QProcess;

class CodexExecutionAdapter final : public AgentExecutionAdapter
{
    Q_OBJECT
public:
    explicit CodexExecutionAdapter(QObject* parent = nullptr);
    QString providerId() const override;
    bool available(QString* error = nullptr) const override;
    void start(const AgentExecutionRequest& request) override;
    void cancel() override;

    static QString programPath();
    static CodexExecutableResolution resolution();
    static QStringList argumentsFor(const AgentExecutionRequest& request);
    static bool workingDirectoryAllowed(const AgentExecutionRequest& request);
    static bool isAsynchronous() { return true; }

private:
    QProcess* process_ = nullptr;
    qint64 processId_ = 0;
    AgentExecutionRequest request_;
    QString standardOutput_;
    QString standardError_;
};
