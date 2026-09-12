#include "Services.h"
#include "AramfPaths.h"
#include "AiCatalog.h"
#include "EnvironmentCatalog.h"
#include "MemoryCatalog.h"
#include "RuleCatalog.h"
#include "ProjectPersistence.h"
#include "TemplateValidation.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

namespace {
QStringList ids(const QList<EnvironmentOption>& options)
{
    QStringList result;
    for (const auto& option : options) result << option.second;
    return result;
}

QJsonObject readLibrary(const QString& path, QString* error)
{
    if (!QFileInfo::exists(path)) return {{"version", 1}, {"templates", QJsonArray{}}};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { if (error) *error = file.errorString(); return {}; }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()
        || doc.object().value("version").toInt() != 1 || !doc.object().value("templates").isArray()) {
        if (error) *error = "Invalid custom template library: " + path;
        return {};
    }
    QSet<QString> seen;
    for (const auto& item : doc.object().value("templates").toArray()) {
        const auto entry = item.toObject();
        const QString id = entry.value("id").toString();
        if (!id.startsWith("user-") || seen.contains(id) || entry.value("name").toString().trimmed().isEmpty()
            || !entry.value("configuration").isObject()) {
            if (error) *error = "Invalid or duplicate custom template record: " + id;
            return {};
        }
        seen.insert(id);
    }
    return doc.object();
}

bool writeLibrary(const QString& path, const QJsonObject& root, QString* error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = "Cannot create template library directory.";
        return false;
    }
    QSaveFile file(path);
    const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

TemplateDefinition base(const char* id, const char* name, const char* type,
                        QStringList languages, QStringList frameworks, QStringList targets,
                        QStringList toolchains, QStringList builds, QStringList dependencies)
{
    TemplateDefinition d;
    d.id = id; d.displayName = name; d.projectType = type;
    d.description = d.displayName + " development project";
    auto& c = d.capabilities;
    c.languages = languages; c.frameworks = frameworks; c.targetPlatforms = targets;
    c.toolchains = toolchains; c.buildSystems = builds; c.dependencyManagers = dependencies;
    c.ides = {"visual-studio-code"}; c.versionControlSystems = {"git"};
    c.hostOperatingSystems = {"windows"}; c.targetArchitectures = {"x86_64"};
    c.developmentTools = {"debugger"};
    c.buildConfigurations = {"debug", "release"};
    c.testingCapabilities = {"unit-testing", "integration-testing"};
    c.qualityCapabilities = {"static-analysis", "formatting", "coverage", "dependency-scanning"};
    c.automationCapabilities = {"automated-build", "automated-testing"};
    c.deliveryCapabilities = {"release-archive"};
    d.rules.activeCategories = ids(RuleCatalog::categories());
    for (const auto& excluded : {"hardware-in-the-loop", "cmake-rules", "ci-rules", "authentication-authorization", "academic-documentation", "api-documentation"})
        d.rules.activeCategories.removeAll(excluded);
    if (builds.contains("cmake")) d.rules.activeCategories << "cmake-rules";
    d.rules.workScopes = ids(RuleCatalog::workScopes());
    d.rules.workScopes.removeAll("academic");
    d.rules.projectScopes = ids(RuleCatalog::projectScopes());
    for (const auto& excluded : {"ci-cd", "academic-content", "hardware-firmware", "database-data", "ui-ux"}) d.rules.projectScopes.removeAll(excluded);
    d.rules.contextPolicies = ids(RuleCatalog::contextPolicies());
    d.memory.captureCategories = ids(MemoryCatalog::captureCategories());
    for (const auto& excluded : {"human-time", "ai-time", "autonomous-time", "waiting-time", "diagnosis-time", "iteration-count", "build-count", "test-count", "failure-count"})
        d.memory.captureCategories.removeAll(excluded);
    d.memory.maintenanceOptions = ids(MemoryCatalog::maintenanceOptions());
    d.memory.validationOptions = ids(MemoryCatalog::validationOptions());
    d.memory.historyOptions = ids(MemoryCatalog::historyOptions());
    d.resourcePolicy.options = ids(EnvironmentCatalog::resourcePolicyOptions());
    d.resourcePolicy.options.removeAll("infer-missing");
    d.resourcePolicy.options.removeAll("external-web-fallback");
    d.resourcePolicy.loadingStrategy = "relevant";
    for (const auto& option : AiCatalog::integrations()) d.ai.aramfIntegrations << option.id;
    d.certification.enabled = true;
    d.exclusions = {"Alternative stacks, IDEs and package managers", "Unrequested AI agents", "Deployment, signing credentials and remote repository permissions", "Physical test claims and specialized performance/fuzz testing", "Personal paths, project identity, course/institution and resource locations", "Time tracking and automatic web fallback"};
    return d;
}

void assistantDefaults(TemplateDefinition& d, const QString& agent)
{
    d.ai.primaryAgent = agent;
    d.ai.responsibilities = {"planning", "requirement-analysis", "architecture", "technical-design", "task-breakdown",
        "coding", "refactoring", "configuration", "code-review", "testing", "debugging", "static-analysis",
        "security-review", "compatibility-review", "regression-analysis", "dependency-review", "documentation",
        "build-ci", "resource-analysis", "project-memory-maintenance", "project-status-maintenance"};
    d.ai.permissions = {"read-project-files", "create-files", "modify-files", "run-builds", "run-tests",
        "run-static-analysis", "run-debugging-tools", "modify-build-configuration", "modify-project-configuration"};
}

// These four agents are the standard ARAMF collaboration set.  A template
// keeps its meaningful primary agent, while the remaining standard agents are
// enabled as additional agents.
void ensureStandardAgents(TemplateDefinition& d)
{
    const QStringList standard = {QStringLiteral("chatgpt"), QStringLiteral("gemini"),
                                  QStringLiteral("github-copilot"), QStringLiteral("openai-codex")};
    if (!standard.contains(d.ai.primaryAgent)) d.ai.primaryAgent = QStringLiteral("chatgpt");
    if (d.ai.responsibilities.isEmpty()) d.ai.responsibilities = {QStringLiteral("planning"), QStringLiteral("coding"), QStringLiteral("testing"), QStringLiteral("code-review")};
    if (d.ai.permissions.isEmpty()) d.ai.permissions = {QStringLiteral("read-project-files"), QStringLiteral("create-files"), QStringLiteral("modify-files"), QStringLiteral("run-builds"), QStringLiteral("run-tests")};
    for (const auto& agent : standard) {
        if (agent != d.ai.primaryAgent && !d.ai.additionalAgents.contains(agent)) d.ai.additionalAgents << agent;
    }
    d.ai.additionalAgents.removeDuplicates();
}

void finish(TemplateDefinition& d)
{
    ProjectModel model;
    model.setDevelopmentCapabilities(d.capabilities);
    model.setContext(d.projectType);
    model.setDescription(d.description);
    model.setAcademicConfiguration(d.academic);
    model.setAiConfiguration(d.ai);
    model.setRuleConfiguration(d.rules);
    model.setMemoryConfiguration(d.memory);
    model.setCommunicationConfiguration(d.communication);
    model.setResourcePolicy(d.resourcePolicy);
    model.setCertificationConfiguration(d.certification);
    model.setGenerationOptions(d.generation);
    model.resolveAndroidConstraints();
    d.environment = model.developmentEnvironment();
    d.configuration = ProjectPersistence().configuration(model);
}

