#include "UpdateExecutionService.h"

#include "AramfPaths.h"
#include "CodexExecutionAdapter.h"
#include "ProjectModel.h"
#include "UpdateService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>

namespace {
QJsonObject readObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeObject(const QString& path, const QJsonObject& object, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const auto data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        if (error) *error = file.errorString();
        return false;
    }
    file.close();
    return true;
}

QString gitStatus(const QString& root)
{
    QProcess git;
    git.setProgram(QStringLiteral("git"));
    git.setArguments({QStringLiteral("-C"), root, QStringLiteral("status"), QStringLiteral("--porcelain=v1")});
    git.start();
    if (!git.waitForFinished(3000) || git.exitCode() != 0) return {};
    return QString::fromLocal8Bit(git.readAllStandardOutput());
}

QStringList statusFiles(const QString& status)
{
    QStringList files;
    for (const auto& line : status.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts)) {
        if (line.size() > 3) files.append(line.mid(3));
    }
    return files;
}
}

UpdateExecutionService::UpdateExecutionService(QObject* parent)
    : QObject(parent)
{
}

bool UpdateExecutionService::hasRequiredPermissions(const ProjectModel& model, QString* error)
{
    const auto permissions = model.aiConfiguration().permissions;
    for (const auto& required : {QStringLiteral("read-project-files"), QStringLiteral("modify-files")}) {
        if (!permissions.contains(required)) {
            if (error) *error = QStringLiteral("Configured AI autonomy does not allow %1.").arg(required);
            return false;
        }
    }
    return true;
}

bool UpdateExecutionService::canExecute(const ProjectModel& model, QString* error) const
{
    if (!hasRequiredPermissions(model, error)) return false;
    if (model.aiConfiguration().primaryAgent != QStringLiteral("openai-codex")) {
        if (error) *error = QStringLiteral("No execution adapter is available for configured agent '%1'.").arg(model.aiConfiguration().primaryAgent);
        return false;
    }
    CodexExecutionAdapter adapter;
    return adapter.available(error);
}

bool UpdateExecutionService::execute(const QString& projectRoot, const ProjectModel& model, QString* error)
{
    if (adapter_) {
        if (error) *error = QStringLiteral("An update execution is already running.");
        return false;
    }
    UpdateService update;
    if (!update.isPlanCurrent(projectRoot, model, error)) {
        emit stateChanged(QStringLiteral("STALE"));
        return false;
    }
    auto plan = readObject(QDir(projectRoot).filePath(AramfPaths::UpdatePlan));
    if (plan.value(QStringLiteral("status")).toString() == QStringLiteral("conflict")) {
        if (error) *error = QStringLiteral("Update is blocked by higher authority.");
        emit stateChanged(QStringLiteral("CONFLICT"));
        return false;
    }
    const bool requiresImplementation = plan.value(QStringLiteral("requiresImplementation")).toBool(true);
    if (requiresImplementation) {
        if (!canExecute(model, error)) {
            emit stateChanged(QStringLiteral("READY_FOR_EXTERNAL_AGENT"));
            return false;
        }
    } else if (!hasRequiredPermissions(model, error)) {
        emit stateChanged(QStringLiteral("FAILED"));
        return false;
    }
    QStringList adoptedIds;
    for (const auto& value : plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray()) {
        const auto selected = value.toObject();
        const QString classification = selected.value(QStringLiteral("classification")).toString();
        if (classification == QStringLiteral("ALREADY_SATISFIED") || classification == QStringLiteral("APPLICABLE_CHANGE_REQUIRED"))
            adoptedIds.append(selected.value(QStringLiteral("id")).toString());
    }
    FrameworkKnowledgeService knowledge;
    QString adoptionError;
    if (!knowledge.adoptKnowledgeForProject(projectRoot, adoptedIds, &adoptionError)) {
        if (error) *error = adoptionError;
        emit stateChanged(QStringLiteral("FAILED"));
        return false;
    }
    plan.insert(QStringLiteral("adoptedFrameworkKnowledge"), QJsonArray::fromStringList(adoptedIds));
    if (!writeObject(QDir(projectRoot).filePath(AramfPaths::UpdatePlan), plan, error)) {
        emit stateChanged(QStringLiteral("FAILED"));
        return false;
    }
    if (!requiresImplementation) {
        AgentExecutionResult noChange;
        noChange.succeeded = true;
        projectRoot_ = QFileInfo(projectRoot).canonicalFilePath();
        executionStartedAt_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        writeResult(noChange, QStringLiteral("AWAITING_VALIDATION"));
        emit stateChanged(QStringLiteral("AWAITING_VALIDATION"));
        emit finished(noChange);
        return true;
    }
    AgentExecutionRequest request;
    request.projectRoot = QFileInfo(projectRoot).canonicalFilePath();
    request.controlRoot = QFileInfo(QDir(projectRoot).filePath(AramfPaths::ControlDirectory)).canonicalFilePath();
    request.planPath = QDir(projectRoot).filePath(AramfPaths::UpdatePlan);
    request.contractPath = QDir(projectRoot).filePath(AramfPaths::UpdateContract);
    request.prompt = QStringLiteral("Read %1 first. Then read %2 and %3. Execute the selected Framework Knowledge update against the managed project root. The control plane is orchestration only; make implementation changes in the managed project when required. Preserve higher-authority sources, do not commit or push, and report actual files changed and validation performed.")
                         .arg(QDir(request.projectRoot).filePath(QStringLiteral("AGENTS.md")), request.planPath, request.contractPath);
    if (!CodexExecutionAdapter::workingDirectoryAllowed(request)) {
        if (error) *error = QStringLiteral("Managed project root is invalid or resolves to the control plane.");
        emit stateChanged(QStringLiteral("FAILED"));
        return false;
    }

    projectRoot_ = request.projectRoot;
    preExecutionStatus_ = gitStatus(projectRoot_);
    executionStartedAt_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    adapter_ = new CodexExecutionAdapter(this);
    connect(adapter_, &AgentExecutionAdapter::started, this, [this](qint64) { emit stateChanged(QStringLiteral("EXECUTING")); });
    connect(adapter_, &AgentExecutionAdapter::outputReceived, this, &UpdateExecutionService::outputReceived);
    connect(adapter_, &AgentExecutionAdapter::finished, this, [this](const AgentExecutionResult& result) {
        writeResult(result, result.succeeded ? QStringLiteral("AWAITING_VALIDATION") : QStringLiteral("FAILED"));
        emit stateChanged(result.succeeded ? QStringLiteral("AWAITING_VALIDATION") : QStringLiteral("FAILED"));
        emit finished(result);
        adapter_->deleteLater();
        adapter_ = nullptr;
    });
    emit stateChanged(QStringLiteral("EXECUTING"));
    adapter_->start(request);
    return true;
}

