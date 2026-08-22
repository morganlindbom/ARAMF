#include "ProjectModel.h"

#include <QUuid>
#include <QSet>

namespace {

bool containsAny(const QStringList& values, std::initializer_list<QStringView> candidates)
{
    for (const auto candidate : candidates) {
        if (values.contains(candidate.toString())) {
            return true;
        }
    }
    return false;
}

QString deriveProjectContext(const DevelopmentCapabilities& capabilities)
{
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
    const QString derivedContext = deriveProjectContext(value);
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
    DevelopmentEnvironment next = environment_;
    next.language = value.languages.isEmpty() ? QString() : value.languages.first();
    next.framework = value.frameworks.isEmpty() ? QString() : value.frameworks.first();
    next.ide = value.ides.isEmpty() ? QString() : value.ides.first();
    next.compiler = value.toolchains.isEmpty() ? QString() : value.toolchains.first();
    next.operatingSystem = value.hostOperatingSystems.isEmpty() ? QString() : value.hostOperatingSystems.first();
    next.targetPlatform = value.targetPlatforms.isEmpty() ? QString() : value.targetPlatforms.first();
    next.targetArchitecture = value.targetArchitectures.isEmpty() ? QString() : value.targetArchitectures.first();
    next.buildSystem = value.buildSystems.isEmpty() ? QString() : value.buildSystems.first();
    if (next.language != environment_.language || next.framework != environment_.framework
        || next.ide != environment_.ide || next.compiler != environment_.compiler
        || next.operatingSystem != environment_.operatingSystem || next.targetPlatform != environment_.targetPlatform
        || next.targetArchitecture != environment_.targetArchitecture || next.buildSystem != environment_.buildSystem) {
        environment_ = next;
        emit developmentEnvironmentChanged();
    }
    emit developmentCapabilitiesChanged();
    notifyChanged();
}

void ProjectModel::setAcademicConfiguration(const AcademicConfiguration& value)
{
    const bool unchanged = academic_.academicMode == value.academicMode
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
        && academic_.academicDeliverables == value.academicDeliverables;
    if (unchanged) {
        return;
    }
    academic_ = value;
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
                || a.authorityLevel != b.authorityLevel || a.scopes != b.scopes || a.status != b.status
                || a.loadingStrategyOverride != b.loadingStrategyOverride || a.lastModified != b.lastModified
                || a.fingerprint != b.fingerprint) {
                unchanged = false;
                break;
            }
        }
        if (unchanged) return;
    }
    resources_ = value;
    resourceNames_.clear();
    for (const auto& resource : resources_) resourceNames_ << resource.name;
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
    if (ruleConfiguration_.activeCategories == value.activeCategories
        && ruleConfiguration_.enforcementLevel == value.enforcementLevel
        && ruleConfiguration_.loadingStrategy == value.loadingStrategy
        && ruleConfiguration_.workScopes == value.workScopes
        && ruleConfiguration_.projectScopes == value.projectScopes
        && ruleConfiguration_.contextPolicies == value.contextPolicies
        && ruleConfiguration_.conflictPolicy == value.conflictPolicy) return;
    ruleConfiguration_ = value;
    notifyChanged();
}

void ProjectModel::setMemoryConfiguration(const MemoryConfiguration& value)
{
    if (memoryConfiguration_.captureCategories == value.captureCategories
        && memoryConfiguration_.retentionLevel == value.retentionLevel
        && memoryConfiguration_.maintenanceOptions == value.maintenanceOptions
        && memoryConfiguration_.validationOptions == value.validationOptions
        && memoryConfiguration_.updateStrategy == value.updateStrategy
        && memoryConfiguration_.historyOptions == value.historyOptions
        && memoryConfiguration_.maximumSizeBytes == value.maximumSizeBytes) return;
    memoryConfiguration_ = value;
    notifyChanged();
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

void ProjectModel::resetForNewProject()
{
    beginUpdate();
    projectId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    projectName_ = QStringLiteral("New AR&MF Project");
    projectPath_.clear();
    projectFilePath_.clear();
    description_.clear();
    templateId_.clear();
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
    profileSelections_.clear();
    ai_ = {};
    options_.clear();
    environmentOverrides_.clear();
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
