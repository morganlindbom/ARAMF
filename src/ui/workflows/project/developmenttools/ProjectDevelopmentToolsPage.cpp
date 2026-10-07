#include "ProjectDevelopmentToolsPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectDevelopmentToolsPage::ProjectDevelopmentToolsPage(ProjectModel* model, QWidget* parent, Section section)
    : QWidget(parent), model_(model), section_(section)
{
    auto* layout = new QVBoxLayout(this);
    const QString title = section_ == Section::Ide ? tr("IDE & editors")
        : section_ == Section::VersionControl ? tr("Version control")
        : section_ == Section::Support ? tr("Development support") : tr("Development tools");
    layout->addWidget(new QLabel(QStringLiteral("<h2>%1</h2>Select the tools used to develop, inspect and version the project.").arg(title), this));
    ides_ = new CapabilityCheckGroup(tr("IDE / Editor"), EnvironmentCatalog::ides(), 3, this);
    versionControl_ = new CapabilityCheckGroup(tr("Version Control"), EnvironmentCatalog::versionControlSystems(), 3, this);
    support_ = new CapabilityCheckGroup(tr("Development Support"), EnvironmentCatalog::developmentSupport(), 3, this);
    ides_->setObjectName(QStringLiteral("developmentToolsIde"));
    versionControl_->setObjectName(QStringLiteral("developmentToolsVersionControl"));
    support_->setObjectName(QStringLiteral("developmentToolsSupport"));
    layout->addWidget(ides_); layout->addWidget(versionControl_); layout->addWidget(support_); layout->addStretch();
    connect(ides_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.ides=value; model_->setDevelopmentCapabilities(c); });
    connect(versionControl_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.versionControlSystems=value; model_->setDevelopmentCapabilities(c); });
    connect(support_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.developmentTools=value; model_->setDevelopmentCapabilities(c); });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectDevelopmentToolsPage::refresh);
    const bool all = section_ == Section::All;
    ides_->setVisible(all || section_ == Section::Ide);
    versionControl_->setVisible(all || section_ == Section::VersionControl);
    support_->setVisible(all || section_ == Section::Support);
    refresh();
}

void ProjectDevelopmentToolsPage::refresh() { const auto c=model_->developmentCapabilities(); ides_->setSelectedIds(c.ides); versionControl_->setSelectedIds(c.versionControlSystems); support_->setSelectedIds(c.developmentTools); }
