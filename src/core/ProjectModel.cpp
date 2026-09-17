#include "ProjectModel.h"
#include <QJsonArray>
#include "AramfPaths.h"

#include <QUuid>
#include <QSet>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QUrl>
#include <QFile>

namespace {

bool isUrlLocation(const QString& location)
{
    return location.contains(QRegularExpression(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*://")));
}

bool containsAny(const QStringList& values, std::initializer_list<QStringView> candidates)
{
    for (const auto candidate : candidates) {
        if (values.contains(candidate.toString())) {
            return true;
        }
    }
    return false;
}

QString sourceText(const ProjectModel* model, const ProjectResource& resource)
{
    if (!resource.description.trimmed().isEmpty()) return resource.description;
    const QString path = QDir::isAbsolutePath(resource.location) ? resource.location : QDir(model->projectPath()).filePath(resource.location);
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString::fromUtf8(file.readAll());
    return {};
}

bool requirement(const QString& text, const QString& pattern)
{
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption).match(text).hasMatch();
}

}

QString canonicalResourceIdentity(const ProjectResource& resource, const QString& projectPath)
{
    const QString location = resource.location.trimmed();
    if (location.isEmpty()) return {};

    if (resource.type.compare(QStringLiteral("url"), Qt::CaseInsensitive) == 0 || isUrlLocation(location)) {
        QUrl url(location);
        if (!url.isValid()) return QStringLiteral("url:") + location;
        url.setScheme(url.scheme().toLower());
        url.setHost(url.host().toLower());
        url.setPath(QDir::cleanPath(url.path()));
        return QStringLiteral("url:") + url.toString(QUrl::FullyEncoded).toCaseFolded();
    }

    QString path = location;
    if (QDir::isRelativePath(path) && !projectPath.trimmed().isEmpty()) {
        path = QDir(projectPath).filePath(path);
    }
    QFileInfo info(QDir::cleanPath(QDir::fromNativeSeparators(path)));
    QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) canonical = info.absoluteFilePath();
    return QStringLiteral("local:") + QDir::cleanPath(QDir::fromNativeSeparators(canonical)).toCaseFolded();
}

bool sameResourceIdentity(const ProjectResource& left, const ProjectResource& right, const QString& projectPath)
{
    const QString leftIdentity = canonicalResourceIdentity(left, projectPath);
    const QString rightIdentity = canonicalResourceIdentity(right, projectPath);
    return !leftIdentity.isEmpty() && leftIdentity == rightIdentity;
}

QString deriveProjectContext(const DevelopmentCapabilities& capabilities)
{
    if (containsAny(capabilities.targetPlatforms, {u"android"})
        || containsAny(capabilities.frameworks, {u"android-sdk", u"jetpack-compose"})) {
        return QStringLiteral("android-application");
    }
    if (containsAny(capabilities.frameworks, {u"pico-sdk", u"arduino", u"platformio", u"esp-idf", u"zephyr"})
        && containsAny(capabilities.targetPlatforms, {u"embedded", u"embedded-system", u"microcontroller", u"bare-metal", u"rtos"})) {
        return QStringLiteral("embedded-firmware");
    }
    if (containsAny(capabilities.frameworks, {u"react", u"vue", u"angular", u"svelte", u"next-js"})
        && containsAny(capabilities.targetPlatforms, {u"web", u"web-browser", u"browser"})) {
        return QStringLiteral("frontend-web-application");
    }
    if (containsAny(capabilities.frameworks, {u"fastapi", u"flask", u"django", u"aspnet", u"spring-boot"})
        && containsAny(capabilities.targetPlatforms, {u"server", u"web-server", u"cloud"})) {
        return QStringLiteral("backend-service");
    }
    if (containsAny(capabilities.frameworks, {u"qt", u"qt6", u"wxwidgets", u"gtk", u"winui"})
        && containsAny(capabilities.targetPlatforms, {u"desktop", u"windows-desktop", u"linux-desktop", u"macos-desktop"})) {
        return QStringLiteral("desktop-application");
    }
    if (containsAny(capabilities.targetPlatforms, {u"library"})) {
        return QStringLiteral("reusable-library");
    }
    return QStringLiteral("software-development");
}

ProjectModel::ProjectModel(QObject* parent) : QObject(parent), projectId_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    environment_.ide = QStringLiteral("visual-studio-code");
    environment_.language = QStringLiteral("cpp");
    environment_.framework = QStringLiteral("none");
    environment_.compiler = QStringLiteral("msys2-ucrt64-gcc");
    environment_.operatingSystem = QStringLiteral("windows");
    environment_.targetPlatform = QStringLiteral("desktop");
    environment_.targetArchitecture = QStringLiteral("x86_64");
    environment_.buildSystem = QStringLiteral("cmake");
    environment_.packageManager = QStringLiteral("none");
    environment_.versionControl = QStringLiteral("none");
    capabilities_.languages = {QStringLiteral("cpp")};
    capabilities_.frameworks = {QStringLiteral("none")};
    capabilities_.ides = {QStringLiteral("visual-studio-code")};
    capabilities_.hostOperatingSystems = {QStringLiteral("windows")};
    capabilities_.targetPlatforms = {QStringLiteral("desktop")};
    capabilities_.targetArchitectures = {QStringLiteral("x86_64")};
    capabilities_.toolchains = {QStringLiteral("msys2-ucrt64-gcc")};
    capabilities_.buildSystems = {QStringLiteral("cmake")};
    capabilities_.buildConfigurations = {QStringLiteral("debug"), QStringLiteral("release")};
    resourcePolicy_.options = {QStringLiteral("read-relevant"), QStringLiteral("prefer-authoritative"), QStringLiteral("respect-scope"), QStringLiteral("ignore-disabled"), QStringLiteral("warn-conflicts")};
}

