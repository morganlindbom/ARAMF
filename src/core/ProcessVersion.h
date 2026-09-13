#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

// Process versions describe ARAMF's governed work lifecycle.  They are
// intentionally separate from product, component, release, schema and
// migration versions.
struct ProcessVersion final
{
    int process = 0;
    int loop = 1;
    int iteration = 0;
    int done = 0;

    QString identifier() const;
    bool isValid(QString* error = nullptr) const;
    static bool parse(const QString& value, ProcessVersion* result, QString* error = nullptr);

    bool operator==(const ProcessVersion& other) const;
    bool operator!=(const ProcessVersion& other) const { return !(*this == other); }
};

struct ProcessVersionState final
{
    // Closed history is append-only through ProcessVersionLifecycle.  The
    // active and next fields preserve state without collapsing history into a
    // single mutable string.
    QList<ProcessVersion> completedHistory;
    bool hasActiveProcess = false;
    ProcessVersion activeProcess;
    bool hasNextProcess = false;
    ProcessVersion nextProcess;

    bool isValid(QString* error = nullptr) const;
    QStringList completedIdentifiers() const;
    QString activeIdentifier() const;
    QString nextIdentifier() const;

    static ProcessVersionState empty();
};

QJsonObject processVersionStateToJson(const ProcessVersionState& state);
bool processVersionStateFromJson(const QJsonValue& value,
                                 ProcessVersionState* state,
                                 QString* error = nullptr);

// Explicit lifecycle authority.  No operation is inferred from tests,
// generation, validation, launch or self-host regeneration.
class ProcessVersionLifecycle final
{
public:
    static bool startNextProcess(ProcessVersionState* state, QString* error = nullptr);
    static bool advanceIteration(ProcessVersionState* state, QString* error = nullptr);
    static bool completeActiveProcess(ProcessVersionState* state, QString* error = nullptr);
};
