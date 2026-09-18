// MemoryEvidenceFoundation.h
// Canonical F1: Memory & Evidence Foundation.
// Owns physical and durable evidence persistence, append-only ledger integrity,
// deterministic manifest reconstruction, cold-start reconstruction, and evidence queries.
// Zero semantic dependency on F2 (Identity/Trust), F3 (Scope/Integrity), F4 (Lifecycle), or P1-P14.

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

struct F1EvidenceReport {
    bool valid = false;
    bool ledgerIntact = false;
    bool sequenceMonotonic = false;
    bool manifestConsistent = false;
    bool manifestReconstructed = false;
    bool certificatesIntact = false;
    bool metricsConsistent = false;
    bool checkpointsIntact = false;
    bool coldStartFresh = false;
    bool schemaCompatible = false;
    bool recoveryRequired = false;
    int totalEvents = 0;
    int totalCertificates = 0;
    int totalCheckpoints = 0;
    qint64 latestSequenceNumber = 0;
    QString evidenceFingerprint;
    QString coldStartFingerprint;
    QString recoveryReason;
    QStringList errors;
    QJsonObject fullReport;
};

class MemoryEvidenceFoundation final
{
public:
    // Physical evidence validation: structural JSON/JSONL, event IDs, sequences,
    // manifest consistency, metrics, checkpoints, certificates, cold-start, schema compatibility.
    // Zero semantic dependency on F2, F3, F4, or P1-P14.
    static F1EvidenceReport validate(const QString& projectRoot, QString* error = nullptr);

    // Hardened deterministic recovery of memory-manifest.json from append-only event-log.jsonl.
    // Rejects if ANY line has malformed JSON, duplicate IDs, invalid sequences, or unsupported schemas.
    // Preserves original files on failure.
    static bool reconstructManifestFromLedger(const QString& projectRoot, QString* error = nullptr);

    // Physical state recovery for interrupted updates (Cases A, B, C).
    // Safely recovers manifest, metrics, or derived current-state from valid ledger.
    // Fails closed if authoritative ledger is corrupted.
    static bool recoverPhysicalState(const QString& projectRoot, QString* error = nullptr);

    // Query API for consumers without reading event-log.jsonl directly
    static QList<QJsonObject> evidenceRecords(const QString& projectRoot, QString* error = nullptr);
    static bool evidenceRecordById(const QString& projectRoot, const QString& eventId,
                                   QJsonObject* result = nullptr, QString* error = nullptr);
    static QList<QJsonObject> evidenceRecordsBySequenceRange(const QString& projectRoot,
                                                            qint64 startSeq, qint64 endSeq,
                                                            QString* error = nullptr);
    static QString evidenceFingerprint(const QString& projectRoot, QString* error = nullptr);
    static QJsonObject sourceBindingMetadata(const QString& projectRoot, QString* error = nullptr);
    static QJsonObject reconstructionStatus(const QString& projectRoot, QString* error = nullptr);
    static QJsonObject physicalCorruptionStatus(const QString& projectRoot, QString* error = nullptr);

    // Cold-start & Summary helpers
    static bool canReconstructFromColdStart(const QString& projectRoot, QString* error = nullptr);
    static QJsonObject evidenceSummary(const QString& projectRoot, QString* error = nullptr);

    // Foundation Contract
    static QJsonObject contract();
};
