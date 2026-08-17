#include "ProjectDetailsPage.h"
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>
ProjectDetailsPage::ProjectDetailsPage(ProjectModel* model, QWidget* parent):QWidget(parent),model_(model),name_(new QLineEdit(this)),path_(new QLineEdit(this)),id_(new QLineEdit(this)),description_(new QTextEdit(this)){ auto* l=new QVBoxLayout(this); l->addWidget(new QLabel(tr("Project Details"),this)); auto* f=new QFormLayout; f->addRow(tr("Project name"),name_); f->addRow(tr("Project path"),path_); f->addRow(tr("Project ID"),id_); f->addRow(tr("Description"),description_); l->addLayout(f); id_->setReadOnly(true); connect(name_,&QLineEdit::textChanged,model_,&ProjectModel::setProjectName); connect(path_,&QLineEdit::textChanged,model_,&ProjectModel::setProjectPath); connect(description_,&QTextEdit::textChanged,this,[this]{model_->setDescription(description_->toPlainText());}); connect(model_,&ProjectModel::modelChanged,this,&ProjectDetailsPage::refreshFromModel); refreshFromModel(); }
void ProjectDetailsPage::refreshFromModel(){const QSignalBlocker a(name_),b(path_),c(id_),d(description_);name_->setText(model_->projectName());path_->setText(model_->projectPath());id_->setText(model_->projectId());description_->setPlainText(model_->description());}
