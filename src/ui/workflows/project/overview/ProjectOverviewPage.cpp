#include "ProjectOverviewPage.h"

ProjectOverviewPage::ProjectOverviewPage(QWidget* parent)
    : ParentOverviewPage(
          tr("What is the project?"),
          tr("Define the identity and technical foundation of this project."),
          {{QStringLiteral("project.file-worker"), QStringLiteral("1.1"),
            tr("Project file, path & Worker"),
            tr("Choose where the project is stored and identify the ARAMF Worker that governs it.")},
           {QStringLiteral("project.modules-templates"), QStringLiteral("1.2"),
            tr("Project modules & templates"),
            tr("Select the capabilities required by the project or start from a prepared project template.")}},
          parent)
{
}
