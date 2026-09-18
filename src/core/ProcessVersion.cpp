#include "ProcessVersion.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {
void setError(QString* error, const QString& message)
{
    if (error) *error = message;
}

bool readNonNegativeInt(const QJsonObject& object, const QString& key, int* result, QString* error)
{
    const auto value = object.value(key);
    if (!value.isDouble()) {
        setError(error, QStringLiteral("Process version field '%1' must be an integer.").arg(key));
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0.0 || std::floor(number) != number
        || number > static_cast<double>(std::numeric_limits<int>::max())) {
        setError(error, QStringLiteral("Process version field '%1' is outside the supported integer range.").arg(key));
        return false;
    }
    *result = static_cast<int>(number);
    return true;
}

bool sameProcessLoop(const ProcessVersion& left, const ProcessVersion& right)
{
    return left.kind == right.kind && left.number == right.number && left.loop == right.loop;
}

bool comesAfter(const ProcessVersion& left, const ProcessVersion& right)
{
    if (left.kind != right.kind) {
        return false;
    }
    return left.number > right.number
        || (left.number == right.number && left.loop > right.loop);
}
}

ProcessVersion::ProcessVersion(int p, int l, int i, int c, int d)
    : kind(ProcessKind::Process), number(p), process(p), loop(l), iteration(i), certification(c), done(d)
{
}

ProcessVersion::ProcessVersion(ProcessKind k, int num, int l, int i, int c, int d)
    : kind(k), number(num), process(k == ProcessKind::Process ? num : 0), loop(l), iteration(i), certification(c), done(d)
{
}

QString ProcessVersion::identifier() const
{
    return QStringLiteral("%1%2.%3.%4.%5.%6")
        .arg(prefix())
        .arg(number)
        .arg(loop)
        .arg(iteration)
        .arg(certification)
        .arg(done);
}

bool ProcessVersion::isValid(QString* error) const
{
    if (number < 0) { setError(error, QStringLiteral("Process/foundation number must be non-negative.")); return false; }
    if (kind == ProcessKind::Foundation && number < 1) {
        setError(error, QStringLiteral("Foundation number must be at least 1."));
        return false;
    }
    if (loop < 1) { setError(error, QStringLiteral("Process loop must be at least 1.")); return false; }
    if (iteration < 0) { setError(error, QStringLiteral("Process iteration must be non-negative.")); return false; }
    if (certification != 0 && certification != 1) { setError(error, QStringLiteral("Process certification flag must be 0 or 1.")); return false; }
    if (done != 0 && done != 1) { setError(error, QStringLiteral("Process done flag must be 0 or 1.")); return false; }
    if (done == 1 && certification != 1) { setError(error, QStringLiteral("A process cannot be done before certification.")); return false; }
    return true;
}

bool ProcessVersion::parse(const QString& value, ProcessVersion* result, QString* error)
{
    if (!result) { setError(error, QStringLiteral("Process version result is not available.")); return false; }
    const auto match = QRegularExpression(QStringLiteral("^([PF])([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([01])\\.([01])$")).match(value.trimmed());
    if (!match.hasMatch()) {
        setError(error, QStringLiteral("Invalid process version '%1'. Expected [P|F]<number>.<loop>.<iteration>.<cert>.<done>.").arg(value));
        return false;
    }
    const QString typeChar = match.captured(1);
    bool ok = false;
    ProcessVersion candidate;
    candidate.kind = (typeChar == QStringLiteral("F")) ? ProcessKind::Foundation : ProcessKind::Process;
    candidate.number = match.captured(2).toInt(&ok);
    if (!ok) { setError(error, QStringLiteral("Process/foundation number is too large.")); return false; }
    candidate.process = (candidate.kind == ProcessKind::Process) ? candidate.number : 0;
    candidate.loop = match.captured(3).toInt(&ok);
    if (!ok) { setError(error, QStringLiteral("Process loop is too large.")); return false; }
    candidate.iteration = match.captured(4).toInt(&ok);
    if (!ok) { setError(error, QStringLiteral("Process iteration is too large.")); return false; }
    candidate.certification = match.captured(5).toInt(&ok);
    if (!ok) { setError(error, QStringLiteral("Process certification flag is invalid.")); return false; }
    candidate.done = match.captured(6).toInt(&ok);
    if (!ok) { setError(error, QStringLiteral("Process done flag is invalid.")); return false; }
    if (!candidate.isValid(error)) return false;
    *result = candidate;
    return true;
}

