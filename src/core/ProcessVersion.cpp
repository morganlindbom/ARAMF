#include "ProcessVersion.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <cmath>
#include <limits>

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

bool processVersionFromJson(const QJsonValue& value, ProcessVersion* result, QString* error)
{
    if (!value.isObject()) {
        setError(error, QStringLiteral("Process version entry must be an object."));
        return false;
    }
    const auto object = value.toObject();
    ProcessVersion candidate;
    if (!readNonNegativeInt(object, QStringLiteral("process"), &candidate.process, error)
        || !readNonNegativeInt(object, QStringLiteral("loop"), &candidate.loop, error)
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
    return {{QStringLiteral("process"), version.process},
            {QStringLiteral("loop"), version.loop},
            {QStringLiteral("iteration"), version.iteration},
            {QStringLiteral("certification"), version.certification},
            {QStringLiteral("done"), version.done}};
}

bool sameProcessLoop(const ProcessVersion& left, const ProcessVersion& right)
{
    return left.process == right.process && left.loop == right.loop;
}

bool comesAfter(const ProcessVersion& left, const ProcessVersion& right)
{
    return left.process > right.process
        || (left.process == right.process && left.loop > right.loop);
}
}

QString ProcessVersion::identifier() const
{
    return QStringLiteral("P%1.%2.%3.%4.%5").arg(process).arg(loop).arg(iteration).arg(certification).arg(done);
}

bool ProcessVersion::isValid(QString* error) const
{
    if (process < 0) { setError(error, QStringLiteral("Process number must be non-negative.")); return false; }
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
    const auto match = QRegularExpression(QStringLiteral("^P([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([01])\\.([01])$")).match(value.trimmed());
    if (!match.hasMatch()) {
        setError(error, QStringLiteral("Invalid process version '%1'. Expected P<process>.<loop>.<iteration>.<done>.").arg(value));
        return false;
    }
    bool ok = false;
    ProcessVersion candidate;
    candidate.process = match.captured(1).toInt(&ok); if (!ok) { setError(error, QStringLiteral("Process number is too large.")); return false; }
    candidate.loop = match.captured(2).toInt(&ok); if (!ok) { setError(error, QStringLiteral("Process loop is too large.")); return false; }
    candidate.iteration = match.captured(3).toInt(&ok); if (!ok) { setError(error, QStringLiteral("Process iteration is too large.")); return false; }
    candidate.certification = match.captured(4).toInt(&ok); if (!ok) { setError(error, QStringLiteral("Process certification flag is invalid.")); return false; }
    candidate.done = match.captured(5).toInt(&ok); if (!ok) { setError(error, QStringLiteral("Process done flag is invalid.")); return false; }
    if (!candidate.isValid(error)) return false;
    *result = candidate;
    return true;
}

bool ProcessVersion::operator==(const ProcessVersion& other) const
{
    return process == other.process && loop == other.loop && iteration == other.iteration
        && certification == other.certification && done == other.done;
}

bool ProcessVersionState::isValid(QString* error) const
{
    ProcessVersion previous;
    bool hasPrevious = false;
    for (const auto& version : completedHistory) {
        if (!version.isValid(error)) return false;
        if (version.done != 1) {
            setError(error, QStringLiteral("Completed process history must contain only done=1 entries."));
            return false;
        }
        if (hasPrevious && !comesAfter(version, previous)) {
            setError(error, QStringLiteral("Completed process history must be ordered by process and loop without duplicates."));
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
        if (hasPrevious && !comesAfter(activeProcess, previous)) {
            setError(error, QStringLiteral("The active process cannot reopen a completed process/loop."));
            return false;
        }
    }

    if (hasNextProcess) {
        if (!nextProcess.isValid(error)) return false;
        if (nextProcess.loop < 1 || nextProcess.iteration != 0 || nextProcess.certification != 0 || nextProcess.done != 0) {
            setError(error, QStringLiteral("The next process must start as P<process>.<loop>.0.0.0."));
            return false;
        }
        if (hasPrevious && nextProcess.process <= previous.process) {
            setError(error, QStringLiteral("The next process must follow completed history."));
            return false;
        }
        if (!hasActiveProcess && hasPrevious && nextProcess.process != previous.process + 1) {
            setError(error, QStringLiteral("The next process must directly follow the latest completed process."));
            return false;
        }
        if (hasActiveProcess && nextProcess.process <= activeProcess.process) {
            setError(error, QStringLiteral("The next process must follow the active process."));
            return false;
        }
        if (hasActiveProcess && nextProcess.process != activeProcess.process + 1) {
            setError(error, QStringLiteral("The next process must directly follow the active process."));
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

ProcessVersionState ProcessVersionState::empty()
{
    return {};
}

QJsonObject processVersionStateToJson(const ProcessVersionState& state)
{
    QJsonArray history;
    for (const auto& version : state.completedHistory) history.append(processVersionToJson(version));
    return {{QStringLiteral("completedHistory"), history},
            {QStringLiteral("legacyHistory"), QJsonArray::fromStringList(state.legacyHistory)},
            {QStringLiteral("active"), state.hasActiveProcess ? QJsonValue(processVersionToJson(state.activeProcess)) : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("next"), state.hasNextProcess ? QJsonValue(processVersionToJson(state.nextProcess)) : QJsonValue(QJsonValue::Null)}};
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
    if (!candidate.isValid(error)) return false;
    *state = candidate;
    return true;
}

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
    state->nextProcess = {state->activeProcess.process + 1, 1, 0, 0, 0};
    state->hasNextProcess = true;
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
    state->nextProcess = {0, 1, 0, 0, 0};
    state->hasNextProcess = true;
    state->legacyHistory.removeDuplicates();
    return state->isValid(error);
}
