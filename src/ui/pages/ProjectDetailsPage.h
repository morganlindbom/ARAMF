#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
class QLineEdit; class QTextEdit;
class ProjectDetailsPage final : public QWidget { public: explicit ProjectDetailsPage(ProjectModel* model, QWidget* parent=nullptr); public slots: void refreshFromModel(); private: ProjectModel* model_; QLineEdit* name_; QLineEdit* path_; QLineEdit* id_; QTextEdit* description_; };
