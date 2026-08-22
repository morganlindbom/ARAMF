#pragma once

#include "core/ProjectModel.h"
#include "core/Services.h"

#include <QWidget>

class ProjectSetupPage;

class QCheckBox;
class QPlainTextEdit;
class QPushButton;

class GeneratePage final : public QWidget
{
    Q_OBJECT

public:
    GeneratePage(ProjectModel* model, ProjectSetupPage* setupPage,
                 GenerationServices* services, QWidget* parent = nullptr);

private:
    GenerationOptions selectedOptions() const;
    void syncOptionsToModel() const;
    void updateSelectAllText();
    void showResult(const GenerationResult& result);

    ProjectModel* model_ = nullptr;
    ProjectSetupPage* setupPage_ = nullptr;
    GenerationServices* services_ = nullptr;
    QList<QCheckBox*> outputProducts_;
    QPushButton* selectAll_ = nullptr;
    QPlainTextEdit* result_ = nullptr;
};
