#pragma once

#include "core/ProjectModel.h"

#include <QWidget>

class QPlainTextEdit;

class ReviewPage final : public QWidget
{
    Q_OBJECT
public:
    explicit ReviewPage(ProjectModel* model, QWidget* parent = nullptr);

public slots:
    void refreshFromModel();

private:
    ProjectModel* model_ = nullptr;
    QPlainTextEdit* summary_ = nullptr;
};