bool ProcessVersion::operator==(const ProcessVersion& other) const
{
    return kind == other.kind && number == other.number && loop == other.loop
        && iteration == other.iteration && certification == other.certification && done == other.done;
}

bool ProcessVersion::mapLegacyToCanonical(int legacyProcess, int* canonicalProcess, QString* error)
{
    if (legacyProcess < 0 || legacyProcess > 12) {
        setError(error, QStringLiteral("Unknown legacy process P%1. Legacy range is P0 through P12.").arg(legacyProcess));
        return false;
    }
    if (canonicalProcess) {
        *canonicalProcess = legacyProcess + 1;
    }
    return true;
}

bool ProcessVersion::mapCanonicalToLegacy(int canonicalProcess, int* legacyProcess, QString* error)
{
    if (canonicalProcess < 1 || canonicalProcess > 14) {
        setError(error, QStringLiteral("Unknown canonical process P%1. Canonical range is P1 through P14.").arg(canonicalProcess));
        return false;
    }
    if (canonicalProcess == 14) {
        setError(error, QStringLiteral("Canonical P14 (Governed Lifecycle Renewal) has no legacy process equivalent."));
        return false;
    }
    if (legacyProcess) {
        *legacyProcess = canonicalProcess - 1;
    }
    return true;
}

bool ProcessVersion::isLegacyProcess(int processNumber)
{
    return processNumber >= 0 && processNumber <= 12;
}

bool ProcessVersion::isCanonicalProcess(int processNumber)
{
    return processNumber >= 1 && processNumber <= 14;
}

bool ProcessVersion::isCanonicalFoundation(int foundationNumber)
{
    return foundationNumber == 1;
}

QString ProcessVersion::foundationName(int foundationNumber)
{
    if (foundationNumber == 1) return QStringLiteral("Memory & Evidence Foundation");
    return QStringLiteral("Unknown Foundation");
}

QString ProcessVersion::processName(int processNumber, ProcessNamespace ns)
{
    if (ns == ProcessNamespace::LegacyV1) {
        switch (processNumber) {
        case 0: return QStringLiteral("Task Execution Governance");
        case 1: return QStringLiteral("Context Coordination");
        case 2: return QStringLiteral("Execution Orchestration");
        case 3: return QStringLiteral("Predictive Task Optimization");
        case 4: return QStringLiteral("Self-Adjusting Routing");
        case 5: return QStringLiteral("Canonical Code Bank");
        case 6: return QStringLiteral("Agent Quality Scoring");
        case 7: return QStringLiteral("Multi-Agent Orchestration");
        case 8: return QStringLiteral("Semantic Conflict Reasoning");
        case 9: return QStringLiteral("Predictive Validation");
        case 10: return QStringLiteral("Controlled Autonomous Improvement");
        case 11: return QStringLiteral("Evaluation & Continuous Improvement Framework");
        case 12: return QStringLiteral("Knowledge Harvest & Core Promotion");
        default: return QStringLiteral("Unknown Legacy Process");
        }
    } else {
        switch (processNumber) {
        case 1: return QStringLiteral("Task Execution Governance");
        case 2: return QStringLiteral("Context Coordination");
        case 3: return QStringLiteral("Execution Orchestration");
        case 4: return QStringLiteral("Predictive Task Optimization");
        case 5: return QStringLiteral("Self-Adjusting Routing");
        case 6: return QStringLiteral("Canonical Code Bank");
        case 7: return QStringLiteral("Agent Quality Scoring");
        case 8: return QStringLiteral("Multi-Agent Orchestration");
        case 9: return QStringLiteral("Semantic Conflict Reasoning");
        case 10: return QStringLiteral("Predictive Validation");
        case 11: return QStringLiteral("Controlled Autonomous Improvement");
        case 12: return QStringLiteral("Evaluation & Continuous Improvement Framework");
        case 13: return QStringLiteral("Knowledge Harvest & Core Promotion");
        case 14: return QStringLiteral("Governed Lifecycle Renewal");
        default: return QStringLiteral("Unknown Canonical Process");
        }
    }
}

