#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"

class QLineEdit;
class QTextEdit;
class TemplateSelector;
class QPushButton;

class ProjectSetupPage final : public QWidget {
public:
    ProjectSetupPage(ProjectModel* model, TemplateManager* manager,
                     ProjectPersistence* persistence, QWidget* parent = nullptr);

public slots:
    void refreshFromModel();

public:
    // Saves the current model, invoking the existing Save As dialog when needed.
    // No generation is performed here.
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

    ProjectModel* model_;
    TemplateManager* manager_;
    ProjectPersistence* persistence_;
    TemplateSelector* templateSelector_;
    QLineEdit* name_;
    QLineEdit* path_;
    QLineEdit* id_;
    QTextEdit* description_;
};
