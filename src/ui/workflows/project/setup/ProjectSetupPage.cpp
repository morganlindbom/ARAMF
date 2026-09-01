#include "ProjectSetupPage.h"
#include "../../../../core/ProjectRootRebindService.h"

#include "ui/workflows/project/template/TemplateSelector.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>

ProjectSetupPage::ProjectSetupPage(ProjectModel* model, TemplateManager* manager,
                                   ProjectPersistence* persistence, QWidget* parent)
    : QWidget(parent),
      model_(model),
      manager_(manager),
      persistence_(persistence),
      templateSelector_(new TemplateSelector(model, manager, this)),
      name_(new QLineEdit(this)),
      path_(new QLineEdit(this)),
      id_(new QLineEdit(this)),
      description_(new QTextEdit(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>What is the project?</h2>Select the template and define the project's identity."), this));

    auto* actions = new QHBoxLayout;
    for (const auto& action : {tr("New"), tr("Open"), tr("Save"), tr("Save As")}) {
        auto* button = new QPushButton(action, this);
        actions->addWidget(button);
        if (action == tr("New")) connect(button, &QPushButton::clicked, this, &ProjectSetupPage::newProject);
        if (action == tr("Open")) connect(button, &QPushButton::clicked, this, &ProjectSetupPage::openProject);
        if (action == tr("Save")) connect(button, &QPushButton::clicked, this, &ProjectSetupPage::saveProject);
        if (action == tr("Save As")) connect(button, &QPushButton::clicked, this, [this] { saveProjectAs(); });
    }
    actions->addStretch();
    layout->addLayout(actions);
    layout->addWidget(templateSelector_);

    auto* form = new QFormLayout;
    form->addRow(tr("Project name"), name_);
    auto* pathRow = new QWidget(this);
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    auto* browse = new QPushButton(tr("Browse..."), pathRow);
    pathLayout->addWidget(path_);
    pathLayout->addWidget(browse);
    form->addRow(tr("Project path"), pathRow);
    form->addRow(tr("Project ID"), id_);

    form->addRow(tr("Description"), description_);
    layout->addLayout(form);
    layout->addStretch();

    id_->setReadOnly(true);
    connect(name_, &QLineEdit::textChanged, model_, &ProjectModel::setProjectName);
    connect(path_, &QLineEdit::textChanged, model_, &ProjectModel::setProjectPath);
    connect(browse, &QPushButton::clicked, this, &ProjectSetupPage::browseProjectPath);
    connect(description_, &QTextEdit::textChanged, this, [this] {
        model_->setDescription(description_->toPlainText());
    });
    connect(model_, &ProjectModel::modelChanged,
            this, &ProjectSetupPage::refreshFromModel);
    refreshFromModel();
}

void ProjectSetupPage::browseProjectPath()
{
    const QString currentPath = model_->projectPath().trimmed();
    const QString initialDirectory = QFileInfo(currentPath).isDir()
        ? currentPath
        : QDir::homePath();
    const QString selectedDirectory = QFileDialog::getExistingDirectory(
        this,
        tr("Select Project Directory"),
        initialDirectory,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (selectedDirectory.isEmpty() || selectedDirectory == model_->projectPath()) {
        return;
    }

    // Browse changes only the managed target path. It never saves or changes
    // the separate ARAMF configuration-file path.
    model_->setProjectPath(selectedDirectory);
}

void ProjectSetupPage::newProject()
{
    if (!confirmDiscardOrSave()) return;
    model_->resetForNewProject();
    manager_->applyTemplate(model_, manager_->builtInTemplates().first());
    model_->setModified(true);
}

void ProjectSetupPage::openProject()
{
    if (!confirmDiscardOrSave()) return;
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open ARAMF Project"), QString(), tr("ARAMF Projects (*.aramf.json *.json);;All files (*)"));
    if (filePath.isEmpty()) return;

    QString error;
    if (!persistence_->load(model_, filePath, &error)) {
        QMessageBox::warning(this, tr("Open Project"), error);
        return;
    }
    const auto rebind = ProjectRootRebindService().rebind(model_, QFileInfo(filePath).absolutePath(), true);
    if (!rebind.success) QMessageBox::warning(this, tr("Open Project"), rebind.error);
}

void ProjectSetupPage::saveProject()
{
    if (model_->projectFilePath().isEmpty()) {
        saveProjectAs();
        return;
    }
    QString error;
    if (!writeProject(model_->projectFilePath(), &error)) {
        QMessageBox::warning(this, tr("Save Project"), error);
    }
}

bool ProjectSetupPage::saveProjectAs(QString* error)
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save ARAMF Project As"), QString(), tr("ARAMF Projects (*.aramf.json);;JSON files (*.json)"));
    if (filePath.isEmpty()) {
        if (error) *error = tr("Save As was cancelled.");
        return false;
    }
    QString saveError;
    if (!writeProject(filePath, &saveError)) {
        if (error) {
            *error = saveError;
        } else {
            QMessageBox::warning(this, tr("Save Project"), saveError);
        }
        return false;
    }
    return true;
}

bool ProjectSetupPage::writeProject(const QString& filePath, QString* error)
{
    QString saveError;
    if (!persistence_->save(*model_, filePath, &saveError)) {
        if (error) *error = saveError;
        return false;
    }
    model_->setProjectFilePath(filePath);
    model_->setModified(false);
    return true;
}

bool ProjectSetupPage::saveForGeneration(QString* error)
{
    if (model_->projectFilePath().trimmed().isEmpty()) {
        return saveProjectAs(error);
    }
    return writeProject(model_->projectFilePath(), error);
}

bool ProjectSetupPage::confirmDiscardOrSave()
{
    if (!model_->isModified()) return true;

    const auto choice = QMessageBox::question(
        this, tr("Unsaved Project"), tr("Save changes before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save) {
        saveProject();
        return !model_->isModified();
    }
    return true;
}

void ProjectSetupPage::refreshFromModel()
{
    const QSignalBlocker nameBlocker(name_);
    const QSignalBlocker pathBlocker(path_);
    const QSignalBlocker idBlocker(id_);
    const QSignalBlocker descriptionBlocker(description_);

    name_->setText(model_->projectName());
    path_->setText(model_->projectPath());
    id_->setText(model_->projectId());
    description_->setPlainText(model_->description());
}
