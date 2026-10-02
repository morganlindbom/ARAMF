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
            tr("Select the capabilities required by the project or start from a prepared project template.")},
           {QStringLiteral("project.communication"), QStringLiteral("1.3"),
            tr("Project communication"),
            tr("Define communication between project targets, including transport, protocol and endpoint roles.")},
           {QStringLiteral("project.compatibility"), QStringLiteral("1.4"),
            tr("Project compatibility & migration"),
            tr("Review schema compatibility, migration notices and canonical project paths.")} },
          parent)
{
}
