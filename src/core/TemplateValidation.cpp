#include "TemplateValidation.h"
#include "Services.h"
#include "AiCatalog.h"
#include "RuleCatalog.h"
#include "MemoryCatalog.h"
#include "ProjectPersistence.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QSet>

namespace {
QList<EnvironmentOption> aiOptions(const QList<AiOption>& options)
{
    QList<EnvironmentOption> result;
    for (const auto& option : options) result << EnvironmentOption{option.displayName, option.id};
    return result;
}
QList<EnvironmentOption> choices(std::initializer_list<const char*> values)
{
    QList<EnvironmentOption> result;
    for (const auto* value : values) result << EnvironmentOption{value, value};
    return result;
}
QJsonValue at(const QJsonObject& root, const QString& path)
{
    QJsonValue value(root);
    for (const auto& part : path.split('.')) value = value.toObject().value(part);
    return value;
}
QStringList strings(const QJsonValue& value)
{
    QStringList result;
    if (value.isString() && !value.toString().isEmpty()) result << value.toString();
    else for (const auto& item : value.toArray()) result << item.toString();
    return result;
}
void shape(const QJsonObject& object, const QString& prefix, QStringList& paths)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QString key = prefix + it.key();
        paths << key;
        if (it.value().isObject()) shape(it.value().toObject(), key + '.', paths);
    }
}
}

QMap<QString, QList<EnvironmentOption>> TemplateValidation::catalogs()
{
    using namespace EnvironmentCatalog;
    return {
        {"capabilities.languages", languages()}, {"capabilities.frameworks", frameworks()},
        {"capabilities.ides", ides()}, {"capabilities.versionControlSystems", versionControlSystems()},
        {"capabilities.developmentTools", developmentSupport()}, {"capabilities.hostOperatingSystems", operatingSystems()},
        {"capabilities.targetPlatforms", targets()}, {"capabilities.targetArchitectures", architectures()},
        {"capabilities.processorFamilies", processorFamilies()}, {"capabilities.hardwareTargets", hardwareTargets()},
        {"capabilities.toolchains", toolchains()}, {"capabilities.buildSystems", buildSystems()},
        {"capabilities.dependencyManagers", dependencyManagers()}, {"capabilities.buildConfigurations", buildConfigurations()},
        {"capabilities.testingCapabilities", testingCapabilities()}, {"capabilities.qualityCapabilities", qualityCapabilities()},
        {"capabilities.automationCapabilities", automationCapabilities()}, {"capabilities.deliveryCapabilities", deliveryCapabilities()},
        {"academic.academicMode", academicModes()}, {"academic.thesisLevel", thesisLevels()},
        {"academic.thesisApproaches", thesisApproaches()}, {"academic.researchMethods", researchMethods()},
        {"academic.citationStyle", citationStyles()}, {"academic.academicLanguage", academicLanguages()},
        {"academic.academicRequirements", academicRequirements()}, {"academic.academicDeliverables", academicDeliverables()},
        {"ai.primaryAgent", aiOptions(AiCatalog::agents()) + choices({"none"})},
        {"ai.additionalAgents", aiOptions(AiCatalog::agents())}, {"ai.responsibilities", aiOptions(AiCatalog::responsibilities())},
        {"ai.permissions", aiOptions(AiCatalog::permissions())}, {"ai.aramfIntegrations", aiOptions(AiCatalog::integrations())},
        {"rules.activeCategories", RuleCatalog::categories()}, {"rules.workScopes", RuleCatalog::workScopes()},
        {"rules.projectScopes", RuleCatalog::projectScopes()}, {"rules.contextPolicies", RuleCatalog::contextPolicies()},
        {"rules.enforcementLevel", choices({"advisory", "standard", "strict"})},
        {"rules.loadingStrategy", choices({"relevant", "always", "explicit", "metadata-first"})},
        {"rules.conflictPolicy", choices({"prefer-user-instruction", "prefer-source-of-truth", "prefer-durable-decision", "prefer-project-rule", "manual-resolution"})},
        {"memory.captureCategories", MemoryCatalog::captureCategories()}, {"memory.maintenanceOptions", MemoryCatalog::maintenanceOptions()},
        {"memory.validationOptions", MemoryCatalog::validationOptions()}, {"memory.historyOptions", MemoryCatalog::historyOptions()},
        {"memory.writerMode", choices({"agent-direct", "project-local-tool", "disabled"})},
        {"memory.retentionLevel", choices({"minimal", "standard", "detailed", "audit"})},
        {"memory.updateStrategy", choices({"meaningful-task", "checkpoint", "before-milestone", "project-close", "manual"})},
        {"resourcePolicy.options", resourcePolicyOptions()}, {"resourcePolicy.loadingStrategy", resourceLoadingStrategies()},
        {"resource.type", resourceTypes()}, {"resource.authorityLevel", authorityLevels()}, {"resource.scopes", resourceScopes()},
        {"resource.loadingStrategyOverride", resourceLoadingStrategies()},
        {"resource.locationMode", choices({"referenced", "project-local-copy"})}
    };
}

