#include "WorkflowWidget.h"
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QSet>
#include <QtMath>
#include "core/ProjectModel.h"
#include "core/WorkflowPageMetadata.h"

namespace {

const QString kLevelOneIndent = QStringLiteral("     ");
const QString kLevelTwoIndent = QStringLiteral("          ");
const QColor kIncompleteParentBackground(255, 251, 224);
const QColor kCompletedParentBackground(204, 238, 211);

struct NavigationPage {
    QString number;
    QString title;
    WorkflowPageId id;
    int level;
    bool countsTowardSetupProgress = false;
    bool userCheckableCompletion = false;

    NavigationPage(const QString& pageNumber, const QString& pageTitle,
                   WorkflowPageId pageId, int pageLevel)
        : number(pageNumber), title(pageTitle), id(pageId), level(pageLevel)
    {
        const auto capabilities = workflowPageCapabilities(workflowPageKey(id));
        countsTowardSetupProgress = capabilities.countsTowardSetupProgress;
        userCheckableCompletion = capabilities.userCheckableCompletion;
    }
};

class NavigationDelegate final : public QStyledItemDelegate
{
public:
    explicit NavigationDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem textOption(option);
        const bool userCheckable = index.data(Qt::UserRole + 1).toBool();
        if (userCheckable) textOption.rect.setRight(textOption.rect.right() - 34);
        QStyledItemDelegate::paint(painter, textOption, index);
        if (!userCheckable) return;

        const bool completed = index.data(Qt::UserRole + 2).toBool();
        const QRect markerRect(option.rect.right() - 29, option.rect.top(), 24, option.rect.height());
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(completed ? QColor(47, 126, 75) : QColor(128, 145, 157));
        painter->setFont(QFont(option.font.family(), option.font.pointSize(), QFont::Bold));
        painter->drawText(markerRect, Qt::AlignCenter, completed ? QStringLiteral("✓") : QStringLiteral("○"));
        painter->restore();
    }
};

QListWidgetItem* addNavigationPage(QListWidget* list,
                                   QMap<int, WorkflowPageId>* rowPageIds,
                                   const NavigationPage& page)
{
    const QString indent = page.level == 2 ? kLevelTwoIndent : kLevelOneIndent;
    auto* item = new QListWidgetItem(indent + page.number + QStringLiteral("  ") + page.title, list);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rowPageIds->insert(list->count() - 1, page.id);
    return item;
}

}

