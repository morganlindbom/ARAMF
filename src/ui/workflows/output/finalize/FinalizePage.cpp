#include "FinalizePage.h"

#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

FinalizePage::FinalizePage(ProjectModel* model, FinalizationServices* services,
                           AgentEntryPointService* entryPointServices, QWidget* parent)
    : QWidget(parent), model_(model), services_(services), entryPointServices_(entryPointServices),
      status_(new QLabel(this)), blockers_(new QLabel(this)), entryPointResult_(new QLabel(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Finalize</h2>Complete the ARAMF lifecycle after reviewing, generating and verifying the project control plane."), this));
    status_->setWordWrap(true);
    blockers_->setWordWrap(true);
    layout->addWidget(status_);
    layout->addWidget(blockers_);
    finalizeButton_ = new QPushButton(tr("Finalize ARAMF project"), this);
    layout->addWidget(finalizeButton_);
    auto* entryPointGroup = new QGroupBox(tr("AI Agent Entry Points"), this);
    auto* entryPointLayout = new QVBoxLayout(entryPointGroup);
    entryPointLayout->addWidget(new QLabel(
        tr("Create bootstrap files for the selected AI agents so they all enter the project through ARAMF."),
        entryPointGroup));
    entryPointButton_ = new QPushButton(tr("Create AI Agent Entry Points"), entryPointGroup);
    entryPointLayout->addWidget(entryPointButton_);
    entryPointResult_->setWordWrap(true);
    entryPointResult_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    entryPointLayout->addWidget(entryPointResult_);
    layout->addWidget(entryPointGroup);
    layout->addStretch();
    connect(finalizeButton_, &QPushButton::clicked, this, [this] {
        const auto result = services_->finalize(*model_, model_->generationOptions());
        if (result.success) {
            status_->setText(result.alreadyFinalized ? tr("FINALIZED — no changes since finalization.") : tr("FINALIZED"));
            blockers_->clear();
            finalizeButton_->setEnabled(false);
        } else {
            status_->setText(tr("Finalization blocked."));
            blockers_->setText(result.blockers.join(QStringLiteral("\n")));
        }
    });
    connect(entryPointButton_, &QPushButton::clicked, this, [this] {
        const auto result = entryPointServices_->createEntryPoints(*model_);
        QStringList lines;
        auto addFiles = [&lines](const QString& label, const QStringList& files) {
            if (!files.isEmpty()) lines << label + QStringLiteral(": ") + files.join(QStringLiteral(", "));
        };
        addFiles(tr("Created"), result.createdFiles);
        addFiles(tr("Updated"), result.updatedFiles);
        addFiles(tr("Unchanged"), result.unchangedFiles);
        addFiles(tr("Generic agents"), result.genericAgents);
        addFiles(tr("Conflicts"), result.conflicts);
        addFiles(tr("Errors"), result.errors);
        entryPointResult_->setText(lines.isEmpty() ? tr("No entry-point changes were required.") : lines.join(QStringLiteral("\n")));
    });
    connect(model_, &ProjectModel::modelChanged, this, &FinalizePage::refreshReadiness);
    refreshReadiness();
}

void FinalizePage::refreshReadiness()
{
    const bool pathReady = !model_->projectPath().trimmed().isEmpty();
    status_->setText(pathReady ? tr("Final status: run Verify, then finalize.") : tr("Final status: BLOCKED"));
    blockers_->setText(pathReady ? tr("Verification must pass for the current configuration before finalization.")
                                  : tr("Project Path is not configured."));
    finalizeButton_->setEnabled(pathReady);
}
