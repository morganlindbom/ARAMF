#include "ResourcesPage.h"
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

ResourcesPage::ResourcesPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model),
      resources_(new QListWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Resources</h2>Add project-local source material and mark authoritative references."),
        this));

    auto* add = new QPushButton(tr("Add resource"), this);
    layout->addWidget(add);
    layout->addWidget(resources_);

    connect(add, &QPushButton::clicked, this, [this] {
        auto* item = new QListWidgetItem(tr("New resource - Source of Truth"), resources_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    });
    connect(model_, &ProjectModel::modelChanged,
            this, &ResourcesPage::refreshFromModel);
    refreshFromModel();
}

void ResourcesPage::refreshFromModel()
{
    const QSignalBlocker blocker(resources_);
    resources_->clear();
    resources_->addItems(model_->resourceNames());
    if (resources_->count() == 0) {
        resources_->addItem(tr("No resources configured"));
    }
}
