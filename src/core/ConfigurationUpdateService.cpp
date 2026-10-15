#include "ConfigurationUpdateService.h"

ConfigurationUpdateService::ConfigurationUpdateService(QObject* parent) : QObject(parent) {}

ConfigurationUpdateResult ConfigurationUpdateService::validate(const ProjectModel& model) const
{
    return analyze(model, true);
}
