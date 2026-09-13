#include "ReleaseOverviewPage.h"

ReleaseOverviewPage::ReleaseOverviewPage(QWidget* parent)
    : ParentOverviewPage(
          tr("Version & release management"),
          tr("Keep product version, component revisions, compatibility, readiness and explicit approval history separate."),
          {
              {QStringLiteral("release.product-version"), QStringLiteral("A"), tr("Product version"),
               tr("See the current ARAMF product version and optionally choose a target release for this project." )},
              {QStringLiteral("release.component-versions"), QStringLiteral("B"), tr("Page & component versions"),
               tr("Review stable four-part component identities, revisions and approval status." )},
              {QStringLiteral("release.schema-compatibility"), QStringLiteral("C"), tr("Schema & compatibility"),
               tr("Inspect project and Worker schema compatibility without confusing schema with release approval." )},
              {QStringLiteral("release.readiness"), QStringLiteral("D"), tr("Release readiness"),
               tr("Calculate whether an optional target is ready for explicit release approval." )},
              {QStringLiteral("release.approval-history"), QStringLiteral("E"), tr("Approval & release history"),
               tr("Review the immutable record of authorized component approvals and their context." )}
          }, parent)
{
}

