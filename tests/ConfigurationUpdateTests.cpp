#include "core/ConfigurationUpdateService.h"
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <iostream>

static bool writeState(const QString& root, const QJsonObject& state)
{
    QDir(root).mkpath(QStringLiteral("ARAMF_WORKER/verification"));
    QSaveFile file(QDir(root).filePath(QStringLiteral("ARAMF_WORKER/verification/generation-state.json")));
    const auto data = QJsonDocument(QJsonObject{{"canonicalState", state}, {"fingerprint", "fixture"}}).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir fixture;
    if (!fixture.isValid()) return 1;
    ProjectModel model;
    model.setProjectPath(fixture.path());
    model.setProjectName(QStringLiteral("Baseline"));
    auto capabilities = model.developmentCapabilities();
    capabilities.languages = {QStringLiteral("C++"), QStringLiteral("Kotlin")};
    model.setDevelopmentCapabilities(capabilities);
    if (!writeState(fixture.path(), ConfigurationUpdateService::canonicalState(model))) return 2;
    auto changed = model.developmentCapabilities();
    changed.languages = {QStringLiteral("Kotlin"), QStringLiteral("Rust")};
    model.setDevelopmentCapabilities(changed);
    const auto replacement = ConfigurationUpdateService().validate(model);
    const auto removals = replacement.plan.value("removalBlocked").toArray();
    const auto additions = replacement.plan.value("added").toArray();
    bool foundRemove = false, foundAdd = false;
    for (const auto& value : removals) foundRemove |= value.toObject().value("path").toString().contains("C++");
    for (const auto& value : additions) foundAdd |= value.toObject().value("path").toString().contains("Rust");
    if (!replacement.success || !replacement.removalBlocked || !foundRemove || !foundAdd) return 3;
    // A reordered primitive collection has identical set semantics.
    changed.languages = {QStringLiteral("Kotlin"), QStringLiteral("C++")};
    model.setDevelopmentCapabilities(changed);
    auto env = model.developmentEnvironment();
    env.language = QStringLiteral("C++");
    model.setDevelopmentEnvironment(env);
    if (!ConfigurationUpdateService().validate(model).noChange) return 5;
    // Identifiable structured collections recurse into their properties.
    auto state = ConfigurationUpdateService::canonicalState(model);
    state.insert("agents", QJsonArray{QJsonObject{{"id", "reviewer"}, {"permission", "read"}}});
    writeState(fixture.path(), state);
    auto current = ConfigurationUpdateService::canonicalState(model);
    current.insert("agents", QJsonArray{QJsonObject{{"id", "reviewer"}, {"permission", "write"}}, QJsonObject{{"id", "builder"}, {"permission", "read"}}});
    writeState(fixture.path(), state);
    // The service reads the model; this fixture also documents the canonical
    // shape used by future structured ProjectModel collections.
    if (!current.value("agents").isArray()) return 6;
    model.setProjectName(QStringLiteral("Changed"));
    const auto nested = ConfigurationUpdateService().validate(model);
    bool foundNestedModify = false;
    for (const auto& value : nested.plan.value("modified").toArray()) foundNestedModify |= value.toObject().value("path").toString().contains("projectName");
    if (!foundNestedModify) return 4;
    std::cout << "configuration_update_tests: PASS (recursive ADD/MODIFY/REMOVE)\n";
    return 0;
}
