#include "core/FrameworkKnowledge.h"
#include "core/ImprovementBacklog.h"
#include <QStandardPaths>
#include <QUuid>
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/workflow/WorkflowWidget.h"
#include "ui/workflow/WorkflowPageId.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTextStream>
#include <QStyle>
#include <QStyleOptionButton>

namespace {
QString regressionRoot;
QString failureDetail;
bool fail(const QString& message) { failureDetail = message; return false; }
const QList<WorkflowPageId> pageIds{
    WorkflowPageId::Setup, WorkflowPageId::ProjectIdentity, WorkflowPageId::ProjectModulesTemplates, WorkflowPageId::Academic, WorkflowPageId::Languages,
    WorkflowPageId::Frameworks, WorkflowPageId::DevelopmentTools, WorkflowPageId::Platforms,
    WorkflowPageId::HardwareArchitecture, WorkflowPageId::BuildDelivery,
    WorkflowPageId::AiAgents, WorkflowPageId::AiResponsibilities, WorkflowPageId::AiAutonomy,
    WorkflowPageId::AiIntegration, WorkflowPageId::ResourceInventory,
    WorkflowPageId::ResourceAuthority, WorkflowPageId::ResourcePolicy,
    WorkflowPageId::RuleSelection, WorkflowPageId::RuleRouting,
    WorkflowPageId::MemoryCapture, WorkflowPageId::MemoryMaintenance,
    WorkflowPageId::ReleaseOverview, WorkflowPageId::ProductVersion, WorkflowPageId::ComponentVersions,
    WorkflowPageId::SchemaCompatibility, WorkflowPageId::ReleaseReadiness, WorkflowPageId::ApprovalHistory,
    WorkflowPageId::Review, WorkflowPageId::Generate, WorkflowPageId::Verify,
    WorkflowPageId::Finalize, WorkflowPageId::UpdateReview, WorkflowPageId::UpdateApply,
    WorkflowPageId::UpdateConfiguration, WorkflowPageId::ImprovementBacklog
};

bool isHistoricalManualScenario(int n) {
    return (n >= 251 && n <= 260) || (n >= 351 && n <= 360)
        || (n >= 401 && n <= 420) || (n >= 541 && n <= 550);
}

QString level(int n) {
    if ((n >= 251 && n <= 260) || (n >= 351 && n <= 360) || (n >= 401 && n <= 420) || (n >= 541 && n <= 550)) return "GUI Manual";
    if (n >= 261 && n <= 400 || n >= 531 && n <= 540 || n >= 541 && n <= 550) return "GUI Automated";
    if (n >= 421 && n <= 520 || n >= 521 && n <= 530) return "System";
    return "Core Regression";
}

QString category(int n) {
    if (n <= 350) return "Actual Qt GUI workflow";
    if (n <= 385) return "Save/Open/Save As/dialogs/cancellation";
    if (n <= 420) return "Navigation/scroll/zoom/window/monitor behavior";
    if (n <= 450) return "Persistence/migration/older-project compatibility";
    if (n <= 480) return "Failure injection/filesystem/corrupt-data handling";
    if (n <= 500) return "Generate/Verify/Finalize extreme combinations";
    if (n <= 520) return "Framework Knowledge";
    if (n <= 530) return "AI bootstrap/provider integration";
    if (n <= 540) return "Repetition/idempotence/long-running behavior";
    return "Startup/shutdown/recovery";
}

void writeText(const QString& path, const QString& text) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly) || f.write(text.toUtf8()) != text.toUtf8().size()) qFatal("Cannot persist release-regression evidence");
}

void dismissModal() {
    if (auto* modal = QApplication::activeModalWidget()) {
        if (auto* box = qobject_cast<QMessageBox*>(modal)) box->accept();
        else if (auto* dialog = qobject_cast<QDialog*>(modal)) dialog->reject();
    }
}

bool clickButton(MainWindow& window, const QString& text) {
    for (auto* button : window.findChildren<QPushButton*>()) {
        if (button->text().contains(text, Qt::CaseInsensitive) && button->isEnabled()) {
            QTimer::singleShot(0, &dismissModal);
            QTest::mouseClick(button, Qt::LeftButton);
            QTest::qWait(5);
            return true;
        }
    }
    return false;
}

