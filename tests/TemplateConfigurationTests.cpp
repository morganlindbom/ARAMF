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
    check(definitions.size() == 12, "all 12 built-ins audited");
    const auto officialDefinitions = manager.officialDefinitions();
    check(officialDefinitions.size() == 2, "two official templates available");
    for (const auto& official : officialDefinitions) {
        ProjectModel officialModel;
        check(manager.applyTemplate(&officialModel, official.id), official.id + " official template applies");
        check(officialModel.templateModules().contains(official.id), official.id + " provenance retained");
    }
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
    const QMap<QString, QStringList> languages{
        {"pico-2w-visual-designer", {"cpp", "c", "pio-assembly"}}, {"android-studio-kotlin-gemini", {"kotlin"}},
        {"qt-desktop-application", {"cpp"}}, {"cpp-command-line", {"cpp"}}, {"cmake-library", {"cpp"}},
        {"raspberry-pi-pico-firmware", {"cpp", "c", "pio-assembly"}}, {"react-frontend", {"typescript", "html", "css"}},
        {"python-backend", {"python"}}, {"csharp-backend", {"csharp"}}, {"mobile-application", {"kotlin"}},
        {"full-stack-web-application", {"typescript", "html", "css"}}, {"bachelor-thesis", {"cpp"}}
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
        check((d.academic.academicMode != "disabled") == (d.id == "bachelor-thesis"), d.id + " academic applicability");
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
    check(restored.projectName() == "New target" && restored.projectId() == newId && restored.projectPath() == fixture.filePath("custom-target"), "custom does not overwrite target identity");
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
        check(firstRowColumns.size() >= 4 || boxes.size() < 4, "module grid uses four columns");
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
                Q_UNUSED(viewport);
            }
        }
    }
    responsive.close();

    audit.insert("validation", QJsonObject{{"checks", checks}, {"failures", failures}, {"catalogFingerprint", TemplateValidation::catalogFingerprint()}});
    saveJson(fixture.filePath("template-audit.json"), audit);
    QTextStream(stdout) << "Template checks: " << checks << ", failures: " << failures << "\nEvidence: " << fixture.path() << '\n';
    return failures ? 1 : 0;
}
