#include "MemoryPage.h"

#include "core/ProjectModel.h"
#include "ui/shared/PageSupport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

MemoryPage::MemoryPage(QWidget* parent)
    : QWidget(parent)
{
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);

    auto* policy = new QFormLayout;
    auto* capture = new QComboBox;
    capture->addItems({tr("Selective durable decisions"),
                       tr("Capture all completed work"), tr("Manual only")});
    auto* retention = new QComboBox;
    retention->addItems({tr("Append-only project memory"),
                         tr("Append-only + checkpoints"), tr("No automatic memory")});
    policy->addRow(tr("Decision capture"), capture);
    policy->addRow(tr("History policy"), retention);
    layout->addWidget(AramfUi::group(tr("Memory policy"), policy, content));

    auto* checks = new QVBoxLayout;
    checks->addWidget(AramfUi::check(
        tr("Validate memory on project activation"), tr("Run cold-start validation."), content));
    checks->addWidget(AramfUi::check(
        tr("Validate consistency before generation"),
        tr("Detect stale derived state and broken references."), content));
    checks->addWidget(AramfUi::check(
        tr("Record automation/offloading decisions"),
        tr("Keep AI and local automation responsibilities auditable."), content));
    checks->addWidget(AramfUi::check(
        tr("Keep production and durable sequence numbers separate"),
        tr("Control-plane events must not look like production changes."), content));
    layout->addWidget(AramfUi::group(tr("Integrity and provenance"), checks, content));

    auto* summary = new QPlainTextEdit;
    summary->setReadOnly(true);
    summary->setPlainText(tr(
        "Current state: Not connected\nLatest event: Not connected\n"
        "Latest checkpoint: Not connected\nIntegrity: Preview mode\n\n"
        "The GUI deliberately has no persistence connection yet."));
    auto* summaryGroup = AramfUi::group(tr("Memory status preview"), new QVBoxLayout, content);
    summaryGroup->layout()->addWidget(summary);
    layout->addWidget(summaryGroup);
    layout->addStretch();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(AramfUi::pageShell(
        tr("Project Memory"),
        tr("Design the durable memory experience before wiring it to the append-only core."),
        content));
}

void MemoryPage::setModel(ProjectModel* model)
{
    if (model_ == model) return;
    model_ = model;
    AramfUi::bindCheckboxes(this, model_, QStringLiteral("memory-policy"));
}
