#pragma once

#include <QWidget>

class QLabel;
class ProjectModel;
class ReleaseManagementService;

class ReleaseReadinessPage final : public QWidget
{
    Q_OBJECT
public:
    ReleaseReadinessPage(ProjectModel* model, ReleaseManagementService* service,
                         QWidget* parent = nullptr);

private:
    void refresh();
    ProjectModel* model_;
    ReleaseManagementService* service_;
    QLabel* mode_;
    QLabel* counts_;
    QLabel* percentage_;
    QLabel* status_;
    QLabel* blockers_;
};

