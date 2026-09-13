#include "ProductVersionPage.h"

#include "core/ComponentVersion.h"
#include "core/ProjectModel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

ProductVersionPage::ProductVersionPage(ProjectModel* model, ReleaseManagementService* service,
                                       QWidget* parent)
    : QWidget(parent), model_(model), service_(service), version_(new QLabel(this)),
      mode_(new QLabel(this)), target_(new QComboBox(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Product version</h2>Describe the ARAMF product independently from page/component versions."), this));
    auto* form = new QFormLayout;
    version_->setObjectName(QStringLiteral("aramfProductVersion"));
    mode_->setObjectName(QStringLiteral("releaseMode"));
    target_->setObjectName(QStringLiteral("targetRelease"));
    target_->addItem(tr("None (Development)"), 0);
    target_->addItem(tr("Target Release 1"), 1);
    target_->addItem(tr("Target Release 2"), 2);
    target_->addItem(tr("Target Release 3"), 3);
    form->addRow(tr("Current ARAMF version:"), version_);
    form->addRow(tr("Mode:"), mode_);
    form->addRow(tr("Target release:"), target_);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("A target is optional. Development, Generate, Test and Verify remain available in every mode."), this));
    layout->addStretch();

    connect(target_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (model_) model_->setTargetRelease(target_->itemData(index).toInt());
    });
    if (model_) connect(model_, &ProjectModel::modelChanged, this, &ProductVersionPage::refresh);
    Q_UNUSED(service_);
    refresh();
}

void ProductVersionPage::refresh()
{
    if (!model_ || !service_) return;
    version_->setText(service_->productVersion().toString());
    mode_->setText(model_->hasTargetRelease()
                       ? tr("Target Release %1").arg(model_->targetRelease())
                       : tr("Development / No target release"));
    const int index = target_->findData(model_->targetRelease());
    if (index >= 0) {
        const QSignalBlocker blocker(target_);
        target_->setCurrentIndex(index);
    }
}

