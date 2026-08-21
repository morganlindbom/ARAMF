#include "TemplatePage.h"
#include <QListWidget>
#include <QLabel>
#include <QSignalBlocker>
#include <QGroupBox>
#include <QCheckBox>
#include <QVBoxLayout>
TemplatePage::TemplatePage(ProjectModel* model, TemplateManager* manager, QWidget* parent)
    : QWidget(parent),
      model_(model),
      manager_(manager),
      templates_(new QListWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Template</h2>Select one primary template. It supplies the normal project configuration for later pages."),
        this));

    templates_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(templates_);

    auto* capabilities = new QGroupBox(tr("Optional Capabilities"), this);
    auto* capabilitiesLayout = new QVBoxLayout(capabilities);
    for (const auto& name : {tr("SQLite"), tr("Networking"), tr("Testing"), tr("Documentation")}) {
        auto* check = new QCheckBox(name, capabilities);
        capabilitiesLayout->addWidget(check);
        connect(check, &QCheckBox::toggled, this, [this, capabilities](bool) {
            QStringList values;
            for (auto* item : capabilities->findChildren<QCheckBox*>()) {
                if (item->isChecked()) values << item->text();
            }
            model_->setOptionValues(QStringLiteral("template-capabilities"), values);
        });
    }
    layout->addWidget(capabilities);
    layout->addStretch();

    for (const auto& definition : manager_->definitions()) {
        auto* item = new QListWidgetItem(definition.displayName, templates_);
        item->setData(Qt::UserRole, definition.id);
    }

    connect(templates_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* item) {
                if (item) manager_->applyTemplate(model_, item->data(Qt::UserRole).toString());
            });
    connect(model_, &ProjectModel::modelChanged,
            this, &TemplatePage::refreshFromModel);
    refreshFromModel();
}

void TemplatePage::refreshFromModel()
{
    const QSignalBlocker templatesBlocker(templates_);
    for (int i = 0; i < templates_->count(); ++i) {
        if (templates_->item(i)->data(Qt::UserRole).toString() == model_->templateId()) {
            templates_->setCurrentRow(i);
        }
    }

    const auto supported = manager_->definition(model_->templateId()).supportedCapabilities;
    for (auto* item : findChildren<QCheckBox*>()) {
        const bool applicable = supported.contains(item->text());
        item->setEnabled(applicable);
        item->setToolTip(applicable ? QString() : tr("Not recommended for the selected template."));
        const QSignalBlocker itemBlocker(item);
        item->setChecked(model_->optionValues(QStringLiteral("template-capabilities"))
                             .contains(item->text()));
    }
}