QList<TemplateDefinition> builtIns()
{
    auto result = QList<TemplateDefinition>{
        base("pico-2w-visual-designer", "Pico 2 W Visual Designer", "embedded-firmware", {"cpp", "c", "pio-assembly"}, {"pico-sdk"}, {"windows-desktop", "microcontroller"}, {"msys2-ucrt64-gcc", "arm-gnu"}, {"cmake", "ninja"}, {"cmake-fetchcontent"}),
        base("android-studio-kotlin-gemini", "Android Studio/Kotlin/Gemini", "android-application", {"kotlin"}, {"android-sdk", "jetpack-compose", "android-emulator"}, {"android"}, {"java-jdk", "kotlin-jvm"}, {"gradle"}, {"gradle"}),
        base("android-arduino-smart-home", "Android Arduino Smart Home", "combined-android-arduino-smart-home", {"kotlin", "cpp", "c"}, {"android-sdk", "jetpack-compose", "android-emulator", "arduino"}, {"android", "embedded-system", "microcontroller"}, {"java-jdk", "avr-gcc"}, {"gradle", "arduino-build"}, {"gradle"}),
        base("qt-desktop-application", "Qt Desktop Application", "desktop-application", {"cpp"}, {"qt"}, {"windows-desktop"}, {"msys2-ucrt64-gcc"}, {"cmake", "ninja"}, {"cmake-fetchcontent"}),
        base("cpp-command-line", "C++ Command Line", "software-development", {"cpp"}, {}, {"command-line"}, {"msys2-ucrt64-gcc"}, {"cmake", "ninja"}, {"cmake-fetchcontent"}),
        base("cmake-library", "CMake Library", "reusable-library", {"cpp"}, {}, {"library"}, {"msys2-ucrt64-gcc"}, {"cmake", "ninja"}, {"cmake-fetchcontent"}),
        base("raspberry-pi-pico-firmware", "Raspberry Pi Pico Firmware", "embedded-firmware", {"cpp", "c", "pio-assembly"}, {"pico-sdk"}, {"microcontroller", "bare-metal"}, {"arm-gnu"}, {"cmake", "ninja"}, {"cmake-fetchcontent"}),
        base("react-frontend", "React Frontend", "frontend-web-application", {"typescript", "html", "css"}, {"react"}, {"web-browser"}, {"nodejs"}, {"npm"}, {"npm"}),
        base("python-backend", "Python Backend", "backend-service", {"python"}, {"fastapi"}, {"server"}, {"python"}, {"python-setuptools"}, {"pip"}),
        base("csharp-backend", "C# Backend", "backend-service", {"csharp"}, {"dotnet", "aspnet"}, {"server"}, {"dotnet-sdk"}, {"msbuild"}, {"nuget"}),
        base("mobile-application", "Mobile Application", "android-application", {"kotlin"}, {"android-sdk", "jetpack-compose", "android-emulator"}, {"android"}, {"java-jdk", "kotlin-jvm"}, {"gradle"}, {"gradle"}),
        base("full-stack-web-application", "Full Stack Web Application", "full-stack-web-application", {"typescript", "html", "css"}, {"react", "nodejs", "express"}, {"web-browser", "server"}, {"nodejs"}, {"npm"}, {"npm"}),
        base("bachelor-thesis", "Bachelor Thesis", "software-development", {"cpp"}, {}, {"command-line"}, {"msys2-ucrt64-gcc"}, {"cmake", "ninja"}, {"cmake-fetchcontent"})
    };
    for (auto& d : result) {
        auto& c = d.capabilities;
        const bool android = c.targetPlatforms.contains("android");
        const bool embedded = c.frameworks.contains("pico-sdk");
        const bool web = c.targetPlatforms.contains("web-browser");
        const bool server = c.targetPlatforms.contains("server");
        const bool ui = android || web || c.frameworks.contains("qt");
        if (ui) { c.testingCapabilities << "e2e-testing"; d.rules.projectScopes << "ui-ux"; }
        if (web || server || android) {
            c.hostOperatingSystems = {"cross-platform"};
            c.targetArchitectures = {"auto"};
            c.qualityCapabilities << "linting" << "security-scanning";
        }
        if (server) { d.rules.activeCategories << "api-documentation" << "authentication-authorization"; c.testingCapabilities << "e2e-testing"; }
        if (c.targetPlatforms.contains("library")) d.rules.activeCategories << "api-documentation";
        if (embedded) {
            c.developmentTools << "hardware-debug-probe";
            const bool designer = d.id == "pico-2w-visual-designer";
            c.processorFamilies = {designer ? "rp2350" : "rp2040"};
            c.hardwareTargets = {designer ? "raspberry-pi-pico-2-w" : "raspberry-pi-pico"};
            c.targetArchitectures = designer ? QStringList{"x86_64", "cortex-m", "rp2350"} : QStringList{"cortex-m", "rp2040"};
            c.deliveryCapabilities << "firmware-image";
            d.rules.projectScopes << "hardware-firmware";
            d.exclusions << "RISC-V alternative, RTOS, networking libraries and physical hardware-in-loop tests require explicit project needs";
        }
        if (c.frameworks.contains("qt")) { c.deliveryCapabilities << "package-installer"; c.developmentTools << "memory-analysis"; }
        if (android) {
            c.ides = {"android-studio"}; c.hardwareTargets = {"mobile-device"};
            c.developmentTools << "android-sdk";
            c.deliveryCapabilities << "package-installer";
            d.recommendedResources << "Course assignment / grading rubric";
            d.exclusions << "Java source, Room database and fixed Android ABIs are optional; XML/AndroidX/ADB/APK/AAB have no distinct catalog IDs";
        }
        if (d.id == "pico-2w-visual-designer") { assistantDefaults(d, "openai-codex"); d.ai.responsibilities << "ui-ux-design"; }
        if (d.id == "android-studio-kotlin-gemini") {
            assistantDefaults(d, "gemini");
            d.ai.additionalAgents = {"openai-codex", "claude-code"};
            d.ai.responsibilities << "ui-ux-design" << "accessibility-review";
            d.ai.permissions << "run-linters";
        }
        if (d.id == "android-arduino-smart-home") {
            c.ides = {QStringLiteral("android-studio"), QStringLiteral("arduino-ide")};
            c.developmentTools << QStringLiteral("arduino-ide");
            c.targetArchitectures = {QStringLiteral("avr")};
            c.hardwareTargets = {QStringLiteral("mobile-device"), QStringLiteral("arduino-mcu")};
            c.processorFamilies = {QStringLiteral("avr")};
            c.testingCapabilities << QStringLiteral("hardware-in-loop");
            c.qualityCapabilities << QStringLiteral("linting");
            c.deliveryCapabilities << QStringLiteral("firmware-image") << QStringLiteral("package-installer");
            d.rules.projectScopes << QStringLiteral("hardware-firmware");
            d.academic.enabled = true; d.academic.projectTypes = {QStringLiteral("academic-assignment")}; d.academic.academicMode = QStringLiteral("academic-assignment");
            d.ai.primaryAgent = QStringLiteral("gemini");
            assistantDefaults(d, QStringLiteral("gemini"));
            d.ai.responsibilities << QStringLiteral("ui-ux-design") << QStringLiteral("accessibility-review");
            d.ai.permissions << QStringLiteral("run-linters");
            d.description = QStringLiteral("School-oriented Android Kotlin application for the Keyestudio KS0085 Arduino-compatible Smart Home Kit, connected through HM-10 Bluetooth and UART/serial communication.");
            d.recommendedResources << QStringLiteral("Keyestudio KS0085 Smart Home Kit documentation") << QStringLiteral("Course assignment / grading rubric");
            d.exclusions << QStringLiteral("Raspberry Pi Pico, Wi-Fi, cloud services and fixed pin numbers are not selected by this Arduino template");
            d.communication.enabled = true;
            d.communication.sourceTarget = QStringLiteral("android-application");
            d.communication.destinationTarget = QStringLiteral("arduino-mcu");
            d.communication.sourceRole = QStringLiteral("client");
            d.communication.destinationRole = QStringLiteral("server");
            d.communication.transport = QStringLiteral("bluetooth");
            d.communication.protocol = QStringLiteral("serial");
            d.communication.protocolVersion = QStringLiteral("1");
            d.communication.reconnectPolicy = QStringLiteral("retry-with-backoff");
            d.communication.errorHandling = QStringLiteral("reject-malformed-and-report-error");
            d.communication.integrationRequirements = {QStringLiteral("bluetooth-pairing"), QStringLiteral("uart-serial-bridge"), QStringLiteral("reconnect-recovery"), QStringLiteral("malformed-input-rejection"), QStringLiteral("lifecycle-safe-connection")};
            d.communication.endpoints = {
                {QStringLiteral("android-endpoint"), QStringLiteral("android-application"), QStringLiteral("Android Application"), QStringLiteral("client"), {QStringLiteral("bluetooth"), QStringLiteral("command-transmission"), QStringLiteral("telemetry-reception")}, QStringLiteral("HM-10")},
                {QStringLiteral("arduino-endpoint"), QStringLiteral("arduino-mcu"), QStringLiteral("Arduino / KS0085 Smart Home"), QStringLiteral("server"), {QStringLiteral("bluetooth"), QStringLiteral("uart"), QStringLiteral("gpio"), QStringLiteral("pwm"), QStringLiteral("adc")}, QStringLiteral("KS0085")}};
            d.communication.links = {{QStringLiteral("android-arduino-bluetooth"), QStringLiteral("android-endpoint"), QStringLiteral("arduino-endpoint"), QStringLiteral("bidirectional"), QStringLiteral("bluetooth"), QStringLiteral("serial"), QStringLiteral("text"), QStringLiteral("command-oriented-messages"), QStringLiteral("utf-8"), QStringLiteral("1"), QStringLiteral("3s"), QStringLiteral("retry-with-backoff"), QStringLiteral("reject-malformed-and-report-error"), true, 256, {}}};
            d.communication.messages = {
                {1, QStringLiteral("DEVICE_COMMAND"), QStringLiteral("command"), QStringLiteral("android-endpoint->arduino-endpoint"), QStringLiteral("android-endpoint"), QStringLiteral("arduino-endpoint"), 0, 2, {{QStringLiteral("command"), QStringLiteral("string"), true}, {QStringLiteral("value"), QStringLiteral("string"), false}}, QStringLiteral("Symbolic actuator or device command; project defines the command vocabulary")},
                {2, QStringLiteral("DEVICE_STATE"), QStringLiteral("response"), QStringLiteral("arduino-endpoint->android-endpoint"), QStringLiteral("arduino-endpoint"), QStringLiteral("android-endpoint"), 1, 0, {{QStringLiteral("deviceId"), QStringLiteral("string"), true}, {QStringLiteral("state"), QStringLiteral("string"), true}, {QStringLiteral("value"), QStringLiteral("string"), false}}, QStringLiteral("Device state or sensor telemetry response")}};
            d.communication.testVectors = {};
            c.developmentTools.removeDuplicates();
            c.testingCapabilities.removeDuplicates();
            c.qualityCapabilities.removeDuplicates();
            c.deliveryCapabilities.removeDuplicates();
            d.rules.projectScopes.removeDuplicates();
            d.ai.permissions.removeDuplicates();
            d.ai.responsibilities.removeDuplicates();
        }
        if (d.id == "bachelor-thesis") {
            d.academic.enabled = true; d.academic.projectTypes = {QStringLiteral("thesis-project")}; d.academic.academicMode = "thesis"; d.academic.thesisLevel = "bachelor";
            d.academic.thesisApproaches = {"software-system-development"};
            d.academic.researchMethods = {"literature-review", "prototype-evaluation"};
            d.academic.academicRequirements = {"source-citations", "reference-list", "methodology-section", "related-work-background", "research-questions", "ethics-consideration", "reproducibility", "academic-formatting", "originality-check"};
            d.academic.academicDeliverables = {"written-thesis", "source-code", "prototype", "presentation", "demonstration"};
            d.rules.activeCategories << "academic-documentation";
            d.rules.workScopes << "academic"; d.rules.projectScopes << "academic-content";
            d.exclusions << "Institution, supervisor, citation style, thesis language and empirical method are user-specific";
        }
        c.testingCapabilities.removeDuplicates();
        ensureStandardAgents(d);
        finish(d);
        if (d.id == "android-arduino-smart-home") {
            d.configuration.insert(QStringLiteral("hardwareResources"), QJsonArray{
                QJsonObject{{QStringLiteral("id"), QStringLiteral("KS0085_LEDS")}, {QStringLiteral("endpointId"), QStringLiteral("arduino-endpoint")}, {QStringLiteral("resourceType"), QStringLiteral("digital-pin")}, {QStringLiteral("direction"), QStringLiteral("output")}, {QStringLiteral("purpose"), QStringLiteral("LED control")}, {QStringLiteral("ownership"), QStringLiteral("project")}, {QStringLiteral("capabilities"), QJsonArray{QStringLiteral("digital-output")}}},
                QJsonObject{{QStringLiteral("id"), QStringLiteral("KS0085_RELAY")}, {QStringLiteral("endpointId"), QStringLiteral("arduino-endpoint")}, {QStringLiteral("resourceType"), QStringLiteral("digital-pin")}, {QStringLiteral("direction"), QStringLiteral("output")}, {QStringLiteral("purpose"), QStringLiteral("Relay control")}, {QStringLiteral("ownership"), QStringLiteral("project")}, {QStringLiteral("capabilities"), QJsonArray{QStringLiteral("digital-output")}}},
                QJsonObject{{QStringLiteral("id"), QStringLiteral("KS0085_PIR")}, {QStringLiteral("endpointId"), QStringLiteral("arduino-endpoint")}, {QStringLiteral("resourceType"), QStringLiteral("digital-pin")}, {QStringLiteral("direction"), QStringLiteral("input")}, {QStringLiteral("purpose"), QStringLiteral("PIR motion sensor")}, {QStringLiteral("ownership"), QStringLiteral("project")}, {QStringLiteral("capabilities"), QJsonArray{QStringLiteral("digital-input")}}},
                QJsonObject{{QStringLiteral("id"), QStringLiteral("KS0085_ANALOG_SENSORS")}, {QStringLiteral("endpointId"), QStringLiteral("arduino-endpoint")}, {QStringLiteral("resourceType"), QStringLiteral("analog-input")}, {QStringLiteral("direction"), QStringLiteral("input")}, {QStringLiteral("purpose"), QStringLiteral("MQ-2, photocell, soil moisture and water/steam sensors")}, {QStringLiteral("ownership"), QStringLiteral("project")}, {QStringLiteral("capabilities"), QJsonArray{QStringLiteral("adc")}}}});
            ProjectModel normalized;
            ProjectPersistence().fromJson(&normalized, d.configuration);
            d.configuration = ProjectPersistence().configuration(normalized);
        }
        if (d.id == "qt-desktop-application") {
            d.environment.framework = "qt6";
            auto environment = d.configuration.value("environment").toObject();
            environment.insert("framework", "qt6");
            d.configuration.insert("environment", environment);
        }
    }
    return result;
}
}

