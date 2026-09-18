// FoundationServices.h
// Unified Foundation query and validation services for F1-F4.
// Each foundation is a thin coordination layer over existing ARAMF core services.
// Bootstrap order: F1 loads → F2 validates trust → F3 validates integrity → F4 reconstructs lifecycle.
// No circular authority chains. No upward dependencies on Process layer.

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>

class ProjectModel;
class QTextStream;

// ─── CLI Entry Point ─────────────────────────────────────────────────────────
int runFoundationCommand(const QStringList& arguments, QTextStream& output, QTextStream& error);

#include "MemoryEvidenceFoundation.h"


// ─── F2: Identity, Provenance & Trust Foundation ────────────────────────────
// F2 ESTABLISHES WHO/WHAT PRODUCED EVIDENCE AND WHETHER ATTRIBUTION IS TRUSTWORTHY.
// Wraps provenance validation, actor identity taxonomy, and trust boundary enforcement.

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
    static F2TrustReport validate(const QString& projectRoot, QString* error = nullptr);

    // Returns the canonical actor taxonomy.
    static QStringList actorTaxonomy();

    // Checks whether an actor slug is in the canonical taxonomy.
    static bool isValidActor(const QString& actor);

    // Validates a single provenance object against F2 rules.
    static bool validateProvenance(const QJsonObject& provenance, QString* error = nullptr);

    // Verifies whether an instruction meets administrative override identity criteria.
    static bool isVerifiedAdministrativeOverride(const QString& instruction);

    // Checks whether text contains prohibited destructive shell command patterns.
    static bool containsDestructivePattern(const QString& text);

    // Checks whether an administrative action respects trust boundaries.
    static bool respectsTrustBoundary(const QString& instruction,
                                      const QString& requestedAction,
                                      QString* error = nullptr);

    // Returns the foundation contract describing F2 responsibilities.
    static QJsonObject contract();
};

// ─── F3: Scope, State & Integrity Foundation ────────────────────────────────
// F3 ESTABLISHES WHETHER STATE/SCOPE RELATIONSHIPS ARE LEGAL AND INTEGRAL.
// Wraps scope taxonomy, scope combination validation, cross-scope file checks,
// and project boundary isolation. NO upward dependency on Process-layer routing.

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
    static F3IntegrityReport validate(const QString& projectRoot,
                                      const ProjectModel* model = nullptr,
                                      QString* error = nullptr);

    // Returns the canonical base reserved scope taxonomy.
    static QStringList canonicalScopes();

    // Returns the base reserved scope set.
    static QSet<QString> baseReservedScopes();

    // Returns the effective canonical scope registry (base reserved scopes + dynamic scopes from scope-routes.json).
    static QSet<QString> effectiveCanonicalScopeRegistry(const QString& projectRoot);

    // Validates project boundary isolation (projectId binding, project root containment, foreign path rejection).
    static bool validateProjectIsolation(const QString& projectRoot, QString* error = nullptr);

    // Validates whether a set of scopes is legal.
    static bool validateScopeSet(const QStringList& scopes, QString* error = nullptr);

    // Validates cross-scope file relationships.
    static bool validateScopeFiles(const QString& scope,
                                   const QStringList& files,
                                   QString* error = nullptr);

    // Returns the foundation contract describing F3 responsibilities.
    static QJsonObject contract();
};

// ─── F4: Lifecycle & Certification Foundation ───────────────────────────────
// F4 ESTABLISHES LIFECYCLE AND CERTIFICATION MEANING.
// Wraps ProcessVersion lifecycle, ProcessVersionState, certification semantics,
// and P6 gating.

struct F4LifecycleReport {
    bool valid = false;
    bool processHistoryValid = false;
    bool activeStateValid = false;
    bool nextStateValid = false;
    bool foundationQueueValid = false;
    bool p6GatingCorrect = false;
    bool certificationSemanticsValid = false;
    bool namespaceVersionCorrect = false;
    int completedProcesses = 0;
    int completedFoundations = 0;
    QStringList errors;
    QJsonObject fullReport;
};

class LifecycleCertificationFoundation final
{
public:
    // Validates the complete lifecycle state: process history, active state,
    // next state, foundation queue, P6 gating, and certification semantics.
    static F4LifecycleReport validate(const QString& projectRoot, QString* error = nullptr);

    // Returns the current lifecycle state summary.
    static QJsonObject lifecycleSummary(const QString& projectRoot, QString* error = nullptr);

    // Validates certification semantics: done requires cert=1, done requires iteration>=1;
    // cert=1 with done=0 is valid for an active iteration.
    static bool validateCertificationSemantics(const QJsonObject& state, QString* error = nullptr);

    // Returns the foundation contract describing F4 responsibilities.
    static QJsonObject contract();
};

// ─── Cross-Foundation Integration ───────────────────────────────────────────
// Coordinates all four foundations in bootstrap order without circular authority.

struct FoundationIntegrationReport {
    bool valid = false;
    F1EvidenceReport f1;
    F2TrustReport f2;
    F3IntegrityReport f3;
    F4LifecycleReport f4;
    bool bootstrapOrderValid = false;
    bool noCircularAuthority = false;
    QString acceptanceType; // "DIAGNOSTIC_RESULT" or "AUTHORITATIVE_ACCEPTANCE"
    bool diagnosticOnly = true;
    QString overallFingerprint;
    QStringList errors;
    QJsonObject fullReport;
};

class FoundationIntegrationService final
{
public:
    // Runs all four foundation validations in bootstrap order (F1→F2→F3→F4).
    // Enforces that later foundations depend on earlier ones but not vice versa.
    static FoundationIntegrationReport validate(const QString& projectRoot,
                                                 const ProjectModel* model = nullptr,
                                                 QString* error = nullptr);

    // Writes canonical foundation-integration.json evidence artifact.
    static bool writeIntegrationEvidence(const QString& projectRoot,
                                         const FoundationIntegrationReport& report,
                                         QString* error = nullptr);

    // Reads canonical foundation-integration.json evidence artifact.
    static QJsonObject readIntegrationEvidence(const QString& projectRoot,
                                               QString* error = nullptr);

    // Returns all four foundation contracts.
    static QJsonObject allContracts();

    // Checks whether the bootstrap order invariant holds.
    static bool verifyBootstrapOrder(QString* error = nullptr);

    // Returns machine-testable P <-> F dependency matrix.
    static QJsonObject dependencyMatrix();
};
