#include "ResourcePolicyPage.h"

#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

ResourcePolicyPage::ResourcePolicyPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>How should AI use the resources?</h2>Define how AI agents should load, prioritize and reference project resources."), this));
    loadingStrategy_ = new QComboBox(this);
    for (const auto& option : EnvironmentCatalog::resourceLoadingStrategies()) loadingStrategy_->addItem(option.first, option.second);
    auto* form = new QFormLayout;
    form->addRow(tr("Default Resource Loading Strategy"), loadingStrategy_);
    layout->addLayout(form);
    options_ = new CapabilityCheckGroup(tr("Resource Use Policy"), EnvironmentCatalog::resourcePolicyOptions(), 2, this);
    layout->addWidget(options_); layout->addStretch();
    connect(loadingStrategy_, &QComboBox::currentIndexChanged, this, [this] { persist(); });
    connect(options_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(model_, &ProjectModel::modelChanged, this, &ResourcePolicyPage::refresh);
    refresh();
}

void ResourcePolicyPage::persist()
{
    ResourcePolicy value;
    value.options = options_->selectedIds(); value.loadingStrategy = loadingStrategy_->currentData().toString();
    model_->setResourcePolicy(value);
}

void ResourcePolicyPage::refresh()
{
    const auto value = model_->resourcePolicy();
    const QSignalBlocker blocker(loadingStrategy_);
    loadingStrategy_->setCurrentIndex(loadingStrategy_->findData(value.loadingStrategy));
    options_->setSelectedIds(value.options);
}
