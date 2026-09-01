#include "ProjectPersistence.h"

#include "ProjectModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace {
QJsonArray toJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const auto& value : values) array.append(value);
    return array;
}

QStringList fromJsonArray(const QJsonValue& value)
{
    QStringList values;
    for (const auto& entry : value.toArray()) values << entry.toString();
    return values;
}

QString normalizeAgentId(const QString& value)
{
    if (value == QStringLiteral("codex")) return QStringLiteral("openai-codex");
    return value;
}

QString normalizeTemplateId(const QString& value)
{
    if (value == QStringLiteral("android-kotlin-lite")) return QStringLiteral("android-studio-kotlin-gemini");
    return value;
}

QStringList migrateResponsibilities(const QStringList& values)
{
    const QHash<QString, QString> ids{
        {QStringLiteral("Planning"), QStringLiteral("planning")},
        {QStringLiteral("Coding"), QStringLiteral("coding")},
        {QStringLiteral("Review"), QStringLiteral("code-review")},
        {QStringLiteral("Testing"), QStringLiteral("testing")},
        {QStringLiteral("Documentation"), QStringLiteral("documentation")}
    };
    QStringList result;
    for (const auto& value : values) result << ids.value(value, value);
    return result;
}

QStringList migrateRuleCategories(const QStringList& values)
{
    const QHash<QString, QString> ids{
        {QStringLiteral("Project architecture"), QStringLiteral("architecture-boundaries")},
        {QStringLiteral("Testing and verification"), QStringLiteral("verification-before-completion")},
        {QStringLiteral("Universal safety"), QStringLiteral("destructive-command-protection")}
    };
    QStringList result;
    for (const auto& value : values) result << ids.value(value, value);
    return result;
}

QStringList migrateMemoryOptions(const QStringList& values)
{
    const QHash<QString, QString> ids{
        {QStringLiteral("Validate memory on project activation"), QStringLiteral("cold-start-validation")},
        {QStringLiteral("Validate consistency before generation"), QStringLiteral("memory-consistency")},
        {QStringLiteral("Record automation/offloading decisions"), QStringLiteral("record-decisions")},
        {QStringLiteral("Keep production and durable sequence numbers separate"), QStringLiteral("preserve-append-only")}
    };
    QStringList result;
    for (const auto& value : values) result << ids.value(value, value);
    return result;
}
}

