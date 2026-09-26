#include "LifecycleCertificationFoundation.h"
#include "ProcessVersion.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSaveFile>
#include <QSet>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QStack>
#include <algorithm>

namespace { QJsonObject f4ReadJson(const QString& path) { QFile f(path); if(!f.open(QIODevice::ReadOnly))return {}; return QJsonDocument::fromJson(f.readAll()).object(); } }
F4LifecycleReport LifecycleCertificationFoundation::validate(const QString& projectRoot, QString* error)
{
    F4LifecycleReport report;

    // Load processVersion state from project.json
    const QString projectJsonPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/project.json"));
    const auto projectJson = f4ReadJson(projectJsonPath);

    if (!projectJson.contains(QStringLiteral("processVersion"))) {
        report.errors.append(QStringLiteral("F4: No processVersion in project.json"));
        return report;
    }

    ProcessVersionState pvState;
    QString parseErr;
    if (!processVersionStateFromJson(projectJson.value(QStringLiteral("processVersion")), &pvState, &parseErr)) {
        report.errors.append(QStringLiteral("F4: Cannot parse processVersion: %1").arg(parseErr));
        return report;
    }

    // 1. Namespace version
    report.namespaceVersionCorrect = pvState.namespaceVersion == static_cast<int>(ProcessNamespace::CanonicalV2);
    if (!report.namespaceVersionCorrect)
        report.errors.append(QStringLiteral("F4: Namespace version is not Canonical V2"));

    // 2. Process history validity - all completed entries must have cert=1, done=1, iteration >= 1
    report.processHistoryValid = true;
    report.completedProcesses = 0;
    report.completedFoundations = 0;
    for (const auto& pv : pvState.completedHistory) {
        if (pv.certification != 1 || pv.done != 1 || pv.iteration < 1) {
            report.processHistoryValid = false;
            report.errors.append(QStringLiteral("F4: Completed entry %1 has cert=%2 done=%3 iteration=%4")
                .arg(pv.identifier()).arg(pv.certification).arg(pv.done).arg(pv.iteration));
        }
        if (pv.isProcess()) report.completedProcesses++;
        if (pv.isFoundation()) report.completedFoundations++;
    }

    // 3. Active state validity
    report.activeStateValid = true;
    if (pvState.hasActiveProcess) {
        if (pvState.activeProcess.done != 0) {
            report.activeStateValid = false;
            report.errors.append(QStringLiteral("F4: Active process has done=1 but is still active"));
        }
        if (pvState.activeProcess.iteration < 1) {
            report.activeStateValid = false;
            report.errors.append(QStringLiteral("F4: Active process has iteration < 1"));
        }
    }

    // 4. Next state validity
    report.nextStateValid = true;
    if (pvState.hasNextProcess) {
        if (pvState.nextProcess.iteration != 0) {
            report.nextStateValid = false;
            report.errors.append(QStringLiteral("F4: Next process has non-zero iteration"));
        }
        if (pvState.nextProcess.certification != 0 || pvState.nextProcess.done != 0) {
            report.nextStateValid = false;
            report.errors.append(QStringLiteral("F4: Next process already certified/done before starting"));
        }
    }

    // 5. Foundation queue validity
    report.foundationQueueValid = true;
    for (const auto& fq : pvState.foundationQueue) {
        if (!fq.isFoundation()) {
            report.foundationQueueValid = false;
            report.errors.append(QStringLiteral("F4: Non-foundation entry in foundation queue"));
            break;
        }
    }

    // 6. P6 gating correctness
    report.p6GatingCorrect = true;
    QString p6Reason;
    bool p6Eligible = pvState.isP6Eligible(&p6Reason);
    // P6 should NOT be eligible unless all foundations are complete and integration valid
    if (p6Eligible && !pvState.allFoundationsComplete()) {
        report.p6GatingCorrect = false;
        report.errors.append(QStringLiteral("F4: P6 eligible but foundations incomplete"));
    }
    if (p6Eligible && !pvState.foundationIntegrationValid) {
        report.p6GatingCorrect = false;
        report.errors.append(QStringLiteral("F4: P6 eligible but foundationIntegrationValid is false"));
    }

    // 7. Certification semantics: completed entries must be cert=1, done=1, iteration >= 1;
    // done=1 requires cert=1; done=1 requires iteration >= 1.
    report.certificationSemanticsValid = true;
    for (const auto& pv : pvState.completedHistory) {
        if (pv.certification != 1 || pv.done != 1 || pv.iteration < 1) {
            report.certificationSemanticsValid = false;
            report.errors.append(QStringLiteral("F4: %1 violates certification semantics").arg(pv.identifier()));
        }
    }
    if (pvState.hasActiveProcess && pvState.activeProcess.done == 1 && pvState.activeProcess.certification != 1) {
        report.certificationSemanticsValid = false;
        report.errors.append(QStringLiteral("F4: Active process marked done=1 without certification=1"));
    }

    // Also validate through CertificationService
    const auto certState = f4ReadJson(QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/certification/current-certification-state.json")));
    report.fullReport.insert(QStringLiteral("certificationState"), certState);

    report.valid = report.processHistoryValid && report.activeStateValid
                && report.nextStateValid && report.foundationQueueValid
                && report.p6GatingCorrect && report.certificationSemanticsValid
                && report.namespaceVersionCorrect;

    return report;
}

QJsonObject LifecycleCertificationFoundation::lifecycleSummary(const QString& projectRoot, QString* error)
{
    QJsonObject summary;

    const QString projectJsonPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/project.json"));
    const auto projectJson = f4ReadJson(projectJsonPath);

    ProcessVersionState pvState;
    processVersionStateFromJson(projectJson.value(QStringLiteral("processVersion")), &pvState, error);

    summary.insert(QStringLiteral("namespaceVersion"), pvState.namespaceVersion);
    summary.insert(QStringLiteral("completedCount"), pvState.completedHistory.size());

    QJsonArray completedIds;
    for (const auto& id : pvState.completedIdentifiers())
        completedIds.append(id);
    summary.insert(QStringLiteral("completedIdentifiers"), completedIds);

    summary.insert(QStringLiteral("hasActive"), pvState.hasActiveProcess);
    if (pvState.hasActiveProcess)
        summary.insert(QStringLiteral("active"), pvState.activeIdentifier());

    summary.insert(QStringLiteral("hasNext"), pvState.hasNextProcess);
    if (pvState.hasNextProcess)
        summary.insert(QStringLiteral("next"), pvState.nextIdentifier());

    QJsonArray remaining;
    for (const auto& fq : pvState.remainingFoundationQueue())
        remaining.append(fq.identifier());
    summary.insert(QStringLiteral("remainingFoundations"), remaining);

    summary.insert(QStringLiteral("allFoundationsComplete"), pvState.allFoundationsComplete());
    summary.insert(QStringLiteral("foundationIntegrationValid"), pvState.foundationIntegrationValid);

    QString p6Reason;
    summary.insert(QStringLiteral("p6Eligible"), pvState.isP6Eligible(&p6Reason));
    summary.insert(QStringLiteral("p6Reason"), p6Reason);

    summary.insert(QStringLiteral("foundation"), QStringLiteral("F4"));
    summary.insert(QStringLiteral("name"), QStringLiteral("Lifecycle & Certification Foundation"));

    return summary;
}

bool LifecycleCertificationFoundation::validateCertificationSemantics(
    const QJsonObject& state, QString* error)
{
    const int cert = state.value(QStringLiteral("certification")).toInt(0);
    const int done = state.value(QStringLiteral("done")).toInt(0);
    const int iteration = state.value(QStringLiteral("iteration")).toInt(0);

    // Done requires certification (cannot complete uncertified)
    if (done == 1 && cert != 1) {
        if (error) *error = QStringLiteral("F4: Done requires certification=1 (cannot complete uncertified)");
        return false;
    }

    // Done requires at least one iteration
    if (done == 1 && iteration < 1) {
        if (error) *error = QStringLiteral("F4: Done requires at least one iteration (iteration >= 1)");
        return false;
    }

    // cert=1 with done=0 is valid for an active certified iteration awaiting completion
    // cert=0 with done=0 is valid for an in-progress iteration
    // cert=1 with done=1 is valid for a completed certified iteration
    return true;
}

QJsonObject LifecycleCertificationFoundation::contract()
{
    return QJsonObject{
        {QStringLiteral("foundation"), QStringLiteral("F4")},
        {QStringLiteral("name"), QStringLiteral("Lifecycle & Certification Foundation")},
        {QStringLiteral("responsibility"),
            QStringLiteral("Establishes lifecycle and certification meaning")},
        {QStringLiteral("owns"), QJsonArray{
            QStringLiteral("process-lifecycle-state-machine"),
            QStringLiteral("foundation-lifecycle-state-machine"),
            QStringLiteral("certification-semantics"),
            QStringLiteral("p6-gating"),
            QStringLiteral("namespace-versioning")
        }},
        {QStringLiteral("bootstrapOrder"), 4},
        {QStringLiteral("dependsOn"), QJsonArray{}},
        {QStringLiteral("requiredBy"), QJsonArray{QStringLiteral("P6")}}
    };
}
