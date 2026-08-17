#include "ResourcesPage.h"
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSignalBlocker>
#include <QVBoxLayout>
ResourcesPage::ResourcesPage(ProjectModel*m,QWidget*p):QWidget(p),model_(m),resources_(new QListWidget(this)){auto*l=new QVBoxLayout(this);l->addWidget(new QLabel(tr("<h2>Resources</h2>Add several resources and classify each one explicitly."),this));auto*add=new QPushButton(tr("Add resource"),this);l->addWidget(add);l->addWidget(resources_);connect(add,&QPushButton::clicked,this,[this]{auto*item=new QListWidgetItem(tr("New resource — Source of Truth"),resources_);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Checked);});connect(model_,&ProjectModel::modelChanged,this,&ResourcesPage::refreshFromModel);refreshFromModel();}
void ResourcesPage::refreshFromModel(){const QSignalBlocker b(resources_);resources_->clear();resources_->addItems(model_->resourceNames());if(resources_->count()==0)resources_->addItem(tr("No resources configured"));}
