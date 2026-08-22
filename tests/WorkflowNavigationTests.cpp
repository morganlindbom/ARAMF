#include "ui/workflow/WorkflowWidget.h"
#include "ui/workflows/ai/agents/AiAgentsPage.h"
#include "ui/workflows/ai/responsibilities/AiResponsibilitiesPage.h"
#include "ui/workflows/ai/autonomy/AiAutonomyPage.h"
#include "ui/workflows/ai/integration/AiIntegrationPage.h"
#include "core/ProjectModel.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QListWidget>
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
    workflow.setStepCount(21);

    auto* list = workflow.findChild<QListWidget*>();
    bool ok = require(list != nullptr, "workflow list must exist");
    if (!ok) return 1;

    QList<WorkflowPageId> selected;
    QObject::connect(&workflow, &WorkflowWidget::pageSelected,
                     [&selected](WorkflowPageId page) { selected << page; });

    const QList<int> rows{1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17, 19, 20, 22, 23, 24, 25};
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
        WorkflowPageId::RulesRouting,
        WorkflowPageId::Memory,
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
    return ok ? 0 : 1;
}
