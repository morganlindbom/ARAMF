#include "TemplatePage.h"
#include <QListWidget>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>
TemplatePage::TemplatePage(ProjectModel* m,TemplateManager* t,QWidget* p):QWidget(p),model_(m),manager_(t),templates_(new QListWidget(this)){auto*l=new QVBoxLayout(this);l->addWidget(new QLabel(tr("<h2>Templates & capability layers</h2>Choose a primary template and optional compatible layers."),this));l->addWidget(new QLabel(tr("Multiple selections are allowed. The first checked built-in remains the primary template."),this));templates_->setSelectionMode(QAbstractItemView::MultiSelection);l->addWidget(templates_);for(const auto&id:manager_->builtInTemplates()){auto*item=new QListWidgetItem(id,templates_);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Unchecked);}connect(templates_,&QListWidget::itemChanged,this,[this](QListWidgetItem*item){if(item->checkState()==Qt::Checked)manager_->applyTemplate(model_,item->text());});connect(model_,&ProjectModel::modelChanged,this,&TemplatePage::refreshFromModel);refreshFromModel();}
void TemplatePage::refreshFromModel(){const QSignalBlocker b(templates_);const int primary=manager_->builtInTemplates().indexOf(model_->templateId());for(int i=0;i<templates_->count();++i)templates_->item(i)->setCheckState(i==primary?Qt::Checked:Qt::Unchecked);}
