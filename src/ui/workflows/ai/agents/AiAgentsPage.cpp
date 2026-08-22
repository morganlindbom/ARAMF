#include "AiAgentsPage.h"

#include "core/AiCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

AiAgentsPage::AiAgentsPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model), primary_(new QComboBox(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Which AI agents are used?</h2>Select the primary execution agent and any additional AI agents used by this project."), this));

    auto* primarySection = new QGroupBox(tr("Primary Execution Agent"), this);
    auto* primaryLayout = new QFormLayout(primarySection);
    primary_->addItem(tr("None / Not Selected"), QStringLiteral("none"));
    for (const auto& option : AiCatalog::agents()) {
        primary_->addItem(option.displayName, option.id);
    }
    primaryLayout->addRow(tr("Primary execution agent"), primary_);
    custom_ = new QLineEdit(primarySection);
    custom_->setPlaceholderText(tr("Custom agent name"));
    custom_->setVisible(false);
    primaryLayout->addRow(tr("Custom agent"), custom_);
    layout->addWidget(primarySection);

    auto* additionalSection = new QGroupBox(tr("Additional AI Agents"), this);
    auto* additionalLayout = new QVBoxLayout(additionalSection);
    QHash<QString, QList<EnvironmentOption>> grouped;
    for (const auto& option : AiCatalog::agents()) {
        grouped[option.category].append({option.displayName, option.id});
    }
    const QStringList categories{
        QStringLiteral("OpenAI"), QStringLiteral("Anthropic"), QStringLiteral("Google"),
        QStringLiteral("GitHub"), QStringLiteral("Microsoft"), QStringLiteral("JetBrains"),
        QStringLiteral("Search / Research AI"), QStringLiteral("Coding / IDE"),
        QStringLiteral("IDE / Editor Agents"),
        QStringLiteral("Terminal / Coding Agents"), QStringLiteral("Autonomous / General Agents"),
        QStringLiteral("Cloud / Vendor Assistants"), QStringLiteral("Local / Self-hosted"),
        QStringLiteral("Other Coding Agents"), QStringLiteral("Other")};
    for (const auto& category : categories) {
        if (!grouped.contains(category)) continue;
        auto* group = new CapabilityCheckGroup(category, grouped.value(category), 3, additionalSection);
        additionalGroups_.append(group);
        additionalLayout->addWidget(group);
        connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    }
    layout->addWidget(additionalSection);
    layout->addStretch();

    connect(primary_, &QComboBox::currentIndexChanged, this, [this] {
        persist();
        refreshAdditionalAgentAvailability();
    });
    connect(custom_, &QLineEdit::textChanged, this, [this] { persist(); });
    connect(model_, &ProjectModel::aiConfigurationChanged, this, &AiAgentsPage::refresh);
    refresh();
}

void AiAgentsPage::persist()
{
    auto value = model_->aiConfiguration();
    value.primaryAgent = primary_->currentData().toString();
    value.additionalAgents.clear();
    for (auto* group : additionalGroups_) value.additionalAgents << group->selectedIds();
    value.customAgentName = custom_->text().trimmed();
    model_->setAiConfiguration(value);
}

void AiAgentsPage::refreshAdditionalAgentAvailability()
{
    // Rebuild availability from the current primary selection every time.
    // This also re-enables the previous primary when the user switches away
    // from it or selects None / Not Selected.
    for (const auto& agent : AiCatalog::agents()) {
        for (auto* group : additionalGroups_) {
            group->setOptionEnabled(agent.id, true);
        }
    }

    const QString primary = primary_->currentData().toString();
    if (!primary.isEmpty() && primary != QStringLiteral("none")) {
        for (auto* group : additionalGroups_) {
            group->setOptionEnabled(primary, false);
        }
    }

    custom_->setVisible(primary == QStringLiteral("custom-agent"));
}

void AiAgentsPage::refresh()
{
    const auto value = model_->aiConfiguration();
    const QSignalBlocker primaryBlocker(primary_);
    const QSignalBlocker customBlocker(custom_);
    const int index = primary_->findData(value.primaryAgent);
    primary_->setCurrentIndex(index >= 0 ? index : 0);
    custom_->setText(value.customAgentName);
    custom_->setVisible(value.primaryAgent == QStringLiteral("custom-agent"));
    for (auto* group : additionalGroups_) group->setSelectedIds(value.additionalAgents);
    refreshAdditionalAgentAvailability();
}
