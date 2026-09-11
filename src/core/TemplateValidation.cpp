#include "TemplateValidation.h"
#include "DocumentTemplate.h"
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
        {"resource.type", resourceTypes()}, {"resource.role", governanceRoles()}, {"resource.authorityLevel", authorityLevels()}, {"resource.scopes", resourceScopes()},
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
    for (const auto& document : {DocumentTemplates::thesis(), DocumentTemplates::report()})
        for (const auto& error : DocumentTemplates::validate(document)) errors << error;
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
    const auto resources = config.value(QStringLiteral("resources")).toArray();
    const auto validateDocument = [&](const QString& label, const QString& key, const QString& role) {
        const auto document = config.value(QStringLiteral("academic")).toObject().value(key).toObject();
        const bool enabled = document.value(QStringLiteral("enabled")).toBool(false);
        const QString mode = document.value(QStringLiteral("templateMode")).toString(QStringLiteral("aramf-default"));
        const QString sourceId = document.value(QStringLiteral("templateSourceId")).toString();
        const QString templateId = document.value(QStringLiteral("templateId")).toString();
        const int templateVersion = document.value(QStringLiteral("templateVersion")).toInt(0);
        require(mode == QStringLiteral("aramf-default") || mode == QStringLiteral("source"), label + " has an invalid template mode");
        if (mode == QStringLiteral("aramf-default")) {
            const QString expectedId = label == QStringLiteral("Thesis") ? QStringLiteral("aramf-default-thesis") : QStringLiteral("aramf-default-report");
            require(templateId == expectedId, label + " default template identity is invalid");
            require(templateVersion >= 1, label + " default template version is invalid");
        }
        if (!enabled) require(mode == QStringLiteral("aramf-default") && sourceId.isEmpty(), label + " disabled state must not retain a template source");
        if (enabled && mode == QStringLiteral("source")) {
            QJsonObject selected;
            for (const auto& value : resources) if (value.toObject().value(QStringLiteral("id")).toString() == sourceId) selected = value.toObject();
            require(!sourceId.isEmpty() && !selected.isEmpty(), label + " template source does not exist");
            if (!selected.isEmpty()) {
                require(selected.value(QStringLiteral("enabled")).toBool(false), label + " template source is disabled");
                require(selected.value(QStringLiteral("role")).toString() == role, label + " template source must have role " + role);
            }
        }
    };
    validateDocument(QStringLiteral("Thesis"), QStringLiteral("thesisDocumentation"), QStringLiteral("thesis-template"));
    validateDocument(QStringLiteral("Report"), QStringLiteral("reportDocumentation"), QStringLiteral("report-template"));
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
    const bool androidTarget = has("capabilities.targetPlatforms", "android");
    const bool picoTarget = has("capabilities.hardwareTargets", "raspberry-pi-pico-2-w") || has("capabilities.frameworks", "pico-sdk");
    if (androidTarget && picoTarget) {
        require(has("capabilities.buildSystems", "gradle"), "Combined Android + Pico projects require Gradle");
        require(has("capabilities.buildSystems", "cmake") || has("capabilities.buildSystems", "pico-sdk-cmake"), "Combined Android + Pico projects require CMake");
    }
    if (has("rules.activeCategories", "cmake-rules")) require(has("capabilities.buildSystems", "cmake") || has("capabilities.buildSystems", "pico-sdk-cmake"), "CMake rules require CMake");
    if (has("capabilities.frameworks", "aspnet")) require(has("capabilities.buildSystems", "msbuild") || has("capabilities.buildSystems", "visual-studio-build"), "ASP.NET requires MSBuild");
    if (has("capabilities.hardwareTargets", "raspberry-pi-pico-2-w")) require(has("capabilities.processorFamilies", "rp2350"), "Pico 2 W requires RP2350");
    if (has("capabilities.hardwareTargets", "raspberry-pi-pico")) require(has("capabilities.processorFamilies", "rp2040"), "Pico requires RP2040");
    const auto communication = at(config, "communication").toObject();
    if (communication.value("enabled").toBool(false)) {
        const QString source = communication.value("sourceTarget").toString();
        const QString destination = communication.value("destinationTarget").toString();
        const QString transport = communication.value("transport").toString();
        const QString protocol = communication.value("protocol").toString();
        const auto endpoints = communication.value("endpoints").toArray();
        QHash<QString, QJsonObject> endpointObjects;
        for (const auto& endpointValue : endpoints) {
            const auto endpoint = endpointValue.toObject();
            endpointObjects.insert(endpoint.value("id").toString(), endpoint);
        }
        require(!source.isEmpty() && !destination.isEmpty() && source != destination,
                "Communication requires distinct source and destination targets");
        require(QStringList{"wifi", "bluetooth", "serial"}.contains(transport), "Unsupported communication transport: " + transport);
        if (!protocol.isEmpty()) require(QStringList{"http-rest", "websocket", "tcp", "udp", "serial"}.contains(protocol), "Unsupported communication protocol: " + protocol);
        if (protocol == "http-rest" || protocol == "websocket" || protocol == "tcp" || protocol == "udp") {
            bool addressConfigured = false;
            if (!endpointObjects.isEmpty()) {
                const auto links = communication.value("links").toArray();
                for (const auto& linkValue : links) {
                    const auto link = linkValue.toObject();
                    const QString linkProtocol = link.value("protocol").toString();
                    if (!linkProtocol.isEmpty() && linkProtocol != protocol) continue;
                    const auto endpointA = endpointObjects.value(link.value("endpointA").toString());
                    const auto endpointB = endpointObjects.value(link.value("endpointB").toString());
                    const auto roleAddress = [](const QJsonObject& endpoint, const QString& role) {
                        return endpoint.value("role").toString().compare(role, Qt::CaseInsensitive) == 0
                            && !endpoint.value("address").toString().trimmed().isEmpty();
                    };
                    if (roleAddress(endpointA, QStringLiteral("server"))) addressConfigured = true;
                    if (roleAddress(endpointB, QStringLiteral("server"))) addressConfigured = true;
                    if (endpointA.value("role").toString().isEmpty() && !endpointA.value("address").toString().trimmed().isEmpty()) addressConfigured = true;
                    if (endpointB.value("role").toString().isEmpty() && !endpointB.value("address").toString().trimmed().isEmpty()) addressConfigured = true;
                }
                if (links.isEmpty()) {
                    for (const auto& endpoint : endpointObjects) {
                        if (endpoint.value("role").toString().compare(QStringLiteral("server"), Qt::CaseInsensitive) == 0
                            && !endpoint.value("address").toString().trimmed().isEmpty()) { addressConfigured = true; break; }
                    }
                }
            } else {
                addressConfigured = !communication.value("endpoint").toString().trimmed().isEmpty();
            }
            // Endpoint address completeness is a readiness concern. Structural
            // configuration validation must still allow reusable templates to
            // be generated before deployment addresses are known.
            if (endpointObjects.isEmpty())
                require(addressConfigured, "Communication protocol requires an endpoint");
        }
        require(!communication.value("protocolVersion").toString().trimmed().isEmpty(), "Communication protocol version is required");
        if (source == "android-application")
            require(strings(at(config, "capabilities.targetPlatforms")).contains("android") || strings(at(config, "capabilities.languages")).contains("kotlin"),
                    "Communication source target is not represented by the selected Android capabilities");
        if (destination == "raspberry-pi-pico-2-w")
            require(strings(at(config, "capabilities.hardwareTargets")).contains("raspberry-pi-pico-2-w") || strings(at(config, "capabilities.frameworks")).contains("pico-sdk"),
                    "Communication destination target is not represented by the selected Pico capabilities");
        QSet<QString> endpointIds;
        for (const auto& endpointValue : endpoints) endpointIds.insert(endpointValue.toObject().value("id").toString());
        const auto links = communication.value("links").toArray();
        QSet<QString> linkEndpointPairs;
        for (const auto& linkValue : links) {
            const auto link = linkValue.toObject();
            const QString endpointA = link.value("endpointA").toString();
            const QString endpointB = link.value("endpointB").toString();
            require(endpointIds.contains(endpointA) && endpointIds.contains(endpointB), "Communication link references an unknown endpoint");
            linkEndpointPairs.insert(endpointA + QLatin1Char('\x1f') + endpointB);
            linkEndpointPairs.insert(endpointB + QLatin1Char('\x1f') + endpointA);
            require(link.value("protocolVersion").toString().trimmed().isEmpty() || link.value("protocolVersion").toString().toInt() > 0, "Communication protocol version is invalid");
            require(link.value("maximumPacketSize").toInt() >= 0, "Communication maximum packet size is invalid");
        }
        QSet<qint64> messageIds;
        const QSet<QString> fieldTypes = {"bool", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float32", "float64", "string", "bytes", "object", "array", "enum", "timestamp", "uuid"};
        const auto messages = communication.value("messages").toArray();
        for (const auto& messageValue : messages) {
            const auto message = messageValue.toObject(); const qint64 id = static_cast<qint64>(message.value("id").toDouble());
            require(id > 0 && !messageIds.contains(id), "Communication message IDs must be unique and positive"); messageIds.insert(id);
            require(!message.value("name").toString().trimmed().isEmpty(), "Communication message name is required");
            const QString sourceEndpoint = message.value("sourceEndpointId").toString();
            const QString destinationEndpoint = message.value("destinationEndpointId").toString();
            require(endpointIds.contains(sourceEndpoint) && endpointIds.contains(destinationEndpoint), "Communication message references an unknown endpoint");
            require(linkEndpointPairs.contains(sourceEndpoint + QLatin1Char('\x1f') + destinationEndpoint), "Communication message endpoints are not connected by a communication link");
            const auto response = static_cast<qint64>(message.value("responseMessageId").toDouble()); const auto request = static_cast<qint64>(message.value("requestMessageId").toDouble());
            require(response == 0 || response != id, "Communication response cannot reference itself"); require(request == 0 || request != id, "Communication request cannot reference itself");
            QSet<QString> fieldNames;
            for (const auto& fieldValue : message.value("fields").toArray()) { const auto field = fieldValue.toObject(); const QString name = field.value("name").toString(); require(!name.isEmpty() && !fieldNames.contains(name), "Communication field names must be unique within a message"); fieldNames.insert(name); require(fieldTypes.contains(field.value("type").toString()), "Communication field type is unsupported: " + field.value("type").toString()); }
        }
        for (const auto& messageValue : messages) { const auto message = messageValue.toObject(); const qint64 response = static_cast<qint64>(message.value("responseMessageId").toDouble()); const qint64 request = static_cast<qint64>(message.value("requestMessageId").toDouble()); if (response && !messageIds.contains(response)) errors << "Communication response references an unknown message"; if (request && !messageIds.contains(request)) errors << "Communication request references an unknown message"; }
    }
    const auto hardwareResources = config.value(QStringLiteral("hardwareResources")).toArray();
    if (!hardwareResources.isEmpty()) {
        QSet<QString> resourceIds;
        QSet<QString> endpointIds;
        for (const auto& endpointValue : communication.value(QStringLiteral("endpoints")).toArray()) endpointIds.insert(endpointValue.toObject().value(QStringLiteral("id")).toString());
        for (const auto& value : hardwareResources) {
            const auto resource = value.toObject();
            const QString id = resource.value(QStringLiteral("id")).toString();
            require(!id.trimmed().isEmpty() && !resourceIds.contains(id), "Hardware resource IDs must be unique and non-empty"); resourceIds.insert(id);
            require(QStringList{QStringLiteral("digital-pin"), QStringLiteral("analog-input"), QStringLiteral("pwm-output"), QStringLiteral("i2c-device"), QStringLiteral("uart"), QStringLiteral("servo"), QStringLiteral("relay"), QStringLiteral("display"), QStringLiteral("buzzer")}.contains(resource.value(QStringLiteral("resourceType")).toString()), "Unsupported hardware resource type");
            require(endpointIds.contains(resource.value(QStringLiteral("endpointId")).toString()), "Hardware resource references an unknown endpoint");
        }
        for (const auto& messageValue : communication.value(QStringLiteral("messages")).toArray()) {
            const auto message = messageValue.toObject();
            if (message.value(QStringLiteral("name")).toString() == QStringLiteral("WRITE_DIGITAL_PIN") || message.value(QStringLiteral("name")).toString() == QStringLiteral("READ_DIGITAL_PIN")) {
                for (const auto& fieldValue : message.value(QStringLiteral("fields")).toArray())
                    if (fieldValue.toObject().value(QStringLiteral("name")).toString() == QStringLiteral("pinId"))
                        require(fieldValue.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("string"), "Digital pin commands require symbolic string pinId");
            }
        }
    }
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
    const auto communication = model.communicationConfiguration();
    if (communication.enabled) {
        const auto protocolNeedsAddress = QStringList{"http-rest", "websocket", "tcp", "udp"}.contains(communication.protocol);
        if (protocolNeedsAddress && !communication.endpoints.isEmpty()) {
            bool listeningAddress = false;
            for (const auto& endpoint : communication.endpoints)
                if (endpoint.role.compare(QStringLiteral("server"), Qt::CaseInsensitive) == 0 && !endpoint.address.trimmed().isEmpty()) listeningAddress = true;
            if (!listeningAddress) errors << "Communication protocol requires a listening endpoint address";
        }
    }
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
