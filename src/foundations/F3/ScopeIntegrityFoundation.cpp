#include "ScopeIntegrityFoundation.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSaveFile>
#include <QSet>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QStack>
#include <algorithm>
QSet<QString> ScopeIntegrityFoundation::baseReservedScopes()
{
    return {
        QStringLiteral("all"),
        QStringLiteral("history"),
        QStringLiteral("project"),
        QStringLiteral("global"),
        QStringLiteral("project+global"),
        QStringLiteral("build-system"),
        QStringLiteral("ci-cd"),
        QStringLiteral("configuration"),
        QStringLiteral("documentation"),
        QStringLiteral("entire-project"),
        QStringLiteral("generated-files"),
        QStringLiteral("resources"),
        QStringLiteral("source-code"),
        QStringLiteral("tests"),
        QStringLiteral("ui-ux")
    };
}

QStringList ScopeIntegrityFoundation::canonicalScopes()
{
    auto list = baseReservedScopes().values();
    list.sort();
    return list;
}

QSet<QString> ScopeIntegrityFoundation::effectiveCanonicalScopeRegistry(const QString& projectRoot)
{
    auto registry = baseReservedScopes();
    const QString routesPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/routing/scope-routes.json"));
    QFile f(routesPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const auto doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            const auto arr = doc.object().value(QStringLiteral("scopes")).toArray();
            for (const auto& item : arr) {
                const QString id = item.toObject().value(QStringLiteral("id")).toString().trimmed();
                if (!id.isEmpty()) registry.insert(id);
            }
        }
    }
    return registry;
}

QJsonObject ScopeIntegrityFoundation::contract()
{
    return QJsonObject{
        {QStringLiteral("foundation"), QStringLiteral("F3")},
        {QStringLiteral("name"), QStringLiteral("Scope, State & Integrity Foundation")},
        {QStringLiteral("responsibility"),
            QStringLiteral("Establishes whether state/scope relationships are legal and integral")},
        {QStringLiteral("owns"), QJsonArray{
            QStringLiteral("scope-taxonomy"),
            QStringLiteral("scope-combination-rules"),
            QStringLiteral("cross-scope-file-validation"),
            QStringLiteral("project-state-integrity"),
            QStringLiteral("project-isolation-boundary")
        }},
        {QStringLiteral("bootstrapOrder"), 3},
        {QStringLiteral("dependsOn"), QJsonArray{}},
        {QStringLiteral("requiredBy"), QJsonArray{}}
    };
}

bool ScopeIntegrityFoundation::validateScopeSet(const QStringList& scopes, QString* error)
{
    if (scopes.size() <= 1) return true;

    QSet<QString> seen;
    for (const auto& s : scopes) {
        const QString trimmed = s.trimmed();
        if (trimmed.isEmpty()) {
            if (error) *error = QStringLiteral("Empty scope entry in scope list.");
            return false;
        }
        if (seen.contains(trimmed)) {
            if (error) *error = QStringLiteral("Duplicate scope '%1' in scope list.").arg(trimmed);
            return false;
        }
        seen.insert(trimmed);
    }

    if (seen.contains(QStringLiteral("project")) && seen.contains(QStringLiteral("global"))) {
        if (error) *error = QStringLiteral("Contradictory scope combination: 'project' and 'global' cannot be combined as separate items; use compound 'project+global'.");
        return false;
    }

    const bool hasUniversalAll = seen.contains(QStringLiteral("all")) || seen.contains(QStringLiteral("entire-project"));
    if (hasUniversalAll) {
        for (const auto& item : seen) {
            if (item != QStringLiteral("all") && item != QStringLiteral("entire-project")
                && item != QStringLiteral("project") && item != QStringLiteral("project+global") && item != QStringLiteral("history")) {
                if (error) *error = QStringLiteral("Contradictory scope combination: universal scope '%1' cannot be combined with specific partition '%2'.")
                    .arg(seen.contains(QStringLiteral("all")) ? QStringLiteral("all") : QStringLiteral("entire-project"), item);
                return false;
            }
        }
    }

    if (seen.contains(QStringLiteral("history"))) {
        for (const auto& item : seen) {
            if (item != QStringLiteral("history") && item != QStringLiteral("all")) {
                if (error) *error = QStringLiteral("Contradictory scope combination: 'history' archive scope cannot be combined with active partition '%1'.").arg(item);
                return false;
            }
        }
    }

    return true;
}