WorkflowWidget::WorkflowWidget(QWidget* parent)
    : QWidget(parent),
      steps_(new QListWidget(this)),
      back_(new QPushButton(tr("Back"), this)),
      forward_(new QPushButton(tr("Forward"), this))
{
    steps_->setStyleSheet(QStringLiteral(
        "QListWidget::item { padding: 6px 4px; }"
        "QListWidget::item:selected { background: #e5eef8; color: #375774; "
        "border-left: 3px solid #4b82a6; }"));
    steps_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(new QLabel(tr("Workflow"), this));
    layout->addWidget(steps_, 1);
    steps_->setMinimumHeight(0);
    steps_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    steps_->setItemDelegate(new NavigationDelegate(steps_));
    steps_->viewport()->installEventFilter(this);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(back_);
    buttons->addWidget(forward_);
    back_->setMinimumHeight(32);
    forward_->setMinimumHeight(32);
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

void WorkflowWidget::setCompletionModel(ProjectModel* model)
{
    if (completionModel_ == model) {
        refreshCompletionState();
        return;
    }
    if (completionModel_) disconnect(completionModel_, nullptr, this, nullptr);
    completionModel_ = model;
    if (completionModel_) {
        connect(completionModel_, &ProjectModel::modelChanged,
                this, &WorkflowWidget::refreshCompletionState);
    }
    refreshCompletionState();
}

WorkflowPageId WorkflowWidget::currentPage() const
{
    const int row = steps_->currentRow();
    const auto page = rowPageIds_.constFind(row);
    return page != rowPageIds_.constEnd() ? page.value() : WorkflowPageId::Setup;
}

QListWidgetItem* WorkflowWidget::navigationItem(WorkflowPageId page) const
{
    return pageItems_.value(page, nullptr);
}

int WorkflowWidget::completablePageCount() const
{
    int count = 0;
    for (auto it = pageItems_.cbegin(); it != pageItems_.cend(); ++it) {
        if (it.value()->data(Qt::UserRole + 3).toBool()) ++count;
    }
    return count;
}

int WorkflowWidget::completedPageCount() const
{
    int count = 0;
    for (auto it = pageItems_.cbegin(); it != pageItems_.cend(); ++it) {
        if (it.value()->data(Qt::UserRole + 3).toBool()
            && it.value()->data(Qt::UserRole + 2).toBool()) ++count;
    }
    return count;
}

int WorkflowWidget::completionPercentage() const
{
    const int total = completablePageCount();
    if (total <= 0) return 0;
    return qRound(100.0 * completedPageCount() / total);
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
    pageItems_.clear();
    parentChildren_.clear();
    const QList<QPair<QString, QList<NavigationPage>>> groups{
        {tr("PROJECT"), {
            {QStringLiteral("1"), tr("What is the project?"), WorkflowPageId::Setup, 1},
            {QStringLiteral("1.1"), tr("Project file, path & Worker"), WorkflowPageId::ProjectIdentity, 2},
            {QStringLiteral("1.2"), tr("Project modules & templates"), WorkflowPageId::ProjectModulesTemplates, 2},
            {QStringLiteral("2"), tr("Academic"), WorkflowPageId::Academic, 1},
            {QStringLiteral("3"), tr("Which languages are used?"), WorkflowPageId::Languages, 1},
            {QStringLiteral("4"), tr("Which frameworks / SDKs are used?"), WorkflowPageId::Frameworks, 1},
            {QStringLiteral("5"), tr("Which development tools are used?"), WorkflowPageId::DevelopmentTools, 1},
            {QStringLiteral("6"), tr("Where does the project run?"), WorkflowPageId::Platforms, 1},
            {QStringLiteral("7"), tr("Which hardware / architecture is used?"), WorkflowPageId::HardwareArchitecture, 1},
            {QStringLiteral("8"), tr("How is it built, tested and delivered?"), WorkflowPageId::BuildDelivery, 1}}},
        {tr("AI"), {
            {QStringLiteral("9"), tr("Which AI agents are used?"), WorkflowPageId::AiAgents, 1},
            {QStringLiteral("10"), tr("What may AI work on?"), WorkflowPageId::AiResponsibilities, 1},
            {QStringLiteral("11"), tr("How autonomous may AI be?"), WorkflowPageId::AiAutonomy, 1},
            {QStringLiteral("12"), tr("Which ARAMF systems should AI use?"), WorkflowPageId::AiIntegration, 1}}},
        {tr("RESOURCES"), {
            {QStringLiteral("13"), tr("Which resources belong to the project?"), WorkflowPageId::ResourceInventory, 1},
            {QStringLiteral("14"), tr("Which sources are authoritative?"), WorkflowPageId::ResourceAuthority, 1},
            {QStringLiteral("15"), tr("How should AI use the resources?"), WorkflowPageId::ResourcePolicy, 1}}},
        {tr("RULES"), {
            {QStringLiteral("16"), tr("Which rules should apply?"), WorkflowPageId::RuleSelection, 1},
            {QStringLiteral("17"), tr("How should rules be routed?"), WorkflowPageId::RuleRouting, 1}}},
        {tr("MEMORY"), {
            {QStringLiteral("18"), tr("What should ARAMF remember?"), WorkflowPageId::MemoryCapture, 1},
            {QStringLiteral("19"), tr("How should project memory be maintained?"), WorkflowPageId::MemoryMaintenance, 1}}},
        {tr("RELEASE"), {
            {QStringLiteral("20"), tr("Version & release management"), WorkflowPageId::ReleaseOverview, 1},
            {QStringLiteral("20.1"), tr("Product version"), WorkflowPageId::ProductVersion, 2},
            {QStringLiteral("20.2"), tr("Page & component versions"), WorkflowPageId::ComponentVersions, 2},
            {QStringLiteral("20.3"), tr("Schema & compatibility"), WorkflowPageId::SchemaCompatibility, 2},
            {QStringLiteral("20.4"), tr("Release readiness"), WorkflowPageId::ReleaseReadiness, 2},
            {QStringLiteral("20.5"), tr("Approval & release history"), WorkflowPageId::ApprovalHistory, 2}}},
        {tr("GENERATE"), {
            {QStringLiteral("26"), tr("Review"), WorkflowPageId::Review, 1},
            {QStringLiteral("27"), tr("Generate"), WorkflowPageId::Generate, 1},
            {QStringLiteral("28"), tr("Verify"), WorkflowPageId::Verify, 1},
            {QStringLiteral("29"), tr("Finalize"), WorkflowPageId::Finalize, 1}}},
        {tr("UPDATE"), {
            {QStringLiteral("30"), tr("Review Framework Knowledge"), WorkflowPageId::UpdateReview, 1},
            {QStringLiteral("31"), tr("Apply Framework Knowledge"), WorkflowPageId::UpdateApply, 1},
            {QStringLiteral("32"), tr("Update complete ARAMF"), WorkflowPageId::UpdateConfiguration, 1},
            {QStringLiteral("33"), tr("ARAMF Improvement Backlog"), WorkflowPageId::ImprovementBacklog, 1}}}
    };
    for (const auto& group : groups) {
        auto* heading = new QListWidgetItem(group.first, steps_);
        heading->setFlags(Qt::NoItemFlags);
        heading->setForeground(QBrush(QColor(23, 60, 90)));
        heading->setBackground(QBrush(QColor(199, 221, 239)));
        heading->setTextAlignment(Qt::AlignCenter);
        QFont font = heading->font();
        font.setBold(true);
        heading->setFont(font);
        heading->setSizeHint(QSize(0, fontMetrics().height() + 16));
        for (const auto& page : group.second) {
            auto* item = addNavigationPage(steps_, &rowPageIds_, page);
            item->setData(Qt::UserRole + 1, page.userCheckableCompletion);
            item->setData(Qt::UserRole + 2, false);
            item->setData(Qt::UserRole + 3, page.countsTowardSetupProgress);
            pageItems_.insert(page.id, item);
            pageSequence_.append(page.id);
        }
    }
    parentChildren_.insert(WorkflowPageId::Setup,
                           {WorkflowPageId::ProjectIdentity, WorkflowPageId::ProjectModulesTemplates});
    refreshCompletionState();
}

void WorkflowWidget::refreshCompletionState()
{
    if (completionModel_) {
        QSet<QString> allowedCompletedPageIds;
        for (auto it = pageItems_.cbegin(); it != pageItems_.cend(); ++it) {
            if (it.value()->data(Qt::UserRole + 1).toBool()) {
                allowedCompletedPageIds.insert(workflowPageKey(it.key()));
            }
        }
        const QSet<QString> currentCompletedPageIds = completionModel_->completedPageIds();
        allowedCompletedPageIds.intersect(currentCompletedPageIds);
        if (currentCompletedPageIds != allowedCompletedPageIds) {
            completionModel_->setCompletedPageIds(allowedCompletedPageIds);
        }
    }
    for (auto it = pageItems_.cbegin(); it != pageItems_.cend(); ++it) {
        const WorkflowPageId page = it.key();
        auto* item = it.value();
        const bool isParent = parentChildren_.contains(page);
        const bool userCheckable = item->data(Qt::UserRole + 1).toBool();
        const bool completed = userCheckable && completionModel_
            && completionModel_->isPageCompleted(workflowPageKey(page));
        item->setData(Qt::UserRole + 2, completed);
        if (isParent) {
            bool allChildrenComplete = true;
            for (const auto child : parentChildren_.value(page)) {
                if (!completionModel_ || !completionModel_->isPageCompleted(workflowPageKey(child))) {
                    allChildrenComplete = false;
                    break;
                }
            }
            item->setBackground(QBrush(allChildrenComplete ? kCompletedParentBackground
                                                           : kIncompleteParentBackground));
        }
    }
    steps_->viewport()->update();
}

void WorkflowWidget::toggleCompletion(WorkflowPageId page)
{
    const auto item = pageItems_.constFind(page);
    if (!completionModel_ || item == pageItems_.constEnd()
        || !item.value()->data(Qt::UserRole + 1).toBool()) return;
    const QString key = workflowPageKey(page);
    completionModel_->setPageCompleted(key, !completionModel_->isPageCompleted(key));
    refreshCompletionState();
}

bool WorkflowWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == steps_->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            const auto* item = steps_->itemAt(mouse->position().toPoint());
            const int row = item ? steps_->row(item) : -1;
            const auto page = rowPageIds_.constFind(row);
            if (page != rowPageIds_.constEnd()
                && pageItems_.value(page.value())->data(Qt::UserRole + 1).toBool()
                && mouse->position().x() >= steps_->viewport()->width() - 36) {
                toggleCompletion(page.value());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
