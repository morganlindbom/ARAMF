#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"

class QLineEdit;
class QTextEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QGroupBox;
class QLabel;

class ProjectSetupPage final : public QWidget {
public:
    ProjectSetupPage(ProjectModel* model, TemplateManager* manager,
                     ProjectPersistence* persistence, QWidget* parent = nullptr);

public slots:
    void refreshFromModel();

public:
    // Saves the current ProjectModel through the canonical project-file path.
    // This is the shared normal save operation used by page-level Save Work.
    bool saveCurrentProject(QString* error = nullptr);

    // Saves the current model, invoking the existing Save As dialog when needed.
    // Kept as a compatibility wrapper for the generation workflow; no
    // generation is performed here.
    bool saveForGeneration(QString* error = nullptr);

private slots:
    void browseProjectPath();
    void newProject();
    void openProject();
    void saveProject();
    bool saveProjectAs(QString* error = nullptr);

private:
    bool confirmDiscardOrSave();
    bool writeProject(const QString& filePath, QString* error = nullptr);
    void syncCanonicalIdentity(bool deriveProjectFile);

    ProjectModel* model_;
    TemplateManager* manager_;
    ProjectPersistence* persistence_;
    QLineEdit* name_;
    QLineEdit* path_;
    QLineEdit* id_;
    QLineEdit* projectFilePath_;
    QLineEdit* type_;
    QTextEdit* description_;
    QLineEdit* workerNameSuffix_ = nullptr;
    QLabel* workerNamePreview_ = nullptr;
    QLineEdit* workerPath_ = nullptr;
    QLabel* migrationStatus_ = nullptr;
    QLabel* migrationDetails_ = nullptr;
    QString workerNameRawInput_;
    bool workerNameEditing_ = false;
    QGroupBox* communicationGroup_ = nullptr;
    QComboBox* communicationProtocol_ = nullptr;
    QComboBox* communicationSourceTarget_ = nullptr;
    QComboBox* communicationDestinationTarget_ = nullptr;
    QComboBox* communicationSourceRole_ = nullptr;
    QComboBox* communicationDestinationRole_ = nullptr;
    QComboBox* communicationDirection_ = nullptr;
    QComboBox* communicationTransport_ = nullptr;
    QComboBox* communicationFrameType_ = nullptr;
    QComboBox* communicationLogicalModel_ = nullptr;
    QComboBox* communicationWireEncoding_ = nullptr;
    QComboBox* communicationByteOrder_ = nullptr;
    QLineEdit* communicationEndpointAAddress_ = nullptr;
    QLineEdit* communicationEndpointBAddress_ = nullptr;
    QLineEdit* communicationVersion_ = nullptr;
    QCheckBox* communicationAuthentication_ = nullptr;
    QCheckBox* communicationEncryption_ = nullptr;
};
