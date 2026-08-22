#include "RulesRoutingPage.h"

#include "core/ProjectModel.h"
#include "ui/shared/PageSupport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

RulesRoutingPage::RulesRoutingPage(QWidget* parent)
    : QWidget(parent)
{
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    auto* table = new QTableWidget(0, 4);
    table->setHorizontalHeaderLabels(
        {tr("Rule set"), tr("Scope"), tr("Priority"), tr("Enabled")});
    table->horizontalHeader()->setStretchLastSection(true);

    const QStringList rows{
        tr("Universal safety"), tr("C++ / Qt"), tr("Project architecture"),
        tr("Testing and verification"), tr("User custom rules")
    };
    for (const auto& name : rows) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(name));
        table->setItem(row, 1, new QTableWidgetItem(tr("Project")));
        table->setItem(row, 2, new QTableWidgetItem(tr("Normal")));
        auto* enabled = new QCheckBox(table);
        enabled->setChecked(true);
        table->setCellWidget(row, 3, enabled);
    }

    auto* catalog = AramfUi::group(tr("Rule catalog"), new QVBoxLayout, content);
    catalog->layout()->addWidget(table);
    layout->addWidget(catalog);

    auto* routing = new QFormLayout;
    auto* task = new QComboBox;
    task->addItems({tr("Use task category"), tr("Use explicit route"),
                    tr("Load all project rules")});
    auto* scope = new QComboBox;
    scope->addItems({tr("Nearest project scope"), tr("Project root"),
                     tr("Selected folder")});
    routing->addRow(tr("Task routing"), task);
    routing->addRow(tr("Scope routing"), scope);
    layout->addWidget(AramfUi::group(tr("Context routing"), routing, content));

    auto* preview = new QPlainTextEdit;
    preview->setReadOnly(true);
    preview->setPlainText(tr(
        "Selected context preview\n\n1. Universal safety\n2. Project architecture\n"
        "3. Relevant task rules\n\nEstimated context: 18.4k tokens\n"
        "Excluded unrelated rules: 42.1k tokens"));
    auto* previewGroup = AramfUi::group(tr("Selection preview"), new QVBoxLayout, content);
    previewGroup->layout()->addWidget(preview);
    layout->addWidget(previewGroup);
    layout->addStretch();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(AramfUi::pageShell(
        tr("Rules & Routing"),
        tr("Control which instructions reach the AI, at what scope, and with what priority."),
        content));
}

void RulesRoutingPage::setModel(ProjectModel* model)
{
    if (model_ == model) return;
    model_ = model;
    AramfUi::bindCheckboxes(this, model_, QStringLiteral("rules-routing"));
}
