#pragma once

#include "core/FrameworkKnowledge.h"
#include "core/ProjectModel.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

class FrameworkKnowledgeReviewPage final : public QWidget
{
    Q_OBJECT
public:
    explicit FrameworkKnowledgeReviewPage(ProjectModel* model, QWidget* parent = nullptr);

private:
    void refresh();
    void showSelected();
    QList<FrameworkKnowledgeEntry> currentEntries() const;

    ProjectModel* model_ = nullptr;
    QComboBox* filter_ = nullptr;
    QLabel* globalLocation_ = nullptr;
    QListWidget* entries_ = nullptr;
    QPlainTextEdit* details_ = nullptr;
    QPushButton* approve_ = nullptr;
    QPushButton* moreEvidence_ = nullptr;
    QPushButton* supersede_ = nullptr;
    QPushButton* promote_ = nullptr;
};
