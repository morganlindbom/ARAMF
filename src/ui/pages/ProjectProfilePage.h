#pragma once
#include <QWidget>
#include <QList>
#include <QStringList>
#include "core/ProjectModel.h"
class QCheckBox;
class ProjectProfilePage final : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectProfilePage(ProjectModel* model, QWidget* parent = nullptr);
signals:
    void profileChanged(const QStringList& profileIds);
private:
    QList<QCheckBox*> options_;
    ProjectModel* model_;
};
