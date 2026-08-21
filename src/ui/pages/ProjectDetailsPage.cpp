#include "ProjectDetailsPage.h"
#include <QFormLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>
ProjectDetailsPage::ProjectDetailsPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model),
      name_(new QLineEdit(this)),
      path_(new QLineEdit(this)),
      id_(new QLineEdit(this)),
      description_(new QTextEdit(this)),
      context_(new QComboBox(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Setup</h2>Define project identity. Technical defaults come from the selected template."),
        this));

    auto* form = new QFormLayout;
    form->addRow(tr("Project name"), name_);
    form->addRow(tr("Project path"), path_);
    form->addRow(tr("Project ID"), id_);
    form->addRow(tr("Description"), description_);

    context_->addItem(tr("Software Development"), QStringLiteral("software-development"));
    context_->addItem(tr("Desktop Application"), QStringLiteral("desktop-application"));
    context_->addItem(tr("Embedded Firmware"), QStringLiteral("embedded-firmware"));
    context_->addItem(tr("Web Application"), QStringLiteral("web-application"));
    context_->addItem(tr("Thesis / Research"), QStringLiteral("thesis"));
    form->addRow(tr("Project type"), context_);
    layout->addLayout(form);

    id_->setReadOnly(true);
    connect(name_, &QLineEdit::textChanged, model_, &ProjectModel::setProjectName);
    connect(path_, &QLineEdit::textChanged, model_, &ProjectModel::setProjectPath);
    connect(description_, &QTextEdit::textChanged, this, [this] {
        model_->setDescription(description_->toPlainText());
    });
    connect(context_, &QComboBox::currentIndexChanged, this, [this] {
        model_->setContext(context_->currentData().toString());
    });
    connect(model_, &ProjectModel::modelChanged,
            this, &ProjectDetailsPage::refreshFromModel);
    refreshFromModel();
}

void ProjectDetailsPage::refreshFromModel()
{
    const QSignalBlocker nameBlocker(name_);
    const QSignalBlocker pathBlocker(path_);
    const QSignalBlocker idBlocker(id_);
    const QSignalBlocker descriptionBlocker(description_);
    const QSignalBlocker contextBlocker(context_);

    name_->setText(model_->projectName());
    path_->setText(model_->projectPath());
    id_->setText(model_->projectId());
    description_->setPlainText(model_->description());

    const int index = context_->findData(model_->context());
    if (index >= 0) context_->setCurrentIndex(index);
}
