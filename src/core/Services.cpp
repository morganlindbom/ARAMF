// Services.cpp

#include "Services.h"

#include "ProjectMemory.h"

#include <QDir>

TemplateManager::TemplateManager(QObject* parent)
    : QObject(parent)
{
    /**Construct the template catalog service.

    Templates describe target projects; they do not change the fact that ARAMF itself is implemented entirely in C++.
    */
}

QStringList TemplateManager::builtInTemplates() const
{
    /**Return the protected built-in template order.

    Pico 2 W Visual Designer remains template number one while cross-language target templates stay available.
    */
    return {
        QStringLiteral("pico-2w-visual-designer"),
        QStringLiteral("qt-desktop-application"),
        QStringLiteral("cpp-command-line"),
        QStringLiteral("cmake-library"),
        QStringLiteral("raspberry-pi-pico-firmware"),
        QStringLiteral("react-frontend"),
        QStringLiteral("python-backend"),
        QStringLiteral("csharp-backend"),
        QStringLiteral("mobile-application"),
        QStringLiteral("full-stack-web-application"),
        QStringLiteral("bachelor-thesis")
    };
}

QList<TemplateDefinition> TemplateManager::definitions() const
{
    auto make = [](const QString& id, const QString& name, const QString& type, const QString& language, const QString& framework, const QString& compiler, const QString& target, const QString& build, const QStringList& capabilities, const QStringList& resources) {
        TemplateDefinition d; d.id=id; d.displayName=name; d.projectType=type; d.environment.language=language; d.environment.framework=framework; d.environment.ide=QStringLiteral("vscode"); d.environment.compiler=compiler; d.environment.operatingSystem=QStringLiteral("windows"); d.environment.targetPlatform=target; d.environment.buildSystem=build; d.environment.packageManager=QStringLiteral("none"); d.environment.versionControl=QStringLiteral("git"); d.supportedCapabilities=capabilities; d.recommendedResources=resources; d.recommendedAiConfiguration={QStringLiteral("codex"), QStringLiteral("planning"), QStringLiteral("coding"), QStringLiteral("review"), QStringLiteral("testing"), QStringLiteral("documentation")}; d.recommendedRules={QStringLiteral("Universal safety"), QStringLiteral("Project architecture")}; return d;
    };
    return {
        make("pico-2w-visual-designer", "Pico 2 W Visual Designer", "embedded-firmware", "cpp", "pico-sdk", "msys2-ucrt64-gcc", "embedded", "cmake", {"Networking", "Testing", "Documentation"}, {"Pico 2 W datasheet", "Pico SDK documentation"}),
        make("qt-desktop-application", "Qt Desktop Application", "desktop-application", "cpp", "qt6", "msys2-ucrt64-gcc", "desktop", "cmake", {"SQLite", "Networking", "Testing", "Documentation"}, {"Qt documentation", "Architecture document"}),
        make("cpp-command-line", "C++ Command Line", "software-development", "cpp", "none", "msys2-ucrt64-gcc", "desktop", "cmake", {"Testing", "Documentation"}, {"Specification"}),
        make("cmake-library", "CMake Library", "reusable-library", "cpp", "none", "msys2-ucrt64-gcc", "desktop", "cmake", {"Testing", "Documentation"}, {"API specification"}),
        make("raspberry-pi-pico-firmware", "Raspberry Pi Pico Firmware", "embedded-firmware", "cpp", "pico-sdk", "msys2-ucrt64-gcc", "embedded", "cmake", {"Testing", "Documentation"}, {"Pico SDK documentation"}),
        make("react-frontend", "React Frontend", "web-application", "typescript", "react", "node", "web", "npm", {"Testing", "Documentation"}, {"Frontend specification"}),
        make("python-backend", "Python Backend", "backend-service", "python", "fastapi", "python", "server", "pyproject", {"SQLite", "Testing", "Documentation"}, {"API specification"}),
        make("csharp-backend", "C# Backend", "backend-service", "csharp", "aspnet", "dotnet", "server", "cmake", {"Testing", "Documentation"}, {"API specification"}),
        make("mobile-application", "Mobile Application", "software-development", "cpp", "qt6", "msvc", "desktop", "cmake", {"Testing", "Documentation"}, {"Mobile specification"}),
        make("full-stack-web-application", "Full Stack Web Application", "web-application", "typescript", "react", "node", "web", "npm", {"SQLite", "Networking", "Testing", "Documentation"}, {"Architecture document", "API specification"}),
        make("bachelor-thesis", "Bachelor Thesis", "thesis", "cpp", "none", "msys2-ucrt64-gcc", "desktop", "cmake", {"Documentation"}, {"Thesis specification", "Reference implementations"})
    };
}

TemplateDefinition TemplateManager::definition(const QString& id) const { for (const auto& definition : definitions()) if (definition.id == id) return definition; return {}; }

bool TemplateManager::applyTemplate(ProjectModel* model, const QString& id) const
{
    /**Apply one known built-in template to the project model.

    Template application changes project configuration only and is grouped as one model update.
    */
    if (!model || !builtInTemplates().contains(id)) {
        return false;
    }

    model->beginUpdate();
    model->setTemplateId(id);
    const auto selected = definition(id);
    model->setContext(selected.projectType);
    model->applyTemplateDefaults(selected.environment);
    model->endUpdate();
    return true;
}

GenerationServices::GenerationServices(QObject* parent)
    : QObject(parent)
{
    /**Construct the project generation service.

    Generation is implemented through the native C++ ProjectMemory service with no Python or Node runtime dependency.
    */
}

QString GenerationServices::generate(const ProjectModel& model) const
{
    /**Generate the canonical ARAMF project control plane on disk.

    A valid project path produces ARAMF/, a minimal root AGENTS.md bootstrap, memory state, routing manifests, and validation artifacts.
    */
    const QString projectRoot = QDir::cleanPath(model.projectPath().trimmed());
    if (projectRoot.isEmpty() || projectRoot == QStringLiteral(".")) {
        return QStringLiteral("Generation stopped: choose a project path first.");
    }

    ProjectMemory memory;
    QString error;
    if (!memory.initialize(projectRoot, &model, &error)) {
        return QStringLiteral("Generation failed: %1").arg(error.isEmpty() ? QStringLiteral("unknown error") : error);
    }

    return QStringLiteral(
               "ARAMF control plane generated successfully.\n\n"
               "Project: %1\n"
               "Project ID: %2\n"
               "Template: %3\n"
               "Control directory: %4\n"
               "Agent entry point: %5\n"
               "Memory validation: PASS\n\n"
               "The generated project requires no Python or Node runtime for ARAMF.")
        .arg(model.projectName(),
             model.projectId(),
             model.templateId().isEmpty() ? QStringLiteral("(none)") : model.templateId(),
             QDir(projectRoot).filePath(QStringLiteral("ARAMF")),
             QDir(projectRoot).filePath(QStringLiteral("AGENTS.md")));
}
