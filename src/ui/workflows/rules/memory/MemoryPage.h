#pragma once

#include <QWidget>

class ProjectModel;

class MemoryPage final : public QWidget {
public:
    explicit MemoryPage(QWidget* parent = nullptr);
    void setModel(ProjectModel* model);

private:
    ProjectModel* model_ = nullptr;
};