QString ProcessVersion::canonicalName() const
{
    if (isFoundation()) return foundationName(number);
    return processName(number, ProcessNamespace::CanonicalV2);
}

ProcessVersion ProcessVersion::toCanonical(ProcessNamespace sourceNs) const
{
    if (isFoundation()) return *this;
    if (sourceNs == ProcessNamespace::CanonicalV2) return *this;
    int canonicalNum = 0;
    if (mapLegacyToCanonical(number, &canonicalNum)) {
        return ProcessVersion(ProcessKind::Process, canonicalNum, loop, iteration, certification, done);
    }
    return *this;
}

ProcessVersion ProcessVersion::toLegacy() const
{
    if (isFoundation()) return *this;
    int legacyNum = 0;
    if (mapCanonicalToLegacy(number, &legacyNum)) {
        return ProcessVersion(ProcessKind::Process, legacyNum, loop, iteration, certification, done);
    }
    return *this;
}

// -------------------------------------------------------------------------
// ProcessNamespaceService
// -------------------------------------------------------------------------
QString ProcessNamespaceService::resolveToCanonical(const QString& identity,
                                                    ProcessNamespace sourceNs,
                                                    QString* error)
{
    QString trimmed = identity.trimmed();
    if (trimmed.startsWith(QStringLiteral("legacy:"))) {
        trimmed = trimmed.mid(7);
        sourceNs = ProcessNamespace::LegacyV1;
    } else if (trimmed.startsWith(QStringLiteral("canonical:"))) {
        trimmed = trimmed.mid(10);
        sourceNs = ProcessNamespace::CanonicalV2;
    }

    ProcessVersion v;
    if (!ProcessVersion::parse(trimmed, &v, error)) {
        return QString();
    }
    if (v.isFoundation()) {
        return v.identifier();
    }
    if (sourceNs == ProcessNamespace::LegacyV1) {
        int canonicalNum = 0;
        if (!ProcessVersion::mapLegacyToCanonical(v.number, &canonicalNum, error)) {
            return QString();
        }
        ProcessVersion canonicalV(ProcessKind::Process, canonicalNum, v.loop, v.iteration, v.certification, v.done);
        return canonicalV.identifier();
    }
    return v.identifier();
}

QString ProcessNamespaceService::canonicalToLegacy(const QString& canonicalIdentity, QString* error)
{
    QString trimmed = canonicalIdentity.trimmed();
    if (trimmed.startsWith(QStringLiteral("canonical:"))) {
        trimmed = trimmed.mid(10);
    }
    ProcessVersion v;
    if (!ProcessVersion::parse(trimmed, &v, error)) {
        return QString();
    }
    if (v.isFoundation()) {
        if (error) *error = QStringLiteral("F1 is a foundation layer with no legacy P equivalent.");
        return QString();
    }
    int legacyNum = 0;
    if (!ProcessVersion::mapCanonicalToLegacy(v.number, &legacyNum, error)) {
        return QString();
    }
    ProcessVersion legacyV(ProcessKind::Process, legacyNum, v.loop, v.iteration, v.certification, v.done);
    return legacyV.identifier();
}

bool ProcessNamespaceService::parseIdentity(const QString& text,
                                            ProcessVersion* version,
                                            ProcessNamespace* detectedNamespace,
                                            QString* error)
{
    QString trimmed = text.trimmed();
    ProcessNamespace ns = ProcessNamespace::CanonicalV2;
    if (trimmed.startsWith(QStringLiteral("legacy:"))) {
        trimmed = trimmed.mid(7);
        ns = ProcessNamespace::LegacyV1;
    } else if (trimmed.startsWith(QStringLiteral("canonical:"))) {
        trimmed = trimmed.mid(10);
        ns = ProcessNamespace::CanonicalV2;
    }
    ProcessVersion v;
    if (!ProcessVersion::parse(trimmed, &v, error)) return false;
    if (version) *version = v;
    if (detectedNamespace) *detectedNamespace = ns;
    return true;
}

