#include "ResourceInventoryPage.h"

#include "core/EnvironmentCatalog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QUuid>

ResourceInventoryPage::ResourceInventoryPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model), resources_(new QListWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Which resources belong to the project?</h2>Add and manage the files, folders, documentation and external references used by this project."), this));
    auto* actions = new QHBoxLayout;
    auto* addFile = new QPushButton(tr("Add File"), this);
    auto* addFolder = new QPushButton(tr("Add Folder"), this);
    auto* addUrl = new QPushButton(tr("Add URL"), this);
    auto* remove = new QPushButton(tr("Remove"), this);
    actions->addWidget(addFile); actions->addWidget(addFolder); actions->addWidget(addUrl); actions->addWidget(remove); actions->addStretch();
    layout->addLayout(actions);
    layout->addWidget(new QLabel(tr("Resources"), this));
    resources_->setMinimumHeight(120);
    resources_->setMaximumHeight(240);
    resources_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(resources_);

    auto* details = new QGroupBox(tr("Selected Resource"), this);
    details->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto* detailsLayout = new QVBoxLayout(details);
    detailsEmptyState_ = new QLabel(tr("No resource selected."), details);
    detailsEditor_ = new QWidget(details);
    detailsEditor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto* form = new QFormLayout(detailsEditor_);
    name_ = new QLineEdit(detailsEditor_);
    type_ = new QComboBox(detailsEditor_);
    for (const auto& option : EnvironmentCatalog::resourceTypes()) type_->addItem(option.first, option.second);
    location_ = new QLineEdit(detailsEditor_);
    description_ = new QTextEdit(detailsEditor_);
    description_->setMinimumHeight(60);
    description_->setMaximumHeight(100);
    description_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    enabled_ = new QCheckBox(tr("Enabled"), detailsEditor_);
    locationMode_ = new QComboBox(detailsEditor_);
    locationMode_->addItem(tr("Reference original location"), QStringLiteral("referenced"));
    locationMode_->addItem(tr("Copy into project resources"), QStringLiteral("project-local-copy"));
    form->addRow(tr("Name"), name_); form->addRow(tr("Type"), type_); form->addRow(tr("Location"), location_);
    form->addRow(tr("Description"), description_); form->addRow(enabled_); form->addRow(tr("Location Mode"), locationMode_);
    detailsLayout->addWidget(detailsEmptyState_);
    detailsLayout->addWidget(detailsEditor_);
    layout->addWidget(details);
    layout->addStretch();

    connect(addFile, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Add Resource File"));
        if (path.isEmpty()) return;
        ProjectResource resource;
        resource.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        resource.name = QFileInfo(path).fileName(); resource.type = QStringLiteral("file"); resource.location = path;
        updateStatus(resource); addResource(resource);
    });
    connect(addFolder, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getExistingDirectory(this, tr("Add Resource Folder"));
        if (path.isEmpty()) return;
        ProjectResource resource;
        resource.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        resource.name = QFileInfo(path).fileName(); resource.type = QStringLiteral("folder"); resource.location = path;
        updateStatus(resource); addResource(resource);
    });
    connect(addUrl, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString url = QInputDialog::getText(this, tr("Add URL Resource"), tr("URL"), QLineEdit::Normal, QString(), &ok);
        if (!ok || url.trimmed().isEmpty()) return;
        const QString name = QInputDialog::getText(this, tr("Add URL Resource"), tr("Name"), QLineEdit::Normal, url, &ok);
        if (!ok) return;
        ProjectResource resource;
        resource.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        resource.name = name.trimmed(); resource.type = QStringLiteral("url"); resource.location = url.trimmed(); resource.status = QStringLiteral("unknown");
        addResource(resource);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = resources_->currentRow();
        if (row < 0 || row >= model_->resources().size()) return;
        auto values = model_->resources(); values.removeAt(row); model_->setResources(values);
    });
    connect(resources_, &QListWidget::currentRowChanged, this, [this] { refreshDetails(); });
    connect(resources_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (!item || item->flags() == Qt::NoItemFlags) return;
        const QString id = item->data(Qt::UserRole).toString();
        auto values = model_->resources();
        for (auto& resource : values) {
            if (resource.id == id) {
                resource.enabled = item->checkState() == Qt::Checked;
                model_->setResources(values);
                break;
            }
        }
    });
    connect(name_, &QLineEdit::textChanged, this, [this] { saveDetails(); });
    connect(type_, &QComboBox::currentIndexChanged, this, [this] { saveDetails(); });
    connect(location_, &QLineEdit::textChanged, this, [this] { saveDetails(); });
    connect(description_, &QTextEdit::textChanged, this, [this] { saveDetails(); });
    connect(enabled_, &QCheckBox::toggled, this, [this] { saveDetails(); });
    connect(locationMode_, &QComboBox::currentIndexChanged, this, [this] { saveDetails(); });
    connect(model_, &ProjectModel::modelChanged, this, &ResourceInventoryPage::refresh);
    refresh();
}

