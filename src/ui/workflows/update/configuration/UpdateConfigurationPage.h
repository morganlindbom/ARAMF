#pragma once

#include <QWidget>

class ProjectModel;
class ProjectSetupPage;
class GenerationServices;
class QPlainTextEdit;
class QPushButton;

class UpdateConfigurationPage final : public QWidget
{
public:
    UpdateConfigurationPage(ProjectModel* model, ProjectSetupPage* setup,
                            GenerationServices* services, QWidget* parent = nullptr);
private:
    void validate();
    ProjectModel* model_ = nullptr;
    ProjectSetupPage* setup_ = nullptr;
    GenerationServices* services_ = nullptr;
    QPlainTextEdit* result_ = nullptr;
    QPushButton* updateButton_ = nullptr;
    QString validatedFingerprint_;
};
