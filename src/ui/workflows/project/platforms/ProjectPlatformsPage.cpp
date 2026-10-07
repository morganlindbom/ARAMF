#include "ProjectPlatformsPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectPlatformsPage::ProjectPlatformsPage(ProjectModel* model, QWidget* parent, Section section)
    : QWidget(parent), model_(model), section_(section)
{
    auto* layout = new QVBoxLayout(this);
    const QString title = section_ == Section::Hosts ? tr("Host operating systems")
        : section_ == Section::Targets ? tr("Target platforms") : tr("Platforms");
    layout->addWidget(new QLabel(QStringLiteral("<h2>%1</h2>Select the operating systems and runtime environments the project supports.").arg(title), this));
    hosts_ = new CapabilityCheckGroup(tr("Host Operating Systems"), EnvironmentCatalog::operatingSystems(), 3, this);
    targets_ = new CapabilityCheckGroup(tr("Target Platforms"), EnvironmentCatalog::targets(), 3, this);
    hosts_->setObjectName(QStringLiteral("hostOperatingSystems"));
    targets_->setObjectName(QStringLiteral("targetPlatforms"));
    layout->addWidget(hosts_); layout->addWidget(targets_); layout->addStretch();
    connect(hosts_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.hostOperatingSystems=value; model_->setDevelopmentCapabilities(c); });
    connect(targets_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.targetPlatforms=value; model_->setDevelopmentCapabilities(c); });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectPlatformsPage::refresh);
    const bool all = section_ == Section::All;
    hosts_->setVisible(all || section_ == Section::Hosts);
    targets_->setVisible(all || section_ == Section::Targets);
    refresh();
}

void ProjectPlatformsPage::refresh() { const auto c=model_->developmentCapabilities(); hosts_->setSelectedIds(c.hostOperatingSystems); targets_->setSelectedIds(c.targetPlatforms); }
