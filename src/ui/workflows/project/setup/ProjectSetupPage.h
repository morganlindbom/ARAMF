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

private slots:
    void browseProjectPath();
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();

private:
    bool confirmDiscardOrSave();
    bool writeProject(const QString& filePath);

    ProjectModel* model_;
    TemplateManager* manager_;
    ProjectPersistence* persistence_;
    TemplateSelector* templateSelector_;
    QLineEdit* name_;
    QLineEdit* path_;
    QLineEdit* id_;
    QTextEdit* description_;
};
