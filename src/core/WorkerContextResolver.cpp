#include "WorkerContextResolver.h"

#include "AramfPaths.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QDir>
#include <algorithm>

namespace {
QJsonObject readObject(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Missing or unreadable routing file: %1").arg(path);
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("Invalid routing JSON: %1").arg(path);
        return {};
    }
    return document.object();
}

QStringList uniqueSorted(QStringList values)
{
    values.removeDuplicates();
    std::sort(values.begin(), values.end());
    return values;
}

QJsonArray array(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}
}

QJsonObject WorkerContextResolver::resolve(const QString& workerRoot, QStringList scopes)
{
    scopes = uniqueSorted(scopes);
    QJsonObject result{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("scopes"), array(scopes)},
                       {QStringLiteral("historyRequired"), false}};
    QString error;
    const QString routePath = QDir(workerRoot).filePath(QStringLiteral("routing/scope-routes.json"));
    const auto routes = readObject(routePath, &error);
    const bool legacyRoutes = routes.value(QStringLiteral("schemaVersion")).isUndefined()
        && routes.value(QStringLiteral("scopes")).isArray()
        && !routes.value(QStringLiteral("scopes")).toArray().isEmpty()
        && routes.value(QStringLiteral("scopes")).toArray().first().isString();
    if (!error.isEmpty() || (routes.value(QStringLiteral("schemaVersion")).toInt(-1) != 1 && !legacyRoutes)) {
        result.insert(QStringLiteral("valid"), false);
        result.insert(QStringLiteral("diagnostics"), QJsonArray{error.isEmpty() ? QStringLiteral("Unsupported scope-route schema.") : error});
        return result;
    }

    QHash<QString, QJsonObject> byScope;
    for (const auto& value : routes.value(QStringLiteral("scopes")).toArray()) {
        if (value.isString()) {
            const QString id = value.toString();
            byScope.insert(id, QJsonObject{{QStringLiteral("id"), id},
                {QStringLiteral("required"), QJsonArray{AramfPaths::ProjectConfiguration, AramfPaths::CurrentState}},
                {QStringLiteral("optional"), QJsonArray{AramfPaths::ResourceManifest}}});
            continue;
        }
        const auto route = value.toObject();
        const QString id = route.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) byScope.insert(id, route);
    }

    QStringList mandatory{AramfPaths::ProjectConfiguration, AramfPaths::WorkerManifest,
                          AramfPaths::CurrentState, AramfPaths::ColdStartValidation};
    QStringList optional;
    QStringList instructions;
    QStringList diagnostics;
    bool history = false;
    for (const auto& scope : scopes) {
        // History is an explicit audit context, not a normal project scope.
        // Keeping it out of the generated route table makes ordinary cold
        // starts provably independent from append-only event history.
        if (scope == QStringLiteral("history")) {
            history = true;
            continue;
        }
        if (!byScope.contains(scope)) {
            diagnostics.append(QStringLiteral("No route defined for scope: %1").arg(scope));
            continue;
        }
        const auto route = byScope.value(scope);
        for (const auto& value : route.value(QStringLiteral("required")).toArray()) mandatory.append(value.toString());
        for (const auto& value : route.value(QStringLiteral("optional")).toArray()) optional.append(value.toString());
        for (const auto& value : route.value(QStringLiteral("instructions")).toArray()) instructions.append(value.toString());
        history = history || route.value(QStringLiteral("historyRequired")).toBool();
    }
    mandatory = uniqueSorted(mandatory);
    optional = uniqueSorted(optional);
    optional.removeAll(QString());
    for (const auto& path : mandatory) optional.removeAll(path);
    instructions = uniqueSorted(instructions);

    QString resourceError;
    const auto resources = readObject(QDir(workerRoot).filePath(QStringLiteral("resources/resources.json")), &resourceError);
    QJsonArray selectedResources;
    if (!resourceError.isEmpty()) {
        diagnostics.append(resourceError);
    } else {
        for (const auto& value : resources.value(QStringLiteral("resources")).toArray()) {
            const auto resource = value.toObject();
            if (!resource.value(QStringLiteral("enabled")).toBool()) continue;
            QStringList resourceScopes;
            for (const auto& scope : resource.value(QStringLiteral("scopes")).toArray()) resourceScopes.append(scope.toString());
            bool relevant = resourceScopes.isEmpty() && scopes.isEmpty();
            for (const auto& scope : scopes) if (resourceScopes.contains(scope)) relevant = true;
            if (relevant) selectedResources.append(QJsonObject{{QStringLiteral("id"), resource.value(QStringLiteral("id"))},
                {QStringLiteral("role"), resource.value(QStringLiteral("role"))}, {QStringLiteral("authority"), resource.value(QStringLiteral("authority"))},
                {QStringLiteral("location"), resource.value(QStringLiteral("location"))}, {QStringLiteral("scopes"), resource.value(QStringLiteral("scopes"))}});
        }
    }
    result.insert(QStringLiteral("valid"), diagnostics.isEmpty());
    result.insert(QStringLiteral("legacyRouteFormat"), legacyRoutes);
    result.insert(QStringLiteral("mandatoryFiles"), array(mandatory));
    result.insert(QStringLiteral("optionalFiles"), array(optional));
    result.insert(QStringLiteral("instructions"), array(instructions));
    result.insert(QStringLiteral("resources"), selectedResources);
    result.insert(QStringLiteral("validation"), QJsonArray{AramfPaths::ColdStartValidation, AramfPaths::ValidationPolicy});
    result.insert(QStringLiteral("historyRequired"), history);
    result.insert(QStringLiteral("historyFiles"), history ? QJsonArray{AramfPaths::EventLog} : QJsonArray{});
    result.insert(QStringLiteral("diagnostics"), array(diagnostics));
    return result;
}
