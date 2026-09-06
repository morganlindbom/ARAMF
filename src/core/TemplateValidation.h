#pragma once

#include "EnvironmentCatalog.h"
#include <QJsonObject>
#include <QMap>

class ProjectModel;
struct TemplateDefinition;

// Catalog-backed configuration review, shared by templates, Review and Generate.
namespace TemplateValidation {
QMap<QString, QList<EnvironmentOption>> catalogs();
QString catalogFingerprint();
QStringList validateConfiguration(const QJsonObject& configuration);
QStringList validateDefinition(const TemplateDefinition& definition);
QStringList readiness(const ProjectModel& model);
QStringList changedDomains(const ProjectModel& model);
QJsonObject optionAudit(const TemplateDefinition& definition);
}
