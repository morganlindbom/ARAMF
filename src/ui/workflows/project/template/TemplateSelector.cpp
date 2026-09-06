#include "TemplateSelector.h"
#include "core/TemplateValidation.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QResizeEvent>

TemplateSelector::TemplateSelector(ProjectModel* model, TemplateManager* manager, QWidget* parent)
    : QWidget(parent), model_(model), manager_(manager), selector_(new QComboBox(this)),
      moduleFrame_(new QGroupBox(tr("Project modules"), this)),
      moduleGrid_(new QWidget(moduleFrame_)), status_(new QLabel(this)), remove_(new QPushButton(tr("Remove saved template"), this))
{
    moduleFrame_->setObjectName(QStringLiteral("moduleGroup"));
    moduleGrid_->setObjectName(QStringLiteral("moduleGrid"));
    templateFrame_ = new QGroupBox(tr("Project templates"), this);
    templateGrid_ = new QWidget(templateFrame_);
    templateFrame_->setObjectName(QStringLiteral("templateGroup"));
    templateGrid_->setObjectName(QStringLiteral("templateGrid"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    moduleFrame_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* frameLayout = new QVBoxLayout(moduleFrame_);
    frameLayout->setContentsMargins(6, 8, 6, 8);
    frameLayout->setSpacing(0);
    frameLayout->addWidget(moduleGrid_);
    auto* grid = new QGridLayout(moduleGrid_);
    grid->setContentsMargins(8, 6, 8, 6);
    grid->setHorizontalSpacing(28);
    grid->setVerticalSpacing(7);
    for (int column = 0; column < 4; ++column) grid->setColumnStretch(column, 1);
    moduleGrid_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(moduleFrame_);
    auto* templateFrameLayout = new QVBoxLayout(templateFrame_);
    templateFrameLayout->setContentsMargins(6, 8, 6, 8);
    templateFrameLayout->addWidget(templateGrid_);
    auto* templateGridLayout = new QGridLayout(templateGrid_);
    templateGridLayout->setContentsMargins(8, 6, 8, 6);
    templateGridLayout->setVerticalSpacing(7);
    templateGridLayout->setHorizontalSpacing(28);
    for (int column = 0; column < 2; ++column) templateGridLayout->setColumnStretch(column, 1);
    templateFrame_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    templateGrid_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(templateFrame_);
    auto* details = new QLabel(tr("Select one or more modules. Settings are merged; unchecking a module removes only its contribution."), this);
    details->setWordWrap(true); details->setObjectName(QStringLiteral("templateDetails")); layout->addWidget(details);
    auto* actions = new QVBoxLayout;
    auto* save = new QPushButton(tr("Save current configuration as template"), this);
    save->setObjectName("saveCustomTemplate"); remove_->setObjectName("removeCustomTemplate");
    save->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    remove_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    actions->addWidget(save); actions->addWidget(remove_); layout->addLayout(actions);
    status_->setObjectName("templateStatus"); status_->setWordWrap(true); layout->addWidget(status_);

    // Kept hidden as a compatibility bridge for older callers. The checkboxes
    // above are the only visible and authoritative module selector.
    selector_->setObjectName("templateSelector"); selector_->setVisible(false); layout->addWidget(selector_);
    connect(selector_, &QComboBox::currentIndexChanged, this, [this] {
        const QString id = selector_->currentData().toString(); QString error;
        manager_->applyModules(model_, id.isEmpty() ? QStringList{} : QStringList{id}, &error);
        if (!error.isEmpty()) status_->setText(error); refreshFromModel();
    });
    connect(save, &QPushButton::clicked, this, [this] {
        bool accepted = false;
        const QString name = QInputDialog::getText(this, tr("Save custom template"), tr("Template name"), QLineEdit::Normal, {}, &accepted);
        if (!accepted) return;
        QString id, error;
        if (!manager_->saveCustomTemplate(*model_, name, &id, &error)) status_->setText(error);
        else status_->setText(tr("Custom template saved: %1").arg(name));
    });
    connect(remove_, &QPushButton::clicked, this, [this] {
        if (!model_->templateId().startsWith("user-")) return;
        QString error; if (!manager_->removeCustomTemplate(model_->templateId(), &error)) status_->setText(error);
    });
    connect(manager_, &TemplateManager::templatesChanged, this, &TemplateSelector::refreshFromModel);
    connect(model_, &ProjectModel::modelChanged, this, &TemplateSelector::refreshFromModel);
    refreshFromModel();
}

void TemplateSelector::refreshFromModel()
{
    const QSignalBlocker legacyBlocker(selector_);
    selector_->clear(); selector_->addItem(tr("No modules"), QString());
    const auto modules = manager_->moduleDefinitions();
    const auto official = manager_->officialDefinitions();
    const auto templates = manager_->customDefinitions();
    const auto legacyTemplates = manager_->definitions();
    for (const auto& d : modules) selector_->addItem(d.displayName, d.id);
    for (const auto& d : legacyTemplates) selector_->addItem(d.displayName, d.id);
    moduleChecks_.clear();
    auto* grid = qobject_cast<QGridLayout*>(moduleGrid_->layout());
    while (auto* item = grid->takeAt(0)) { if (item->widget()) item->widget()->deleteLater(); delete item; }
    const int columns = 4;
    for (int i = 0; i < modules.size(); ++i) {
        const auto& d = modules.at(i);
        auto* check = new QCheckBox(d.displayName, moduleGrid_);
        check->setObjectName(QStringLiteral("module_") + d.id);
        check->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        check->setToolTip(d.description + (d.exclusions.isEmpty() ? QString() : QStringLiteral("\n\nIntentionally excluded: ") + d.exclusions.join(", ")));
        check->setChecked(model_->templateModules().contains(d.id) || (model_->templateModules().isEmpty() && model_->templateId() == d.id));
        moduleChecks_ << check;
        grid->addWidget(check, i / columns, i % columns);
        connect(check, &QCheckBox::toggled, this, [this] {
            QStringList selected;
            for (auto* item : moduleChecks_) if (item->isChecked()) selected << item->property("moduleId").toString();
            QString error; manager_->applyModules(model_, selected, &error);
            if (!error.isEmpty()) status_->setText(error);
            QTimer::singleShot(0, this, &TemplateSelector::refreshFromModel);
        });
        check->setProperty("moduleId", d.id);
    }
    templateChecks_.clear();
    auto* templateGrid = qobject_cast<QGridLayout*>(templateGrid_->layout());
    while (auto* item = templateGrid->takeAt(0)) { if (item->widget()) item->widget()->deleteLater(); delete item; }
    QList<TemplateDefinition> visibleTemplates = official;
    visibleTemplates.append(templates);
    for (int i = 0; i < visibleTemplates.size(); ++i) {
        const auto& d = visibleTemplates.at(i);
        auto* check = new QCheckBox(d.displayName, templateGrid_);
        check->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        check->setToolTip(d.description);
        check->setChecked(model_->templateModules().contains(d.id));
        check->setProperty("templateId", d.id);
        templateChecks_ << check;
        templateGrid->addWidget(check, i / 2, i % 2);
        connect(check, &QCheckBox::toggled, this, [this] {
            QStringList selected;
            for (auto* item : moduleChecks_) if (item->isChecked()) selected << item->property("moduleId").toString();
            for (auto* item : templateChecks_) if (item->isChecked()) selected << item->property("templateId").toString();
            QString error; manager_->applyModules(model_, selected, &error);
            if (!error.isEmpty()) status_->setText(error);
            QTimer::singleShot(0, this, &TemplateSelector::refreshFromModel);
        });
    }
    if (templates.isEmpty()) {
        auto* empty = new QLabel(tr("No saved templates."), templateGrid_);
        empty->setObjectName(QStringLiteral("noSavedTemplates"));
        templateGrid->addWidget(empty, 0, 0, 1, 2);
    }
    templateFrame_->setTitle(tr("Project templates"));
    templateFrame_->updateGeometry();
    moduleFrame_->updateGeometry();
    const auto errors = TemplateValidation::readiness(*model_);
    const bool savedActive = !model_->templateId().isEmpty() && model_->templateId().startsWith("user-");
    status_->setText(model_->templateModules().isEmpty() ? tr("Manual configuration. No project modules selected.")
        : savedActive ? tr("Saved template active with %1 project modules.").arg(model_->templateModules().size())
        : errors.isEmpty() ? tr("%1 project modules active.").arg(model_->templateModules().size())
        : tr("Modules active. Remaining: %1").arg(errors.join("; ")));
    remove_->setEnabled(model_->templateId().startsWith("user-"));
    reflowGrids();
}

void TemplateSelector::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    reflowGrids();
}

void TemplateSelector::reflowGrids()
{
    auto* grid = qobject_cast<QGridLayout*>(moduleGrid_->layout());
    if (!grid) return;
    const int available = qMax(1, moduleGrid_->width());
    int widest = 1;
    for (auto* box : moduleChecks_) widest = qMax(widest, box->sizeHint().width());
    const int spacing = grid->horizontalSpacing();
    int columns = 1;
    for (int candidate = 4; candidate >= 1; --candidate) {
        if (candidate * widest + (candidate - 1) * spacing <= available) { columns = candidate; break; }
    }
    if (columns != moduleColumns_) {
        moduleColumns_ = columns;
        while (auto* item = grid->takeAt(0)) delete item;
        for (int column = 0; column < 4; ++column) grid->setColumnStretch(column, column < columns ? 1 : 0);
        for (int i = 0; i < moduleChecks_.size(); ++i) grid->addWidget(moduleChecks_.at(i), i / columns, i % columns);
    }

    auto* templateGrid = qobject_cast<QGridLayout*>(templateGrid_->layout());
    if (!templateGrid) return;
    int templateWidest = 1;
    for (auto* box : templateChecks_) templateWidest = qMax(templateWidest, box->sizeHint().width());
    const int templateColumns = templateChecks_.isEmpty() || templateWidest + spacing > available ? 1 : 2;
    if (templateColumns != templateColumns_) {
        templateColumns_ = templateColumns;
        while (auto* item = templateGrid->takeAt(0)) delete item;
        for (int column = 0; column < 2; ++column) templateGrid->setColumnStretch(column, column < templateColumns ? 1 : 0);
        for (int i = 0; i < templateChecks_.size(); ++i) templateGrid->addWidget(templateChecks_.at(i), i / templateColumns, i % templateColumns);
        if (templateChecks_.isEmpty()) {
            if (auto* empty = templateGrid_->findChild<QLabel*>("noSavedTemplates")) templateGrid->addWidget(empty, 0, 0, 1, templateColumns);
        }
    }
}
