#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
struct F2TrustReport {
    bool valid = false;
    bool allProvenanceValid = false;
    bool actorTaxonomyConsistent = false;
    bool trustBoundariesEnforced = false;
    bool adminOverrideValid = false;
    int eventsWithProvenance = 0;
    int eventsWithoutProvenance = 0;
    int legacyExemptEvents = 0;
    QStringList recognizedActors;
    QStringList errors;
    QJsonObject fullReport;
};

class IdentityTrustFoundation final
{
public:
    // Validates provenance across all events, actor identity taxonomy,
    // and trust boundary enforcement (admin override safety).
    static F2TrustReport validate(const QJsonArray& records, int legacyCutoff = 0, QString* error = nullptr);

    // Returns the canonical actor taxonomy.
    static QStringList actorTaxonomy();

    // Checks whether an actor slug is in the canonical taxonomy.
    static bool isValidActor(const QString& actor);

    // Validates a single provenance object against F2 rules.
    static bool validateProvenance(const QJsonObject& provenance, QString* error = nullptr);

    // Verifies whether an instruction meets administrative override identity criteria.
    static bool isVerifiedAdministrativeOverride(const QString& instruction);

    // Recognizes an explicit, constrained temporary governance repair directive.
    // This is a distinct intent predicate, not authentication or a file grant.
    // The external coordinator must bind current project, exact scope, origins,
    // lifetime and actual recording provenance before issuing any permission.
    static bool isExplicitTemporaryGovernanceRepairAuthority(const QString& instruction);

    // Checks whether text contains prohibited destructive shell command patterns.
    static bool containsDestructivePattern(const QString& text);

    // Checks whether an administrative action respects trust boundaries.
    static bool respectsTrustBoundary(const QString& instruction,
                                      const QString& requestedAction,
                                      QString* error = nullptr);

    // Returns the foundation contract describing F2 responsibilities.
    static QJsonObject contract();
};
