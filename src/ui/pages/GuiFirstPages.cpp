#include "GuiFirstPages.h"
#include "core/ProjectModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
void bindCheckboxes(QWidget* page, ProjectModel* model, const QString& key) {
    if (!model) return;
    const auto checks = page->findChildren<QCheckBox*>();
    const auto refresh = [page, model, key] {
        const auto values = model->optionValues(key);
        const auto checks = page->findChildren<QCheckBox*>();
        for (auto* box : checks) { const QSignalBlocker blocker(box); box->setChecked(values.contains(box->text())); }
    };
    for (auto* box : checks) QObject::connect(box, &QCheckBox::toggled, page, [page, model, key] {
        QStringList values;
        for (auto* item : page->findChildren<QCheckBox*>()) if (item->isChecked()) values << item->text();
        model->setOptionValues(key, values);
    });
    QObject::connect(model, &ProjectModel::optionChanged, page, [key, refresh](const QString& changed) { if (changed == key) refresh(); });
    refresh();
}

QWidget* pageShell(const QString& title, const QString& intro, QWidget* content) {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(24, 20, 24, 20);
    outer->setSpacing(14);
    outer->addWidget(new QLabel(QStringLiteral("<h1>%1</h1><p>%2</p>").arg(title, intro), page));
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    return page;
}

QGroupBox* group(const QString& title, QLayout* layout, QWidget* parent) {
    auto* box = new QGroupBox(title, parent);
    box->setLayout(layout);
    return box;
}

QCheckBox* check(const QString& text, const QString& hint, QWidget* parent) {
    auto* result = new QCheckBox(text, parent);
    result->setToolTip(hint);
    return result;
}
}

AiStrategyPage::AiStrategyPage(QWidget* parent) : QWidget(parent) {
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    auto* behavior = new QFormLayout;
    auto* mode = new QComboBox; mode->addItems({tr("Balanced"), tr("Fast iteration"), tr("Maximum verification"), tr("Minimal context")});
    auto* autonomy = new QComboBox; autonomy->addItems({tr("Suggest only"), tr("Execute with approval"), tr("Autonomous within safe scope")});
    auto* contextBudget = new QSpinBox; contextBudget->setRange(1, 100); contextBudget->setValue(35); contextBudget->setSuffix(tr(" % of available context"));
    behavior->addRow(tr("Operating mode"), mode); behavior->addRow(tr("Autonomy level"), autonomy); behavior->addRow(tr("Context budget"), contextBudget);
    layout->addWidget(group(tr("AI operating profile"), behavior, content));
    auto* quality = new QVBoxLayout;
    quality->addWidget(check(tr("Prefer deterministic local automation for repeatable work"), tr("Move builds, scans and certification loops out of the reasoning loop."), content));
    quality->addWidget(check(tr("Require evidence before claiming completion"), tr("Keep verification separate from implementation status."), content));
    quality->addWidget(check(tr("Ask before changing user-owned files"), tr("Protect custom content and foreign configuration."), content));
    quality->addWidget(check(tr("Summarize reused context and token impact"), tr("Make context reuse measurable."), content));
    layout->addWidget(group(tr("Reasoning and execution policy"), quality, content));
    auto* roles = new QFormLayout;
    auto* planner = new QComboBox; planner->addItems({tr("AI agent"), tr("Human"), tr("Shared")});
    auto* executor = new QComboBox; executor->addItems({tr("Local automation"), tr("AI agent"), tr("Shared")});
    roles->addRow(tr("Planning and interpretation"), planner); roles->addRow(tr("Deterministic execution"), executor);
    layout->addWidget(group(tr("Work allocation"), roles, content)); layout->addStretch();
    auto* shell = pageShell(tr("AI Strategy"), tr("Shape how an AI works on this project: context, autonomy, evidence and work allocation."), content);
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0,0,0,0); outer->addWidget(shell);
}

RulesRoutingPage::RulesRoutingPage(QWidget* parent) : QWidget(parent) {
    auto* content = new QWidget; auto* layout = new QVBoxLayout(content);
    auto* table = new QTableWidget(0, 4); table->setHorizontalHeaderLabels({tr("Rule set"), tr("Scope"), tr("Priority"), tr("Enabled")}); table->horizontalHeader()->setStretchLastSection(true);
    const QStringList rows{tr("Universal safety"), tr("C++ / Qt"), tr("Project architecture"), tr("Testing and verification"), tr("User custom rules")};
    for (const auto& name : rows) { const int row = table->rowCount(); table->insertRow(row); table->setItem(row,0,new QTableWidgetItem(name)); table->setItem(row,1,new QTableWidgetItem(tr("Project"))); table->setItem(row,2,new QTableWidgetItem(tr("Normal"))); table->setCellWidget(row,3,new QCheckBox(table)); static_cast<QCheckBox*>(table->cellWidget(row,3))->setChecked(true); }
    layout->addWidget(group(tr("Rule catalog"), new QVBoxLayout, content)); layout->itemAt(0)->widget()->layout()->addWidget(table);
    auto* routing = new QFormLayout; auto* task = new QComboBox; task->addItems({tr("Use task category"), tr("Use explicit route"), tr("Load all project rules")}); auto* scope = new QComboBox; scope->addItems({tr("Nearest project scope"), tr("Project root"), tr("Selected folder")}); routing->addRow(tr("Task routing"), task); routing->addRow(tr("Scope routing"), scope);
    layout->addWidget(group(tr("Context routing"), routing, content));
    auto* preview = new QPlainTextEdit; preview->setReadOnly(true); preview->setPlainText(tr("Selected context preview\n\n1. Universal safety\n2. Project architecture\n3. Relevant task rules\n\nEstimated context: 18.4k tokens\nExcluded unrelated rules: 42.1k tokens"));
    layout->addWidget(group(tr("Selection preview"), new QVBoxLayout, content)); layout->itemAt(2)->widget()->layout()->addWidget(preview); layout->addStretch();
    auto* shell = pageShell(tr("Rules & Routing"), tr("Control which instructions reach the AI, at what scope, and with what priority."), content); auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0,0,0,0); outer->addWidget(shell);
}