QString TemplateValidation::catalogFingerprint()
{
    QJsonObject catalog;
    const auto domains = catalogs();
    for (auto it = domains.begin(); it != domains.end(); ++it) {
        QStringList options;
        for (const auto& option : it.value()) options << option.second;
        options.sort(); catalog.insert(it.key(), QJsonArray::fromStringList(options));
    }
    ProjectModel model;
    QStringList paths;
    shape(ProjectPersistence().configuration(model), {}, paths);
    // Includes free text, booleans, legacy extension state and derived Android fields.
    catalog.insert("configurationFields", QJsonArray::fromStringList(paths));
    return QCryptographicHash::hash(QJsonDocument(catalog).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex();
}

QStringList TemplateValidation::validateConfiguration(const QJsonObject& config)
{
    QStringList errors;
    ProjectModel empty;
    const auto expected = ProjectPersistence().configuration(empty);
    for (auto it = expected.begin(); it != expected.end(); ++it) {
        if (!config.contains(it.key()) || config.value(it.key()).type() != it.value().type()) errors << "Missing or invalid configuration domain: " + it.key();
    }
    const auto domains = catalogs();
    auto check = [&](const QString& path, const QJsonValue& value, const QList<EnvironmentOption>& catalog) {
        QSet<QString> allowed, selected;
        for (const auto& option : catalog) allowed.insert(option.second);
        for (const auto& id : strings(value)) {
            if (!allowed.contains(id)) errors << "Unavailable option: " + path + '/' + id;
            if (selected.contains(id)) errors << "Duplicate option: " + path + '/' + id;
            selected.insert(id);
        }
        for (const auto& exclusive : {"none", "not-applicable", "auto", "cross-platform", "multi-architecture"})
            if (selected.contains(exclusive) && selected.size() > 1) errors << "Conflicting selections: " + path + '/' + exclusive;
    };
    for (auto it = domains.begin(); it != domains.end(); ++it) {
        // Resource records are user-owned and may use project-specific types
        // (for example markdown or a course handout). Their structural fields
        // are preserved; catalog validation applies to template capability
        // selections and resource policy, not arbitrary inventory metadata.
        if (!it.key().startsWith("resource.")) check(it.key(), at(config, it.key()), it.value());
    }
    auto has = [&](const QString& path, const QString& id) { return strings(at(config, path)).contains(id); };
    auto require = [&](bool condition, const QString& reason) { if (!condition) errors << reason; };
    for (const auto& domain : {"languages", "ides", "hostOperatingSystems", "targetPlatforms", "targetArchitectures", "toolchains", "buildSystems", "buildConfigurations"})
        require(!strings(at(config, "capabilities." + QString(domain))).isEmpty(), "Configure " + QString(domain));
    // Each selected framework declares its required language/toolchain rather than assuming one global stack.
    const QList<QStringList> dependencies{
        {"react", "languages", "typescript|javascript"}, {"react", "toolchains", "nodejs"},
        {"express", "toolchains", "nodejs"}, {"nodejs", "toolchains", "nodejs"},
        {"fastapi", "languages", "python"}, {"fastapi", "toolchains", "python"},
        {"aspnet", "languages", "csharp"}, {"aspnet", "toolchains", "dotnet-sdk"},
        {"dotnet", "toolchains", "dotnet-sdk"}, {"qt", "languages", "cpp|python"},
        {"pico-sdk", "languages", "cpp|c"}, {"pico-sdk", "toolchains", "arm-gnu|arm-clang|pico-sdk-toolchain|riscv-gnu"},
        {"pico-sdk", "buildSystems", "cmake|pico-sdk-cmake"},
        {"pico-sdk", "targetPlatforms", "microcontroller|bare-metal|embedded-system"},
        {"jetpack-compose", "frameworks", "android-sdk"}, {"jetpack-compose", "languages", "kotlin"},
        {"room", "frameworks", "android-sdk"}, {"android-emulator", "frameworks", "android-sdk"},
        {"android-device-testing", "frameworks", "android-sdk"}, {"android-sdk", "targetPlatforms", "android"},
        {"android-sdk", "toolchains", "java-jdk"}, {"android-sdk", "buildSystems", "gradle"}
    };
    for (const auto& rule : dependencies) if (has("capabilities.frameworks", rule[0])) {
        bool found = false;
        for (const auto& alternative : rule[2].split('|')) found |= has("capabilities." + rule[1], alternative);
        require(found, rule[0] + " requires " + rule[1] + ": " + rule[2]);
    }
    if (has("capabilities.targetPlatforms", "android")) {
        require(has("capabilities.frameworks", "android-sdk"), "Android requires Android SDK");
        require(has("capabilities.dependencyManagers", "gradle"), "Android requires Gradle dependency management");
    }
    if (has("capabilities.languages", "pio-assembly")) require(has("capabilities.frameworks", "pico-sdk"), "PIO Assembly requires Pico SDK");
    if (has("rules.activeCategories", "cmake-rules")) require(has("capabilities.buildSystems", "cmake") || has("capabilities.buildSystems", "pico-sdk-cmake"), "CMake rules require CMake");
    if (has("capabilities.frameworks", "aspnet")) require(has("capabilities.buildSystems", "msbuild") || has("capabilities.buildSystems", "visual-studio-build"), "ASP.NET requires MSBuild");
    if (has("capabilities.hardwareTargets", "raspberry-pi-pico-2-w")) require(has("capabilities.processorFamilies", "rp2350"), "Pico 2 W requires RP2350");
    if (has("capabilities.hardwareTargets", "raspberry-pi-pico")) require(has("capabilities.processorFamilies", "rp2040"), "Pico requires RP2040");
    if (has("academic.academicMode", "disabled")) {
        for (const auto& path : {"academic.thesisLevel", "academic.thesisApproaches", "academic.researchMethods", "academic.academicRequirements", "academic.academicDeliverables"})
            require(strings(at(config, path)).isEmpty(), "Disabled academic mode has active selections: " + QString(path));
        require(!has("rules.activeCategories", "academic-documentation"), "Academic documentation requires academic mode");
    }
    const bool agent = !has("ai.primaryAgent", "none") || !strings(at(config, "ai.additionalAgents")).isEmpty();
    if (!agent) {
        require(strings(at(config, "ai.responsibilities")).isEmpty(), "Agent responsibilities require a selected agent");
        require(strings(at(config, "ai.permissions")).isEmpty(), "Agent permissions require a selected agent");
    }
    if (agent) require(has("ai.permissions", "read-project-files"), "AI work requires read-project-files");
    for (const auto& responsibility : {"coding", "refactoring", "configuration", "project-memory-maintenance", "project-status-maintenance"})
        if (has("ai.responsibilities", responsibility)) require(has("ai.permissions", "modify-files"), QString(responsibility) + " requires modify-files");
    if (has("ai.responsibilities", "coding")) require(has("ai.permissions", "create-files"), "Coding requires create-files");
    if (has("ai.responsibilities", "testing")) require(has("ai.permissions", "run-tests"), "Testing responsibility requires run-tests");
    if (has("ai.responsibilities", "build-ci")) require(has("ai.permissions", "run-builds"), "Build responsibility requires run-builds");
    if (has("ai.responsibilities", "static-analysis")) require(has("ai.permissions", "run-static-analysis"), "Static analysis responsibility requires run-static-analysis");
    if (has("ai.additionalAgents", at(config, "ai.primaryAgent").toString())) errors << "Primary agent cannot also be an additional agent";
    require(!at(config, "context").toString().trimmed().isEmpty(), "Configure project type");
    require(at(config, "memory.maximumSizeBytes").toDouble() > 0, "Memory must have a positive finite capacity");
    const auto outputs = config.value("generationOptions").toObject();
    bool product = false;
    for (auto it = outputs.begin(); it != outputs.end(); ++it) product |= it.value().toBool();
    require(product, "Select at least one generation product");
    return errors;
}

QStringList TemplateValidation::validateDefinition(const TemplateDefinition& definition)
{
    auto errors = validateConfiguration(definition.configuration);
    if (definition.id == QStringLiteral("academic-school-project")) {
        errors.removeAll(QStringLiteral("Configure languages"));
        errors.removeAll(QStringLiteral("Configure buildSystems"));
        errors.removeAll(QStringLiteral("Configure buildConfigurations"));
        errors.removeAll(QStringLiteral("CMake rules require CMake"));
    }
    if (definition.id.isEmpty() || definition.displayName.isEmpty()) errors << "Template identity is missing";
    // The fingerprint is stored with each applied template and custom template.
    // Catalog changes remain visible to Review and audit tooling without making a
    // valid built-in unusable merely because this executable was rebuilt.
    if (!definition.userDefined) {
        for (const auto& domain : {"rules.activeCategories", "rules.workScopes", "rules.projectScopes", "memory.captureCategories", "memory.maintenanceOptions", "memory.validationOptions", "memory.historyOptions", "ai.aramfIntegrations", "resourcePolicy.options"})
            if (strings(at(definition.configuration, domain)).isEmpty()) errors << "Incomplete built-in domain: " + QString(domain);
        for (const auto& option : AiCatalog::permissions()) {
            if (option.category == "High-Risk Actions" && definition.ai.permissions.contains(option.id)) errors << "Built-in template grants high-risk permission: " + option.id;
        }
    }
    return errors;
}

QStringList TemplateValidation::readiness(const ProjectModel& model)
{
    QStringList errors;
    // Older/manual projects retain their existing selective-generation contract.
    if (!model.templateState().isEmpty()) errors = validateConfiguration(ProjectPersistence().configuration(model));
    if (model.projectPath().trimmed().isEmpty() || QDir::cleanPath(model.projectPath()) == ".") errors << "Choose a Project Path.";
    return errors;
}

QStringList TemplateValidation::changedDomains(const ProjectModel& model)
{
    const auto baseline = model.templateState().value("baseline").toObject();
    const auto current = ProjectPersistence().configuration(model);
    QStringList changed;
    for (auto it = baseline.begin(); it != baseline.end(); ++it) if (current.value(it.key()) != it.value()) changed << it.key();
    return changed;
}

QJsonObject TemplateValidation::optionAudit(const TemplateDefinition& definition)
{
    QJsonObject result;
    const auto domains = catalogs();
    for (auto it = domains.begin(); it != domains.end(); ++it) {
        QJsonObject options;
        for (const auto& option : it.value()) options.insert(option.second,
            strings(at(definition.configuration, it.key())).contains(option.second) ? "SELECT" : "DO NOT SELECT");
        result.insert(it.key(), options);
    }
    return result;
}
