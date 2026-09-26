#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
struct F3IntegrityReport {
    bool valid = false;
    bool scopeTaxonomyValid = false;
    bool scopeCombinationsLegal = false;
    bool crossScopeFilesValid = false;
    bool projectStateIntegral = false;
    bool projectIsolationValid = false;
    int canonicalScopeCount = 0;
    int dynamicScopeCount = 0;
    QStringList errors;
    QJsonObject fullReport;
};

class ScopeIntegrityFoundation final
{
public:
    // Validates scope taxonomy, scope combinations, cross-scope file patterns,
    // project state integrity, and project boundary isolation.
    static F3IntegrityReport validate(const QString& projectRoot, const QJsonObject& project,
                                      const QJsonArray& records, const QSet<QString>& scopes,
                                      QString* error = nullptr);

    // Returns the canonical base reserved scope taxonomy.
    static QStringList canonicalScopes();

    // Returns the base reserved scope set.
    static QSet<QString> baseReservedScopes();

    // Returns the effective canonical scope registry (base reserved scopes + dynamic scopes from scope-routes.json).
    static QSet<QString> effectiveCanonicalScopeRegistry(const QString& projectRoot);

    // Validates project boundary isolation (projectId binding, project root containment, foreign path rejection).
    static bool validateProjectIsolation(const QString& projectRoot, const QJsonObject& project,
                                         const QJsonArray& records, QString* error = nullptr);

    // Validates whether a set of scopes is legal.
    static bool validateScopeSet(const QStringList& scopes, QString* error = nullptr);

    // Validates cross-scope file relationships.
    static bool validateScopeFiles(const QString& scope,
                                   const QStringList& files,
                                   QString* error = nullptr);

    static bool validateScopeFiles(const QStringList& scopes, const QStringList& files, QString* error = nullptr);

    // Returns the foundation contract describing F3 responsibilities.
    static QJsonObject contract();
};
