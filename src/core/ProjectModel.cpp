#include "ProjectModel.h"

#include <QUuid>
#include <QSet>

ProjectModel::ProjectModel(QObject* parent) : QObject(parent), projectId_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    environment_.ide = QStringLiteral("vscode");
    environment_.language = QStringLiteral("cpp");
    environment_.framework = QStringLiteral("none");
    environment_.compiler = QStringLiteral("msys2-ucrt64-gcc");
    environment_.operatingSystem = QStringLiteral("windows");
    environment_.targetPlatform = QStringLiteral("desktop");
    environment_.buildSystem = QStringLiteral("cmake");
    environment_.packageManager = QStringLiteral("none");
    environment_.versionControl = QStringLiteral("none");
}

void ProjectModel::notifyChanged()
{
    if (updateDepth_ > 0) {
        pendingNotification_ = true;
        return;
    }
    emit modelChanged();
}

void ProjectModel::setProjectName(const QString& value)
{
    if (projectName_ == value) return;
    projectName_ = value;
    notifyChanged();
}

void ProjectModel::setProjectPath(const QString& value)
{
    if (projectPath_ == value) return;
    projectPath_ = value;
    notifyChanged();
}

void ProjectModel::setDescription(const QString& value)
{
    if (description_ == value) return;
    description_ = value;
    notifyChanged();
}

void ProjectModel::setTemplateId(const QString& value)
{
    if (templateId_ == value) return;
    templateId_ = value;
    notifyChanged();
}

void ProjectModel::setContext(const QString& value)
{
    if (context_ == value) return;
    context_ = value;
    notifyChanged();
}

/*
 * Environment setters intentionally remain explicit instead of using a macro.
 * This makes the distinction between user overrides and template defaults clear.
 */
void ProjectModel::setDevelopmentEnvironment(const DevelopmentEnvironment& value)
{
    const bool unchanged = environment_.language == value.language
        && environment_.framework == value.framework
        && environment_.ide == value.ide
        && environment_.compiler == value.compiler
        && environment_.operatingSystem == value.operatingSystem
        && environment_.targetPlatform == value.targetPlatform
        && environment_.buildSystem == value.buildSystem
        && environment_.packageManager == value.packageManager
        && environment_.versionControl == value.versionControl;
    if (unchanged) return;
    if (environment_.language != value.language) environmentOverrides_.insert(QStringLiteral("language"));
    if (environment_.framework != value.framework) environmentOverrides_.insert(QStringLiteral("framework"));
    if (environment_.ide != value.ide) environmentOverrides_.insert(QStringLiteral("ide"));
    if (environment_.compiler != value.compiler) environmentOverrides_.insert(QStringLiteral("compiler"));
    if (environment_.operatingSystem != value.operatingSystem) environmentOverrides_.insert(QStringLiteral("operatingSystem"));
    if (environment_.targetPlatform != value.targetPlatform) environmentOverrides_.insert(QStringLiteral("targetPlatform"));
    if (environment_.buildSystem != value.buildSystem) environmentOverrides_.insert(QStringLiteral("buildSystem"));
    environment_ = value;
    emit developmentEnvironmentChanged();
    notifyChanged();
}

void ProjectModel::applyTemplateDefaults(const DevelopmentEnvironment& value)
{
    DevelopmentEnvironment next = environment_;
    auto apply = [&](const QString& key, QString& destination, const QString& source) {
        if (!environmentOverrides_.contains(key)) destination = source;
    };
    apply(QStringLiteral("language"), next.language, value.language);
    apply(QStringLiteral("framework"), next.framework, value.framework);
    apply(QStringLiteral("ide"), next.ide, value.ide);
    apply(QStringLiteral("compiler"), next.compiler, value.compiler);
    apply(QStringLiteral("operatingSystem"), next.operatingSystem, value.operatingSystem);
    apply(QStringLiteral("targetPlatform"), next.targetPlatform, value.targetPlatform);
    apply(QStringLiteral("buildSystem"), next.buildSystem, value.buildSystem);
    next.packageManager = value.packageManager;
    next.versionControl = value.versionControl;
    if (next.language == environment_.language
        && next.framework == environment_.framework
        && next.ide == environment_.ide
        && next.compiler == environment_.compiler
        && next.operatingSystem == environment_.operatingSystem
        && next.targetPlatform == environment_.targetPlatform
        && next.buildSystem == environment_.buildSystem) {
        return;
    }
    environment_ = next;
    emit developmentEnvironmentChanged();
    notifyChanged();
}

void ProjectModel::setAiPlatforms(const QStringList& value)
{
    if (aiPlatforms_ == value) return;
    aiPlatforms_ = value;
    emit aiPlatformsChanged();
    notifyChanged();
}

void ProjectModel::setResourceNames(const QStringList& value)
{
    if (resourceNames_ == value) return;
    resourceNames_ = value;
    notifyChanged();
}

void ProjectModel::setProfileSelections(const QStringList& value)
{
    if (profileSelections_ == value) return;
    profileSelections_ = value;
    emit profileChanged();
    notifyChanged();
}

void ProjectModel::setOptionValues(const QString& key, const QStringList& value)
{
    if (options_.value(key) == value) return;
    options_[key] = value;
    emit optionChanged(key);
    notifyChanged();
}

void ProjectModel::beginUpdate()
{
    ++updateDepth_;
}

void ProjectModel::endUpdate()
{
    if (updateDepth_ == 0) return;
    if (--updateDepth_ == 0 && pendingNotification_) {
        pendingNotification_ = false;
        emit modelChanged();
    }
}