bool UpdateExecutionService::validate(const QString& projectRoot, QString* error)
{
    if (validationProcess_) {
        if (error) *error = QStringLiteral("Update validation is already running.");
        return false;
    }
    if (executionState(projectRoot) != QStringLiteral("AWAITING_VALIDATION")) {
        if (error) *error = QStringLiteral("Validation requires an awaiting update execution.");
        return false;
    }
    const auto existingResult = readObject(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json")));
    if (existingResult.value(QStringLiteral("noChangeRequired")).toBool(false)) {
        return completeAfterValidation(projectRoot, true, true, QStringLiteral("NO_CHANGE_REQUIRED: adopted knowledge is already satisfied; no implementation diff required."), error);
    }
    const QString buildRoot = QDir(projectRoot).filePath(QStringLiteral("build"));
    if (!QFileInfo::exists(QDir(buildRoot).filePath(QStringLiteral("CTestTestfile.cmake")))) {
        if (error) *error = QStringLiteral("No CTest build was found for the configured subsystem validation.");
        emit validationFinished(false, *error);
        return false;
    }
    validationProcess_ = new QProcess(this);
    validationProcess_->setProgram(QStringLiteral("ctest"));
    validationProcess_->setArguments({QStringLiteral("--test-dir"), buildRoot, QStringLiteral("--output-on-failure")});
    validationProcess_->setWorkingDirectory(projectRoot);
    connect(validationProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, projectRoot](int exitCode, QProcess::ExitStatus exitStatus) {
        const QString output = QString::fromLocal8Bit(validationProcess_->readAllStandardOutput())
            + QString::fromLocal8Bit(validationProcess_->readAllStandardError());
        const bool passed = exitStatus == QProcess::NormalExit && exitCode == 0;
        QString ignored;
        completeAfterValidation(projectRoot, passed, false, output, &ignored);
        emit validationFinished(passed, output);
        validationProcess_->deleteLater();
        validationProcess_ = nullptr;
    });
    validationProcess_->start();
    if (!validationProcess_->waitForStarted(1000)) {
        if (error) *error = validationProcess_->errorString();
        validationProcess_->deleteLater();
        validationProcess_ = nullptr;
        return false;
    }
    emit stateChanged(QStringLiteral("AWAITING_VALIDATION"));
    return true;
}