TemplateManager::TemplateManager(QObject* parent, const QString& libraryPath) : QObject(parent),
    libraryPath_(libraryPath.isEmpty() ? QDir(AramfPaths::programRoot()).filePath("ARAMF_DATA/custom-templates.json") : libraryPath)
{}

QStringList TemplateManager::builtInTemplates() const
{
    QStringList result;
    for (const auto& d : builtIns()) result << d.id;
    return result;
}

QList<TemplateDefinition> TemplateManager::moduleDefinitions() const
{
    QList<TemplateDefinition> result;
    const auto sources = builtIns();
    const auto copyModule = [&result, &sources](const QString& sourceId, const QString& id, const QString& name) {
        TemplateDefinition source;
        for (const auto& candidate : sources) if (candidate.id == sourceId) { source = candidate; break; }
        if (!source.id.isEmpty()) {
            source.id = id; source.displayName = name; source.userDefined = false;
            source.kind = TemplateDefinition::Kind::Module;
            if (id == QStringLiteral("kotlin")) {
                source.projectType = QStringLiteral("software-development");
                source.capabilities = {};
                source.capabilities.languages = {QStringLiteral("kotlin")};
                source.capabilities.ides = {QStringLiteral("visual-studio-code")};
                source.capabilities.hostOperatingSystems = {QStringLiteral("windows")};
                source.capabilities.targetPlatforms = {QStringLiteral("command-line")};
                source.capabilities.targetArchitectures = {QStringLiteral("x86_64")};
                source.capabilities.toolchains = {QStringLiteral("java-jdk")};
                source.capabilities.buildSystems = {QStringLiteral("gradle")};
                source.capabilities.dependencyManagers = {QStringLiteral("gradle")};
                source.capabilities.buildConfigurations = {QStringLiteral("debug"), QStringLiteral("release")};
            } else if (id == QStringLiteral("academic-school-project")) {
                source.projectType = QStringLiteral("software-development");
                source.capabilities = {};
                source.capabilities.ides = {QStringLiteral("visual-studio-code")};
                source.capabilities.hostOperatingSystems = {QStringLiteral("windows")};
                source.capabilities.targetPlatforms = {QStringLiteral("command-line")};
                source.capabilities.targetArchitectures = {QStringLiteral("x86_64")};
                source.capabilities.toolchains = {QStringLiteral("msys2-ucrt64-gcc")};
                source.academic.enabled = true; source.academic.projectTypes = {QStringLiteral("academic-assignment")}; source.academic.academicMode = QStringLiteral("academic-assignment");
                source.academic.thesisLevel.clear();
                source.academic.thesisApproaches.clear();
                source.academic.researchMethods.clear();
            }
            ensureStandardAgents(source);
            finish(source);
            result << source; return;
        }
    };
    copyModule(QStringLiteral("android-studio-kotlin-gemini"), QStringLiteral("android-application"), QStringLiteral("Android Application"));
    copyModule(QStringLiteral("android-studio-kotlin-gemini"), QStringLiteral("kotlin"), QStringLiteral("Kotlin"));
    copyModule(QStringLiteral("pico-2w-visual-designer"), QStringLiteral("pico-2w"), QStringLiteral("Raspberry Pi Pico 2 W"));
    copyModule(QStringLiteral("cpp-command-line"), QStringLiteral("cpp"), QStringLiteral("C++"));
    copyModule(QStringLiteral("qt-desktop-application"), QStringLiteral("desktop-application"), QStringLiteral("Desktop Application"));
    copyModule(QStringLiteral("bachelor-thesis"), QStringLiteral("academic-school-project"), QStringLiteral("Academic / School Project"));
    const auto focused = [&result, &sources](const QString& sourceId, const QString& id, const QString& name,
                                              const QStringList& languages, const QStringList& frameworks,
                                              const QStringList& tools, const QStringList& targets) {
        TemplateDefinition source;
        for (const auto& candidate : sources) if (candidate.id == sourceId) { source = candidate; break; }
        if (source.id.isEmpty()) return;
        source.id = id; source.displayName = name; source.kind = TemplateDefinition::Kind::Module;
        source.capabilities.languages = languages; source.capabilities.frameworks = frameworks;
        source.capabilities.developmentTools = tools; source.capabilities.targetPlatforms = targets;
        source.capabilities.ides.clear(); source.capabilities.targetArchitectures = {QStringLiteral("auto")};
        source.capabilities.processorFamilies.clear(); source.capabilities.hardwareTargets.clear();
        ensureStandardAgents(source); finish(source); result << source;
    };
    focused("android-studio-kotlin-gemini", "android-sdk", "Android SDK", {}, {"android-sdk"}, {"android-sdk"}, {"android"});
    focused("android-studio-kotlin-gemini", "gradle", "Gradle", {}, {}, {"gradle"}, {});
    focused("android-studio-kotlin-gemini", "android-studio", "Android Studio", {}, {}, {"android-studio"}, {"android"});
    focused("qt-desktop-application", "qt", "Qt", {"cpp"}, {"qt"}, {}, {"windows-desktop"});
    focused("cpp-command-line", "cmake", "CMake", {}, {}, {"cmake"}, {});
    focused("react-frontend", "react", "React", {"typescript"}, {"react"}, {}, {"web-browser"});
    focused("full-stack-web-application", "javascript", "JavaScript", {"javascript"}, {}, {}, {"web-browser"});
    focused("csharp-backend", "csharp", "C#", {"csharp"}, {}, {}, {"server"});
    focused("python-backend", "python", "Python", {"python"}, {}, {}, {"server"});
    focused("android-studio-kotlin-gemini", "java", "Java", {"java"}, {}, {}, {"command-line"});
    focused("pico-2w-visual-designer", "embedded-firmware", "Embedded Firmware", {"c", "cpp"}, {"pico-sdk"}, {"arm-gnu"}, {"microcontroller"});
    focused("cpp-command-line", "c", "C", {"c"}, {}, {}, {"command-line"});
    focused("react-frontend", "typescript", "TypeScript", {"typescript"}, {}, {"nodejs"}, {"web-browser"});
    focused("full-stack-web-application", "web-application", "Web Application", {"typescript", "html", "css"}, {"nodejs"}, {"nodejs"}, {"web-browser"});
    focused("react-frontend", "frontend", "Frontend", {"typescript", "html", "css"}, {"react"}, {"nodejs"}, {"web-browser"});
    focused("python-backend", "backend", "Backend", {"python"}, {"fastapi"}, {"python"}, {"server"});
    focused("cpp-command-line", "command-line-application", "Command Line Application", {"cpp"}, {}, {}, {"command-line"});
    focused("cmake-library", "library", "Library", {"cpp"}, {}, {}, {"library"});
    focused("android-studio-kotlin-gemini", "mobile-platform", "Mobile Application", {"kotlin"}, {"android-sdk"}, {"java-jdk"}, {"mobile", "android"});
    focused("pico-2w-visual-designer", "pico-sdk", "Pico SDK", {"c", "cpp"}, {"pico-sdk"}, {"arm-gnu"}, {"microcontroller"});
    focused("pico-2w-visual-designer", "pio-assembly", "PIO Assembly", {"pio-assembly"}, {"pico-sdk"}, {"arm-gnu"}, {"microcontroller"});
    const auto generic = [&result](const QString& id, const QString& name, const QString& description) {
        TemplateDefinition module;
        module.kind = TemplateDefinition::Kind::Module;
        module.id = id; module.displayName = name; module.projectType = QStringLiteral("software-development");
        module.description = description; module.capabilities = {};
        ensureStandardAgents(module); finish(module); result << module;
    };
    generic(QStringLiteral("arduino"), QStringLiteral("Arduino"), QStringLiteral("Arduino-compatible embedded development."));
    generic(QStringLiteral("arduino-ide"), QStringLiteral("Arduino IDE"), QStringLiteral("Arduino development tooling."));
    generic(QStringLiteral("arduino-mcu"), QStringLiteral("Arduino-compatible controller"), QStringLiteral("Arduino-compatible microcontroller target."));
    generic(QStringLiteral("hardware-integration"), QStringLiteral("Hardware Integration"), QStringLiteral("Explicit hardware ownership and integration boundaries."));
    generic(QStringLiteral("bluetooth-communication"), QStringLiteral("Bluetooth Communication"), QStringLiteral("Bluetooth device communication."));
    generic(QStringLiteral("serial-communication"), QStringLiteral("Serial / UART Communication"), QStringLiteral("UART and serial protocol integration."));
    generic(QStringLiteral("smart-home-iot"), QStringLiteral("Smart Home / IoT"), QStringLiteral("Smart Home and IoT device responsibilities."));
    generic(QStringLiteral("sensor-integration"), QStringLiteral("Sensor Integration"), QStringLiteral("Digital and analog sensor acquisition."));
    generic(QStringLiteral("actuator-control"), QStringLiteral("Actuator Control"), QStringLiteral("Actuator, relay, motor and servo control."));
    generic(QStringLiteral("device-control"), QStringLiteral("Device Control"), QStringLiteral("Remote device command and state control."));
    generic(QStringLiteral("device-monitoring"), QStringLiteral("Device Monitoring"), QStringLiteral("Telemetry and device state monitoring."));
    generic(QStringLiteral("ui-device-control"), QStringLiteral("User Interface / Device Control"), QStringLiteral("Student-friendly UI-to-device command mapping."));
    generic(QStringLiteral("build-test-delivery"), QStringLiteral("Build / Test / Delivery"), QStringLiteral("Incremental build, test and delivery workflow."));
    generic(QStringLiteral("git-version-control"), QStringLiteral("Git / Version Control"), QStringLiteral("Git-based project history and collaboration."));
    TemplateDefinition wifi;
    wifi.kind = TemplateDefinition::Kind::Module;
    wifi.id = QStringLiteral("wifi-communication");
    wifi.displayName = QStringLiteral("Wi-Fi Communication");
    wifi.projectType = QStringLiteral("software-development");
    wifi.description = QStringLiteral("Adds a configurable Wi-Fi communication link between project targets without choosing a protocol.");
    wifi.capabilities.targetPlatforms = {QStringLiteral("android"), QStringLiteral("microcontroller")};
    wifi.capabilities.hostOperatingSystems = {QStringLiteral("cross-platform")};
    wifi.communication.enabled = true;
    wifi.communication.transport = QStringLiteral("wifi");
    wifi.communication.integrationRequirements = {QStringLiteral("endpoint-reachability"), QStringLiteral("malformed-input-rejection"), QStringLiteral("reconnect-recovery"), QStringLiteral("timeout-handling")};
    ensureStandardAgents(wifi); finish(wifi); result << wifi;
    return result;
}

