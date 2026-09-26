// FoundationServices.h
// External query, certification, and integration coordination for independent F1-F4.
// Foundation implementations live under foundations/F1 through foundations/F4.
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


#include "../foundations/F2/IdentityTrustFoundation.h"
#include "../foundations/F3/ScopeIntegrityFoundation.h"
#include "../foundations/F4/LifecycleCertificationFoundation.h"
#include "FoundationProjectValidation.h"

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
    // The historical order field describes governance evaluation, not runtime dependency.
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
