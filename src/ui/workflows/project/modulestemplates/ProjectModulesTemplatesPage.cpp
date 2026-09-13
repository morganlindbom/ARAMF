#include "ProjectModulesTemplatesPage.h"

#include "ui/workflows/project/template/TemplateSelector.h"

#include <QLabel>
#include <QVBoxLayout>

ProjectModulesTemplatesPage::ProjectModulesTemplatesPage(
    ProjectModel* model, TemplateManager* manager, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(
        tr("<h2>Project modules &amp; templates</h2>Select the capabilities and reusable templates for this project."), this);
    heading->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(new TemplateSelector(model, manager, this), 1);
}
