#include "ImprovementBacklogPage.h"

#include "core/ImprovementBacklog.h"
#include "core/ProjectModel.h"

#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageBox>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
QString displayText(const QJsonObject& item)
{
    return QStringLiteral("[%1] %2 | %3 | %4 | %5 occurrences")
        .arg(item.value(QStringLiteral("stage")).toString(), item.value(QStringLiteral("todoId")).toString(),
             item.value(QStringLiteral("status")).toString(), item.value(QStringLiteral("title")).toString())
        .arg(item.value(QStringLiteral("occurrences")).toArray().size());
}
}

ImprovementBacklogPage::ImprovementBacklogPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model), items_(new QListWidget(this)), filter_(new QComboBox(this)), summary_(new QLabel(this)),
      details_(new QPlainTextEdit(this)), title_(new QLineEdit(this)), observation_(new QPlainTextEdit(this)),
      expected_(new QLineEdit(this)), area_(new QLineEdit(this)), evidence_(new QLineEdit(this)), status_(new QLabel(this)),
      promote_(new QPushButton(tr("Promote to TODO"), this)), projectSpecific_(new QPushButton(tr("Mark Project Specific"), this)),
      duplicate_(new QPushButton(tr("Mark Duplicate"), this)), moreEvidence_(new QPushButton(tr("Needs More Evidence"), this)),
      resolved_(new QPushButton(tr("Mark Already Resolved"), this)), reject_(new QPushButton(tr("Reject"), this)),
      delete_(new QPushButton(tr("Delete"), this)),
      priority_(new QComboBox(this)), duplicateOf_(new QLineEdit(this)), lifecycle_(new QComboBox(this))
{
    auto* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("<h2>ARAMF Improvement Backlog</h2>Record and triage reusable deficiencies in ARAMF itself. Reports are observations, not automatic TODOs or Framework Knowledge."), this));
    filter_->setObjectName(QStringLiteral("improvementBacklogFilter"));
    filter_->addItems({tr("All"), tr("Observations"), tr("TODO"), tr("In Progress"), tr("Completed")});
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Filter:"), this)); filterRow->addWidget(filter_); filterRow->addStretch();
    root->addLayout(filterRow);
    summary_->setObjectName(QStringLiteral("improvementBacklogSummary")); root->addWidget(summary_);
    auto* content = new QGridLayout;
    items_->setObjectName(QStringLiteral("improvementBacklogItems")); content->addWidget(items_, 0, 0, 3, 1);
    details_->setObjectName(QStringLiteral("improvementBacklogDetails")); details_->setReadOnly(true); content->addWidget(details_, 0, 1, 1, 1);
    auto* triage = new QHBoxLayout;
    delete_->setObjectName(QStringLiteral("deleteImprovementBacklogItem")); delete_->setEnabled(false);
    for (auto* button : {promote_, projectSpecific_, duplicate_, moreEvidence_, resolved_, reject_, delete_}) triage->addWidget(button);
    content->addLayout(triage, 1, 1);
    priority_->setObjectName(QStringLiteral("improvementBacklogPriority")); priority_->addItems({QStringLiteral("unassigned"), QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high"), QStringLiteral("critical")});
    lifecycle_->setObjectName(QStringLiteral("improvementBacklogStatus")); lifecycle_->addItems({QStringLiteral("OPEN"), QStringLiteral("IN_PROGRESS"), QStringLiteral("IMPLEMENTED"), QStringLiteral("VALIDATED"), QStringLiteral("COMPLETED")});
    duplicateOf_->setObjectName(QStringLiteral("improvementBacklogDuplicateOf")); duplicateOf_->setPlaceholderText(tr("gap-id target"));
    auto* statusRow = new QHBoxLayout; statusRow->addWidget(new QLabel(tr("Priority:"), this)); statusRow->addWidget(priority_); statusRow->addWidget(new QLabel(tr("Duplicate of:"), this)); statusRow->addWidget(duplicateOf_); statusRow->addWidget(new QLabel(tr("TODO status:"), this)); statusRow->addWidget(lifecycle_); content->addLayout(statusRow, 2, 1);
    root->addLayout(content, 1);
    auto* form = new QFormLayout;
    title_->setObjectName(QStringLiteral("improvementBacklogTitle")); observation_->setObjectName(QStringLiteral("improvementBacklogObservation")); expected_->setObjectName(QStringLiteral("improvementBacklogExpected")); area_->setObjectName(QStringLiteral("improvementBacklogArea")); evidence_->setObjectName(QStringLiteral("improvementBacklogEvidence"));
    form->addRow(tr("Report title"), title_); form->addRow(tr("Observation"), observation_); form->addRow(tr("Expected framework behavior"), expected_); form->addRow(tr("Framework area"), area_); form->addRow(tr("Evidence"), evidence_);
    auto* report = new QPushButton(tr("Report ARAMF Gap"), this); report->setObjectName(QStringLiteral("reportAramfGap")); form->addRow(report); root->addLayout(form);
    status_->setObjectName(QStringLiteral("improvementBacklogStatusMessage")); root->addWidget(status_);
    connect(filter_, &QComboBox::currentTextChanged, this, &ImprovementBacklogPage::refresh);
    connect(items_, &QListWidget::currentItemChanged, this, [this] { refreshDetails(); });
    connect(items_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) { if (item->checkState() == Qt::Checked) selectedIds_.insert(item->data(Qt::UserRole).toString()); else selectedIds_.remove(item->data(Qt::UserRole).toString()); });
    const auto triageAction = [this](const QString& action) { const auto* item = items_->currentItem(); if (!item) return; QString error; if (!ImprovementBacklogService().triage(item->data(Qt::UserRole).toString(), action, duplicateOf_->text().trimmed(), priority_->currentText(), &error)) status_->setText(error); else { status_->setText(tr("Triage saved.")); refresh(); } };
    connect(promote_, &QPushButton::clicked, this, [triageAction] { triageAction(QStringLiteral("promote")); });
    connect(projectSpecific_, &QPushButton::clicked, this, [triageAction] { triageAction(QStringLiteral("project-specific")); });
    connect(duplicate_, &QPushButton::clicked, this, [triageAction] { triageAction(QStringLiteral("duplicate")); });
    connect(moreEvidence_, &QPushButton::clicked, this, [triageAction] { triageAction(QStringLiteral("needs-evidence")); });
    connect(resolved_, &QPushButton::clicked, this, [triageAction] { triageAction(QStringLiteral("already-resolved")); });
    connect(reject_, &QPushButton::clicked, this, [triageAction] { triageAction(QStringLiteral("reject")); });
    connect(delete_, &QPushButton::clicked, this, [this] {
        const auto* selected = items_->currentItem();
        if (!selected) return;
        const auto id = selected->data(Qt::UserRole).toString();
        QJsonObject item;
        QString readError;
        for (const auto& value : ImprovementBacklogService().items(&readError)) {
            if (value.value(QStringLiteral("id")).toString() == id) { item = value; break; }
        }
        if (item.isEmpty()) { status_->setText(readError.isEmpty() ? tr("Backlog item was not found.") : readError); return; }
        QMessageBox confirmation(this);
        confirmation.setIcon(QMessageBox::Warning);
        confirmation.setWindowTitle(tr("Delete backlog item?"));
        confirmation.setText(tr("%1\n%2\n%3\n\nThis permanently removes the item and its evidence from the global ARAMF Improvement Backlog.")
                                 .arg(item.value(QStringLiteral("todoId")).toString(), item.value(QStringLiteral("title")).toString(), id));
        auto* cancel = confirmation.addButton(tr("Cancel"), QMessageBox::RejectRole);
        auto* remove = confirmation.addButton(tr("Delete"), QMessageBox::AcceptRole);
        confirmation.setDefaultButton(cancel);
        confirmation.exec();
        if (confirmation.clickedButton() != remove) return;
        QString error;
        if (!ImprovementBacklogService().removeItem(id, &error)) { status_->setText(error); return; }
        selectedIds_.remove(id);
        status_->setText(tr("Backlog item deleted."));
        refresh();
    });
    connect(lifecycle_, &QComboBox::currentTextChanged, this, [this](const QString& value) { const auto* item = items_->currentItem(); if (!item || item->data(Qt::UserRole + 1).toString() != QStringLiteral("todo")) return; QString error; if (!ImprovementBacklogService().setStatus(item->data(Qt::UserRole).toString(), value, &error)) status_->setText(error); else refresh(); });
    connect(report, &QPushButton::clicked, this, [this] { QJsonObject result; QString error; const QString projectRoot = model_ && !model_->projectPath().isEmpty() ? model_->projectPath() : QDir::currentPath(); ImprovementBacklogProjectIdentity projectIdentity{model_ ? model_->projectId() : QString(), model_ ? model_->projectName() : QString(), projectRoot}; if (ImprovementBacklogService().reportWithIdentity(projectIdentity, title_->text(), observation_->toPlainText(), expected_->text(), area_->text(), evidence_->text().isEmpty() ? QStringList{} : QStringList{evidence_->text()}, {}, {}, &result, &error)) { status_->setText(tr("Reported %1 (%2).").arg(result.value(QStringLiteral("id")).toString(), result.value(QStringLiteral("outcome")).toString())); title_->clear(); observation_->clear(); expected_->clear(); area_->clear(); evidence_->clear(); refresh(); } else status_->setText(error); });
    refresh();
}

void ImprovementBacklogPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event); refresh();
}

QSet<QString> ImprovementBacklogPage::selectedIds() const { return selectedIds_; }

void ImprovementBacklogPage::refresh()
{
    const auto selected = selectedIds();
    QString error; const auto all = ImprovementBacklogService().items(&error);
    items_->blockSignals(true); items_->clear();
    int observations = 0, todos = 0, inProgress = 0, completed = 0;
    for (const auto& item : all) {
        const auto stage = item.value(QStringLiteral("stage")).toString(); const auto itemStatus = item.value(QStringLiteral("status")).toString();
        if (stage == QStringLiteral("observation")) ++observations; if (stage == QStringLiteral("todo")) ++todos; if (itemStatus == QStringLiteral("IN_PROGRESS")) ++inProgress; if (itemStatus == QStringLiteral("COMPLETED") || stage == QStringLiteral("completed")) ++completed;
        const auto filter = filter_->currentText(); if ((filter == tr("Observations") && stage != QStringLiteral("observation")) || (filter == tr("TODO") && stage != QStringLiteral("todo")) || (filter == tr("In Progress") && itemStatus != QStringLiteral("IN_PROGRESS")) || (filter == tr("Completed") && itemStatus != QStringLiteral("COMPLETED"))) continue;
        auto* listItem = new QListWidgetItem(displayText(item), items_); listItem->setData(Qt::UserRole, item.value(QStringLiteral("id")).toString()); listItem->setData(Qt::UserRole + 1, stage); listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable); listItem->setCheckState(selected.contains(item.value(QStringLiteral("id")).toString()) ? Qt::Checked : Qt::Unchecked);
    }
    items_->blockSignals(false); summary_->setText(tr("Observations: %1 | Open TODOs: %2 | In progress: %3 | Completed: %4").arg(observations).arg(todos).arg(inProgress).arg(completed)); status_->setText(error); refreshDetails();
}

void ImprovementBacklogPage::refreshDetails()
{
    const auto* item = items_->currentItem(); if (!item) { details_->clear(); delete_->setEnabled(false); return; }
    delete_->setEnabled(true);
    QString error; for (const auto& value : ImprovementBacklogService().items(&error)) if (value.value(QStringLiteral("id")).toString() == item->data(Qt::UserRole).toString()) { details_->setPlainText(QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Indented))); const QSignalBlocker priorityBlocker(priority_); const QSignalBlocker lifecycleBlocker(lifecycle_); priority_->setCurrentText(value.value(QStringLiteral("priority")).toString()); lifecycle_->setCurrentText(value.value(QStringLiteral("status")).toString()); promote_->setEnabled(value.value(QStringLiteral("stage")).toString() == QStringLiteral("observation")); return; }
}