QString ProcessNamespaceService::qualifiedIdentity(const ProcessVersion& version, ProcessNamespace ns)
{
    if (ns == ProcessNamespace::LegacyV1) {
        return QStringLiteral("legacy:%1").arg(version.identifier());
    }
    return QStringLiteral("canonical:%1").arg(version.identifier());
}

QStringList ProcessNamespaceService::canonicalRoadmapNames()
{
    return {
        QStringLiteral("F1 — Memory & Evidence Foundation"),
        QStringLiteral("P1 — Task Execution Governance"),
        QStringLiteral("P2 — Context Coordination"),
        QStringLiteral("P3 — Execution Orchestration"),
        QStringLiteral("P4 — Predictive Task Optimization"),
        QStringLiteral("P5 — Self-Adjusting Routing"),
        QStringLiteral("P6 — Canonical Code Bank"),
        QStringLiteral("P7 — Agent Quality Scoring"),
        QStringLiteral("P8 — Multi-Agent Orchestration"),
        QStringLiteral("P9 — Semantic Conflict Reasoning"),
        QStringLiteral("P10 — Predictive Validation"),
        QStringLiteral("P11 — Controlled Autonomous Improvement"),
        QStringLiteral("P12 — Evaluation & Continuous Improvement Framework"),
        QStringLiteral("P13 — Knowledge Harvest & Core Promotion"),
        QStringLiteral("P14 — Governed Lifecycle Renewal")
    };
}

QStringList ProcessNamespaceService::legacyRoadmapNames()
{
    return {
        QStringLiteral("P0 — Task Execution Governance"),
        QStringLiteral("P1 — Context Coordination"),
        QStringLiteral("P2 — Execution Orchestration"),
        QStringLiteral("P3 — Predictive Task Optimization"),
        QStringLiteral("P4 — Self-Adjusting Routing"),
        QStringLiteral("P5 — Canonical Code Bank"),
        QStringLiteral("P6 — Agent Quality Scoring"),
        QStringLiteral("P7 — Multi-Agent Orchestration"),
        QStringLiteral("P8 — Semantic Conflict Reasoning"),
        QStringLiteral("P9 — Predictive Validation"),
        QStringLiteral("P10 — Controlled Autonomous Improvement"),
        QStringLiteral("P11 — Evaluation & Continuous Improvement Framework"),
        QStringLiteral("P12 — Knowledge Harvest & Core Promotion")
    };
}

// -------------------------------------------------------------------------
// JSON Serialization
// -------------------------------------------------------------------------
bool processVersionFromJson(const QJsonValue& value, ProcessVersion* result, QString* error)
{
    if (!value.isObject()) {
        setError(error, QStringLiteral("Process version entry must be an object."));
        return false;
    }
    const auto object = value.toObject();
    ProcessVersion candidate;
    if (object.contains(QStringLiteral("foundation"))) {
        candidate.kind = ProcessKind::Foundation;
        if (!readNonNegativeInt(object, QStringLiteral("foundation"), &candidate.number, error)) return false;
        candidate.process = 0;
    } else if (object.contains(QStringLiteral("process"))) {
        candidate.kind = ProcessKind::Process;
        if (!readNonNegativeInt(object, QStringLiteral("process"), &candidate.number, error)) return false;
        candidate.process = candidate.number;
    } else {
        setError(error, QStringLiteral("Process version must specify either 'process' or 'foundation'."));
        return false;
    }

    if (!readNonNegativeInt(object, QStringLiteral("loop"), &candidate.loop, error)
        || !readNonNegativeInt(object, QStringLiteral("iteration"), &candidate.iteration, error)
        || !readNonNegativeInt(object, QStringLiteral("certification"), &candidate.certification, error)
        || !readNonNegativeInt(object, QStringLiteral("done"), &candidate.done, error)) {
        return false;
    }
    if (!candidate.isValid(error)) return false;
    *result = candidate;
    return true;
}

