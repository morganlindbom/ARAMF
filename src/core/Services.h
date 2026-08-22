// Services.h

#pragma once

#include "ProjectModel.h"

#include <QObject>
#include <QStringList>

struct GenerationResult
{
    bool success = false;
    QString fingerprint;
    QStringList generatedFiles;
    QStringList skippedProducts;
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
};

class TemplateManager final : public QObject
{
    Q_OBJECT

public:
    explicit TemplateManager(QObject* parent = nullptr);
    QStringList builtInTemplates() const;
    TemplateDefinition definition(const QString& id) const;
    QList<TemplateDefinition> definitions() const;
    bool applyTemplate(ProjectModel* model, const QString& id) const;
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
