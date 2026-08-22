#include "AiAutonomyPage.h"

#include "core/AiCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <algorithm>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

AiAutonomyPage::AiAutonomyPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>How autonomous may AI be?</h2>Define which actions AI agents may perform without requiring manual execution."), this));
    selectAll_ = new QPushButton(tr("Select All"), this);
    selectAll_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    layout->addWidget(selectAll_);
    QHash<QString, QList<EnvironmentOption>> grouped;
    for (const auto& option : AiCatalog::permissions()) grouped[option.category].append({option.displayName, option.id});
    const QStringList categories{QStringLiteral("File Operations"), QStringLiteral("Execution"), QStringLiteral("Project Configuration"), QStringLiteral("Dependencies / Environment"), QStringLiteral("Version Control"), QStringLiteral("High-Risk Actions")};
    for (const auto& category : categories) {
        auto* group = new CapabilityCheckGroup(category, grouped.value(category), 3, this);
        if (category == QStringLiteral("High-Risk Actions")) highRiskGroups_.append(group);
        else groups_.append(group);
        layout->addWidget(group);
        connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    }
    layout->addStretch();
    connect(model_, &ProjectModel::aiConfigurationChanged, this, &AiAutonomyPage::refresh);
    connect(selectAll_, &QPushButton::clicked, this, &AiAutonomyPage::toggleAll);
    refresh();
}

void AiAutonomyPage::persist()
{
    auto value = model_->aiConfiguration();
    value.permissions.clear();
    for (auto* group : groups_) value.permissions << group->selectedIds();
    model_->setAiConfiguration(value);
    updateSelectAllText();
}

void AiAutonomyPage::toggleAll()
{
    const bool select = !std::all_of(groups_.cbegin(), groups_.cend(), [](CapabilityCheckGroup* group) {
        return group->allSelectableSelected();
    });
    if (select) {
        for (auto* group : groups_) group->setAllSelected(true);
    } else {
        for (auto* group : groups_) group->setAllSelected(false, true);
        for (auto* group : highRiskGroups_) group->setAllSelected(false, true);
    }
    updateSelectAllText();
}

void AiAutonomyPage::updateSelectAllText()
{
    const bool allSelected = !groups_.isEmpty()
        && std::all_of(groups_.cbegin(), groups_.cend(), [](CapabilityCheckGroup* group) {
               return group->allSelectableSelected();
           });
    selectAll_->setText(allSelected ? tr("Clear All") : tr("Select All"));
}

void AiAutonomyPage::refresh()
{
    const auto values = model_->aiConfiguration().permissions;
    for (auto* group : groups_) group->setSelectedIds(values);
    for (auto* group : highRiskGroups_) group->setSelectedIds(values);
    updateSelectAllText();
}
