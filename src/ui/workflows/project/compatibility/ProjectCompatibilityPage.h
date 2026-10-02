#pragma once

#include <QWidget>

class ProjectModel;
class QLabel;

class ProjectCompatibilityPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectCompatibilityPage(ProjectModel* model, QWidget* parent = nullptr);

public slots:
    void refreshFromModel();

private:
    ProjectModel* model_ = nullptr;
    QLabel* schema_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* notices_ = nullptr;
    QLabel* workerPath_ = nullptr;
    QLabel* projectFile_ = nullptr;
};