void ProjectModel::notifyChanged()
{
    modified_ = true;
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

void ProjectModel::setProjectFilePath(const QString& value)
{
    if (projectFilePath_ == value) return;
    projectFilePath_ = value;
    notifyChanged();
}

void ProjectModel::setProjectId(const QString& value)
{
    if (projectId_ == value || value.isEmpty()) return;
    projectId_ = value;
    notifyChanged();
}

void ProjectModel::setDescription(const QString& value)
{
    if (description_ == value) return;
    description_ = value;
    notifyChanged();
}

void ProjectModel::setTemplateState(const QJsonObject& state)
{
    if (templateState_ == state) return;
    templateState_ = state;
    if (projectTypeLocked()) context_ = state.value("projectType").toString();
    notifyChanged();
}

void ProjectModel::setTemplateId(const QString& value)
{
    if (templateId_ == value) return;
    templateId_ = value;
    if (value.isEmpty()) { templateState_ = {}; environmentOverrides_.clear(); }
    notifyChanged();
}

void ProjectModel::setWorkerNameSuffix(const QString& value)
{
    const QString normalized = AramfPaths::normalizeWorkerNameSuffix(value);
    const QString canonicalName = AramfPaths::workerDirectoryName(normalized);
    if (workerNameSuffix_ == normalized && projectName_ == canonicalName) return;
    workerNameSuffix_ = normalized;
    projectName_ = canonicalName;
    notifyChanged();
}

void ProjectModel::setTemplateModules(const QStringList& value)
{
    QStringList normalized = value;
    normalized.removeAll(QString());
    normalized.removeDuplicates();
    if (templateModules_ == normalized) return;
    templateModules_ = normalized;
    if (templateModules_.isEmpty()) {
        templateId_.clear();
        templateState_ = {};
        environmentOverrides_.clear();
    } else if (templateModules_.size() == 1) {
        templateId_ = templateModules_.first();
    } else {
        templateId_ = QStringLiteral("composed");
    }
    notifyChanged();
}

void ProjectModel::setContext(const QString& value)
{
    if (projectTypeLocked() && value != templateState_.value("projectType").toString()) return;
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
        && environment_.targetArchitecture == value.targetArchitecture
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
    if (environment_.targetArchitecture != value.targetArchitecture) environmentOverrides_.insert(QStringLiteral("targetArchitecture"));
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
    apply(QStringLiteral("targetArchitecture"), next.targetArchitecture, value.targetArchitecture);
    apply(QStringLiteral("buildSystem"), next.buildSystem, value.buildSystem);
    next.packageManager = value.packageManager;
    next.versionControl = value.versionControl;
    if (next.language == environment_.language
        && next.framework == environment_.framework
        && next.ide == environment_.ide
        && next.compiler == environment_.compiler
        && next.operatingSystem == environment_.operatingSystem
        && next.targetPlatform == environment_.targetPlatform
        && next.targetArchitecture == environment_.targetArchitecture
        && next.buildSystem == environment_.buildSystem) {
        return;
    }
    environment_ = next;
    emit developmentEnvironmentChanged();
    notifyChanged();
}

void ProjectModel::setDevelopmentCapabilities(const DevelopmentCapabilities& value)
{
    const QString derivedContext = projectTypeLocked() ? templateState_.value("projectType").toString() : deriveProjectContext(value);
    if (capabilities_.languages == value.languages
        && capabilities_.frameworks == value.frameworks
        && capabilities_.ides == value.ides
        && capabilities_.versionControlSystems == value.versionControlSystems
        && capabilities_.developmentTools == value.developmentTools
        && capabilities_.hostOperatingSystems == value.hostOperatingSystems
        && capabilities_.targetPlatforms == value.targetPlatforms
        && capabilities_.targetArchitectures == value.targetArchitectures
        && capabilities_.processorFamilies == value.processorFamilies
        && capabilities_.hardwareTargets == value.hardwareTargets
        && capabilities_.toolchains == value.toolchains
        && capabilities_.buildSystems == value.buildSystems
        && capabilities_.dependencyManagers == value.dependencyManagers
        && capabilities_.buildConfigurations == value.buildConfigurations
        && capabilities_.testingCapabilities == value.testingCapabilities
        && capabilities_.qualityCapabilities == value.qualityCapabilities
        && capabilities_.automationCapabilities == value.automationCapabilities
        && capabilities_.deliveryCapabilities == value.deliveryCapabilities
        && context_ == derivedContext) {
        return;
    }

    capabilities_ = value;
    context_ = derivedContext;
    resolveAndroidConstraints();
    const auto& effective = capabilities_;
    DevelopmentEnvironment next = environment_;
    next.language = effective.languages.isEmpty() ? QString() : effective.languages.first();
    next.framework = effective.frameworks.isEmpty() ? QString() : effective.frameworks.first();
    next.ide = effective.ides.isEmpty() ? QString() : effective.ides.first();
    next.compiler = effective.toolchains.isEmpty() ? QString() : effective.toolchains.first();
    next.operatingSystem = effective.hostOperatingSystems.isEmpty() ? QString() : effective.hostOperatingSystems.first();
    next.targetPlatform = effective.targetPlatforms.isEmpty() ? QString() : effective.targetPlatforms.first();
    next.targetArchitecture = effective.targetArchitectures.isEmpty() ? QString() : effective.targetArchitectures.first();
    next.buildSystem = effective.buildSystems.isEmpty() ? QString() : effective.buildSystems.first();
    next.packageManager = effective.dependencyManagers.isEmpty() ? QString() : effective.dependencyManagers.first();
    next.versionControl = effective.versionControlSystems.isEmpty() ? QString() : effective.versionControlSystems.first();
    if (next.language != environment_.language || next.framework != environment_.framework
        || next.ide != environment_.ide || next.compiler != environment_.compiler
        || next.operatingSystem != environment_.operatingSystem || next.targetPlatform != environment_.targetPlatform
        || next.targetArchitecture != environment_.targetArchitecture || next.buildSystem != environment_.buildSystem
        || next.packageManager != environment_.packageManager || next.versionControl != environment_.versionControl) {
        environment_ = next;
        emit developmentEnvironmentChanged();
    }
    emit developmentCapabilitiesChanged();
    notifyChanged();
}

void ProjectModel::setAcademicConfiguration(const AcademicConfiguration& value)
{
    AcademicConfiguration next = value;
    if (next.projectTypes.isEmpty() && !value.academicMode.isEmpty() && value.academicMode != QStringLiteral("disabled")) {
        const QString legacy = value.academicMode.startsWith(QStringLiteral("custom:")) ? QStringLiteral("other-custom") : value.academicMode == QStringLiteral("thesis") ? QStringLiteral("thesis-project") : value.academicMode;
        next.projectTypes << legacy;
    }
    next.projectTypes.removeDuplicates();
    next.enabled = value.enabled || !next.projectTypes.isEmpty() || (!value.academicMode.isEmpty() && value.academicMode != QStringLiteral("disabled"));
    if (!next.enabled) next.projectTypes.clear();
    if (next.projectTypes.isEmpty()) next.academicMode = QStringLiteral("disabled");
    else if (value.academicMode.isEmpty() || value.academicMode == QStringLiteral("disabled")) next.academicMode = next.projectTypes.first();
    else next.academicMode = value.academicMode;
    if (next.projectTypes.contains(QStringLiteral("thesis-project"))) next.thesisDocumentation.enabled = true;
    if (next.projectTypes.contains(QStringLiteral("report-project"))) next.reportDocumentation.enabled = true;
    auto normalize = [](AcademicConfiguration::DocumentationConfiguration& document, const QString& defaultId) {
        if (!document.enabled) {
            // Keep a previously selected source while inactive so removing a
            // checkbox does not discard the user's authoritative resource.
            if (document.templateSourceId.isEmpty()) document.templateMode = QStringLiteral("aramf-default");
        } else if (document.templateMode != QStringLiteral("source")) {
            document.templateMode = QStringLiteral("aramf-default");
            document.templateSourceId.clear();
        }
        if (document.templateMode == QStringLiteral("source")) {
            document.templateId.clear();
            document.templateVersion = 0;
        } else {
            document.templateId = defaultId;
            document.templateVersion = 1;
        }
        if (document.language.isEmpty()) document.language = QStringLiteral("sv");
        document.instructionId = defaultId == QStringLiteral("aramf-default-thesis") ? QStringLiteral("aramf-thesis-instruction") : QStringLiteral("aramf-report-instruction");
        document.instructionVersion = 1;
    };
    normalize(next.thesisDocumentation, QStringLiteral("aramf-default-thesis"));
    normalize(next.reportDocumentation, QStringLiteral("aramf-default-report"));
    const auto resolveSource = [this](AcademicConfiguration::DocumentationConfiguration& document, const QString& role) {
        if (!document.enabled || !document.templateSourceId.isEmpty()) return;
        QList<const ProjectResource*> candidates;
        for (const auto& resource : resources_) {
            if (resource.enabled && resource.role == role) candidates.append(&resource);
        }
        // A single matching source is deterministic. Multiple sources stay
        // unresolved so the authority/resource workflow can surface the
        // conflict instead of silently selecting one.
        if (candidates.size() == 1) {
            document.templateMode = QStringLiteral("source");
            document.templateSourceId = candidates.first()->id;
            document.templateId.clear();
            document.templateVersion = 0;
        }
    };
    resolveSource(next.thesisDocumentation, QStringLiteral("thesis-template"));
    resolveSource(next.reportDocumentation, QStringLiteral("report-template"));
    const bool unchanged = academic_.enabled == next.enabled
        && academic_.projectTypes == next.projectTypes
        && academic_.academicMode == next.academicMode
        && academic_.thesisLevel == value.thesisLevel
        && academic_.thesisApproaches == value.thesisApproaches
        && academic_.researchMethods == value.researchMethods
        && academic_.institution == value.institution
        && academic_.programmeOrCourse == value.programmeOrCourse
        && academic_.supervisor == value.supervisor
        && academic_.examiner == value.examiner
        && academic_.citationStyle == value.citationStyle
        && academic_.academicLanguage == value.academicLanguage
        && academic_.academicRequirements == value.academicRequirements
        && academic_.academicDeliverables == next.academicDeliverables
        && academic_.thesisDocumentation.enabled == next.thesisDocumentation.enabled
        && academic_.thesisDocumentation.templateMode == next.thesisDocumentation.templateMode
        && academic_.thesisDocumentation.templateSourceId == next.thesisDocumentation.templateSourceId
        && academic_.thesisDocumentation.templateId == next.thesisDocumentation.templateId
        && academic_.thesisDocumentation.templateVersion == next.thesisDocumentation.templateVersion
        && academic_.thesisDocumentation.language == next.thesisDocumentation.language
        && academic_.thesisDocumentation.instructionId == next.thesisDocumentation.instructionId
        && academic_.thesisDocumentation.instructionVersion == next.thesisDocumentation.instructionVersion
        && academic_.reportDocumentation.enabled == next.reportDocumentation.enabled
        && academic_.reportDocumentation.templateMode == next.reportDocumentation.templateMode
        && academic_.reportDocumentation.templateSourceId == next.reportDocumentation.templateSourceId
        && academic_.reportDocumentation.templateId == next.reportDocumentation.templateId
        && academic_.reportDocumentation.templateVersion == next.reportDocumentation.templateVersion
        && academic_.reportDocumentation.language == next.reportDocumentation.language
        && academic_.reportDocumentation.instructionId == next.reportDocumentation.instructionId
        && academic_.reportDocumentation.instructionVersion == next.reportDocumentation.instructionVersion;
    if (unchanged) {
        return;
    }
    academic_ = next;
    notifyChanged();
}

void ProjectModel::setAiConfiguration(const AiConfiguration& value)
{
    AiConfiguration next = value;
    if (next.primaryAgent.isEmpty()) {
        next.primaryAgent = QStringLiteral("none");
    }
    next.additionalAgents.removeAll(next.primaryAgent);
    if (ai_.primaryAgent == next.primaryAgent
        && ai_.additionalAgents == next.additionalAgents
        && ai_.responsibilities == next.responsibilities
        && ai_.permissions == next.permissions
        && ai_.aramfIntegrations == next.aramfIntegrations
        && ai_.customAgentName == next.customAgentName
        && ai_.autonomyPreset == next.autonomyPreset) {
        return;
    }
    ai_ = next;
    aiPlatforms_.clear();
    if (ai_.primaryAgent != QStringLiteral("none")) {
        aiPlatforms_ << ai_.primaryAgent;
    }
    aiPlatforms_ << ai_.additionalAgents;
    emit aiConfigurationChanged();
    emit aiPlatformsChanged();
    notifyChanged();
}

void ProjectModel::applyTemplateCapabilities(const DevelopmentCapabilities& value)
{
    DevelopmentCapabilities next = value;
    if (environmentOverrides_.contains(QStringLiteral("language"))) next.languages = {environment_.language};
    if (environmentOverrides_.contains(QStringLiteral("framework"))) next.frameworks = {environment_.framework};
    if (environmentOverrides_.contains(QStringLiteral("ide"))) next.ides = {environment_.ide};
    if (environmentOverrides_.contains(QStringLiteral("compiler"))) next.toolchains = {environment_.compiler};
    if (environmentOverrides_.contains(QStringLiteral("operatingSystem"))) next.hostOperatingSystems = {environment_.operatingSystem};
    if (environmentOverrides_.contains(QStringLiteral("targetPlatform"))) next.targetPlatforms = {environment_.targetPlatform};
    if (environmentOverrides_.contains(QStringLiteral("targetArchitecture"))) next.targetArchitectures = {environment_.targetArchitecture};
    if (environmentOverrides_.contains(QStringLiteral("buildSystem"))) next.buildSystems = {environment_.buildSystem};
    const QSet<QString> oldOverrides = environmentOverrides_;
    environmentOverrides_.clear();
    setDevelopmentCapabilities(next);
    environmentOverrides_ = oldOverrides;
}

void ProjectModel::setAiPlatforms(const QStringList& value)
{
    if (aiPlatforms_ == value) return;
    aiPlatforms_ = value;
    ai_.primaryAgent = value.isEmpty() ? QStringLiteral("none") : value.first();
    ai_.additionalAgents = value.mid(1);
    emit aiPlatformsChanged();
    emit aiConfigurationChanged();
    notifyChanged();
}

void ProjectModel::setResourceNames(const QStringList& value)
{
    if (resourceNames_ == value) return;
    resourceNames_ = value;
    if (resources_.isEmpty()) {
        for (const auto& name : value) {
            ProjectResource resource;
            resource.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            resource.name = name;
            resource.type = QStringLiteral("other");
            resource.role = QStringLiteral("supporting-material");
            resource.authorityLevel = QStringLiteral("supporting-reference");
            resources_.append(resource);
        }
    }
    notifyChanged();
}

void ProjectModel::setResources(const QList<ProjectResource>& value)
{
    if (resources_.size() == value.size()) {
        bool unchanged = true;
        for (int index = 0; index < value.size(); ++index) {
            const auto& a = resources_.at(index);
            const auto& b = value.at(index);
            if (a.id != b.id || a.name != b.name || a.type != b.type || a.location != b.location
                || a.description != b.description || a.enabled != b.enabled || a.locationMode != b.locationMode
                || a.role != b.role || a.authorityLevel != b.authorityLevel || a.scopes != b.scopes || a.status != b.status
                || a.loadingStrategyOverride != b.loadingStrategyOverride || a.lastModified != b.lastModified
                || a.fingerprint != b.fingerprint) {
                unchanged = false;
                break;
            }
        }
        if (unchanged) return;
    }
    resources_ = value;
    // Preserve callers using the legacy authority-only Source of Truth API.
    for (auto& resource : resources_) {
        if (resource.role == QStringLiteral("supporting-material")
            && resource.authorityLevel.compare(QStringLiteral("primary-source-of-truth"), Qt::CaseInsensitive) == 0)
            resource.role = QStringLiteral("source-of-truth");
    }
    resourceNames_.clear();
    for (const auto& resource : resources_) resourceNames_ << resource.name;
    resolveAndroidConstraints();
    // Resources own custom-template selection. Re-normalize documentation so
    // a resource added after Academic selection is resolved automatically.
    setAcademicConfiguration(academic_);
    notifyChanged();
}

void ProjectModel::setResourcePolicy(const ResourcePolicy& value)
{
    if (resourcePolicy_.options == value.options && resourcePolicy_.loadingStrategy == value.loadingStrategy) return;
    resourcePolicy_ = value;
    notifyChanged();
}

void ProjectModel::setRuleConfiguration(const RuleConfiguration& value)
{
    auto normalized = value;
    for (auto it = normalized.scopeMetadata.begin(); it != normalized.scopeMetadata.end(); ++it) {
        auto metadata = it.value().toObject();
        if (!it.value().isObject()) continue;
        for (auto field = metadata.begin(); field != metadata.end(); ++field) {
            if (!field.value().isArray()) continue;
            QStringList entries;
            bool stringSet = true;
            for (const auto& entry : field.value().toArray()) {
                if (!entry.isString()) { stringSet = false; break; }
                entries.append(entry.toString());
            }
            if (stringSet) { entries.removeDuplicates(); entries.sort(); field.value() = QJsonArray::fromStringList(entries); }
        }
        it.value() = metadata;
    }
    if (ruleConfiguration_.activeCategories == value.activeCategories
        && ruleConfiguration_.enforcementLevel == value.enforcementLevel
        && ruleConfiguration_.loadingStrategy == value.loadingStrategy
        && ruleConfiguration_.workScopes == value.workScopes
        && ruleConfiguration_.projectScopes == value.projectScopes
        && ruleConfiguration_.scopeMetadata == normalized.scopeMetadata
        && ruleConfiguration_.contextPolicies == value.contextPolicies
        && ruleConfiguration_.conflictPolicy == value.conflictPolicy) return;
    ruleConfiguration_ = normalized;
    notifyChanged();
}

void ProjectModel::setMemoryConfiguration(const MemoryConfiguration& value)
{
    if (memoryConfiguration_.writerMode == value.writerMode
        && memoryConfiguration_.captureCategories == value.captureCategories
        && memoryConfiguration_.retentionLevel == value.retentionLevel
        && memoryConfiguration_.maintenanceOptions == value.maintenanceOptions
        && memoryConfiguration_.validationOptions == value.validationOptions
        && memoryConfiguration_.updateStrategy == value.updateStrategy
        && memoryConfiguration_.historyOptions == value.historyOptions
        && memoryConfiguration_.maximumSizeBytes == value.maximumSizeBytes) return;
    memoryConfiguration_ = value;
    notifyChanged();
}

void ProjectModel::setCertificationConfiguration(const CertificationConfiguration& value)
{
    if (certificationConfiguration_.enabled == value.enabled
        && certificationConfiguration_.defaultVerificationLevel == value.defaultVerificationLevel) return;
    certificationConfiguration_ = value;
    notifyChanged();
}

void ProjectModel::setAndroidConstraints(const AndroidProjectConstraints& value)
{
    androidConstraints_ = value;
    notifyChanged();
}

void ProjectModel::resolveAndroidConstraints()
{
    AndroidProjectConstraints next;
    if (templateId_ != QStringLiteral("android-studio-kotlin-gemini") && context_ != QStringLiteral("android-application")) {
        androidConstraints_ = next;
        return;
    }
    next.composeSelected = capabilities_.frameworks.contains("jetpack-compose");
    next.uiTechnology = next.composeSelected ? "compose" : "xml";
    for (const auto& resource : resources_) {
        if (!resource.enabled || resource.role.compare(QStringLiteral("source-of-truth"), Qt::CaseInsensitive) != 0) continue;
        const QString text = sourceText(this, resource);
        if (text.trimmed().isEmpty()) continue;
        const QString source = resource.id;
        next.sourceOfTruthResource = source;
        next.sourceOfTruthTitle = resource.name;
        next.minSdkSource = source; next.primaryIdeSource = source; next.kotlinSource = source;
        next.uiTechnologySource = source; next.composeSource = source; next.roomSource = source;
        next.unitTestsSource = source; next.lintSource = source;
        const auto sdk = QRegularExpression(QStringLiteral("minimum\\s+sdk\\s*[=:]\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption).match(text);
        if (sdk.hasMatch()) { next.minSdk = sdk.captured(1).toInt(); next.minSdkSpecified = true; }
        const auto setText = [](QString& value, QString& provenance, const QString& derived, const QString& source) {
            value = derived; provenance = source;
        };
        if (text.contains(QStringLiteral("LABORATORY HANDBOOK: MULTI-AGENT SANDBOX"), Qt::CaseInsensitive))
            setText(next.courseName, next.courseNameSource, QStringLiteral("Android Development and Software Architecture"), source);
        if (text.contains(QStringLiteral("smart-home-gitops"), Qt::CaseInsensitive))
            setText(next.projectDomain, next.projectDomainSource, QStringLiteral("Smart Home GitOps control application"), source);
        if (requirement(text, QStringLiteral("\\bMVVM\\b.*(required|strict|separation)|strict separation.*MVVM")))
            setText(next.architecture, next.architectureSource, QStringLiteral("MVVM"), source);
        const QList<QPair<QString, QString>> technologies = {
            {QStringLiteral("Kotlin Coroutines"), QStringLiteral("kotlin\\s+coroutines")},
            {QStringLiteral("Jetpack Compose"), QStringLiteral("jetpack\\s+compose")},
            {QStringLiteral("GitOps"), QStringLiteral("gitops")},
            {QStringLiteral("Retrofit"), QStringLiteral("retrofit")},
            {QStringLiteral("Gson"), QStringLiteral("\\bgson\\b")},
            {QStringLiteral("Kotlin Flow / StateFlow"), QStringLiteral("stateflow|kotlin\\s+flow")},
            {QStringLiteral("Regex or weighted heuristics"), QStringLiteral("regex\\s+or\\s+weighted\\s+heuristics|weighted\\s+heuristics")}
        };
        for (const auto& technology : technologies) {
            if (requirement(text, technology.second)) next.declaredTechnologies << technology.first;
        }
        if (!next.declaredTechnologies.isEmpty()) next.declaredTechnologiesSource = source;
        next.kotlinRequired = requirement(text, QStringLiteral("kotlin.*(mandatory|required)"));
        if (next.kotlinRequired) next.kotlinSource = source;
        if (requirement(text, QStringLiteral("android\\s+studio.*(official|primary|required)"))) next.primaryIde = QStringLiteral("android-studio");
        if (next.primaryIde == QStringLiteral("android-studio")) next.primaryIdeSource = source;
        next.composeRequired = requirement(text, QStringLiteral("jetpack\\s+compose.*(explicitly|required|must)|ui framework:\\s*jetpack\\s+compose"));
        if (next.composeRequired) { next.composeRequiredSource = source; next.composeSource = source; next.uiTechnology = QStringLiteral("jetpack-compose"); next.uiTechnologySource = source; }
        if (requirement(text, QStringLiteral("xml\\s+layouts?.*(mandatory|required)"))) { next.xmlRequired = true; next.uiTechnology = QStringLiteral("xml"); next.uiTechnologySource = source; }
        if (requirement(text, QStringLiteral("compose.*(must\\s+not|prohibited|forbidden|disabled)"))) { next.composeAllowed = false; next.composeSelected = false; }
        else if (next.composeRequired) { next.composeAllowed = true; next.composeSelected = true; }
        next.roomRequired = requirement(text, QStringLiteral("room\\s+(?:is\\s+)?(?:mandatory|required)|(?:mandatory|required|must\\s+use)\\s+room"));
        next.unitTestsRequired = requirement(text, QStringLiteral("unit\\s+tests?.*(mandatory|required)|validation expectations.*unit\\s+tests"));
        next.lintRequired = requirement(text, QStringLiteral("lint.*(mandatory|required|must\\s+pass)"));
        next.retrofitRequired = requirement(text, QStringLiteral("retrofit.*(required|must)")); next.retrofitSource = next.retrofitRequired ? source : QString();
        next.gsonRequired = requirement(text, QStringLiteral("gson.*(required|must)")); next.gsonSource = next.gsonRequired ? source : QString();
        next.internetPermissionRequired = requirement(text, QStringLiteral("internet permission.*required|android\\.permission\\.internet")); next.internetPermissionSource = next.internetPermissionRequired ? source : QString();
        next.githubApiRequired = requirement(text, QStringLiteral("github api.*required|github api.*communication")); next.githubApiSource = next.githubApiRequired ? source : QString();
        next.githubPatRequired = requirement(text, QStringLiteral("personal access token|github pat|github.*PAT")); next.githubPatSource = next.githubPatRequired ? source : QString();
        next.classicPatRequired = requirement(text, QStringLiteral("PAT type:\\s*classic|classic.*PAT")); next.classicPatSource = next.classicPatRequired ? source : QString();
        if (requirement(text, QStringLiteral("scope:\\s*`?repo`?|\\brepo\\s+scope"))) { next.patScope = QStringLiteral("repo"); next.patScopeSource = source; }
        next.hardcodedTokenProhibited = requirement(text, QStringLiteral("hardcoded token.*(prohibited|never)|token.*not.*hardcoded|must never be hardcoded")); next.hardcodedTokenSource = next.hardcodedTokenProhibited ? source : QString();
        next.localPropertiesRequired = requirement(text, QStringLiteral("local\\.properties")); next.localPropertiesSource = next.localPropertiesRequired ? source : QString();
        next.buildConfigRequired = requirement(text, QStringLiteral("buildconfig|build config")); next.buildConfigSource = next.buildConfigRequired ? source : QString();
        next.privateRepositoryRequired = requirement(text, QStringLiteral("strictly private|private github repository")); next.privateRepositorySource = next.privateRepositoryRequired ? source : QString();
        next.instructorCollaboratorRequired = requirement(text, QStringLiteral("instructor collaborator.*(required|write permission)")); next.instructorCollaboratorSource = next.instructorCollaboratorRequired ? source : QString();
        if (text.contains(QStringLiteral("smart-home-gitops"), Qt::CaseInsensitive)) { next.repositoryName = QStringLiteral("smart-home-gitops"); next.repositoryNameSource = source; }
        if (requirement(text, QStringLiteral("initial branch:?\\s*`?main`?"))) { next.initialBranch = QStringLiteral("main"); next.initialBranchSource = source; }
        if (requirement(text, QStringLiteral("house_config\\.json"))) { next.requiredInitialFile = QStringLiteral("house_config.json"); next.requiredInitialFileSource = source; }
        if ((text.contains(QStringLiteral("house_config.json"), Qt::CaseInsensitive) && text.contains(QStringLiteral("official house state"), Qt::CaseInsensitive))
            || requirement(text, QStringLiteral("house_config\\.json.*single source|single source.*house_config"))) { next.applicationStateSource = QStringLiteral("GitHub/house_config.json"); next.applicationStateSourceSource = source; }
        if (requirement(text, QStringLiteral("declaratively managed|state model.*declarative"))) { next.stateModel = QStringLiteral("declarative"); next.stateModelSource = source; }
        if (requirement(text, QStringLiteral("workflow.*gitops|gitops.*workflow"))) { next.workflow = QStringLiteral("GitOps"); next.workflowSource = source; }
        next.applicationStateFields = {QStringLiteral("target_temperature"), QStringLiteral("living_room_lights"), QStringLiteral("hvac_mode"), QStringLiteral("security_system"), QStringLiteral("last_updated_by")};
        if (text.contains(QStringLiteral("target_temperature"), Qt::CaseInsensitive)) next.applicationStateFieldsSource = source;
        if (requirement(text, QStringLiteral("EcoAgent"))) next.domainAgents << QStringLiteral("EcoAgent");
        if (requirement(text, QStringLiteral("LuxAgent"))) next.domainAgents << QStringLiteral("LuxAgent");
        if (!next.domainAgents.isEmpty()) { next.domainAgentsSource = source; next.domainAgentDistinction = QStringLiteral("EcoAgent and LuxAgent are application/course simulation agents; Gemini, Codex, and ChatGPT are ARAMF development agents."); next.domainAgentDistinctionSource = source; }
        next.pollingRequired = requirement(text, QStringLiteral("polling.*required|polling loop")); next.pollingSource = next.pollingRequired ? source : QString();
        const auto interval = QRegularExpression(QStringLiteral("(30)\\s*seconds"), QRegularExpression::CaseInsensitiveOption).match(text);
        if (interval.hasMatch()) { next.pollingIntervalSeconds = interval.captured(1).toInt(); next.pollingIntervalSource = source; }
        if (requirement(text, QStringLiteral("execution location:\\s*viewmodel|polling.*viewmodel"))) { next.pollingExecutionLocation = QStringLiteral("ViewModel"); next.pollingExecutionLocationSource = source; }
        if (requirement(text, QStringLiteral("Dispatchers\\.IO"))) { next.networkDispatcher = QStringLiteral("Dispatchers.IO"); next.networkDispatcherSource = source; }
        if (requirement(text, QStringLiteral("Dispatchers\\.Default"))) { next.analysisDispatcher = QStringLiteral("Dispatchers.Default"); next.analysisDispatcherSource = source; }
        next.stateFlowRequired = requirement(text, QStringLiteral("stateflow")); next.stateFlowSource = next.stateFlowRequired ? source : QString();
        next.collectAsStateRequired = requirement(text, QStringLiteral("collectAsState")); next.collectAsStateSource = next.collectAsStateRequired ? source : QString();
        next.deceptionDetectorRequired = requirement(text, QStringLiteral("deceptiondetector")); next.deceptionDetectorSource = next.deceptionDetectorRequired ? source : QString();
        next.regexOrHeuristicsRequired = requirement(text, QStringLiteral("regex\\s+or\\s+weighted\\s+heuristics")); next.regexOrHeuristicsSource = next.regexOrHeuristicsRequired ? source : QString();
        if (requirement(text, QStringLiteral("0\\s*[-–]\\s*100%|0–100%|0-100"))) { next.confidenceMinPercent = 0; next.confidenceMaxPercent = 100; next.confidenceRangeSource = source; }
        next.normalGreenRequired = requirement(text, QStringLiteral("normal.*green|green ui")); next.normalGreenSource = next.normalGreenRequired ? source : QString();
        next.securityAlertRedRequired = requirement(text, QStringLiteral("securityalert.*red|red ui")); next.securityAlertRedSource = next.securityAlertRedRequired ? source : QString();
        next.rawAdversarialTextRequired = requirement(text, QStringLiteral("raw (ai|adversarial).*text|raw ai text")); next.rawAdversarialTextSource = next.rawAdversarialTextRequired ? source : QString();
        next.humanInTheLoopRequired = requirement(text, QStringLiteral("human-in-the-loop.*required|human operator.*override")); next.humanInTheLoopSource = next.humanInTheLoopRequired ? source : QString();
        next.validationRequirements = {QStringLiteral("Android compilation"), QStringLiteral("unit tests"), QStringLiteral("application startup"), QStringLiteral("emulator verification"), QStringLiteral("MVVM structural verification"), QStringLiteral("secure token handling verification"), QStringLiteral("polling verification"), QStringLiteral("Dispatchers.IO verification"), QStringLiteral("Dispatchers.Default verification"), QStringLiteral("Compose reactive state verification"), QStringLiteral("GitHub API behavior verification"), QStringLiteral("deception detector verification"), QStringLiteral("confidence score verification")};
        next.validationRequirementsSource = source;
        next.submissionRequirements = {QStringLiteral("screen recording with audio"), QStringLiteral("Canvas submission"), QStringLiteral("private GitHub repository link")}; next.submissionRequirementsSource = source;
        if (requirement(text, QStringLiteral("maximum 3\\s*[-–]\\s*5 minutes|3\\s*[-–]\\s*5 minutes"))) { next.submissionVideoDuration = QStringLiteral("maximum 3–5 minutes"); next.submissionVideoDurationSource = source; }
        next.submissionSegments = {QStringLiteral("Live Simulation Demonstration"), QStringLiteral("MVVM Architecture & Security Walkthrough"), QStringLiteral("Algorithm Analysis / Verification")}; next.submissionSegmentsSource = source;
        next.unresolvedRequirements = {QStringLiteral("Android minimum SDK"), QStringLiteral("target compile SDK"), QStringLiteral("application package name"), QStringLiteral("exact instructor GitHub username"), QStringLiteral("exact GitHub API endpoints"), QStringLiteral("Human-in-the-Loop override UI"), QStringLiteral("deception scoring weights")};
        next.unresolvedRequirementsSource = source;
        break;
    }
    androidConstraints_ = next;
    if (!next.composeAllowed || !next.composeSelected) capabilities_.frameworks.removeAll(QStringLiteral("jetpack-compose"));
    auto select = [](QStringList& items, const QString& id) { if (!items.contains(id)) items << id; };
    if (next.composeRequired && next.composeAllowed) select(capabilities_.frameworks, "jetpack-compose");
    if (next.roomRequired) select(capabilities_.frameworks, "room");
    if (next.kotlinRequired) select(capabilities_.languages, "kotlin");
    if (next.unitTestsRequired) select(capabilities_.testingCapabilities, "unit-testing");
    if (next.lintRequired) select(capabilities_.qualityCapabilities, "linting");
}

void ProjectModel::setGenerationOptions(const GenerationOptions& value)
{
    if (generationOptions_.generateAgentRules == value.generateAgentRules
        && generationOptions_.generateRouting == value.generateRouting
        && generationOptions_.generatePlatforms == value.generatePlatforms
        && generationOptions_.generateResources == value.generateResources
        && generationOptions_.generateMemory == value.generateMemory
        && generationOptions_.generateProvenance == value.generateProvenance) {
        return;
    }
    generationOptions_ = value;
    notifyChanged();
}

void ProjectModel::setHardwareResources(const QList<HardwareResource>& value)
{
    if (hardwareResources_.size() == value.size()) {
        bool equal = true;
        for (int i = 0; i < value.size(); ++i) {
            const auto& a = hardwareResources_.at(i); const auto& b = value.at(i);
            equal &= a.id == b.id && a.endpointId == b.endpointId && a.resourceType == b.resourceType
                && a.physicalResource == b.physicalResource && a.direction == b.direction
                && a.logicalMode == b.logicalMode && a.activeLevel == b.activeLevel && a.purpose == b.purpose
                && a.ownership == b.ownership && a.capabilities == b.capabilities;
        }
        if (equal) return;
    }
    hardwareResources_ = value;
    notifyChanged();
}

void ProjectModel::setCommunicationConfiguration(const CommunicationConfiguration& value)
{
    CommunicationConfiguration normalized = value;
    if (normalized.endpoints.size() >= 2) {
        if (!normalized.sourceTarget.isEmpty()) normalized.endpoints[0].targetId = normalized.sourceTarget;
        if (!normalized.destinationTarget.isEmpty()) normalized.endpoints[1].targetId = normalized.destinationTarget;
        if (!normalized.sourceRole.isEmpty()) normalized.endpoints[0].role = normalized.sourceRole;
        if (!normalized.destinationRole.isEmpty()) normalized.endpoints[1].role = normalized.destinationRole;
        normalized.sourceTarget = normalized.endpoints.at(0).targetId;
        normalized.destinationTarget = normalized.endpoints.at(1).targetId;
        normalized.sourceRole = normalized.endpoints.at(0).role;
        normalized.destinationRole = normalized.endpoints.at(1).role;
    }
    if (!normalized.links.isEmpty()) {
        auto& link = normalized.links.first();
        if (!normalized.transport.isEmpty()) link.transport = normalized.transport;
        if (!normalized.protocol.isEmpty()) link.protocol = normalized.protocol;
        if (!normalized.protocolVersion.isEmpty()) link.protocolVersion = normalized.protocolVersion;
        normalized.transport = link.transport;
        normalized.protocol = link.protocol;
        normalized.protocolVersion = link.protocolVersion;
    }
    if (normalized.endpoints.isEmpty() && !normalized.sourceTarget.isEmpty() && !normalized.destinationTarget.isEmpty()) {
        normalized.endpoints = {{QStringLiteral("endpoint-a"), normalized.sourceTarget, normalized.sourceTarget, normalized.sourceRole, {}, {}},
                                {QStringLiteral("endpoint-b"), normalized.destinationTarget, normalized.destinationTarget, normalized.destinationRole, {}, {}}};
    }
    // One-time migration of the legacy shared endpoint into the canonical
    // listening endpoint. Generic endpoint addresses remain authoritative once
    // any address has been configured.
    if (!normalized.endpoint.trimmed().isEmpty() && !normalized.endpoints.isEmpty()) {
        bool hasAddress = false;
        for (const auto& endpoint : normalized.endpoints) hasAddress |= !endpoint.address.trimmed().isEmpty();
        if (!hasAddress) {
            auto server = std::find_if(normalized.endpoints.begin(), normalized.endpoints.end(), [](const auto& endpoint) {
                return endpoint.role.compare(QStringLiteral("server"), Qt::CaseInsensitive) == 0;
            });
            if (server == normalized.endpoints.end()) server = normalized.endpoints.begin() + (normalized.endpoints.size() > 1 ? 1 : 0);
            server->address = normalized.endpoint;
        }
    }
    if (normalized.links.isEmpty() && normalized.endpoints.size() >= 2) {
        normalized.links = {{QStringLiteral("link-1"), normalized.endpoints.at(0).id, normalized.endpoints.at(1).id,
                             QStringLiteral("bidirectional"), normalized.transport, normalized.protocol, {}, {}, {}, normalized.protocolVersion,
                             {}, normalized.reconnectPolicy, normalized.errorHandling, false, 0, {}}};
    }
    auto endpointsEqual = [](const QList<CommunicationEndpoint>& a, const QList<CommunicationEndpoint>& b) {
        if (a.size() != b.size()) return false;
        for (int i = 0; i < a.size(); ++i) if (a.at(i).id != b.at(i).id || a.at(i).targetId != b.at(i).targetId || a.at(i).displayName != b.at(i).displayName || a.at(i).role != b.at(i).role || a.at(i).capabilities != b.at(i).capabilities || a.at(i).address != b.at(i).address) return false;
        return true;
    };
    auto linksEqual = [](const QList<CommunicationLink>& a, const QList<CommunicationLink>& b) {
        if (a.size() != b.size()) return false;
        for (int i = 0; i < a.size(); ++i) if (a.at(i).id != b.at(i).id || a.at(i).endpointA != b.at(i).endpointA || a.at(i).endpointB != b.at(i).endpointB || a.at(i).direction != b.at(i).direction || a.at(i).transport != b.at(i).transport || a.at(i).protocol != b.at(i).protocol || a.at(i).frameType != b.at(i).frameType || a.at(i).logicalDataModel != b.at(i).logicalDataModel || a.at(i).wireEncoding != b.at(i).wireEncoding || a.at(i).protocolVersion != b.at(i).protocolVersion || a.at(i).byteOrder != b.at(i).byteOrder || a.at(i).timeout != b.at(i).timeout || a.at(i).reconnectPolicy != b.at(i).reconnectPolicy || a.at(i).errorHandling != b.at(i).errorHandling || a.at(i).acknowledgement != b.at(i).acknowledgement || a.at(i).maximumPacketSize != b.at(i).maximumPacketSize) return false;
        return true;
    };
    if (communication_.enabled == normalized.enabled
        && communication_.sourceTarget == normalized.sourceTarget
        && communication_.destinationTarget == value.destinationTarget
        && communication_.transport == value.transport
        && communication_.protocol == value.protocol
        && communication_.sourceRole == value.sourceRole
        && communication_.destinationRole == value.destinationRole
        && communication_.endpoint == value.endpoint
        && communication_.dataFormat == value.dataFormat
        && communication_.protocolVersion == value.protocolVersion
        && communication_.authenticationRequired == value.authenticationRequired
        && communication_.encryptionRequired == value.encryptionRequired
        && communication_.reconnectPolicy == value.reconnectPolicy
        && communication_.errorHandling == value.errorHandling
        && communication_.integrationRequirements == normalized.integrationRequirements
        && endpointsEqual(communication_.endpoints, normalized.endpoints)
        && linksEqual(communication_.links, normalized.links)
        && communication_.messages.size() == normalized.messages.size()
        && communication_.testVectors.size() == normalized.testVectors.size()) return;
    communication_ = normalized;
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
        emit developmentCapabilitiesChanged();
        emit developmentEnvironmentChanged();
        emit aiConfigurationChanged();
        emit modelChanged();
    }
}

void ProjectModel::resetForNewProject()
{
    beginUpdate();
    projectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    projectName_ = QStringLiteral("New AR&MF Project");
    projectPath_.clear();
    projectFilePath_.clear();
    workerNameSuffix_.clear();
    description_.clear();
    templateId_.clear();
    templateModules_.clear();
    templateState_ = {};
    context_.clear();
    environment_ = {};
    environment_.ide = QStringLiteral("visual-studio-code");
    environment_.language = QStringLiteral("cpp");
    environment_.framework = QStringLiteral("none");
    environment_.compiler = QStringLiteral("msys2-ucrt64-gcc");
    environment_.operatingSystem = QStringLiteral("windows");
    environment_.targetPlatform = QStringLiteral("desktop");
    environment_.targetArchitecture = QStringLiteral("x86_64");
    environment_.buildSystem = QStringLiteral("cmake");
    environment_.packageManager = QStringLiteral("none");
    environment_.versionControl = QStringLiteral("none");
    capabilities_ = {};
    capabilities_.languages = {QStringLiteral("cpp")};
    capabilities_.frameworks = {QStringLiteral("none")};
    capabilities_.ides = {QStringLiteral("visual-studio-code")};
    capabilities_.hostOperatingSystems = {QStringLiteral("windows")};
    capabilities_.targetPlatforms = {QStringLiteral("desktop")};
    capabilities_.targetArchitectures = {QStringLiteral("x86_64")};
    capabilities_.toolchains = {QStringLiteral("msys2-ucrt64-gcc")};
    capabilities_.buildSystems = {QStringLiteral("cmake")};
    capabilities_.buildConfigurations = {QStringLiteral("debug"), QStringLiteral("release")};
    academic_ = {};
    aiPlatforms_.clear();
    resourceNames_.clear();
    resources_.clear();
    resourcePolicy_ = {};
    resourcePolicy_.options = {QStringLiteral("read-relevant"), QStringLiteral("prefer-authoritative"), QStringLiteral("respect-scope"), QStringLiteral("ignore-disabled"), QStringLiteral("warn-conflicts")};
    ruleConfiguration_ = {};
    memoryConfiguration_ = {};
    certificationConfiguration_ = {};
    generationOptions_ = {};
    communication_ = {};
    androidConstraints_ = {};
    profileSelections_.clear();
    ai_ = {};
    options_.clear();
    environmentOverrides_.clear();
    projectSchemaVersion_ = ProjectSchema::CurrentVersion;
    migratedFromSchemaVersion_ = ProjectSchema::CurrentVersion;
    migrationStatus_ = ProjectSchema::MigrationOk;
    migrationNotices_ = {};
    targetRelease_ = 0;
    completedPageIds_.clear();
    processVersionState_ = ProcessVersionState::empty();
    orchestrationState_ = {};
    notifyChanged();
    endUpdate();
    setModified(false);
}

void ProjectModel::setModified(bool modified)
{
    if (modified_ == modified) return;
    modified_ = modified;
    emit modifiedChanged(modified_);
}

void ProjectModel::setMigrationState(int sourceVersion, const QString& status, const QJsonArray& notices)
{
    projectSchemaVersion_ = ProjectSchema::CurrentVersion;
    migratedFromSchemaVersion_ = sourceVersion;
    migrationStatus_ = status;
    migrationNotices_ = notices;
}

void ProjectModel::setTargetRelease(int release)
{
    const int normalized = qMax(0, release);
    if (targetRelease_ == normalized) return;
    targetRelease_ = normalized;
    notifyChanged();
}

void ProjectModel::setPageCompleted(const QString& pageId, bool completed)
{
    const QString key = pageId.trimmed();
    if (key.isEmpty()) return;
    const bool alreadyCompleted = completedPageIds_.contains(key);
    if (alreadyCompleted == completed) return;
    if (completed) completedPageIds_.insert(key);
    else completedPageIds_.remove(key);
    notifyChanged();
}

void ProjectModel::setCompletedPageIds(const QSet<QString>& pageIds)
{
    QSet<QString> normalized;
    for (const auto& pageId : pageIds) {
        const QString key = pageId.trimmed();
        if (!key.isEmpty()) normalized.insert(key);
    }
    completedPageIds_ = normalized;
}

void ProjectModel::setOrchestrationState(const QJsonObject& state)
{
    if (orchestrationState_ == state) return;
    orchestrationState_ = state;
    notifyChanged();
}

bool ProjectModel::startNextProcess(QString* error)
{
    if (!ProcessVersionLifecycle::startNextProcess(&processVersionState_, error)) return false;
    notifyChanged();
    return true;
}

bool ProjectModel::advanceProcessIteration(QString* error)
{
    if (!ProcessVersionLifecycle::advanceIteration(&processVersionState_, error)) return false;
    notifyChanged();
    return true;
}

bool ProjectModel::completeActiveProcess(QString* error)
{
    if (!ProcessVersionLifecycle::completeActiveProcess(&processVersionState_, error)) return false;
    notifyChanged();
    return true;
}

bool ProjectModel::certifyCurrentProcessIteration(QString* error)
{
    if (!ProcessVersionLifecycle::certifyCurrentIteration(&processVersionState_, error)) return false;
    notifyChanged();
    return true;
}

bool ProjectModel::resetForFiveStageProcessCampaign(QString* error)
{
    if (!ProcessVersionLifecycle::resetForFiveStageCampaign(&processVersionState_, error)) return false;
    notifyChanged();
    return true;
}

bool ProjectModel::restoreProcessVersionState(const ProcessVersionState& state, QString* error)
{
    if (!state.isValid(error)) return false;
    processVersionState_ = state;
    return true;
}
