// Services.h

#pragma once

#include "ProjectModel.h"

#include <QObject>
#include <QStringList>

struct GenerationResult
{
    bool success = false;
    bool partial = false;
    QString fingerprint;
    QStringList generatedFiles;
    QStringList skippedProducts;
    QString failedProduct;
    QStringList notAttemptedProducts;
    QStringList warnings;
    QString error;
};

enum class VerificationStatus { Pass, Warning, Fail, NotApplicable };

struct VerificationCheck {
    QString id;
    QString name;
    VerificationStatus status = VerificationStatus::NotApplicable;
    QString details;
};

struct VerificationResult {
    VerificationStatus overallStatus = VerificationStatus::Fail;
    QString fingerprint;
    QList<VerificationCheck> checks;
    QString error;
};

struct FinalizationResult {
    bool success = false;
    bool alreadyFinalized = false;
    QString fingerprint;
    QStringList blockers;
    QString error;
};

struct AgentEntryPointDefinition {
    QString agentId;
    QString displayName;
    QString templatePath;
    QString targetPath;
    bool usesGenericBootstrap = true;
};

struct AgentEntryPointResult {
    bool success = false;
    QStringList createdFiles;
    QStringList updatedFiles;
    QStringList unchangedFiles;
    QStringList conflicts;
    QStringList genericAgents;
    QStringList errors;
};

QString projectConfigurationFingerprint(const ProjectModel& model,
                                        const GenerationOptions& options);

struct TemplateDefinition {
    enum class Kind { Module, CompositeTemplate };
    Kind kind = Kind::CompositeTemplate;
    QString id;
    QString displayName;
    QString projectType;
    DevelopmentEnvironment environment;
    DevelopmentCapabilities capabilities;
    AcademicConfiguration academic;
    AiConfiguration ai;
    QStringList recommendedRules;
    QStringList recommendedResources;
    QStringList recommendedAiConfiguration;
    QStringList supportedCapabilities;
    RuleConfiguration rules;
    MemoryConfiguration memory;
    ResourcePolicy resourcePolicy;
    GenerationOptions generation;
    CertificationConfiguration certification;
    QString description;
    QStringList exclusions;
    QJsonObject configuration;
    bool userDefined = false;
    bool official = false;
};

class TemplateManager final : public QObject
{
    Q_OBJECT

public:
    explicit TemplateManager(QObject* parent = nullptr, const QString& libraryPath = {});
    QStringList builtInTemplates() const;
    QList<TemplateDefinition> moduleDefinitions() const;
    QList<TemplateDefinition> compositeDefinitions() const;
    QList<TemplateDefinition> customDefinitions() const;
    QList<TemplateDefinition> officialDefinitions() const;
    TemplateDefinition definition(const QString& id) const;
    QList<TemplateDefinition> definitions() const;
    bool applyTemplate(ProjectModel* model, const QString& id, QString* error = nullptr) const;
    bool applyModules(ProjectModel* model, const QStringList& moduleIds, QString* error = nullptr) const;
    bool saveCustomTemplate(const ProjectModel& model, const QString& name, QString* id = nullptr, QString* error = nullptr);
    bool removeCustomTemplate(const QString& id, QString* error = nullptr);
    QString libraryPath() const { return libraryPath_; }
    QString libraryError() const;
signals:
    void templatesChanged();
private:
    QString libraryPath_;
};

class GenerationServices final : public QObject
{
    Q_OBJECT

public:
    explicit GenerationServices(QObject* parent = nullptr);
    GenerationResult generate(const ProjectModel& model,
                              const GenerationOptions& options) const;
};

class VerificationServices final : public QObject
{
    Q_OBJECT
public:
    explicit VerificationServices(QObject* parent = nullptr);
    VerificationResult verify(const ProjectModel& model,
                              const GenerationOptions& expectedOptions) const;
};

class FinalizationServices final : public QObject
{
    Q_OBJECT
public:
    explicit FinalizationServices(QObject* parent = nullptr);
    FinalizationResult finalize(const ProjectModel& model,
                                const GenerationOptions& expectedOptions) const;
};

class AgentEntryPointService final : public QObject
{
    Q_OBJECT
public:
    explicit AgentEntryPointService(QObject* parent = nullptr);
    AgentEntryPointResult createEntryPoints(const ProjectModel& model) const;
    QList<AgentEntryPointDefinition> definitions() const;
};
