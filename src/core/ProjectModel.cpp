#include "ProjectModel.h"

#include <QUuid>

ProjectModel::ProjectModel(QObject* parent) : QObject(parent), projectId_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    environment_.ide = QStringLiteral("vscode");
    environment_.compiler = QStringLiteral("msys2-ucrt64-gcc");
    environment_.operatingSystem = QStringLiteral("windows");
    environment_.targetPlatform = QStringLiteral("desktop");
    environment_.buildSystem = QStringLiteral("cmake");
    environment_.packageManager = QStringLiteral("none");
    environment_.versionControl = QStringLiteral("none");
}

void ProjectModel::notifyChanged() {
    if (updateDepth_ > 0) { pendingNotification_ = true; return; }
    emit modelChanged();
}

#define MODEL_SETTER(name, field) \
void ProjectModel::name(const QString& value) { if (field == value) return; field = value; notifyChanged(); }
MODEL_SETTER(setProjectName, projectName_)
MODEL_SETTER(setProjectPath, projectPath_)
MODEL_SETTER(setDescription, description_)
MODEL_SETTER(setTemplateId, templateId_)
MODEL_SETTER(setContext, context_)
#undef MODEL_SETTER

void ProjectModel::setDevelopmentEnvironment(const DevelopmentEnvironment& value) {
    if (environment_.ide == value.ide && environment_.compiler == value.compiler && environment_.operatingSystem == value.operatingSystem && environment_.targetPlatform == value.targetPlatform && environment_.buildSystem == value.buildSystem && environment_.packageManager == value.packageManager && environment_.versionControl == value.versionControl) return;
    environment_ = value; emit developmentEnvironmentChanged(); notifyChanged();
}

void ProjectModel::setAiPlatforms(const QStringList& value) { if (aiPlatforms_ == value) return; aiPlatforms_ = value; emit aiPlatformsChanged(); notifyChanged(); }
void ProjectModel::setResourceNames(const QStringList& value) { if (resourceNames_ == value) return; resourceNames_ = value; notifyChanged(); }
void ProjectModel::setProfileSelections(const QStringList& value) { if (profileSelections_ == value) return; profileSelections_ = value; emit profileChanged(); notifyChanged(); }
void ProjectModel::setOptionValues(const QString& key, const QStringList& value) { if (options_.value(key) == value) return; options_[key] = value; emit optionChanged(key); notifyChanged(); }
void ProjectModel::beginUpdate() { ++updateDepth_; }
void ProjectModel::endUpdate() { if (updateDepth_ == 0) return; if (--updateDepth_ == 0 && pendingNotification_) { pendingNotification_ = false; emit modelChanged(); } }
