#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

struct CertificationSourceManifest final {
    QString subject;
    QString manifestVersion = QStringLiteral("1");
    QStringList files;

    QJsonObject toJson() const;
    QString fingerprint() const;
};

struct CertificationDependencyBinding final {
    QString subject;
    QString requiredLifecycle;
    QString certificateId;
    QString sourceRevision;
    QString contractFingerprint;
    QString evidenceFingerprint;
    QString dependencyManifestFingerprint;

    QJsonObject toJson() const;
};

struct CertificationDependencyManifest final {
    QString subject;
    QString lifecycle;
    QList<CertificationDependencyBinding> directDependencies;
    QString manifestFingerprint;

    QJsonObject toJson(bool includeFingerprint = true) const;
    QString computedFingerprint() const;
};

enum class CertificationFreshnessStatus {
    Fresh,
    SourceStale,
    DependencyStale,
    TransitiveDependencyStale,
    DependencyBindingIncomplete,
    EvidenceStale,
    FreshnessIndeterminate,
    NotCertified,
    NotStarted
};

QString certificationFreshnessStatusName(CertificationFreshnessStatus status);

struct CertificationFreshnessResult final {
    QString subject;
    QString historicalCertificateId;
    QString historicalLifecycle;
    QString certifiedSourceRevision;
    QString currentSourceRevision;
    QString certifiedSourceFingerprint;
    QString currentSourceFingerprint;
    QString certifiedContractFingerprint;
    QString currentContractFingerprint;
    QString certifiedEvidenceFingerprint;
    QString currentEvidenceFingerprint;
    QString dependencyManifestFingerprint;
    bool sourceFresh = false;
    bool sourceBindingComplete = false;
    bool contractFresh = false;
    bool directDependencyFresh = false;
    bool transitiveDependencyFresh = false;
    bool evidenceFresh = false;
    bool bindingComplete = false;
    CertificationFreshnessStatus status = CertificationFreshnessStatus::FreshnessIndeterminate;
    QStringList reasons;
    QList<CertificationFreshnessResult> directDependencyResults;

    QJsonObject toJson() const;
};

class CertificationSourceManifestProvider final {
public:
    static CertificationSourceManifest manifest(const QString& subject);
    static bool fingerprint(const QString& projectRoot, const QString& subject,
                            QString* result, QString* error = nullptr);
};

class CertificationContractManifestProvider final {
public:
    static bool fingerprintFromProjectJson(const QString& subject, const QJsonObject& project,
                                           QString* result, QString* error = nullptr);
    static bool fingerprint(const QString& subject, const QString& projectRoot,
                            QString* result, QString* error = nullptr);
};

class CertificationDependencyBindingProvider final {
public:
    static QJsonObject currentBindings(const QString& subject, const QString& projectRoot,
                                       QString* error = nullptr);
};

class CertificationFreshnessService final {
public:
    static CertificationDependencyManifest dependencyManifest(const QString& subject);

    // This method is pure and is used by certification and freshness tests.
    // It does not read or write repository state.
    static CertificationFreshnessResult evaluateSnapshot(
        const QString& subject,
        const QJsonObject& historical,
        const QString& currentSourceRevision,
        const QString& currentSourceFingerprint,
        const QString& currentContractFingerprint,
        const QString& currentEvidenceFingerprint,
        const QHash<QString, CertificationFreshnessResult>& dependencyResults = {});

    // Reads certificates, evidence, source files, and the current project
    // JSON only. It never generates, certifies, or persists anything.
    static CertificationFreshnessResult evaluate(const QString& subject,
                                                  const QString& projectRoot,
                                                  QString* error = nullptr);
};
