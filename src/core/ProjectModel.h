#pragma once

#include <QObject>
#include <QStringList>
#include <QHash>

struct DevelopmentEnvironment {
    QString ide;
    QString compiler;
    QString operatingSystem;
    QString targetPlatform;
    QString buildSystem;
    QString packageManager;
    QString versionControl;
};

class ProjectModel final : public QObject {
    Q_OBJECT
public:
    explicit ProjectModel(QObject* parent = nullptr);

    QString projectId() const { return projectId_; }
    QString projectName() const { return projectName_; }
    QString projectPath() const { return projectPath_; }
    QString description() const { return description_; }
    QString templateId() const { return templateId_; }
    QString context() const { return context_; }
    DevelopmentEnvironment developmentEnvironment() const { return environment_; }
    QStringList aiPlatforms() const { return aiPlatforms_; }
    QStringList resourceNames() const { return resourceNames_; }
    QStringList profileSelections() const { return profileSelections_; }
    QStringList optionValues(const QString& key) const { return options_.value(key); }

    void setProjectName(const QString& value);
    void setProjectPath(const QString& value);
    void setDescription(const QString& value);
    void setTemplateId(const QString& value);
    void setContext(const QString& value);
    void setDevelopmentEnvironment(const DevelopmentEnvironment& value);
    void setAiPlatforms(const QStringList& value);
    void setResourceNames(const QStringList& value);
    void setProfileSelections(const QStringList& value);
    void setOptionValues(const QString& key, const QStringList& value);

    void beginUpdate();
    void endUpdate();

signals:
    void modelChanged();
    void developmentEnvironmentChanged();
    void aiPlatformsChanged();
    void profileChanged();
    void optionChanged(const QString& key);

private:
    void notifyChanged();
    QString projectId_;
    QString projectName_ = QStringLiteral("New AR&MF Project");
    QString projectPath_;
    QString description_;
    QString templateId_;
    QString context_;
    DevelopmentEnvironment environment_;
    QStringList aiPlatforms_;
    QStringList resourceNames_;
    QStringList profileSelections_;
    QHash<QString, QStringList> options_;
    int updateDepth_ = 0;
    bool pendingNotification_ = false;
};