void ResourceInventoryPage::addResource(const ProjectResource& resource)
{
    auto values = model_->resources(); values.append(resource); model_->setResources(values);
    refresh(); resources_->setCurrentRow(values.size() - 1);
}

void ResourceInventoryPage::updateStatus(ProjectResource& resource)
{
    resource.status = QFileInfo(resource.location).exists() ? QStringLiteral("available") : QStringLiteral("missing");
}

void ResourceInventoryPage::refresh()
{
    const QString selectedId = resources_->currentItem() ? resources_->currentItem()->data(Qt::UserRole).toString() : QString();
    const QSignalBlocker blocker(resources_);
    resources_->clear();
    const auto values = model_->resources();
    int selectedRow = -1;
    for (int index = 0; index < values.size(); ++index) {
        const auto& resource = values.at(index);
        auto* item = new QListWidgetItem(QStringLiteral("%1 — %2 — %3").arg(resource.name, resource.type, resource.location), resources_);
        item->setData(Qt::UserRole, resource.id); item->setCheckState(resource.enabled ? Qt::Checked : Qt::Unchecked);
        if (resource.id == selectedId) selectedRow = index;
    }
    if (values.isEmpty()) {
        auto* empty = new QListWidgetItem(tr("No resources configured"), resources_); empty->setFlags(Qt::NoItemFlags);
    } else {
        resources_->setCurrentRow(selectedRow >= 0 ? selectedRow : 0);
    }
    refreshDetails();
}

void ResourceInventoryPage::refreshDetails()
{
    const int row = resources_->currentRow();
    const auto values = model_->resources();
    const bool valid = row >= 0 && row < values.size();
    detailsEditor_->setVisible(valid);
    detailsEmptyState_->setVisible(!valid);
    if (!valid) return;
    const auto& resource = values.at(row);
    const QSignalBlocker b1(name_), b2(type_), b3(location_), b4(description_), b5(enabled_), b6(locationMode_);
    name_->setText(resource.name);
    const int typeIndex = type_->findData(resource.type);
    type_->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    location_->setText(resource.location);
    description_->setPlainText(resource.description);
    enabled_->setChecked(resource.enabled);
    const int locationModeIndex = locationMode_->findData(resource.locationMode);
    locationMode_->setCurrentIndex(locationModeIndex >= 0 ? locationModeIndex : 0);
}

void ResourceInventoryPage::saveDetails()
{
    const int row = resources_->currentRow();
    auto values = model_->resources();
    if (row < 0 || row >= values.size()) return;
    auto& resource = values[row];
    resource.name = name_->text(); resource.type = type_->currentData().toString(); resource.location = location_->text();
    resource.description = description_->toPlainText(); resource.enabled = enabled_->isChecked(); resource.locationMode = locationMode_->currentData().toString();
    if (resource.type != QStringLiteral("url")) updateStatus(resource);
    model_->setResources(values);
}