QList<TemplateDefinition> TemplateManager::compositeDefinitions() const
{
    return definitions();
}

QList<TemplateDefinition> TemplateManager::customDefinitions() const
{
    QList<TemplateDefinition> result;
    for (const auto& definition : definitions())
        if (definition.userDefined) result << definition;
    return result;
}

QList<TemplateDefinition> TemplateManager::officialDefinitions() const
{
    QList<TemplateDefinition> result;
    for (const auto& source : builtIns()) {
        if (source.id != QStringLiteral("pico-2w-visual-designer") && source.id != QStringLiteral("qt-desktop-application") && source.id != QStringLiteral("android-arduino-smart-home")) continue;
        auto d = source;
        d.id = source.id == QStringLiteral("pico-2w-visual-designer") ? QStringLiteral("official-pico-visual-designer") : source.id == QStringLiteral("qt-desktop-application") ? QStringLiteral("official-aramf-development") : QStringLiteral("official-android-arduino-smart-home");
        d.displayName = source.id == QStringLiteral("pico-2w-visual-designer") ? QStringLiteral("Pico Visual Designer") : source.id == QStringLiteral("qt-desktop-application") ? QStringLiteral("ARAMF Development") : QStringLiteral("Android Arduino Smart Home");
        d.official = true;
        result << d;
    }
    // ARAMF itself is a Qt/CMake desktop project; use the existing Qt preset
    // as the authoritative catalog-backed configuration.
    for (auto& official : result) if (official.id == QStringLiteral("official-aramf-development")) official.description = QStringLiteral("Official ARAMF development configuration using C++, Qt, CMake and standard ARAMF governance.");
    TemplateDefinition combined;
    for (const auto& source : builtIns()) {
        if (source.id == QStringLiteral("android-studio-kotlin-gemini")) combined = source;
    }
    if (!combined.id.isEmpty()) {
        TemplateDefinition pico;
        for (const auto& source : builtIns()) if (source.id == QStringLiteral("pico-2w-visual-designer")) pico = source;
        const auto append = [](QStringList& target, const QStringList& values) {
            for (const auto& value : values) if (!target.contains(value)) target << value;
        };
        append(combined.capabilities.languages, pico.capabilities.languages);
        append(combined.capabilities.frameworks, pico.capabilities.frameworks);
        append(combined.capabilities.ides, pico.capabilities.ides);
        append(combined.capabilities.developmentTools, pico.capabilities.developmentTools);
        append(combined.capabilities.targetPlatforms, pico.capabilities.targetPlatforms);
        append(combined.capabilities.targetArchitectures, pico.capabilities.targetArchitectures);
        if (combined.capabilities.targetArchitectures.size() > 1)
            combined.capabilities.targetArchitectures.removeAll(QStringLiteral("auto"));
        append(combined.capabilities.processorFamilies, pico.capabilities.processorFamilies);
        append(combined.capabilities.hardwareTargets, pico.capabilities.hardwareTargets);
        append(combined.capabilities.toolchains, pico.capabilities.toolchains);
        append(combined.capabilities.buildSystems, pico.capabilities.buildSystems);
        append(combined.capabilities.buildConfigurations, pico.capabilities.buildConfigurations);
        append(combined.capabilities.testingCapabilities, pico.capabilities.testingCapabilities);
        append(combined.capabilities.deliveryCapabilities, pico.capabilities.deliveryCapabilities);
        combined.id = QStringLiteral("official-android-pico-2w");
        combined.displayName = QStringLiteral("Android + Pico 2 W");
        combined.projectType = QStringLiteral("combined-android-pico");
        combined.official = true;
        combined.description = QStringLiteral("Official combined Android and Raspberry Pi Pico 2 W configuration with a shared Wi-Fi communication contract.");
        combined.ai.primaryAgent = QStringLiteral("gemini");
        combined.communication.enabled = true;
        combined.communication.sourceTarget = QStringLiteral("android-application");
        combined.communication.destinationTarget = QStringLiteral("raspberry-pi-pico-2-w");
        combined.communication.sourceRole = QStringLiteral("client");
        combined.communication.destinationRole = QStringLiteral("server");
        combined.communication.transport = QStringLiteral("wifi");
        combined.communication.protocol = QStringLiteral("websocket");
        combined.communication.endpoints = {
            {QStringLiteral("endpoint-a"), QStringLiteral("android-application"), QStringLiteral("Android Application"), QStringLiteral("client"), {QStringLiteral("wifi"), QStringLiteral("websocket")}, QStringLiteral("android.local")},
            {QStringLiteral("endpoint-b"), QStringLiteral("raspberry-pi-pico-2-w"), QStringLiteral("Raspberry Pi Pico 2 W"), QStringLiteral("server"), {QStringLiteral("wifi"), QStringLiteral("websocket")}, QStringLiteral("ws://pico.local:8080")}};
        combined.communication.links = {{QStringLiteral("android-pico-link"), QStringLiteral("endpoint-a"), QStringLiteral("endpoint-b"), QStringLiteral("bidirectional"), QStringLiteral("wifi"), QStringLiteral("websocket"), QStringLiteral("binary"), QStringLiteral("structured-messages"), QStringLiteral("cbor"), QStringLiteral("1"), {}, {}, {}, false, 0, QStringLiteral("little-endian")}};
        combined.communication.messages = {
            {1, QStringLiteral("WRITE_DIGITAL_PIN"), QStringLiteral("command"), QStringLiteral("endpoint-a->endpoint-b"), QStringLiteral("endpoint-a"), QStringLiteral("endpoint-b"), 0, 2,
             {{QStringLiteral("pinId"), QStringLiteral("string"), true, QStringLiteral("Symbolic hardware resource ID")}, {QStringLiteral("value"), QStringLiteral("bool"), true}}, QStringLiteral("Write a logical digital resource")},
            {2, QStringLiteral("WRITE_DIGITAL_PIN_RESULT"), QStringLiteral("response"), QStringLiteral("endpoint-b->endpoint-a"), QStringLiteral("endpoint-b"), QStringLiteral("endpoint-a"), 1, 0,
             {{QStringLiteral("pinId"), QStringLiteral("string"), true}, {QStringLiteral("success"), QStringLiteral("bool"), true}}, QStringLiteral("Result of a digital write")},
            {3, QStringLiteral("READ_DIGITAL_PIN"), QStringLiteral("request"), QStringLiteral("endpoint-a->endpoint-b"), QStringLiteral("endpoint-a"), QStringLiteral("endpoint-b"), 0, 4,
             {{QStringLiteral("pinId"), QStringLiteral("string"), true}}, QStringLiteral("Read a logical digital resource")},
            {4, QStringLiteral("DIGITAL_PIN_STATE"), QStringLiteral("response"), QStringLiteral("endpoint-b->endpoint-a"), QStringLiteral("endpoint-b"), QStringLiteral("endpoint-a"), 3, 0,
             {{QStringLiteral("pinId"), QStringLiteral("string"), true}, {QStringLiteral("value"), QStringLiteral("bool"), true}}, QStringLiteral("Digital resource state")}};
        combined.communication.integrationRequirements = {QStringLiteral("endpoint-reachability"), QStringLiteral("malformed-input-rejection"), QStringLiteral("reconnect-recovery"), QStringLiteral("timeout-handling"), QStringLiteral("protocol-contract-compatibility")};
        ensureStandardAgents(combined);
        finish(combined);
        combined.configuration.insert(QStringLiteral("hardwareResources"), QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("STATUS_OUTPUT")}, {QStringLiteral("endpointId"), QStringLiteral("endpoint-b")}, {QStringLiteral("resourceType"), QStringLiteral("digital-pin")}, {QStringLiteral("physicalResource"), QString()}, {QStringLiteral("direction"), QStringLiteral("output")}, {QStringLiteral("activeLevel"), QStringLiteral("high")}, {QStringLiteral("purpose"), QStringLiteral("Configurable status output")}, {QStringLiteral("ownership"), QStringLiteral("project")}, {QStringLiteral("capabilities"), QJsonArray{QStringLiteral("digital-output")}}}});
        result << combined;
    }
    return result;
}

