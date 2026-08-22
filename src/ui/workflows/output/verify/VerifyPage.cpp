#include "VerifyPage.h"

#include "core/ProjectModel.h"
#include "ui/shared/PageSupport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>

VerifyPage::VerifyPage(QWidget* parent)
    : QWidget(parent)
{
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);

    auto* checks = new QVBoxLayout;
    checks->addWidget(AramfUi::check(tr("Build"), tr("Compile the generated project."), content));
    checks->addWidget(AramfUi::check(tr("Tests"), tr("Run the project test suite."), content));
    checks->addWidget(AramfUi::check(tr("Launch"), tr("Start and observe the generated application."), content));
    checks->addWidget(AramfUi::check(tr("Memory consistency"), tr("Validate append-only and derived state."), content));
    checks->addWidget(AramfUi::check(tr("AGENTS deployment safety"), tr("Verify managed/foreign root file behavior."), content));
    layout->addWidget(AramfUi::group(tr("Verification plan"), checks, content));

    auto* evidence = new QFormLayout;
    auto* level = new QComboBox;
    level->addItems({tr("Development"), tr("Release candidate"), tr("Certification baseline")});
    auto* artifact = new QComboBox;
    artifact->addItems({tr("Keep local evidence"), tr("Record project evidence"),
                        tr("Record reusable certification")});
    evidence->addRow(tr("Validation level"), level);
    evidence->addRow(tr("Evidence policy"), artifact);
    layout->addWidget(AramfUi::group(tr("Evidence"), evidence, content));
    layout->addStretch();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(AramfUi::pageShell(
        tr("Verification & Evidence"),
        tr("Separate implementation, verification and reusable certification claims."),
        content));
}

void VerifyPage::setModel(ProjectModel* model)
{
    if (model_ == model) return;
    model_ = model;
    AramfUi::bindCheckboxes(this, model_, QStringLiteral("verification-plan"));
}
