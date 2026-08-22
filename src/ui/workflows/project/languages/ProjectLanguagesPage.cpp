#include "ProjectLanguagesPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectLanguagesPage::ProjectLanguagesPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Which languages are used?</h2>Select every programming or markup language used by the project."), this));
    languages_ = new CapabilityCheckGroup(tr("Languages"), EnvironmentCatalog::languages(), 3, this);
    layout->addWidget(languages_);
    layout->addStretch();
    connect(languages_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) {
        auto capabilities = model_->developmentCapabilities();
        capabilities.languages = value;
        model_->setDevelopmentCapabilities(capabilities);
    });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectLanguagesPage::refresh);
    refresh();
}

void ProjectLanguagesPage::refresh() { languages_->setSelectedIds(model_->developmentCapabilities().languages); }