void UpdateExecutionService::writeResult(const AgentExecutionResult& result, const QString& state)
{
    const QString path = QDir(projectRoot_).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json"));
    QJsonObject object = readObject(path);
    object.insert(QStringLiteral("executionId"), object.value(QStringLiteral("executionId")).toString(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    object.insert(QStringLiteral("projectRoot"), projectRoot_);
    object.insert(QStringLiteral("controlRoot"), QDir(projectRoot_).filePath(AramfPaths::ControlDirectory));
    const auto plan = readObject(QDir(projectRoot_).filePath(AramfPaths::UpdatePlan));
    object.insert(QStringLiteral("knowledgeIds"), plan.value(QStringLiteral("selectedFrameworkKnowledge")));
    object.insert(QStringLiteral("adoptedFrameworkKnowledge"), plan.value(QStringLiteral("adoptedFrameworkKnowledge")));
    object.insert(QStringLiteral("planFingerprint"), plan.value(QStringLiteral("planFingerprint")));
    object.insert(QStringLiteral("agentProvider"), QStringLiteral("openai-codex"));
    object.insert(QStringLiteral("executionMechanism"), QStringLiteral("Codex CLI via QProcess"));
    object.insert(QStringLiteral("startedAt"), executionStartedAt_);
    object.insert(QStringLiteral("finishedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    object.insert(QStringLiteral("agentLaunched"), result.launched);
    object.insert(QStringLiteral("agentExitCode"), result.exitCode);
    object.insert(QStringLiteral("agentSucceeded"), result.succeeded);
    object.insert(QStringLiteral("stdout"), result.standardOutput);
    object.insert(QStringLiteral("stderr"), result.standardError);
    object.insert(QStringLiteral("failureReason"), result.failureReason);
    object.insert(QStringLiteral("noChangeRequired"), !result.launched && result.succeeded);
    object.insert(QStringLiteral("validationPerformed"), QStringLiteral("pending"));
    object.insert(QStringLiteral("preExistingGitStatus"), preExecutionStatus_);
    const QString postStatus = gitStatus(projectRoot_);
    object.insert(QStringLiteral("postExecutionGitStatus"), postStatus);
    const QStringList beforeFiles = statusFiles(preExecutionStatus_);
    const QStringList afterFiles = statusFiles(postStatus);
    object.insert(QStringLiteral("preExistingProjectFiles"), QJsonArray::fromStringList(beforeFiles));
    object.insert(QStringLiteral("postExecutionProjectFiles"), QJsonArray::fromStringList(afterFiles));
    const QSet<QString> beforeSet = QSet<QString>(beforeFiles.cbegin(), beforeFiles.cend());
    QStringList actualFiles;
    for (const auto& file : afterFiles) if (!beforeSet.contains(file)) actualFiles.append(file);
    object.insert(QStringLiteral("actualProjectFiles"), QJsonArray::fromStringList(actualFiles));
    object.insert(QStringLiteral("controlPlaneFiles"), QJsonArray{QStringLiteral("ARAMF_WORKER/update/update-application-result.json")});
    object.insert(QStringLiteral("processId"), result.processId);
    object.insert(QStringLiteral("finalState"), state);
    object.insert(QStringLiteral("validationPerformed"), false);
    QString ignored;
    writeObject(path, object, &ignored);
}

bool UpdateExecutionService::completeAfterValidation(const QString& projectRoot,
                                                     bool validationPassed,
                                                     bool noChangeRequired,
                                                     const QString& validationSummary,
                                                     QString* error)
{
    const QString path = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json"));
    auto result = readObject(path);
    if (result.isEmpty() || result.value(QStringLiteral("finalState")).toString() != QStringLiteral("AWAITING_VALIDATION")) {
        if (error) *error = QStringLiteral("No awaiting update execution evidence exists.");
        return false;
    }
    if (!validationPassed || (!noChangeRequired && (!result.value(QStringLiteral("agentSucceeded")).toBool()
                                                    || result.value(QStringLiteral("actualProjectFiles")).toArray().isEmpty()))) {
        result.insert(QStringLiteral("finalState"), QStringLiteral("FAILED"));
        result.insert(QStringLiteral("validationResult"), QStringLiteral("FAIL"));
        result.insert(QStringLiteral("validationSummary"), validationSummary);
        result.insert(QStringLiteral("validationPerformed"), true);
        writeObject(path, result, error);
        emit stateChanged(QStringLiteral("FAILED"));
        return false;
    }
    result.insert(QStringLiteral("finalState"), QStringLiteral("COMPLETED"));
    result.insert(QStringLiteral("validationResult"), QStringLiteral("PASS"));
    result.insert(QStringLiteral("validationSummary"), validationSummary);
    result.insert(QStringLiteral("validationPerformed"), true);
    result.insert(QStringLiteral("noChangeRequired"), noChangeRequired);
    if (!writeObject(path, result, error)) return false;
    emit stateChanged(QStringLiteral("COMPLETED"));
    return true;
}

QString UpdateExecutionService::executionState(const QString& projectRoot, QString* error) const
{
    const auto result = readObject(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json")));
    if (result.isEmpty()) {
        if (error) *error = QStringLiteral("No update execution result exists.");
        return QStringLiteral("NOT_ANALYZED");
    }
    return result.value(QStringLiteral("finalState")).toString(QStringLiteral("NOT_ANALYZED"));
}
