#pragma once

#include <QWidget>
#include <QList>
#include <QMap>
#include "WorkflowPageId.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;
class ProjectModel;
class QEvent;

class WorkflowWidget final : public QWidget {
    Q_OBJECT
public:
    explicit WorkflowWidget(QWidget* parent = nullptr);
    WorkflowPageId currentPage() const;
    void setCurrentPage(WorkflowPageId page);
    void setStepCount(int count);
    void setCompletionModel(ProjectModel* model);
    QList<WorkflowPageId> workflowPageIds() const { return pageSequence_; }
    QListWidgetItem* navigationItem(WorkflowPageId page) const;
    int completionPercentage() const;
    int completablePageCount() const;
    int completedPageCount() const;

signals:
    void pageSelected(WorkflowPageId page);
    void backRequested();
    void forwardRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refreshCompletionState();
    void toggleCompletion(WorkflowPageId page);

    QListWidget* steps_;
    QPushButton* back_;
    QPushButton* forward_;
    QMap<int, WorkflowPageId> rowPageIds_;
    QList<WorkflowPageId> pageSequence_;
    QMap<WorkflowPageId, QListWidgetItem*> pageItems_;
    QMap<WorkflowPageId, QList<WorkflowPageId>> parentChildren_;
    ProjectModel* completionModel_ = nullptr;
};
