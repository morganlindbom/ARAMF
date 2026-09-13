#include "ReleaseReadinessPage.h"

#include "core/ComponentVersion.h"
#include "core/ProjectModel.h"

#include <QLabel>
#include <QVBoxLayout>

ReleaseReadinessPage::ReleaseReadinessPage(ProjectModel* model, ReleaseManagementService* service,
                                           QWidget* parent)
    : QWidget(parent), model_(model), service_(service), mode_(new QLabel(this)),
      counts_(new QLabel(this)), percentage_(new QLabel(this)), status_(new QLabel(this)),
      blockers_(new QLabel(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Release readiness</h2>Readiness is an approval report for an optional target, never a development gate."), this));
    mode_->setObjectName(QStringLiteral("releaseReadinessMode"));
    counts_->setObjectName(QStringLiteral("releaseReadinessCounts"));
    percentage_->setObjectName(QStringLiteral("releaseReadinessPercentage"));
    status_->setObjectName(QStringLiteral("releaseReadinessStatus"));
    blockers_->setObjectName(QStringLiteral("releaseReadinessBlockers"));
    blockers_->setWordWrap(true);
    layout->addWidget(mode_);
    layout->addWidget(counts_);
    layout->addWidget(percentage_);
    layout->addWidget(status_);
    layout->addWidget(blockers_);
    layout->addWidget(new QLabel(tr("Generation: AVAILABLE in Development mode and even when a selected target is not ready."), this));
    layout->addStretch();
    if (model_) connect(model_, &ProjectModel::modelChanged, this, &ReleaseReadinessPage::refresh);
    if (service_) connect(service_, &ReleaseManagementService::registryChanged, this, &ReleaseReadinessPage::refresh);
    refresh();
}

void ReleaseReadinessPage::refresh()
{
    if (!model_ || !service_) return;
    const auto summary = service_->readiness(model_->hasTargetRelease()
                                                 ? std::optional<int>(model_->targetRelease())
                                                 : std::nullopt);
    if (!summary.targetRequested) {
        mode_->setText(tr("Mode: Development / No target release"));
        counts_->setText(tr("Release approval: Not requested"));
        percentage_->setText(tr("Percentage: N/A"));
        status_->setText(tr("READY FOR DEVELOPMENT — not a release approval request"));
        blockers_->setText(tr("Generation: AVAILABLE"));
        return;
    }
    mode_->setText(tr("Target Release %1").arg(summary.targetRelease));
    counts_->setText(tr("Approved %1 / %2 — Remaining %3 / %2")
                         .arg(summary.approved).arg(summary.totalRequired).arg(summary.remaining));
    percentage_->setText(tr("Percentage: %1% of required components").arg(summary.percentage));
    status_->setText(summary.readyForApproval
                         ? tr("READY FOR RELEASE APPROVAL")
                         : tr("NOT READY FOR RELEASE APPROVAL"));
    blockers_->setText(summary.blockingComponents.isEmpty()
                           ? tr("No blocking components.")
                           : tr("Blocking components: %1").arg(summary.blockingComponents.join(QStringLiteral(", "))));
}