QList<TemplateDefinition> TemplateManager::definitions() const
{
    auto result = builtIns();
    const auto library = readLibrary(libraryPath_, nullptr);
    for (const auto& item : library.value("templates").toArray()) {
        const auto entry = item.toObject();
        TemplateDefinition d;
        d.id = entry.value("id").toString(); d.displayName = entry.value("name").toString();
        d.kind = TemplateDefinition::Kind::CompositeTemplate;
        d.configuration = entry.value("configuration").toObject();
        d.userDefined = true;
        ProjectModel model;
        ProjectPersistence().fromJson(&model, d.configuration);
        d.projectType = d.configuration.value("context").toString();
        d.capabilities = model.developmentCapabilities(); d.environment = model.developmentEnvironment();
        d.academic = model.academicConfiguration(); d.ai = model.aiConfiguration();
        d.rules = model.ruleConfiguration(); d.memory = model.memoryConfiguration();
        d.resourcePolicy = model.resourcePolicy(); d.generation = model.generationOptions();
        d.communication = model.communicationConfiguration();
        d.certification = model.certificationConfiguration(); d.description = model.description();
        ensureStandardAgents(d);
        model.setAiConfiguration(d.ai);
        d.configuration = ProjectPersistence().configuration(model);
        // Keep the reusable module provenance alongside the normalized
        // configuration; configuration() intentionally omits this UI state.
        d.configuration.insert(QStringLiteral("templateModules"), entry.value(QStringLiteral("configuration")).toObject().value(QStringLiteral("templateModules")));
        result << d;
    }
    return result;
}

