#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

// Lifecycle process kind: standard sequential process (P) or foundational layer (F).
enum class ProcessKind {
    Process,
    Foundation
};

// Versioned lifecycle process namespace:
// LegacyV1: P0..P12 (P0=Governance, P1=Context, ..., P5=Code Bank, etc.)
// CanonicalV2: F1 Foundation, P1..P14 (P1=Governance, ..., P5=Routing, P6=Code Bank, ..., P14=Renewal)
enum class ProcessNamespace {
    LegacyV1 = 1,
    CanonicalV2 = 2
};

// Process versions describe ARAMF's governed work lifecycle. They are
// intentionally separate from product, component, release, schema and
// migration versions.
struct ProcessVersion final
{
    ProcessKind kind = ProcessKind::Process;
    int number = 0;        // Canonical process number (1..14) or Foundation number (1)
    int process = 0;       // Backward-compatible alias/sync for number when kind == Process
    int loop = 1;
    int iteration = 0;
    int certification = 0;
    int done = 0;

    ProcessVersion() = default;
    ProcessVersion(int p, int l, int i, int c, int d);
    ProcessVersion(ProcessKind k, int num, int l, int i, int c, int d);

    bool isFoundation() const { return kind == ProcessKind::Foundation; }
    bool isProcess() const { return kind == ProcessKind::Process; }
    int foundationNumber() const { return isFoundation() ? number : 0; }
    int processNumber() const { return isProcess() ? number : 0; }

    QString prefix() const { return kind == ProcessKind::Foundation ? QStringLiteral("F") : QStringLiteral("P"); }
    QString identifier() const;
    QString canonicalName() const;

    bool isValid(QString* error = nullptr) const;
    static bool parse(const QString& value, ProcessVersion* result, QString* error = nullptr);

    // Migration & Mapping methods between Legacy V1 and Canonical V2
    static bool mapLegacyToCanonical(int legacyProcess, int* canonicalProcess, QString* error = nullptr);
    static bool mapCanonicalToLegacy(int canonicalProcess, int* legacyProcess, QString* error = nullptr);
    static bool isLegacyProcess(int processNumber);
    static bool isCanonicalProcess(int processNumber);
    static bool isCanonicalFoundation(int foundationNumber);
    static QString processName(int processNumber, ProcessNamespace ns = ProcessNamespace::CanonicalV2);
    static QString foundationName(int foundationNumber);

    ProcessVersion toCanonical(ProcessNamespace sourceNs = ProcessNamespace::LegacyV1) const;
    ProcessVersion toLegacy() const;

    bool operator==(const ProcessVersion& other) const;
    bool operator!=(const ProcessVersion& other) const { return !(*this == other); }
};

// Canonical namespace resolution service for legacy vs canonical process identifiers.
class ProcessNamespaceService final
{
public:
    static QString resolveToCanonical(const QString& identity,
                                      ProcessNamespace sourceNs = ProcessNamespace::LegacyV1,
                                      QString* error = nullptr);
    static QString canonicalToLegacy(const QString& canonicalIdentity, QString* error = nullptr);
    static bool parseIdentity(const QString& text,
                              ProcessVersion* version,
                              ProcessNamespace* detectedNamespace = nullptr,
                              QString* error = nullptr);
    static QString qualifiedIdentity(const ProcessVersion& version, ProcessNamespace ns);
    static QStringList canonicalRoadmapNames();
    static QStringList legacyRoadmapNames();
};

struct ProcessVersionState final
{
    // Namespace schema version: 1 = Legacy V1, 2 = Canonical V2
    int namespaceVersion = static_cast<int>(ProcessNamespace::CanonicalV2);

    // Closed history is append-only through ProcessVersionLifecycle. The
    // active, next, and future fields preserve state without collapsing history into a
    // single mutable string.
    QList<ProcessVersion> completedHistory;
    QStringList legacyHistory;
    bool hasActiveProcess = false;
    ProcessVersion activeProcess;
    bool hasNextProcess = false;
    ProcessVersion nextProcess;
    bool hasFutureProcess = false;
    ProcessVersion futureProcess;

    // Foundation progression queue and integrated validation gating
    QList<ProcessVersion> foundationQueue;
    bool foundationIntegrationValid = false;

    bool isValid(QString* error = nullptr) const;
    QStringList completedIdentifiers() const;
    QString activeIdentifier() const;
    QString nextIdentifier() const;
    QString futureIdentifier() const;
    QStringList foundationQueueIdentifiers() const;

    bool isFoundationComplete(int foundationNumber) const;
    bool allFoundationsComplete() const;
    bool isP6Eligible(QString* reason = nullptr) const;
    QList<ProcessVersion> remainingFoundationQueue() const;
    static QList<ProcessVersion> canonicalFoundationQueue();

    static ProcessVersionState empty();
    static ProcessVersionState currentCanonicalState();
};

QJsonObject processVersionToJson(const ProcessVersion& version);
bool processVersionFromJson(const QJsonValue& value,
                            ProcessVersion* result,
                            QString* error = nullptr);

QJsonObject processVersionStateToJson(const ProcessVersionState& state);
bool processVersionStateFromJson(const QJsonValue& value,
                                 ProcessVersionState* state,
                                 QString* error = nullptr);

// Explicit lifecycle authority. No operation is inferred from tests,
// generation, validation, launch or self-host regeneration.
class ProcessVersionLifecycle final
{
public:
    static bool startNextProcess(ProcessVersionState* state, QString* error = nullptr);
    static bool reworkCompletedProcess(ProcessVersionState* state, int process,
                                       QString* error = nullptr);
    static bool advanceIteration(ProcessVersionState* state, QString* error = nullptr);
    static bool certifyCurrentIteration(ProcessVersionState* state, QString* error = nullptr);
    static bool completeActiveProcess(ProcessVersionState* state, QString* error = nullptr);
    static bool resetForFiveStageCampaign(ProcessVersionState* state, QString* error = nullptr);
};