bool exerciseGui(int n) {
    MainWindow window(999, 1000, 600);
    window.show();
    QTest::qWait(10);
    auto* workflow = window.findChild<WorkflowWidget*>();
    auto* list = workflow ? workflow->findChild<QListWidget*>() : nullptr;
    auto* scroll = window.findChild<QScrollArea*>("workflowPageScroll");
    if (!workflow || !list || !scroll) return fail("Missing canonical workflow navigation or page scroll host");
    const bool reverse = n % 2 == 0;
    for (int i = 0; i < pageIds.size(); ++i) {
        const auto page = pageIds.at(reverse ? pageIds.size() - i - 1 : i);
        auto* item = workflow->navigationItem(page);
        if (!item) return fail("Missing page: " + workflowPageKey(page));
        list->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        QTest::qWait(2);
        // visualItemRect is in viewport coordinates, not QListWidget coordinates.
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(item).center());
        QTest::qWait(2);
        if (list->currentItem() != item || workflow->currentPage() != page)
            return fail("Widget navigation failed: " + workflowPageKey(page));
        if (scroll->horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
            return fail("Page host violates current horizontal-scroll policy");
    }
    auto* selected = workflow->navigationItem(pageIds.at(n % pageIds.size()));
    list->scrollToItem(selected, QAbstractItemView::PositionAtCenter);
    QTest::qWait(2);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(selected).center());
    QTest::qWait(2);
    for (auto* check : window.findChildren<QCheckBox*>()) {
        if (check->isVisible() && check->isEnabled()) {
            const bool before = check->isChecked();
            scroll->ensureWidgetVisible(check);
            QTest::qWait(2);
            QStyleOptionButton option; option.initFrom(check);
            const auto hit = check->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, check).center();
            QTest::mouseClick(check, Qt::LeftButton, Qt::NoModifier, hit);
            if (check->isChecked() == before) return fail("Visible checkbox did not toggle: " + check->text());
            QTest::mouseClick(check, Qt::LeftButton, Qt::NoModifier, hit);
            if (check->isChecked() != before) return fail("Visible checkbox did not restore: " + check->text());
            break;
        }
    }
    if (n % 4 == 0) {
        QTest::keyClick(&window, Qt::Key_Plus, Qt::ControlModifier);
        QTest::keyClick(&window, Qt::Key_Minus, Qt::ControlModifier);
        QTest::keyClick(&window, Qt::Key_0, Qt::ControlModifier);
    }
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    window.close();
    return true;
}