QJsonObject processVersionToJson(const ProcessVersion& version)
{
    QJsonObject obj{
        {version.kind == ProcessKind::Foundation ? QStringLiteral("foundation") : QStringLiteral("process"), version.number},
        {QStringLiteral("loop"), version.loop},
        {QStringLiteral("iteration"), version.iteration},
        {QStringLiteral("certification"), version.certification},
        {QStringLiteral("done"), version.done}
    };
    return obj;
}

// -------------------------------------------------------------------------
// ProcessVersionState
// -------------------------------------------------------------------------
bool ProcessVersionState::isValid(QString* error) const
{
    ProcessVersion previous;
    bool hasPrevious = false;
    for (qsizetype index = 0; index < completedHistory.size(); ++index) {
        const auto& version = completedHistory.at(index);
        if (!version.isValid(error)) return false;
        if (version.done != 1) {
            setError(error, QStringLiteral("Completed process history must contain only done=1 entries."));
            return false;
        }
        if (index > 0 && std::any_of(completedHistory.cbegin(), completedHistory.cbegin() + index,
                                       [&version](const auto& prior) { return prior == version; })) {
            setError(error, QStringLiteral("Completed process history must not contain duplicate versions."));
            return false;
        }
        previous = version;
        hasPrevious = true;
    }

    if (hasActiveProcess) {
        if (!activeProcess.isValid(error)) return false;
        if (activeProcess.done != 0) {
            setError(error, QStringLiteral("The active process must have done=0."));
            return false;
        }
        const auto active = activeProcess;
        const bool rework = std::any_of(completedHistory.cbegin(), completedHistory.cend(),
                                        [&active](const auto& prior) {
                                            return prior.kind == active.kind
                                                && prior.number == active.number
                                                && prior.loop == active.loop
                                                && prior.iteration < active.iteration;
                                        });
        if (hasPrevious && !comesAfter(activeProcess, previous) && !rework) {
            setError(error, QStringLiteral("The active process cannot reopen a completed process/loop."));
            return false;
        }
    }

    if (hasNextProcess) {
        if (!nextProcess.isValid(error)) return false;
        if (nextProcess.loop < 1 || nextProcess.iteration != 0 || nextProcess.certification != 0 || nextProcess.done != 0) {
            setError(error, QStringLiteral("The next process must start as [P|F]<number>.<loop>.0.0.0."));
            return false;
        }
        if (nextProcess.isProcess()) {
            const int highestProcess = std::accumulate(completedHistory.cbegin(), completedHistory.cend(), 0,
                                                       [](int highest, const auto& version) {
                                                           return version.isProcess() ? qMax(highest, version.number) : highest;
                                                       });
            if (hasPrevious && nextProcess.number <= highestProcess) {
                setError(error, QStringLiteral("The next process must follow completed history."));
                return false;
            }
            if (!hasActiveProcess && hasPrevious && nextProcess.number != highestProcess + 1) {
                setError(error, QStringLiteral("The next process must directly follow the latest completed process."));
                return false;
            }
            const auto active = activeProcess;
            const bool activeRework = hasActiveProcess && std::any_of(completedHistory.cbegin(), completedHistory.cend(),
                                                                        [&active](const auto& prior) {
                                                                            return prior.kind == active.kind
                                                                                && prior.number == active.number
                                                                                && prior.loop == active.loop
                                                                                && prior.iteration < active.iteration;
                                                                        });
            if (hasActiveProcess && !activeRework && nextProcess.number <= activeProcess.number) {
                setError(error, QStringLiteral("The next process must follow the active process."));
                return false;
            }
            if (hasActiveProcess && !activeRework && nextProcess.number != activeProcess.number + 1) {
                setError(error, QStringLiteral("The next process must directly follow the active process."));
                return false;
            }
        }
    }

    if (hasFutureProcess) {
        if (!futureProcess.isValid(error)) return false;
        if (futureProcess.loop < 1 || futureProcess.iteration != 0 || futureProcess.certification != 0 || futureProcess.done != 0) {
            setError(error, QStringLiteral("The future process must start with iteration=0, certification=0, done=0."));
            return false;
        }
    }
    return true;
}