bool ProjectPersistence::save(const ProjectModel& model, const QString& filePath, QString* error) const
{
    QJsonObject environment;
    const auto env = model.developmentEnvironment();
    environment.insert(QStringLiteral("language"), env.language);
    environment.insert(QStringLiteral("framework"), env.framework);
    environment.insert(QStringLiteral("ide"), env.ide);
    environment.insert(QStringLiteral("compiler"), env.compiler);
    environment.insert(QStringLiteral("operatingSystem"), env.operatingSystem);
    environment.insert(QStringLiteral("targetPlatform"), env.targetPlatform);
    environment.insert(QStringLiteral("targetArchitecture"), env.targetArchitecture);
    environment.insert(QStringLiteral("buildSystem"), env.buildSystem);
    environment.insert(QStringLiteral("packageManager"), env.packageManager);
    environment.insert(QStringLiteral("versionControl"), env.versionControl);

    const auto capabilities = model.developmentCapabilities();
    QJsonObject capabilityObject;
    capabilityObject.insert(QStringLiteral("languages"), toJsonArray(capabilities.languages));
    capabilityObject.insert(QStringLiteral("frameworks"), toJsonArray(capabilities.frameworks));
    capabilityObject.insert(QStringLiteral("ides"), toJsonArray(capabilities.ides));
    capabilityObject.insert(QStringLiteral("versionControlSystems"), toJsonArray(capabilities.versionControlSystems));
    capabilityObject.insert(QStringLiteral("developmentTools"), toJsonArray(capabilities.developmentTools));
    capabilityObject.insert(QStringLiteral("hostOperatingSystems"), toJsonArray(capabilities.hostOperatingSystems));
    capabilityObject.insert(QStringLiteral("targetPlatforms"), toJsonArray(capabilities.targetPlatforms));
    capabilityObject.insert(QStringLiteral("targetArchitectures"), toJsonArray(capabilities.targetArchitectures));
    capabilityObject.insert(QStringLiteral("processorFamilies"), toJsonArray(capabilities.processorFamilies));
    capabilityObject.insert(QStringLiteral("hardwareTargets"), toJsonArray(capabilities.hardwareTargets));
    capabilityObject.insert(QStringLiteral("toolchains"), toJsonArray(capabilities.toolchains));
    capabilityObject.insert(QStringLiteral("buildSystems"), toJsonArray(capabilities.buildSystems));
    capabilityObject.insert(QStringLiteral("dependencyManagers"), toJsonArray(capabilities.dependencyManagers));
    capabilityObject.insert(QStringLiteral("buildConfigurations"), toJsonArray(capabilities.buildConfigurations));
    capabilityObject.insert(QStringLiteral("testingCapabilities"), toJsonArray(capabilities.testingCapabilities));
    capabilityObject.insert(QStringLiteral("qualityCapabilities"), toJsonArray(capabilities.qualityCapabilities));
    capabilityObject.insert(QStringLiteral("automationCapabilities"), toJsonArray(capabilities.automationCapabilities));
    capabilityObject.insert(QStringLiteral("deliveryCapabilities"), toJsonArray(capabilities.deliveryCapabilities));

    QJsonObject options;
    for (auto it = model.options().cbegin(); it != model.options().cend(); ++it) {
        options.insert(it.key(), toJsonArray(it.value()));
    }

    QJsonObject root;
    root.insert(QStringLiteral("projectId"), model.projectId());
    root.insert(QStringLiteral("projectName"), model.projectName());
    root.insert(QStringLiteral("projectPath"), model.projectPath());
    root.insert(QStringLiteral("projectFilePath"), filePath);
    root.insert(QStringLiteral("description"), model.description());
    root.insert(QStringLiteral("templateId"), model.templateId());
    root.insert(QStringLiteral("context"), model.context());
    const auto ai = model.aiConfiguration();
    QJsonObject aiObject;
    aiObject.insert(QStringLiteral("primaryAgent"), ai.primaryAgent);
    aiObject.insert(QStringLiteral("additionalAgents"), toJsonArray(ai.additionalAgents));
    aiObject.insert(QStringLiteral("responsibilities"), toJsonArray(ai.responsibilities));
    aiObject.insert(QStringLiteral("permissions"), toJsonArray(ai.permissions));
    aiObject.insert(QStringLiteral("aramfIntegrations"), toJsonArray(ai.aramfIntegrations));
    aiObject.insert(QStringLiteral("customAgentName"), ai.customAgentName);
    aiObject.insert(QStringLiteral("autonomyPreset"), ai.autonomyPreset);
    root.insert(QStringLiteral("ai"), aiObject);
    const auto academic = model.academicConfiguration();
    QJsonObject academicObject;
    academicObject.insert(QStringLiteral("academicMode"), academic.academicMode);
    academicObject.insert(QStringLiteral("thesisLevel"), academic.thesisLevel);
    academicObject.insert(QStringLiteral("thesisApproaches"), toJsonArray(academic.thesisApproaches));
    academicObject.insert(QStringLiteral("researchMethods"), toJsonArray(academic.researchMethods));
    academicObject.insert(QStringLiteral("institution"), academic.institution);
    academicObject.insert(QStringLiteral("programmeOrCourse"), academic.programmeOrCourse);
    academicObject.insert(QStringLiteral("supervisor"), academic.supervisor);
    academicObject.insert(QStringLiteral("examiner"), academic.examiner);
    academicObject.insert(QStringLiteral("citationStyle"), academic.citationStyle);
    academicObject.insert(QStringLiteral("academicLanguage"), academic.academicLanguage);
    academicObject.insert(QStringLiteral("academicRequirements"), toJsonArray(academic.academicRequirements));
    academicObject.insert(QStringLiteral("academicDeliverables"), toJsonArray(academic.academicDeliverables));
    root.insert(QStringLiteral("academic"), academicObject);
    root.insert(QStringLiteral("environment"), environment);
    root.insert(QStringLiteral("capabilities"), capabilityObject);
    root.insert(QStringLiteral("aiPlatforms"), toJsonArray(model.aiPlatforms()));
    QJsonArray resourceArray;
    for (const auto& resource : model.resources()) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), resource.id);
        object.insert(QStringLiteral("name"), resource.name);
        object.insert(QStringLiteral("type"), resource.type);
        object.insert(QStringLiteral("location"), resource.location);
        object.insert(QStringLiteral("description"), resource.description);
        object.insert(QStringLiteral("enabled"), resource.enabled);
        object.insert(QStringLiteral("locationMode"), resource.locationMode);
        object.insert(QStringLiteral("authorityLevel"), resource.authorityLevel);
        object.insert(QStringLiteral("scopes"), toJsonArray(resource.scopes));
        object.insert(QStringLiteral("status"), resource.status);
        object.insert(QStringLiteral("loadingStrategyOverride"), resource.loadingStrategyOverride);
        object.insert(QStringLiteral("lastModified"), resource.lastModified);
        object.insert(QStringLiteral("fingerprint"), resource.fingerprint);
        resourceArray.append(object);
    }
    root.insert(QStringLiteral("resources"), resourceArray);
    const auto resourcePolicy = model.resourcePolicy();
    QJsonObject resourcePolicyObject;
    resourcePolicyObject.insert(QStringLiteral("options"), toJsonArray(resourcePolicy.options));
    resourcePolicyObject.insert(QStringLiteral("loadingStrategy"), resourcePolicy.loadingStrategy);
    root.insert(QStringLiteral("resourcePolicy"), resourcePolicyObject);
    const auto android = model.androidConstraints();
    QJsonObject androidObject{
        {QStringLiteral("minSdk"), android.minSdk}, {QStringLiteral("minSdkSource"), android.minSdkSource},
        {QStringLiteral("minSdkSpecified"), android.minSdkSpecified}, {QStringLiteral("courseName"), android.courseName}, {QStringLiteral("courseNameSource"), android.courseNameSource},
        {QStringLiteral("projectDomain"), android.projectDomain}, {QStringLiteral("projectDomainSource"), android.projectDomainSource}, {QStringLiteral("architecture"), android.architecture}, {QStringLiteral("architectureSource"), android.architectureSource},
        {QStringLiteral("declaredTechnologies"), toJsonArray(android.declaredTechnologies)}, {QStringLiteral("declaredTechnologiesSource"), android.declaredTechnologiesSource},
        {QStringLiteral("primaryIde"), android.primaryIde}, {QStringLiteral("primaryIdeSource"), android.primaryIdeSource},
        {QStringLiteral("kotlinRequired"), android.kotlinRequired}, {QStringLiteral("kotlinSource"), android.kotlinSource},
        {QStringLiteral("uiTechnology"), android.uiTechnology}, {QStringLiteral("xmlRequired"), android.xmlRequired},
        {QStringLiteral("uiTechnologySource"), android.uiTechnologySource}, {QStringLiteral("composeAvailable"), android.composeAvailable},
        {QStringLiteral("composeAllowed"), android.composeAllowed}, {QStringLiteral("composeSelected"), android.composeSelected},
        {QStringLiteral("composeSource"), android.composeSource}, {QStringLiteral("composeRequired"), android.composeRequired}, {QStringLiteral("composeRequiredSource"), android.composeRequiredSource},
        {QStringLiteral("retrofitRequired"), android.retrofitRequired}, {QStringLiteral("retrofitSource"), android.retrofitSource}, {QStringLiteral("gsonRequired"), android.gsonRequired}, {QStringLiteral("gsonSource"), android.gsonSource},
        {QStringLiteral("internetPermissionRequired"), android.internetPermissionRequired}, {QStringLiteral("internetPermissionSource"), android.internetPermissionSource}, {QStringLiteral("githubApiRequired"), android.githubApiRequired}, {QStringLiteral("githubApiSource"), android.githubApiSource},
        {QStringLiteral("githubPatRequired"), android.githubPatRequired}, {QStringLiteral("githubPatSource"), android.githubPatSource}, {QStringLiteral("classicPatRequired"), android.classicPatRequired}, {QStringLiteral("classicPatSource"), android.classicPatSource}, {QStringLiteral("patScope"), android.patScope}, {QStringLiteral("patScopeSource"), android.patScopeSource},
        {QStringLiteral("hardcodedTokenProhibited"), android.hardcodedTokenProhibited}, {QStringLiteral("hardcodedTokenSource"), android.hardcodedTokenSource}, {QStringLiteral("localPropertiesRequired"), android.localPropertiesRequired}, {QStringLiteral("localPropertiesSource"), android.localPropertiesSource}, {QStringLiteral("buildConfigRequired"), android.buildConfigRequired}, {QStringLiteral("buildConfigSource"), android.buildConfigSource},
        {QStringLiteral("privateRepositoryRequired"), android.privateRepositoryRequired}, {QStringLiteral("privateRepositorySource"), android.privateRepositorySource}, {QStringLiteral("instructorCollaboratorRequired"), android.instructorCollaboratorRequired}, {QStringLiteral("instructorCollaboratorSource"), android.instructorCollaboratorSource}, {QStringLiteral("repositoryName"), android.repositoryName}, {QStringLiteral("repositoryNameSource"), android.repositoryNameSource}, {QStringLiteral("initialBranch"), android.initialBranch}, {QStringLiteral("initialBranchSource"), android.initialBranchSource}, {QStringLiteral("requiredInitialFile"), android.requiredInitialFile}, {QStringLiteral("requiredInitialFileSource"), android.requiredInitialFileSource},
        {QStringLiteral("applicationStateSource"), android.applicationStateSource}, {QStringLiteral("applicationStateSourceSource"), android.applicationStateSourceSource}, {QStringLiteral("stateModel"), android.stateModel}, {QStringLiteral("stateModelSource"), android.stateModelSource}, {QStringLiteral("workflow"), android.workflow}, {QStringLiteral("workflowSource"), android.workflowSource}, {QStringLiteral("applicationStateFields"), toJsonArray(android.applicationStateFields)}, {QStringLiteral("applicationStateFieldsSource"), android.applicationStateFieldsSource},
        {QStringLiteral("domainAgents"), toJsonArray(android.domainAgents)}, {QStringLiteral("domainAgentsSource"), android.domainAgentsSource}, {QStringLiteral("domainAgentDistinction"), android.domainAgentDistinction}, {QStringLiteral("domainAgentDistinctionSource"), android.domainAgentDistinctionSource}, {QStringLiteral("pollingRequired"), android.pollingRequired}, {QStringLiteral("pollingSource"), android.pollingSource}, {QStringLiteral("pollingIntervalSeconds"), android.pollingIntervalSeconds}, {QStringLiteral("pollingIntervalSource"), android.pollingIntervalSource}, {QStringLiteral("pollingExecutionLocation"), android.pollingExecutionLocation}, {QStringLiteral("pollingExecutionLocationSource"), android.pollingExecutionLocationSource}, {QStringLiteral("networkDispatcher"), android.networkDispatcher}, {QStringLiteral("networkDispatcherSource"), android.networkDispatcherSource}, {QStringLiteral("analysisDispatcher"), android.analysisDispatcher}, {QStringLiteral("analysisDispatcherSource"), android.analysisDispatcherSource},
        {QStringLiteral("stateFlowRequired"), android.stateFlowRequired}, {QStringLiteral("stateFlowSource"), android.stateFlowSource}, {QStringLiteral("collectAsStateRequired"), android.collectAsStateRequired}, {QStringLiteral("collectAsStateSource"), android.collectAsStateSource}, {QStringLiteral("deceptionDetectorRequired"), android.deceptionDetectorRequired}, {QStringLiteral("deceptionDetectorSource"), android.deceptionDetectorSource}, {QStringLiteral("regexOrHeuristicsRequired"), android.regexOrHeuristicsRequired}, {QStringLiteral("regexOrHeuristicsSource"), android.regexOrHeuristicsSource}, {QStringLiteral("confidenceMinPercent"), android.confidenceMinPercent}, {QStringLiteral("confidenceMaxPercent"), android.confidenceMaxPercent}, {QStringLiteral("confidenceRangeSource"), android.confidenceRangeSource},
        {QStringLiteral("normalGreenRequired"), android.normalGreenRequired}, {QStringLiteral("normalGreenSource"), android.normalGreenSource}, {QStringLiteral("securityAlertRedRequired"), android.securityAlertRedRequired}, {QStringLiteral("securityAlertRedSource"), android.securityAlertRedSource}, {QStringLiteral("rawAdversarialTextRequired"), android.rawAdversarialTextRequired}, {QStringLiteral("rawAdversarialTextSource"), android.rawAdversarialTextSource}, {QStringLiteral("humanInTheLoopRequired"), android.humanInTheLoopRequired}, {QStringLiteral("humanInTheLoopSource"), android.humanInTheLoopSource},
        {QStringLiteral("validationRequirements"), toJsonArray(android.validationRequirements)}, {QStringLiteral("validationRequirementsSource"), android.validationRequirementsSource}, {QStringLiteral("submissionRequirements"), toJsonArray(android.submissionRequirements)}, {QStringLiteral("submissionRequirementsSource"), android.submissionRequirementsSource}, {QStringLiteral("submissionVideoDuration"), android.submissionVideoDuration}, {QStringLiteral("submissionVideoDurationSource"), android.submissionVideoDurationSource}, {QStringLiteral("submissionSegments"), toJsonArray(android.submissionSegments)}, {QStringLiteral("submissionSegmentsSource"), android.submissionSegmentsSource}, {QStringLiteral("unresolvedRequirements"), toJsonArray(android.unresolvedRequirements)}, {QStringLiteral("unresolvedRequirementsSource"), android.unresolvedRequirementsSource}, {QStringLiteral("sourceOfTruthTitle"), android.sourceOfTruthTitle}, {QStringLiteral("sourceOfTruthResource"), android.sourceOfTruthResource},
        {QStringLiteral("roomRequired"), android.roomRequired},
        {QStringLiteral("roomSource"), android.roomSource}, {QStringLiteral("unitTestsRequired"), android.unitTestsRequired},
        {QStringLiteral("unitTestsSource"), android.unitTestsSource}, {QStringLiteral("lintRequired"), android.lintRequired},
        {QStringLiteral("lintSource"), android.lintSource}};
    root.insert(QStringLiteral("androidConstraints"), androidObject);
    const auto rules = model.ruleConfiguration();
    QJsonObject rulesObject;
    rulesObject.insert(QStringLiteral("activeCategories"), toJsonArray(rules.activeCategories));
    rulesObject.insert(QStringLiteral("enforcementLevel"), rules.enforcementLevel);
    rulesObject.insert(QStringLiteral("loadingStrategy"), rules.loadingStrategy);
    rulesObject.insert(QStringLiteral("workScopes"), toJsonArray(rules.workScopes));
    rulesObject.insert(QStringLiteral("projectScopes"), toJsonArray(rules.projectScopes));
    rulesObject.insert(QStringLiteral("contextPolicies"), toJsonArray(rules.contextPolicies));
    rulesObject.insert(QStringLiteral("conflictPolicy"), rules.conflictPolicy);
    root.insert(QStringLiteral("rules"), rulesObject);
    const auto memory = model.memoryConfiguration();
    QJsonObject memoryObject;
    memoryObject.insert(QStringLiteral("writerMode"), memory.writerMode);
    memoryObject.insert(QStringLiteral("captureCategories"), toJsonArray(memory.captureCategories));
    memoryObject.insert(QStringLiteral("retentionLevel"), memory.retentionLevel);
    memoryObject.insert(QStringLiteral("maintenanceOptions"), toJsonArray(memory.maintenanceOptions));
    memoryObject.insert(QStringLiteral("validationOptions"), toJsonArray(memory.validationOptions));
    memoryObject.insert(QStringLiteral("updateStrategy"), memory.updateStrategy);
    memoryObject.insert(QStringLiteral("historyOptions"), toJsonArray(memory.historyOptions));
    memoryObject.insert(QStringLiteral("maximumSizeBytes"), memory.maximumSizeBytes);
    root.insert(QStringLiteral("memory"), memoryObject);
    const auto certification = model.certificationConfiguration();
    root.insert(QStringLiteral("certification"), QJsonObject{
        {QStringLiteral("enabled"), certification.enabled},
        {QStringLiteral("defaultVerificationLevel"), certification.defaultVerificationLevel}});

    const auto generation = model.generationOptions();
    root.insert(QStringLiteral("generationOptions"), QJsonObject{
        {QStringLiteral("agentRules"), generation.generateAgentRules},
        {QStringLiteral("routing"), generation.generateRouting},
        {QStringLiteral("platforms"), generation.generatePlatforms},
        {QStringLiteral("resources"), generation.generateResources},
        {QStringLiteral("memory"), generation.generateMemory},
        {QStringLiteral("provenance"), generation.generateProvenance}});
    root.insert(QStringLiteral("profileSelections"), toJsonArray(model.profileSelections()));
    root.insert(QStringLiteral("options"), options);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool ProjectPersistence::load(ProjectModel* model, const QString& filePath, QString* error) const
{
    if (!model) {
        if (error) *error = QStringLiteral("Project model is not available.");
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return false;
    }

    const auto root = document.object();
    AcademicConfiguration academic;
    const auto academicObject = root.value(QStringLiteral("academic")).toObject();
    if (!academicObject.isEmpty()) {
        academic.academicMode = academicObject.value(QStringLiteral("academicMode")).toString(QStringLiteral("disabled"));
        academic.thesisLevel = academicObject.value(QStringLiteral("thesisLevel")).toString();
        academic.thesisApproaches = fromJsonArray(academicObject.value(QStringLiteral("thesisApproaches")));
        academic.researchMethods = fromJsonArray(academicObject.value(QStringLiteral("researchMethods")));
        academic.institution = academicObject.value(QStringLiteral("institution")).toString();
        academic.programmeOrCourse = academicObject.value(QStringLiteral("programmeOrCourse")).toString();
        academic.supervisor = academicObject.value(QStringLiteral("supervisor")).toString();
        academic.examiner = academicObject.value(QStringLiteral("examiner")).toString();
        academic.citationStyle = academicObject.value(QStringLiteral("citationStyle")).toString();
        academic.academicLanguage = academicObject.value(QStringLiteral("academicLanguage")).toString();
        academic.academicRequirements = fromJsonArray(academicObject.value(QStringLiteral("academicRequirements")));
        academic.academicDeliverables = fromJsonArray(academicObject.value(QStringLiteral("academicDeliverables")));
    }
    AiConfiguration ai;
    const auto aiObject = root.value(QStringLiteral("ai")).toObject();
    if (!aiObject.isEmpty()) {
        ai.primaryAgent = normalizeAgentId(aiObject.value(QStringLiteral("primaryAgent")).toString(QStringLiteral("none")));
        for (const auto& value : fromJsonArray(aiObject.value(QStringLiteral("additionalAgents")))) {
            ai.additionalAgents << normalizeAgentId(value);
        }
        ai.responsibilities = fromJsonArray(aiObject.value(QStringLiteral("responsibilities")));
        ai.permissions = fromJsonArray(aiObject.value(QStringLiteral("permissions")));
        ai.aramfIntegrations = fromJsonArray(aiObject.value(QStringLiteral("aramfIntegrations")));
        ai.customAgentName = aiObject.value(QStringLiteral("customAgentName")).toString();
        ai.autonomyPreset = aiObject.value(QStringLiteral("autonomyPreset")).toString(QStringLiteral("custom"));
    } else {
        const QStringList legacyAgents = fromJsonArray(root.value(QStringLiteral("aiPlatforms")));
        if (!legacyAgents.isEmpty()) {
            ai.primaryAgent = normalizeAgentId(legacyAgents.first());
            for (const auto& value : legacyAgents.mid(1)) ai.additionalAgents << normalizeAgentId(value);
        }
        ai.responsibilities = migrateResponsibilities(
            fromJsonArray(root.value(QStringLiteral("options")).toObject().value(QStringLiteral("ai-working-mode"))));
        ai.aramfIntegrations = {QStringLiteral("agents-md"), QStringLiteral("project-memory"), QStringLiteral("rules"), QStringLiteral("routing")};
    }
    const auto environmentObject = root.value(QStringLiteral("environment")).toObject();
    DevelopmentEnvironment environment;
    environment.language = environmentObject.value(QStringLiteral("language")).toString();
    environment.framework = environmentObject.value(QStringLiteral("framework")).toString();
    environment.ide = environmentObject.value(QStringLiteral("ide")).toString();
    if (environment.ide == QStringLiteral("vscode")) {
        environment.ide = QStringLiteral("visual-studio-code");
    }
    environment.compiler = environmentObject.value(QStringLiteral("compiler")).toString();
    environment.operatingSystem = environmentObject.value(QStringLiteral("operatingSystem")).toString();
    environment.targetPlatform = environmentObject.value(QStringLiteral("targetPlatform")).toString();
    environment.targetArchitecture = environmentObject.value(QStringLiteral("targetArchitecture")).toString();
    if (environment.targetArchitecture.isEmpty()) {
        environment.targetArchitecture = QStringLiteral("auto");
    }
    environment.buildSystem = environmentObject.value(QStringLiteral("buildSystem")).toString();
    environment.packageManager = environmentObject.value(QStringLiteral("packageManager")).toString();
    environment.versionControl = environmentObject.value(QStringLiteral("versionControl")).toString();

    const auto capabilityObject = root.value(QStringLiteral("capabilities")).toObject();
    DevelopmentCapabilities capabilities;
    if (!capabilityObject.isEmpty()) {
        capabilities.languages = fromJsonArray(capabilityObject.value(QStringLiteral("languages")));
        capabilities.frameworks = fromJsonArray(capabilityObject.value(QStringLiteral("frameworks")));
        capabilities.ides = fromJsonArray(capabilityObject.value(QStringLiteral("ides")));
        capabilities.versionControlSystems = fromJsonArray(capabilityObject.value(QStringLiteral("versionControlSystems")));
        capabilities.developmentTools = fromJsonArray(capabilityObject.value(QStringLiteral("developmentTools")));
        capabilities.hostOperatingSystems = fromJsonArray(capabilityObject.value(QStringLiteral("hostOperatingSystems")));
        capabilities.targetPlatforms = fromJsonArray(capabilityObject.value(QStringLiteral("targetPlatforms")));
        capabilities.targetArchitectures = fromJsonArray(capabilityObject.value(QStringLiteral("targetArchitectures")));
        capabilities.processorFamilies = fromJsonArray(capabilityObject.value(QStringLiteral("processorFamilies")));
        capabilities.hardwareTargets = fromJsonArray(capabilityObject.value(QStringLiteral("hardwareTargets")));
        capabilities.toolchains = fromJsonArray(capabilityObject.value(QStringLiteral("toolchains")));
        capabilities.buildSystems = fromJsonArray(capabilityObject.value(QStringLiteral("buildSystems")));
        capabilities.dependencyManagers = fromJsonArray(capabilityObject.value(QStringLiteral("dependencyManagers")));
        capabilities.buildConfigurations = fromJsonArray(capabilityObject.value(QStringLiteral("buildConfigurations")));
        capabilities.testingCapabilities = fromJsonArray(capabilityObject.value(QStringLiteral("testingCapabilities")));
        capabilities.qualityCapabilities = fromJsonArray(capabilityObject.value(QStringLiteral("qualityCapabilities")));
        capabilities.automationCapabilities = fromJsonArray(capabilityObject.value(QStringLiteral("automationCapabilities")));
        capabilities.deliveryCapabilities = fromJsonArray(capabilityObject.value(QStringLiteral("deliveryCapabilities")));
    } else {
        if (!environment.language.isEmpty()) capabilities.languages = {environment.language};
        if (!environment.framework.isEmpty() && environment.framework != QStringLiteral("none")) capabilities.frameworks = {environment.framework};
        if (!environment.ide.isEmpty()) capabilities.ides = {environment.ide};
        if (!environment.versionControl.isEmpty() && environment.versionControl != QStringLiteral("none")) capabilities.versionControlSystems = {environment.versionControl};
        if (!environment.operatingSystem.isEmpty()) capabilities.hostOperatingSystems = {environment.operatingSystem};
        if (!environment.targetPlatform.isEmpty()) capabilities.targetPlatforms = {environment.targetPlatform};
        if (!environment.targetArchitecture.isEmpty()) capabilities.targetArchitectures = {environment.targetArchitecture};
        if (!environment.compiler.isEmpty()) capabilities.toolchains = {environment.compiler};
        if (!environment.buildSystem.isEmpty()) capabilities.buildSystems = {environment.buildSystem};
        capabilities.buildConfigurations = {QStringLiteral("debug"), QStringLiteral("release")};
    }

    model->beginUpdate();
    model->setProjectId(root.value(QStringLiteral("projectId")).toString());
    model->setProjectName(root.value(QStringLiteral("projectName")).toString());
    model->setProjectPath(root.value(QStringLiteral("projectPath")).toString());
    model->setProjectFilePath(filePath);
    model->setDescription(root.value(QStringLiteral("description")).toString());
    model->setTemplateId(normalizeTemplateId(root.value(QStringLiteral("templateId")).toString()));
    model->setContext(root.value(QStringLiteral("context")).toString());
    model->setDevelopmentCapabilities(capabilities);
    model->setDevelopmentEnvironment(environment);
    model->setAcademicConfiguration(academic);
    model->setAiConfiguration(ai);
    // Structured AI configuration is authoritative when present. The legacy
    // aiPlatforms mirror must not overwrite a persisted agent handoff.
    if (aiObject.isEmpty()) model->setAiPlatforms(fromJsonArray(root.value(QStringLiteral("aiPlatforms"))));
    RuleConfiguration rules;
    const auto rulesObject = root.value(QStringLiteral("rules")).toObject();
    rules.activeCategories = fromJsonArray(rulesObject.value(QStringLiteral("activeCategories")));
    rules.enforcementLevel = rulesObject.value(QStringLiteral("enforcementLevel")).toString(QStringLiteral("standard"));
    rules.loadingStrategy = rulesObject.value(QStringLiteral("loadingStrategy")).toString(QStringLiteral("relevant"));
    rules.workScopes = fromJsonArray(rulesObject.value(QStringLiteral("workScopes")));
    rules.projectScopes = fromJsonArray(rulesObject.value(QStringLiteral("projectScopes")));
    rules.contextPolicies = fromJsonArray(rulesObject.value(QStringLiteral("contextPolicies")));
    rules.conflictPolicy = rulesObject.value(QStringLiteral("conflictPolicy")).toString(QStringLiteral("prefer-user-instruction"));
    if (rules.activeCategories.isEmpty()) rules.activeCategories = migrateRuleCategories(fromJsonArray(root.value(QStringLiteral("options")).toObject().value(QStringLiteral("rules-routing"))));
    model->setRuleConfiguration(rules);
    MemoryConfiguration memory;
    const auto memoryObject = root.value(QStringLiteral("memory")).toObject();
    memory.writerMode = memoryObject.value(QStringLiteral("writerMode")).toString(QStringLiteral("agent-direct"));
    if (memory.writerMode != QStringLiteral("agent-direct")
        && memory.writerMode != QStringLiteral("project-local-tool")
        && memory.writerMode != QStringLiteral("disabled")) {
        memory.writerMode = QStringLiteral("agent-direct");
    }
    memory.captureCategories = fromJsonArray(memoryObject.value(QStringLiteral("captureCategories")));
    memory.retentionLevel = memoryObject.value(QStringLiteral("retentionLevel")).toString(QStringLiteral("standard"));
    memory.maintenanceOptions = fromJsonArray(memoryObject.value(QStringLiteral("maintenanceOptions")));
    memory.validationOptions = fromJsonArray(memoryObject.value(QStringLiteral("validationOptions")));
    memory.updateStrategy = memoryObject.value(QStringLiteral("updateStrategy")).toString(QStringLiteral("meaningful-task"));
    memory.historyOptions = fromJsonArray(memoryObject.value(QStringLiteral("historyOptions")));
    memory.maximumSizeBytes = memoryObject.value(QStringLiteral("maximumSizeBytes")).toVariant().toLongLong();
    if (memory.maximumSizeBytes <= 0) memory.maximumSizeBytes = 10LL * 1024LL * 1024LL * 1024LL;
    if (memory.captureCategories.isEmpty()) memory.captureCategories = migrateMemoryOptions(fromJsonArray(root.value(QStringLiteral("options")).toObject().value(QStringLiteral("memory-policy"))));
    model->setMemoryConfiguration(memory);
    CertificationConfiguration certification;
    const auto certificationObject = root.value(QStringLiteral("certification")).toObject();
    certification.enabled = certificationObject.value(QStringLiteral("enabled")).toBool(false);
    certification.defaultVerificationLevel = certificationObject.value(QStringLiteral("defaultVerificationLevel"))
        .toString(QStringLiteral("HOST_TEST"));
    model->setCertificationConfiguration(certification);

    const auto generationObject = root.value(QStringLiteral("generationOptions")).toObject();
    if (!generationObject.isEmpty()) {
        GenerationOptions generation;
        generation.generateAgentRules = generationObject.value(QStringLiteral("agentRules")).toBool(true);
        generation.generateRouting = generationObject.value(QStringLiteral("routing")).toBool(true);
        generation.generatePlatforms = generationObject.value(QStringLiteral("platforms")).toBool(true);
        generation.generateResources = generationObject.value(QStringLiteral("resources")).toBool(true);
        generation.generateMemory = generationObject.value(QStringLiteral("memory")).toBool(true);
        generation.generateProvenance = generationObject.value(QStringLiteral("provenance")).toBool(true);
        model->setGenerationOptions(generation);
    }
    QList<ProjectResource> resources;
    const auto resourceValue = root.value(QStringLiteral("resources"));
    const auto resourceArray = resourceValue.toArray();
    for (const auto& entry : resourceArray) {
        if (entry.isString()) {
            ProjectResource resource;
            resource.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            resource.name = entry.toString();
            resource.type = QStringLiteral("other");
            resource.authorityLevel = QStringLiteral("supporting-reference");
            resources.append(resource);
            continue;
        }
        const auto object = entry.toObject();
        ProjectResource resource;
        resource.id = object.value(QStringLiteral("id")).toString();
        if (resource.id.isEmpty()) resource.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        resource.name = object.value(QStringLiteral("name")).toString();
        resource.type = object.value(QStringLiteral("type")).toString(QStringLiteral("other"));
        resource.location = object.value(QStringLiteral("location")).toString();
        resource.description = object.value(QStringLiteral("description")).toString();
        resource.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        resource.locationMode = object.value(QStringLiteral("locationMode")).toString(QStringLiteral("referenced"));
        resource.authorityLevel = object.value(QStringLiteral("authorityLevel")).toString();
        if (resource.authorityLevel.isEmpty()) {
            const auto legacySourceOfTruth = object.value(QStringLiteral("sourceOfTruth"));
            resource.authorityLevel = legacySourceOfTruth.toBool(false)
                ? QStringLiteral("primary-source-of-truth")
                : QStringLiteral("supporting-reference");
        }
        resource.scopes = fromJsonArray(object.value(QStringLiteral("scopes")));
        resource.status = object.value(QStringLiteral("status")).toString(QStringLiteral("unknown"));
        resource.loadingStrategyOverride = object.value(QStringLiteral("loadingStrategyOverride")).toString();
        resource.lastModified = object.value(QStringLiteral("lastModified")).toString();
        resource.fingerprint = object.value(QStringLiteral("fingerprint")).toString();
        resources.append(resource);
    }
    if (resourceValue.isArray() && resourceArray.isEmpty()) {
        model->setResources({});
    } else if (resources.isEmpty()) {
        model->setResourceNames(fromJsonArray(resourceValue));
    } else {
        model->setResources(resources);
    }
    const auto androidObject = root.value(QStringLiteral("androidConstraints")).toObject();
    if (!androidObject.isEmpty()) {
        AndroidProjectConstraints android;
        android.minSdk = androidObject.value(QStringLiteral("minSdk")).toInt(0);
        android.minSdkSpecified = androidObject.value(QStringLiteral("minSdkSpecified")).toBool(android.minSdk != 0);
        android.minSdkSource = androidObject.value(QStringLiteral("minSdkSource")).toString();
        android.courseName = androidObject.value(QStringLiteral("courseName")).toString(); android.courseNameSource = androidObject.value(QStringLiteral("courseNameSource")).toString();
        android.projectDomain = androidObject.value(QStringLiteral("projectDomain")).toString(); android.projectDomainSource = androidObject.value(QStringLiteral("projectDomainSource")).toString(); android.architecture = androidObject.value(QStringLiteral("architecture")).toString(); android.architectureSource = androidObject.value(QStringLiteral("architectureSource")).toString();
        android.declaredTechnologies = fromJsonArray(androidObject.value(QStringLiteral("declaredTechnologies"))); android.declaredTechnologiesSource = androidObject.value(QStringLiteral("declaredTechnologiesSource")).toString();
        android.primaryIde = androidObject.value(QStringLiteral("primaryIde")).toString(QStringLiteral("android-studio"));
        android.primaryIdeSource = androidObject.value(QStringLiteral("primaryIdeSource")).toString();
        android.kotlinRequired = androidObject.value(QStringLiteral("kotlinRequired")).toBool(false);
        android.kotlinSource = androidObject.value(QStringLiteral("kotlinSource")).toString();
        android.uiTechnology = androidObject.value(QStringLiteral("uiTechnology")).toString(QStringLiteral("compose"));
        android.xmlRequired = androidObject.value(QStringLiteral("xmlRequired")).toBool(false);
        android.uiTechnologySource = androidObject.value(QStringLiteral("uiTechnologySource")).toString();
        android.composeAvailable = androidObject.value(QStringLiteral("composeAvailable")).toBool(true);
        android.composeAllowed = androidObject.value(QStringLiteral("composeAllowed")).toBool(true);
        android.composeSelected = androidObject.value(QStringLiteral("composeSelected")).toBool(true);
        android.composeSource = androidObject.value(QStringLiteral("composeSource")).toString();
        android.composeRequired = androidObject.value(QStringLiteral("composeRequired")).toBool(false); android.composeRequiredSource = androidObject.value(QStringLiteral("composeRequiredSource")).toString();
        android.retrofitRequired = androidObject.value(QStringLiteral("retrofitRequired")).toBool(false); android.retrofitSource = androidObject.value(QStringLiteral("retrofitSource")).toString(); android.gsonRequired = androidObject.value(QStringLiteral("gsonRequired")).toBool(false); android.gsonSource = androidObject.value(QStringLiteral("gsonSource")).toString();
        android.internetPermissionRequired = androidObject.value(QStringLiteral("internetPermissionRequired")).toBool(false); android.internetPermissionSource = androidObject.value(QStringLiteral("internetPermissionSource")).toString(); android.githubApiRequired = androidObject.value(QStringLiteral("githubApiRequired")).toBool(false); android.githubApiSource = androidObject.value(QStringLiteral("githubApiSource")).toString();
        android.githubPatRequired = androidObject.value(QStringLiteral("githubPatRequired")).toBool(false); android.githubPatSource = androidObject.value(QStringLiteral("githubPatSource")).toString(); android.classicPatRequired = androidObject.value(QStringLiteral("classicPatRequired")).toBool(false); android.classicPatSource = androidObject.value(QStringLiteral("classicPatSource")).toString(); android.patScope = androidObject.value(QStringLiteral("patScope")).toString(); android.patScopeSource = androidObject.value(QStringLiteral("patScopeSource")).toString();
        android.hardcodedTokenProhibited = androidObject.value(QStringLiteral("hardcodedTokenProhibited")).toBool(false); android.hardcodedTokenSource = androidObject.value(QStringLiteral("hardcodedTokenSource")).toString(); android.localPropertiesRequired = androidObject.value(QStringLiteral("localPropertiesRequired")).toBool(false); android.localPropertiesSource = androidObject.value(QStringLiteral("localPropertiesSource")).toString(); android.buildConfigRequired = androidObject.value(QStringLiteral("buildConfigRequired")).toBool(false); android.buildConfigSource = androidObject.value(QStringLiteral("buildConfigSource")).toString();
        android.privateRepositoryRequired = androidObject.value(QStringLiteral("privateRepositoryRequired")).toBool(false); android.privateRepositorySource = androidObject.value(QStringLiteral("privateRepositorySource")).toString(); android.instructorCollaboratorRequired = androidObject.value(QStringLiteral("instructorCollaboratorRequired")).toBool(false); android.instructorCollaboratorSource = androidObject.value(QStringLiteral("instructorCollaboratorSource")).toString(); android.repositoryName = androidObject.value(QStringLiteral("repositoryName")).toString(); android.repositoryNameSource = androidObject.value(QStringLiteral("repositoryNameSource")).toString(); android.initialBranch = androidObject.value(QStringLiteral("initialBranch")).toString(); android.initialBranchSource = androidObject.value(QStringLiteral("initialBranchSource")).toString(); android.requiredInitialFile = androidObject.value(QStringLiteral("requiredInitialFile")).toString(); android.requiredInitialFileSource = androidObject.value(QStringLiteral("requiredInitialFileSource")).toString();
        android.applicationStateSource = androidObject.value(QStringLiteral("applicationStateSource")).toString(); android.applicationStateSourceSource = androidObject.value(QStringLiteral("applicationStateSourceSource")).toString(); android.stateModel = androidObject.value(QStringLiteral("stateModel")).toString(); android.stateModelSource = androidObject.value(QStringLiteral("stateModelSource")).toString(); android.workflow = androidObject.value(QStringLiteral("workflow")).toString(); android.workflowSource = androidObject.value(QStringLiteral("workflowSource")).toString(); android.applicationStateFields = fromJsonArray(androidObject.value(QStringLiteral("applicationStateFields"))); android.applicationStateFieldsSource = androidObject.value(QStringLiteral("applicationStateFieldsSource")).toString();
        android.domainAgents = fromJsonArray(androidObject.value(QStringLiteral("domainAgents"))); android.domainAgentsSource = androidObject.value(QStringLiteral("domainAgentsSource")).toString(); android.domainAgentDistinction = androidObject.value(QStringLiteral("domainAgentDistinction")).toString(); android.domainAgentDistinctionSource = androidObject.value(QStringLiteral("domainAgentDistinctionSource")).toString(); android.pollingRequired = androidObject.value(QStringLiteral("pollingRequired")).toBool(false); android.pollingSource = androidObject.value(QStringLiteral("pollingSource")).toString(); android.pollingIntervalSeconds = androidObject.value(QStringLiteral("pollingIntervalSeconds")).toInt(0); android.pollingIntervalSource = androidObject.value(QStringLiteral("pollingIntervalSource")).toString(); android.pollingExecutionLocation = androidObject.value(QStringLiteral("pollingExecutionLocation")).toString(); android.pollingExecutionLocationSource = androidObject.value(QStringLiteral("pollingExecutionLocationSource")).toString(); android.networkDispatcher = androidObject.value(QStringLiteral("networkDispatcher")).toString(); android.networkDispatcherSource = androidObject.value(QStringLiteral("networkDispatcherSource")).toString(); android.analysisDispatcher = androidObject.value(QStringLiteral("analysisDispatcher")).toString(); android.analysisDispatcherSource = androidObject.value(QStringLiteral("analysisDispatcherSource")).toString();
        android.stateFlowRequired = androidObject.value(QStringLiteral("stateFlowRequired")).toBool(false); android.stateFlowSource = androidObject.value(QStringLiteral("stateFlowSource")).toString(); android.collectAsStateRequired = androidObject.value(QStringLiteral("collectAsStateRequired")).toBool(false); android.collectAsStateSource = androidObject.value(QStringLiteral("collectAsStateSource")).toString(); android.deceptionDetectorRequired = androidObject.value(QStringLiteral("deceptionDetectorRequired")).toBool(false); android.deceptionDetectorSource = androidObject.value(QStringLiteral("deceptionDetectorSource")).toString(); android.regexOrHeuristicsRequired = androidObject.value(QStringLiteral("regexOrHeuristicsRequired")).toBool(false); android.regexOrHeuristicsSource = androidObject.value(QStringLiteral("regexOrHeuristicsSource")).toString(); android.confidenceMinPercent = androidObject.value(QStringLiteral("confidenceMinPercent")).toInt(-1); android.confidenceMaxPercent = androidObject.value(QStringLiteral("confidenceMaxPercent")).toInt(-1); android.confidenceRangeSource = androidObject.value(QStringLiteral("confidenceRangeSource")).toString();
        android.normalGreenRequired = androidObject.value(QStringLiteral("normalGreenRequired")).toBool(false); android.normalGreenSource = androidObject.value(QStringLiteral("normalGreenSource")).toString(); android.securityAlertRedRequired = androidObject.value(QStringLiteral("securityAlertRedRequired")).toBool(false); android.securityAlertRedSource = androidObject.value(QStringLiteral("securityAlertRedSource")).toString(); android.rawAdversarialTextRequired = androidObject.value(QStringLiteral("rawAdversarialTextRequired")).toBool(false); android.rawAdversarialTextSource = androidObject.value(QStringLiteral("rawAdversarialTextSource")).toString(); android.humanInTheLoopRequired = androidObject.value(QStringLiteral("humanInTheLoopRequired")).toBool(false); android.humanInTheLoopSource = androidObject.value(QStringLiteral("humanInTheLoopSource")).toString();
        android.validationRequirements = fromJsonArray(androidObject.value(QStringLiteral("validationRequirements"))); android.validationRequirementsSource = androidObject.value(QStringLiteral("validationRequirementsSource")).toString(); android.submissionRequirements = fromJsonArray(androidObject.value(QStringLiteral("submissionRequirements"))); android.submissionRequirementsSource = androidObject.value(QStringLiteral("submissionRequirementsSource")).toString(); android.submissionVideoDuration = androidObject.value(QStringLiteral("submissionVideoDuration")).toString(); android.submissionVideoDurationSource = androidObject.value(QStringLiteral("submissionVideoDurationSource")).toString(); android.submissionSegments = fromJsonArray(androidObject.value(QStringLiteral("submissionSegments"))); android.submissionSegmentsSource = androidObject.value(QStringLiteral("submissionSegmentsSource")).toString(); android.unresolvedRequirements = fromJsonArray(androidObject.value(QStringLiteral("unresolvedRequirements"))); android.unresolvedRequirementsSource = androidObject.value(QStringLiteral("unresolvedRequirementsSource")).toString(); android.sourceOfTruthTitle = androidObject.value(QStringLiteral("sourceOfTruthTitle")).toString(); android.sourceOfTruthResource = androidObject.value(QStringLiteral("sourceOfTruthResource")).toString();
        android.roomRequired = androidObject.value(QStringLiteral("roomRequired")).toBool(false);
        android.roomSource = androidObject.value(QStringLiteral("roomSource")).toString();
        android.unitTestsRequired = androidObject.value(QStringLiteral("unitTestsRequired")).toBool(false);
        android.unitTestsSource = androidObject.value(QStringLiteral("unitTestsSource")).toString();
        android.lintRequired = androidObject.value(QStringLiteral("lintRequired")).toBool(false);
        android.lintSource = androidObject.value(QStringLiteral("lintSource")).toString();
        model->setAndroidConstraints(android);
    }
    ResourcePolicy resourcePolicy;
    const auto resourcePolicyObject = root.value(QStringLiteral("resourcePolicy")).toObject();
    resourcePolicy.options = fromJsonArray(resourcePolicyObject.value(QStringLiteral("options")));
    if (resourcePolicy.options.isEmpty()) {
        resourcePolicy.options = {QStringLiteral("read-relevant"), QStringLiteral("prefer-authoritative"), QStringLiteral("respect-scope"), QStringLiteral("ignore-disabled"), QStringLiteral("warn-conflicts")};
    }
    resourcePolicy.loadingStrategy = resourcePolicyObject.value(QStringLiteral("loadingStrategy")).toString(QStringLiteral("relevant"));
    model->setResourcePolicy(resourcePolicy);
    model->setProfileSelections(fromJsonArray(root.value(QStringLiteral("profileSelections"))));
    const auto optionsObject = root.value(QStringLiteral("options")).toObject();
    for (auto it = optionsObject.constBegin(); it != optionsObject.constEnd(); ++it) {
        model->setOptionValues(it.key(), fromJsonArray(it.value()));
    }
    model->endUpdate();
    model->setModified(false);
    return true;
}
