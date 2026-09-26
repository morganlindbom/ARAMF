#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
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
