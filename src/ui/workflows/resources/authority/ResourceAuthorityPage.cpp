#include "ResourceAuthorityPage.h"

#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>

ResourceAuthorityPage::ResourceAuthorityPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model), resources_(new QListWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Which sources are authoritative?</h2>Define which resources ARAMF should trust most and what areas of the project they govern."), this));
    layout->addWidget(new QLabel(tr("Resources"), this));
    resources_->setMinimumHeight(100);
    resources_->setMaximumHeight(200);
    resources_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resources_);
    authority_ = new QComboBox(this);
    for (const auto& option : EnvironmentCatalog::authorityLevels()) authority_->addItem(option.first, option.second);
    scopes_ = new CapabilityCheckGroup(tr("Applies to"), EnvironmentCatalog::resourceScopes(), 3, this);
    auto* form = new QFormLayout;
    form->addRow(tr("Authority"), authority_);
    layout->addLayout(form); layout->addWidget(scopes_); layout->addStretch();
    connect(resources_, &QListWidget::currentRowChanged, this, [this] {
        selectedResourceId_ = resources_->currentItem()
            ? resources_->currentItem()->data(Qt::UserRole).toString()
            : QString();
        refreshSelected();
    });
    connect(authority_, &QComboBox::currentIndexChanged, this, [this] { saveSelected(); });
    connect(scopes_, &CapabilityCheckGroup::selectionChanged, this, [this] { saveSelected(); });
    connect(model_, &ProjectModel::modelChanged, this, &ResourceAuthorityPage::refresh);
    refresh();
}

ProjectResource* ResourceAuthorityPage::selectedResource(QList<ProjectResource>& resources) const
{
    const auto* item = resources_->currentItem();
    const QString selectedId = selectedResourceId_.isEmpty()
        ? (item ? item->data(Qt::UserRole).toString() : QString())
        : selectedResourceId_;
    if (selectedId.isEmpty()) return nullptr;
    for (auto& resource : resources) {
        if (resource.id == selectedId) return &resource;
    }
    return nullptr;
}

void ResourceAuthorityPage::refresh()
{
    const QString selectedId = selectedResourceId_.isEmpty()
        ? (resources_->currentItem() ? resources_->currentItem()->data(Qt::UserRole).toString() : QString())
        : selectedResourceId_;
    const QSignalBlocker blocker(resources_);
    resources_->clear();
    const auto values = model_->resources();
    int rowToSelect = 0;
    for (int index = 0; index < values.size(); ++index) {
        const auto& resource = values.at(index);
        auto* item = new QListWidgetItem(resource.name, resources_);
        item->setData(Qt::UserRole, resource.id);
        if (resource.id == selectedId) rowToSelect = index;
    }
    if (!values.isEmpty()) {
        resources_->setCurrentRow(rowToSelect);
        selectedResourceId_ = resources_->currentItem()->data(Qt::UserRole).toString();
    } else {
        selectedResourceId_.clear();
    }
    refreshSelected();
}

void ResourceAuthorityPage::refreshSelected()
{
    const auto values = model_->resources();
    const auto* item = resources_->currentItem();
    const QString selectedId = selectedResourceId_.isEmpty()
        ? (item ? item->data(Qt::UserRole).toString() : QString())
        : selectedResourceId_;
    const auto resourceIt = std::find_if(values.cbegin(), values.cend(), [&selectedId](const ProjectResource& resource) {
        return resource.id == selectedId;
    });
    const bool valid = resourceIt != values.cend();
    authority_->setEnabled(valid); scopes_->setEnabled(valid);
    if (!valid) return;
    const auto& resource = *resourceIt;
    const QSignalBlocker authorityBlocker(authority_);
    const QSignalBlocker scopesBlocker(scopes_);
    authority_->setCurrentIndex(authority_->findData(resource.authorityLevel));
    scopes_->setSelectedIds(resource.scopes);
}

void ResourceAuthorityPage::saveSelected()
{
    auto values = model_->resources();
    auto* resource = selectedResource(values);
    if (!resource) return;
    resource->authorityLevel = authority_->currentData().toString();
    resource->scopes = scopes_->selectedIds();
    model_->setResources(values);
}
