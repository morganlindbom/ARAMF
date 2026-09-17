#include "UpdateConfigurationPage.h"
#include "core/Services.h"
#include "core/ConfigurationUpdateService.h"
#include "core/TemplateValidation.h"
#include "core/ProjectModel.h"
#include "ui/workflows/project/setup/ProjectSetupPage.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QJsonValue>

UpdateConfigurationPage::UpdateConfigurationPage(ProjectModel* model, ProjectSetupPage* setup,
                                                 GenerationServices* services, QWidget* parent)
    : QWidget(parent), model_(model), setup_(setup), services_(services)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Update ARAMF</h2>Validate changed settings, then update the complete ARAMF configuration. Existing files are preserved; this operation never deletes."), this));
    auto* validateButton = new QPushButton(tr("Validate update"), this);
    validateButton->setObjectName(QStringLiteral("validateAramfUpdate"));
    updateButton_ = new QPushButton(tr("Update entire ARAMF"), this);
    updateButton_->setObjectName(QStringLiteral("updateEntireAramf"));
    updateButton_->setEnabled(false);
    layout->addWidget(validateButton);
    layout->addWidget(updateButton_);
    result_ = new QPlainTextEdit(this);
    result_->setObjectName(QStringLiteral("aramfUpdateResult"));
    result_->setReadOnly(true);
    layout->addWidget(result_);
    connect(validateButton, &QPushButton::clicked, this, &UpdateConfigurationPage::validate);
    connect(updateButton_, &QPushButton::clicked, this, [this] {
        QString error;
        if (!setup_->saveForGeneration(&error)) { result_->setPlainText(tr("Update: NOT RUN\nSave failed: %1").arg(error)); return; }
        const auto update = ConfigurationUpdateService().apply(*model_, validatedFingerprint_);
        result_->setPlainText(update.success
            ? tr("Validation: PASS\nUpdate: PASS\n\nUpdated complete ARAMF configuration.\nValidation and verification passed.\nGenerated baseline updated.\nExisting files were preserved; no files were deleted.")
            : tr("Validation: PASS\nUpdate: FAIL\n\n%1").arg(update.error));
        updateButton_->setEnabled(false);
    });
    connect(model_, &ProjectModel::modelChanged, this, [this] { updateButton_->setEnabled(false); validatedFingerprint_.clear(); });
}

void UpdateConfigurationPage::validate()
{
    const auto validation = ConfigurationUpdateService().validate(*model_);
    if (!validation.success) {
        result_->setPlainText(tr("Validation: FAIL\n\n%1").arg(validation.error));
        updateButton_->setEnabled(false);
        return;
    }
    const auto plan = validation.plan;
    validatedFingerprint_ = plan.value(QStringLiteral("validationFingerprint")).toString();
    const auto added = plan.value(QStringLiteral("added")).toArray();
    const auto modified = plan.value(QStringLiteral("modified")).toArray();
    const auto removed = plan.value(QStringLiteral("removalBlocked")).toArray();
    QString text = tr("UPDATE VALIDATION\n\nAdded: %1\nModified: %2\nRemoval detected - currently blocked: %3\nUnchanged: %4\n\nStatus: %5")
        .arg(added.size()).arg(modified.size()).arg(removed.size()).arg(plan.value(QStringLiteral("unchanged")).toArray().size())
        .arg(removed.isEmpty() ? (validation.noChange ? tr("NO UPDATE REQUIRED") : tr("VALID FOR ADD/MODIFY UPDATE")) : tr("REMOVAL BLOCKED"));
    const auto describe = [](const QJsonValue& value) { return value.isObject() ? value.toObject().value(QStringLiteral("path")).toString() : value.toString(); };
    for (const auto& value : added) text += tr("\nADD: %1").arg(describe(value));
    for (const auto& value : modified) text += tr("\nMODIFY: %1").arg(describe(value));
    for (const auto& value : removed) text += tr("\nREMOVE blocked: %1").arg(describe(value));
    result_->setPlainText(text);
    updateButton_->setEnabled(!validation.removalBlocked && !validation.noChange);
}
