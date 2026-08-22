#include "TemplateSelector.h"

#include <QComboBox>
#include <QFormLayout>
#include <QSignalBlocker>

TemplateSelector::TemplateSelector(ProjectModel* model, TemplateManager* manager, QWidget* parent)
    : QWidget(parent),
      model_(model),
      manager_(manager),
      selector_(new QComboBox(this))
{
    auto* layout = new QFormLayout(this);
    layout->addRow(tr("Template"), selector_);
    selector_->addItem(tr("Disable"), QString());
    for (const auto& definition : manager_->definitions()) {
        selector_->addItem(definition.displayName, definition.id);
    }
    connect(selector_, &QComboBox::currentIndexChanged, this, [this] {
        const QString templateId = selector_->currentData().toString();
        if (templateId.isEmpty()) {
            model_->setTemplateId(QString());
            model_->setDevelopmentCapabilities({});
            model_->setAcademicConfiguration({});
            model_->setAiConfiguration({});
            return;
        }
        manager_->applyTemplate(model_, templateId);
    });
    connect(model_, &ProjectModel::modelChanged, this, &TemplateSelector::refreshFromModel);
    refreshFromModel();
}

void TemplateSelector::refreshFromModel()
{
    const QSignalBlocker blocker(selector_);
    const int index = selector_->findData(model_->templateId());
    if (index >= 0) selector_->setCurrentIndex(index);
}
