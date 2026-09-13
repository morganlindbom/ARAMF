#pragma once

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QJsonValue>
#include <QByteArray>
#include <QJsonArray>

#include "ProjectSchema.h"

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
    bool enabled = false;
    QString academicMode = QStringLiteral("disabled");
    QStringList projectTypes;
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
    // Thesis and Report are independent document products.  The resource
    // system owns external source identity; these fields only select it.
    struct DocumentationConfiguration {
        bool enabled = false;
        QString templateMode = QStringLiteral("aramf-default");
        QString templateSourceId;
        QString templateId;
        int templateVersion = 0;
        QString language;
        QString instructionId;
        int instructionVersion = 0;
    } thesisDocumentation{false, QStringLiteral("aramf-default"), {}, QStringLiteral("aramf-default-thesis"), 1, QStringLiteral("sv"), QStringLiteral("aramf-thesis-instruction"), 1};
    DocumentationConfiguration reportDocumentation{false, QStringLiteral("aramf-default"), {}, QStringLiteral("aramf-default-report"), 1, QStringLiteral("sv"), QStringLiteral("aramf-report-instruction"), 1};
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
    // Governance role is independent from authority strength.  Keep this
    // extensible string identifier rather than coupling behavior to labels.
    QString role = QStringLiteral("supporting-material");
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

struct HardwareResource {
    QString id;
    QString endpointId;
    QString resourceType = QStringLiteral("digital-pin");
    QString physicalResource;
    QString direction;
    QString logicalMode;
    QString activeLevel;
    QString purpose;
    QString ownership;
    QStringList capabilities;
};

struct AndroidProjectConstraints {
    int minSdk = 0;
    bool minSdkSpecified = false;
    QString minSdkSource;
    QString courseName;
    QString courseNameSource;
    QString projectDomain;
    QString projectDomainSource;
    QString architecture;
    QString architectureSource;
    QStringList declaredTechnologies;
    QString declaredTechnologiesSource;
    QString primaryIde = QStringLiteral("android-studio");
    QString primaryIdeSource;
    bool kotlinRequired = false;
    QString kotlinSource;
    QString uiTechnology = QStringLiteral("compose");
    bool xmlRequired = false;
    QString uiTechnologySource;
    bool composeAvailable = true;
    bool composeAllowed = true;
    bool composeSelected = true;
    QString composeSource;
    bool composeRequired = false;
    QString composeRequiredSource;
    bool retrofitRequired = false;
    QString retrofitSource;
    bool gsonRequired = false;
    QString gsonSource;
    bool internetPermissionRequired = false;
    QString internetPermissionSource;
    bool githubApiRequired = false;
    QString githubApiSource;
    bool githubPatRequired = false;
    QString githubPatSource;
    bool classicPatRequired = false;
    QString classicPatSource;
    QString patScope;
    QString patScopeSource;
    bool hardcodedTokenProhibited = false;
    QString hardcodedTokenSource;
    bool localPropertiesRequired = false;
    QString localPropertiesSource;
    bool buildConfigRequired = false;
    QString buildConfigSource;
    bool privateRepositoryRequired = false;
    QString privateRepositorySource;
    bool instructorCollaboratorRequired = false;
    QString instructorCollaboratorSource;
    QString repositoryName;
    QString repositoryNameSource;
    QString initialBranch;
    QString initialBranchSource;
    QString requiredInitialFile;
    QString requiredInitialFileSource;
    QString applicationStateSource;
    QString applicationStateSourceSource;
    QString stateModel;
    QString stateModelSource;
    QString workflow;
    QString workflowSource;
    QStringList applicationStateFields;
    QString applicationStateFieldsSource;
    QStringList domainAgents;
    QString domainAgentsSource;
    QString domainAgentDistinction;
    QString domainAgentDistinctionSource;
    bool pollingRequired = false;
    QString pollingSource;
    int pollingIntervalSeconds = 0;
    QString pollingIntervalSource;
    QString pollingExecutionLocation;
    QString pollingExecutionLocationSource;
    QString networkDispatcher;
    QString networkDispatcherSource;
    QString analysisDispatcher;
    QString analysisDispatcherSource;
    bool stateFlowRequired = false;
    QString stateFlowSource;
    bool collectAsStateRequired = false;
    QString collectAsStateSource;
    bool deceptionDetectorRequired = false;
    QString deceptionDetectorSource;
    bool regexOrHeuristicsRequired = false;
    QString regexOrHeuristicsSource;
    int confidenceMinPercent = -1;
    int confidenceMaxPercent = -1;
    QString confidenceRangeSource;
    bool normalGreenRequired = false;
    QString normalGreenSource;
    bool securityAlertRedRequired = false;
    QString securityAlertRedSource;
    bool rawAdversarialTextRequired = false;
    QString rawAdversarialTextSource;
    bool humanInTheLoopRequired = false;
    QString humanInTheLoopSource;
    QStringList validationRequirements;
    QString validationRequirementsSource;
    QStringList submissionRequirements;
    QString submissionRequirementsSource;
    QString submissionVideoDuration;
    QString submissionVideoDurationSource;
    QStringList submissionSegments;
    QString submissionSegmentsSource;
    QStringList unresolvedRequirements;
    QString unresolvedRequirementsSource;
    QString sourceOfTruthTitle;
    QString sourceOfTruthResource;
    bool roomRequired = false;
    QString roomSource;
    bool unitTestsRequired = false;
    QString unitTestsSource;
    bool lintRequired = false;
    QString lintSource;
};

