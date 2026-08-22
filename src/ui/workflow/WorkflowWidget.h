#pragma once

#include <QWidget>
#include <QList>
#include <QMap>
#include "WorkflowPageId.h"

class QListWidget;
class QPushButton;

class WorkflowWidget final : public QWidget {
    Q_OBJECT
public:
    explicit WorkflowWidget(QWidget* parent = nullptr);
    WorkflowPageId currentPage() const;
    void setCurrentPage(WorkflowPageId page);
    void setStepCount(int count);

signals:
    void pageSelected(WorkflowPageId page);
    void backRequested();
    void forwardRequested();

private:
    QListWidget* steps_;
    QPushButton* back_;
    QPushButton* forward_;
    QMap<int, WorkflowPageId> rowPageIds_;
    QList<WorkflowPageId> pageSequence_;
};
