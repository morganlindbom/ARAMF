#include "ReviewPage.h"

#include "core/AiCatalog.h"
#include "core/EnvironmentCatalog.h"
#include "core/RuleCatalog.h"

#include <algorithm>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace {
QString listOrNone(const QStringList& values)
{
    return values.isEmpty() ? QObject::tr("None configured") : values.join(QStringLiteral(", "));
}

QString displayList(const QStringList& values, const QList<EnvironmentOption>& catalog)
{
    QStringList display;
    for (const auto& value : values) {
        auto it = std::find_if(catalog.cbegin(), catalog.cend(), [&](const auto& option) { return option.second == value; });
        display << (it == catalog.cend() ? value : it->first);
    }
    return listOrNone(display);
}

QString displayAiList(const QStringList& values)
{
    const auto catalog = AiCatalog::agents();
    QStringList display;
    for (const auto& value : values) {
        auto it = std::find_if(catalog.cbegin(), catalog.cend(), [&](const auto& option) { return option.id == value; });
        display << (it == catalog.cend() ? value : it->displayName);
    }
    return listOrNone(display);
}
}

ReviewPage::ReviewPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model), summary_(new QPlainTextEdit(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Review</h2>Review the ARAMF configuration and generated output plan before generation."), this));
    summary_->setReadOnly(true);
    layout->addWidget(summary_);
    layout->addStretch();
    connect(model_, &ProjectModel::modelChanged, this, &ReviewPage::refreshFromModel);
    refreshFromModel();
}

void ReviewPage::refreshFromModel()
{
    const auto capabilities = model_->developmentCapabilities();
    const auto ai = model_->aiConfiguration();
    const auto rules = model_->ruleConfiguration();
    const auto memory = model_->memoryConfiguration();
    const auto options = model_->generationOptions();
    const auto resources = model_->resources();
    int enabled = 0;
    int authoritative = 0;
    int primarySources = 0;
    for (const auto& resource : resources) {
        if (resource.enabled) ++enabled;
        if (resource.authorityLevel != QStringLiteral("supporting-reference")) ++authoritative;
        if (resource.authorityLevel == QStringLiteral("primary-source")) ++primarySources;
    }

    QString text;
    text += tr("Project\n");
    text += tr("  Name: %1\n  ID: %2\n  Path: %3\n  Template: %4\n  Academic: %5\n\n")
                .arg(model_->projectName(), model_->projectId(),
                     model_->projectPath().isEmpty() ? tr("Project Path is not configured.") : model_->projectPath(),
                     model_->templateId().isEmpty() ? tr("Disable") : model_->templateId(),
                     model_->academicConfiguration().academicMode);
    text += tr("Project Configuration\n");
    text += tr("  Languages: %1\n  Frameworks / SDKs: %2\n  Development tools: %3\n  Platforms: %4\n  Hardware / Architecture: %5\n  Build / Testing / Delivery: %6\n\n")
                .arg(displayList(capabilities.languages, EnvironmentCatalog::languages()), displayList(capabilities.frameworks, EnvironmentCatalog::frameworks()),
                     listOrNone(capabilities.developmentTools), listOrNone(capabilities.targetPlatforms),
                     listOrNone(capabilities.hardwareTargets),
                     listOrNone(capabilities.buildSystems + capabilities.testingCapabilities + capabilities.deliveryCapabilities));
    text += tr("AI Configuration\n  Primary agent: %1\n  Additional agents: %2\n  Responsibilities: %3\n  Permissions: %4\n  ARAMF integrations: %5\n\n")
                .arg(displayAiList({ai.primaryAgent}), displayAiList(ai.additionalAgents), listOrNone(ai.responsibilities),
                     listOrNone(ai.permissions), listOrNone(ai.aramfIntegrations));
    text += tr("Resources\n  Enabled: %1\n  Authoritative: %2\n  Primary Sources of Truth: %3\n\n")
                .arg(enabled).arg(authoritative).arg(primarySources);
    text += tr("Rules\n  Enforcement: %1\n  Active categories: %2\n  Loading strategy: %3\n  Conflict policy: %4\n\n")
                .arg(rules.enforcementLevel).arg(rules.activeCategories.size())
                .arg(rules.loadingStrategy).arg(rules.conflictPolicy);
    text += tr("Memory\n  Retention: %1\n  Capture groups: %2\n  Maintenance: %3\n  Validation: %4\n  Maximum size: %5 GB\n\n")
                .arg(memory.retentionLevel).arg(memory.captureCategories.size())
                .arg(memory.maintenanceOptions.size()).arg(memory.validationOptions.size())
                .arg(QString::number(static_cast<double>(memory.maximumSizeBytes) / (1024.0 * 1024.0 * 1024.0), 'g', 6));
    text += tr("Generation Plan\n  Agent rules: %1\n  Routing: %2\n  Platform/environment metadata: %3\n  Resource manifest: %4\n  Project Memory: %5\n  Provenance / selection effects: %6\n\n")
                .arg(options.generateAgentRules ? tr("selected") : tr("not selected"),
                     options.generateRouting ? tr("selected") : tr("not selected"),
                     options.generatePlatforms ? tr("selected") : tr("not selected"),
                     options.generateResources ? tr("selected") : tr("not selected"),
                     options.generateMemory ? tr("selected") : tr("not selected"),
                     options.generateProvenance ? tr("selected") : tr("not selected"));
    const bool blocked = model_->projectPath().trimmed().isEmpty();
    text += tr("Review Status: %1\n").arg(blocked ? tr("BLOCKED — Project Path is not configured.") : tr("READY"));
    summary_->setPlainText(text);
}