QStringList ProcessVersionState::completedIdentifiers() const
{
    QStringList result;
    for (const auto& version : completedHistory) result << version.identifier();
    return result;
}

QString ProcessVersionState::activeIdentifier() const
{
    return hasActiveProcess ? activeProcess.identifier() : QString();
}

QString ProcessVersionState::nextIdentifier() const
{
    return hasNextProcess ? nextProcess.identifier() : QString();
}

QString ProcessVersionState::futureIdentifier() const
{
    return hasFutureProcess ? futureProcess.identifier() : QString();
}

ProcessVersionState ProcessVersionState::empty()
{
    ProcessVersionState s;
    s.namespaceVersion = static_cast<int>(ProcessNamespace::CanonicalV2);
    return s;
}

ProcessVersionState ProcessVersionState::currentCanonicalState()
{
    ProcessVersionState s;
    s.namespaceVersion = static_cast<int>(ProcessNamespace::CanonicalV2);
    s.completedHistory = {
        ProcessVersion(ProcessKind::Process, 1, 1, 1, 1, 1),
        ProcessVersion(ProcessKind::Process, 2, 1, 1, 1, 1),
        ProcessVersion(ProcessKind::Process, 1, 1, 2, 1, 1),
        ProcessVersion(ProcessKind::Process, 3, 1, 4, 1, 1),
        ProcessVersion(ProcessKind::Process, 4, 1, 4, 1, 1),
        ProcessVersion(ProcessKind::Process, 5, 1, 4, 1, 1)
    };
    s.legacyHistory = {
        QStringLiteral("P0.1.1.1"),
        QStringLiteral("P1.1.1.1"),
        QStringLiteral("P2.1.1.1"),
        QStringLiteral("P3.1.0.0"),
        QStringLiteral("P0.1.1.1.1"),
        QStringLiteral("P1.1.1.1.1"),
        QStringLiteral("P2.1.1.1.1"),
        QStringLiteral("P1.1.1.1"),
        QStringLiteral("legacy:P0.1.2.1.1"),
        QStringLiteral("legacy:P1.1.1.1.1"),
        QStringLiteral("legacy:P2.1.4.1.1"),
        QStringLiteral("legacy:P3.1.4.1.1"),
        QStringLiteral("legacy:P4.1.4.1.1")
    };
    s.hasActiveProcess = false;
    s.activeProcess = {};
    s.hasNextProcess = true;
    s.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0); // F1.1.0.0.0
    s.hasFutureProcess = true;
    s.futureProcess = ProcessVersion(ProcessKind::Process, 6, 1, 0, 0, 0); // P6.1.0.0.0
    return s;
}

QJsonObject processVersionStateToJson(const ProcessVersionState& state)
{
    QJsonArray history;
    for (const auto& version : state.completedHistory) history.append(processVersionToJson(version));
    QJsonObject obj{
        {QStringLiteral("namespaceVersion"), state.namespaceVersion},
        {QStringLiteral("completedHistory"), history},
        {QStringLiteral("legacyHistory"), QJsonArray::fromStringList(state.legacyHistory)},
        {QStringLiteral("active"), state.hasActiveProcess ? QJsonValue(processVersionToJson(state.activeProcess)) : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("next"), state.hasNextProcess ? QJsonValue(processVersionToJson(state.nextProcess)) : QJsonValue(QJsonValue::Null)}
    };
    if (state.hasFutureProcess) {
        obj.insert(QStringLiteral("futureProcess"), processVersionToJson(state.futureProcess));
    }
    return obj;
}