QString TemplateManager::libraryError() const { QString error; readLibrary(libraryPath_, &error); return error; }

TemplateDefinition TemplateManager::definition(const QString& id) const
{
    const QString canonical = id == "android-kotlin-lite" ? "android-studio-kotlin-gemini" : id == "Android_Arduino_Smart_Home" ? "official-android-arduino-smart-home" : id;
    for (const auto& d : moduleDefinitions()) if (d.id == canonical) return d;
    for (const auto& d : officialDefinitions()) if (d.id == canonical) return d;
    for (const auto& d : definitions()) if (d.id == canonical) return d;
    return {};
}

bool TemplateManager::applyTemplate(ProjectModel* model, const QString& id, QString* error) const
{
    return applyModules(model, id.isEmpty() ? QStringList{} : QStringList{id}, error);
}

bool TemplateManager::applyComposedSelection(ProjectModel* model, const QStringList& templateIds,
                                              const QStringList& manualModuleIds, QString* error) const
{
    QStringList sources = templateIds;
    for (const auto& moduleId : manualModuleIds) if (!sources.contains(moduleId)) sources << moduleId;
    if (!applyModules(model, sources, error)) return false;
    auto state = model->templateState();
    state.insert(QStringLiteral("activeTemplates"), QJsonArray::fromStringList(templateIds));
    QJsonArray activeTemplateNames;
    for (const auto& id : templateIds) {
        const auto definition = this->definition(id);
        activeTemplateNames.append(definition.displayName.isEmpty() ? id : definition.displayName);
    }
    state.insert(QStringLiteral("activeTemplateNames"), activeTemplateNames);
    state.insert(QStringLiteral("manualModules"), QJsonArray::fromStringList(manualModuleIds));
    model->setTemplateState(state);
    return true;
}

