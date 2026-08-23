#include "FrameworkKnowledgeReviewPage.h"

#include "core/FrameworkKnowledge.h"
#include "core/AramfPaths.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

FrameworkKnowledgeReviewPage::FrameworkKnowledgeReviewPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model), filter_(new QComboBox(this)), entries_(new QListWidget(this)), details_(new QPlainTextEdit(this)),
      approve_(new QPushButton(tr("Approve"), this)), moreEvidence_(new QPushButton(tr("More Evidence"), this)),
      supersede_(new QPushButton(tr("Supersede"), this)), promote_(new QPushButton(tr("Make available to future projects"), this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Review Framework Knowledge</h2>What has ARAMF learned and what requires my decision?"), this));
    filter_->addItems({tr("Needs Review"), tr("Candidates"), tr("Approved"), tr("Superseded"), tr("All")});
    layout->addWidget(filter_);
    globalLocation_ = new QLabel(this);
    globalLocation_->setObjectName(QStringLiteral("globalFrameworkKnowledgeLocation"));
    globalLocation_->setWordWrap(true);
    layout->addWidget(globalLocation_);
    entries_->setObjectName(QStringLiteral("frameworkKnowledgeEntries"));
    layout->addWidget(entries_);
    details_->setReadOnly(true);
    details_->setObjectName(QStringLiteral("frameworkKnowledgeDetails"));
    layout->addWidget(details_);
    auto* actions = new QHBoxLayout;
    approve_->setObjectName(QStringLiteral("approveFrameworkKnowledge"));
    moreEvidence_->setObjectName(QStringLiteral("requestMoreFrameworkEvidence"));
    supersede_->setObjectName(QStringLiteral("supersedeFrameworkKnowledge"));
    promote_->setObjectName(QStringLiteral("promoteFrameworkKnowledge"));
    actions->addWidget(approve_); actions->addWidget(moreEvidence_); actions->addWidget(supersede_); actions->addWidget(promote_); actions->addStretch();
    layout->addLayout(actions);
    connect(filter_, &QComboBox::currentIndexChanged, this, &FrameworkKnowledgeReviewPage::refresh);
    connect(entries_, &QListWidget::currentRowChanged, this, [this] { showSelected(); });
    connect(approve_, &QPushButton::clicked, this, [this] {
        const auto item = entries_->currentItem(); if (!item) return;
        bool accepted = false;
        const QString source = QInputDialog::getText(this, tr("Approve Framework Knowledge"), tr("Approval source:"), QLineEdit::Normal, {}, &accepted);
        if (!accepted) return;
        QString error; FrameworkKnowledgeService service;
        service.approve(model_->projectPath(), item->data(Qt::UserRole).toString(), source, &error);
        refresh();
    });
    connect(moreEvidence_, &QPushButton::clicked, this, [this] {
        const auto item = entries_->currentItem(); if (!item) return;
        QString error; FrameworkKnowledgeService service;
        service.markMoreEvidence(model_->projectPath(), item->data(Qt::UserRole).toString(), &error);
        refresh();
    });
    connect(supersede_, &QPushButton::clicked, this, [this] {
        const auto item = entries_->currentItem(); if (!item) return;
        bool accepted = false;
        const QString replacement = QInputDialog::getText(this, tr("Supersede Framework Knowledge"), tr("Replacement entry ID:"), QLineEdit::Normal, {}, &accepted);
        if (!accepted) return;
        QString error; FrameworkKnowledgeService service;
        service.supersede(model_->projectPath(), item->data(Qt::UserRole).toString(), replacement, &error);
        refresh();
    });
    connect(promote_, &QPushButton::clicked, this, [this] {
        const auto item = entries_->currentItem(); if (!item) return;
        QString error; FrameworkKnowledgeService service;
        service.promoteToGlobal(model_->projectPath(), item->data(Qt::UserRole).toString(), &error);
        refresh();
    });
    connect(model_, &ProjectModel::modelChanged, this, &FrameworkKnowledgeReviewPage::refresh);
    refresh();
}

QList<FrameworkKnowledgeEntry> FrameworkKnowledgeReviewPage::currentEntries() const
{
    FrameworkKnowledgeService service;
    QString error;
    return service.effectiveKnowledgeForProject(model_->projectPath(), &error);
}

void FrameworkKnowledgeReviewPage::refresh()
{
    globalLocation_->setText(tr("Global ARAMF Knowledge: %1").arg(FrameworkKnowledgeService().globalLibraryPath()));
    entries_->clear();
    const auto values = currentEntries();
    const int mode = filter_->currentIndex();
    for (const auto& entry : values) {
        const bool show = mode == 4
            || (mode == 0 && entry.status == QStringLiteral("candidate") && entry.reviewStatus == QStringLiteral("more-evidence"))
            || (mode == 1 && entry.status == QStringLiteral("candidate"))
            || (mode == 2 && entry.status == QStringLiteral("approved"))
            || (mode == 3 && entry.status == QStringLiteral("superseded"));
        if (!show) continue;
        auto* item = new QListWidgetItem(QStringLiteral("%1 - %2 (%3, %4, %5)").arg(entry.title, entry.id, entry.status, entry.reviewStatus, entry.origin), entries_);
        item->setData(Qt::UserRole, entry.id);
    }
    if (entries_->count() > 0) entries_->setCurrentRow(0);
    showSelected();
}

void FrameworkKnowledgeReviewPage::showSelected()
{
    const auto item = entries_->currentItem();
    if (!item) { details_->clear(); return; }
    const auto values = currentEntries();
    for (const auto& entry : values) {
        if (entry.id != item->data(Qt::UserRole).toString()) continue;
        details_->setPlainText(tr("ID: %1\nTitle: %2\nOrigin: %3\nLesson: %4\nStatus: %5\nReview status: %6\nScopes: %7\nEvidence: %8\nCreated: %9\nApproved: %10\nApproval source: %11\nSuperseded by: %12\nPortable: %13")
                              .arg(entry.id, entry.title, entry.origin, entry.lesson, entry.status, entry.reviewStatus, entry.scopes.join(QStringLiteral(", ")), entry.evidence.join(QStringLiteral("\n")),
                                   entry.createdAt, entry.approvedAt, entry.approvalSource, entry.supersededBy, entry.portable ? tr("yes") : tr("no")));
        const bool projectEntry = entry.origin == QStringLiteral("project") || entry.origin == QStringLiteral("project+global");
        approve_->setEnabled(projectEntry && entry.status == QStringLiteral("candidate"));
        moreEvidence_->setEnabled(projectEntry && entry.status == QStringLiteral("candidate"));
        supersede_->setEnabled(projectEntry && entry.status == QStringLiteral("approved"));
        const bool globallyAvailable = entry.origin == QStringLiteral("global") || entry.origin == QStringLiteral("project+global");
        promote_->setEnabled(projectEntry && !globallyAvailable && entry.status == QStringLiteral("approved") && entry.portable);
        promote_->setText(globallyAvailable ? tr("Available to future ARAMF projects") : tr("Make available to future projects"));
        return;
    }
}
