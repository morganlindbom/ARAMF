#pragma once

#include <QList>
#include <QPair>
#include <QString>

using EnvironmentOption = QPair<QString, QString>;

namespace EnvironmentCatalog {

QList<EnvironmentOption> ides();
QList<EnvironmentOption> toolchains();
QList<EnvironmentOption> operatingSystems();
QList<EnvironmentOption> targets();
QList<EnvironmentOption> architectures();
QList<EnvironmentOption> buildSystems();
QList<EnvironmentOption> languages();
QList<EnvironmentOption> frameworks();
QList<EnvironmentOption> hardwareTargets();
QList<EnvironmentOption> buildConfigurations();
QList<EnvironmentOption> toolingCapabilities();
QList<EnvironmentOption> versionControlSystems();
QList<EnvironmentOption> developmentSupport();
QList<EnvironmentOption> processorFamilies();
QList<EnvironmentOption> dependencyManagers();
QList<EnvironmentOption> testingCapabilities();
QList<EnvironmentOption> qualityCapabilities();
QList<EnvironmentOption> automationCapabilities();
QList<EnvironmentOption> deliveryCapabilities();
QList<EnvironmentOption> academicModes();
QList<EnvironmentOption> thesisLevels();
QList<EnvironmentOption> thesisApproaches();
QList<EnvironmentOption> researchMethods();
QList<EnvironmentOption> citationStyles();
QList<EnvironmentOption> academicLanguages();
QList<EnvironmentOption> academicRequirements();
QList<EnvironmentOption> academicDeliverables();
QList<EnvironmentOption> resourceTypes();
QList<EnvironmentOption> authorityLevels();
QList<EnvironmentOption> resourceScopes();
QList<EnvironmentOption> resourcePolicyOptions();
QList<EnvironmentOption> resourceLoadingStrategies();

}
