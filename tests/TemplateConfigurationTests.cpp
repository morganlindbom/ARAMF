#include "core/Services.h"
#include "core/ComponentVersion.h"
#include "core/TemplateValidation.h"
#include "core/ProjectPersistence.h"
#include "core/ProjectMemory.h"
#include "core/AramfPaths.h"
#include "core/EnvironmentCatalog.h"
#include "core/DocumentTemplate.h"
#include "core/DocumentInstruction.h"
#include "core/DocumentTemplateInspector.h"
#include "ui/workflows/project/setup/ProjectSetupPage.h"
#include "ui/workflows/project/modulestemplates/ProjectModulesTemplatesPage.h"
#include "ui/workflows/output/review/ReviewPage.h"
#include "ui/workflows/output/generate/GeneratePage.h"
#include "ui/workflows/output/verify/VerifyPage.h"
#include "ui/workflows/output/finalize/FinalizePage.h"
#include "ui/workflows/project/languages/ProjectLanguagesPage.h"
#include "ui/workflows/project/frameworks/ProjectFrameworksPage.h"
#include "ui/workflows/project/academic/ProjectAcademicPage.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/shared/FooterProgressDisplay.h"
#include "ui/workflow/WorkflowWidget.h"
#include "ui/workflows/release/overview/ReleaseOverviewPage.h"
#include "ui/workflows/release/productversion/ProductVersionPage.h"
#include "ui/workflows/release/componentversions/ComponentVersionsPage.h"
#include "ui/workflows/release/schema/SchemaCompatibilityPage.h"
#include "ui/workflows/release/readiness/ReleaseReadinessPage.h"
#include "ui/workflows/release/history/ApprovalHistoryPage.h"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QFrame>
#include <QGridLayout>
#include <QAbstractScrollArea>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QJsonObject>
#include <algorithm>
#include <cmath>

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
    ReleaseManagementService::setStatePathForTests(fixture.filePath("release-management.json"));
    TemplateManager manager(nullptr, fixture.filePath("templates.json"));
    ProjectPersistence persistence;
    GenerationServices generation;
    VerificationServices verification;
    FinalizationServices finalization;
    AgentEntryPointService entryPoints;
    ProjectMemory memory;
    ProductVersion productVersion;
    ComponentVersion componentVersion;
    check(ProductVersion::parse(QStringLiteral("0.0.0"), &productVersion)
              && productVersion.toString() == QStringLiteral("0.0.0")
              && ReleaseManagementService::productVersion().toString() == QStringLiteral("0.0.0"),
          "product version parses and formats as MAJOR.MINOR.PATCH");
    check(ComponentVersion::parse(QStringLiteral("0.1.1.0"), &componentVersion)
              && componentVersion.toString() == QStringLiteral("0.1.1.0"),
          "component version parses and formats as four-part identity");
    const auto defaultReleaseComponents = ReleaseManagementService::defaultComponents();
    const auto identityComponent = std::find_if(defaultReleaseComponents.cbegin(), defaultReleaseComponents.cend(), [](const ReleaseComponent& component) {
        return component.id == QStringLiteral("project.modules-templates");
    });
    const auto releaseIdentityComponent = std::find_if(defaultReleaseComponents.cbegin(), defaultReleaseComponents.cend(), [](const ReleaseComponent& component) {
        return component.id == QStringLiteral("release.product-version");
    });
    check(defaultReleaseComponents.size() == 34
              && identityComponent != defaultReleaseComponents.cend()
              && identityComponent->version.toString() == QStringLiteral("0.1.2.0")
              && releaseIdentityComponent != defaultReleaseComponents.cend()
              && releaseIdentityComponent->version.toString() == QStringLiteral("0.6.1.0")
              && std::all_of(defaultReleaseComponents.cbegin(), defaultReleaseComponents.cend(), [](const ReleaseComponent& component) {
                     return component.version.approvedRelease == 0 && component.version.revision == 0;
                 }),
          "component versions start at zero while stable parent/item identities are preserved");
    componentVersion.revision = 9;
    const auto revisedComponent = componentVersion.incrementedRevision();
    check(revisedComponent.toString() == QStringLiteral("0.1.1.10")
              && revisedComponent.approvedRelease == 0
              && revisedComponent.parent == 1 && revisedComponent.item == 1,
          "component revision increments independently without carrying fields");
    const QList<ReleaseComponent> readinessComponents{
        {QStringLiteral("a"), QStringLiteral("A"), ComponentVersion{1, 1, 0, 3}, true, true, {}},
        {QStringLiteral("b"), QStringLiteral("B"), ComponentVersion{1, 1, 1, 4}, true, true, {}},
        {QStringLiteral("c"), QStringLiteral("C"), ComponentVersion{0, 1, 2, 7}, true, true, {}}};
    const auto noTargetReadiness = ReleaseReadinessService::evaluate(readinessComponents, std::nullopt);
    const auto targetReadiness = ReleaseReadinessService::evaluate(readinessComponents, 1);
    check(noTargetReadiness.generationAllowed && !noTargetReadiness.targetRequested,
          "no target release is a healthy development mode");
    check(targetReadiness.totalRequired == 3 && targetReadiness.approved == 2
              && targetReadiness.remaining == 1 && !targetReadiness.readyForApproval
              && targetReadiness.generationAllowed,
          "incomplete target reports readiness without blocking generation");
    ReleaseManagementService releaseService;
    QString releaseError;
    check(releaseService.approveComponentForRelease(QStringLiteral("project.file-worker"), 1, QStringLiteral("user"), &releaseError),
          "explicit authorized component approval succeeds");
    check(!releaseService.approveComponentForRelease(QStringLiteral("project.file-worker"), 0, QStringLiteral("user"), &releaseError),
          "invalid approval target is rejected");
    check(!releaseService.approveComponentForRelease(QStringLiteral("project.file-worker"), 0, QStringLiteral("agent"), &releaseError),
          "unrecognized approval authority is rejected");
    check(!releaseService.approveComponentForRelease(QStringLiteral("project.file-worker"), 0, QStringLiteral("user"), &releaseError),
          "approved release downgrade is rejected");
    ReleaseManagementService reloadedReleaseService;
    const auto releaseComponent = std::find_if(reloadedReleaseService.components().cbegin(), reloadedReleaseService.components().cend(), [](const ReleaseComponent& component) {
        return component.id == QStringLiteral("project.file-worker");
    });
    check(releaseComponent != reloadedReleaseService.components().cend()
              && releaseComponent->version.approvedRelease == 1
              && !reloadedReleaseService.approvalHistory().isEmpty(),
          "component approval and history persist in the app-owned registry");
    ProjectModel releaseModel;
    releaseModel.setTargetRelease(1);
    const QString releaseProjectFile = fixture.filePath(QStringLiteral("release-target.aramf.json"));
    check(persistence.save(releaseModel, releaseProjectFile),
          "optional target release saves with project state");
    ProjectModel reloadedReleaseModel;
    check(persistence.load(&reloadedReleaseModel, releaseProjectFile)
              && reloadedReleaseModel.targetRelease() == 1
              && !persistence.configuration(reloadedReleaseModel).contains(QStringLiteral("releaseManagement")),
          "optional target release reloads separately from technical configuration");
    QJsonObject legacyReleaseProject = persistence.toJson(releaseModel);
    legacyReleaseProject.remove(QStringLiteral("releaseManagement"));
    ProjectModel legacyReleaseModel;
    QString legacyReleaseError;
    check(persistence.fromJson(&legacyReleaseModel, legacyReleaseProject, &legacyReleaseError)
              && !legacyReleaseModel.hasTargetRelease()
              && legacyReleaseModel.targetRelease() == 0,
          "older projects without optional release metadata default safely to development mode");
    ReleaseOverviewPage releaseOverview;
    ProductVersionPage productPage(&releaseModel, &releaseService);
    ComponentVersionsPage componentPage(&releaseService);
    SchemaCompatibilityPage schemaPage(&releaseModel, &releaseService);
    ReleaseReadinessPage readinessPage(&releaseModel, &releaseService);
    ApprovalHistoryPage historyPage(&releaseService);
    check(releaseOverview.findChildren<ParentOverviewCard*>().size() == 5
              && componentPage.findChild<QTableWidget*>(QStringLiteral("componentVersionsTable")) != nullptr
              && readinessPage.findChild<QLabel*>(QStringLiteral("releaseReadinessStatus")) != nullptr
              && historyPage.findChild<QTableWidget*>(QStringLiteral("approvalHistoryTable")) != nullptr,
          "release parent and five reusable child page surfaces exist");
    QJsonObject audit;
    const auto definitions = manager.definitions();
    check(definitions.size() == 14, "all 14 built-ins audited");
    const auto officialDefinitions = manager.officialDefinitions();
    check(officialDefinitions.size() == 5, "five official templates available");
    for (const auto& official : officialDefinitions) {
        ProjectModel officialModel;
        check(manager.applyTemplate(&officialModel, official.id), official.id + " official template applies");
        check(officialModel.templateModules().contains(official.id), official.id + " provenance retained");
    }
    const auto hasCatalogId = [](const QList<EnvironmentOption>& options, const QString& id) {
        return std::any_of(options.cbegin(), options.cend(), [&id](const EnvironmentOption& option) { return option.second == id; });
    };
    for (const auto& id : {QStringLiteral("python"), QStringLiteral("scikit-learn"), QStringLiteral("numpy"), QStringLiteral("pandas"), QStringLiteral("scipy"), QStringLiteral("matplotlib"), QStringLiteral("jupyterlab"), QStringLiteral("python-venv"), QStringLiteral("cpu"), QStringLiteral("pytorch"), QStringLiteral("tensorflow"), QStringLiteral("keras"), QStringLiteral("xgboost"), QStringLiteral("lightgbm"), QStringLiteral("hugging-face-transformers"), QStringLiteral("opencv"), QStringLiteral("conda"), QStringLiteral("miniconda"), QStringLiteral("uv"), QStringLiteral("nvidia-gpu"), QStringLiteral("cuda"), QStringLiteral("cudnn"), QStringLiteral("onnx"), QStringLiteral("onnx-runtime"), QStringLiteral("tensorflow-lite")}) {
        const bool found = hasCatalogId(EnvironmentCatalog::languages(), id)
            || hasCatalogId(EnvironmentCatalog::frameworks(), id)
            || hasCatalogId(EnvironmentCatalog::developmentSupport(), id)
            || hasCatalogId(EnvironmentCatalog::toolchains(), id)
            || hasCatalogId(EnvironmentCatalog::hardwareTargets(), id);
        check(found, "ML catalog contains " + id);
    }
    ProjectModel mlModel;
    QString mlError;
    check(manager.applyTemplate(&mlModel, QStringLiteral("machine-learning"), &mlError), "Machine Learning template applies: " + mlError);
    const auto cleanCommunication = [](const CommunicationConfiguration& communication) {
        return !communication.enabled && communication.sourceRole.isEmpty() && communication.sourceTarget.isEmpty()
            && communication.destinationRole.isEmpty() && communication.destinationTarget.isEmpty()
            && communication.endpoint.isEmpty() && communication.transport.isEmpty() && communication.protocol.isEmpty()
            && !communication.authenticationRequired && !communication.encryptionRequired
            && communication.endpoints.isEmpty() && communication.links.isEmpty() && communication.messages.isEmpty()
            && communication.testVectors.isEmpty();
    };
    check(cleanCommunication(mlModel.communicationConfiguration()), "Machine Learning communication is clean and disabled");
    ProjectModel freshModel;
    freshModel.resetForNewProject();
    check(cleanCommunication(freshModel.communicationConfiguration()), "fresh new project communication is clean and disabled");
    ProjectModel androidToMl;
    check(manager.applyTemplate(&androidToMl, QStringLiteral("official-android-arduino-smart-home")), "Android Arduino Smart Home transition source applies");
    check(!cleanCommunication(androidToMl.communicationConfiguration()), "Android Arduino Smart Home has communication defaults");
    check(manager.applyTemplate(&androidToMl, QStringLiteral("machine-learning")), "Android Arduino Smart Home to Machine Learning applies");
    check(cleanCommunication(androidToMl.communicationConfiguration()), "Android Arduino Smart Home to Machine Learning clears communication");
    ProjectModel roundTripTemplates;
    check(manager.applyTemplate(&roundTripTemplates, QStringLiteral("machine-learning")), "Machine Learning transition source applies");
    check(manager.applyTemplate(&roundTripTemplates, QStringLiteral("official-android-arduino-smart-home")), "Machine Learning to Android Arduino Smart Home applies");
    check(manager.applyTemplate(&roundTripTemplates, QStringLiteral("machine-learning")), "Machine Learning round trip applies");
    check(cleanCommunication(roundTripTemplates.communicationConfiguration()), "Machine Learning to Android to Machine Learning remains clean");
    const auto mlCapabilities = mlModel.developmentCapabilities();
    check(mlModel.templateId() == QStringLiteral("machine-learning") && mlModel.context() == QStringLiteral("ai-machine-learning"), "Machine Learning identity and context");
    check(mlCapabilities.languages == QStringList{QStringLiteral("python")}, "Machine Learning selects Python only");
    check(mlCapabilities.frameworks == QStringList{QStringLiteral("scikit-learn"), QStringLiteral("numpy"), QStringLiteral("pandas"), QStringLiteral("scipy"), QStringLiteral("matplotlib")}, "Machine Learning scientific baseline");
    check(mlCapabilities.ides == QStringList{QStringLiteral("visual-studio-code")} && mlCapabilities.developmentTools.contains(QStringLiteral("jupyterlab")), "Machine Learning selects VS Code and JupyterLab");
    check(mlCapabilities.toolchains == QStringList{QStringLiteral("python"), QStringLiteral("python-venv")} && mlCapabilities.dependencyManagers == QStringList{QStringLiteral("pip")}, "Machine Learning selects Python venv and pip");
    check(mlCapabilities.targetPlatforms == QStringList{QStringLiteral("ai-machine-learning")} && mlCapabilities.hardwareTargets == QStringList{QStringLiteral("cpu")}, "Machine Learning target and CPU baseline");
    check(!mlCapabilities.frameworks.contains(QStringLiteral("pytorch")) && !mlCapabilities.frameworks.contains(QStringLiteral("tensorflow"))
              && !mlCapabilities.hardwareTargets.contains(QStringLiteral("nvidia-gpu")) && !mlCapabilities.frameworks.contains(QStringLiteral("cuda")),
          "Machine Learning does not force optional acceleration stacks");
    const auto mlDefinition = manager.definition(QStringLiteral("machine-learning"));
    check(mlDefinition.official && mlDefinition.displayName == QStringLiteral("Machine Learning"), "Machine Learning official definition");
    check(TemplateValidation::validateDefinition(mlDefinition).isEmpty(), "Machine Learning template validates");
    const QString mlFile = fixture.filePath("machine-learning.aramf.json");
    check(persistence.save(mlModel, mlFile, &mlError), "Machine Learning project saves: " + mlError);
    ProjectModel mlReloaded;
    check(persistence.load(&mlReloaded, mlFile, &mlError), "Machine Learning project reloads: " + mlError);
    check(mlReloaded.developmentCapabilities().frameworks == mlCapabilities.frameworks
              && mlReloaded.developmentCapabilities().developmentTools == mlCapabilities.developmentTools
              && mlReloaded.developmentCapabilities().hardwareTargets == mlCapabilities.hardwareTargets,
          "Machine Learning baseline persists exactly");
    ProjectModel mlGuiModel;
    ProjectModulesTemplatesPage mlGuiPage(&mlGuiModel, &manager);
    auto mlTemplateBoxes = mlGuiPage.findChildren<QCheckBox*>();
    auto mlTemplateBox = std::find_if(mlTemplateBoxes.cbegin(), mlTemplateBoxes.cend(), [](QCheckBox* box) { return box->text() == QStringLiteral("Machine Learning"); });
    check(mlTemplateBox != mlTemplateBoxes.cend(), "Machine Learning template is visible in Project modules & templates");
    if (mlTemplateBox != mlTemplateBoxes.cend()) {
        (*mlTemplateBox)->click();
        QApplication::processEvents();
        check(mlGuiModel.templateId() == QStringLiteral("machine-learning") && mlGuiModel.developmentCapabilities().languages == QStringList{QStringLiteral("python")}, "Project Setup applies Machine Learning template through GUI control");
    }
    auto mlOptional = mlReloaded.developmentCapabilities();
    mlOptional.frameworks << QStringLiteral("pytorch") << QStringLiteral("cuda") << QStringLiteral("onnx-runtime");
    mlOptional.hardwareTargets << QStringLiteral("nvidia-gpu");
    mlReloaded.setDevelopmentCapabilities(mlOptional);
    check(persistence.save(mlReloaded, mlFile, &mlError), "Machine Learning optional selections save: " + mlError);
    ProjectModel mlOptionalReloaded;
    check(persistence.load(&mlOptionalReloaded, mlFile, &mlError), "Machine Learning optional selections reload: " + mlError);
    check(mlOptionalReloaded.developmentCapabilities().frameworks == mlOptional.frameworks
              && mlOptionalReloaded.developmentCapabilities().hardwareTargets == mlOptional.hardwareTargets,
          "Machine Learning optional selections persist exactly");
    auto cleanRoundTrip = mlReloaded.communicationConfiguration();
    cleanRoundTrip.sourceTarget = QStringLiteral("should-not-be-persisted-as-ML-default");
    cleanRoundTrip.enabled = false;
    mlReloaded.setCommunicationConfiguration(cleanRoundTrip);
    check(persistence.save(mlReloaded, mlFile, &mlError), "Machine Learning communication save: " + mlError);
    ProjectModel mlCommunicationReloaded;
    check(persistence.load(&mlCommunicationReloaded, mlFile, &mlError), "Machine Learning communication reload: " + mlError);
    check(mlCommunicationReloaded.communicationConfiguration().sourceTarget == QStringLiteral("should-not-be-persisted-as-ML-default"), "explicit communication remains user-persisted");
    ProjectModel cleanSavedMl;
    check(manager.applyTemplate(&cleanSavedMl, QStringLiteral("machine-learning")), "clean Machine Learning save source applies");
    const QString cleanMlFile = fixture.filePath("clean-machine-learning.aramf.json");
    check(persistence.save(cleanSavedMl, cleanMlFile, &mlError), "clean Machine Learning project saves: " + mlError);
    ProjectModel cleanSavedMlReloaded;
    check(persistence.load(&cleanSavedMlReloaded, cleanMlFile, &mlError), "clean Machine Learning project reloads: " + mlError);
    check(cleanCommunication(cleanSavedMlReloaded.communicationConfiguration()), "clean Machine Learning save/reload preserves communication state");
    mlModel.setProjectPath(fixture.filePath("machine-learning-target"));
    const auto mlGeneration = generation.generate(mlModel, mlModel.generationOptions());
    check(mlGeneration.success, "Machine Learning worker generation");
    QFile mlMetadata(QDir(mlModel.projectPath()).filePath(QStringLiteral("ARAMF_WORKER/platforms/platform-metadata.json")));
    check(mlMetadata.open(QIODevice::ReadOnly), "Machine Learning platform metadata readable");
    const auto mlPlatform = QJsonDocument::fromJson(mlMetadata.readAll()).object();
    const auto contains = [](const QJsonValue& value, const QString& id) { return list(value).contains(id); };
    check(contains(mlPlatform.value(QStringLiteral("languages")), "python")
              && contains(mlPlatform.value(QStringLiteral("frameworks")), "scikit-learn")
              && contains(mlPlatform.value(QStringLiteral("frameworks")), "numpy")
              && contains(mlPlatform.value(QStringLiteral("frameworks")), "pandas")
              && contains(mlPlatform.value(QStringLiteral("frameworks")), "scipy")
              && contains(mlPlatform.value(QStringLiteral("frameworks")), "matplotlib")
              && contains(mlPlatform.value(QStringLiteral("developmentTools")), "jupyterlab")
              && contains(mlPlatform.value(QStringLiteral("hardwareTargets")), "cpu"),
          "Machine Learning metadata exposes baseline capabilities");
    QFile mlAgent(QDir(mlModel.projectPath()).filePath(QStringLiteral("ARAMF_WORKER/AGENTS.md")));
    check(mlAgent.open(QIODevice::ReadOnly) && QString::fromUtf8(mlAgent.readAll()).contains(QStringLiteral("Python Machine Learning guidance")), "Machine Learning guidance generated");
    QFile mlRootAgent(QDir(mlModel.projectPath()).filePath(QStringLiteral("AGENTS.md")));
    check(mlRootAgent.open(QIODevice::ReadOnly) && QString::fromUtf8(mlRootAgent.readAll()).contains(QStringLiteral("ARAMF_WORKER/AGENTS.md")), "unsuffixed root AGENTS.md uses canonical worker");
    check(QFile::exists(QDir(mlModel.projectPath()).filePath(QStringLiteral("ARAMF_WORKER/ARAMF_WORKER.json"))), "unsuffixed identity JSON matches worker directory");
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
    QFile suffixedRootAgent(QDir(suffixedModel.projectPath()).filePath(QStringLiteral("AGENTS.md")));
    QFile suffixedWorkerAgent(QDir(suffixedModel.projectPath()).filePath(QStringLiteral("ARAMF_WORKER_ANDROID_PICO/AGENTS.md")));
    QString suffixedRootText;
    QString suffixedWorkerText;
    if (suffixedRootAgent.open(QIODevice::ReadOnly)) suffixedRootText = QString::fromUtf8(suffixedRootAgent.readAll());
    if (suffixedWorkerAgent.open(QIODevice::ReadOnly)) suffixedWorkerText = QString::fromUtf8(suffixedWorkerAgent.readAll());
    check(suffixedRootText.contains(QStringLiteral("ARAMF_WORKER_ANDROID_PICO/AGENTS.md"))
              && !suffixedRootText.contains(QStringLiteral("ARAMF_WORKER/AGENTS.md")), "suffixed root AGENTS.md uses resolved worker");
    check(suffixedWorkerText.contains(QStringLiteral("ARAMF_WORKER_ANDROID_PICO/"))
              && !suffixedWorkerText.contains(QStringLiteral("ARAMF_WORKER/")), "suffixed worker AGENTS.md uses resolved worker");
    check(std::none_of(suffixedGeneration.generatedFiles.cbegin(), suffixedGeneration.generatedFiles.cend(), [](const QString& path) {
              return path.contains(QStringLiteral("ARAMF_WORKER_")) && path.contains(QStringLiteral("ARAMF_WORKER_WORKER"));
          }), "suffixed generated paths do not duplicate worker prefix");
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
              && QFileInfo(suffixReloaded.projectFilePath()).fileName() == QStringLiteral("suffixed-roundtrip.aramf.json"),
          "worker suffix save/reload preserves the actual project file");
    ProjectModel mlSuffixedModel;
    check(manager.applyTemplate(&mlSuffixedModel, QStringLiteral("machine-learning")), "explicit-suffix ML template setup");
    mlSuffixedModel.setProjectPath(fixture.filePath("machine-learning-suffixed"));
    mlSuffixedModel.setWorkerNameSuffix(QStringLiteral("Experiment 01"));
    const QString mlSuffixFile = fixture.filePath("machine-learning-suffixed.aramf.json");
    check(persistence.save(mlSuffixedModel, mlSuffixFile, &suffixError), "explicit-suffix ML save");
    ProjectModel mlSuffixedReloaded;
    check(persistence.load(&mlSuffixedReloaded, mlSuffixFile, &suffixError)
              && mlSuffixedReloaded.workerNameSuffix() == QStringLiteral("EXPERIMENT_01"), "explicit-suffix ML save/reload");
    mlSuffixedReloaded.setProjectPath(fixture.filePath("machine-learning-suffixed"));
    const auto mlSuffixedGeneration = generation.generate(mlSuffixedReloaded, mlSuffixedReloaded.generationOptions());
    const QString mlSuffixedName = QStringLiteral("ARAMF_WORKER_EXPERIMENT_01");
    check(mlSuffixedGeneration.success && QDir(mlSuffixedReloaded.projectPath()).exists(mlSuffixedName), "explicit-suffix ML generation uses resolved directory");
    QFile mlSuffixedRoot(QDir(mlSuffixedReloaded.projectPath()).filePath(QStringLiteral("AGENTS.md")));
    QFile mlSuffixedWorker(QDir(mlSuffixedReloaded.projectPath()).filePath(mlSuffixedName + QStringLiteral("/AGENTS.md")));
    QString mlSuffixedRootText;
    QString mlSuffixedWorkerText;
    if (mlSuffixedRoot.open(QIODevice::ReadOnly)) mlSuffixedRootText = QString::fromUtf8(mlSuffixedRoot.readAll());
    if (mlSuffixedWorker.open(QIODevice::ReadOnly)) mlSuffixedWorkerText = QString::fromUtf8(mlSuffixedWorker.readAll());
    check(mlSuffixedRootText.contains(mlSuffixedName + QStringLiteral("/AGENTS.md"))
              && mlSuffixedWorkerText.contains(mlSuffixedName + QStringLiteral("/"))
              && QFile::exists(QDir(mlSuffixedReloaded.projectPath()).filePath(mlSuffixedName + QStringLiteral("/") + mlSuffixedName + QStringLiteral(".json"))),
          "explicit-suffix ML AGENTS and identity names match");
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
        , {"android-arduino-smart-home", {"kotlin", "cpp", "c"}}, {"machine-learning", {"python"}}
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
        ProjectModulesTemplatesPage modulesPage(&model, &manager);
        ReviewPage review(&model);
        GeneratePage generate(&model, &setup, &generation);
        // These pages exist before the selection, just as in the production application.
        ProjectLanguagesPage languagePage(&model);
        ProjectFrameworksPage frameworkPage(&model);
        const auto combo = modulesPage.findChild<QComboBox*>("templateSelector");
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
    ProjectModulesTemplatesPage modulesPage(&custom, &manager);
    check(setup.findChild<QGroupBox*>("moduleGroup") == nullptr
              && setup.findChild<QGroupBox*>("templateGroup") == nullptr,
          "identity page does not duplicate modules or templates");
    check(modulesPage.findChild<QGroupBox*>("moduleGroup") != nullptr
              && modulesPage.findChild<QGroupBox*>("templateGroup") != nullptr,
          "modules and templates belong to the dedicated child page");
    check(setup.findChild<QLineEdit*>("projectFilePath") != nullptr
              && setup.findChild<QLineEdit*>("workerPath") != nullptr,
          "identity page exposes canonical project file and Worker path state");
    auto* description = setup.findChild<QTextEdit*>("projectDescription");
    check(description != nullptr
              && description->sizePolicy().verticalPolicy() == QSizePolicy::Preferred
              && description->minimumHeight() >= 64
              && description->maximumHeight() <= 96,
          "identity description remains usable but uses a compact bounded height");
    QTimer::singleShot(0, [] {
        QTimer::singleShot(100, [] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* dialog = qobject_cast<QInputDialog*>(widget)) {
                    dialog->setTextValue("Saved through GUI");
                    dialog->accept();
                    break;
                }
            }
        });
    });
    modulesPage.findChild<QPushButton*>("saveCustomTemplate")->click();
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
    ProjectModulesTemplatesPage geometryPage(&geometryModel, &manager);
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
            for (auto* box : templateBoxes) {
                check(box->sizePolicy().verticalPolicy() == QSizePolicy::Preferred,
                      "template checkbox uses natural vertical size policy");
            }
            auto* templateGrid = templateGroup->findChild<QWidget*>("templateGrid");
            if (templateGrid) {
                auto* templateLayout = qobject_cast<QGridLayout*>(templateGrid->layout());
                check(templateLayout && templateLayout->verticalSpacing() <= 12,
                      "template grid uses compact normal row spacing");
                auto* saveTemplate = geometryPage.findChild<QPushButton*>("saveCustomTemplate");
                auto* removeTemplate = geometryPage.findChild<QPushButton*>("removeCustomTemplate");
                check(saveTemplate && removeTemplate && saveTemplate->geometry().y() == removeTemplate->geometry().y(),
                      "template controls share a compact horizontal action row");
            }
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
        ProjectModulesTemplatesPage modulePage(&moduleModel, &manager);
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

    // Regression: Save Work must round-trip manual workflow completion to the
    // real project file, while the displayed percentage remains derived state.
    QTemporaryDir completionRoundTrip;
    const QString completionFile = QDir(completionRoundTrip.path()).filePath(QStringLiteral("completion.aramf.json"));
    ProjectModel completionModel;
    completionModel.setProjectPath(completionRoundTrip.path());
    completionModel.setProjectFilePath(completionFile);
    const QStringList expectedCompletedPages{
        workflowPageKey(WorkflowPageId::ProjectIdentity),
        workflowPageKey(WorkflowPageId::ProjectModulesTemplates),
        workflowPageKey(WorkflowPageId::Academic)};
    for (const auto& pageId : expectedCompletedPages) completionModel.setPageCompleted(pageId, true);
    ProjectSetupPage completionSavePage(&completionModel, &manager, &persistence);
    QString completionError;
    check(completionModel.isModified(), "workflow completion changes mark the project dirty");
    check(completionSavePage.saveCurrentProject(&completionError),
          "Save Work canonical path writes the current project model");
    check(!completionModel.isModified(), "successful Save Work clears the existing dirty state");

    QFile completionFileReader(completionFile);
    check(completionFileReader.open(QIODevice::ReadOnly), "saved completion project file is readable");
    const QJsonObject persistedCompletion = QJsonDocument::fromJson(completionFileReader.readAll()).object();
    completionFileReader.close();
    const auto persistedPages = persistedCompletion.value(QStringLiteral("workflowProgress"))
        .toObject().value(QStringLiteral("completedPages")).toArray();
    QSet<QString> persistedPageIds;
    for (const auto& value : persistedPages) persistedPageIds.insert(value.toString());
    QSet<QString> expectedCompletedPageSet;
    for (const auto& pageId : expectedCompletedPages) expectedCompletedPageSet.insert(pageId);
    check(persistedPageIds == expectedCompletedPageSet
              && !persistedCompletion.contains(QStringLiteral("progressPercentage")),
          "Save Work serializes only stable completed page IDs, not a percentage cache");

    const QString oldProjectFile = QDir(completionRoundTrip.path()).filePath(QStringLiteral("old-project.aramf.json"));
    QJsonObject oldProjectWithoutProgress = persistedCompletion;
    oldProjectWithoutProgress.remove(QStringLiteral("workflowProgress"));
    QFile oldProjectWriter(oldProjectFile);
    check(oldProjectWriter.open(QIODevice::WriteOnly)
              && oldProjectWriter.write(QJsonDocument(oldProjectWithoutProgress).toJson(QJsonDocument::Indented)) > 0,
          "older project without optional workflow progress is writable");
    oldProjectWriter.close();
    ProjectModel oldProjectModel;
    check(persistence.load(&oldProjectModel, oldProjectFile, &completionError)
              && oldProjectModel.completedPageIds().isEmpty(),
          "older project without workflowProgress loads with empty completion state");

    ProjectModel reopenedCompletion;
    check(persistence.load(&reopenedCompletion, completionFile, &completionError),
          "saved completion project reloads into a fresh model");
    WorkflowWidget reopenedWorkflow;
    reopenedWorkflow.setStepCount(28);
    reopenedWorkflow.setCompletionModel(&reopenedCompletion);
    reopenedWorkflow.show();
    QApplication::processEvents();
    auto* reopenedList = reopenedWorkflow.findChild<QListWidget*>();
    const int expectedCompletionPercentage = reopenedWorkflow.completablePageCount() <= 0
        ? 0
        : static_cast<int>(std::lround(100.0 * expectedCompletedPages.size()
                                       / reopenedWorkflow.completablePageCount()));
    check(reopenedCompletion.completedPageIds() == persistedPageIds
              && reopenedList->item(2)->data(Qt::UserRole + 2).toBool()
              && reopenedList->item(3)->data(Qt::UserRole + 2).toBool()
              && reopenedList->item(4)->data(Qt::UserRole + 2).toBool()
              && reopenedList->item(1)->background().color() == QColor(204, 238, 211)
              && reopenedWorkflow.completionPercentage() == expectedCompletionPercentage,
          "fresh reload restores checkmarks, parent aggregate, and derived progress");

    reopenedCompletion.setPageCompleted(workflowPageKey(WorkflowPageId::Academic), false);
    check(reopenedCompletion.isModified(), "clearing a completion marker marks the project dirty");
    ProjectSetupPage secondCompletionSave(&reopenedCompletion, &manager, &persistence);
    check(secondCompletionSave.saveCurrentProject(&completionError),
          "second Save Work persists an unchecked page");
    ProjectModel reopenedAfterClear;
    check(persistence.load(&reopenedAfterClear, completionFile, &completionError),
          "project reloads after the completion clear is saved");
    check(reopenedAfterClear.isPageCompleted(workflowPageKey(WorkflowPageId::ProjectIdentity))
              && reopenedAfterClear.isPageCompleted(workflowPageKey(WorkflowPageId::ProjectModulesTemplates))
              && !reopenedAfterClear.isPageCompleted(workflowPageKey(WorkflowPageId::Academic)),
          "an unchecked completion does not return after Save Work and reload");

    // Stale completion IDs from a non-SETUP domain must be ignored both by
    // persistence and by the workflow UI capability model.
    ProjectModel staleCompletionModel;
    staleCompletionModel.setPageCompleted(workflowPageKey(WorkflowPageId::ProjectIdentity), true);
    staleCompletionModel.setPageCompleted(workflowPageKey(WorkflowPageId::ProductVersion), true);
    staleCompletionModel.setPageCompleted(workflowPageKey(WorkflowPageId::Generate), true);
    const QString staleCompletionFile = QDir(completionRoundTrip.path()).filePath(QStringLiteral("stale-completion.aramf.json"));
    check(persistence.save(staleCompletionModel, staleCompletionFile),
          "stale non-SETUP completion fixture saves through canonical persistence");
    ProjectModel reloadedStaleCompletion;
    check(persistence.load(&reloadedStaleCompletion, staleCompletionFile, &completionError)
              && reloadedStaleCompletion.completedPageIds().size() == 1
              && reloadedStaleCompletion.isPageCompleted(workflowPageKey(WorkflowPageId::ProjectIdentity))
              && !reloadedStaleCompletion.isPageCompleted(workflowPageKey(WorkflowPageId::ProductVersion))
              && !reloadedStaleCompletion.isPageCompleted(workflowPageKey(WorkflowPageId::Generate)),
          "non-SETUP completion IDs are filtered during project round-trip");

    MainWindow responsive(0, 1280, 720);
    responsive.show();
    auto* pageScroll = responsive.findChild<QScrollArea*>("workflowPageScroll");
    auto* stack = responsive.findChild<QStackedWidget*>();
    auto* saveWork = responsive.findChild<QPushButton*>("saveWork");
    auto* setupProgress = responsive.findChild<FooterProgressDisplay*>("setupProgress");
    auto* pageFooter = responsive.findChild<QFrame*>("workflowPageFooter");
    check(pageScroll != nullptr && stack != nullptr && saveWork != nullptr
              && setupProgress != nullptr && pageFooter != nullptr,
          "workflow page scroll host, Save Work footer, and setup progress exist");
    if (pageScroll && saveWork) {
        check(!pageScroll->isAncestorOf(saveWork), "Save Work footer remains outside scrolling content");
        check(!pageScroll->widgetResizable(), "workflow scroll host preserves natural content height");
        check(pageScroll->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded, "workflow vertical scrolling is AsNeeded");
    }
    if (saveWork && setupProgress) {
        check(saveWork->geometry().left() < setupProgress->geometry().left()
                  && setupProgress->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding,
              "Save Work is left of the expanding setup progress bar");
        check(saveWork->height() == setupProgress->height()
                  && saveWork->height() >= 38 && saveWork->height() <= 42,
              "Save Work and setup progress use the same modest footer control height");
        check(setupProgress->alignment() == Qt::AlignCenter,
              "setup progress text is centered inside the progress bar");
        check(setupProgress->minimum() == 0 && setupProgress->maximum() == 100
                  && setupProgress->format() == "%p% of your setup is done",
              "setup progress uses the manual completion percentage format");
    }
    if (pageFooter && saveWork && setupProgress) {
        check(!pageScroll->isAncestorOf(pageFooter), "shared footer remains outside scroll content");
        const QRect footerRect(pageFooter->mapToGlobal(QPoint(0, 0)), pageFooter->size());
        const QRect windowRect(responsive.mapToGlobal(QPoint(0, 0)), responsive.size());
        check(windowRect.contains(footerRect.topLeft()) && windowRect.contains(footerRect.bottomRight()),
              "shared footer remains inside the visible page shell");
    }
    auto* responsiveWorkflow = responsive.findChild<WorkflowWidget*>();
    auto* responsiveNavigation = responsiveWorkflow ? responsiveWorkflow->findChild<QListWidget*>() : nullptr;
    auto* startupModuleGroup = responsive.findChild<QGroupBox*>("moduleGroup");
    auto* startupTemplateGroup = responsive.findChild<QGroupBox*>("templateGroup");
    check(responsiveWorkflow && responsiveNavigation && startupModuleGroup && startupTemplateGroup,
          "cold-start layout regression has the Project modules & templates shell");
    QRect startupModule;
    QRect startupTemplates;
    QSize startupPageSize;
    QSize startupFooterSize;
    const QSize startupWindowSize = responsive.size();
    if (responsiveNavigation && startupModuleGroup && startupTemplateGroup) {
        // Row 3 is the stable 1.2 navigation item after PROJECT and page 1.
        responsiveNavigation->setCurrentRow(3);
        QApplication::processEvents();
        QApplication::processEvents();
        auto* startupPage = stack->currentWidget();
        startupModule = startupModuleGroup->geometry();
        startupTemplates = startupTemplateGroup->geometry();
        startupPageSize = startupPage ? startupPage->size() : QSize();
        startupFooterSize = pageFooter ? pageFooter->size() : QSize();
        check(startupModuleGroup->sizePolicy().verticalPolicy() == QSizePolicy::Preferred
                  && startupTemplateGroup->sizePolicy().verticalPolicy() == QSizePolicy::Preferred
                  && pageScroll->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded,
              "cold-start layout keeps natural content sizing and AsNeeded scrolling");
        // Return through the real navigation path before the existing direct
        // stack-index viewport sweep below; this keeps that sweep independent
        // of the comparison page selected above.
        responsiveNavigation->setCurrentRow(1);
        QApplication::processEvents();
    }
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
                          QStringLiteral("workflow page child remains within viewport width (page %1, %2, child %3: %4..%5, viewport %6..%7, stack=%8, pageWidth=%9)")
                              .arg(index)
                              .arg(page->metaObject()->className())
                              .arg(child->objectName().isEmpty() ? child->metaObject()->className() : child->objectName())
                              .arg(childRect.left())
                              .arg(childRect.right())
                              .arg(viewport.left())
                              .arg(viewport.right())
                              .arg(stack->width())
                              .arg(page->width()));
                }
            }
        }
    }
    if (responsiveNavigation && startupModuleGroup && startupTemplateGroup) {
        responsive.resize(startupWindowSize);
        QApplication::processEvents();
        QApplication::processEvents();
        ProjectModel* liveModel = responsive.findChild<ProjectModel*>();
        QString startupLoadError;
        check(liveModel && persistence.load(liveModel, completionFile, &startupLoadError),
              "cold-start comparison loads a project into the same MainWindow");
        QApplication::processEvents();
        QApplication::processEvents();
        responsiveNavigation->setCurrentRow(3);
        QApplication::processEvents();
        auto* loadedPage = stack->currentWidget();
        const QRect loadedModule = startupModuleGroup->geometry();
        const QRect loadedTemplates = startupTemplateGroup->geometry();
        const QSize loadedPageSize = loadedPage ? loadedPage->size() : QSize();
        const QSize loadedFooterSize = pageFooter ? pageFooter->size() : QSize();
        const auto closeEnough = [](int first, int second) { return qAbs(first - second) <= 2; };
        check(closeEnough(startupModule.width(), loadedModule.width())
                  && closeEnough(startupModule.height(), loadedModule.height())
                  && closeEnough(startupTemplates.width(), loadedTemplates.width())
                  && closeEnough(startupTemplates.height(), loadedTemplates.height())
                  && closeEnough(startupPageSize.width(), loadedPageSize.width())
                  && closeEnough(startupPageSize.height(), loadedPageSize.height())
                  && closeEnough(startupFooterSize.height(), loadedFooterSize.height()),
              QStringLiteral("cold-start and post-load Project modules & templates geometry are equivalent (modules %1x%2/%3x%4, templates %5x%6/%7x%8, page %9x%10/%11x%12, footer %13/%14)")
                  .arg(startupModule.width()).arg(startupModule.height())
                  .arg(loadedModule.width()).arg(loadedModule.height())
                  .arg(startupTemplates.width()).arg(startupTemplates.height())
                  .arg(loadedTemplates.width()).arg(loadedTemplates.height())
                  .arg(startupPageSize.width()).arg(startupPageSize.height())
                  .arg(loadedPageSize.width()).arg(loadedPageSize.height())
                  .arg(startupFooterSize.height()).arg(loadedFooterSize.height()));
    }
    responsive.close();

    // Thesis/Report source-role regression coverage: canonical IDs, independent
    // defaults, role filtering/validation, and persistence.
    const auto governance = EnvironmentCatalog::governanceRoles();
    check(std::any_of(governance.cbegin(), governance.cend(), [](const auto& option) { return option.second == "thesis-template" && option.first == "Thesis Template"; }), "Thesis Template role is canonical and exposed");
    check(std::any_of(governance.cbegin(), governance.cend(), [](const auto& option) { return option.second == "report-template" && option.first == "Report Template"; }), "Report Template role is canonical and exposed");
    ProjectModel documentationModel;
    auto documentation = documentationModel.academicConfiguration();
    documentation.thesisDocumentation.enabled = true;
    documentation.reportDocumentation.enabled = true;
    documentation.reportDocumentation.templateMode = QStringLiteral("source");
    documentation.reportDocumentation.templateSourceId = QStringLiteral("report-resource");
    ProjectResource reportResource; reportResource.id = QStringLiteral("report-resource"); reportResource.name = QStringLiteral("Report source"); reportResource.role = QStringLiteral("report-template"); reportResource.enabled = true;
    documentationModel.setResources({reportResource});
    documentationModel.setAcademicConfiguration(documentation);
    check(documentationModel.academicConfiguration().thesisDocumentation.templateMode == "aramf-default", "Thesis enabling auto-selects ARAMF default");
    check(documentationModel.academicConfiguration().reportDocumentation.templateMode == "source" && documentationModel.academicConfiguration().reportDocumentation.templateSourceId == "report-resource", "Report custom source remains independent");
    auto invalidDocumentation = documentation;
    invalidDocumentation.reportDocumentation.templateSourceId = QStringLiteral("missing");
    documentationModel.setAcademicConfiguration(invalidDocumentation);
    const auto invalidErrors = TemplateValidation::validateConfiguration(ProjectPersistence().configuration(documentationModel));
    check(std::any_of(invalidErrors.cbegin(), invalidErrors.cend(), [](const QString& error) { return error.contains("Report") && error.contains("does not exist"); }), "Missing document source is detected structurally");
    documentationModel.setAcademicConfiguration(documentation);
    const auto documentationJson = persistence.toJson(documentationModel);
    ProjectModel reopenedDocumentation;
    check(persistence.fromJson(&reopenedDocumentation, documentationJson), "Documentation configuration reloads");
    check(reopenedDocumentation.academicConfiguration().thesisDocumentation.enabled && reopenedDocumentation.academicConfiguration().reportDocumentation.templateSourceId == "report-resource", "Thesis/Report persistence remains independent");

    const auto thesisDefault = DocumentTemplates::thesis();
    const auto reportDefault = DocumentTemplates::report();
    check(thesisDefault.id == "aramf-default-thesis" && thesisDefault.version == 1, "Default Thesis identity and version are stable");
    check(reportDefault.id == "aramf-default-report" && reportDefault.version == 1, "Default Report identity and version are stable");
    check(!thesisDefault.sections.isEmpty() && !reportDefault.sections.isEmpty(), "Default document templates are non-empty");
    check(thesisDefault.sections.size() != reportDefault.sections.size() || thesisDefault.sections.first().id != reportDefault.sections.first().id, "Thesis and Report structures are distinct");
    const auto checkDocument = [&](const DocumentTemplate& document, const QStringList& requiredIds) {
        check(DocumentTemplates::validate(document).isEmpty(), document.documentType + " canonical structure validates");
        QSet<QString> ids;
        int previousOrder = 0;
        for (const auto& item : DocumentTemplates::tableOfContents(document)) {
            ids.insert(item.id);
            check(item.order > previousOrder && item.level >= 1 && item.level <= 6, document.documentType + " section order and level are valid");
            check(!item.titleSv.isEmpty() && !item.titleEn.isEmpty() && !item.guidanceSv.isEmpty() && !item.guidanceEn.isEmpty(), document.documentType + " section is bilingual and guided");
            previousOrder = item.order;
        }
        for (const auto& id : requiredIds) check(ids.contains(id), document.documentType + " contains " + id);
    };
    checkDocument(thesisDefault, {"introduction", "theory-background", "method", "results", "discussion", "conclusion", "references"});
    checkDocument(reportDefault, {"introduction", "method", "execution", "results", "discussion", "conclusion", "references"});
    check(DocumentTemplates::manifest(true, "aramf-default", {}, true, "aramf-default", {}).value("documents").toArray().first().toObject().value("sections").toArray().size() == thesisDefault.sections.size(), "TOC/generation manifest derives from canonical Thesis sections");

    const auto thesisInstruction = DocumentInstructions::thesis();
    const auto reportInstruction = DocumentInstructions::report();
    check(thesisInstruction.id == "aramf-thesis-instruction" && thesisInstruction.version == 1, "Thesis canonical instruction identity/version");
    check(reportInstruction.id == "aramf-report-instruction" && reportInstruction.version == 1, "Report canonical instruction identity/version");
    check(thesisInstruction.id != reportInstruction.id && thesisInstruction.purpose != reportInstruction.purpose
              && thesisInstruction.rules.join(" ").contains("research questions")
              && !reportInstruction.rules.join(" ").contains("research questions"), "Thesis and Report instructions are independent");
    check(documentationModel.academicConfiguration().thesisDocumentation.instructionId == thesisInstruction.id
              && documentationModel.academicConfiguration().reportDocumentation.instructionId == reportInstruction.id,
          "Thesis and Report resolve their own canonical instructions");
    QFile markdown(fixture.filePath("custom-thesis.md"));
    check(markdown.open(QIODevice::WriteOnly), "custom markdown source opens");
    markdown.write("# Introduction\n\n## Method\n\n### Evidence\n");
    markdown.close();
    ProjectResource customResource; customResource.id = "custom-thesis"; customResource.role = "thesis-template"; customResource.location = markdown.fileName(); customResource.enabled = true;
    const auto inspectedMarkdown = DocumentTemplateInspector::inspect(customResource);
    check(inspectedMarkdown.format == "md" && inspectedMarkdown.capability == "text-structured" && inspectedMarkdown.structurallyParsed && inspectedMarkdown.sections.size() == 3, "Markdown custom template is classified and parsed");
    check(inspectedMarkdown.sections.at(1).level == 2 && inspectedMarkdown.sections.at(1).titleEn == "Method", "Markdown heading order/level/title preserved");
    const auto repeatedInspection = DocumentTemplateInspector::inspect(customResource);
    check(repeatedInspection.contentHash == inspectedMarkdown.contentHash && repeatedInspection.sections.size() == inspectedMarkdown.sections.size(), "Custom inspection is deterministic");
    QFile docx(fixture.filePath("custom-report.docx"));
    check(docx.open(QIODevice::WriteOnly), "custom DOCX source opens"); docx.write("opaque bytes"); docx.close();
    ProjectResource officeResource; officeResource.id = "custom-report"; officeResource.role = "report-template"; officeResource.location = docx.fileName();
    const auto inspectedOffice = DocumentTemplateInspector::inspect(officeResource);
    check(inspectedOffice.format == "docx" && inspectedOffice.capability == "office-structured" && !inspectedOffice.structurallyParsed && inspectedOffice.sections.isEmpty(), "DOCX is recognized without false parsing");
    check(markdown.size() == QFileInfo(markdown.fileName()).size(), "Custom source remains unchanged after inspection");

    AcademicConfiguration multi;
    multi.enabled = true;
    multi.projectTypes = {QStringLiteral("academic-assignment"), QStringLiteral("research-project"), QStringLiteral("thesis-project"), QStringLiteral("report-project")};
    documentationModel.setAcademicConfiguration(multi);
    const auto multiValue = documentationModel.academicConfiguration();
    check(multiValue.projectTypes.size() == 4 && multiValue.projectTypes.contains(QStringLiteral("research-project")) && multiValue.projectTypes.contains(QStringLiteral("thesis-project")), "Academic project types support simultaneous selections");
    check(multiValue.thesisDocumentation.enabled && multiValue.reportDocumentation.enabled, "Thesis and Report project types enable their independent documentation outputs");
    multi.projectTypes.removeAll(QStringLiteral("research-project"));
    documentationModel.setAcademicConfiguration(multi);
    check(documentationModel.academicConfiguration().projectTypes.size() == 3 && documentationModel.academicConfiguration().projectTypes.contains(QStringLiteral("thesis-project")), "Deselecting one academic project type preserves the others");
    auto legacyProject = persistence.toJson(documentationModel);
    auto legacyAcademic = legacyProject.value(QStringLiteral("academic")).toObject();
    legacyAcademic.remove(QStringLiteral("enabled")); legacyAcademic.remove(QStringLiteral("projectTypes")); legacyAcademic.remove(QStringLiteral("reportDocumentation")); legacyAcademic.insert(QStringLiteral("academicMode"), QStringLiteral("thesis"));
    legacyProject.insert(QStringLiteral("academic"), legacyAcademic);
    ProjectModel migratedLegacy;
    check(persistence.fromJson(&migratedLegacy, legacyProject), "Legacy academic project loads");
    check(migratedLegacy.academicConfiguration().enabled && migratedLegacy.academicConfiguration().projectTypes == QStringList{QStringLiteral("thesis-project")}, "Legacy Thesis migrates to Thesis Project");
    ProjectModel legacyDisabled;
    legacyProject.insert(QStringLiteral("academic"), QJsonObject{{QStringLiteral("academicMode"), QStringLiteral("disabled")} });
    check(persistence.fromJson(&legacyDisabled, legacyProject) && !legacyDisabled.academicConfiguration().enabled && legacyDisabled.academicConfiguration().projectTypes.isEmpty(), "Legacy Disabled migrates to disabled academic state");
    ProjectModel thesisTemplateModel;
    check(manager.applyTemplate(&thesisTemplateModel, QStringLiteral("bachelor-thesis")), "Thesis template initializes academic project type");
    check(thesisTemplateModel.academicConfiguration().projectTypes.contains(QStringLiteral("thesis-project")), "Official Thesis template uses multi-select project types");

    ProjectModel academicUiModel;
    ProjectAcademicPage academicPage(&academicUiModel);
    const auto academicChecks = academicPage.findChildren<QCheckBox*>();
    const auto hasExactText = [&](const QString& label) {
        return std::count_if(academicChecks.cbegin(), academicChecks.cend(), [&label](QCheckBox* box) { return box->text() == label; });
    };
    check(hasExactText(QStringLiteral("Thesis")) == 1 && hasExactText(QStringLiteral("Report")) == 1, "Academic UI exposes one Thesis and one Report selection");
    check(hasExactText(QStringLiteral("Enabled")) == 0, "Academic UI has no redundant Enabled checkbox");
    check(hasExactText(QStringLiteral("Thesis Project")) == 0 && hasExactText(QStringLiteral("Report Project")) == 0, "Academic UI hides internal project-type labels");
    academicPage.close();

    audit.insert("validation", QJsonObject{{"checks", checks}, {"failures", failures}, {"catalogFingerprint", TemplateValidation::catalogFingerprint()}});
    saveJson(fixture.filePath("template-audit.json"), audit);
    QTextStream(stdout) << "Template checks: " << checks << ", failures: " << failures << "\nEvidence: " << fixture.path() << '\n';
    return failures ? 1 : 0;
}
