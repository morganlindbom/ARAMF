#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
class QTextBrowser;
class ReviewPage final:public QWidget{public:explicit ReviewPage(ProjectModel*,QWidget* parent=nullptr);public slots:void refreshFromModel();private:ProjectModel*model_;QTextBrowser*summary_;};
