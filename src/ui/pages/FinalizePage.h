#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
class QLabel;
class FinalizePage final:public QWidget{public:explicit FinalizePage(ProjectModel*,QWidget* parent=nullptr);private:QLabel*status_;};
