#include "WorkflowWidget.h"
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QBrush>
#include <QColor>
#include <QFont>

WorkflowWidget::WorkflowWidget(QWidget* parent)
    : QWidget(parent),
      steps_(new QListWidget(this)),
      back_(new QPushButton(tr("Back"), this)),
      forward_(new QPushButton(tr("Forward"), this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Workflow"), this));
    layout->addWidget(steps_, 1);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(back_);
    buttons->addWidget(forward_);
    layout->addLayout(buttons);

    connect(steps_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < pageRows_.size()) {
            emit stepSelected(pageRows_.at(row));
        }
    });
    connect(back_, &QPushButton::clicked, this, &WorkflowWidget::backRequested);
    connect(forward_, &QPushButton::clicked, this, &WorkflowWidget::forwardRequested);
}

int WorkflowWidget::currentStep() const
{
    const int row = steps_->currentRow();
    return row >= 0 && row < pageRows_.size() ? pageRows_.at(row) : -1;
}

void WorkflowWidget::setCurrentStep(int index)
{
    const int row = pageRows_.indexOf(index);
    if (row >= 0) steps_->setCurrentRow(row);
}

void WorkflowWidget::setStepCount(int)
{
    steps_->clear();
    pageRows_.clear();
    const QList<QPair<QString, QList<QPair<QString, int>>>> groups{
        {tr("PROJECT"), {{tr("Setup"), 0}, {tr("Environment"), 2}}},
        {tr("TEMPLATE"), {{tr("Primary Template"), 1}}},
        {tr("AI"), {{tr("AI Configuration"), 3}}},
        {tr("RESOURCES"), {{tr("Project Resources"), 4}}},
        {tr("RULES"), {{tr("Rules & Routing"), 5}, {tr("Memory"), 6}}},
        {tr("GENERATE"), {{tr("Review"), 7}, {tr("Generate"), 8}, {tr("Verify"), 9}, {tr("Finalize"), 10}}}
    };
    for (const auto& group : groups) {
        auto* heading = new QListWidgetItem(group.first, steps_);
        heading->setFlags(Qt::NoItemFlags);
        heading->setForeground(QBrush(QColor(100, 110, 125)));
        QFont font = heading->font();
        font.setBold(true);
        heading->setFont(font);
        for (const auto& pageEntry : group.second) {
            auto* item = new QListWidgetItem(QStringLiteral("    ") + pageEntry.first, steps_);
            item->setData(Qt::UserRole, pageEntry.second);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            pageRows_.append(pageEntry.second);
        }
    }
}

void WorkflowWidget::setStepEnabled(int index, bool enabled, const QString& reason)
{
    const int row = pageRows_.indexOf(index);
    if (row < 0) return;

    if (auto* item = steps_->item(row)) {
        item->setFlags(enabled ? item->flags() | Qt::ItemIsEnabled
                               : item->flags() & ~Qt::ItemIsEnabled);
        item->setToolTip(enabled ? QString() : reason);
    }
}