MemoryPage::MemoryPage(QWidget* parent) : QWidget(parent) {
    auto* content = new QWidget; auto* layout = new QVBoxLayout(content);
    auto* policy = new QFormLayout; auto* capture = new QComboBox; capture->addItems({tr("Selective durable decisions"), tr("Capture all completed work"), tr("Manual only")}); auto* retention = new QComboBox; retention->addItems({tr("Append-only project memory"), tr("Append-only + checkpoints"), tr("No automatic memory")}); policy->addRow(tr("Decision capture"), capture); policy->addRow(tr("History policy"), retention); layout->addWidget(group(tr("Memory policy"), policy, content));
    auto* checks = new QVBoxLayout; checks->addWidget(check(tr("Validate memory on project activation"), tr("Run cold-start validation."), content)); checks->addWidget(check(tr("Validate consistency before generation"), tr("Detect stale derived state and broken references."), content)); checks->addWidget(check(tr("Record automation/offloading decisions"), tr("Keep AI and local automation responsibilities auditable."), content)); checks->addWidget(check(tr("Keep production and durable sequence numbers separate"), tr("Control-plane events must not look like production changes."), content)); layout->addWidget(group(tr("Integrity and provenance"), checks, content));
    auto* summary = new QPlainTextEdit; summary->setReadOnly(true); summary->setPlainText(tr("Current state: Not connected\nLatest event: Not connected\nLatest checkpoint: Not connected\nIntegrity: Preview mode\n\nThe GUI deliberately has no persistence connection yet.")); layout->addWidget(group(tr("Memory status preview"), new QVBoxLayout, content)); layout->itemAt(2)->widget()->layout()->addWidget(summary); layout->addStretch();
    auto* shell = pageShell(tr("Project Memory"), tr("Design the durable memory experience before wiring it to the append-only core."), content); auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0,0,0,0); outer->addWidget(shell);
}

VerificationPage::VerificationPage(QWidget* parent) : QWidget(parent) {
    auto* content = new QWidget; auto* layout = new QVBoxLayout(content);
    auto* checks = new QVBoxLayout; checks->addWidget(check(tr("Build"), tr("Compile the generated project."), content)); checks->addWidget(check(tr("Tests"), tr("Run the project test suite."), content)); checks->addWidget(check(tr("Launch"), tr("Start and observe the generated application."), content)); checks->addWidget(check(tr("Memory consistency"), tr("Validate append-only and derived state."), content)); checks->addWidget(check(tr("AGENTS deployment safety"), tr("Verify managed/foreign root file behavior."), content)); layout->addWidget(group(tr("Verification plan"), checks, content));
    auto* evidence = new QFormLayout; auto* level = new QComboBox; level->addItems({tr("Development"), tr("Release candidate"), tr("Certification baseline")}); auto* artifact = new QComboBox; artifact->addItems({tr("Keep local evidence"), tr("Record project evidence"), tr("Record reusable certification")}); evidence->addRow(tr("Validation level"), level); evidence->addRow(tr("Evidence policy"), artifact); layout->addWidget(group(tr("Evidence"), evidence, content)); layout->addStretch();
    auto* shell = pageShell(tr("Verification & Evidence"), tr("Separate implementation, verification and reusable certification claims."), content); auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0,0,0,0); outer->addWidget(shell);
}

void AiStrategyPage::setModel(ProjectModel* model) { if (model_ == model) return; model_ = model; bindCheckboxes(this, model_, QStringLiteral("ai-strategy")); }
void RulesRoutingPage::setModel(ProjectModel* model) { if (model_ == model) return; model_ = model; bindCheckboxes(this, model_, QStringLiteral("rules-routing")); }
void MemoryPage::setModel(ProjectModel* model) { if (model_ == model) return; model_ = model; bindCheckboxes(this, model_, QStringLiteral("memory-policy")); }
void VerificationPage::setModel(ProjectModel* model) { if (model_ == model) return; model_ = model; bindCheckboxes(this, model_, QStringLiteral("verification-plan")); }
