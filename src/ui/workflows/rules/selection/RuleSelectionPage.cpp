#include "RuleSelectionPage.h"

#include "core/ProjectModel.h"
#include "core/RuleCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <algorithm>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

RuleSelectionPage::RuleSelectionPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Which rules should apply?</h2>Select the rule categories ARAMF should enforce for this project."), this));
    auto* form = new QFormLayout;
    enforcement_ = new QComboBox(this);
    enforcement_->addItem(tr("Advisory"), QStringLiteral("advisory"));
    enforcement_->addItem(tr("Standard"), QStringLiteral("standard"));
    enforcement_->addItem(tr("Strict"), QStringLiteral("strict"));
    form->addRow(tr("Rule Enforcement Level"), enforcement_);
    layout->addLayout(form);
    selectAll_ = new QPushButton(tr("Select All"), this);
    selectAll_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    layout->addWidget(selectAll_);

    const auto all = RuleCatalog::categories();
    const QList<QPair<QString, int>> sections{{tr("Code Quality"), 10}, {tr("Architecture"), 9}, {tr("Testing"), 9}, {tr("Build / Tooling"), 8}, {tr("Security / Safety"), 8}, {tr("AI Behaviour"), 10}, {tr("Documentation"), 7}};
    int offset = 0;
    for (const auto& section : sections) {
        const auto options = all.mid(offset, section.second);
        auto* group = new CapabilityCheckGroup(section.first, options, 3, this);
        groups_.append(group); layout->addWidget(group);
        connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
        offset += section.second;
    }
    layout->addStretch();
    connect(enforcement_, &QComboBox::currentIndexChanged, this, [this] { persist(); });
    connect(selectAll_, &QPushButton::clicked, this, &RuleSelectionPage::toggleAll);
    connect(model_, &ProjectModel::modelChanged, this, &RuleSelectionPage::refresh);
    refresh();
}

void RuleSelectionPage::persist()
{
    auto value = model_->ruleConfiguration();
    value.activeCategories.clear();
    for (auto* group : groups_) value.activeCategories << group->selectedIds();
    value.enforcementLevel = enforcement_->currentData().toString();
    model_->setRuleConfiguration(value);
    updateSelectAllText();
}

void RuleSelectionPage::toggleAll()
{
    const bool select = !std::all_of(groups_.cbegin(), groups_.cend(), [](auto* group) { return group->allSelectableSelected(); });
    for (auto* group : groups_) group->setAllSelected(select);
    persist();
}

void RuleSelectionPage::updateSelectAllText()
{
    const bool selected = !groups_.isEmpty() && std::all_of(groups_.cbegin(), groups_.cend(), [](auto* group) { return group->allSelectableSelected(); });
    selectAll_->setText(selected ? tr("Clear All") : tr("Select All"));
}

void RuleSelectionPage::refresh()
{
    const auto value = model_->ruleConfiguration();
    enforcement_->setCurrentIndex(enforcement_->findData(value.enforcementLevel));
    for (auto* group : groups_) group->setSelectedIds(value.activeCategories);
    updateSelectAllText();
}
