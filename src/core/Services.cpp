#include "Services.h"

TemplateManager::TemplateManager(QObject* parent) : QObject(parent) {}

QStringList TemplateManager::builtInTemplates() const {
    return {QStringLiteral("pico-2w-visual-designer"), QStringLiteral("qt-desktop-application"), QStringLiteral("cpp-command-line"), QStringLiteral("cmake-library"), QStringLiteral("raspberry-pi-pico-firmware"), QStringLiteral("react-frontend"), QStringLiteral("python-backend"), QStringLiteral("csharp-backend"), QStringLiteral("mobile-application"), QStringLiteral("full-stack-web-application"), QStringLiteral("bachelor-thesis")};
}

bool TemplateManager::applyTemplate(ProjectModel* model, const QString& id) const {
    if (!model || !builtInTemplates().contains(id)) return false;
    model->beginUpdate();
    model->setTemplateId(id);
    if (id == QStringLiteral("pico-2w-visual-designer")) model->setContext(QStringLiteral("embedded-firmware"));
    model->endUpdate();
    return true;
}

QString GenerationServices::generate(const ProjectModel& model) const {
    return QStringLiteral("Generation plan prepared for %1 in canonical aramf/ output.\nTemplate: %2\nProject ID: %3")
        .arg(model.projectName(), model.templateId().isEmpty() ? QStringLiteral("(none)") : model.templateId(), model.projectId());
}