bool processVersionStateFromJson(const QJsonValue& value, ProcessVersionState* state, QString* error)
{
    if (!state) { setError(error, QStringLiteral("Process version state result is not available.")); return false; }
    if (!value.isObject()) { setError(error, QStringLiteral("processVersion must be an object.")); return false; }
    const auto object = value.toObject();
    if (!object.value(QStringLiteral("completedHistory")).isArray()
        || !object.contains(QStringLiteral("active"))
        || !object.contains(QStringLiteral("next"))) {
        setError(error, QStringLiteral("processVersion requires completedHistory, active and next fields."));
        return false;
    }

    ProcessVersionState candidate;
    candidate.namespaceVersion = object.value(QStringLiteral("namespaceVersion")).toInt(static_cast<int>(ProcessNamespace::CanonicalV2));

    for (const auto& entry : object.value(QStringLiteral("legacyHistory")).toArray())
        if (entry.isString() && !entry.toString().trimmed().isEmpty()) candidate.legacyHistory << entry.toString().trimmed();

    for (const auto& entry : object.value(QStringLiteral("completedHistory")).toArray()) {
        if (!entry.toObject().contains(QStringLiteral("certification"))) {
            const auto legacy = entry.toObject();
            ProcessVersion old;
            if (!readNonNegativeInt(legacy, QStringLiteral("process"), &old.process, error)
                || !readNonNegativeInt(legacy, QStringLiteral("loop"), &old.loop, error)
                || !readNonNegativeInt(legacy, QStringLiteral("iteration"), &old.iteration, error)
                || !readNonNegativeInt(legacy, QStringLiteral("done"), &old.done, error)) return false;
            candidate.legacyHistory << QStringLiteral("P%1.%2.%3.%4").arg(old.process).arg(old.loop).arg(old.iteration).arg(old.done);
            continue;
        }
        ProcessVersion version;
        if (!processVersionFromJson(entry, &version, error)) return false;
        candidate.completedHistory << version;
    }

    const auto active = object.value(QStringLiteral("active"));
    if (!active.isNull()) {
        if (!active.toObject().contains(QStringLiteral("certification"))) {
            const auto legacy = active.toObject();
            candidate.legacyHistory << QStringLiteral("P%1.%2.%3.%4").arg(legacy.value("process").toInt()).arg(legacy.value("loop").toInt()).arg(legacy.value("iteration").toInt()).arg(legacy.value("done").toInt());
        } else {
            if (!processVersionFromJson(active, &candidate.activeProcess, error)) return false;
            candidate.hasActiveProcess = true;
        }
    }

    const auto next = object.value(QStringLiteral("next"));
    if (!next.isNull()) {
        if (!next.toObject().contains(QStringLiteral("certification"))) {
            const auto legacy = next.toObject();
            candidate.legacyHistory << QStringLiteral("P%1.%2.%3.%4").arg(legacy.value("process").toInt()).arg(legacy.value("loop").toInt()).arg(legacy.value("iteration").toInt()).arg(legacy.value("done").toInt());
        } else {
            if (!processVersionFromJson(next, &candidate.nextProcess, error)) return false;
            candidate.hasNextProcess = true;
        }
    }

    if (object.contains(QStringLiteral("futureProcess")) && !object.value(QStringLiteral("futureProcess")).isNull()) {
        if (!processVersionFromJson(object.value(QStringLiteral("futureProcess")), &candidate.futureProcess, error)) return false;
        candidate.hasFutureProcess = true;
    }

    if (!candidate.isValid(error)) return false;
    *state = candidate;
    return true;
}

// -------------------------------------------------------------------------
// ProcessVersionLifecycle
// -------------------------------------------------------------------------
bool ProcessVersionLifecycle::startNextProcess(ProcessVersionState* state, QString* error)
{
    if (!state) { setError(error, QStringLiteral("Process version state is not available.")); return false; }
    if (!state->isValid(error)) return false;
    if (state->hasActiveProcess) { setError(error, QStringLiteral("An active process already exists.")); return false; }
    if (!state->hasNextProcess) { setError(error, QStringLiteral("There is no next process to start.")); return false; }

    state->activeProcess = state->nextProcess;
    state->activeProcess.iteration = 1;
    state->activeProcess.certification = 0;
    state->activeProcess.done = 0;
    state->hasActiveProcess = true;
    if (state->activeProcess.isFoundation()) {
        if (state->hasFutureProcess) {
            state->nextProcess = state->futureProcess;
            state->hasNextProcess = true;
            state->futureProcess = {};
            state->hasFutureProcess = false;
        } else {
            state->nextProcess = {};
            state->hasNextProcess = false;
        }
    } else {
        state->nextProcess = {ProcessKind::Process, state->activeProcess.number + 1, 1, 0, 0, 0};
        state->hasNextProcess = true;
    }
    return state->isValid(error);
}

