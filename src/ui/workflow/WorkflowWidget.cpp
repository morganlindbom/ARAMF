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
        const auto page = rowPageIds_.constFind(row);
        if (page != rowPageIds_.constEnd()) {
            emit pageSelected(page.value());
        }
    });
    connect(back_, &QPushButton::clicked, this, &WorkflowWidget::backRequested);
    connect(forward_, &QPushButton::clicked, this, &WorkflowWidget::forwardRequested);
}

WorkflowPageId WorkflowWidget::currentPage() const
{
    const int row = steps_->currentRow();
    const auto page = rowPageIds_.constFind(row);
    return page != rowPageIds_.constEnd() ? page.value() : WorkflowPageId::Setup;
}

void WorkflowWidget::setCurrentPage(WorkflowPageId page)
{
    for (auto it = rowPageIds_.cbegin(); it != rowPageIds_.cend(); ++it) {
        if (it.value() == page) {
            steps_->setCurrentRow(it.key());
            return;
        }
    }
}

void WorkflowWidget::setStepCount(int)
{
    steps_->clear();
    rowPageIds_.clear();
    pageSequence_.clear();
    const QList<QPair<QString, QList<QPair<QString, WorkflowPageId>>>> groups{
        {tr("PROJECT"), {{tr("What is the project?"), WorkflowPageId::Setup}, {tr("Academic"), WorkflowPageId::Academic}, {tr("Which languages are used?"), WorkflowPageId::Languages}, {tr("Which frameworks / SDKs are used?"), WorkflowPageId::Frameworks}, {tr("Which development tools are used?"), WorkflowPageId::DevelopmentTools}, {tr("Where does the project run?"), WorkflowPageId::Platforms}, {tr("Which hardware / architecture is used?"), WorkflowPageId::HardwareArchitecture}, {tr("How is it built, tested and delivered?"), WorkflowPageId::BuildDelivery}}},
        {tr("AI"), {{tr("Which AI agents are used?"), WorkflowPageId::AiAgents}, {tr("What may AI work on?"), WorkflowPageId::AiResponsibilities}, {tr("How autonomous may AI be?"), WorkflowPageId::AiAutonomy}, {tr("Which ARAMF systems should AI use?"), WorkflowPageId::AiIntegration}}},
        {tr("RESOURCES"), {{tr("Which resources belong to the project?"), WorkflowPageId::ResourceInventory}, {tr("Which sources are authoritative?"), WorkflowPageId::ResourceAuthority}, {tr("How should AI use the resources?"), WorkflowPageId::ResourcePolicy}}},
        {tr("RULES"), {{tr("Which rules should apply?"), WorkflowPageId::RuleSelection}, {tr("How should rules be routed?"), WorkflowPageId::RuleRouting}}},
        {tr("MEMORY"), {{tr("What should ARAMF remember?"), WorkflowPageId::MemoryCapture}, {tr("How should project memory be maintained?"), WorkflowPageId::MemoryMaintenance}}},
        {tr("GENERATE"), {{tr("Review"), WorkflowPageId::Review}, {tr("Generate"), WorkflowPageId::Generate}, {tr("Verify"), WorkflowPageId::Verify}, {tr("Finalize"), WorkflowPageId::Finalize}}}
    };
    for (const auto& group : groups) {
        auto* heading = new QListWidgetItem(group.first, steps_);
        heading->setFlags(Qt::NoItemFlags);
        heading->setForeground(QBrush(QColor(100, 110, 125)));
        QFont font = heading->font();
        font.setBold(true);
        heading->setFont(font);
        for (const auto& pageEntry : group.second) {
            const int displayNumber = pageSequence_.size() + 1;
            const QString label = QStringLiteral("%1  %2")
                                      .arg(displayNumber, 2, 10, QLatin1Char(' '))
                                      .arg(pageEntry.first);
            auto* item = new QListWidgetItem(QStringLiteral("    ") + label, steps_);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            rowPageIds_.insert(steps_->count() - 1, pageEntry.second);
            pageSequence_.append(pageEntry.second);
        }
    }
}
