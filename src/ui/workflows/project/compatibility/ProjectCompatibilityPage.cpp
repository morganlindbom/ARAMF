#include "ProjectCompatibilityPage.h"

#include "core/AramfPaths.h"
#include "core/ProjectModel.h"

#include <QDir>
#include <QLabel>
#include <QVBoxLayout>

ProjectCompatibilityPage::ProjectCompatibilityPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(
        tr("<h2>Project compatibility &amp; migration</h2>Review the schema status and canonical paths used by this project."), this);
    heading->setWordWrap(true);
    layout->addWidget(heading);

    schema_ = new QLabel(this);
    status_ = new QLabel(this);
    notices_ = new QLabel(this);
    workerPath_ = new QLabel(this);
    projectFile_ = new QLabel(this);
    for (auto* label : {schema_, status_, notices_, workerPath_, projectFile_}) label->setWordWrap(true);
    schema_->setObjectName(QStringLiteral("projectSchemaStatus"));
    status_->setObjectName(QStringLiteral("projectMigrationStatus"));
    notices_->setObjectName(QStringLiteral("projectMigrationNotices"));
    workerPath_->setObjectName(QStringLiteral("canonicalWorkerPath"));
    projectFile_->setObjectName(QStringLiteral("canonicalProjectFile"));
    layout->addWidget(schema_);
    layout->addWidget(status_);
    layout->addWidget(workerPath_);
    layout->addWidget(projectFile_);
    layout->addWidget(notices_);
    layout->addStretch();

    connect(model_, &ProjectModel::modelChanged, this, &ProjectCompatibilityPage::refreshFromModel);
    refreshFromModel();
}

void ProjectCompatibilityPage::refreshFromModel()
{
    if (!model_) return;
    const int current = model_->projectSchemaVersion();
    const int migratedFrom = model_->migratedFromSchemaVersion();
    schema_->setText(tr("Current project schema: %1").arg(current));
    status_->setText(model_->migrationNotices().isEmpty()
                         ? tr("Compatibility: current; no migration review is required.")
                         : tr("Compatibility: migrated from schema %1 to %2 (%3).")
                               .arg(migratedFrom).arg(current).arg(model_->migrationStatus()));
    const QString workerName = AramfPaths::workerDirectoryName(model_->workerNameSuffix());
    const QString workerPath = model_->projectPath().isEmpty()
        ? workerName
        : QDir(model_->projectPath()).filePath(workerName);
    workerPath_->setText(tr("Canonical Worker path: %1").arg(workerPath));
    projectFile_->setText(tr("Project file: %1")
                              .arg(model_->projectFilePath().isEmpty()
                                       ? tr("not saved yet")
                                       : model_->projectFilePath()));

    QStringList lines;
    for (const auto& value : model_->migrationNotices()) {
        const auto notice = value.toObject();
        lines << tr("• %1: %2 Result: %3")
                     .arg(notice.value(QStringLiteral("legacyPath")).toString(),
                          notice.value(QStringLiteral("reason")).toString(),
                          notice.value(QStringLiteral("resultingState")).toString());
    }
    notices_->setText(lines.isEmpty() ? tr("No migration notices.") : lines.join(QLatin1Char('\n')));
}
