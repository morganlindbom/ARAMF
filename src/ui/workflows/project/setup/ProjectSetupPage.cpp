#include "ProjectSetupPage.h"
#include "../../../../core/ProjectRootRebindService.h"

#include "core/AramfPaths.h"

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
      name_(new QLineEdit(this)),
      path_(new QLineEdit(this)),
      id_(new QLineEdit(this)),
      projectFilePath_(new QLineEdit(this)),
      type_(new QLineEdit(this)),
      description_(new QTextEdit(this))
{
    name_->setObjectName(QStringLiteral("canonicalProjectName"));
    // Description is the flexible field, but it must yield space to the
    // fixed-content controls above it when the page is short.
    description_->setObjectName(QStringLiteral("projectDescription"));
    description_->setMinimumHeight(64);
    description_->setMaximumHeight(96);
    description_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(
        tr("<h2>Project file, path &amp; Worker</h2>Define project identity, storage and the canonical Worker directory."), this);
    heading->setWordWrap(true);
    layout->addWidget(heading);

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
    auto* workerNameForm = new QFormLayout;
    workerNameSuffix_ = new QLineEdit(this);
    workerNameSuffix_->setObjectName(QStringLiteral("workerNameSuffix"));
    workerNameSuffix_->setPlaceholderText(tr("Optional suffix, e.g. ANDROID_PICO"));
    workerNamePreview_ = new QLabel(this);
    workerNamePreview_->setObjectName(QStringLiteral("workerNamePreview"));
    workerPath_ = new QLineEdit(this);
    workerPath_->setObjectName(QStringLiteral("workerPath"));
    workerPath_->setReadOnly(true);
    workerNameForm->addRow(tr("Worker name suffix"), workerNameSuffix_);
    workerNameForm->addRow(tr("Worker name"), workerNamePreview_);
    workerNameForm->addRow(tr("Worker path"), workerPath_);
    layout->addLayout(workerNameForm);

    auto* form = new QFormLayout;
    form->addRow(tr("Project name"), name_);
    auto* pathRow = new QWidget(this);
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    auto* browse = new QPushButton(tr("Browse..."), pathRow);
    path_->setMinimumWidth(0);
    path_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    browse->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    pathLayout->addWidget(path_);
    pathLayout->addWidget(browse);
    form->addRow(tr("Project path"), pathRow);
    projectFilePath_->setObjectName(QStringLiteral("projectFilePath"));
    projectFilePath_->setReadOnly(true);
    form->addRow(tr("Project file"), projectFilePath_);
    form->addRow(tr("Project ID"), id_);
    type_->setObjectName("projectType");
    form->addRow(tr("Project type"), type_);

    form->addRow(tr("Description"), description_);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    layout->addLayout(form);
    layout->addStretch();

    id_->setReadOnly(true);
    connect(name_, &QLineEdit::textEdited, model_, &ProjectModel::setProjectName);
    connect(type_, &QLineEdit::textEdited, model_, &ProjectModel::setContext);
    connect(path_, &QLineEdit::textChanged, this, [this](const QString& value) {
        model_->setProjectPath(value);
        syncCanonicalIdentity(true);
    });
    connect(workerNameSuffix_, &QLineEdit::textChanged, this, [this](const QString& raw) {
        workerNameEditing_ = true;
        workerNameRawInput_ = raw;
        const QString normalized = AramfPaths::normalizeWorkerNameSuffix(raw);
        model_->setWorkerNameSuffix(normalized);
        syncCanonicalIdentity(true);
    });
    connect(workerNameSuffix_, &QLineEdit::editingFinished, this, [this] {
        workerNameEditing_ = false;
        const QString normalized = AramfPaths::normalizeWorkerNameSuffix(workerNameSuffix_->text());
        const QSignalBlocker blocker(workerNameSuffix_);
        workerNameSuffix_->setText(normalized);
        model_->setWorkerNameSuffix(normalized);
        workerNameRawInput_ = normalized;
        syncCanonicalIdentity(true);
    });
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
    QString error;
    if (!saveCurrentProject(&error)) {
        QMessageBox::warning(this, tr("Save Project"), error);
    }
}

bool ProjectSetupPage::saveProjectAs(QString* error)
{
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Select Project Directory"), model_->projectPath().isEmpty() ? QDir::homePath() : model_->projectPath());
    if (directory.isEmpty()) {
        if (error) *error = tr("Save As was cancelled.");
        return false;
    }
    const QString filePath = QDir(directory).filePath(AramfPaths::workerDirectoryName(model_->workerNameSuffix()) + QStringLiteral(".aramf.json"));
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

bool ProjectSetupPage::saveCurrentProject(QString* error)
{
    if (model_->projectFilePath().trimmed().isEmpty()) {
        return saveProjectAs(error);
    }
    return writeProject(model_->projectFilePath(), error);
}

bool ProjectSetupPage::saveForGeneration(QString* error)
{
    return saveCurrentProject(error);
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

void ProjectSetupPage::syncCanonicalIdentity(bool deriveProjectFile)
{
    const QString workerName = AramfPaths::workerDirectoryName(model_->workerNameSuffix());
    workerNamePreview_->setText(workerName);
    const QString derivedFileName = workerName + QStringLiteral(".aramf.json");
    if (deriveProjectFile && !model_->projectPath().trimmed().isEmpty() && model_->projectFilePath().trimmed().isEmpty())
        model_->setProjectFilePath(QDir(model_->projectPath()).filePath(derivedFileName));
    projectFilePath_->setText(model_->projectFilePath().isEmpty() ? derivedFileName : model_->projectFilePath());
    const QString workerPath = model_->projectPath().trimmed().isEmpty()
        ? workerName
        : QDir(model_->projectPath()).filePath(workerName);
    workerPath_->setText(workerPath);
}

void ProjectSetupPage::refreshFromModel()
{
    const QSignalBlocker nameBlocker(name_);
    const QSignalBlocker pathBlocker(path_);
    const QSignalBlocker idBlocker(id_);
    const QSignalBlocker projectFileBlocker(projectFilePath_);
    const QSignalBlocker typeBlocker(type_);
    const QSignalBlocker descriptionBlocker(description_);
    const QSignalBlocker suffixBlocker(workerNameSuffix_);

    name_->setText(model_->projectName());
    path_->setText(model_->projectPath());
    id_->setText(model_->projectId());
    type_->setText(model_->context());
    type_->setReadOnly(model_->projectTypeLocked());
    description_->setPlainText(model_->description());
    if (!workerNameEditing_ && !workerNameSuffix_->hasFocus()) {
        workerNameRawInput_ = model_->workerNameSuffix();
        workerNameSuffix_->setText(workerNameRawInput_);
    }
    syncCanonicalIdentity(false);
}