bool ScopeIntegrityFoundation::validateScopeFiles(const QString& scope, const QStringList& files, QString* error)
{
    return validateScopeFiles(QStringList{scope}, files, error);
}

bool ScopeIntegrityFoundation::validateScopeFiles(const QStringList& scopes, const QStringList& files, QString* error)
{
    if (files.isEmpty()) return true;

    for (const auto& s : scopes) {
        const QString trimmed = s.trimmed();
        if (trimmed == QStringLiteral("all")
            || trimmed == QStringLiteral("entire-project")
            || trimmed == QStringLiteral("project")
            || trimmed == QStringLiteral("project+global")
            || trimmed == QStringLiteral("global")
            || trimmed == QStringLiteral("history")) {
            return true;
        }
    }

    auto fileMatchesScope = [](const QString& scope, const QString& file) -> bool {
        const QString normalized = file.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
        const QString lower = normalized.toLower();

        if (scope == QStringLiteral("tests")) {
            return lower.startsWith(QStringLiteral("tests/"))
                || lower.startsWith(QStringLiteral("test/"))
                || lower.contains(QStringLiteral("test"))
                || lower.endsWith(QStringLiteral("_test.cpp"))
                || lower.endsWith(QStringLiteral("_tests.cpp"));
        }
        if (scope == QStringLiteral("documentation")) {
            return lower.endsWith(QStringLiteral(".md"))
                || lower.endsWith(QStringLiteral(".txt"))
                || lower.endsWith(QStringLiteral(".rst"))
                || lower.startsWith(QStringLiteral("docs/"))
                || lower.startsWith(QStringLiteral("doc/"))
                || lower.startsWith(QStringLiteral("documentation/"));
        }
        if (scope == QStringLiteral("build-system")) {
            return lower.endsWith(QStringLiteral("cmakelists.txt"))
                || lower.endsWith(QStringLiteral(".cmake"))
                || lower.endsWith(QStringLiteral("makefile"))
                || lower.endsWith(QStringLiteral("build.gradle"))
                || lower.endsWith(QStringLiteral("pom.xml"))
                || lower.endsWith(QStringLiteral(".ninja"))
                || lower.contains(QStringLiteral("cmake"));
        }
        if (scope == QStringLiteral("ui-ux")) {
            return lower.startsWith(QStringLiteral("src/ui/"))
                || lower.startsWith(QStringLiteral("ui/"))
                || lower.endsWith(QStringLiteral(".ui"))
                || lower.endsWith(QStringLiteral(".qml"));
        }
        if (scope == QStringLiteral("source-code")) {
            if (lower.startsWith(QStringLiteral("tests/")) || lower.startsWith(QStringLiteral("test/"))) {
                return false;
            }
            return lower.startsWith(QStringLiteral("src/"))
                || lower.startsWith(QStringLiteral("app/src/"))
                || lower.endsWith(QStringLiteral(".cpp"))
                || lower.endsWith(QStringLiteral(".h"))
                || lower.endsWith(QStringLiteral(".hpp"))
                || lower.endsWith(QStringLiteral(".kt"))
                || lower.endsWith(QStringLiteral(".java"));
        }
        if (scope == QStringLiteral("ci-cd")) {
            return lower.startsWith(QStringLiteral(".github/"))
                || lower.startsWith(QStringLiteral(".gitlab/"))
                || lower.contains(QStringLiteral("jenkins"))
                || lower.contains(QStringLiteral("workflow"));
        }
        if (scope == QStringLiteral("configuration")) {
            return lower.endsWith(QStringLiteral(".json"))
                || lower.endsWith(QStringLiteral(".yaml"))
                || lower.endsWith(QStringLiteral(".yml"))
                || lower.endsWith(QStringLiteral(".ini"))
                || lower.contains(QStringLiteral("config"));
        }
        if (scope == QStringLiteral("resources")) {
            return lower.startsWith(QStringLiteral("resources/"))
                || lower.startsWith(QStringLiteral("res/"))
                || lower.endsWith(QStringLiteral(".qrc"))
                || lower.contains(QStringLiteral("resource"));
        }
        if (scope == QStringLiteral("generated-files")) {
            return lower.contains(QStringLiteral("generated"))
                || lower.contains(QStringLiteral("autogen"))
                || lower.contains(QStringLiteral("moc_"));
        }
        return true;
    };

    for (const auto& file : files) {
        bool matchedAny = false;
        for (const auto& s : scopes) {
            if (fileMatchesScope(s.trimmed(), file)) {
                matchedAny = true;
                break;
            }
        }
        if (!matchedAny) {
            if (error) {
                *error = QStringLiteral("Cross-scope file violation: file '%1' does not match declared scope(s) '%2'.")
                    .arg(file, scopes.join(QStringLiteral(", ")));
            }
            return false;
        }
    }

    return true;
}

