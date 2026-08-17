#include "CategoryPage.h"
#include <QListWidget>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>
CategoryPage::CategoryPage(ProjectModel* model,const QString& title,const QString& description,QStringList options,QWidget* parent):QWidget(parent),model_(model),options_(new QListWidget(this)){auto*l=new QVBoxLayout(this);l->addWidget(new QLabel(QStringLiteral("<h2>%1</h2>%2<br>Choose one or more compatible context tags.").arg(title,description),this));for(const auto&o:options){auto*item=new QListWidgetItem(o,options_);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Unchecked);}l->addWidget(options_);l->addStretch();connect(options_,&QListWidget::itemChanged,this,[this]{QStringList values;for(int i=0;i<options_->count();++i)if(options_->item(i)->checkState()==Qt::Checked)values<<options_->item(i)->text();model_->setContext(values.join(", "));});}