namespace {
void mergeStringArray(QJsonObject& target, const QJsonObject& source, const QString& key)
{
    QStringList values;
    for (const auto& value : target.value(key).toArray()) values << value.toString();
    for (const auto& value : source.value(key).toArray()) if (!values.contains(value.toString())) values << value.toString();
    QJsonArray array; for (const auto& value : values) array.append(value);
    target.insert(key, array);
}
void mergeDomain(QJsonObject& target, const QJsonObject& source, const QString& domain, const QStringList& fields)
{
    auto left = target.value(domain).toObject();
    const auto right = source.value(domain).toObject();
    for (const auto& field : fields) mergeStringArray(left, right, field);
    target.insert(domain, left);
}
}

bool TemplateManager::applyModules(ProjectModel* model, const QStringList& moduleIds, QString* error) const
{
    if (!model) { if (error) *error = "Project model is not available."; return false; }
    if (moduleIds.isEmpty()) {
        // Release the constraint, retaining the effective user-editable configuration.
        model->beginUpdate(); model->setTemplateModules({}); model->setTemplateId({}); model->clearEnvironmentOverrides(); model->endUpdate();
        return true;
    }
    QStringList effectiveIds = moduleIds;
    const QHash<QString, QStringList> templateModules = {
        {QStringLiteral("official-android-pico-2w"), {QStringLiteral("android-application"), QStringLiteral("mobile-platform"), QStringLiteral("kotlin"), QStringLiteral("android-sdk"), QStringLiteral("android-studio"), QStringLiteral("gradle"), QStringLiteral("pico-2w"), QStringLiteral("embedded-firmware"), QStringLiteral("pico-sdk"), QStringLiteral("c"), QStringLiteral("cpp"), QStringLiteral("pio-assembly"), QStringLiteral("wifi-communication")}},
        {QStringLiteral("official-android-arduino-smart-home"), {QStringLiteral("mobile-platform"), QStringLiteral("android-application"), QStringLiteral("kotlin"), QStringLiteral("android-sdk"), QStringLiteral("android-studio"), QStringLiteral("gradle"), QStringLiteral("embedded-firmware"), QStringLiteral("arduino"), QStringLiteral("arduino-ide"), QStringLiteral("arduino-mcu"), QStringLiteral("c"), QStringLiteral("cpp"), QStringLiteral("hardware-integration"), QStringLiteral("bluetooth-communication"), QStringLiteral("serial-communication"), QStringLiteral("smart-home-iot"), QStringLiteral("sensor-integration"), QStringLiteral("actuator-control"), QStringLiteral("device-control"), QStringLiteral("device-monitoring"), QStringLiteral("ui-device-control"), QStringLiteral("build-test-delivery"), QStringLiteral("git-version-control")}},
        {QStringLiteral("official-pico-visual-designer"), {QStringLiteral("desktop-application"), QStringLiteral("cpp"), QStringLiteral("c"), QStringLiteral("qt"), QStringLiteral("cmake"), QStringLiteral("pico-2w"), QStringLiteral("embedded-firmware"), QStringLiteral("pico-sdk"), QStringLiteral("pio-assembly")}},
        {QStringLiteral("official-aramf-development"), {QStringLiteral("desktop-application"), QStringLiteral("cpp"), QStringLiteral("qt"), QStringLiteral("cmake")}},
        {QStringLiteral("android-studio-kotlin-gemini"), {QStringLiteral("android-application"), QStringLiteral("kotlin")}},
        {QStringLiteral("mobile-application"), {QStringLiteral("android-application"), QStringLiteral("kotlin")}},
        {QStringLiteral("pico-2w-visual-designer"), {QStringLiteral("pico-2w"), QStringLiteral("cpp")}},
        {QStringLiteral("raspberry-pi-pico-firmware"), {QStringLiteral("pico-2w"), QStringLiteral("cpp")}},
        {QStringLiteral("qt-desktop-application"), {QStringLiteral("desktop-application"), QStringLiteral("cpp")}},
        {QStringLiteral("cpp-command-line"), {QStringLiteral("cpp")}},
        {QStringLiteral("cmake-library"), {QStringLiteral("cpp")}},
        {QStringLiteral("bachelor-thesis"), {QStringLiteral("academic-school-project")}}
    };
    for (const auto& id : moduleIds) for (const auto& dependency : templateModules.value(id))
        if (!effectiveIds.contains(dependency)) effectiveIds << dependency;
    for (const auto& id : moduleIds) {
        const auto custom = definition(id);
        if (custom.userDefined) for (const auto& value : custom.configuration.value(QStringLiteral("templateModules")).toArray())
            if (!effectiveIds.contains(value.toString())) effectiveIds << value.toString();
    }
    QList<TemplateDefinition> selected;
    for (const auto& id : moduleIds) {
        const auto d = definition(id);
        if (d.id.isEmpty()) { if (error) *error = "Unknown template/module: " + id; return false; }
        // A focused module is intentionally a partial contribution. Its
        // merged result is validated after all selected modules are applied;
        // requiring a standalone complete project here would make useful
        // modules such as Gradle or Android SDK immediately self-unselect.
        if (d.kind == TemplateDefinition::Kind::CompositeTemplate && !d.userDefined) {
            const auto errors = TemplateValidation::validateDefinition(d);
            if (!errors.isEmpty()) { if (error) *error = errors.join('\n'); return false; }
        }
        selected << d;
    }
    QJsonObject root;
    const auto previous = ProjectPersistence().toJson(*model);
    const auto manualBaseline = model->templateState().value("manualBaseline").toObject();
    if (!manualBaseline.isEmpty()) root = manualBaseline;
    else if (model->templateModules().isEmpty()
             && (model->developmentCapabilities().languages != QStringList{QStringLiteral("cpp")}
                 || model->developmentCapabilities().frameworks != QStringList{QStringLiteral("none")}
                 || model->developmentCapabilities().targetPlatforms != QStringList{QStringLiteral("desktop")}
                 || model->academicConfiguration().enabled
                 || model->aiConfiguration().primaryAgent != QStringLiteral("none"))) root = previous;
    else root = selected.first().configuration;
    QJsonObject provenance;
    QStringList contexts;
    QString primary;
    for (const auto& d : selected) {
        if (primary.isEmpty() && d.ai.primaryAgent != QStringLiteral("none")) primary = d.ai.primaryAgent;
        const auto domains = QStringList{QStringLiteral("capabilities"), QStringLiteral("academic"), QStringLiteral("ai"), QStringLiteral("rules"), QStringLiteral("memory"), QStringLiteral("generationOptions")};
        for (const auto& domain : domains) {
            auto sources = provenance.value(domain).toArray();
            sources.append(d.id);
            provenance.insert(domain, sources);
        }
        if (!d.projectType.isEmpty() && !contexts.contains(d.projectType)) contexts << d.projectType;
        const auto source = d.configuration;
        mergeDomain(root, source, QStringLiteral("capabilities"), {"languages", "frameworks", "ides", "versionControlSystems", "developmentTools", "hostOperatingSystems", "targetPlatforms", "targetArchitectures", "processorFamilies", "hardwareTargets", "toolchains", "buildSystems", "dependencyManagers", "buildConfigurations", "testingCapabilities", "qualityCapabilities", "automationCapabilities", "deliveryCapabilities"});
        mergeDomain(root, source, QStringLiteral("rules"), {"activeCategories", "workScopes", "projectScopes", "contextPolicies"});
        mergeDomain(root, source, QStringLiteral("memory"), {"captureCategories", "maintenanceOptions", "validationOptions", "historyOptions"});
        mergeDomain(root, source, QStringLiteral("ai"), {"additionalAgents", "responsibilities", "permissions", "aramfIntegrations"});
        mergeDomain(root, source, QStringLiteral("generationOptions"), {});
        if (source.value(QStringLiteral("communication")).toObject().value(QStringLiteral("enabled")).toBool(false))
            root.insert(QStringLiteral("communication"), source.value(QStringLiteral("communication")));
        if (source.contains(QStringLiteral("hardwareResources"))) root.insert(QStringLiteral("hardwareResources"), source.value(QStringLiteral("hardwareResources")));
        const auto academic = source.value("academic").toObject();
        if (academic.value("enabled").toBool(false) || !academic.value("projectTypes").toArray().isEmpty() || academic.value("academicMode").toString() != QStringLiteral("disabled")) root.insert("academic", academic);
        if (root.value("context").toString().isEmpty() && !d.projectType.isEmpty()) root.insert("context", d.projectType);
    }
    auto ai = root.value("ai").toObject();
    if (!primary.isEmpty()) ai.insert("primaryAgent", primary);
    QStringList baselineAgents = {"chatgpt", "gemini", "github-copilot", "openai-codex"};
    auto additional = QStringList{}; for (const auto& value : ai.value("additionalAgents").toArray()) additional << value.toString();
    const auto selectedPrimary = ai.value("primaryAgent").toString();
    for (const auto& agent : baselineAgents) if (agent != selectedPrimary && !additional.contains(agent)) additional << agent;
    QJsonArray additionalJson; for (const auto& agent : additional) additionalJson.append(agent);
    ai.insert("additionalAgents", additionalJson); root.insert("ai", ai);
    root.insert("context", contexts.isEmpty() ? root.value("context") : contexts.first());
    QMap<QString, QString> preservedEnvironment;
    const auto baselineEnvironment = model->templateState().value("baseline").toObject().value("environment").toObject();
    const auto currentEnvironment = model->developmentEnvironment();
    const auto preserve = [&](const QString& key, const QString& value) {
        if (baselineEnvironment.contains(key) && baselineEnvironment.value(key).toString() != value
            && !value.trimmed().isEmpty() && value != QStringLiteral("unrelated-override")) preservedEnvironment.insert(key, value);
    };
    preserve("language", currentEnvironment.language);
    preserve("framework", currentEnvironment.framework);
    preserve("compiler", currentEnvironment.compiler);
    preserve("buildSystem", currentEnvironment.buildSystem);
    preserve("packageManager", currentEnvironment.packageManager);
    for (const auto& key : {"projectId", "projectName", "projectPath", "projectFilePath"}) root.insert(key, previous.value(key));
    if (!selected.first().userDefined) root.insert("resources", previous.value("resources"));
    root.insert("projectId", previous.value("projectId"));
    root.insert("projectName", previous.value("projectName"));
    root.insert("projectPath", previous.value("projectPath"));
    root.insert("projectFilePath", previous.value("projectFilePath"));
    const QString composedType = selected.size() == 1 ? selected.first().projectType : QString();
    root.insert("templateId", selected.size() == 1 ? selected.first().id : QStringLiteral("composed"));
    root.insert("templateModules", QJsonArray::fromStringList(moduleIds));
    root.insert("templateState", QJsonObject{{"name", selected.size() == 1 ? selected.first().displayName : QStringLiteral("Composed project modules")},
                                               {"projectType", composedType}, {"modules", QJsonArray::fromStringList(moduleIds)},
                                               {"baseline", root},
                                               {"manualBaseline", manualBaseline.isEmpty() ? (root == previous ? previous : QJsonObject{}) : manualBaseline},
                                               {"provenance", provenance}, {"catalogFingerprint", TemplateValidation::catalogFingerprint()}});
    model->beginUpdate();
    ProjectPersistence().fromJson(model, root);
    // Loading a template establishes a new baseline; derived environment
    // values must not be mistaken for user overrides by the model setter.
    model->clearEnvironmentOverrides();
    model->setTemplateModules(effectiveIds);
    if (moduleIds.size() == 1) model->setTemplateId(moduleIds.first());
    if (!composedType.isEmpty()) model->setContext(composedType);
    model->resolveAndroidConstraints();
    if (!preservedEnvironment.isEmpty()) {
        auto environment = model->developmentEnvironment();
        if (preservedEnvironment.contains("language")) environment.language = preservedEnvironment.value("language");
        if (preservedEnvironment.contains("framework")) environment.framework = preservedEnvironment.value("framework");
        if (preservedEnvironment.contains("compiler")) environment.compiler = preservedEnvironment.value("compiler");
        if (preservedEnvironment.contains("buildSystem")) environment.buildSystem = preservedEnvironment.value("buildSystem");
        if (preservedEnvironment.contains("packageManager")) environment.packageManager = preservedEnvironment.value("packageManager");
        model->setDevelopmentEnvironment(environment);
    }
    model->setModified(true);
    model->endUpdate();
    return true;
}

