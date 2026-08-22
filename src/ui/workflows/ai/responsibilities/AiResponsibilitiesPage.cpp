#include "AiResponsibilitiesPage.h"

#include "core/AiCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <algorithm>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

AiResponsibilitiesPage::AiResponsibilitiesPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>What may AI work on?</h2>Select the areas of project work that AI agents may participate in."), this));
    selectAll_ = new QPushButton(tr("Select All"), this);
    selectAll_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    layout->addWidget(selectAll_);
    QHash<QString, QList<EnvironmentOption>> grouped;
    for (const auto& option : AiCatalog::responsibilities()) grouped[option.category].append({option.displayName, option.id});
    const QStringList categories{QStringLiteral("Planning / Design"), QStringLiteral("Implementation"), QStringLiteral("Quality"), QStringLiteral("Project Support")};
    for (const auto& category : categories) {
        auto* group = new CapabilityCheckGroup(category, grouped.value(category), 3, this);
        groups_.append(group);
        layout->addWidget(group);
        connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    }
    layout->addStretch();
    connect(model_, &ProjectModel::aiConfigurationChanged, this, &AiResponsibilitiesPage::refresh);
    connect(selectAll_, &QPushButton::clicked, this, &AiResponsibilitiesPage::toggleAll);
    refresh();
}

void AiResponsibilitiesPage::persist()
{
    auto value = model_->aiConfiguration();
    value.responsibilities.clear();
    for (auto* group : groups_) value.responsibilities << group->selectedIds();
    model_->setAiConfiguration(value);
    updateSelectAllText();
}

void AiResponsibilitiesPage::toggleAll()
{
    const bool select = !std::all_of(groups_.cbegin(), groups_.cend(), [](CapabilityCheckGroup* group) {
        return group->allSelectableSelected();
    });
    for (auto* group : groups_) group->setAllSelected(select);
    updateSelectAllText();
}

void AiResponsibilitiesPage::updateSelectAllText()
{
    const bool allSelected = !groups_.isEmpty()
        && std::all_of(groups_.cbegin(), groups_.cend(), [](CapabilityCheckGroup* group) {
               return group->allSelectableSelected();
           });
    selectAll_->setText(allSelected ? tr("Clear All") : tr("Select All"));
}

void AiResponsibilitiesPage::refresh()
{
    const auto values = model_->aiConfiguration().responsibilities;
    for (auto* group : groups_) group->setSelectedIds(values);
    updateSelectAllText();
}