struct RuleConfiguration {
    QStringList activeCategories;
    QString enforcementLevel = QStringLiteral("standard");
    QString loadingStrategy = QStringLiteral("relevant");
    QStringList workScopes;
    QStringList projectScopes;
    // Canonical, project-supplied scope ownership/dependency/test metadata.
    // No source-file permissions are inferred from display names or AI text.
    QJsonObject scopeMetadata;
    QStringList contextPolicies;
    QString conflictPolicy = QStringLiteral("prefer-user-instruction");
};

struct MemoryConfiguration {
    // Ownership of project-local persistence. The default keeps memory usable
    // by the active coding agent without requiring a machine-global recorder.
    QString writerMode = QStringLiteral("agent-direct");
    QStringList captureCategories;
    QString retentionLevel = QStringLiteral("standard");
    QStringList maintenanceOptions;
    QStringList validationOptions;
    QString updateStrategy = QStringLiteral("meaningful-task");
    QStringList historyOptions;
    qint64 maximumSizeBytes = 10LL * 1024LL * 1024LL * 1024LL;
};

struct CertificationConfiguration {
    bool enabled = false;
    QString defaultVerificationLevel = QStringLiteral("HOST_TEST");
};

struct GenerationOptions {
    bool generateAgentRules = true;
    bool generateRouting = true;
    bool generatePlatforms = true;
    bool generateResources = true;
    bool generateMemory = true;
    bool generateProvenance = true;
};

struct CommunicationEndpoint {
    QString id;
    QString targetId;
    QString displayName;
    QString role;
    QStringList capabilities;
    QString address;
};

struct CommunicationLink {
    QString id;
    QString endpointA;
    QString endpointB;
    QString direction = QStringLiteral("bidirectional");
    QString transport;
    QString protocol;
    QString frameType;
    QString logicalDataModel;
    QString wireEncoding;
    QString protocolVersion = QStringLiteral("1");
    QString timeout;
    QString reconnectPolicy;
    QString errorHandling;
    bool acknowledgement = false;
    int maximumPacketSize = 0;
    QString byteOrder;
};

struct CommunicationField {
    QString name;
    QString type;
    bool required = false;
    QString description;
    bool array = false;
    QString min;
    QString max;
    QString defaultValue;
    QStringList enumValues;
};

struct CommunicationMessage {
    qint64 id = 0;
    QString name;
    QString type;
    QString direction;
    QString sourceEndpointId;
    QString destinationEndpointId;
    qint64 requestMessageId = 0;
    qint64 responseMessageId = 0;
    QList<CommunicationField> fields;
    QString description;
};

struct CommunicationTestVector {
    QString id;
    qint64 messageId = 0;
    QJsonValue logicalInput;
    QByteArray expectedEncodedPayload;
    QJsonObject expectedDecodedFields;
    QString direction;
};

struct CommunicationConfiguration {
    bool enabled = false;
    QString sourceTarget;
    QString destinationTarget;
    QString transport;
    QString protocol;
    QString sourceRole;
    QString destinationRole;
    QString endpoint;
    QString dataFormat;
    QString protocolVersion = QStringLiteral("1");
    bool authenticationRequired = false;
    bool encryptionRequired = false;
    QString reconnectPolicy;
    QString errorHandling;
    QStringList integrationRequirements;
    QList<CommunicationEndpoint> endpoints;
    QList<CommunicationLink> links;
    QList<CommunicationMessage> messages;
    QList<CommunicationTestVector> testVectors;
};

class ProjectModel final : public QObject {
    Q_OBJECT
public:
    explicit ProjectModel(QObject* parent = nullptr);