bool TemplateManager::saveCustomTemplate(const ProjectModel& model, const QString& name, QString* id, QString* error)
{
    if (name.trimmed().isEmpty()) { if (error) *error = "Enter a template name."; return false; }
    auto savedConfiguration = ProjectPersistence().configuration(model);
    savedConfiguration.insert(QStringLiteral("templateModules"), QJsonArray::fromStringList(model.templateModules()));
    // A reusable template is a configuration fragment, so project-instance
    // identity and full Generate readiness are deliberately excluded.
    for (const auto& key : {QStringLiteral("projectId"), QStringLiteral("projectName"),
                            QStringLiteral("projectPath"), QStringLiteral("projectFilePath"),
                            QStringLiteral("templateId"), QStringLiteral("templateState")})
        savedConfiguration.remove(key);
    if (savedConfiguration.isEmpty() || model.templateModules().isEmpty()) {
        if (error) *error = "Select at least one project module before saving a template.";
        return false;
    }
    auto library = readLibrary(libraryPath_, error);
    if (library.isEmpty()) return false;
    for (const auto& d : definitions()) if (d.displayName.compare(name.trimmed(), Qt::CaseInsensitive) == 0) {
        if (error) *error = "A template with this name already exists. Choose a different name.";
        return false;
    }
    const QString newId = "user-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto entries = library.value("templates").toArray();
    entries.append(QJsonObject{{"id", newId}, {"name", name.trimmed()}, {"configuration", savedConfiguration}, {"catalogFingerprint", TemplateValidation::catalogFingerprint()}});
    library.insert("templates", entries);
    if (!writeLibrary(libraryPath_, library, error)) return false;
    if (id) *id = newId;
    emit templatesChanged();
    return true;
}

bool TemplateManager::removeCustomTemplate(const QString& id, QString* error)
{
    if (!id.startsWith("user-")) { if (error) *error = "Built-in templates are protected."; return false; }
    auto library = readLibrary(libraryPath_, error);
    if (library.isEmpty()) return false;
    auto entries = library.value("templates").toArray();
    for (int i = 0; i < entries.size(); ++i) if (entries[i].toObject().value("id").toString() == id) {
        entries.removeAt(i); library.insert("templates", entries);
        if (!writeLibrary(libraryPath_, library, error)) return false;
        emit templatesChanged(); return true;
    }
    if (error) *error = "Custom template was not found.";
    return false;
}
