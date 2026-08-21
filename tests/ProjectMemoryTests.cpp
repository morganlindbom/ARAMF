// ProjectMemoryTests.cpp

#include "core/ProjectMemory.h"
#include "core/ProjectModel.h"
#include "core/Services.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <iostream>

namespace {
bool require(bool condition, const char* message)
{
    /**Report one test assertion without an external test framework.

    Returning false keeps the test binary dependency-free beyond Qt Core and makes CTest integration straightforward.
    */
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}
}

int main(int argc, char** argv)
{
    /**Exercise the native C++ project-memory lifecycle.

    The test proves initialization, uppercase ARAMF layout, root agent bootstrap, and consistency validation without Python.
    */
    QCoreApplication app(argc, argv);
    QTemporaryDir temporaryProject;
    if (!require(temporaryProject.isValid(), "temporary project directory must be valid")) {
        return 1;
    }

    ProjectModel model;
    model.setProjectName(QStringLiteral("Memory Test Project"));
    model.setProjectPath(temporaryProject.path());

    ProjectMemory memory;
    QString error;
    if (!require(memory.initialize(temporaryProject.path(), &model, &error), qPrintable(error))) {
        return 1;
    }

    const QDir root(temporaryProject.path());
    bool ok = true;
    ok &= require(root.exists(QStringLiteral("ARAMF/AGENTS.md")), "canonical ARAMF/AGENTS.md must exist");
    ok &= require(root.exists(QStringLiteral("ARAMF/PROJECT_STATUS.md")), "ARAMF/PROJECT_STATUS.md must exist");
    ok &= require(root.exists(QStringLiteral("ARAMF/memory/decisions.md")), "durable decisions must live under ARAMF/memory");
    ok &= require(root.exists(QStringLiteral("AGENTS.md")), "root agent bootstrap must exist");
    ok &= require(!root.exists(QStringLiteral("aramf.py")), "no Python backend file may be generated");

    TemplateManager templates;
    ok &= require(templates.builtInTemplates().first() == QStringLiteral("pico-2w-visual-designer"), "Pico visual designer must remain the first template");
    ok &= require(templates.applyTemplate(&model, QStringLiteral("qt-desktop-application")), "Qt template must apply");
    ok &= require(model.developmentEnvironment().framework == QStringLiteral("qt6"), "template must derive framework");
    auto overridden = model.developmentEnvironment(); overridden.language = QStringLiteral("python"); model.setDevelopmentEnvironment(overridden);
    ok &= require(templates.applyTemplate(&model, QStringLiteral("pico-2w-visual-designer")), "Pico template must apply");
    ok &= require(model.developmentEnvironment().language == QStringLiteral("python"), "compatible user override must survive template change");
    ok &= require(model.developmentEnvironment().framework == QStringLiteral("pico-sdk"), "non-overridden framework must follow template");

    const QJsonObject report = memory.validate(temporaryProject.path(), &error);
    ok &= require(report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"), "memory validation must pass");
    return ok ? 0 : 1;
}