bool exerciseSystem(int n) {
    QTemporaryDir target(regressionRoot + "/fixtures/system-" + QString::number(n) + "-XXXXXX");
    target.setAutoRemove(false);
    if (!target.isValid()) return fail("Cannot create isolated system fixture");
    ProjectModel model;
    model.setProjectId(QString("release-%1").arg(n));
    model.setProjectName(QString("Release validation %1").arg(n));
    model.setProjectPath(target.path());
    ProjectPersistence persistence;
    QString file = QDir(target.path()).filePath("project.aramf.json");
    QString error;
    if (!TemplateManager(nullptr, regressionRoot + "/templates.json").applyTemplate(&model, "cpp-command-line", &error)) return fail("Fixture template: " + error);
    model.setProjectPath(target.path()); model.setProjectFilePath(file);
    if (!persistence.save(model, file, &error)) return fail("Save: " + error);
    ProjectModel loaded;
    if (!persistence.load(&loaded, file, &error)) return fail("Reload: " + error);
    loaded.setProjectPath(target.path());
    loaded.setProjectFilePath(file);
    if (n >= 501 && n <= 520) {
        FrameworkKnowledgeService knowledge;
        const QString title = "Release candidate " + QString::number(n % 4);
        const QString lesson = "Optional components must not impose downstream requirements when excluded.";
        const QString id = knowledge.propose(target.path(), title, lesson,
            {"lifecycle", "selective-generation"}, {QString("TEST-%1").arg(n), "isolated evidence"}, true, &error);
        if (id.isEmpty()) return fail("Propose knowledge: " + error);
        if (!knowledge.approvedEntries(target.path(), {"lifecycle"}, &error).isEmpty() || !error.isEmpty()) return fail("Candidate incorrectly approved or knowledge read failed: " + error);
        if (n % 4 == 0) {
            if (!knowledge.approve(target.path(), id, "explicit-isolated-test-review", &error)) return fail("Test-fixture approval: " + error);
            const auto approved = knowledge.approvedEntries(target.path(), {"lifecycle"}, &error);
            if (approved.size() != 1 || approved.first().id != id) return fail("Approval readback does not match candidate");
        }
        if (knowledge.entries(target.path(), &error).size() != 1) return fail("Candidate persistence: " + error);
        return true;
    }
    if (n >= 521 && n <= 530) {
        AiConfiguration ai; ai.primaryAgent = n % 2 ? "claude-code" : "github-copilot"; ai.additionalAgents = {"gemini"}; ai.permissions = {"read-project-files"};
        loaded.setAiConfiguration(ai);
        GenerationOptions generationOptions;
        GenerationServices generation;
        const auto generated = generation.generate(loaded, generationOptions);
        if (!generated.success) return fail("Bootstrap generation: " + generated.error);
        AgentEntryPointService service;
        const auto result = service.createEntryPoints(loaded);
        if (!result.success || !QFile::exists(QDir(target.path()).filePath("AGENTS.md"))
            || !QFile::exists(QDir(target.path()).filePath("ARAMF_WORKER/AGENTS.md"))) return fail("Bootstrap: " + result.errors.join("; "));
        return true;
    }
    if (n >= 451 && n <= 480) {
        QFile malformed(QDir(target.path()).filePath("ARAMF_WORKER/memory/framework-knowledge.json"));
        QDir().mkpath(QFileInfo(malformed).absolutePath());
        if (!malformed.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return fail("Cannot create malformed-store fixture");
        malformed.write(n % 2 ? "{" : "[]"); malformed.close();
        FrameworkKnowledgeService knowledge;
        QString failure;
        const auto entries = knowledge.entries(target.path(), &failure);
        if (!entries.isEmpty() || failure.isEmpty()) return fail("Malformed knowledge was not rejected");
        return true;
    }
    if (loaded.projectId() != model.projectId() || loaded.projectName() != model.projectName()) return fail("Persistence identity mismatch");
    return true;
}

QString resultMarkdown(int n, bool passed) {
    return QString("# TEST-%1\n\n## Historical category\n\n%2\n\n## Current coverage\n\n%3\n\n"
                   "## Result\n\n%4\n\n## Details\n\n%5\n\n"
                   "Only this invocation is reported. No prior retest, manual inspection or release certification is implied.\n")
        .arg(n).arg(category(n))
        .arg(n <= 400 || n >= 531 ? "Native MainWindow widget navigation, visible control interaction and scrolling; no manual visual certification."
             : n >= 521 ? "Canonical generation and provider bootstrap."
             : n >= 501 ? "Isolated candidate/approval persistence and separation."
             : n >= 451 && n <= 480 ? "Corrupt Framework Knowledge rejection."
             : "Current project Save/Load identity preservation; no legacy migration or full lifecycle claim.")
        .arg(passed ? "PASS" : "FAIL").arg(passed ? "Assertions passed." : failureDetail);
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    regressionRoot = QDir::current().filePath("build/test_550/runs/" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QDir().mkpath(regressionRoot + "/fixtures");
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(regressionRoot + "/global/framework-knowledge-library.json");
    ImprovementBacklogService::setPathForTests(regressionRoot + "/global/improvement-backlog.json");
    QString globalError;
    if (!FrameworkKnowledgeService().ensureGlobalLibrary(&globalError)) { qCritical("%s", qPrintable(globalError)); return 2; }
    const int selected = app.arguments().contains("--scenario") ? app.arguments().value(app.arguments().indexOf("--scenario") + 1).toInt() : 0;
    if (app.arguments().contains("--scenario") && (selected < 251 || selected > 550 || isHistoricalManualScenario(selected))) return 2;
    const bool injectFailure = app.arguments().contains("--inject-failure");
    QTextStream(stdout) << "EVIDENCE " << regressionRoot << Qt::endl;
    int automated = 0, guiPass = 0, systemPass = 0, failures = 0, historicalManualExcluded = 0;
    for (int n = 251; n <= 550; ++n) {
        if (selected && selected != n) continue;
        if (isHistoricalManualScenario(n)) { ++historicalManualExcluded; continue; }
        failureDetail.clear();
        const bool passed = injectFailure ? fail("Injected reporting negative control") : ((n <= 400 || n >= 531) ? exerciseGui(n) : exerciseSystem(n));
        ++automated;
        if (passed) { if (n <= 400 || n >= 531) ++guiPass; else ++systemPass; }
        else { ++failures; QTextStream(stdout) << "FAIL " << n << ": " << failureDetail << Qt::endl; }
        const QString id = QString("test_%1").arg(n, 3, 10, QLatin1Char('0'));
        writeText(regressionRoot + "/scenarios/" + id + "/result.md", resultMarkdown(n, passed));
    }
    QJsonObject campaign{{"planned", selected ? 1 : 250}, {"completed", automated},
        {"passed", automated - failures}, {"failed", failures}, {"guiAutomatedPass", guiPass},
        {"systemPass", systemPass}, {"historicalManualExcluded", historicalManualExcluded},
        {"guiManualPass", 0}, {"injectedFailure", injectFailure}};
    writeText(regressionRoot + "/campaign.json", QJsonDocument(campaign).toJson(QJsonDocument::Indented));
    writeText(regressionRoot + "/info.md", QString("# Current test_550 automated subset\n\nExecuted: %1\nPASS: %2\nFAIL: %3\nManual cases not run: %4\n\n"
        "No historical pass-equivalence, automatic retest or full release-certification claim.\n"
        "Per-scenario current coverage is explicit; historical category labels alone do not prove that coverage.\n")
        .arg(automated).arg(automated - failures).arg(failures).arg(historicalManualExcluded));
    QTextStream(stdout) << "automatedCompleted=" << automated << " automatedPass=" << (automated - failures)
                        << " historicalManualExcluded=" << historicalManualExcluded << " failures=" << failures << Qt::endl;
    return failures == 0 && automated == (selected ? 1 : 250) ? 0 : 1;
}
