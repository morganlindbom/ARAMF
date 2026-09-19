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

// ─── Foundation Certification Service ───────────────────────────────────────
// Owns evidence-bound foundation certification and completion.
// Backed by CertificationService, MemoryEvidenceFoundation, and ProcessVersionLifecycle.

struct F1CommandSpec {
    QString name;
    QString identity;
    QString executable;
    QStringList argumentTemplates;
};

struct F1VerificationCheck {
    QString name;
    QString command;
    QString commandIdentity;
    QString status = QStringLiteral("FAIL");
    int exitCode = -1;
    QString timestamp;
    QString sourceRevision;
    QString evidenceReference;
    QString evidenceFingerprint;
    QString foundationVersion;
    int iteration = 0;
    QString evidenceNamespace;

    bool isPass(const QString& expectedRevision = QString()) const;
    // Full evidence chain validation: also verifies evidenceReference is non-empty,
    // referenced file exists and is readable within permitted scope,
    // and SHA-256(file bytes) matches evidenceFingerprint.
    bool isPassWithEvidence(const QString& projectRoot,
                            const QString& expectedRevision = QString(),
                            QString* error = nullptr) const;
    QJsonObject toJson() const;
    static F1VerificationCheck fromJson(const QJsonObject& json);
};

struct F1CertificationEvidence {
    QString foundation = QStringLiteral("F1");
    QString foundationName = QStringLiteral("Memory & Evidence Foundation");
    QString foundationVersion = QStringLiteral("F1.1.4");
    int iteration = 0;
    QString evidenceNamespace;
    QString sourceRevision;
    QString verificationLevel = QStringLiteral("HOST_TEST");

    // Machine-verifiable structured check records
    QList<F1VerificationCheck> checks;

    // Derived verification results
    bool f1FocusedPass = false;
    bool foundationNamespacePass = false;
    bool processMigrationPass = false;
    bool p1GovernancePass = false;
    bool p2ContextPass = false;
    bool p3ExecutionPass = false;
    bool p4PredictivePass = false;
    bool p5RoutingPass = false;
    bool provenanceAndScopePass = false;
    bool f1PhysicalValidationPass = false;
    bool memoryColdStartPass = false;
    bool memoryConsistencyPass = false;
    bool fullCTestPass = false;

    QString evidenceFingerprint;
    QString timestamp;
    QJsonArray knownLimitations;
    QJsonObject rawDetails;

    void updateDerivedFlags();
    bool isComplete(const QString& expectedRevision = QString(), QString* error = nullptr) const;
    // Full evidence chain validation including physical evidence file verification,
    // duplicate check rejection, and path traversal protection.
    bool isCompleteWithEvidence(const QString& projectRoot,
                                const QString& expectedRevision = QString(),
                                QString* error = nullptr) const;
    QJsonObject toJson() const;
    static F1CertificationEvidence fromJson(const QJsonObject& json);
    static QStringList requiredCheckNames();
};

class FoundationCertificationService final
{
public:
    // Ensures certification directories and contract exist under ARAMF_WORKER/certification
    static bool ensureCertificationArea(const QString& projectRoot, QString* error = nullptr);

    // Verifies that sourceRevision is a real Git commit, matches clean HEAD, and working tree is clean
    static bool verifyGitSourceRevision(const QString& projectRoot,
                                        const QString& sourceRevision,
                                        QString* error = nullptr);

    // Writes the atomic F1 evidence artifact to ARAMF_WORKER/certification/evidence/f1-evidence.json (or versioned iteration path)
    static bool writeEvidenceArtifact(const QString& projectRoot,
                                      const F1CertificationEvidence& evidence,
                                      QString* relativePath = nullptr,
                                      QString* sha256 = nullptr,
                                      QString* error = nullptr);

    static bool writeEvidenceArtifact(const QString& projectRoot,
                                      const F1CertificationEvidence& evidence,
                                      const QString& customRelativePath,
                                      QString* relativePath = nullptr,
                                      QString* sha256 = nullptr,
                                      QString* error = nullptr);

    // Reads and validates an existing evidence artifact from disk
    static bool readEvidenceArtifact(const QString& artifactAbsolutePath,
                                     F1CertificationEvidence* evidence = nullptr,
                                     QString* error = nullptr);

    // Executes the 13 verification checks and collects real machine-verifiable results
    static bool executeF1VerificationSuite(const QString& projectRoot,
                                          const QString& sourceRevision,
                                          F1CertificationEvidence* evidence,
                                          QString* error = nullptr);

    // Executes an individual check by name and returns its record
    static F1VerificationCheck executeCheck(const QString& projectRoot,
                                           const QString& checkName,
                                           const QString& sourceRevision,
                                           QString* error = nullptr);

    // Returns the single canonical semantic command specification for a required check.
    static bool canonicalCommandSpec(const QString& checkName,
                                     F1CommandSpec* spec,
                                     QString* error = nullptr);

    // Loads verification checks from a directory of JSON check records
    static bool loadVerificationChecks(const QString& checksDirectory,
                                       const QString& expectedRevision,
                                       QList<F1VerificationCheck>* checks,
                                       QString* error = nullptr);

    // Starts F1 if inactive (transitions next F1.1.0.0.0 -> active F1.1.1.0.0)
    static bool startF1(const QString& projectRoot,
                        const QString& projectFilePath,
                        QString* error = nullptr);

    // Reopens completed F1 for rework (transitions completed F1.1.1.1.1 -> active F1.1.2.0.0)
    static bool reworkF1(const QString& projectRoot,
                         const QString& projectFilePath,
                         QString* error = nullptr);

    // Performs evidence-bound certification of F1
    static bool certifyF1(const QString& projectRoot,
                          const QString& projectFilePath,
                          const QString& sourceRevision,
                          const QString& evidenceArtifactPath = QString(),
                          QJsonObject* issuedCertificate = nullptr,
                          QString* error = nullptr);

    // Completes active certified F1 (transitions active F1.1.x.1.0 -> completed F1.1.x.1.1)
    static bool completeF1(const QString& projectRoot,
                           const QString& projectFilePath,
                           QString* error = nullptr);

    // Synchronizes ARAMF_WORKER/project.json with the canonical producer
    static bool synchronizeProjectJson(const QString& projectRoot,
                                       const ProjectModel& model,
                                       QString* error = nullptr);
};
