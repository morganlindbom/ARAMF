#pragma once

#include <QWidget>

class ProjectModel;

class VerifyPage final : public QWidget {
public:
    explicit VerifyPage(QWidget* parent = nullptr);
    void setModel(ProjectModel* model);

private:
    ProjectModel* model_ = nullptr;
};
