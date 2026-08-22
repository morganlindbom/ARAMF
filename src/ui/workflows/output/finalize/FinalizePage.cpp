// FinalizePage.cpp

#include "FinalizePage.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

FinalizePage::FinalizePage(ProjectModel*, QWidget* parent)
    : QWidget(parent),
      status_(new QLabel(this))
{
    /**Construct the final workflow page.

    Final actions remain independently selectable so deployment, verification, checkpoints, and certification can report their own outcomes.
    */
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Finalize</h2>Choose independent final actions; each can be run and reported separately."), this));

    auto* actions = new QGroupBox(tr("Final actions"), this);
    auto* actionLayout = new QVBoxLayout(actions);
    for (const auto& text : {
             tr("Deploy minimal root AGENTS.md bootstrap"),
             tr("Save custom template"),
             tr("Run external build verification"),
             tr("Run external test verification"),
             tr("Run launch verification"),
             tr("Create checkpoint"),
             tr("Record certification evidence")}) {
        actionLayout->addWidget(new QCheckBox(text, actions));
    }
    layout->addWidget(actions);

    layout->addWidget(new QLabel(
        tr("Build: Not Run\nTests: Not Run\nLaunch: Not Run\n\nCanonical agent state lives in ARAMF/. Foreign root AGENTS.md files must be preserved."),
        this));
    layout->addWidget(status_);
    status_->setText(tr("Ready for explicit final actions."));
    layout->addStretch();
}