bool ProcessVersionLifecycle::reworkCompletedProcess(ProcessVersionState* state, int process, QString* error)
{
    if (!state) { setError(error, QStringLiteral("Process version state is not available.")); return false; }
    if (!state->isValid(error) || state->hasActiveProcess) {
        setError(error, QStringLiteral("A process rework requires no active process."));
        return false;
    }
    auto match = std::find_if(state->completedHistory.crbegin(), state->completedHistory.crend(),
                              [process](const auto& version) {
                                  return version.isProcess() && version.number == process;
                              });
    if (match == state->completedHistory.crend()) {
        setError(error, QStringLiteral("The requested process has no completed history."));
        return false;
    }
    if (match->done != 1 || match->certification != 1 || match->iteration == std::numeric_limits<int>::max()) {
        setError(error, QStringLiteral("Only a completed certified process can be reopened for rework."));
        return false;
    }
    ProcessVersion next{ProcessKind::Process, process, match->loop, match->iteration + 1, 0, 0};
    state->activeProcess = next;
    state->hasActiveProcess = true;
    return state->isValid(error);
}

bool ProcessVersionLifecycle::advanceIteration(ProcessVersionState* state, QString* error)
{
    if (!state) { setError(error, QStringLiteral("Process version state is not available.")); return false; }
    if (!state->isValid(error)) return false;
    if (!state->hasActiveProcess) { setError(error, QStringLiteral("Only an active, not-done process may advance iteration.")); return false; }
    if (state->activeProcess.done != 0 || state->activeProcess.iteration == std::numeric_limits<int>::max()) {
        setError(error, QStringLiteral("The active process cannot advance iteration."));
        return false;
    }
    ++state->activeProcess.iteration;
    state->activeProcess.certification = 0;
    return true;
}

bool ProcessVersionLifecycle::certifyCurrentIteration(ProcessVersionState* state, QString* error)
{
    if (!state) { setError(error, QStringLiteral("Process version state is not available.")); return false; }
    if (!state->isValid(error) || !state->hasActiveProcess || state->activeProcess.done != 0
        || state->activeProcess.certification != 0) {
        setError(error, QStringLiteral("Only an active, uncertified process may be certified."));
        return false;
    }
    state->activeProcess.certification = 1;
    return state->isValid(error);
}

bool ProcessVersionLifecycle::completeActiveProcess(ProcessVersionState* state, QString* error)
{
    if (!state) { setError(error, QStringLiteral("Process version state is not available.")); return false; }
    if (!state->isValid(error)) return false;
    if (!state->hasActiveProcess || state->activeProcess.done != 0 || state->activeProcess.certification != 1) {
        setError(error, QStringLiteral("Only an active, certified process may be explicitly completed."));
        return false;
    }
    ProcessVersion completed = state->activeProcess;
    completed.done = 1;
    state->completedHistory << completed;
    state->hasActiveProcess = false;
    state->activeProcess = {};
    return state->isValid(error);
}

bool ProcessVersionLifecycle::resetForFiveStageCampaign(ProcessVersionState* state, QString* error)
{
    if (!state) { setError(error, QStringLiteral("Process version state is not available.")); return false; }
    for (const auto& version : state->completedHistory)
        state->legacyHistory << QStringLiteral("%1").arg(version.identifier());
    state->completedHistory.clear();
    state->hasActiveProcess = false;
    state->activeProcess = {};
    state->nextProcess = {ProcessKind::Process, 0, 1, 0, 0, 0};
    state->hasNextProcess = true;
    state->futureProcess = {};
    state->hasFutureProcess = false;
    state->legacyHistory.removeDuplicates();
    return state->isValid(error);
}
