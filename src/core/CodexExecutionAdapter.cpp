#include "CodexExecutionAdapter.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

CodexExecutionAdapter::CodexExecutionAdapter(QObject* parent)
    : AgentExecutionAdapter(parent)
{
}

QString CodexExecutionAdapter::providerId() const
{
    return QStringLiteral("openai-codex");
}

QString CodexExecutionAdapter::programPath()
{
    return resolution().path;
}

CodexExecutableResolution CodexExecutionAdapter::resolution()
{
    return CodexExecutableResolver::resolve();
}

bool CodexExecutionAdapter::available(QString* error) const
{
    const auto result = resolution();
    if (result.available) return true;
    if (error) *error = result.error;
    return false;
}

bool CodexExecutionAdapter::workingDirectoryAllowed(const AgentExecutionRequest& request)
{
    const QString project = QFileInfo(request.projectRoot).canonicalFilePath();
    const QString control = QFileInfo(request.controlRoot).canonicalFilePath();
    if (project.isEmpty() || control.isEmpty()) return false;
    const QString relative = QDir(project).relativeFilePath(control);
    return QDir::cleanPath(project) != QDir::cleanPath(control)
        && relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("..%1").arg(QDir::separator()));
}

QStringList CodexExecutionAdapter::argumentsFor(const AgentExecutionRequest& request)
{
    return {QStringLiteral("exec"), QStringLiteral("-C"), request.projectRoot,
            QStringLiteral("-s"), QStringLiteral("workspace-write"),
            QStringLiteral("--json"), QStringLiteral("--ephemeral"), request.prompt};
}

void CodexExecutionAdapter::start(const AgentExecutionRequest& request)
{
    request_ = request;
    standardOutput_.clear();
    standardError_.clear();
    AgentExecutionResult failure;
    QString error;
    if (!available(&error)) {
        failure.failureReason = error;
        emit finished(failure);
        return;
    }
    if (!workingDirectoryAllowed(request)) {
        failure.failureReason = QStringLiteral("Managed project working directory is invalid or resolves to the control plane.");
        emit finished(failure);
        return;
    }

    process_ = new QProcess(this);
    process_->setWorkingDirectory(QDir::cleanPath(request.projectRoot));
    process_->setProgram(programPath());
    process_->setArguments(argumentsFor(request));
    connect(process_, &QProcess::readyReadStandardOutput, this, [this] {
        const auto data = process_->readAllStandardOutput();
        standardOutput_ += QString::fromLocal8Bit(data);
        emit outputReceived(QString::fromLocal8Bit(data));
    });
    connect(process_, &QProcess::readyReadStandardError, this, [this] {
        const auto data = process_->readAllStandardError();
        standardError_ += QString::fromLocal8Bit(data);
        emit outputReceived(QString::fromLocal8Bit(data));
    });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        AgentExecutionResult result;
        result.failureReason = process_->errorString();
        result.standardOutput = standardOutput_;
        result.standardError = standardError_;
        emit finished(result);
    });
    connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        AgentExecutionResult result;
        result.launched = true;
        result.processId = processId_;
        result.exitCode = exitCode;
        result.succeeded = exitStatus == QProcess::NormalExit && exitCode == 0;
        result.standardOutput = standardOutput_;
        result.standardError = standardError_;
        if (!result.succeeded) result.failureReason = QStringLiteral("Codex exited with code %1.").arg(exitCode);
        emit finished(result);
        process_->deleteLater();
        process_ = nullptr;
    });
    process_->start();
    if (!process_->waitForStarted(1000)) {
        failure.failureReason = process_->errorString();
        emit finished(failure);
        process_->deleteLater();
        process_ = nullptr;
        return;
    }
    processId_ = process_->processId();
    emit started(processId_);
}

void CodexExecutionAdapter::cancel()
{
    if (process_) process_->terminate();
}
