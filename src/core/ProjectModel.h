#pragma once

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QSet>

struct DevelopmentEnvironment {
    QString language;
    QString framework;
    QString ide;
    QString compiler;
    QString operatingSystem;
    QString targetPlatform;
    QString targetArchitecture;
    QString buildSystem;
    QString packageManager;
    QString versionControl;
};

struct DevelopmentCapabilities {
    QStringList languages;
    QStringList frameworks;
    QStringList ides;
    QStringList versionControlSystems;
    QStringList developmentTools;
    QStringList hostOperatingSystems;
    QStringList targetPlatforms;
    QStringList targetArchitectures;
    QStringList processorFamilies;
    QStringList hardwareTargets;
    QStringList toolchains;
    QStringList buildSystems;
    QStringList dependencyManagers;
    QStringList buildConfigurations;
    QStringList testingCapabilities;
    QStringList qualityCapabilities;
    QStringList automationCapabilities;
    QStringList deliveryCapabilities;
};

struct AcademicConfiguration {
    QString academicMode = QStringLiteral("disabled");
    QString thesisLevel;
    QStringList thesisApproaches;
    QStringList researchMethods;
    QString institution;
    QString programmeOrCourse;
    QString supervisor;
    QString examiner;
    QString citationStyle;
    QString academicLanguage;
    QStringList academicRequirements;
    QStringList academicDeliverables;
};

struct AiConfiguration {
    QString primaryAgent = QStringLiteral("none");
    QStringList additionalAgents;
    QStringList responsibilities;
    QStringList permissions;
    QStringList aramfIntegrations;
    QString customAgentName;
    QString autonomyPreset = QStringLiteral("custom");
};

struct ProjectResource {
    QString id;
    QString name;
    QString type = QStringLiteral("file");
    QString location;
    QString description;
    bool enabled = true;
    QString locationMode = QStringLiteral("referenced");
    QString authorityLevel = QStringLiteral("supporting-reference");
    QStringList scopes;
    QString status = QStringLiteral("unknown");
    QString loadingStrategyOverride;
    QString lastModified;
    QString fingerprint;
};

// Returns a stable identity for a resource location. Local paths are resolved
// relative to projectPath and normalized for equivalent Windows spellings;
// URLs use a separate normalized identity namespace.
QString canonicalResourceIdentity(const ProjectResource& resource,
                                  const QString& projectPath = QString());

bool sameResourceIdentity(const ProjectResource& left,
                          const ProjectResource& right,
                          const QString& projectPath = QString());

struct ResourcePolicy {
    QStringList options;
    QString loadingStrategy = QStringLiteral("relevant");
};

struct RuleConfiguration {
    QStringList activeCategories;
    QString enforcementLevel = QStringLiteral("standard");
    QString loadingStrategy = QStringLiteral("relevant");
    QStringList workScopes;
    QStringList projectScopes;
    QStringList contextPolicies;
    QString conflictPolicy = QStringLiteral("prefer-user-instruction");
};

struct MemoryConfiguration {
    QStringList captureCategories;
    QString retentionLevel = QStringLiteral("standard");
    QStringList maintenanceOptions;
    QStringList validationOptions;
    QString updateStrategy = QStringLiteral("meaningful-task");
    QStringList historyOptions;
    qint64 maximumSizeBytes = 10LL * 1024LL * 1024LL * 1024LL;
};

struct GenerationOptions {
    bool generateAgentRules = true;
    bool generateRouting = true;
    bool generatePlatforms = true;
    bool generateResources = true;
    bool generateMemory = true;
    bool generateProvenance = true;
};

class ProjectModel final : public QObject {
    Q_OBJECT
public:
    explicit ProjectModel(QObject* parent = nullptr);

    QString projectId() const { return projectId_; }
    QString projectName() const { return projectName_; }
    QString projectPath() const { return projectPath_; }
    QString projectFilePath() const { return projectFilePath_; }
    QString description() const { return description_; }
    QString templateId() const { return templateId_; }
    QString context() const { return context_; }
    DevelopmentEnvironment developmentEnvironment() const { return environment_; }
    DevelopmentCapabilities developmentCapabilities() const { return capabilities_; }
    AcademicConfiguration academicConfiguration() const { return academic_; }
    AiConfiguration aiConfiguration() const { return ai_; }
    QList<ProjectResource> resources() const { return resources_; }
    ResourcePolicy resourcePolicy() const { return resourcePolicy_; }
    RuleConfiguration ruleConfiguration() const { return ruleConfiguration_; }
    MemoryConfiguration memoryConfiguration() const { return memoryConfiguration_; }
    GenerationOptions generationOptions() const { return generationOptions_; }
    QStringList aiPlatforms() const { return aiPlatforms_; }
    QStringList resourceNames() const { return resourceNames_; }
    QStringList profileSelections() const { return profileSelections_; }
    QStringList optionValues(const QString& key) const { return options_.value(key); }
    QHash<QString, QStringList> options() const { return options_; }
    bool isModified() const { return modified_; }

    void setProjectName(const QString& value);
    void setProjectPath(const QString& value);
    void setProjectFilePath(const QString& value);
    void setProjectId(const QString& value);
    void setDescription(const QString& value);
    void setTemplateId(const QString& value);
    void setContext(const QString& value);
    void setDevelopmentEnvironment(const DevelopmentEnvironment& value);
    void setDevelopmentCapabilities(const DevelopmentCapabilities& value);
    void setAcademicConfiguration(const AcademicConfiguration& value);
    void setAiConfiguration(const AiConfiguration& value);
    void setResources(const QList<ProjectResource>& value);
    void setResourcePolicy(const ResourcePolicy& value);
    void setRuleConfiguration(const RuleConfiguration& value);
    void setMemoryConfiguration(const MemoryConfiguration& value);
    void setGenerationOptions(const GenerationOptions& value);
    void applyTemplateCapabilities(const DevelopmentCapabilities& value);
    void applyTemplateDefaults(const DevelopmentEnvironment& value);
    void setAiPlatforms(const QStringList& value);
    void setResourceNames(const QStringList& value);
    void setProfileSelections(const QStringList& value);
    void setOptionValues(const QString& key, const QStringList& value);
    void resetForNewProject();
    void setModified(bool modified);

    void beginUpdate();
    void endUpdate();

signals:
    void modelChanged();
    void developmentEnvironmentChanged();
    void developmentCapabilitiesChanged();
    void aiPlatformsChanged();
    void aiConfigurationChanged();
    void profileChanged();
    void optionChanged(const QString& key);
    void modifiedChanged(bool modified);

private:
    void notifyChanged();
    QString projectId_;
    QString projectName_ = QStringLiteral("New AR&MF Project");
    QString projectPath_;
    QString projectFilePath_;
    QString description_;
    QString templateId_;
    QString context_;
    DevelopmentEnvironment environment_;
    DevelopmentCapabilities capabilities_;
    AcademicConfiguration academic_;
    AiConfiguration ai_;
    QList<ProjectResource> resources_;
    ResourcePolicy resourcePolicy_;
    RuleConfiguration ruleConfiguration_;
    MemoryConfiguration memoryConfiguration_;
    GenerationOptions generationOptions_;
    QStringList aiPlatforms_;
    QStringList resourceNames_;
    QStringList profileSelections_;
    QHash<QString, QStringList> options_;
    QSet<QString> environmentOverrides_;
    int updateDepth_ = 0;
    bool pendingNotification_ = false;
    bool modified_ = false;
};
