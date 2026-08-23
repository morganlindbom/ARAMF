#pragma once

#include "core/ProjectModel.h"

#include <QWidget>

class QListWidget;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QShowEvent;
class UpdateExecutionService;

class FrameworkKnowledgeApplyPage final : public QWidget
{
    Q_OBJECT
public:
    explicit FrameworkKnowledgeApplyPage(ProjectModel* model, QWidget* parent = nullptr);

private:
    void showEvent(QShowEvent* event) override;
    void refresh();
    QStringList selectedIds() const;

    ProjectModel* model_ = nullptr;
    QListWidget* entries_ = nullptr;
    QPlainTextEdit* analysis_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* analyze_ = nullptr;
    QPushButton* apply_ = nullptr;
    QPushButton* selectAll_ = nullptr;
    QPushButton* clearAll_ = nullptr;
    QPushButton* execute_ = nullptr;
    QPushButton* validate_ = nullptr;
    QLabel* executionInfo_ = nullptr;
    UpdateExecutionService* execution_ = nullptr;
};
