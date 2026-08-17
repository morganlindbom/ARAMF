#include "WorkflowWidget.h"
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

WorkflowWidget::WorkflowWidget(QWidget* parent) : QWidget(parent), steps_(new QListWidget(this)), back_(new QPushButton(tr("Back"), this)), forward_(new QPushButton(tr("Forward"), this)) {
    auto* layout = new QVBoxLayout(this); layout->addWidget(new QLabel(tr("Workflow"), this)); layout->addWidget(steps_, 1);
    auto* buttons = new QHBoxLayout; buttons->addWidget(back_); buttons->addWidget(forward_); layout->addLayout(buttons);
    connect(steps_, &QListWidget::currentRowChanged, this, &WorkflowWidget::stepSelected);
    connect(back_, &QPushButton::clicked, this, &WorkflowWidget::backRequested); connect(forward_, &QPushButton::clicked, this, &WorkflowWidget::forwardRequested);
}
int WorkflowWidget::currentStep() const { return steps_->currentRow(); }
void WorkflowWidget::setCurrentStep(int index) { steps_->setCurrentRow(index); }
void WorkflowWidget::setStepCount(int count) { steps_->clear(); const QStringList labels{tr("1  Project"),tr("2  Project Profile"),tr("3  Template"),tr("4  Context"),tr("5  Development Environment"),tr("6  AI Strategy"),tr("7  AI Tools"),tr("8  Rules & Routing"),tr("9  Memory"),tr("10 Resources"),tr("11 Review"),tr("12 Generate"),tr("13 Verify"),tr("14 Finalize")}; for (int i=0; i<count && i<labels.size(); ++i) steps_->addItem(labels[i]); }
void WorkflowWidget::setStepEnabled(int index, bool enabled, const QString& reason) { if (auto* item = steps_->item(index)) { item->setFlags(enabled ? item->flags() | Qt::ItemIsEnabled : item->flags() & ~Qt::ItemIsEnabled); item->setToolTip(enabled ? QString() : reason); } }
