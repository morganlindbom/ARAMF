#include "AiIntegrationPage.h"

#include "core/AiCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <algorithm>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

AiIntegrationPage::AiIntegrationPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Which ARAMF systems should AI use?</h2>Select which ARAMF subsystems participate in AI-assisted project work."), this));
    selectAll_ = new QPushButton(tr("Select All"), this);
    selectAll_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    layout->addWidget(selectAll_);
    QHash<QString, QList<EnvironmentOption>> grouped;
    for (const auto& option : AiCatalog::integrations()) grouped[option.category].append({option.displayName, option.id});
    const QStringList categories{QStringLiteral("Core Agent Integration"), QStringLiteral("Knowledge and Authority"), QStringLiteral("Validation and History"), QStringLiteral("Automatic Project Maintenance")};
    for (const auto& category : categories) {
        auto* group = new CapabilityCheckGroup(category, grouped.value(category), 3, this);
        groups_.append(group);
        layout->addWidget(group);
        connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    }
    layout->addStretch();
    connect(model_, &ProjectModel::aiConfigurationChanged, this, &AiIntegrationPage::refresh);
    connect(selectAll_, &QPushButton::clicked, this, &AiIntegrationPage::toggleAll);
    refresh();
}

void AiIntegrationPage::persist()
{
    auto value = model_->aiConfiguration();
    value.aramfIntegrations.clear();
    for (auto* group : groups_) value.aramfIntegrations << group->selectedIds();
    model_->setAiConfiguration(value);
    updateSelectAllText();
}

void AiIntegrationPage::toggleAll()
{
    const bool select = !std::all_of(groups_.cbegin(), groups_.cend(), [](CapabilityCheckGroup* group) {
        return group->allSelectableSelected();
    });
    for (auto* group : groups_) group->setAllSelected(select);
    if (!select) {
        for (auto* group : groups_) group->setAllSelected(false, true);
    }
    updateSelectAllText();
}

void AiIntegrationPage::updateSelectAllText()
{
    const bool allSelected = !groups_.isEmpty()
        && std::all_of(groups_.cbegin(), groups_.cend(), [](CapabilityCheckGroup* group) {
               return group->allSelectableSelected();
           });
    selectAll_->setText(allSelected ? tr("Clear All") : tr("Select All"));
}

void AiIntegrationPage::refresh()
{
    const auto values = model_->aiConfiguration().aramfIntegrations;
    for (auto* group : groups_) group->setSelectedIds(values);
    updateSelectAllText();
}
