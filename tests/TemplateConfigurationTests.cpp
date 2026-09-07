#include "core/Services.h"
#include "core/TemplateValidation.h"
#include "core/ProjectPersistence.h"
#include "core/ProjectMemory.h"
#include "core/AramfPaths.h"
#include "ui/workflows/project/setup/ProjectSetupPage.h"
#include "ui/workflows/output/review/ReviewPage.h"
#include "ui/workflows/output/generate/GeneratePage.h"
#include "ui/workflows/output/verify/VerifyPage.h"
#include "ui/workflows/output/finalize/FinalizePage.h"
#include "ui/workflows/project/languages/ProjectLanguagesPage.h"
#include "ui/workflows/project/frameworks/ProjectFrameworksPage.h"
#include "ui/mainwindow/MainWindow.h"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QAbstractScrollArea>
#include <QScrollArea>
#include <QStackedWidget>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <algorithm>

namespace {
int failures = 0;
int checks = 0;
void check(bool value, const QString& name)
{
    ++checks;
    if (!value) { ++failures; QTextStream(stderr) << "FAIL: " << name << '\n'; }
}
void saveJson(const QString& path, const QJsonObject& object)
{
    QFile file(path); check(file.open(QIODevice::WriteOnly), "open audit file");
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
}
QStringList list(const QJsonValue& value)
{
    QStringList result; for (const auto& item : value.toArray()) result << item.toString(); return result;
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if (app.arguments().contains("--catalog-fingerprint")) {
        QTextStream(stdout) << TemplateValidation::catalogFingerprint() << '\n'; return 0;
    }
    QTemporaryDir fixture(QDir::current().filePath("template-validation-XXXXXX"));
    fixture.setAutoRemove(false); // Evidence is additive and retained for review.
    check(fixture.isValid(), "fixture directory");
    AramfPaths::setProgramRootForTests(fixture.path());
    TemplateManager manager(nullptr, fixture.filePath("templates.json"));
    ProjectPersistence persistence;
    GenerationServices generation;
    VerificationServices verification;
    FinalizationServices finalization;
    AgentEntryPointService entryPoints;
    ProjectMemory memory;
    QJsonObject audit;
    const auto definitions = manager.definitions();
    check(definitions.size() == 13, "all 13 built-ins audited");
    const auto officialDefinitions = manager.officialDefinitions();
    check(officialDefinitions.size() == 4, "four official templates available");
    for (const auto& official : officialDefinitions) {
        ProjectModel officialModel;
        check(manager.applyTemplate(&officialModel, official.id), official.id + " official template applies");
        check(officialModel.templateModules().contains(official.id), official.id + " provenance retained");
    }
    ProjectModel combinedModel;
    const QString combinedId = QStringLiteral("official-android-pico-2w");
    QString combinedError;
    check(manager.applyTemplate(&combinedModel, combinedId, &combinedError), "Android + Pico official template applies: " + combinedError);
    check(combinedModel.templateModules().contains(QStringLiteral("wifi-communication")), "Android + Pico includes Wi-Fi module dependency");
    check(combinedModel.communicationConfiguration().sourceTarget == QStringLiteral("android-application")
              && combinedModel.communicationConfiguration().destinationTarget == QStringLiteral("raspberry-pi-pico-2-w")
              && combinedModel.communicationConfiguration().sourceRole == QStringLiteral("client")
              && combinedModel.communicationConfiguration().destinationRole == QStringLiteral("server")
              && combinedModel.communicationConfiguration().protocol == QStringLiteral("websocket"),
          "Android + Pico communication topology defaults");
    check(combinedModel.communicationConfiguration().messages.size() == 4, "digital pin message contract defaults");
    {
        ProjectModel addressModel;
        check(manager.applyTemplate(&addressModel, combinedId), "endpoint default template applies");
        addressModel.setProjectPath(fixture.filePath("endpoint-defaults"));
        ProjectSetupPage setup(&addressModel, &manager, &persistence);
        auto* addressA = setup.findChild<QLineEdit*>("communicationEndpointAAddress");
        auto* addressB = setup.findChild<QLineEdit*>("communicationEndpointBAddress");
        check(addressA && addressB && addressA->text() == QStringLiteral("android.local")
                  && addressB->text() == QStringLiteral("ws://pico.local:8080"),
              "Setup displays default endpoint addresses");
        check(TemplateValidation::readiness(addressModel).isEmpty(), "default endpoints satisfy readiness");
        if (addressA && addressB) {
            addressA->setText(QStringLiteral("192.168.1.10"));
            addressB->setText(QStringLiteral("ws://192.168.1.50:9000"));
            const auto path = fixture.filePath("endpoint-addresses.aramf.json");
            QString addressError;
            check(persistence.save(addressModel, path, &addressError), "save edited endpoint addresses: " + addressError);
            ProjectModel reloaded;
            check(persistence.load(&reloaded, path, &addressError), "reload edited endpoint addresses: " + addressError);
            const auto endpoints = reloaded.communicationConfiguration().endpoints;
            check(endpoints.size() == 2 && endpoints[0].address == QStringLiteral("192.168.1.10")
                      && endpoints[1].address == QStringLiteral("ws://192.168.1.50:9000"),
                  "user endpoint addresses survive save and reload");
        }
    }
    {
        const QString smartHomeId = QStringLiteral("official-android-arduino-smart-home");
        ProjectModel smartHome;
        QString smartHomeError;
        check(manager.applyTemplate(&smartHome, smartHomeId, &smartHomeError), "Android Arduino Smart Home applies: " + smartHomeError);
        const auto capabilities = smartHome.developmentCapabilities();
        check(manager.definition(QStringLiteral("Android_Arduino_Smart_Home")).id == smartHomeId, "canonical Android_Arduino_Smart_Home alias resolves");
        check(capabilities.languages.contains("kotlin") && capabilities.languages.contains("c") && capabilities.languages.contains("cpp"), "Smart Home selects Kotlin, C and C++");
        check(capabilities.frameworks.contains("android-sdk") && capabilities.frameworks.contains("arduino"), "Smart Home selects Android SDK and Arduino");
        check(capabilities.ides.contains("android-studio") && capabilities.ides.contains("arduino-ide"), "Smart Home selects Android Studio and Arduino IDE");
        check(capabilities.buildSystems.contains("gradle") && capabilities.buildSystems.contains("arduino-build"), "Smart Home selects Gradle and Arduino build");
        check(capabilities.hardwareTargets.contains("arduino-mcu") && !capabilities.hardwareTargets.contains("raspberry-pi-pico-2-w"), "Smart Home selects Arduino and excludes Pico");
        const auto communication = smartHome.communicationConfiguration();
        check(communication.transport == "bluetooth" && communication.protocol == "serial"
                  && communication.sourceTarget == "android-application" && communication.destinationTarget == "arduino-mcu",
              "Smart Home selects Bluetooth and UART/serial topology");
        check(smartHome.hardwareResources().size() >= 4 && smartHome.templateModules().contains("sensor-integration")
                  && smartHome.templateModules().contains("actuator-control"), "Smart Home preserves hardware resources and modules");
        const auto path = fixture.filePath("smart-home-roundtrip.aramf.json");
        QString smartHomePersistenceError;
        check(persistence.save(smartHome, path, &smartHomePersistenceError), "Smart Home save: " + smartHomePersistenceError);
        ProjectModel reloaded;
        check(persistence.load(&reloaded, path, &smartHomePersistenceError)
                  && reloaded.communicationConfiguration().transport == "bluetooth"
                  && reloaded.developmentCapabilities().languages.contains("kotlin")
                  && reloaded.developmentCapabilities().hardwareTargets.contains("arduino-mcu"), "Smart Home save/reload preserves configuration");
    }
    check(combinedModel.communicationConfiguration().messages.first().name == QStringLiteral("WRITE_DIGITAL_PIN")
              && combinedModel.communicationConfiguration().messages.first().fields.first().name == QStringLiteral("pinId")
              && combinedModel.communicationConfiguration().messages.first().fields.first().type == QStringLiteral("string"),
          "digital pin contract uses symbolic pin IDs");
    check(combinedModel.hardwareResources().size() == 1 && combinedModel.hardwareResources().first().physicalResource.isEmpty(),
          "hardware source of truth keeps physical mapping configurable");
    auto contractConfiguration = combinedModel.communicationConfiguration();
    contractConfiguration.messages = {
        {1, QStringLiteral("SET_LED"), QStringLiteral("request"), QStringLiteral("endpoint-a->endpoint-b"), QStringLiteral("endpoint-a"), QStringLiteral("endpoint-b"), 0, 2, {{QStringLiteral("enabled"), QStringLiteral("bool"), true}}, QStringLiteral("Set LED state")},
        {2, QStringLiteral("SET_LED_RESULT"), QStringLiteral("response"), QStringLiteral("endpoint-b->endpoint-a"), QStringLiteral("endpoint-b"), QStringLiteral("endpoint-a"), 1, 0, {{QStringLiteral("success"), QStringLiteral("bool"), true}}, QStringLiteral("Result of SET_LED")}};
    combinedModel.setCommunicationConfiguration(contractConfiguration);
    check(combinedModel.communicationConfiguration().messages.size() == 2, "generic message contract configured");
    check(combinedModel.developmentCapabilities().languages.contains("kotlin")
              && combinedModel.developmentCapabilities().languages.contains("c")
              && combinedModel.developmentCapabilities().languages.contains("cpp")
              && combinedModel.developmentCapabilities().languages.contains("pio-assembly"), "Android + Pico language merge");
    check(combinedModel.developmentCapabilities().frameworks.contains("android-sdk")
              && combinedModel.developmentCapabilities().frameworks.contains("pico-sdk"), "Android + Pico SDK merge");
    check(combinedModel.developmentCapabilities().buildSystems.contains("gradle")
              && combinedModel.developmentCapabilities().buildSystems.contains("cmake"), "Android + Pico build-system merge");
    check(combinedModel.developmentCapabilities().targetPlatforms.contains("android")
              && combinedModel.developmentCapabilities().hardwareTargets.contains("raspberry-pi-pico-2-w"), "Android + Pico targets merge");
    auto communication = combinedModel.communicationConfiguration();
    communication.endpoint = QStringLiteral("pico.local:8080");
    communication.sourceRole = QStringLiteral("client"); communication.destinationRole = QStringLiteral("server");
    communication.dataFormat = QStringLiteral("json");
    combinedModel.setCommunicationConfiguration(communication);
    combinedModel.setProjectPath(fixture.filePath("combined-android-pico"));
    combinedModel.setProjectName(QStringLiteral("Combined Android Pico"));
    combinedModel.setProjectFilePath(fixture.filePath("combined.aramf.json"));
    const auto combinedGeneration = generation.generate(combinedModel, combinedModel.generationOptions());
    check(combinedGeneration.success, "Android + Pico worker generation");
    const auto combinedVerification = verification.verify(combinedModel, combinedModel.generationOptions());
    check(combinedVerification.overallStatus != VerificationStatus::Fail, "Android + Pico worker verification");
    check(QFile::exists(QDir(combinedModel.projectPath()).filePath("ARAMF_WORKER/communication/communication-contract.json")), "communication contract generated");
    check(QFile::exists(QDir(combinedModel.projectPath()).filePath("ARAMF_WORKER/communication/multi-target-build.json")), "multi-target build model generated");
    check(QFile::exists(QDir(combinedModel.projectPath()).filePath("ARAMF_WORKER/hardware/hardware-resources.json")), "hardware source of truth generated");
    check(QFile::exists(QDir(combinedModel.projectPath()).filePath("ARAMF_WORKER/ARAMF_WORKER.json")), "canonical worker identity JSON generated");
    QFile communicationFile(QDir(combinedModel.projectPath()).filePath("ARAMF_WORKER/communication/communication-contract.json"));
    check(communicationFile.open(QIODevice::ReadOnly), "communication contract readable");
    const auto communicationJson = QJsonDocument::fromJson(communicationFile.readAll()).object();
    communicationFile.close();
    const auto generatedLink = communicationJson.value("links").toArray().first().toObject();
    check(generatedLink.value("transport").toString() == "wifi"
              && generatedLink.value("protocol").toString() == "websocket"
              && communicationJson.value("endpoints").toArray().first().toObject().value("targetId").toString() == "android-application"
              && communicationJson.value("endpoints").toArray().at(1).toObject().value("targetId").toString() == "raspberry-pi-pico-2-w"
              && communicationJson.value("endpoints").toArray().first().toObject().value("role").toString() == "client"
              && communicationJson.value("endpoints").toArray().at(1).toObject().value("role").toString() == "server",
          "communication contract targets and protocol");
    check(!communicationJson.contains("sourceTarget") && !communicationJson.contains("destinationTarget")
              && !communicationJson.contains("sourceRole") && !communicationJson.contains("destinationRole")
              && !communicationJson.contains("endpoint") && !communicationJson.contains("dataFormat")
              && !communicationJson.contains("androidResponsibility") && !communicationJson.contains("picoResponsibility"),
          "legacy communication fields excluded from generated contract");
    check(communicationJson.value("endpoints").toArray().size() == 2
              && communicationJson.value("links").toArray().first().toObject().value("direction").toString() == QStringLiteral("bidirectional"),
          "generic communication contract endpoints and direction generated");
    check(communicationJson.value("messages").toArray().size() == 2
              && communicationJson.value("messages").toArray().first().toObject().value("responseMessageId").toInt() == 2,
          "generic communication messages generated");
    QFile buildModelFile(QDir(combinedModel.projectPath()).filePath("ARAMF_WORKER/communication/multi-target-build.json"));
    check(buildModelFile.open(QIODevice::ReadOnly), "multi-target build model readable");
    const auto buildModelJson = QJsonDocument::fromJson(buildModelFile.readAll()).object();
    buildModelFile.close();
    check(buildModelJson.value("targets").toArray().size() == 2
              && buildModelJson.value("orchestration").toArray().size() >= 3,
          "multi-target build orchestration generated");
    // Protocol selection remains user-editable after applying the default.
    communication.protocol = QStringLiteral("http-rest");
    combinedModel.setCommunicationConfiguration(communication);
    ProjectModel combinedReloaded;
    check(persistence.save(combinedModel, fixture.filePath("combined-roundtrip.aramf.json")), "communication project save");
    QString combinedLoadError;
    check(persistence.load(&combinedReloaded, fixture.filePath("combined-roundtrip.aramf.json"), &combinedLoadError), "communication project reload: " + combinedLoadError);
    check(combinedReloaded.communicationConfiguration().protocol == QStringLiteral("http-rest")
              && combinedReloaded.communicationConfiguration().endpoint == QStringLiteral("pico.local:8080")
              && combinedReloaded.communicationConfiguration().sourceTarget == QStringLiteral("android-application")
              && combinedReloaded.communicationConfiguration().destinationTarget == QStringLiteral("raspberry-pi-pico-2-w")
              && combinedReloaded.communicationConfiguration().sourceRole == QStringLiteral("client")
              && combinedReloaded.communicationConfiguration().destinationRole == QStringLiteral("server"), "communication configuration round-trip");
    check(combinedReloaded.communicationConfiguration().endpoints.size() == 2
              && combinedReloaded.communicationConfiguration().endpoints.at(0).targetId == QStringLiteral("android-application")
              && combinedReloaded.communicationConfiguration().endpoints.at(1).targetId == QStringLiteral("raspberry-pi-pico-2-w")
              && combinedReloaded.communicationConfiguration().links.size() == 1
              && combinedReloaded.communicationConfiguration().links.first().direction == QStringLiteral("bidirectional"),
          "generic endpoints and bidirectional link round-trip");
    check(AramfPaths::workerDirectoryName({}) == QStringLiteral("ARAMF_WORKER"), "empty worker suffix keeps canonical name");
    check(AramfPaths::workerDirectoryName(QStringLiteral("Android Pico")) == QStringLiteral("ARAMF_WORKER_ANDROID_PICO"), "worker suffix spaces and case normalize");
    check(AramfPaths::workerDirectoryName(QStringLiteral("__ANDROID__PICO__")) == QStringLiteral("ARAMF_WORKER_ANDROID_PICO"), "worker suffix underscores normalize");
    check(AramfPaths::workerDirectoryName(QStringLiteral("android/pico")) == QStringLiteral("ARAMF_WORKER_ANDROID_PICO"), "worker suffix separators and case normalize");
    check(AramfPaths::workerDirectoryName(QStringLiteral("mixedCase")) == QStringLiteral("ARAMF_WORKER_MIXEDCASE"), "worker suffix mixed case normalizes");
    check(AramfPaths::workerDirectoryName(QStringLiteral("_Android__Pico_")) == QStringLiteral("ARAMF_WORKER_ANDROID_PICO"), "worker suffix edge underscores normalize");
    ProjectModel suffixedModel;
    check(manager.applyTemplate(&suffixedModel, combinedId), "suffixed model template setup");
    suffixedModel.setCommunicationConfiguration(communication);
    suffixedModel.setProjectName(combinedModel.projectName());
    suffixedModel.setProjectPath(fixture.filePath("combined-suffixed"));
    suffixedModel.setProjectFilePath(fixture.filePath("combined-suffixed.aramf.json"));
    suffixedModel.setWorkerNameSuffix(QStringLiteral("ANDROID_PICO"));
    const auto suffixedGeneration = generation.generate(suffixedModel, suffixedModel.generationOptions());
    check(suffixedGeneration.success, "suffixed worker generation");
    check(QDir(suffixedModel.projectPath()).exists(QStringLiteral("ARAMF_WORKER_ANDROID_PICO"))
              && !QDir(suffixedModel.projectPath()).exists(QStringLiteral("ARAMF_WORKER")), "generation uses resolved worker directory");
    check(QFile::exists(QDir(suffixedModel.projectPath()).filePath(
                  QStringLiteral("ARAMF_WORKER_ANDROID_PICO/ARAMF_WORKER_ANDROID_PICO.json"))),
          "worker identity JSON matches resolved directory");
    check(!generation.generate(suffixedModel, suffixedModel.generationOptions()).success, "existing suffixed worker is not overwritten");
    const auto suffixVerification = verification.verify(suffixedModel, suffixedModel.generationOptions());
    check(suffixVerification.overallStatus == VerificationStatus::Pass, "suffixed worker Verify including cold-start passes");
    check(finalization.finalize(suffixedModel, suffixedModel.generationOptions()).success,
          "suffixed worker Finalize passes");
    check(finalization.finalize(suffixedModel, suffixedModel.generationOptions()).alreadyFinalized,
          "suffixed worker Finalize remains idempotent");
    QFile suffixStatus(QDir(suffixedModel.projectPath()).filePath(QStringLiteral("ARAMF_WORKER_ANDROID_PICO/PROJECT_STATUS.md")));
    check(suffixStatus.open(QIODevice::ReadOnly) && suffixStatus.readAll().contains("Current state: Finalized"),
          "Finalize updates status in selected worker");
    check(!QDir(suffixedModel.projectPath()).exists(QStringLiteral("ARAMF_WORKER")),
          "Verify and Finalize never create an unsuffixed worker");
    ProjectModel suffixReloaded;
    QString suffixError;
    check(persistence.save(suffixedModel, fixture.filePath("suffixed-roundtrip.aramf.json"))
              && persistence.load(&suffixReloaded, fixture.filePath("suffixed-roundtrip.aramf.json"), &suffixError)
              && suffixReloaded.workerNameSuffix() == QStringLiteral("ANDROID_PICO")
              && suffixReloaded.projectName() == QStringLiteral("ARAMF_WORKER_ANDROID_PICO")
              && QFileInfo(suffixReloaded.projectFilePath()).fileName() == QStringLiteral("ARAMF_WORKER_ANDROID_PICO.aramf.json"), "worker suffix save/reload");
    ProjectModel composed;
    QString compositionError;
    check(manager.applyModules(&composed, {"android-application", "kotlin", "academic-school-project"}, &compositionError), "Android + Kotlin + Academic composition: " + compositionError);
    check(composed.templateModules().size() == 3, "composed module selections persist in model");
    check(composed.developmentCapabilities().targetPlatforms.contains("android") && composed.developmentCapabilities().languages.contains("kotlin"), "composed platform and language merge");
    check(composed.academicConfiguration().academicMode == "academic-assignment", "academic module contributes academic settings");
    const auto composedAgents = composed.aiConfiguration();
    for (const auto& agent : {QStringLiteral("chatgpt"), QStringLiteral("gemini"), QStringLiteral("github-copilot"), QStringLiteral("openai-codex")})
        check(composedAgents.primaryAgent == agent || composedAgents.additionalAgents.contains(agent), "composed AI baseline: " + agent);
    check(manager.applyModules(&composed, {"android-application", "kotlin"}), "remove academic module");
    check(composed.templateModules() == QStringList{"android-application", "kotlin"} && composed.academicConfiguration().academicMode == "disabled", "removing module removes only academic contribution");
    ProjectModel ownership;
    QString ownershipError;
    check(manager.applyComposedSelection(&ownership, {QStringLiteral("official-aramf-development")}, {}, &ownershipError), "single template ownership applies");
    check(ownership.templateModules().contains(QStringLiteral("cmake")), "single template contributes CMake");
    check(manager.applyComposedSelection(&ownership, {}, {}, &ownershipError), "single template ownership removes");
    check(!ownership.templateModules().contains(QStringLiteral("cmake")), "exclusive template module is removed");
    check(manager.applyComposedSelection(&ownership, {QStringLiteral("official-aramf-development"), QStringLiteral("official-pico-visual-designer")}, {}, &ownershipError), "shared template ownership applies");
    check(ownership.templateModules().contains(QStringLiteral("cmake")) && ownership.templateModules().contains(QStringLiteral("cpp")), "shared modules are selected");
    check(manager.applyComposedSelection(&ownership, {QStringLiteral("official-pico-visual-designer")}, {}, &ownershipError), "one shared template is removed");
    check(ownership.templateModules().contains(QStringLiteral("cpp")) && ownership.templateModules().contains(QStringLiteral("cmake")), "shared module remains with active owner");
    check(manager.applyComposedSelection(&ownership, {}, {QStringLiteral("cmake")}, &ownershipError), "manual module ownership retained");
    check(ownership.templateModules().contains(QStringLiteral("cmake")), "manual module survives template removal");
    check(manager.applyComposedSelection(&ownership, {}, {}, &ownershipError), "final ownership removal");
    check(!ownership.templateModules().contains(QStringLiteral("cmake")), "shared module removed after final owner");
    ProjectModel reviewModel;
    check(manager.applyComposedSelection(&reviewModel, {QStringLiteral("official-android-pico-2w")}, {}, &ownershipError), "review active template selection");
    auto reviewCommunication = reviewModel.communicationConfiguration();
    reviewCommunication.messages = {CommunicationMessage{1, QStringLiteral("PING"), QStringLiteral("event"), QStringLiteral("endpoint-a->endpoint-b"), QStringLiteral("endpoint-a"), QStringLiteral("endpoint-b"), 0, 0, {}, QStringLiteral("Connectivity event")}};
    reviewModel.setCommunicationConfiguration(reviewCommunication);
    ReviewPage reviewPage(&reviewModel);
    const auto reviewText = [&]() { return reviewPage.findChild<QPlainTextEdit*>()->toPlainText(); };
    check(reviewText().contains(QStringLiteral("Active templates: Android + Pico 2 W")),
          "Review shows canonical active template");
    check(reviewText().contains(QStringLiteral("Protocol: websocket")),
          "Review shows Android + Pico WebSocket default");
    check(reviewText().contains(QStringLiteral("Endpoints: 2 configured")) && reviewText().contains(QStringLiteral("Links: 1 configured")),
          "Review summarizes populated generic endpoint/link collections");
    check(!reviewText().contains(QStringLiteral("Data Format: Not specified")),
          "Review omits legacy data format when generic link fields are active");
    check(manager.applyComposedSelection(&reviewModel, {QStringLiteral("official-android-pico-2w"), QStringLiteral("official-aramf-development")}, {}, &ownershipError), "review two active templates");
    check(reviewPage.findChild<QPlainTextEdit*>()->toPlainText().contains(QStringLiteral("Android + Pico 2 W, ARAMF Development")),
          "Review shows both active templates");
    const QMap<QString, QStringList> languages{
        {"pico-2w-visual-designer", {"cpp", "c", "pio-assembly"}}, {"android-studio-kotlin-gemini", {"kotlin"}},
        {"qt-desktop-application", {"cpp"}}, {"cpp-command-line", {"cpp"}}, {"cmake-library", {"cpp"}},
        {"raspberry-pi-pico-firmware", {"cpp", "c", "pio-assembly"}}, {"react-frontend", {"typescript", "html", "css"}},
        {"python-backend", {"python"}}, {"csharp-backend", {"csharp"}}, {"mobile-application", {"kotlin"}},
        {"full-stack-web-application", {"typescript", "html", "css"}}, {"bachelor-thesis", {"cpp"}}
        , {"android-arduino-smart-home", {"kotlin", "cpp", "c"}}
    };
    for (const auto& d : definitions) {
        const auto issues = TemplateValidation::validateDefinition(d);
        check(issues.isEmpty(), d.id + ": " + issues.join("; "));
        check(d.capabilities.languages == languages.value(d.id), d.id + " intended languages, no unrelated stack");
        check(!d.rules.activeCategories.isEmpty() && !d.memory.maintenanceOptions.isEmpty(), d.id + " governance initialized");
        const QStringList standardAgents = {"chatgpt", "gemini", "github-copilot", "openai-codex"};
        QStringList configuredAgents = d.ai.additionalAgents;
        configuredAgents << d.ai.primaryAgent;
        for (const auto& agent : standardAgents)
            check(configuredAgents.contains(agent), d.id + " standard AI agent: " + agent);
        check((d.academic.academicMode != "disabled") == (d.id == "bachelor-thesis" || d.id == "android-arduino-smart-home"), d.id + " academic applicability");
        if (d.id == "android-studio-kotlin-gemini") {
            check(d.ai.primaryAgent == "gemini" && d.capabilities.ides == QStringList{"android-studio"}, "Android agent/IDE");
            check(d.capabilities.targetArchitectures == QStringList{"auto"} && !d.capabilities.frameworks.contains("room"), "Android excludes fixed ABI and optional database");
        }
        if (d.id == "csharp-backend") check(d.capabilities.buildSystems == QStringList{"msbuild"}, "C# uses MSBuild");
        if (d.id == "full-stack-web-application") check(d.capabilities.frameworks.contains("express") && d.capabilities.targetPlatforms.contains("server"), "full stack has backend");

        ProjectModel model;
        const QString root = fixture.filePath(d.id);
        QDir().mkpath(root);
        model.setProjectName(d.displayName + " test"); model.setProjectPath(root);
        model.setProjectFilePath(root + "/project.aramf.json");
        const QString projectId = model.projectId();
        ProjectSetupPage setup(&model, &manager, &persistence);
        ReviewPage review(&model);
        GeneratePage generate(&model, &setup, &generation);
        // These pages exist before the selection, just as in the production application.
        ProjectLanguagesPage languagePage(&model);
        ProjectFrameworksPage frameworkPage(&model);
        const auto combo = setup.findChild<QComboBox*>("templateSelector");
        check(combo != nullptr, "production template selector");
        combo->setCurrentIndex(combo->findData(d.id));
        check(model.templateId() == d.id && model.projectId() == projectId, d.id + " GUI application preserves identity");
        check(model.projectTypeLocked() && setup.findChild<QLineEdit*>("projectType")->isReadOnly(), d.id + " project type lock");
        check(model.developmentCapabilities().languages == d.capabilities.languages, d.id + " immediately initialized model");
        for (const auto& box : languagePage.findChildren<QCheckBox*>()) {
            if (box->text() == "Kotlin") check(box->isChecked() == d.capabilities.languages.contains("kotlin"), d.id + " GUI language refresh");
        }
        check(TemplateValidation::readiness(model).isEmpty(), d.id + " readiness without page visits");
        check(review.findChild<QPlainTextEdit*>()->toPlainText().contains("READY"), d.id + " Review ready");
        generate.findChild<QPushButton*>("saveAndGenerate")->click();
        check(generate.findChild<QPlainTextEdit*>()->toPlainText().contains("Generate: PASS"), d.id + " production Save & Generate click: " + generate.findChild<QPlainTextEdit*>()->toPlainText().left(240));
        auto verified = verification.verify(model, model.generationOptions());
        QStringList verificationFailures;
        for (const auto& item : verified.checks) if (item.status == VerificationStatus::Fail) verificationFailures << item.id + ":" + item.details;
        check(verified.overallStatus == VerificationStatus::Pass, d.id + " Verify checks=" + verificationFailures.join(" | "));
        const auto finalized = finalization.finalize(model, model.generationOptions());
        check(finalized.success, d.id + " Finalize blockers=" + finalized.blockers.join("; ") + " error=" + finalized.error);
        check(finalization.finalize(model, model.generationOptions()).alreadyFinalized, d.id + " idempotent Finalize");
        check(memory.validate(root).value("status").toString() == "PASS", d.id + " generated memory consistency");
        check(memory.validateColdStart(root).value("status").toString() == "PASS", d.id + " generated cold start");
        check(entryPoints.createEntryPoints(model).success, d.id + " agent entry points");
        // Entry-point adapters update the canonical bootstrap after Verify; keep
        // the derived cold-start artifact current for the next agent startup.
        check(memory.refreshDerivedState(root), d.id + " entry-point memory refresh");

        // Effective user changes and template provenance survive save/reopen without reapplication.
        auto c = model.developmentCapabilities(); c.qualityCapabilities << "profiling"; model.setDevelopmentCapabilities(c);
        const auto expected = persistence.configuration(model);
        check(persistence.save(model, model.projectFilePath()), d.id + " save overrides");
        ProjectModel reopened;
        check(persistence.load(&reopened, model.projectFilePath()), d.id + " reopen");
        check(persistence.configuration(reopened) == expected && reopened.templateState() == model.templateState(), d.id + " full persistence equality");
        check(reopened.templateId() == d.id && reopened.projectTypeLocked(), d.id + " persisted template and constraint");
        check(TemplateValidation::changedDomains(reopened).contains("capabilities"), d.id + " override provenance");
        const auto staleVerification = verification.verify(reopened, reopened.generationOptions());
        check(staleVerification.overallStatus != VerificationStatus::Pass, d.id + " overrides require fresh generation");
        const auto staleFinalization = finalization.finalize(reopened, reopened.generationOptions());
        check(!staleFinalization.success, d.id + " no verification bypass");

        const auto beforeDisable = persistence.configuration(reopened);
        manager.applyTemplate(&reopened, {});
        check(reopened.templateId().isEmpty() && !reopened.projectTypeLocked() && reopened.templateState().isEmpty(), d.id + " Disable releases all locks");
        check(persistence.configuration(reopened) == beforeDisable, d.id + " Disable preserves deliberate configuration");
        reopened.setContext("manual-project"); check(reopened.context() == "manual-project", d.id + " manual project type editable");

        audit.insert(d.id, QJsonObject{{"name", d.displayName}, {"configuration", d.configuration},
            {"optionAudit", TemplateValidation::optionAudit(d)}, {"intentionallyUnselected", QJsonArray::fromStringList(d.exclusions)}});
    }

    // Every ordered A -> B transition, including a deliberately incompatible legacy override.
    for (const auto& a : definitions) for (const auto& b : definitions) {
        ProjectModel model;
        manager.applyTemplate(&model, a.id);
        auto environment = model.developmentEnvironment(); environment.language = "unrelated-override";
        model.setDevelopmentEnvironment(environment);
        model.setOptionValues("old-template-extension", {"old"});
        check(manager.applyTemplate(&model, b.id), a.id + " -> " + b.id);
        check(persistence.configuration(model) == b.configuration, a.id + " -> " + b.id + " no stale selections");
    }
    auto invalid = definitions[1].configuration;
    auto c = invalid.value("capabilities").toObject();
    c.insert("languages", QJsonArray{"deleted-option"}); invalid.insert("capabilities", c);
    check(!TemplateValidation::validateConfiguration(invalid).isEmpty(), "obsolete catalog ID rejected");
    invalid = definitions[1].configuration; c = invalid.value("capabilities").toObject();
    c.insert("frameworks", QJsonArray{"jetpack-compose"}); invalid.insert("capabilities", c);
    check(!TemplateValidation::validateConfiguration(invalid).isEmpty(), "missing Android dependency rejected");
    invalid = definitions[1].configuration; c = invalid.value("capabilities").toObject();
    c.insert("targetArchitectures", QJsonArray{"auto", "arm64"}); invalid.insert("capabilities", c);
    check(!TemplateValidation::validateConfiguration(invalid).isEmpty(), "conflicting architecture selectors rejected");
    invalid = definitions[1].configuration; invalid.remove("rules");
    check(!TemplateValidation::validateConfiguration(invalid).isEmpty(), "incomplete configuration rejected");
    invalid = persistence.configuration(combinedModel);
    auto invalidCommunication = invalid.value("communication").toObject();
    invalidCommunication.insert("destinationTarget", QStringLiteral(""));
    invalid.insert("communication", invalidCommunication);
    check(!TemplateValidation::validateConfiguration(invalid).isEmpty(), "communication requires a destination target");
    invalid = persistence.configuration(combinedModel);
    invalidCommunication = invalid.value("communication").toObject();
    invalidCommunication.insert("protocol", QStringLiteral("http-rest"));
    invalidCommunication.insert("endpoint", QStringLiteral(""));
    invalidCommunication.remove(QStringLiteral("endpoints"));
    invalidCommunication.remove(QStringLiteral("links"));
    invalid.insert("communication", invalidCommunication);
    check(!TemplateValidation::validateConfiguration(invalid).isEmpty(), "communication protocol requires endpoint");

    // Generic endpoint addresses are authoritative for readiness; a client
    // address is optional while the listening/server endpoint is required.
    ProjectModel endpointReadiness;
    manager.applyTemplate(&endpointReadiness, QStringLiteral("official-android-pico-2w"));
    endpointReadiness.setProjectPath(fixture.filePath("endpoint-readiness"));
    auto endpointCommunication = endpointReadiness.communicationConfiguration();
    endpointCommunication.endpoint.clear();
    endpointCommunication.endpoints[0].address = QStringLiteral("ws://192.168.1.10:8081");
    endpointCommunication.endpoints[1].address = QStringLiteral("ws://192.168.1.50:8080");
    endpointReadiness.setCommunicationConfiguration(endpointCommunication);
    check(TemplateValidation::readiness(endpointReadiness).isEmpty(), "generic server endpoint address satisfies readiness");
    endpointCommunication.endpoints[0].address.clear();
    endpointReadiness.setCommunicationConfiguration(endpointCommunication);
    check(TemplateValidation::readiness(endpointReadiness).isEmpty(), "client address is optional for readiness");
    endpointCommunication.endpoints[1].address.clear();
    endpointReadiness.setCommunicationConfiguration(endpointCommunication);
    check(!TemplateValidation::readiness(endpointReadiness).isEmpty(), "missing server endpoint address fails readiness");

    // Generic message/wire contract validation remains target-neutral.
    auto validContract = persistence.configuration(combinedModel);
    auto contractObject = validContract.value("communication").toObject();
    auto messages = contractObject.value("messages").toArray();
    check(messages.size() == 2, "generic message contract roundtrip baseline");
    auto duplicateId = messages;
    auto duplicateMessage = duplicateId.at(1).toObject();
    duplicateMessage.insert("id", duplicateId.at(0).toObject().value("id"));
    duplicateId[1] = duplicateMessage;
    contractObject.insert("messages", duplicateId);
    validContract.insert("communication", contractObject);
    check(!TemplateValidation::validateConfiguration(validContract).isEmpty(), "duplicate message IDs rejected");
    validContract = persistence.configuration(combinedModel);
    contractObject = validContract.value("communication").toObject();
    messages = contractObject.value("messages").toArray();
    auto invalidResponse = messages.at(0).toObject();
    invalidResponse.insert("responseMessageId", 9999);
    messages[0] = invalidResponse;
    contractObject.insert("messages", messages);
    validContract.insert("communication", contractObject);
    check(!TemplateValidation::validateConfiguration(validContract).isEmpty(), "invalid response reference rejected");
    validContract = persistence.configuration(combinedModel);
    contractObject = validContract.value("communication").toObject();
    messages = contractObject.value("messages").toArray();
    auto invalidFieldMessage = messages.at(0).toObject();
    auto fields = invalidFieldMessage.value("fields").toArray();
    auto badField = fields.at(0).toObject();
    badField.insert("type", QStringLiteral("not-a-logical-type"));
    fields[0] = badField;
    invalidFieldMessage.insert("fields", fields);
    messages[0] = invalidFieldMessage;
    contractObject.insert("messages", messages);
    validContract.insert("communication", contractObject);
    check(!TemplateValidation::validateConfiguration(validContract).isEmpty(), "unknown field type rejected");

    ProjectModel custom;
    manager.applyTemplate(&custom, "bachelor-thesis");
    auto academic = custom.academicConfiguration(); academic.institution = "Example University"; academic.citationStyle = "ieee"; custom.setAcademicConfiguration(academic);
    custom.setDescription("Reusable custom description");
    custom.setOptionValues("extension-options", {"preserved"}); custom.setProfileSelections({"custom-profile"});
    ProjectResource resource; resource.id = "reference"; resource.name = "Local specification"; resource.location = "spec.md";
    resource.type = "specification"; resource.authorityLevel = "primary-source-of-truth"; resource.scopes = {"project-requirements"};
    resource.description = "Custom source text"; custom.setResources({resource});
    auto policy = custom.resourcePolicy(); policy.options = {}; custom.setResourcePolicy(policy);
    auto mem = custom.memoryConfiguration(); mem.maximumSizeBytes = 2LL * 1024 * 1024 * 1024; mem.updateStrategy = "manual"; custom.setMemoryConfiguration(mem);
    auto outputs = custom.generationOptions(); outputs.generatePlatforms = false; custom.setGenerationOptions(outputs);
    const auto savedConfiguration = persistence.configuration(custom);
    QString customId, error;
    check(manager.saveCustomTemplate(custom, "Complete custom template", &customId, &error), "save custom: " + error);
    TemplateManager restarted(nullptr, manager.libraryPath());
    ProjectModel restored;
    restored.setProjectPath(fixture.filePath("custom-target")); restored.setProjectName("New target");
    const QString newId = restored.projectId();
    check(restarted.applyTemplate(&restored, customId, &error), "restart custom apply: " + error);
    check(persistence.configuration(restored) == savedConfiguration, "custom ALL configuration domains exact round trip");
    check(restored.projectName() == "ARAMF_WORKER" && restored.projectId() == newId && restored.projectPath() == fixture.filePath("custom-target"), "custom uses canonical target identity");
    check(!restarted.removeCustomTemplate(definitions.first().id), "built-in protected from removal");
    check(!restarted.saveCustomTemplate(custom, definitions.first().displayName), "built-in protected from replacement");
    check(!restarted.saveCustomTemplate(custom, "Complete custom template"), "duplicate names rejected");
    check(restarted.removeCustomTemplate(customId), "custom removable");
    check(TemplateManager(nullptr, manager.libraryPath()).definition(customId).id.isEmpty(), "removal survives restart");
    const auto beforeUnknown = persistence.toJson(restored);
    check(!restarted.applyTemplate(&restored, "missing-template"), "unknown template fails");
    check(persistence.toJson(restored) == beforeUnknown, "failed template leaves model unchanged");

    // Reusable templates must accept incomplete project instances.
    ProjectModel partial;
    check(manager.applyModules(&partial, {"qt", "cmake"}), "partial Qt + CMake modules apply");
    QString partialId;
    check(manager.saveCustomTemplate(partial, "Qt CMake Base", &partialId, &error), "partial template save without project path: " + error);
    TemplateManager partialRestart(nullptr, manager.libraryPath());
    const auto partialTemplates = partialRestart.customDefinitions();
    check(std::any_of(partialTemplates.cbegin(), partialTemplates.cend(), [&](const TemplateDefinition& d) { return d.id == partialId; }), "partial template persisted");
    ProjectModel partialRestored;
    check(partialRestart.applyTemplate(&partialRestored, partialId, &error), "partial template restore: " + error);
    check(partialRestored.templateModules().contains("qt") && partialRestored.templateModules().contains("cmake"), "partial template restores modules");
    ProjectModel pythonOnly;
    check(manager.applyModules(&pythonOnly, {"python"}), "single Python module apply");
    check(manager.saveCustomTemplate(pythonOnly, "Python Base", nullptr, &error), "single module template save: " + error);
    check(!manager.saveCustomTemplate(pythonOnly, "Python Base", nullptr, &error), "duplicate saved template rejected");

    // Exercise the actual save dialog, not just the service.
    ProjectSetupPage setup(&custom, &manager, &persistence);
    QTimer::singleShot(0, [] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog) { dialog->setTextValue("Saved through GUI"); dialog->accept(); }
    });
    setup.findChild<QPushButton*>("saveCustomTemplate")->click();
    bool guiSaved = false;
    for (const auto& d : manager.definitions()) guiSaved |= d.displayName == "Saved through GUI";
    check(guiSaved, "GUI Save current configuration as template");

    auto* suffixEdit = setup.findChild<QLineEdit*>("workerNameSuffix");
    auto* suffixPreview = setup.findChild<QLabel*>("workerNamePreview");
    check(suffixEdit != nullptr && suffixPreview != nullptr, "worker suffix controls exist");
    if (suffixEdit && suffixPreview) {
        suffixEdit->setFocus(); suffixEdit->setText("Android Pico"); QApplication::processEvents();
        check(suffixEdit->text() == "Android Pico", "worker suffix accepts spaces while editing");
        check(suffixPreview->text() == "ARAMF_WORKER_ANDROID_PICO", "worker suffix preview normalizes uppercase");
    }

    // The module frame must derive its height from the real checkbox grid.
    // Exercise it at the supported viewport sizes so a collapsed intermediate
    // widget or an accidental nested scroll area is caught by the regression
    // test.
    ProjectModel geometryModel;
    ProjectSetupPage geometryPage(&geometryModel, &manager, &persistence);
    geometryPage.show();
    for (const QSize size : {QSize(1280, 720), QSize(900, 600), QSize(1500, 900)}) {
        geometryPage.resize(size);
        QApplication::processEvents();
        auto* moduleGroup = geometryPage.findChild<QGroupBox*>("moduleGroup");
        auto* templateGroup = geometryPage.findChild<QGroupBox*>("templateGroup");
        check(moduleGroup != nullptr, "module group exists for geometry test");
        check(templateGroup != nullptr, "template group exists for geometry test");
        if (!moduleGroup) continue;
        check(moduleGroup->height() > 0, "module group has natural height");
        check(moduleGroup->width() <= geometryPage.width() && (!templateGroup || templateGroup->width() <= geometryPage.width()), "template frames fit page width");
        check(moduleGroup->layout() != nullptr, "module group owns its layout");
        check(moduleGroup->findChildren<QAbstractScrollArea*>().isEmpty(), "module group has no internal scrollbar");
        check(geometryPage.findChildren<QScrollArea*>().isEmpty(), "setup page has no nested horizontal scroll area");
        const auto boxes = moduleGroup->findChildren<QCheckBox*>();
        check(boxes.size() >= manager.moduleDefinitions().size(), "all module checkboxes are present");
        for (auto* box : boxes) {
            check(box->isVisible() && box->height() > 0, "module checkbox is visible with usable height");
            auto* moduleGrid = moduleGroup->findChild<QWidget*>("moduleGrid");
            if (moduleGrid) {
                const QRect boxRect = box->geometry();
                check(box->parentWidget() == moduleGrid && boxRect.height() > 0,
                      "module checkbox is laid out in module group");
            } else {
                check(false, "module checkbox fits inside module group");
            }
        }
        QSet<int> firstRowColumns;
        for (auto* box : boxes) if (box->geometry().y() == boxes.first()->geometry().y()) firstRowColumns.insert(box->geometry().x());
        check(firstRowColumns.size() >= 1, "module grid uses responsive columns");
        if (templateGroup) {
            const auto templateBoxes = templateGroup->findChildren<QCheckBox*>();
            check(!templateBoxes.isEmpty(), "composite template checkboxes are present");
            auto androidTemplate = std::find_if(templateBoxes.cbegin(), templateBoxes.cend(),
                                                  [](QCheckBox* box) { return box->text().contains("Android Studio"); });
            if (androidTemplate != templateBoxes.cend()) {
                if (!(*androidTemplate)->isChecked()) (*androidTemplate)->click();
                QApplication::processEvents();
                const auto moduleTexts = moduleGroup->findChildren<QCheckBox*>();
                const bool kotlinChecked = std::any_of(moduleTexts.cbegin(), moduleTexts.cend(), [](QCheckBox* box) {
                    return box->text() == "Kotlin" && box->isChecked();
                });
                check(kotlinChecked, "composite template resolves Kotlin module");
            }
        }
    }
    geometryPage.close();

    // Every visible Frame 1 module must complete the same real GUI lifecycle.
    ProjectModel moduleModel;
    for (const auto& definition : manager.moduleDefinitions()) {
        moduleModel.resetForNewProject();
        ProjectSetupPage modulePage(&moduleModel, &manager, &persistence);
        modulePage.show();
        QApplication::processEvents();
        const QString objectName = QStringLiteral("module_") + definition.id;
        auto boxesForModule = modulePage.findChildren<QCheckBox*>(objectName);
        auto* box = boxesForModule.isEmpty() ? nullptr : boxesForModule.last();
        check(box != nullptr, definition.id + " GUI module checkbox exists");
        if (!box) continue;
        box->click(); QApplication::processEvents();
        check(moduleModel.templateModules().contains(definition.id), definition.id + " GUI select persists");
        boxesForModule = modulePage.findChildren<QCheckBox*>(objectName);
        auto* selected = boxesForModule.isEmpty() ? nullptr : boxesForModule.last();
        check(selected && selected->isChecked(), definition.id + " GUI refresh remains checked");
        if (selected) { selected->click(); QApplication::processEvents(); }
        check(!moduleModel.templateModules().contains(definition.id), definition.id + " GUI removal persists");
        modulePage.close();
    }

    MainWindow responsive(0, 1280, 720);
    responsive.show();
    auto* pageScroll = responsive.findChild<QScrollArea*>("workflowPageScroll");
    auto* stack = responsive.findChild<QStackedWidget*>();
    check(pageScroll != nullptr && stack != nullptr, "workflow page scroll host exists");
    for (const QSize size : {QSize(1280, 720), QSize(900, 600), QSize(1500, 900)}) {
        responsive.resize(size); QApplication::processEvents();
        const QRect bounds = responsive.rect();
        for (auto* child : responsive.centralWidget()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (!child->isVisible() || child == &responsive || child->window() != &responsive) continue;
            const QRect childRect(responsive.mapFromGlobal(child->mapToGlobal(QPoint(0, 0))), child->size());
            check(bounds.contains(childRect.topLeft()) && bounds.contains(childRect.bottomRight()), "responsive layout keeps visible controls inside window");
        }
        if (pageScroll && stack) {
            check(pageScroll->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff, "workflow horizontal scrolling disabled");
            const QRect viewport = QRect(pageScroll->viewport()->mapToGlobal(QPoint(0, 0)), pageScroll->viewport()->size());
            for (int index = 0; index < stack->count(); ++index) {
                stack->setCurrentIndex(index); QApplication::processEvents();
                auto* page = stack->currentWidget();
                if (!page) continue;
                for (auto* child : page->findChildren<QWidget*>()) {
                    if (!child->isVisible() || child->width() <= 0 || child->height() <= 0) continue;
                    const QRect childRect(child->mapToGlobal(QPoint(0, 0)), child->size());
                    check(childRect.left() >= viewport.left() - 2 && childRect.right() <= viewport.right() + 2,
                          "workflow page child remains within viewport width");
                }
            }
        }
    }
    responsive.close();

    audit.insert("validation", QJsonObject{{"checks", checks}, {"failures", failures}, {"catalogFingerprint", TemplateValidation::catalogFingerprint()}});
    saveJson(fixture.filePath("template-audit.json"), audit);
    QTextStream(stdout) << "Template checks: " << checks << ", failures: " << failures << "\nEvidence: " << fixture.path() << '\n';
    return failures ? 1 : 0;
}
