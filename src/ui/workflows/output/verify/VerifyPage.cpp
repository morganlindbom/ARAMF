#include "VerifyPage.h"

#include <QAbstractItemView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString statusText(VerificationStatus status)
{
    switch (status) {
    case VerificationStatus::Pass: return QStringLiteral("PASS");
    case VerificationStatus::Warning: return QStringLiteral("WARNING");
    case VerificationStatus::Fail: return QStringLiteral("FAIL");
    case VerificationStatus::NotApplicable: return QStringLiteral("NOT APPLICABLE");
    }
    return QStringLiteral("FAIL");
}
}

VerifyPage::VerifyPage(ProjectModel* model, VerificationServices* services, QWidget* parent)
    : QWidget(parent), model_(model), services_(services), status_(new QLabel(this)), checks_(new QListWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Verify</h2>Verify that the generated ARAMF control plane is complete, consistent and matches the current project configuration."), this));
    status_->setText(tr("Overall status: NOT VERIFIED"));
    layout->addWidget(status_);
    verifyButton_ = new QPushButton(tr("Verify ARAMF control plane"), this);
    layout->addWidget(verifyButton_);
    checks_->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(checks_);
    layout->addStretch();
    connect(verifyButton_, &QPushButton::clicked, this, &VerifyPage::runVerification);
    connect(model_, &ProjectModel::modelChanged, this, [this] {
        status_->setText(tr("Overall status: OUTDATED — run Verify again"));
    });
}

void VerifyPage::runVerification()
{
    showResult(services_->verify(*model_, model_->generationOptions()));
}

void VerifyPage::showResult(const VerificationResult& result)
{
    status_->setText(tr("Overall status: %1").arg(statusText(result.overallStatus)));
    checks_->clear();
    for (const auto& check : result.checks) {
        checks_->addItem(QStringLiteral("%1  %2 — %3\n    %4")
                             .arg(statusText(check.status), check.name, check.id, check.details));
    }
}
