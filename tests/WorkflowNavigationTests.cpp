#include "ui/workflow/WorkflowWidget.h"
#include "ui/workflows/ai/agents/AiAgentsPage.h"
#include "ui/workflows/ai/responsibilities/AiResponsibilitiesPage.h"
#include "ui/workflows/ai/autonomy/AiAutonomyPage.h"
#include "ui/workflows/ai/integration/AiIntegrationPage.h"
#include "ui/workflows/memory/maintenance/MemoryMaintenancePage.h"
#include "core/ProjectModel.h"
#include "core/ProjectMemory.h"
#include "core/FrameworkKnowledge.h"
#include "ui/workflows/resources/authority/ResourceAuthorityPage.h"
#include "ui/workflows/update/review/FrameworkKnowledgeReviewPage.h"
#include "ui/workflows/update/apply/FrameworkKnowledgeApplyPage.h"
#include "ui/workflows/update/backlog/ImprovementBacklogPage.h"
#include "core/CodexExecutionAdapter.h"
#include "core/AramfPaths.h"
#include "core/ImprovementBacklog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <cmath>
#include <iostream>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir globalData;
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(QDir(globalData.path()).filePath(QStringLiteral("ARAMF_DATA/framework-knowledge-library.json")));
    ImprovementBacklogService::setPathForTests(QDir(globalData.path()).filePath(QStringLiteral("ARAMF_DATA/aramf-improvement-backlog.json")));
    QFile::remove(FrameworkKnowledgeService().legacyGlobalLibraryPath());
    WorkflowWidget workflow;
    workflow.setStepCount(26);

    auto* list = workflow.findChild<QListWidget*>();
    bool ok = require(list != nullptr, "workflow list must exist");
    if (!ok) return 1;

    QList<WorkflowPageId> selected;
    QObject::connect(&workflow, &WorkflowWidget::pageSelected,
                     [&selected](WorkflowPageId page) { selected << page; });

    const QList<int> rows{1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17, 19, 20, 22, 23, 25, 26, 27, 28, 30, 31, 32};
    const QList<WorkflowPageId> expected{
        WorkflowPageId::Setup,
        WorkflowPageId::Academic,
        WorkflowPageId::Languages,
        WorkflowPageId::Frameworks,
        WorkflowPageId::DevelopmentTools,
        WorkflowPageId::Platforms,
        WorkflowPageId::HardwareArchitecture,
        WorkflowPageId::BuildDelivery,
        WorkflowPageId::AiAgents,
        WorkflowPageId::AiResponsibilities,
        WorkflowPageId::AiAutonomy,
        WorkflowPageId::AiIntegration,
        WorkflowPageId::ResourceInventory,
        WorkflowPageId::ResourceAuthority,
        WorkflowPageId::ResourcePolicy,
        WorkflowPageId::RuleSelection,
        WorkflowPageId::RuleRouting,
        WorkflowPageId::MemoryCapture,
        WorkflowPageId::MemoryMaintenance,
        WorkflowPageId::Review,
        WorkflowPageId::Generate,
        WorkflowPageId::Verify,
        WorkflowPageId::Finalize,
        WorkflowPageId::UpdateReview,
        WorkflowPageId::UpdateApply,
        WorkflowPageId::ImprovementBacklog
    };

    for (int i = 0; i < rows.size(); ++i) {
        list->setCurrentRow(rows.at(i));
        app.processEvents();
        ok &= require(workflow.currentPage() == expected.at(i),
                      "clickable row must emit its explicit page identifier");
    }

    const int selectionCount = selected.size();
    list->setCurrentRow(0);
    app.processEvents();
    ok &= require(selected.size() == selectionCount,
                  "heading rows must not emit page selection");
    ok &= require(list->item(0)->flags() == Qt::NoItemFlags,
                  "heading rows must not be selectable");

    ProjectModel model;
    AiAgentsPage agentsPage(&model);
    auto* primary = agentsPage.findChild<QComboBox*>();
    const auto additional = [&agentsPage](const QString& id) {
        for (auto* check : agentsPage.findChildren<QCheckBox*>()) {
            if (check->property("capabilityId").toString() == id) return check;
        }
        return static_cast<QCheckBox*>(nullptr);
    };
    ok &= require(primary != nullptr, "AI primary agent selector must exist");
    auto* codex = additional(QStringLiteral("openai-codex"));
    auto* chatgpt = additional(QStringLiteral("chatgpt"));
    auto* claudeCode = additional(QStringLiteral("claude-code"));
    ok &= require(codex && chatgpt && claudeCode, "AI additional agent checkboxes must expose stable IDs");
    ok &= require(codex->isEnabled() && chatgpt->isEnabled() && claudeCode->isEnabled(),
                  "None must enable every additional agent");

    const auto selectPrimary = [&](const QString& id) {
        primary->setCurrentIndex(primary->findData(id));
        app.processEvents();
    };
    chatgpt->setChecked(true);
    app.processEvents();
    selectPrimary(QStringLiteral("openai-codex"));
    ok &= require(!codex->isEnabled() && codex->isChecked() == false,
                  "Codex must be disabled and unselected as an additional agent");
    ok &= require(chatgpt->isEnabled(), "unrelated additional agents must remain enabled");
    selectPrimary(QStringLiteral("chatgpt"));
    ok &= require(codex->isEnabled() && !chatgpt->isEnabled() && !chatgpt->isChecked(),
                  "switching to ChatGPT must re-enable Codex and disable ChatGPT");
    ok &= require(!model.aiConfiguration().additionalAgents.contains(QStringLiteral("chatgpt")),
                  "primary agent must not remain in additional agents");
    selectPrimary(QStringLiteral("claude-code"));
    ok &= require(chatgpt->isEnabled() && !claudeCode->isEnabled(),
                  "switching to Claude Code must re-enable ChatGPT");
    selectPrimary(QStringLiteral("none"));
    ok &= require(codex->isEnabled() && chatgpt->isEnabled() && claudeCode->isEnabled(),
                  "None must re-enable every additional agent");

    const auto checkSelectAllPage = [&app, &ok](QWidget& page, const QString& highRiskTitle = QString()) {
        auto* button = page.findChild<QPushButton*>();
        ok &= require(button != nullptr, "AI capability page must have Select All control");
        if (!button) return;
        ok &= require(button->text() == QStringLiteral("Select All"), "partial capability state must show Select All");
        button->click();
        app.processEvents();
        ok &= require(button->text() == QStringLiteral("Clear All"), "Select All must switch to Clear All");
        for (auto* check : page.findChildren<QCheckBox*>()) {
            bool exempt = false;
            if (!highRiskTitle.isEmpty()) {
                for (auto* group : page.findChildren<QGroupBox*>()) {
                    if (group->title() == highRiskTitle && group->isAncestorOf(check)) exempt = true;
                }
            }
            if (!exempt && check->isEnabled()) {
                ok &= require(check->isChecked(), "Select All must select applicable enabled options");
            }
        }
        if (!highRiskTitle.isEmpty()) {
            for (auto* group : page.findChildren<QGroupBox*>()) {
                if (group->title() == highRiskTitle) {
                    for (auto* check : group->findChildren<QCheckBox*>()) {
                        ok &= require(!check->isChecked(), "Select All must not select high-risk permissions");
                    }
                }
            }
        }
        auto* first = page.findChildren<QCheckBox*>().value(0, nullptr);
        if (first) first->setChecked(false);
        app.processEvents();
        ok &= require(button->text() == QStringLiteral("Select All"), "manual clearing must update Select All state");
        button->click();
        app.processEvents();
        ok &= require(button->text() == QStringLiteral("Clear All"), "Select All must restore normal selections");
        button->click();
        app.processEvents();
        ok &= require(button->text() == QStringLiteral("Select All"), "Clear All must clear the page");
        for (auto* check : page.findChildren<QCheckBox*>()) {
            ok &= require(!check->isChecked(), "Clear All must clear every permission/capability");
        }
    };

    AiResponsibilitiesPage responsibilities(&model);
    checkSelectAllPage(responsibilities);
    AiAutonomyPage autonomy(&model);
    checkSelectAllPage(autonomy, QStringLiteral("High-Risk Actions"));
    AiIntegrationPage integration(&model);
    checkSelectAllPage(integration);

    MemoryMaintenancePage memoryPage(&model);
    const auto memoryPresets = MemoryMaintenancePage::memorySizePresets();
    const QList<qint64> expectedMemorySizes{
        500LL * 1024LL * 1024LL,
        1LL * 1024LL * 1024LL * 1024LL,
        2LL * 1024LL * 1024LL * 1024LL,
        5LL * 1024LL * 1024LL * 1024LL,
        10LL * 1024LL * 1024LL * 1024LL,
        20LL * 1024LL * 1024LL * 1024LL,
        50LL * 1024LL * 1024LL * 1024LL,
        100LL * 1024LL * 1024LL * 1024LL
    };
    ok &= require(memoryPresets.size() == expectedMemorySizes.size(),
                  "memory preset catalog must contain all standard sizes");
    for (int index = 0; index < memoryPresets.size(); ++index) {
        ok &= require(memoryPresets.at(index).second == expectedMemorySizes.at(index),
                      "memory preset must use the exact byte value");
    }

    auto* memoryPreset = memoryPage.findChild<QComboBox*>(QStringLiteral("memorySizePreset"));
    auto* customSize = memoryPage.findChild<QDoubleSpinBox*>(QStringLiteral("customMemorySize"));
    auto* customUnit = memoryPage.findChild<QComboBox*>(QStringLiteral("customMemoryUnit"));
    ok &= require(memoryPreset && customSize && customUnit,
                  "memory size preset and custom controls must exist");
    if (memoryPreset && customSize && customUnit) {
        ok &= require(memoryPreset->currentIndex() == 4
                          && model.memoryConfiguration().maximumSizeBytes == expectedMemorySizes.at(4),
                      "memory size must default to 10 GB");
        for (int index = 0; index < memoryPresets.size(); ++index) {
            memoryPreset->setCurrentIndex(index);
            app.processEvents();
            ok &= require(model.memoryConfiguration().maximumSizeBytes == expectedMemorySizes.at(index),
                          "selecting a memory preset must update bytes");
        }

        memoryPreset->setCurrentIndex(4);
        app.processEvents();
        memoryPreset->setCurrentIndex(memoryPresets.size());
        app.processEvents();
        ok &= require(customSize->decimals() == 0 && std::abs(customSize->value() - 10.0) < 0.000001,
                      "10 GB custom display must not show trailing decimals");
        customUnit->setCurrentIndex(customUnit->findData(QStringLiteral("mb")));
        customSize->setValue(2048.0);
        app.processEvents();
        const qint64 twoGiB = 2LL * 1024LL * 1024LL * 1024LL;
        ok &= require(model.memoryConfiguration().maximumSizeBytes == twoGiB,
                      "custom MB value must persist as exact bytes");
        customUnit->setCurrentIndex(customUnit->findData(QStringLiteral("gb")));
        app.processEvents();
        ok &= require(std::abs(customSize->value() - 2.0) < 0.000001
                          && model.memoryConfiguration().maximumSizeBytes == twoGiB,
                      "switching custom units must preserve bytes");

        customUnit->setCurrentIndex(customUnit->findData(QStringLiteral("mb")));
        customSize->setValue(750.0);
        app.processEvents();
        const qint64 sevenHundredFiftyMiB = 750LL * 1024LL * 1024LL;
        ok &= require(model.memoryConfiguration().maximumSizeBytes == sevenHundredFiftyMiB,
                      "custom 750 MB value must be exact");
        customUnit->setCurrentIndex(customUnit->findData(QStringLiteral("gb")));
        app.processEvents();
        ok &= require(std::abs(customSize->value() - (750.0 / 1024.0)) < 0.000001
                          && model.memoryConfiguration().maximumSizeBytes == sevenHundredFiftyMiB,
                      "non-integer custom unit conversion must preserve bytes");
        ok &= require(customSize->decimals() == 9,
                      "fractional GB display must retain sufficient precision");
        customUnit->setCurrentIndex(customUnit->findData(QStringLiteral("mb")));
        app.processEvents();
        ok &= require(std::abs(customSize->value() - 750.0) < 0.000001,
                      "switching back to MB must restore the exact display value");
        customSize->setValue(1536.0);
        app.processEvents();
        customUnit->setCurrentIndex(customUnit->findData(QStringLiteral("gb")));
        app.processEvents();
        ok &= require(customSize->decimals() == 1 && std::abs(customSize->value() - 1.5) < 0.000001,
                      "1.5 GB display must omit unnecessary trailing zeroes");
    }

    ProjectModel authorityModel;
    ProjectResource authorityA;
    authorityA.id = QStringLiteral("authority-a");
    authorityA.name = QStringLiteral("CMakeLists.txt");
    authorityA.location = QStringLiteral("C:/project/CMakeLists.txt");
    authorityA.authorityLevel = QStringLiteral("primary-source-of-truth");
    ProjectResource authorityB;
    authorityB.id = QStringLiteral("authority-b");
    authorityB.name = QStringLiteral("src");
    authorityB.location = QStringLiteral("C:/project/src");
    authorityB.authorityLevel = QStringLiteral("trusted-reference");
    ProjectResource authorityC;
    authorityC.id = QStringLiteral("authority-c");
    authorityC.name = QStringLiteral("tests");
    authorityC.location = QStringLiteral("C:/project/tests");
    authorityC.authorityLevel = QStringLiteral("supporting-reference");
    authorityModel.setResources({authorityA, authorityB, authorityC});

    ResourceAuthorityPage authorityPage(&authorityModel);
    app.processEvents();
    auto* authorityList = authorityPage.findChild<QListWidget*>();
    auto* authorityCombo = authorityPage.findChild<QComboBox*>();
    ok &= require(authorityList && authorityCombo, "resource authority controls must exist");
    bool hasAuthorityHelp = false;
    for (auto* label : authorityPage.findChildren<QLabel*>()) {
        hasAuthorityHelp = label->text().contains(QStringLiteral("within its applicable scopes"));
        if (hasAuthorityHelp) break;
    }
    ok &= require(hasAuthorityHelp, "resource authority selector must explain Source of Truth strength");

    auto authorityAi = authorityModel.aiConfiguration();
    authorityAi.primaryAgent = QStringLiteral("openai-codex");
    authorityAi.permissions = {QStringLiteral("read-project-files"), QStringLiteral("modify-files")};
    authorityModel.setAiConfiguration(authorityAi);
    FrameworkKnowledgeReviewPage updateReviewPage(&authorityModel);
    FrameworkKnowledgeApplyPage updateApplyPage(&authorityModel);
    ok &= require(updateReviewPage.findChild<QListWidget*>(QStringLiteral("frameworkKnowledgeEntries")) != nullptr,
                  "UPDATE review page must expose Framework Knowledge entries");
    ok &= require(updateApplyPage.findChild<QListWidget*>(QStringLiteral("approvedFrameworkKnowledgeSelection")) != nullptr,
                  "UPDATE apply page must expose explicit approved-knowledge selection");
    auto* executionInfo = updateApplyPage.findChild<QLabel*>(QStringLiteral("updateExecutionInfo"));
    ok &= require(executionInfo && executionInfo->text().contains(QStringLiteral("Codex status:")),
                  "Page 25 must expose Codex discovery status");
    if (CodexExecutionAdapter::resolution().available) {
        ok &= require(executionInfo->text().contains(CodexExecutionAdapter::resolution().path),
                      "Page 25 must display the resolved Codex executable");
        ok &= require(executionInfo->text().contains(CodexExecutionAdapter::resolution().version),
                      "Page 25 must display the resolved Codex version");
    }

    QTemporaryDir backlogProject;
    QJsonObject backlogReport;
    QString backlogError;
    const bool backlogReported = ImprovementBacklogService().report(backlogProject.path(), QStringLiteral("Page 26 fixture gap"), QStringLiteral("A framework capability was missing during managed-project work."), QStringLiteral("ARAMF should provide a canonical route."), QStringLiteral("workflow"), {QStringLiteral("workflow-test")}, QStringLiteral("backlog acceptance"), QStringLiteral("test"), &backlogReport, &backlogError);
    ImprovementBacklogPage backlogPage(&model);
    backlogPage.show();
    app.processEvents();
    auto* backlogList = backlogPage.findChild<QListWidget*>(QStringLiteral("improvementBacklogItems"));
    auto* backlogSummary = backlogPage.findChild<QLabel*>(QStringLiteral("improvementBacklogSummary"));
    ok &= require(backlogReported && backlogList && backlogList->count() == 1, "Page 26 must display reported observations");
    ok &= require(backlogSummary && backlogSummary->text().contains(QStringLiteral("Observations: 1")), "Page 26 must display backlog summary counts");
    if (backlogList) backlogList->setCurrentRow(0);
    QPushButton* promoteBacklog = nullptr;
    const auto buttons = backlogPage.findChildren<QPushButton*>();
    for (auto* button : buttons) if (button->text() == QStringLiteral("Promote to TODO")) promoteBacklog = button;
    if (promoteBacklog) promoteBacklog->click();
    app.processEvents();
    ok &= require(backlogList && backlogList->item(0)->text().contains(QStringLiteral("TODO-001")), "Page 26 must explicitly promote an observation to TODO");
    auto* backlogFilter = backlogPage.findChild<QComboBox*>(QStringLiteral("improvementBacklogFilter"));
    if (backlogFilter) backlogFilter->setCurrentText(QStringLiteral("TODO"));
    app.processEvents();
    ok &= require(backlogList && backlogList->count() == 1, "Page 26 TODO filter must show accepted TODOs");

    QTemporaryDir updateFixture;
    ProjectModel updateModel;
    updateModel.setProjectPath(updateFixture.path());
    ProjectMemory updateMemory;
    QString updateError;
    ok &= require(updateFixture.isValid() && updateMemory.initialize(updateFixture.path(), &updateModel, &updateError),
                  "UPDATE fixture memory must initialize");
    QDir(updateFixture.path()).mkpath(QStringLiteral("src"));
    QDir(updateFixture.path()).mkpath(QStringLiteral("aramf_setup"));
    QFile updateCmake(QDir(updateFixture.path()).filePath(QStringLiteral("CMakeLists.txt")));
    const bool updateCmakeOpened = updateCmake.open(QIODevice::WriteOnly);
    updateCmake.close();
    ok &= require(updateCmakeOpened, "UPDATE fixture baseline must be writable");
    RuleConfiguration updateRules;
    updateRules.projectScopes = {QStringLiteral("lifecycle"), QStringLiteral("selective-generation"), QStringLiteral("verification")};
    updateModel.setRuleConfiguration(updateRules);
    auto updateAi = updateModel.aiConfiguration();
    updateAi.permissions = {QStringLiteral("read-project-files"), QStringLiteral("modify-files")};
    updateModel.setAiConfiguration(updateAi);
    FrameworkKnowledgeService updateKnowledge;
    const QString updateId = updateKnowledge.propose(
        updateFixture.path(), QStringLiteral("Workflow global promotion fixture %1").arg(updateFixture.path()),
        QStringLiteral("A workflow-only portable lesson used to verify explicit global promotion at %1.").arg(updateFixture.path()),
        {QStringLiteral("lifecycle"), QStringLiteral("selective-generation"), QStringLiteral("optional-components"), QStringLiteral("verification"), QStringLiteral("finalization")}, {}, true, &updateError);
    FrameworkKnowledgeApplyPage liveApplyPage(&updateModel);
    liveApplyPage.show();
    app.processEvents();
    auto* liveApplyList = liveApplyPage.findChild<QListWidget*>(QStringLiteral("approvedFrameworkKnowledgeSelection"));
    const auto containsUpdateEntry = [liveApplyList, &updateId]() {
        if (!liveApplyList) return false;
        for (int row = 0; row < liveApplyList->count(); ++row)
            if (liveApplyList->item(row)->data(Qt::UserRole).toString() == updateId) return true;
        return false;
    };
    ok &= require(!containsUpdateEntry(),
                  "UPDATE page must initially exclude the unapproved fixture knowledge");
    ok &= require(updateKnowledge.approve(updateFixture.path(), updateId, QStringLiteral("test-user"), &updateError),
                  "fixture approval must persist");
    FrameworkKnowledgeReviewPage liveReviewPage(&updateModel);
    liveReviewPage.show();
    app.processEvents();
    auto* globalLocation = liveReviewPage.findChild<QLabel*>(QStringLiteral("globalFrameworkKnowledgeLocation"));
    ok &= require(globalLocation && globalLocation->text().contains(QStringLiteral("ARAMF_DATA")),
                  "Page 24 must report the Global ARAMF storage location");
    if (auto* reviewFilter = liveReviewPage.findChild<QComboBox*>()) reviewFilter->setCurrentIndex(2);
    app.processEvents();
    auto* liveReviewList = liveReviewPage.findChild<QListWidget*>(QStringLiteral("frameworkKnowledgeEntries"));
    if (liveReviewList) {
        for (int row = 0; row < liveReviewList->count(); ++row) {
            if (liveReviewList->item(row)->data(Qt::UserRole).toString() == updateId) liveReviewList->setCurrentRow(row);
        }
    }
    app.processEvents();
    auto* promoteButton = liveReviewPage.findChild<QPushButton*>(QStringLiteral("promoteFrameworkKnowledge"));
    ok &= require(promoteButton && promoteButton->isEnabled(),
                  "approved portable knowledge must expose explicit global promotion");
    QString promotionError;
    if (promoteButton) promoteButton->click();
    app.processEvents();
    const auto clickedGlobalEntries = FrameworkKnowledgeService().globalEntries(&promotionError);
    const bool clickedPromotion = std::any_of(clickedGlobalEntries.cbegin(), clickedGlobalEntries.cend(), [&updateId](const auto& entry) { return entry.id == updateId; });
    ok &= require(clickedPromotion,
                  qPrintable(QStringLiteral("explicit promotion must persist in the global library: %1").arg(promotionError)));
    QTemporaryDir globalOnlySource;
    ProjectModel globalOnlySourceModel;
    globalOnlySourceModel.setProjectPath(globalOnlySource.path());
    ProjectMemory globalOnlySourceMemory;
    QString globalOnlySourceError;
    const QString globalOnlyId = globalOnlySource.isValid() && globalOnlySourceMemory.initialize(globalOnlySource.path(), &globalOnlySourceModel, &globalOnlySourceError)
        ? updateKnowledge.propose(globalOnlySource.path(), QStringLiteral("Global-only Page 25 fixture"), QStringLiteral("This approved entry must be visible without a project-local copy."), {QStringLiteral("lifecycle")}, {}, true, &globalOnlySourceError)
        : QString();
    const bool globalOnlyPromoted = !globalOnlyId.isEmpty()
        && updateKnowledge.approve(globalOnlySource.path(), globalOnlyId, QStringLiteral("test-user"), &globalOnlySourceError)
        && updateKnowledge.promoteToGlobal(globalOnlySource.path(), globalOnlyId, &globalOnlySourceError);
    QTemporaryDir globalOnlyTarget;
    ProjectModel globalOnlyTargetModel;
    globalOnlyTargetModel.setProjectPath(globalOnlyTarget.path());
    RuleConfiguration globalOnlyTargetRules;
    globalOnlyTargetRules.projectScopes = {QStringLiteral("lifecycle")};
    globalOnlyTargetModel.setRuleConfiguration(globalOnlyTargetRules);
    ok &= require(globalOnlyPromoted && updateKnowledge.ensureFile(globalOnlyTarget.path(), &globalOnlySourceError),
                  "global-only Page 25 fixture must be prepared without copying project knowledge");
    FrameworkKnowledgeApplyPage globalOnlyApplyPage(&globalOnlyTargetModel);
    globalOnlyApplyPage.show();
    app.processEvents();
    auto* globalOnlyList = globalOnlyApplyPage.findChild<QListWidget*>(QStringLiteral("approvedFrameworkKnowledgeSelection"));
    bool globalOnlyVisible = false;
    if (globalOnlyList) for (int row = 0; row < globalOnlyList->count(); ++row) {
        const auto* item = globalOnlyList->item(row);
        if (item->data(Qt::UserRole).toString() == globalOnlyId && item->text().contains(QStringLiteral("global"))) globalOnlyVisible = true;
    }
    ok &= require(globalOnlyVisible, "Page 25 must display an approved global-only entry as applicable");
    liveApplyPage.hide();
    liveApplyPage.show();
    app.processEvents();
    liveApplyList = liveApplyPage.findChild<QListWidget*>(QStringLiteral("approvedFrameworkKnowledgeSelection"));
    int checkableCount = 0;
    if (liveApplyList) for (int row = 0; row < liveApplyList->count(); ++row)
        if (liveApplyList->item(row)->flags() & Qt::ItemIsUserCheckable) ++checkableCount;
    ok &= require(checkableCount == liveApplyList->count(), "every active approved entry must have a selectable checkbox");
    auto* selectAllButton = liveApplyPage.findChild<QPushButton*>(QStringLiteral("selectAllFrameworkKnowledge"));
    auto* clearAllButton = liveApplyPage.findChild<QPushButton*>(QStringLiteral("clearAllFrameworkKnowledge"));
    if (selectAllButton) selectAllButton->click();
    bool allSelected = true;
    if (liveApplyList) for (int row = 0; row < liveApplyList->count(); ++row) allSelected &= liveApplyList->item(row)->checkState() == Qt::Checked;
    ok &= require(selectAllButton && allSelected, "Select All selects all active approved knowledge");
    if (clearAllButton) clearAllButton->click();
    bool allCleared = true;
    if (liveApplyList) for (int row = 0; row < liveApplyList->count(); ++row) allCleared &= liveApplyList->item(row)->checkState() == Qt::Unchecked;
    ok &= require(clearAllButton && allCleared, "Clear All clears knowledge selection");
    QListWidgetItem* adoptItem = nullptr;
    if (liveApplyList) for (int row = 0; row < liveApplyList->count(); ++row)
        if (liveApplyList->item(row)->data(Qt::UserRole).toString() == updateId) adoptItem = liveApplyList->item(row);
    if (adoptItem) adoptItem->setCheckState(Qt::Checked);
    QFile updateKnowledgeFileBeforePrepare(QDir(updateFixture.path()).filePath(AramfPaths::FrameworkKnowledge));
    QByteArray updateKnowledgeBeforePrepare;
    if (updateKnowledgeFileBeforePrepare.open(QIODevice::ReadOnly)) updateKnowledgeBeforePrepare = updateKnowledgeFileBeforePrepare.readAll();
    updateKnowledgeFileBeforePrepare.close();
    auto* analyzeButton = liveApplyPage.findChild<QPushButton*>(QStringLiteral("analyzeFrameworkUpdate"));
    auto* prepareButton = liveApplyPage.findChild<QPushButton*>(QStringLiteral("applyFrameworkUpdate"));
    auto* updateStatus = liveApplyPage.findChild<QLabel*>(QStringLiteral("updateStatus"));
    if (analyzeButton) analyzeButton->click();
    app.processEvents();
    if (prepareButton) prepareButton->click();
    app.processEvents();
    const auto preparedProjectEntries = updateKnowledge.entries(updateFixture.path(), &updateError);
    QFile updateKnowledgeFileAfterPrepare(QDir(updateFixture.path()).filePath(AramfPaths::FrameworkKnowledge));
    QByteArray updateKnowledgeAfterPrepare;
    if (updateKnowledgeFileAfterPrepare.open(QIODevice::ReadOnly)) updateKnowledgeAfterPrepare = updateKnowledgeFileAfterPrepare.readAll();
    updateKnowledgeFileAfterPrepare.close();
    ok &= require(updateKnowledgeBeforePrepare == updateKnowledgeAfterPrepare && !preparedProjectEntries.isEmpty(),
                  "Prepare must not mutate project Framework Knowledge");
    QListWidgetItem* preparedItem = nullptr;
    if (liveApplyList) for (int row = 0; row < liveApplyList->count(); ++row)
        if (liveApplyList->item(row)->data(Qt::UserRole).toString() == updateId) preparedItem = liveApplyList->item(row);
    ok &= require(preparedItem && preparedItem->checkState() == Qt::Checked,
                  "Prepare must preserve the selected knowledge checkbox");
    auto* analysisText = liveApplyPage.findChild<QPlainTextEdit*>(QStringLiteral("updateImpactAnalysis"));
    ok &= require(analysisText && preparedItem && preparedItem->text().contains(QStringLiteral("ALREADY_SATISFIED")),
                  "Prepare must preserve the completed applicability analysis");
    auto* executionButton = liveApplyPage.findChild<QPushButton*>(QStringLiteral("executeFrameworkUpdate"));
    ok &= require(updateStatus && updateStatus->text().contains(QStringLiteral("READY")) && executionButton && executionButton->isEnabled(),
                  "Prepare must leave the page ready for explicit Execute");
    if (executionButton) executionButton->click();
    app.processEvents();
    auto* validateButton = liveApplyPage.findChild<QPushButton*>(QStringLiteral("validateFrameworkUpdate"));
    ok &= require(validateButton && validateButton->isEnabled(), "Execute must reach validation-ready state");
    if (validateButton) validateButton->click();
    app.processEvents();
    const auto adoptedProjectEntries = updateKnowledge.entries(updateFixture.path(), &updateError);
    ok &= require(std::any_of(adoptedProjectEntries.cbegin(), adoptedProjectEntries.cend(), [&updateId](const auto& entry) {
        return entry.id == updateId && entry.origin == QStringLiteral("project+global") && entry.status == QStringLiteral("approved");
    }), "explicit Execute adopts selected global knowledge into project memory");
    globalOnlyApplyPage.hide();
    bool originShown = false;
    if (liveReviewList) for (int row = 0; row < liveReviewList->count(); ++row) {
        const auto* item = liveReviewList->item(row);
        if (item->data(Qt::UserRole).toString() == updateId && item->text().contains(QStringLiteral("project+global"))) originShown = true;
    }
    ok &= require(originShown, "Page 24 must show the promoted entry as project+global");
    liveApplyPage.hide();
    liveApplyPage.show();
    app.processEvents();
    ok &= require(containsUpdateEntry(),
                  "UPDATE page activation must refresh newly approved knowledge");
    QListWidgetItem* updateItem = nullptr;
    if (liveApplyList) {
        for (int row = 0; row < liveApplyList->count(); ++row)
            if (liveApplyList->item(row)->data(Qt::UserRole).toString() == updateId) updateItem = liveApplyList->item(row);
    }
    ok &= require(updateItem && updateItem->checkState() == Qt::Unchecked,
                  "approved knowledge must not be automatically selected");
    ok &= require(updateStatus && updateStatus->text().contains(QStringLiteral("Approved and applicable:")),
                  "UPDATE page must explain approved applicability counts");
    ok &= require(authorityModel.resources().at(0).authorityLevel == authorityA.authorityLevel
                  && authorityModel.resources().at(1).authorityLevel == authorityB.authorityLevel
                  && authorityModel.resources().at(2).authorityLevel == authorityC.authorityLevel,
                  "opening ResourceAuthorityPage must not mutate model authority values");
    if (authorityList && authorityCombo) {
        authorityList->setCurrentRow(1);
        app.processEvents();
        authorityCombo->setCurrentIndex(authorityCombo->findData(QStringLiteral("authoritative")));
        app.processEvents();
        authorityList->setCurrentRow(2);
        app.processEvents();
        authorityCombo->setCurrentIndex(authorityCombo->findData(QStringLiteral("primary-source-of-truth")));
        app.processEvents();
        authorityList->setCurrentRow(0);
        app.processEvents();
        ok &= require(authorityModel.resources().at(0).authorityLevel == authorityA.authorityLevel,
                      "selecting another resource must not change the first authority");
        ok &= require(authorityModel.resources().at(1).authorityLevel == QStringLiteral("authoritative"),
                      "authority combo must update only the selected resource");
        ok &= require(authorityModel.resources().at(2).authorityLevel == QStringLiteral("primary-source-of-truth"),
                      "different authority levels must remain attached to their resource IDs");
        authorityModel.setDescription(QStringLiteral("trigger authority page refresh"));
        app.processEvents();
        ok &= require(authorityModel.resources().at(0).authorityLevel == authorityA.authorityLevel
                      && authorityModel.resources().at(1).authorityLevel == QStringLiteral("authoritative")
                      && authorityModel.resources().at(2).authorityLevel == QStringLiteral("primary-source-of-truth"),
                      "authority values must survive model-driven list rebuilds");
        ResourceAuthorityPage reopenedAuthorityPage(&authorityModel);
        app.processEvents();
        ok &= require(authorityModel.resources().at(0).authorityLevel == authorityA.authorityLevel
                      && authorityModel.resources().at(1).authorityLevel == QStringLiteral("authoritative")
                      && authorityModel.resources().at(2).authorityLevel == QStringLiteral("primary-source-of-truth"),
                      "constructing a second authority page must be read-only");
    }

    ImprovementBacklogService::clearPathForTests();
    FrameworkKnowledgeService::clearGlobalLibraryPathForTests();
    return ok ? 0 : 1;
}
