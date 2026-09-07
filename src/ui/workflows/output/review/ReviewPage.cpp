#include "ReviewPage.h"

#include "core/AiCatalog.h"
#include "core/EnvironmentCatalog.h"
#include "core/RuleCatalog.h"
#include "core/TemplateValidation.h"
#include "core/ProjectPersistence.h"
#include "core/AramfPaths.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QFileInfo>

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

QString fieldLabel(QString key)
{
    key.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
    if (!key.isEmpty()) key[0] = key[0].toUpper();
    return key;
}

QString configurationSummary(const QJsonObject& object, const QString& prefix = {}, int depth = 0)
{
    QString text;
    const auto catalogs = TemplateValidation::catalogs();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.key() == "environment" || it.key() == "profileSelections" || it.key() == "options") continue;
        if (it.key() == "androidConstraints" && it.value().toObject().value("sourceOfTruthResource").toString().isEmpty()) continue;
        // Once a generic link is present, the legacy flat dataFormat field is
        // retained for migration but is no longer a current review setting.
        if (prefix == QStringLiteral("communication.") && it.key() == QStringLiteral("dataFormat")
            && !object.value(QStringLiteral("links")).toArray().isEmpty()) continue;
        const QString path = prefix + it.key();
        const QString indent(depth * 2, ' ');
        if (it.value().isObject()) {
            text += indent + fieldLabel(it.key()) + "\n" + configurationSummary(it.value().toObject(), path + '.', depth + 1);
        } else if (it.value().isArray()) {
            QStringList values;
            QString objectText;
            int objectCount = 0;
            for (const auto& item : it.value().toArray()) {
                if (item.isObject()) {
                    ++objectCount;
                    objectText += indent + QStringLiteral("  • ") + configurationSummary(item.toObject(), path + '.', depth + 2);
                }
                else values << item.toString();
            }
            if (objectCount > 0)
                text += indent + fieldLabel(it.key()) + ": " + QString::number(objectCount) + QObject::tr(" configured") + "\n" + objectText;
            else
                text += indent + fieldLabel(it.key()) + ": " + displayList(values, catalogs.value(path)) + "\n";
        } else {
            QString value = it.value().isBool() ? (it.value().toBool() ? QObject::tr("Enabled") : QObject::tr("Disabled"))
                : it.value().isDouble() ? QString::number(it.value().toDouble(), 'g', 12)
                : displayList({it.value().toString()}, catalogs.value(path));
            if (value.isEmpty()) value = QObject::tr("Not specified");
            text += indent + fieldLabel(it.key()) + ": " + value + "\n";
        }
    }
    return text;
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
        if (resource.authorityLevel == QStringLiteral("primary-source-of-truth")) ++primarySources;
    }

    QString text;
    QStringList activeModules;
    for (const auto& id : model_->templateModules()) activeModules << id;
    QStringList activeTemplates;
    const auto templateState = model_->templateState();
    for (const auto& value : templateState.value(QStringLiteral("activeTemplateNames")).toArray())
        activeTemplates << value.toString();
    if (activeTemplates.isEmpty()) {
        for (const auto& value : templateState.value(QStringLiteral("activeTemplates")).toArray())
            activeTemplates << value.toString();
    }
    text += tr("Project\n");
    const QString workerName = AramfPaths::workerDirectoryName(model_->workerNameSuffix());
    const QString projectFile = model_->projectFilePath().isEmpty()
        ? workerName + QStringLiteral(".aramf.json") : QFileInfo(model_->projectFilePath()).fileName();
    text += tr("  Name: %1\n  Worker name: %2\n  Project file: %3\n  ID: %4\n  Path: %5\n  Active modules: %6\n  Active templates: %7\n  Academic: %8\n\n")
                .arg(model_->projectName(), workerName, projectFile, model_->projectId(),
                     model_->projectPath().isEmpty() ? tr("Project Path is not configured.") : model_->projectPath(),
                     listOrNone(activeModules), listOrNone(activeTemplates),
                     model_->academicConfiguration().academicMode);
    text += tr("Project Configuration\n");
    text += tr("  Languages: %1\n  Frameworks / SDKs: %2\n  Development tools: %3\n  Platforms: %4\n  Hardware / Architecture: %5\n  Build / Testing / Delivery: %6\n\n")
                .arg(displayList(capabilities.languages, EnvironmentCatalog::languages()), displayList(capabilities.frameworks, EnvironmentCatalog::frameworks()),
                     listOrNone(capabilities.developmentTools), listOrNone(capabilities.targetPlatforms),
                     listOrNone(capabilities.hardwareTargets),
                     listOrNone(capabilities.buildSystems + capabilities.testingCapabilities + capabilities.deliveryCapabilities));
    const auto communication = model_->communicationConfiguration();
    const auto hardwareResources = model_->hardwareResources();
    if (communication.enabled) {
        const auto endpointName = [](const CommunicationEndpoint& endpoint) {
            return endpoint.displayName.isEmpty() ? endpoint.targetId : endpoint.displayName;
        };
        const auto endpointA = communication.endpoints.size() > 0 ? endpointName(communication.endpoints.at(0)) : communication.sourceTarget;
        const auto endpointB = communication.endpoints.size() > 1 ? endpointName(communication.endpoints.at(1)) : communication.destinationTarget;
        const auto roleA = communication.endpoints.size() > 0 ? communication.endpoints.at(0).role : communication.sourceRole;
        const auto roleB = communication.endpoints.size() > 1 ? communication.endpoints.at(1).role : communication.destinationRole;
        const auto link = communication.links.isEmpty() ? CommunicationLink{} : communication.links.first();
        const auto direction = link.direction.isEmpty() ? QStringLiteral("bidirectional") : link.direction;
        const auto transport = link.transport.isEmpty() ? communication.transport : link.transport;
        const auto protocol = link.protocol.isEmpty() ? communication.protocol : link.protocol;
        text += tr("Communication Link\n  Endpoint A: %1\n  Role: %2\n  Endpoint A address: %3\n  Endpoint B: %4\n  Role: %5\n  Endpoint B address: %6\n  Direction: %7\n  Transport: %8\n  Protocol: %9\n  Frame: %10\n  Logical model: %11\n  Wire encoding: %12\n  Byte order: %13\n  Version: %14\n  Authentication: %15\n  Encryption: %16\n  Integration requirements: %17\n\n")
            .arg(endpointA, roleA,
                 communication.endpoints.size() > 0 && !communication.endpoints.at(0).address.isEmpty() ? communication.endpoints.at(0).address : tr("Not configured"),
                 endpointB, roleB,
                 communication.endpoints.size() > 1 && !communication.endpoints.at(1).address.isEmpty() ? communication.endpoints.at(1).address : tr("Not configured"),
                 direction,
                 transport.isEmpty() ? tr("Not selected") : transport,
                 protocol.isEmpty() ? tr("Not selected") : protocol,
                 link.frameType.isEmpty() ? tr("Not specified") : link.frameType,
                 link.logicalDataModel.isEmpty() ? tr("Not specified") : link.logicalDataModel,
                 link.wireEncoding.isEmpty() ? tr("Not specified") : link.wireEncoding,
                 link.byteOrder.isEmpty() ? tr("Not specified") : link.byteOrder,
                 link.protocolVersion.isEmpty() ? communication.protocolVersion : link.protocolVersion,
                 communication.authenticationRequired ? tr("required") : tr("not required"),
                 communication.encryptionRequired ? tr("required") : tr("not required"),
                 listOrNone(communication.integrationRequirements));
    }
    QStringList hardwareSummary;
    for (const auto& resource : hardwareResources)
        hardwareSummary << (resource.purpose.isEmpty() ? resource.id : resource.purpose);
    const QString hardwareText = hardwareResources.isEmpty()
        ? tr("None configured")
        : QString::number(hardwareResources.size()) + tr(" configured (" ) + hardwareSummary.join(QStringLiteral(", ")) + QLatin1Char(')');
    text += tr("Hardware resources: %1\n\n").arg(hardwareText);
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
    const auto issues = TemplateValidation::readiness(*model_);
    text += tr("Review Status: %1\n").arg(issues.isEmpty() ? tr("READY - review is optional") : issues.join('\n'));
    if (!model_->templateState().isEmpty()) {
        text += tr("\nTemplate defaults: %1\nProject type: %2 (template constraint)\nChanged from template: %3\n")
            .arg(model_->templateState().value("name").toString(), model_->context(), listOrNone(TemplateValidation::changedDomains(*model_)));
    }
    text += tr("\nComplete effective configuration\n") + configurationSummary(ProjectPersistence().configuration(*model_));
    summary_->setPlainText(text);
}