    QString projectId() const { return projectId_; }
    QString projectName() const { return projectName_; }
    QString projectPath() const { return projectPath_; }
    QString projectFilePath() const { return projectFilePath_; }
    QString workerNameSuffix() const { return workerNameSuffix_; }
    QString description() const { return description_; }
    QString templateId() const { return templateId_; }
    QStringList templateModules() const { return templateModules_; }
    QJsonObject templateState() const { return templateState_; }
    void setTemplateState(const QJsonObject& state);
    bool projectTypeLocked() const { return !templateId_.isEmpty() && !templateState_.value("projectType").toString().isEmpty(); }
    void clearEnvironmentOverrides() { environmentOverrides_.clear(); }
    QString context() const { return context_; }
    DevelopmentEnvironment developmentEnvironment() const { return environment_; }
    DevelopmentCapabilities developmentCapabilities() const { return capabilities_; }
    AcademicConfiguration academicConfiguration() const { return academic_; }
    AiConfiguration aiConfiguration() const { return ai_; }
    QList<ProjectResource> resources() const { return resources_; }
    QList<HardwareResource> hardwareResources() const { return hardwareResources_; }
    ResourcePolicy resourcePolicy() const { return resourcePolicy_; }
    RuleConfiguration ruleConfiguration() const { return ruleConfiguration_; }
    MemoryConfiguration memoryConfiguration() const { return memoryConfiguration_; }
    CertificationConfiguration certificationConfiguration() const { return certificationConfiguration_; }
    AndroidProjectConstraints androidConstraints() const { return androidConstraints_; }
    GenerationOptions generationOptions() const { return generationOptions_; }
    CommunicationConfiguration communicationConfiguration() const { return communication_; }
    QStringList aiPlatforms() const { return aiPlatforms_; }
    QStringList resourceNames() const { return resourceNames_; }
    QStringList profileSelections() const { return profileSelections_; }
    QStringList optionValues(const QString& key) const { return options_.value(key); }
    QHash<QString, QStringList> options() const { return options_; }
    bool isModified() const { return modified_; }
    int projectSchemaVersion() const { return projectSchemaVersion_; }
    int migratedFromSchemaVersion() const { return migratedFromSchemaVersion_; }
    QString migrationStatus() const { return migrationStatus_; }
    QJsonArray migrationNotices() const { return migrationNotices_; }
    int targetRelease() const { return targetRelease_; }
    bool hasTargetRelease() const { return targetRelease_ > 0; }
    QSet<QString> completedPageIds() const { return completedPageIds_; }
    bool isPageCompleted(const QString& pageId) const { return completedPageIds_.contains(pageId); }

    void setProjectName(const QString& value);
    void setProjectPath(const QString& value);
    void setProjectFilePath(const QString& value);
    void setWorkerNameSuffix(const QString& value);
    void setProjectId(const QString& value);
    void setDescription(const QString& value);
    void setTemplateId(const QString& value);
    void setTemplateModules(const QStringList& value);
    void setContext(const QString& value);
    void setDevelopmentEnvironment(const DevelopmentEnvironment& value);
    void setDevelopmentCapabilities(const DevelopmentCapabilities& value);
    void setAcademicConfiguration(const AcademicConfiguration& value);
    void setAiConfiguration(const AiConfiguration& value);
    void setResources(const QList<ProjectResource>& value);
    void setHardwareResources(const QList<HardwareResource>& value);
    void setResourcePolicy(const ResourcePolicy& value);
    void setRuleConfiguration(const RuleConfiguration& value);
    void setMemoryConfiguration(const MemoryConfiguration& value);
    void setCertificationConfiguration(const CertificationConfiguration& value);
    void setAndroidConstraints(const AndroidProjectConstraints& value);
    void resolveAndroidConstraints();
    void setGenerationOptions(const GenerationOptions& value);
    void setCommunicationConfiguration(const CommunicationConfiguration& value);
    void applyTemplateCapabilities(const DevelopmentCapabilities& value);
    void applyTemplateDefaults(const DevelopmentEnvironment& value);
    void setAiPlatforms(const QStringList& value);
    void setResourceNames(const QStringList& value);
    void setProfileSelections(const QStringList& value);
    void setOptionValues(const QString& key, const QStringList& value);
    void resetForNewProject();
    void setModified(bool modified);
    void setMigrationState(int sourceVersion, const QString& status, const QJsonArray& notices);
    void setTargetRelease(int release);
    void setPageCompleted(const QString& pageId, bool completed);
    void setCompletedPageIds(const QSet<QString>& pageIds);

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
    QString workerNameSuffix_;
    QString description_;
    QString templateId_;
    QStringList templateModules_;
    QJsonObject templateState_;
    QString context_;
    DevelopmentEnvironment environment_;
    DevelopmentCapabilities capabilities_;
    AcademicConfiguration academic_;
    AiConfiguration ai_;
    QList<ProjectResource> resources_;
    QList<HardwareResource> hardwareResources_;
    ResourcePolicy resourcePolicy_;
    RuleConfiguration ruleConfiguration_;
    MemoryConfiguration memoryConfiguration_;
    CertificationConfiguration certificationConfiguration_;
    AndroidProjectConstraints androidConstraints_;
    GenerationOptions generationOptions_;
    CommunicationConfiguration communication_;
    QStringList aiPlatforms_;
    QStringList resourceNames_;
    QStringList profileSelections_;
    QHash<QString, QStringList> options_;
    QSet<QString> environmentOverrides_;
    int updateDepth_ = 0;
    bool pendingNotification_ = false;
    bool modified_ = false;
    int projectSchemaVersion_ = ProjectSchema::CurrentVersion;
    int migratedFromSchemaVersion_ = ProjectSchema::CurrentVersion;
    QString migrationStatus_ = ProjectSchema::MigrationOk;
    QJsonArray migrationNotices_;
    int targetRelease_ = 0;
    QSet<QString> completedPageIds_;
};
