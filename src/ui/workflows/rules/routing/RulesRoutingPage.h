#pragma once

#include <QWidget>

class ProjectModel;

class RulesRoutingPage final : public QWidget {
public:
    explicit RulesRoutingPage(QWidget* parent = nullptr);
    void setModel(ProjectModel* model);

private:
    ProjectModel* model_ = nullptr;
};
