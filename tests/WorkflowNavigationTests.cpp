#include "ui/workflow/WorkflowWidget.h"
#include "ui/workflows/ai/agents/AiAgentsPage.h"
#include "ui/workflows/ai/responsibilities/AiResponsibilitiesPage.h"
#include "ui/workflows/ai/autonomy/AiAutonomyPage.h"
#include "ui/workflows/ai/integration/AiIntegrationPage.h"
#include "ui/workflows/memory/maintenance/MemoryMaintenancePage.h"
#include "core/ProjectModel.h"
#include "ui/workflows/resources/authority/ResourceAuthorityPage.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QListWidget>
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
    WorkflowWidget workflow;
    workflow.setStepCount(23);

    auto* list = workflow.findChild<QListWidget*>();
    bool ok = require(list != nullptr, "workflow list must exist");
    if (!ok) return 1;

    QList<WorkflowPageId> selected;
    QObject::connect(&workflow, &WorkflowWidget::pageSelected,
                     [&selected](WorkflowPageId page) { selected << page; });

    const QList<int> rows{1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17, 19, 20, 22, 23, 25, 26, 27, 28};
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
        WorkflowPageId::Finalize
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

    return ok ? 0 : 1;
}