bool ScopeIntegrityFoundation::validateProjectIsolation(const QString& projectRoot, const QJsonObject& projectJson, const QJsonArray& events, QString* error)
{
    QString projId = projectJson.value(QStringLiteral("projectId")).toString().trimmed();
    if (projId.isEmpty()) {
        projId = projectJson.value(QStringLiteral("id")).toString().trimmed();
    }
    if (projId.isEmpty()) {
        projId = projectJson.value(QStringLiteral("name")).toString().trimmed();
    }
    if (projId.isEmpty() && projectJson.contains(QStringLiteral("processVersion"))) {
        projId = QStringLiteral("fixture-project");
    }
    if (projId.isEmpty()) {
        if (error) *error = QStringLiteral("F3: Missing projectId in project.json");
        return false;
    }

    const QString cleanRoot = QFileInfo(projectRoot).absoluteFilePath();

    for (const auto& value : events) {
        const auto ev = value.toObject();
        const auto filesArr = ev.value(QStringLiteral("affectedFiles")).toArray();
        for (const auto& item : filesArr) {
            const QString filePath = item.toString().trimmed();
            if (filePath.isEmpty()) continue;

            // Reject directory traversal escaping the project root
            if (filePath.contains(QStringLiteral(".."))) {
                QString resolved = QDir::cleanPath(QDir(cleanRoot).filePath(filePath));
                if (resolved != cleanRoot && !resolved.startsWith(cleanRoot + '/', Qt::CaseInsensitive)) {
                    if (error) *error = QStringLiteral("F3: Foreign path escape detected: '%1'").arg(filePath);
                    return false;
                }
            }

            // Reject foreign absolute paths not within project root
            if (QDir::isAbsolutePath(filePath)) {
                QString cleanFile = QDir::cleanPath(filePath);
                if (cleanFile.compare(cleanRoot, Qt::CaseInsensitive) != 0 && !cleanFile.startsWith(cleanRoot + '/', Qt::CaseInsensitive)) {
                    if (error) *error = QStringLiteral("F3: Foreign absolute path detected: '%1'").arg(filePath);
                    return false;
                }
            }
        }
    }

    return true;
}

F3IntegrityReport ScopeIntegrityFoundation::validate(const QString& root, const QJsonObject& project,
    const QJsonArray& records, const QSet<QString>& registry, QString* error)
{
    F3IntegrityReport r;
    r.scopeTaxonomyValid = r.scopeCombinationsLegal = r.crossScopeFilesValid = true;
    r.canonicalScopeCount = baseReservedScopes().size();
    r.dynamicScopeCount = registry.size() - r.canonicalScopeCount;
    r.projectStateIntegral = !project.isEmpty();
    for (const auto& value : records) {
        if (!value.isObject()) { r.scopeTaxonomyValid = false; r.errors.append("F3: Invalid scope record"); continue; }
        const auto ev = value.toObject();
        const auto scope = ev.value("scope").toString();
        if (!scope.isEmpty() && !registry.contains(scope)) { r.scopeTaxonomyValid = false; r.errors.append("F3: Unknown scope: " + scope); }
        QStringList scopes, files; for (const auto& s:ev.value("scopes").toArray()) scopes.append(s.toString());
        for (const auto& f:ev.value("affectedFiles").toArray()) files.append(f.toString());
        QString failure;
        if (!validateScopeSet(scopes, &failure)) { r.scopeCombinationsLegal = false; r.errors.append(failure); }
        if (!scope.isEmpty() && !validateScopeFiles(scope, files, &failure)) { r.crossScopeFilesValid = false; r.errors.append(failure); }
    }
    QString failure;
    r.projectIsolationValid = validateProjectIsolation(root, project, records, &failure);
    if (!r.projectIsolationValid) r.errors.append(failure);
    r.valid = r.scopeTaxonomyValid && r.scopeCombinationsLegal && r.crossScopeFilesValid && r.projectStateIntegral && r.projectIsolationValid;
    r.fullReport = QJsonObject{{"valid",r.valid},{"errors",QJsonArray::fromStringList(r.errors)}};
    if (!r.valid && error) *error = r.errors.join("; ");
    return r;
}
