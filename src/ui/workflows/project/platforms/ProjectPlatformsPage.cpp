#include "ProjectPlatformsPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectPlatformsPage::ProjectPlatformsPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Where does the project run?</h2>Select the operating systems and runtime environments the project supports."), this));
    hosts_ = new CapabilityCheckGroup(tr("Host Operating Systems"), EnvironmentCatalog::operatingSystems(), 3, this);
    targets_ = new CapabilityCheckGroup(tr("Target Platforms"), EnvironmentCatalog::targets(), 3, this);
    layout->addWidget(hosts_); layout->addWidget(targets_); layout->addStretch();
    connect(hosts_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.hostOperatingSystems=value; model_->setDevelopmentCapabilities(c); });
    connect(targets_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.targetPlatforms=value; model_->setDevelopmentCapabilities(c); });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectPlatformsPage::refresh);
    refresh();
}

void ProjectPlatformsPage::refresh() { const auto c=model_->developmentCapabilities(); hosts_->setSelectedIds(c.hostOperatingSystems); targets_->setSelectedIds(c.targetPlatforms); }
