// FoundationServices.h
// Unified Foundation query and validation services for F1-F4.
// Each foundation is a thin coordination layer over existing ARAMF core services.
// Bootstrap order: F1 loads → F2 validates trust → F3 validates integrity → F4 reconstructs lifecycle.
// No circular authority chains.

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class ProjectModel;

// ─── F1: Memory & Evidence Foundation ───────────────────────────────────────
// F1 STORES AND RECONSTRUCTS EVIDENCE.
// Wraps ProjectMemory, ProjectMemoryCompaction, and CertificationService
// into a unified evidence persistence and reconstruction contract.

struct F1EvidenceReport {
    bool valid = false;
    bool ledgerIntact = false;
    bool coldStartFresh = false;
    bool sequenceMonotonic = false;
    bool manifestConsistent = false;
    bool certificatesIntact = false;
    int totalEvents = 0;
    int totalCertificates = 0;
    QString coldStartFingerprint;
    QStringList errors;
    QJsonObject fullReport;
};

class MemoryEvidenceFoundation final
{
public:
    // Validates the entire evidence layer: append-only ledger integrity,
    // cold-start reconstruction, manifest consistency, and certification ledger.
    static F1EvidenceReport validate(const QString& projectRoot, QString* error = nullptr);

    // Returns the current evidence summary for cold-start reconstruction.
    static QJsonObject evidenceSummary(const QString& projectRoot, QString* error = nullptr);

    // Checks whether the ledger can be reconstructed from cold-start.
    static bool canReconstructFromColdStart(const QString& projectRoot, QString* error = nullptr);

    // Returns the foundation contract describing F1 responsibilities.
    static QJsonObject contract();
};

// ─── F2: Identity, Provenance & Trust Foundation ────────────────────────────
// F2 ESTABLISHES WHO/WHAT PRODUCED EVIDENCE AND WHETHER ATTRIBUTION IS TRUSTWORTHY.
// Wraps provenance validation, actor identity taxonomy, and trust boundary enforcement.

struct F2TrustReport {
    bool valid = false;
    bool allProvenanceValid = false;
    bool actorTaxonomyConsistent = false;
    bool trustBoundariesEnforced = false;
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

    // Validates a single provenance object against F2 rules.
    static bool validateProvenance(const QJsonObject& provenance, QString* error = nullptr);

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
// and project state integrity.

struct F3IntegrityReport {
    bool valid = false;
    bool scopeTaxonomyValid = false;
    bool scopeCombinationsLegal = false;
    bool crossScopeFilesValid = false;
    bool projectStateIntegral = false;
    bool validationRoutingConsistent = false;
    int canonicalScopeCount = 0;
    int dynamicScopeCount = 0;
    QStringList errors;
    QJsonObject fullReport;
};

class ScopeIntegrityFoundation final
{
public:
    // Validates scope taxonomy, scope combinations, cross-scope file patterns,
    // project state integrity, and validation routing consistency.
    static F3IntegrityReport validate(const QString& projectRoot,
                                      const ProjectModel* model = nullptr,
                                      QString* error = nullptr);

    // Returns the canonical scope taxonomy.
    static QStringList canonicalScopes();

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

    // Validates certification semantics: cert cannot be 1 without done conditions met.
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

    // Returns all four foundation contracts.
    static QJsonObject allContracts();

    // Checks whether the bootstrap order invariant holds.
    static bool verifyBootstrapOrder(QString* error = nullptr);
};
