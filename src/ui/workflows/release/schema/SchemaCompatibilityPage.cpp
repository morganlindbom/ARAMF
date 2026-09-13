#include "SchemaCompatibilityPage.h"

#include "core/ComponentVersion.h"
#include "core/ProjectModel.h"
#include "core/ProjectSchema.h"

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

SchemaCompatibilityPage::SchemaCompatibilityPage(ProjectModel* model, ReleaseManagementService* service,
                                                 QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Schema & compatibility</h2>Schema versions remain integer migration discriminators and are not component release versions."), this));
    auto* form = new QFormLayout;
    auto* projectSchema = new QLabel(QString::number(model ? model->projectSchemaVersion() : 0), this);
    projectSchema->setObjectName(QStringLiteral("projectSchemaVersion"));
    auto* workerSchema = new QLabel(tr("1 (current Worker schema)"), this);
    workerSchema->setObjectName(QStringLiteral("workerSchemaVersion"));
    auto* migration = new QLabel(model ? model->migrationStatus() : tr("Unknown"), this);
    migration->setObjectName(QStringLiteral("migrationCompatibility"));
    auto* appCompatibility = new QLabel(tr("Application product %1; component metadata is separate.")
                                            .arg(service ? service->productVersion().toString() : QStringLiteral("unknown")), this);
    appCompatibility->setWordWrap(true);
    form->addRow(tr("Project schema:"), projectSchema);
    form->addRow(tr("Worker schema:"), workerSchema);
    form->addRow(tr("Migration compatibility:"), migration);
    form->addRow(tr("Application compatibility:"), appCompatibility);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("Git, Qt, CMake and external SDK/dependency versions remain their own version domains."), this));
    layout->addStretch();
}

