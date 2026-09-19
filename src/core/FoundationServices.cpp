// FoundationServices.cpp
// Implementation of the F1-F4 Foundation services.
// These are thin coordination layers that integrate existing ARAMF core services
// under the canonical foundation responsibility model.
// Bootstrap order: F1 loads → F2 validates trust → F3 validates integrity → F4 reconstructs lifecycle.
// No circular authority chains. No upward dependencies on Process layer.

#include "FoundationServices.h"
#include "AramfPaths.h"
#include "ProjectMemory.h"
#include "ProjectMemoryCompaction.h"
#include "CertificationService.h"
#include "ProcessVersion.h"
#include "ProjectModel.h"
#include "ProjectPersistence.h"
#include "Services.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTextStream>
#include <QUuid>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSet>

namespace {

QByteArray readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

QJsonObject readJsonFile(const QString& path)
{
    const auto data = readFile(path);
    if (data.isEmpty()) return {};
    return QJsonDocument::fromJson(data).object();
}

QList<QJsonObject> readJsonlEvents(const QString& path)
{
    QList<QJsonObject> result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    while (!f.atEnd()) {
        const auto line = f.readLine().trimmed();
        if (line.isEmpty()) continue;
        auto doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) result.append(doc.object());
    }
    return result;
}

QString computeFingerprint(const QJsonObject& obj)
{
    const QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString computeFileSha256(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        hash.addData(f.read(65536));
    }
    return QString::fromLatin1(hash.result().toHex());
}

} // anonymous namespace

// F1: Memory & Evidence Foundation implementation is located in MemoryEvidenceFoundation.cpp.



// ═══════════════════════════════════════════════════════════════════════════════
// F2: Identity, Provenance & Trust Foundation
// ═══════════════════════════════════════════════════════════════════════════════

F2TrustReport IdentityTrustFoundation::validate(const QString& projectRoot, QString* error)
{
    F2TrustReport report;
    ProjectMemory memory;

    // 1. Validate provenance across all events
    const auto memReport = memory.validate(projectRoot, error, false);
    report.fullReport.insert(QStringLiteral("memoryValidation"), memReport);

    const auto checks = memReport.value(QStringLiteral("checks")).toArray();
    for (const auto& v : checks) {
        const auto obj = v.toObject();
        if (obj.value(QStringLiteral("name")).toString() == QStringLiteral("event-provenance-valid")) {
            report.allProvenanceValid =
                obj.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
            if (!report.allProvenanceValid)
                report.errors.append(QStringLiteral("F2: Event provenance validation failed"));
        }
    }

    // 2. Actor taxonomy consistency check
    const QString eventLogPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    const auto events = readJsonlEvents(eventLogPath);

    const auto validActors = actorTaxonomy();
    QSet<QString> seenActors;
    report.actorTaxonomyConsistent = true;

    // Read legacy cutoff from manifest
    const QString manifestPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
    const auto manifest = readJsonFile(manifestPath);
    const int legacyCutoff = manifest.value(QStringLiteral("legacyProvenanceCutoffSequence")).toInt(0);

    for (const auto& ev : events) {
        const int seq = ev.value(QStringLiteral("sequenceNumber")).toInt(0);
        const auto prov = ev.value(QStringLiteral("provenance")).toObject();
        const auto actor = prov.value(QStringLiteral("actor")).toString();

        if (seq <= legacyCutoff && actor.isEmpty()) {
            report.legacyExemptEvents++;
            continue;
        }

        if (!actor.isEmpty()) {
            seenActors.insert(actor);
            report.eventsWithProvenance++;
            if (!validActors.contains(actor)) {
                report.actorTaxonomyConsistent = false;
                report.errors.append(QStringLiteral("F2: Unknown actor '%1' in event seq %2")
                    .arg(actor).arg(seq));
            }
        } else {
            report.eventsWithoutProvenance++;
        }
    }
    report.recognizedActors = seenActors.values();
    report.recognizedActors.sort();

    // 3. Trust boundary enforcement - verify admin override events have correct provenance & identity
    report.trustBoundariesEnforced = true;
    report.adminOverrideValid = true;
    for (const auto& ev : events) {
        if (ev.value(QStringLiteral("eventType")).toString() == QStringLiteral("ADMIN_OVERRIDE")) {
            const auto prov = ev.value(QStringLiteral("provenance")).toObject();
            const QString actor = prov.value(QStringLiteral("actor")).toString();
            if (actor != QStringLiteral("human")) {
                report.trustBoundariesEnforced = false;
                report.adminOverrideValid = false;
                report.errors.append(QStringLiteral("F2: ADMIN_OVERRIDE event has non-human actor '%1'").arg(actor));
            }
            const QString instruction = ev.value(QStringLiteral("instruction")).toString();
            if (!instruction.isEmpty() && !isVerifiedAdministrativeOverride(instruction)) {
                report.adminOverrideValid = false;
                report.errors.append(QStringLiteral("F2: ADMIN_OVERRIDE event has invalid instruction identity"));
            }
            const QString action = ev.value(QStringLiteral("requestedAction")).toString();
            if (containsDestructivePattern(action) || containsDestructivePattern(instruction)) {
                report.trustBoundariesEnforced = false;
                report.errors.append(QStringLiteral("F2: Prohibited destructive action in ADMIN_OVERRIDE event"));
            }
        }
    }

    report.valid = report.allProvenanceValid && report.actorTaxonomyConsistent
                && report.trustBoundariesEnforced && report.adminOverrideValid;
    return report;
}

QStringList IdentityTrustFoundation::actorTaxonomy()
{
    return {
        QStringLiteral("human"),
        QStringLiteral("user"),
        QStringLiteral("agent"),
        QStringLiteral("autonomous-agent"),
        QStringLiteral("tool"),
        QStringLiteral("runtime"),
        QStringLiteral("system")
    };
}

bool IdentityTrustFoundation::isValidActor(const QString& actor)
{
    return actorTaxonomy().contains(actor.toLower().trimmed());
}

bool IdentityTrustFoundation::validateProvenance(const QJsonObject& provenance, QString* error)
{
    return ProjectMemory::validateProvenanceObject(provenance, error);
}

bool IdentityTrustFoundation::isVerifiedAdministrativeOverride(const QString& instruction)
{
    const QString normalized = instruction.simplified();
    return normalized.contains(QStringLiteral("Admin Morgan Lindbom"), Qt::CaseSensitive)
        && normalized.contains(QStringLiteral("override"), Qt::CaseInsensitive);
}

bool IdentityTrustFoundation::containsDestructivePattern(const QString& text)
{
    static const QStringList patterns = {
        QStringLiteral("rmdir /s /q"),
        QStringLiteral("rd /s /q"),
        QStringLiteral("rm -rf"),
        QStringLiteral("Remove-Item -Recurse"),
        QStringLiteral("Remove-Item -r"),
        QStringLiteral("del /s /q"),
        QStringLiteral("git reset --hard"),
        QStringLiteral("git clean")
    };
    for (const auto& p : patterns) {
        if (text.contains(p, Qt::CaseInsensitive)) return true;
    }
    return false;
}

bool IdentityTrustFoundation::respectsTrustBoundary(const QString& instruction,
                                                     const QString& requestedAction,
                                                     QString* error)
{
    // Trust boundary: admin overrides must come from verified admin identity
    if (!isVerifiedAdministrativeOverride(instruction)) {
        if (error) *error = QStringLiteral("F2: Instruction does not pass administrative verification (exact Admin Morgan Lindbom identity and override intent required)");
        return false;
    }

    // Block dangerous shell operations regardless of admin status
    if (containsDestructivePattern(requestedAction) || containsDestructivePattern(instruction)) {
        if (error) *error = QStringLiteral("F2: Recursive deletion blocked by trust boundary");
        return false;
    }

    return true;
}

QJsonObject IdentityTrustFoundation::contract()
{
    return QJsonObject{
        {QStringLiteral("foundation"), QStringLiteral("F2")},
        {QStringLiteral("name"), QStringLiteral("Identity, Provenance & Trust Foundation")},
        {QStringLiteral("responsibility"),
            QStringLiteral("Establishes who/what produced evidence and whether attribution is trustworthy")},
        {QStringLiteral("owns"), QJsonArray{
            QStringLiteral("actor-identity-taxonomy"),
            QStringLiteral("provenance-validation"),
            QStringLiteral("trust-boundary-enforcement"),
            QStringLiteral("admin-override-verification")
        }},
        {QStringLiteral("bootstrapOrder"), 2},
        {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1")}},
        {QStringLiteral("requiredBy"), QJsonArray{QStringLiteral("F3"), QStringLiteral("F4")}}
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// F3: Scope, State & Integrity Foundation
// ═══════════════════════════════════════════════════════════════════════════════

QSet<QString> ScopeIntegrityFoundation::baseReservedScopes()
{
    return {
        QStringLiteral("all"),
        QStringLiteral("history"),
        QStringLiteral("project"),
        QStringLiteral("global"),
        QStringLiteral("project+global"),
        QStringLiteral("build-system"),
        QStringLiteral("ci-cd"),
        QStringLiteral("configuration"),
        QStringLiteral("documentation"),
        QStringLiteral("entire-project"),
        QStringLiteral("generated-files"),
        QStringLiteral("resources"),
        QStringLiteral("source-code"),
        QStringLiteral("tests"),
        QStringLiteral("ui-ux")
    };
}

QStringList ScopeIntegrityFoundation::canonicalScopes()
{
    auto list = baseReservedScopes().values();
    list.sort();
    return list;
}

QSet<QString> ScopeIntegrityFoundation::effectiveCanonicalScopeRegistry(const QString& projectRoot)
{
    auto registry = baseReservedScopes();
    const QString routesPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/routing/scope-routes.json"));
    QFile f(routesPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const auto doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            const auto arr = doc.object().value(QStringLiteral("scopes")).toArray();
            for (const auto& item : arr) {
                const QString id = item.toObject().value(QStringLiteral("id")).toString().trimmed();
                if (!id.isEmpty()) registry.insert(id);
            }
        }
    }
    return registry;
}

bool ScopeIntegrityFoundation::validateProjectIsolation(const QString& projectRoot, QString* error)
{
    // 1. Verify project identity binding in project.json
    const QString projectJsonPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/project.json"));
    const auto projectJson = readJsonFile(projectJsonPath);
    QString projId = projectJson.value(QStringLiteral("projectId")).toString().trimmed();
    if (projId.isEmpty()) {
        projId = projectJson.value(QStringLiteral("id")).toString().trimmed();
    }
    if (projId.isEmpty()) {
        projId = projectJson.value(QStringLiteral("name")).toString().trimmed();
    }
    if (projId.isEmpty() && projectJson.contains(QStringLiteral("processVersion"))) {
        projId = QStringLiteral("fixture-project");
    }
    if (projId.isEmpty()) {
        if (error) *error = QStringLiteral("F3: Missing projectId in project.json");
        return false;
    }

    // 2. Scan event log for foreign path leakage or root escapes
    const QString eventLogPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    const auto events = readJsonlEvents(eventLogPath);
    const QString cleanRoot = QDir::cleanPath(projectRoot);

    for (const auto& ev : events) {
        const auto filesArr = ev.value(QStringLiteral("affectedFiles")).toArray();
        for (const auto& item : filesArr) {
            const QString filePath = item.toString().trimmed();
            if (filePath.isEmpty()) continue;

            // Reject directory traversal escaping the project root
            if (filePath.contains(QStringLiteral(".."))) {
                QString resolved = QDir::cleanPath(QDir(cleanRoot).filePath(filePath));
                if (!resolved.startsWith(cleanRoot)) {
                    if (error) *error = QStringLiteral("F3: Foreign path escape detected: '%1'").arg(filePath);
                    return false;
                }
            }

            // Reject foreign absolute paths not within project root
            if (QDir::isAbsolutePath(filePath)) {
                QString cleanFile = QDir::cleanPath(filePath);
                if (!cleanFile.startsWith(cleanRoot, Qt::CaseInsensitive)) {
                    if (error) *error = QStringLiteral("F3: Foreign absolute path detected: '%1'").arg(filePath);
                    return false;
                }
            }
        }
    }

    return true;
}

F3IntegrityReport ScopeIntegrityFoundation::validate(const QString& projectRoot,
                                                      const ProjectModel* model,
                                                      QString* error)
{
    F3IntegrityReport report;
    ProjectMemory memory;

    // 1. Scope taxonomy validation via memory validation
    const auto memReport = memory.validate(projectRoot, error, false);
    report.fullReport.insert(QStringLiteral("memoryValidation"), memReport);

    const auto checks = memReport.value(QStringLiteral("checks")).toArray();
    for (const auto& v : checks) {
        const auto obj = v.toObject();
        if (obj.value(QStringLiteral("name")).toString() == QStringLiteral("persisted-scope-validity")) {
            report.scopeTaxonomyValid =
                obj.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
            if (!report.scopeTaxonomyValid)
                report.errors.append(QStringLiteral("F3: Persisted scope validity failed"));
        }
    }

    // 2. Count canonical and dynamic scopes
    const auto canonical = canonicalScopes();
    report.canonicalScopeCount = canonical.size();

    // Load dynamic scopes from scope-routes.json
    const QString routesPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/routing/scope-routes.json"));
    const auto routesDoc = readJsonFile(routesPath);
    const auto scopeArray = routesDoc.value(QStringLiteral("scopes")).toArray();
    int dynamicCount = 0;
    for (const auto& s : scopeArray) {
        const auto id = s.toObject().value(QStringLiteral("id")).toString();
        if (!canonical.contains(id)) dynamicCount++;
    }
    report.dynamicScopeCount = dynamicCount;

    // 3. Scope combination legality - verify all events have legal scope combinations
    report.scopeCombinationsLegal = true;
    const QString eventLogPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    const auto events = readJsonlEvents(eventLogPath);
    for (const auto& ev : events) {
        const auto scopesVal = ev.value(QStringLiteral("scopes"));
        if (scopesVal.isArray()) {
            QStringList scopes;
            for (const auto& s : scopesVal.toArray())
                scopes.append(s.toString());
            QString combErr;
            if (!ProjectMemory::validateScopeCombinations(scopes, &combErr)) {
                report.scopeCombinationsLegal = false;
                report.errors.append(QStringLiteral("F3: Illegal scope combination in event: %1").arg(combErr));
                break;
            }
        }
    }

    // 4. Cross-scope file validation
    report.crossScopeFilesValid = true;
    for (const auto& ev : events) {
        const auto scope = ev.value(QStringLiteral("scope")).toString();
        const auto filesArr = ev.value(QStringLiteral("affectedFiles")).toArray();
        if (!scope.isEmpty() && !filesArr.isEmpty()) {
            QStringList files;
            for (const auto& f : filesArr)
                files.append(f.toString());
            QString csErr;
            if (!ProjectMemory::validateCrossScopeFiles(scope, files, &csErr)) {
                report.crossScopeFilesValid = false;
                report.errors.append(QStringLiteral("F3: Cross-scope file violation: %1").arg(csErr));
                break;
            }
        }
    }

    // 5. Project state integrity - check consistency of processVersion state
    report.projectStateIntegral = true;
    const QString projectJsonPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/project.json"));
    const auto projectJson = readJsonFile(projectJsonPath);
    if (projectJson.contains(QStringLiteral("processVersion"))) {
        ProcessVersionState pvState;
        QString pvErr;
        if (!processVersionStateFromJson(projectJson.value(QStringLiteral("processVersion")), &pvState, &pvErr)) {
            report.projectStateIntegral = false;
            report.errors.append(QStringLiteral("F3: Process version state parse error: %1").arg(pvErr));
        } else {
            QString stateErr;
            if (!pvState.isValid(&stateErr)) {
                report.projectStateIntegral = false;
                report.errors.append(QStringLiteral("F3: Process version state invalid: %1").arg(stateErr));
            }
        }
    }

    // 6. Project isolation boundary validation (NO upward dependency on P-layer routing)
    QString isolErr;
    report.projectIsolationValid = validateProjectIsolation(projectRoot, &isolErr);
    if (!report.projectIsolationValid) {
        report.errors.append(isolErr);
    }

    report.valid = report.scopeTaxonomyValid && report.scopeCombinationsLegal
                && report.crossScopeFilesValid && report.projectStateIntegral
                && report.projectIsolationValid;

    return report;
}

bool ScopeIntegrityFoundation::validateScopeSet(const QStringList& scopes, QString* error)
{
    return ProjectMemory::validateScopeCombinations(scopes, error);
}

bool ScopeIntegrityFoundation::validateScopeFiles(const QString& scope,
                                                     const QStringList& files,
                                                     QString* error)
{
    return ProjectMemory::validateCrossScopeFiles(scope, files, error);
}

QJsonObject ScopeIntegrityFoundation::contract()
{
    return QJsonObject{
        {QStringLiteral("foundation"), QStringLiteral("F3")},
        {QStringLiteral("name"), QStringLiteral("Scope, State & Integrity Foundation")},
        {QStringLiteral("responsibility"),
            QStringLiteral("Establishes whether state/scope relationships are legal and integral")},
        {QStringLiteral("owns"), QJsonArray{
            QStringLiteral("scope-taxonomy"),
            QStringLiteral("scope-combination-rules"),
            QStringLiteral("cross-scope-file-validation"),
            QStringLiteral("project-state-integrity"),
            QStringLiteral("project-isolation-boundary")
        }},
        {QStringLiteral("bootstrapOrder"), 3},
        {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2")}},
        {QStringLiteral("requiredBy"), QJsonArray{QStringLiteral("F4")}}
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// F4: Lifecycle & Certification Foundation
// ═══════════════════════════════════════════════════════════════════════════════

F4LifecycleReport LifecycleCertificationFoundation::validate(const QString& projectRoot, QString* error)
{
    F4LifecycleReport report;

    // Load processVersion state from project.json
    const QString projectJsonPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/project.json"));
    const auto projectJson = readJsonFile(projectJsonPath);

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
    CertificationService certService;
    const auto certState = certService.currentState(projectRoot, error);
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
    const auto projectJson = readJsonFile(projectJsonPath);

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
        {QStringLiteral("dependsOn"), QJsonArray{
            QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3")
        }},
        {QStringLiteral("requiredBy"), QJsonArray{QStringLiteral("P6")}}
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cross-Foundation Integration Service
// ═══════════════════════════════════════════════════════════════════════════════

FoundationIntegrationReport FoundationIntegrationService::validate(
    const QString& projectRoot, const ProjectModel* model, QString* error)
{
    FoundationIntegrationReport report;
    report.acceptanceType = QStringLiteral("DIAGNOSTIC_RESULT");
    report.diagnosticOnly = true;

    // Execute in strict bootstrap order: F1 → F2 → F3 → F4
    // Each foundation depends on its predecessors but not on successors.

    // F1: Memory & Evidence (no dependencies)
    report.f1 = MemoryEvidenceFoundation::validate(projectRoot, error);
    if (!report.f1.valid) {
        report.errors.append(QStringLiteral("Integration: F1 failed - cannot proceed to F2"));
    }

    // F2: Identity, Provenance & Trust (depends on F1)
    report.f2 = IdentityTrustFoundation::validate(projectRoot, error);
    if (!report.f2.valid) {
        report.errors.append(QStringLiteral("Integration: F2 failed - trust layer compromised"));
    }

    // F3: Scope, State & Integrity (depends on F1, F2)
    report.f3 = ScopeIntegrityFoundation::validate(projectRoot, model, error);
    if (!report.f3.valid) {
        report.errors.append(QStringLiteral("Integration: F3 failed - integrity layer compromised"));
    }

    // F4: Lifecycle & Certification (depends on F1, F2, F3)
    report.f4 = LifecycleCertificationFoundation::validate(projectRoot, error);
    if (!report.f4.valid) {
        report.errors.append(QStringLiteral("Integration: F4 failed - lifecycle layer invalid"));
    }

    // Bootstrap order verification
    report.bootstrapOrderValid = verifyBootstrapOrder(error);

    // No circular authority check: verify each foundation contract's dependsOn
    report.noCircularAuthority = true;
    const auto f1c = MemoryEvidenceFoundation::contract();
    const auto f2c = IdentityTrustFoundation::contract();
    const auto f3c = ScopeIntegrityFoundation::contract();
    const auto f4c = LifecycleCertificationFoundation::contract();

    // F1 depends on nothing
    if (!f1c.value(QStringLiteral("dependsOn")).toArray().isEmpty()) {
        report.noCircularAuthority = false;
        report.errors.append(QStringLiteral("Integration: F1 must not depend on other foundations"));
    }
    // F2 depends only on F1
    const auto f2Deps = f2c.value(QStringLiteral("dependsOn")).toArray();
    for (const auto& d : f2Deps) {
        if (d.toString() != QStringLiteral("F1")) {
            report.noCircularAuthority = false;
            report.errors.append(QStringLiteral("Integration: F2 has unexpected dependency: %1").arg(d.toString()));
        }
    }
    // F3 depends only on F1 and F2
    const auto f3Deps = f3c.value(QStringLiteral("dependsOn")).toArray();
    for (const auto& d : f3Deps) {
        const auto dep = d.toString();
        if (dep != QStringLiteral("F1") && dep != QStringLiteral("F2")) {
            report.noCircularAuthority = false;
            report.errors.append(QStringLiteral("Integration: F3 has unexpected dependency: %1").arg(dep));
        }
    }
    // F4 depends only on F1, F2, and F3
    const auto f4Deps = f4c.value(QStringLiteral("dependsOn")).toArray();
    for (const auto& d : f4Deps) {
        const auto dep = d.toString();
        if (dep != QStringLiteral("F1") && dep != QStringLiteral("F2") && dep != QStringLiteral("F3")) {
            report.noCircularAuthority = false;
            report.errors.append(QStringLiteral("Integration: F4 has unexpected dependency: %1").arg(dep));
        }
    }

    // Build full report and compute fingerprints
    report.fullReport.insert(QStringLiteral("f1"), report.f1.fullReport);
    report.fullReport.insert(QStringLiteral("f2"), report.f2.fullReport);
    report.fullReport.insert(QStringLiteral("f3"), report.f3.fullReport);
    report.fullReport.insert(QStringLiteral("f4"), report.f4.fullReport);
    report.fullReport.insert(QStringLiteral("bootstrapOrderValid"), report.bootstrapOrderValid);
    report.fullReport.insert(QStringLiteral("noCircularAuthority"), report.noCircularAuthority);
    report.fullReport.insert(QStringLiteral("acceptanceType"), report.acceptanceType);
    report.fullReport.insert(QStringLiteral("diagnosticOnly"), report.diagnosticOnly);

    report.overallFingerprint = computeFingerprint(report.fullReport);
    report.fullReport.insert(QStringLiteral("overallFingerprint"), report.overallFingerprint);

    report.valid = report.f1.valid && report.f2.valid && report.f3.valid && report.f4.valid
                && report.bootstrapOrderValid && report.noCircularAuthority;

    return report;
}

bool FoundationIntegrationService::writeIntegrationEvidence(const QString& projectRoot,
                                                            const FoundationIntegrationReport& report,
                                                            QString* error)
{
    const QString targetPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/verification/foundation-integration.json"));
    QDir(projectRoot).mkpath(QStringLiteral("ARAMF_WORKER/verification"));

    QJsonObject evidence{
        {QStringLiteral("_file"), QStringLiteral("foundation-integration.json")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("acceptanceType"), report.acceptanceType},
        {QStringLiteral("authoritative"), false},
        {QStringLiteral("diagnosticOnly"), report.diagnosticOnly},
        {QStringLiteral("valid"), report.valid},
        {QStringLiteral("bootstrapOrderValid"), report.bootstrapOrderValid},
        {QStringLiteral("noCircularAuthority"), report.noCircularAuthority},
        {QStringLiteral("overallFingerprint"), report.overallFingerprint},
        {QStringLiteral("foundations"), QJsonObject{
            {QStringLiteral("F1"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Memory & Evidence Foundation")},
                {QStringLiteral("valid"), report.f1.valid},
                {QStringLiteral("ledgerIntact"), report.f1.ledgerIntact},
                {QStringLiteral("coldStartFresh"), report.f1.coldStartFresh},
                {QStringLiteral("totalEvents"), report.f1.totalEvents},
                {QStringLiteral("certificatesIntact"), report.f1.certificatesIntact}
            }},
            {QStringLiteral("F2"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Identity, Provenance & Trust Foundation")},
                {QStringLiteral("valid"), report.f2.valid},
                {QStringLiteral("allProvenanceValid"), report.f2.allProvenanceValid},
                {QStringLiteral("actorTaxonomyConsistent"), report.f2.actorTaxonomyConsistent},
                {QStringLiteral("trustBoundariesEnforced"), report.f2.trustBoundariesEnforced},
                {QStringLiteral("adminOverrideValid"), report.f2.adminOverrideValid}
            }},
            {QStringLiteral("F3"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Scope, State & Integrity Foundation")},
                {QStringLiteral("valid"), report.f3.valid},
                {QStringLiteral("scopeTaxonomyValid"), report.f3.scopeTaxonomyValid},
                {QStringLiteral("scopeCombinationsLegal"), report.f3.scopeCombinationsLegal},
                {QStringLiteral("projectIsolationValid"), report.f3.projectIsolationValid},
                {QStringLiteral("canonicalScopeCount"), report.f3.canonicalScopeCount}
            }},
            {QStringLiteral("F4"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Lifecycle & Certification Foundation")},
                {QStringLiteral("valid"), report.f4.valid},
                {QStringLiteral("namespaceVersionCorrect"), report.f4.namespaceVersionCorrect},
                {QStringLiteral("p6GatingCorrect"), report.f4.p6GatingCorrect},
                {QStringLiteral("certificationSemanticsValid"), report.f4.certificationSemanticsValid}
            }}
        }}
    };

    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open foundation-integration.json: %1").arg(file.errorString());
        return false;
    }
    const QByteArray content = QJsonDocument(evidence).toJson(QJsonDocument::Indented);
    if (file.write(content) != content.size()) {
        if (error) *error = QStringLiteral("Cannot write full content to foundation-integration.json: %1").arg(file.errorString());
        return false;
    }
    if (!file.commit()) {
        if (error) *error = QStringLiteral("Cannot commit foundation-integration.json: %1").arg(file.errorString());
        return false;
    }
    return true;
}

QJsonObject FoundationIntegrationService::readIntegrationEvidence(const QString& projectRoot, QString* error)
{
    const QString targetPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/verification/foundation-integration.json"));
    return readJsonFile(targetPath);
}

QJsonObject FoundationIntegrationService::allContracts()
{
    return QJsonObject{
        {QStringLiteral("F1"), MemoryEvidenceFoundation::contract()},
        {QStringLiteral("F2"), IdentityTrustFoundation::contract()},
        {QStringLiteral("F3"), ScopeIntegrityFoundation::contract()},
        {QStringLiteral("F4"), LifecycleCertificationFoundation::contract()}
    };
}

bool FoundationIntegrationService::verifyBootstrapOrder(QString* error)
{
    const auto f1 = MemoryEvidenceFoundation::contract();
    const auto f2 = IdentityTrustFoundation::contract();
    const auto f3 = ScopeIntegrityFoundation::contract();
    const auto f4 = LifecycleCertificationFoundation::contract();

    // Verify bootstrap order numbers are 1, 2, 3, 4
    if (f1.value(QStringLiteral("bootstrapOrder")).toInt() != 1
        || f2.value(QStringLiteral("bootstrapOrder")).toInt() != 2
        || f3.value(QStringLiteral("bootstrapOrder")).toInt() != 3
        || f4.value(QStringLiteral("bootstrapOrder")).toInt() != 4) {
        if (error) *error = QStringLiteral("Bootstrap order must be F1=1, F2=2, F3=3, F4=4");
        return false;
    }

    return true;
}

QJsonObject FoundationIntegrationService::dependencyMatrix()
{
    return QJsonObject{
        {QStringLiteral("processes"), QJsonObject{
            {QStringLiteral("P1"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Task Execution Governance")},
                {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2")}}
            }},
            {QStringLiteral("P2"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Context Coordination")},
                {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3")}}
            }},
            {QStringLiteral("P3"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Execution Orchestration")},
                {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3")}}
            }},
            {QStringLiteral("P4"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Predictive Task Optimization")},
                {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3")}}
            }},
            {QStringLiteral("P5"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Self-Adjusting Routing")},
                {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3")}}
            }},
            {QStringLiteral("P6"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Canonical Code Bank")},
                {QStringLiteral("gatedOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3"), QStringLiteral("F4")}}
            }}
        }},
        {QStringLiteral("foundations"), QJsonObject{
            {QStringLiteral("F1"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Memory & Evidence Foundation")},
                {QStringLiteral("dependsOn"), QJsonArray{}}
            }},
            {QStringLiteral("F2"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Identity, Provenance & Trust Foundation")},
                {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1")}}
            }},
            {QStringLiteral("F3"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Scope, State & Integrity Foundation")},
                {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2")}}
            }},
            {QStringLiteral("F4"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Lifecycle & Certification Foundation")},
                {QStringLiteral("dependsOn"), QJsonArray{QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3")}}
            }}
        }}
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// Foundation Certification Service Implementation
// ═══════════════════════════════════════════════════════════════════════════════

bool F1VerificationCheck::isPass(const QString& expectedRevision) const
{
    if (status != QStringLiteral("PASS")) return false;
    if (exitCode != 0) return false;
    if (sourceRevision.trimmed().isEmpty()) return false;
    if (!expectedRevision.isEmpty() && sourceRevision != expectedRevision) return false;
    if (evidenceFingerprint.trimmed().isEmpty()) return false;
    return true;
}

bool F1VerificationCheck::isPassWithEvidence(const QString& projectRoot,
                                              const QString& expectedRevision,
                                              QString* error) const
{
    F1CommandSpec spec;
    if (!FoundationCertificationService::canonicalCommandSpec(name, &spec, error)) {
        return false;
    }
    if (commandIdentity.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Check '%1' has empty commandIdentity.").arg(name);
        return false;
    }
    if (commandIdentity != spec.identity) {
        if (error) *error = QStringLiteral("Check '%1' commandIdentity '%2' does not match canonical identity '%3'.")
            .arg(name, commandIdentity, spec.identity);
        return false;
    }

    // All basic checks first
    if (status != QStringLiteral("PASS")) {
        if (error) *error = QStringLiteral("Check '%1' did not PASS: status is '%2'.").arg(name, status);
        return false;
    }
    if (exitCode != 0) {
        if (error) *error = QStringLiteral("Check '%1' did not PASS: exitCode is %2, not 0.").arg(name).arg(exitCode);
        return false;
    }
    if (sourceRevision.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Check '%1' has empty sourceRevision.").arg(name);
        return false;
    }
    if (!expectedRevision.isEmpty() && sourceRevision != expectedRevision) {
        if (error) *error = QStringLiteral("Check '%1' sourceRevision '%2' does not match expected '%3'.")
            .arg(name, sourceRevision, expectedRevision);
        return false;
    }
    if (evidenceFingerprint.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Check '%1' has empty evidenceFingerprint.").arg(name);
        return false;
    }

    // Evidence reference must be non-empty
    if (evidenceReference.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Check '%1' has empty evidenceReference.").arg(name);
        return false;
    }

    // Path traversal protection: must be within ARAMF_WORKER/certification/evidence/
    const QString normalizedRef = QDir::cleanPath(evidenceReference);
    if (normalizedRef.contains(QStringLiteral(".."))
        || !normalizedRef.startsWith(QStringLiteral("ARAMF_WORKER/certification/evidence/"))) {
        if (error) *error = QStringLiteral("Check '%1' evidenceReference '%2' is outside permitted evidence scope.")
            .arg(name, evidenceReference);
        return false;
    }

    // Physical file existence and readability
    const QString absPath = QDir(projectRoot).filePath(evidenceReference);
    QFile logFile(absPath);
    if (!logFile.exists()) {
        if (error) *error = QStringLiteral("Check '%1' referenced evidence file does not exist: %2")
            .arg(name, evidenceReference);
        return false;
    }
    if (!logFile.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Check '%1' referenced evidence file is not readable: %2")
            .arg(name, evidenceReference);
        return false;
    }
    const QByteArray fileBytes = logFile.readAll();
    logFile.close();

    // SHA-256 fingerprint verification
    const QString actualHash = QString::fromLatin1(
        QCryptographicHash::hash(fileBytes, QCryptographicHash::Sha256).toHex());
    if (actualHash != evidenceFingerprint) {
        if (error) *error = QStringLiteral("Check '%1' evidenceFingerprint mismatch: recorded '%2', actual file '%3'.")
            .arg(name, evidenceFingerprint, actualHash);
        return false;
    }

    return true;
}

QJsonObject F1VerificationCheck::toJson() const
{
    return QJsonObject{
        {QStringLiteral("name"), name},
        {QStringLiteral("command"), command},
        {QStringLiteral("commandIdentity"), commandIdentity},
        {QStringLiteral("status"), status},
        {QStringLiteral("exitCode"), exitCode},
        {QStringLiteral("timestamp"), timestamp},
        {QStringLiteral("sourceRevision"), sourceRevision},
        {QStringLiteral("evidenceReference"), evidenceReference},
        {QStringLiteral("evidenceFingerprint"), evidenceFingerprint}
    };
}

F1VerificationCheck F1VerificationCheck::fromJson(const QJsonObject& json)
{
    F1VerificationCheck c;
    c.name = json.value(QStringLiteral("name")).toString();
    c.command = json.value(QStringLiteral("command")).toString();
    c.commandIdentity = json.value(QStringLiteral("commandIdentity")).toString();
    c.status = json.value(QStringLiteral("status")).toString(QStringLiteral("FAIL"));
    c.exitCode = json.value(QStringLiteral("exitCode")).toInt(-1);
    c.timestamp = json.value(QStringLiteral("timestamp")).toString();
    c.sourceRevision = json.value(QStringLiteral("sourceRevision")).toString();
    c.evidenceReference = json.value(QStringLiteral("evidenceReference")).toString();
    c.evidenceFingerprint = json.value(QStringLiteral("evidenceFingerprint")).toString();
    return c;
}

QStringList F1CertificationEvidence::requiredCheckNames()
{
    return QStringList{
        QStringLiteral("f1-focused-suite"),
        QStringLiteral("foundation-namespace"),
        QStringLiteral("process-namespace-migration"),
        QStringLiteral("p1-governance"),
        QStringLiteral("p2-context"),
        QStringLiteral("p3-execution"),
        QStringLiteral("p4-predictive"),
        QStringLiteral("p5-routing"),
        QStringLiteral("provenance-and-scope"),
        QStringLiteral("f1-physical-validation"),
        QStringLiteral("memory-cold-start"),
        QStringLiteral("memory-consistency"),
        QStringLiteral("full-ctest")
    };
}

bool FoundationCertificationService::canonicalCommandSpec(const QString& checkName,
                                                          F1CommandSpec* spec,
                                                          QString* error)
{
    static const QList<F1CommandSpec> specs{
        {QStringLiteral("f1-focused-suite"), QStringLiteral("aramf_core_tests::--f1-certification"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--f1-certification")}},
        {QStringLiteral("foundation-namespace"), QStringLiteral("aramf_core_tests::--foundation-namespace"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--foundation-namespace")}},
        {QStringLiteral("process-namespace-migration"), QStringLiteral("aramf_core_tests::--process-migration"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--process-migration")}},
        {QStringLiteral("p1-governance"), QStringLiteral("aramf_core_tests::--p1-governance"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--p1-governance")}},
        {QStringLiteral("p2-context"), QStringLiteral("aramf_core_tests::--p2-context"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--p2-context")}},
        {QStringLiteral("p3-execution"), QStringLiteral("aramf_core_tests::--p3-execution"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--p3-execution")}},
        {QStringLiteral("p4-predictive"), QStringLiteral("aramf_core_tests::--p4-predictive"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--p4-predictive")}},
        {QStringLiteral("p5-routing"), QStringLiteral("aramf_core_tests::--p5-routing"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--p5-routing")}},
        {QStringLiteral("provenance-and-scope"), QStringLiteral("aramf_core_tests::--provenance-and-scope"),
         QStringLiteral("aramf_core_tests"), {QStringLiteral("--provenance-and-scope")}},
        {QStringLiteral("f1-physical-validation"), QStringLiteral("aramf::foundation f1-validate --project {PROJECT_ROOT}"),
         QStringLiteral("aramf"), {QStringLiteral("foundation"), QStringLiteral("f1-validate"),
                                   QStringLiteral("--project"), QStringLiteral("{PROJECT_ROOT}")}},
        {QStringLiteral("memory-cold-start"), QStringLiteral("aramf::memory cold-start --project {PROJECT_ROOT}"),
         QStringLiteral("aramf"), {QStringLiteral("memory"), QStringLiteral("cold-start"),
                                   QStringLiteral("--project"), QStringLiteral("{PROJECT_ROOT}")}},
        {QStringLiteral("memory-consistency"), QStringLiteral("aramf::memory validate --project {PROJECT_ROOT}"),
         QStringLiteral("aramf"), {QStringLiteral("memory"), QStringLiteral("validate"),
                                   QStringLiteral("--project"), QStringLiteral("{PROJECT_ROOT}")}},
        {QStringLiteral("full-ctest"), QStringLiteral("ctest::--test-dir {BUILD_DIR} --output-on-failure"),
         QStringLiteral("ctest"), {QStringLiteral("--test-dir"), QStringLiteral("{BUILD_DIR}"),
                                   QStringLiteral("--output-on-failure")}}
    };

    for (const auto& candidate : specs) {
        if (candidate.name == checkName) {
            if (spec) *spec = candidate;
            return true;
        }
    }
    if (error) *error = QStringLiteral("Unknown check name: %1").arg(checkName);
    return false;
}

void F1CertificationEvidence::updateDerivedFlags()
{
    f1FocusedPass = false;
    foundationNamespacePass = false;
    processMigrationPass = false;
    p1GovernancePass = false;
    p2ContextPass = false;
    p3ExecutionPass = false;
    p4PredictivePass = false;
    p5RoutingPass = false;
    provenanceAndScopePass = false;
    f1PhysicalValidationPass = false;
    memoryColdStartPass = false;
    memoryConsistencyPass = false;
    fullCTestPass = false;

    for (const auto& c : checks) {
        const bool pass = c.isPass(sourceRevision);
        if (c.name == QStringLiteral("f1-focused-suite")) f1FocusedPass = pass;
        else if (c.name == QStringLiteral("foundation-namespace")) foundationNamespacePass = pass;
        else if (c.name == QStringLiteral("process-namespace-migration")) processMigrationPass = pass;
        else if (c.name == QStringLiteral("p1-governance")) p1GovernancePass = pass;
        else if (c.name == QStringLiteral("p2-context")) p2ContextPass = pass;
        else if (c.name == QStringLiteral("p3-execution")) p3ExecutionPass = pass;
        else if (c.name == QStringLiteral("p4-predictive")) p4PredictivePass = pass;
        else if (c.name == QStringLiteral("p5-routing")) p5RoutingPass = pass;
        else if (c.name == QStringLiteral("provenance-and-scope")) provenanceAndScopePass = pass;
        else if (c.name == QStringLiteral("f1-physical-validation")) f1PhysicalValidationPass = pass;
        else if (c.name == QStringLiteral("memory-cold-start")) memoryColdStartPass = pass;
        else if (c.name == QStringLiteral("memory-consistency")) memoryConsistencyPass = pass;
        else if (c.name == QStringLiteral("full-ctest")) fullCTestPass = pass;
    }
}

bool F1CertificationEvidence::isComplete(const QString& expectedRevision, QString* error) const
{
    if (sourceRevision.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("F1 certification evidence has empty sourceRevision.");
        return false;
    }
    if (!expectedRevision.isEmpty() && sourceRevision != expectedRevision) {
        if (error) *error = QStringLiteral("Evidence sourceRevision '%1' does not match expected '%2'.")
            .arg(sourceRevision, expectedRevision);
        return false;
    }

    const auto req = requiredCheckNames();
    QSet<QString> presentNames;
    for (const auto& c : checks) {
        presentNames.insert(c.name);
        if (!c.isPass(sourceRevision)) {
            if (error) *error = QStringLiteral("Verification check '%1' did not PASS (status: %2, exitCode: %3, revision: %4).")
                .arg(c.name, c.status).arg(c.exitCode).arg(c.sourceRevision);
            return false;
        }
    }

    for (const auto& r : req) {
        if (!presentNames.contains(r)) {
            if (error) *error = QStringLiteral("Required verification check '%1' is missing from evidence.").arg(r);
            return false;
        }
    }

    if (!f1FocusedPass || !foundationNamespacePass || !processMigrationPass
        || !p1GovernancePass || !p2ContextPass || !p3ExecutionPass
        || !p4PredictivePass || !p5RoutingPass || !provenanceAndScopePass
        || !f1PhysicalValidationPass || !memoryColdStartPass || !memoryConsistencyPass
        || !fullCTestPass) {
        if (error) *error = QStringLiteral("F1 certification evidence is incomplete: all 13 required verification suites/checks must PASS.");
        return false;
    }

    return true;
}

bool F1CertificationEvidence::isCompleteWithEvidence(const QString& projectRoot,
                                                      const QString& expectedRevision,
                                                      QString* error) const
{
    if (sourceRevision.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("F1 certification evidence has empty sourceRevision.");
        return false;
    }
    if (!expectedRevision.isEmpty() && sourceRevision != expectedRevision) {
        if (error) *error = QStringLiteral("Evidence sourceRevision '%1' does not match expected '%2'.")
            .arg(sourceRevision, expectedRevision);
        return false;
    }

    const auto req = requiredCheckNames();
    const QSet<QString> requiredNames(req.cbegin(), req.cend());

    // Reject duplicate check names
    QSet<QString> presentNames;
    for (const auto& c : checks) {
        if (!requiredNames.contains(c.name)) {
            if (error) *error = QStringLiteral("Unexpected verification check name '%1' in evidence.").arg(c.name);
            return false;
        }
        if (presentNames.contains(c.name)) {
            if (error) *error = QStringLiteral("Duplicate verification check name '%1' in evidence.").arg(c.name);
            return false;
        }
        presentNames.insert(c.name);

        // Full evidence chain validation for each check
        QString checkErr;
        if (!c.isPassWithEvidence(projectRoot, sourceRevision, &checkErr)) {
            if (error) *error = checkErr;
            return false;
        }
    }

    if (checks.size() != req.size()) {
        if (error) *error = QStringLiteral("F1 certification evidence must contain exactly %1 required checks; found %2.")
            .arg(req.size()).arg(checks.size());
        return false;
    }

    for (const auto& r : req) {
        if (!presentNames.contains(r)) {
            if (error) *error = QStringLiteral("Required verification check '%1' is missing from evidence.").arg(r);
            return false;
        }
    }

    if (!f1FocusedPass || !foundationNamespacePass || !processMigrationPass
        || !p1GovernancePass || !p2ContextPass || !p3ExecutionPass
        || !p4PredictivePass || !p5RoutingPass || !provenanceAndScopePass
        || !f1PhysicalValidationPass || !memoryColdStartPass || !memoryConsistencyPass
        || !fullCTestPass) {
        if (error) *error = QStringLiteral("F1 certification evidence is incomplete: all 13 required verification suites/checks must PASS.");
        return false;
    }

    return true;
}

QJsonObject F1CertificationEvidence::toJson() const
{
    QJsonArray checksArray;
    for (const auto& c : checks) {
        checksArray.append(c.toJson());
    }

    return QJsonObject{
        {QStringLiteral("_file"), QStringLiteral("f1-evidence.json")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("foundation"), foundation},
        {QStringLiteral("foundationName"), foundationName},
        {QStringLiteral("foundationVersion"), foundationVersion},
        {QStringLiteral("sourceRevision"), sourceRevision},
        {QStringLiteral("verificationLevel"), verificationLevel},
        {QStringLiteral("timestamp"), timestamp.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : timestamp},
        {QStringLiteral("evidenceFingerprint"), evidenceFingerprint},
        {QStringLiteral("contract"), MemoryEvidenceFoundation::contract()},
        {QStringLiteral("results"), QJsonObject{
            {QStringLiteral("f1FocusedSuite"), f1FocusedPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("foundationNamespace"), foundationNamespacePass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("processNamespaceMigration"), processMigrationPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("p1Governance"), p1GovernancePass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("p2Context"), p2ContextPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("p3Execution"), p3ExecutionPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("p4Predictive"), p4PredictivePass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("p5Routing"), p5RoutingPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("provenanceAndScope"), provenanceAndScopePass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("f1PhysicalValidation"), f1PhysicalValidationPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("memoryColdStart"), memoryColdStartPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("memoryConsistency"), memoryConsistencyPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
            {QStringLiteral("fullCTest"), fullCTestPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}
        }},
        {QStringLiteral("checks"), checksArray},
        {QStringLiteral("knownLimitations"), knownLimitations.isEmpty() ? QJsonArray{
            QStringLiteral("F1 covers Memory & Evidence Foundation only; F2-F4 remain uncertified and in pre-certification state."),
            QStringLiteral("Integrated foundation validation (foundationIntegrationValid) remains false; P6 gating remains strictly BLOCKED.")
        } : knownLimitations},
        {QStringLiteral("overallStatus"), isComplete(sourceRevision) ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
        {QStringLiteral("rawDetails"), rawDetails}
    };
}

F1CertificationEvidence F1CertificationEvidence::fromJson(const QJsonObject& json)
{
    F1CertificationEvidence ev;
    ev.foundation = json.value(QStringLiteral("foundation")).toString(QStringLiteral("F1"));
    ev.foundationName = json.value(QStringLiteral("foundationName")).toString();
    ev.foundationVersion = json.value(QStringLiteral("foundationVersion")).toString(QStringLiteral("F1.1.2"));
    ev.sourceRevision = json.value(QStringLiteral("sourceRevision")).toString();
    ev.verificationLevel = json.value(QStringLiteral("verificationLevel")).toString(QStringLiteral("HOST_TEST"));
    ev.timestamp = json.value(QStringLiteral("timestamp")).toString();
    ev.evidenceFingerprint = json.value(QStringLiteral("evidenceFingerprint")).toString();
    ev.knownLimitations = json.value(QStringLiteral("knownLimitations")).toArray();
    ev.rawDetails = json.value(QStringLiteral("rawDetails")).toObject();

    const auto checksArray = json.value(QStringLiteral("checks")).toArray();
    for (const auto& val : checksArray) {
        if (val.isObject()) {
            ev.checks.append(F1VerificationCheck::fromJson(val.toObject()));
        }
    }

    if (!ev.checks.isEmpty()) {
        ev.updateDerivedFlags();
    } else {
        const auto results = json.value(QStringLiteral("results")).toObject();
        ev.f1FocusedPass = (results.value(QStringLiteral("f1FocusedSuite")).toString() == QStringLiteral("PASS"));
        ev.foundationNamespacePass = (results.value(QStringLiteral("foundationNamespace")).toString() == QStringLiteral("PASS"));
        ev.processMigrationPass = (results.value(QStringLiteral("processNamespaceMigration")).toString() == QStringLiteral("PASS"));
        ev.p1GovernancePass = (results.value(QStringLiteral("p1Governance")).toString() == QStringLiteral("PASS"));
        ev.p2ContextPass = (results.value(QStringLiteral("p2Context")).toString() == QStringLiteral("PASS"));
        ev.p3ExecutionPass = (results.value(QStringLiteral("p3Execution")).toString() == QStringLiteral("PASS"));
        ev.p4PredictivePass = (results.value(QStringLiteral("p4Predictive")).toString() == QStringLiteral("PASS"));
        ev.p5RoutingPass = (results.value(QStringLiteral("p5Routing")).toString() == QStringLiteral("PASS"));
        ev.provenanceAndScopePass = (results.value(QStringLiteral("provenanceAndScope")).toString() == QStringLiteral("PASS"));
        ev.f1PhysicalValidationPass = (results.value(QStringLiteral("f1PhysicalValidation")).toString() == QStringLiteral("PASS"));
        ev.memoryColdStartPass = (results.value(QStringLiteral("memoryColdStart")).toString() == QStringLiteral("PASS"));
        ev.memoryConsistencyPass = (results.value(QStringLiteral("memoryConsistency")).toString() == QStringLiteral("PASS"));
        ev.fullCTestPass = (results.value(QStringLiteral("fullCTest")).toString() == QStringLiteral("PASS"));
    }

    return ev;
}

static QString findExecutablePath(const QString& projectRoot, const QString& baseName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(baseName + QStringLiteral(".exe")),
        QDir(appDir).filePath(baseName),
        QDir(projectRoot).filePath(QStringLiteral("build/") + baseName + QStringLiteral(".exe")),
        QDir(projectRoot).filePath(QStringLiteral("build/") + baseName)
    };
    for (const auto& c : candidates) {
        if (QFile::exists(c)) return QDir::cleanPath(c);
    }
    const QString inPath = QStandardPaths::findExecutable(baseName);
    if (!inPath.isEmpty()) return inPath;
    return baseName;
}

bool FoundationCertificationService::verifyGitSourceRevision(const QString& projectRoot,
                                                            const QString& sourceRevision,
                                                            QString* error)
{
    const QString trimmed = sourceRevision.trimmed();
    if (trimmed.isEmpty()) {
        if (error) *error = QStringLiteral("sourceRevision is empty.");
        return false;
    }

    // 1. Verify git is available and projectRoot is a git repo by running git rev-parse HEAD
    QProcess gitHead;
    gitHead.setWorkingDirectory(projectRoot);
    gitHead.setProgram(QStringLiteral("git"));
    gitHead.setArguments({QStringLiteral("-C"), projectRoot, QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    gitHead.start();
    if (!gitHead.waitForFinished(5000) || gitHead.exitStatus() != QProcess::NormalExit || gitHead.exitCode() != 0) {
        if (error) *error = QStringLiteral("Failed to execute git rev-parse HEAD in project root.");
        return false;
    }
    const QString headSha = QString::fromLocal8Bit(gitHead.readAllStandardOutput()).trimmed();

    // 2. Verify sourceRevision is a real commit in repository
    QProcess gitCat;
    gitCat.setWorkingDirectory(projectRoot);
    gitCat.setProgram(QStringLiteral("git"));
    gitCat.setArguments({QStringLiteral("-C"), projectRoot, QStringLiteral("cat-file"), QStringLiteral("-e"), trimmed + QStringLiteral("^{commit}")});
    gitCat.start();
    if (!gitCat.waitForFinished(5000) || gitCat.exitStatus() != QProcess::NormalExit || gitCat.exitCode() != 0) {
        if (error) *error = QStringLiteral("sourceRevision '%1' is not a valid commit in the repository.").arg(trimmed);
        return false;
    }

    // 3. Resolve full commit SHA of sourceRevision
    QProcess gitRev;
    gitRev.setWorkingDirectory(projectRoot);
    gitRev.setProgram(QStringLiteral("git"));
    gitRev.setArguments({QStringLiteral("-C"), projectRoot, QStringLiteral("rev-parse"), trimmed + QStringLiteral("^{commit}")});
    gitRev.start();
    if (!gitRev.waitForFinished(5000) || gitRev.exitStatus() != QProcess::NormalExit || gitRev.exitCode() != 0) {
        if (error) *error = QStringLiteral("Failed to resolve commit SHA for '%1'.").arg(trimmed);
        return false;
    }
    const QString resolvedCommitSha = QString::fromLocal8Bit(gitRev.readAllStandardOutput()).trimmed();

    // 4. Verify sourceRevision matches HEAD
    if (resolvedCommitSha != headSha) {
        if (error) *error = QStringLiteral("sourceRevision '%1' (%2) does not match current HEAD (%3).")
            .arg(trimmed, resolvedCommitSha, headSha);
        return false;
    }

    // 5. Verify working tree is clean
    QProcess gitStatus;
    gitStatus.setWorkingDirectory(projectRoot);
    gitStatus.setProgram(QStringLiteral("git"));
    gitStatus.setArguments({QStringLiteral("-C"), projectRoot, QStringLiteral("status"), QStringLiteral("--porcelain=v1")});
    gitStatus.start();
    if (!gitStatus.waitForFinished(5000) || gitStatus.exitStatus() != QProcess::NormalExit || gitStatus.exitCode() != 0) {
        if (error) *error = QStringLiteral("Failed to check git status.");
        return false;
    }
    const QString statusOut = QString::fromLocal8Bit(gitStatus.readAllStandardOutput());
    if (!statusOut.trimmed().isEmpty()) {
        const QStringList statusLines = statusOut.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts);
        QStringList dirtyLines;
        for (const auto& line : statusLines) {
            const QString trimmedLine = line.trimmed();
            if (trimmedLine.isEmpty()) continue;
            const int spaceIdx = trimmedLine.indexOf(QLatin1Char(' '));
            if (spaceIdx < 0) continue;
            QString file = trimmedLine.mid(spaceIdx + 1).trimmed();
            if (file.contains(QStringLiteral(" -> "))) {
                file = file.section(QStringLiteral(" -> "), 1, 1).trimmed();
            }
            if (file.startsWith(QLatin1Char('"')) && file.endsWith(QLatin1Char('"'))) {
                file = file.mid(1, file.length() - 2);
            }
            file.replace(QLatin1Char('\\'), QLatin1Char('/'));
            // Permitted certification dirtiness: certification evidence, project
            // config updated during lifecycle transitions, and the two canonical
            // machine-generated validation outputs produced by the required
            // memory-cold-start and memory-consistency verification checks.
            if (file.startsWith(QStringLiteral("ARAMF_WORKER/certification/"))
                || file == QStringLiteral("ARAMF_WORKER.aramf.json")
                || file == QStringLiteral("ARAMF_WORKER/project.json")
                || file == QStringLiteral("ARAMF_WORKER/memory/cold-start-validation.json")
                || file == QStringLiteral("ARAMF_WORKER/memory/memory-consistency-validation.json")) {
                continue;
            }
            dirtyLines.append(line);
        }
        if (!dirtyLines.isEmpty()) {
            if (error) *error = QStringLiteral("Git working tree is dirty; certification requires a clean working tree:\n%1").arg(dirtyLines.join(QLatin1Char('\n')));
            return false;
        }
    }

    return true;
}

bool FoundationCertificationService::ensureCertificationArea(const QString& projectRoot, QString* error)
{
    const QString certDir = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification"));
    const QString evidDir = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence"));
    if (!QDir().mkpath(certDir) || !QDir().mkpath(evidDir)) {
        if (error) *error = QStringLiteral("Failed to create certification directories");
        return false;
    }
    const QString contractPath = QDir(certDir).filePath(QStringLiteral("certification-contract.json"));
    if (!QFile::exists(contractPath)) {
        const QJsonObject contractObject{
            {QStringLiteral("_file"), QStringLiteral("certification-contract.json")},
            {QStringLiteral("version"), 1},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("history"), QStringLiteral("certificates.jsonl is append-only; a retest always creates a new certificate ID and never rewrites prior certificates.")},
            {QStringLiteral("levels"), QJsonArray{QStringLiteral("BUILD_ONLY"), QStringLiteral("HOST_TEST"), QStringLiteral("SIMULATED"), QStringLiteral("RUNTIME"), QStringLiteral("ON_TARGET"), QStringLiteral("PHYSICAL"), QStringLiteral("GUI_END_TO_END"), QStringLiteral("HARDWARE_CERTIFIED")}},
            {QStringLiteral("passRule"), QStringLiteral("PASS requires all applicable required evidence to be present and verified. Missing physical or on-target evidence cannot produce HARDWARE_CERTIFIED PASS.")},
            {QStringLiteral("currentState"), QStringLiteral("current-certification-state.json resolves the latest certificate per subject; it is derived state, not the certificate history.")},
            {QStringLiteral("projectMemory"), QStringLiteral("CERTIFICATION_STARTED, CERTIFICATE_ISSUED, and CERTIFICATE_FAILED are also recorded in the Project Memory event log.")},
            {QStringLiteral("status"), QStringLiteral("PROJECT_STATUS.md may summarize current certification, while certificates.jsonl remains the durable evidence source.")},
            {QStringLiteral("evidence"), QStringLiteral("Evidence references must identify real persisted evidence. Agents must not fabricate test, build, runtime, physical, or hardware results.")},
            {QStringLiteral("certificateFields"), QJsonArray{
                QStringLiteral("certificateId"), QStringLiteral("certificateType"), QStringLiteral("subject"), QStringLiteral("scope"),
                QStringLiteral("requirements"), QStringLiteral("testMethod"), QStringLiteral("result"), QStringLiteral("verificationLevel"),
                QStringLiteral("environment"), QStringLiteral("targetPlatform"), QStringLiteral("hardwareConfiguration"),
                QStringLiteral("softwareBuildConfiguration"), QStringLiteral("sourceRevision"), QStringLiteral("buildResult"),
                QStringLiteral("testResult"), QStringLiteral("validationResult"), QStringLiteral("evidenceReferences"),
                QStringLiteral("limitations"), QStringLiteral("knownExclusions"), QStringLiteral("relatedProjectMemoryEvents"),
                QStringLiteral("previousCertificateId"), QStringLiteral("supersedesCertificateId")}}
        };
        QSaveFile cf(contractPath);
        if (cf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            const QByteArray data = QJsonDocument(contractObject).toJson(QJsonDocument::Indented);
            cf.write(data);
            cf.commit();
        }
    }
    const QString curCertPath = QDir(certDir).filePath(QStringLiteral("current-certification-state.json"));
    if (!QFile::exists(curCertPath)) {
        QSaveFile csf(curCertPath);
        if (csf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            const QByteArray data = QJsonDocument(QJsonObject{
                {QStringLiteral("_file"), QStringLiteral("current-certification-state.json")},
                {QStringLiteral("version"), 1},
                {QStringLiteral("subjects"), QJsonObject{}}
            }).toJson(QJsonDocument::Indented);
            csf.write(data);
            csf.commit();
        }
    }
    const QString certsPath = QDir(certDir).filePath(QStringLiteral("certificates.jsonl"));
    if (!QFile::exists(certsPath)) {
        QFile cfl(certsPath);
        if (cfl.open(QIODevice::WriteOnly | QIODevice::Text)) {
            cfl.close();
        }
    }
    return true;
}

bool FoundationCertificationService::writeEvidenceArtifact(const QString& projectRoot,
                                                           const F1CertificationEvidence& evidence,
                                                           QString* relativePath,
                                                           QString* sha256,
                                                           QString* error)
{
    return writeEvidenceArtifact(projectRoot, evidence, QString(), relativePath, sha256, error);
}

bool FoundationCertificationService::writeEvidenceArtifact(const QString& projectRoot,
                                                           const F1CertificationEvidence& evidence,
                                                           const QString& customRelativePath,
                                                           QString* relativePath,
                                                           QString* sha256,
                                                           QString* error)
{
    if (!ensureCertificationArea(projectRoot, error)) return false;

    QString rel = customRelativePath.trimmed();
    if (rel.isEmpty()) {
        const QString defaultRel = QStringLiteral("ARAMF_WORKER/certification/evidence/f1-evidence.json");
        const QString certsPath = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
        bool defaultBound = false;
        if (QFile::exists(certsPath)) {
            QFile cf(certsPath);
            if (cf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                while (!cf.atEnd()) {
                    const QByteArray line = cf.readLine();
                    if (line.contains("f1-evidence.json")) {
                        defaultBound = true;
                        break;
                    }
                }
            }
        }

        if (defaultBound) {
            int activeIteration = 0;
            const QString pjPath = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER.aramf.json"));
            if (QFile::exists(pjPath)) {
                ProjectModel pm;
                ProjectPersistence pp;
                if (pp.load(&pm, pjPath, nullptr) && pm.processVersionState().hasActiveProcess) {
                    activeIteration = pm.processVersionState().activeProcess.iteration;
                }
            }
            if (activeIteration > 1) {
                rel = QStringLiteral("ARAMF_WORKER/certification/evidence/f1-evidence-iteration-%1.json").arg(activeIteration);
            } else {
                rel = QStringLiteral("ARAMF_WORKER/certification/evidence/f1-evidence-rework.json");
            }
        } else {
            rel = defaultRel;
        }
    }

    const QString full = QFileInfo(rel).isAbsolute() ? rel : QDir(projectRoot).filePath(rel);
    QDir().mkpath(QFileInfo(full).path());
    QSaveFile file(full);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open %1 for writing: %2").arg(full, file.errorString());
        return false;
    }
    const QByteArray content = QJsonDocument(evidence.toJson()).toJson(QJsonDocument::Indented);
    if (file.write(content) != content.size()) {
        if (error) *error = QStringLiteral("Cannot write full content to %1: %2").arg(full, file.errorString());
        return false;
    }
    if (!file.commit()) {
        if (error) *error = QStringLiteral("Cannot commit %1: %2").arg(full, file.errorString());
        return false;
    }
    const QString actualRel = QDir(projectRoot).relativeFilePath(full);
    if (relativePath) *relativePath = actualRel;
    if (sha256) *sha256 = computeFileSha256(full);
    return true;
}

bool FoundationCertificationService::readEvidenceArtifact(const QString& artifactAbsolutePath,
                                                          F1CertificationEvidence* evidence,
                                                          QString* error)
{
    QFile file(artifactAbsolutePath);
    if (!file.exists()) {
        if (error) *error = QStringLiteral("Evidence artifact not found: %1").arg(artifactAbsolutePath);
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open evidence artifact: %1").arg(file.errorString());
        return false;
    }
    QJsonParseError parseErr;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("Malformed evidence artifact JSON: %1").arg(parseErr.errorString());
        return false;
    }
    if (evidence) *evidence = F1CertificationEvidence::fromJson(doc.object());
    return true;
}

F1VerificationCheck FoundationCertificationService::executeCheck(const QString& projectRoot,
                                                                const QString& checkName,
                                                                const QString& sourceRevision,
                                                                QString* error)
{
    F1VerificationCheck check;
    check.name = checkName;
    check.sourceRevision = sourceRevision;
    check.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    F1CommandSpec spec;
    if (!FoundationCertificationService::canonicalCommandSpec(checkName, &spec, error)) {
        check.status = QStringLiteral("FAIL");
        return check;
    }
    check.commandIdentity = spec.identity;

    QString program = findExecutablePath(projectRoot, spec.executable);
    QStringList args = spec.argumentTemplates;
    for (auto& arg : args) {
        arg.replace(QStringLiteral("{PROJECT_ROOT}"), projectRoot);
    }
    if (checkName == QStringLiteral("full-ctest")) {
        QString testDir = QDir(projectRoot).filePath(QStringLiteral("build"));
        if (!QFile::exists(QDir(testDir).filePath(QStringLiteral("CTestTestfile.cmake")))) {
            testDir = QCoreApplication::applicationDirPath();
        }
        args.replaceInStrings(QStringLiteral("{BUILD_DIR}"), testDir);
    }

    check.command = QStringLiteral("%1 %2").arg(program, args.join(QLatin1Char(' ')));

    QProcess proc;
    proc.setWorkingDirectory(projectRoot);
    proc.setProgram(program);
    proc.setArguments(args);
    proc.start();
    const bool finished = proc.waitForFinished(300000);
    const int exitCode = finished ? proc.exitCode() : -1;
    const QByteArray stdoutBytes = proc.readAllStandardOutput();
    const QByteArray stderrBytes = proc.readAllStandardError();
    const QByteArray outputBytes = stdoutBytes + stderrBytes;

    const QString checksDir = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence/checks"));
    QDir().mkpath(checksDir);

    const QString logRel = QStringLiteral("ARAMF_WORKER/certification/evidence/checks/%1.log").arg(checkName);
    const QString logPath = QDir(projectRoot).filePath(logRel);
    QSaveFile logFile(logPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || logFile.write(outputBytes) != outputBytes.size()
        || !logFile.commit()) {
        check.status = QStringLiteral("FAIL");
        check.exitCode = -1;
        check.evidenceReference.clear();
        check.evidenceFingerprint.clear();
        if (error) *error = QStringLiteral("Could not durably persist evidence log for check '%1'.")
            .arg(checkName);
        return check;
    }

    check.exitCode = exitCode;
    check.evidenceReference = logRel;
    check.evidenceFingerprint = computeFileSha256(logPath);
    if (check.evidenceFingerprint.isEmpty()) {
        check.status = QStringLiteral("FAIL");
        if (error) *error = QStringLiteral("Could not fingerprint persisted evidence log for check '%1'.")
            .arg(checkName);
        return check;
    }
    check.status = (finished && proc.exitStatus() == QProcess::NormalExit && exitCode == 0)
        ? QStringLiteral("PASS") : QStringLiteral("FAIL");

    const QString jsonRel = QStringLiteral("ARAMF_WORKER/certification/evidence/checks/%1.json").arg(checkName);
    const QString jsonPath = QDir(projectRoot).filePath(jsonRel);
    QSaveFile jsonFile(jsonPath);
    const QByteArray jData = QJsonDocument(check.toJson()).toJson(QJsonDocument::Indented);
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || jsonFile.write(jData) != jData.size()
        || !jsonFile.commit()) {
        check.status = QStringLiteral("FAIL");
        check.exitCode = -1;
        if (error) *error = QStringLiteral("Could not durably persist check record for '%1'.")
            .arg(checkName);
        return check;
    }

    if (check.status != QStringLiteral("PASS") && error) {
        *error = QStringLiteral("Check %1 failed with exit code %2").arg(checkName).arg(exitCode);
    }

    return check;
}

bool FoundationCertificationService::executeF1VerificationSuite(const QString& projectRoot,
                                                                const QString& sourceRevision,
                                                                F1CertificationEvidence* evidence,
                                                                QString* error)
{
    if (!evidence) return false;
    evidence->foundation = QStringLiteral("F1");
    evidence->foundationName = QStringLiteral("Memory & Evidence Foundation");
    evidence->foundationVersion = QStringLiteral("F1.1.3");
    evidence->sourceRevision = sourceRevision;
    evidence->verificationLevel = QStringLiteral("HOST_TEST");
    evidence->timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    evidence->checks.clear();

    const QString checksDir = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence/checks"));
    const auto req = F1CertificationEvidence::requiredCheckNames();
    QStringList failedChecks;

    for (const auto& name : req) {
        const QString checkJsonPath = QDir(checksDir).filePath(name + QStringLiteral(".json"));
        bool reused = false;
        if (QFile::exists(checkJsonPath)) {
            QFile f(checkJsonPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const auto doc = QJsonDocument::fromJson(f.readAll()).object();
                f.close();
                const auto existingCheck = F1VerificationCheck::fromJson(doc);
                if (existingCheck.isPassWithEvidence(projectRoot, sourceRevision, nullptr)) {
                    evidence->checks.append(existingCheck);
                    reused = true;
                }
            }
        }
        if (!reused) {
            QString chkErr;
            const auto check = executeCheck(projectRoot, name, sourceRevision, &chkErr);
            evidence->checks.append(check);
            if (!check.isPass(sourceRevision)) {
                failedChecks.append(name);
            }
        }
    }

    evidence->updateDerivedFlags();

    if (!failedChecks.isEmpty()) {
        if (error) *error = QStringLiteral("Failed verification checks: %1").arg(failedChecks.join(QStringLiteral(", ")));
        return false;
    }
    return evidence->isCompleteWithEvidence(projectRoot, sourceRevision, error);
}

bool FoundationCertificationService::loadVerificationChecks(const QString& checksDirectory,
                                                            const QString& expectedRevision,
                                                            QList<F1VerificationCheck>* checks,
                                                            QString* error)
{
    if (!checks) return false;
    checks->clear();
    QDir dir(checksDirectory);
    if (!dir.exists()) {
        if (error) *error = QStringLiteral("Checks directory does not exist: %1").arg(checksDirectory);
        return false;
    }

    QDir projectDir(checksDirectory);
    if (!projectDir.cdUp() || !projectDir.cdUp() || !projectDir.cdUp() || !projectDir.cdUp()) {
        if (error) *error = QStringLiteral("Cannot resolve project root from checks directory: %1").arg(checksDirectory);
        return false;
    }
    const QString projectRoot = projectDir.absolutePath();
    const auto req = F1CertificationEvidence::requiredCheckNames();
    for (const auto& name : req) {
        const QString checkFilePath = dir.filePath(name + QStringLiteral(".json"));
        if (!QFile::exists(checkFilePath)) {
            if (error) *error = QStringLiteral("Missing check record file: %1").arg(checkFilePath);
            return false;
        }
        QFile f(checkFilePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error) *error = QStringLiteral("Cannot open check record file: %1").arg(checkFilePath);
            return false;
        }
        QJsonParseError parseErr;
        const auto doc = QJsonDocument::fromJson(f.readAll(), &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error) *error = QStringLiteral("Malformed check record JSON in %1").arg(checkFilePath);
            return false;
        }
        const auto check = F1VerificationCheck::fromJson(doc.object());
        QString checkErr;
        if (!check.isPassWithEvidence(projectRoot, expectedRevision, &checkErr)) {
            if (error) *error = checkErr;
            return false;
        }
        checks->append(check);
    }
    return true;
}

bool FoundationCertificationService::startF1(const QString& projectRoot,
                                            const QString& projectFilePath,
                                            QString* error)
{
    const QString resolved = QFileInfo(projectFilePath).isAbsolute()
        ? projectFilePath : QDir(projectRoot).filePath(projectFilePath);
    ProjectModel model;
    ProjectPersistence persistence;
    if (!persistence.load(&model, resolved, error)) return false;

    if (model.processVersionState().hasActiveProcess) {
        const auto& act = model.processVersionState().activeProcess;
        if (act.isFoundation() && act.foundationNumber() == 1) {
            return true; // already active F1
        }
        if (error) *error = QStringLiteral("An active process already exists: %1").arg(model.processVersionState().activeIdentifier());
        return false;
    }

    if (!model.processVersionState().hasNextProcess
        || !model.processVersionState().nextProcess.isFoundation()
        || model.processVersionState().nextProcess.foundationNumber() != 1) {
        if (error) *error = QStringLiteral("Next process is not F1 (found: %1)").arg(model.processVersionState().nextIdentifier());
        return false;
    }

    if (!model.startNextProcess(error)) return false;
    if (!persistence.save(model, resolved, error)) return false;
    if (!synchronizeProjectJson(projectRoot, model, error)) return false;
    return true;
}

bool FoundationCertificationService::reworkF1(const QString& projectRoot,
                                              const QString& projectFilePath,
                                              QString* error)
{
    const QString resolved = QFileInfo(projectFilePath).isAbsolute()
        ? projectFilePath : QDir(projectRoot).filePath(projectFilePath);
    ProjectModel model;
    ProjectPersistence persistence;
    if (!persistence.load(&model, resolved, error)) return false;

    if (!model.reworkCompletedFoundation(1, error)) {
        return false;
    }
    if (!persistence.save(model, resolved, error)) {
        return false;
    }
    if (!synchronizeProjectJson(projectRoot, model, error)) {
        return false;
    }
    return true;
}

bool FoundationCertificationService::certifyF1(const QString& projectRoot,
                                              const QString& projectFilePath,
                                              const QString& sourceRevision,
                                              const QString& evidenceArtifactPath,
                                              QJsonObject* issuedCertificate,
                                              QString* error)
{
    // 1. Validate Git source revision preconditions
    if (!verifyGitSourceRevision(projectRoot, sourceRevision, error)) {
        return false;
    }

    const QString resolved = QFileInfo(projectFilePath).isAbsolute()
        ? projectFilePath : QDir(projectRoot).filePath(projectFilePath);
    ProjectModel model;
    ProjectPersistence persistence;
    if (!persistence.load(&model, resolved, error)) return false;

    // Namespace check: CanonicalV2 required
    if (model.processVersionState().namespaceVersion != static_cast<int>(ProcessNamespace::CanonicalV2)) {
        if (error) *error = QStringLiteral("Lifecycle namespace must be CanonicalV2.");
        return false;
    }

    // foundationIntegrationValid must remain false
    if (model.processVersionState().foundationIntegrationValid) {
        if (error) *error = QStringLiteral("foundationIntegrationValid must remain false prior to all-foundation certification.");
        return false;
    }

    // Fail-closed consistency check between persisted and generated lifecycle states
    const QString genPath = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/project.json"));
    if (!QFile::exists(genPath)) {
        if (error) *error = QStringLiteral("Generated project configuration (ARAMF_WORKER/project.json) does not exist.");
        return false;
    }
    QFile gf(genPath);
    if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open ARAMF_WORKER/project.json: %1").arg(gf.errorString());
        return false;
    }
    QJsonParseError parseErr;
    const auto gDoc = QJsonDocument::fromJson(gf.readAll(), &parseErr).object();
    gf.close();
    if (parseErr.error != QJsonParseError::NoError || !gDoc.contains(QStringLiteral("processVersion"))) {
        if (error) *error = QStringLiteral("ARAMF_WORKER/project.json is malformed or missing processVersion.");
        return false;
    }
    ProcessVersionState gState;
    QString gErr;
    if (!processVersionStateFromJson(gDoc.value(QStringLiteral("processVersion")), &gState, &gErr)) {
        if (error) *error = QStringLiteral("Cannot parse processVersion from ARAMF_WORKER/project.json: %1").arg(gErr);
        return false;
    }
    if (processVersionStateToJson(model.processVersionState()) != processVersionStateToJson(gState)) {
        if (error) *error = QStringLiteral("Persisted and generated lifecycle states are not synchronized.");
        return false;
    }
    if (!gState.isValid(error)) {
        return false;
    }

    // Lifecycle position check:
    if (!model.processVersionState().hasActiveProcess) {
        if (model.processVersionState().nextIdentifier() == QStringLiteral("F1.1.0.0.0")) {
            if (!startF1(projectRoot, resolved, error)) return false;
            if (!persistence.load(&model, resolved, error)) return false;
        } else if (model.processVersionState().isFoundationComplete(1)) {
            if (error) *error = QStringLiteral("F1 is already completed; call reworkF1 before certifying a new iteration.");
            return false;
        } else {
            if (error) *error = QStringLiteral("Lifecycle position '%1' is not valid for F1 certification.")
                .arg(model.processVersionState().nextIdentifier());
            return false;
        }
    }

    // Active process must be an in-progress F1 foundation (e.g. F1.1.1.0.0 or F1.1.2.0.0)
    const auto& act = model.processVersionState().activeProcess;
    if (!act.isFoundation() || act.foundationNumber() != 1 || act.done != 0) {
        if (error) *error = QStringLiteral("Active process '%1' is not an in-progress F1 foundation.")
            .arg(model.processVersionState().activeIdentifier());
        return false;
    }

    // 2. Physical evidence validation: MemoryEvidenceFoundation::validate must pass
    const auto memRep = MemoryEvidenceFoundation::validate(projectRoot, error);
    if (!memRep.valid) {
        if (error && error->isEmpty()) *error = QStringLiteral("MemoryEvidenceFoundation physical evidence validation failed.");
        return false;
    }

    // 3. Evidence artifact validation
    QString artPath;
    if (!evidenceArtifactPath.trimmed().isEmpty()) {
        artPath = QFileInfo(evidenceArtifactPath).isAbsolute()
            ? evidenceArtifactPath : QDir(projectRoot).filePath(evidenceArtifactPath);
    } else {
        if (act.iteration > 1) {
            const QString iterPath = QDir(projectRoot).filePath(
                QStringLiteral("ARAMF_WORKER/certification/evidence/f1-evidence-iteration-%1.json").arg(act.iteration));
            const QString reworkPath = QDir(projectRoot).filePath(
                QStringLiteral("ARAMF_WORKER/certification/evidence/f1-evidence-rework.json"));
            if (QFile::exists(iterPath)) {
                artPath = iterPath;
            } else if (QFile::exists(reworkPath)) {
                artPath = reworkPath;
            }
        }
        if (artPath.isEmpty()) {
            artPath = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence/f1-evidence.json"));
        }
    }

    F1CertificationEvidence ev;
    if (!readEvidenceArtifact(artPath, &ev, error)) return false;

    if (ev.foundation != QStringLiteral("F1")) {
        if (error) *error = QStringLiteral("Evidence artifact foundation '%1' does not match F1.").arg(ev.foundation);
        return false;
    }
    if (ev.sourceRevision != sourceRevision) {
        if (error) *error = QStringLiteral("Evidence artifact sourceRevision '%1' does not match requested '%2'.")
            .arg(ev.sourceRevision, sourceRevision);
        return false;
    }
    if (!ev.isCompleteWithEvidence(projectRoot, sourceRevision, error)) {
        if (error && error->isEmpty()) *error = QStringLiteral("F1 certification evidence is incomplete: all 13 required verification suites/checks must PASS.");
        return false;
    }

    const QString artSha256 = computeFileSha256(artPath);
    if (artSha256.isEmpty()) {
        if (error) *error = QStringLiteral("Could not compute SHA-256 fingerprint of evidence artifact.");
        return false;
    }
    const QString artRelPath = QDir(projectRoot).relativeFilePath(artPath);

    // 4. CertificationService: check for existing certificate to supersede
    if (!ensureCertificationArea(projectRoot, error)) return false;
    CertificationService certService;

    QJsonObject priorCert;
    QString priorCertId;
    if (certService.latestForSubject(projectRoot, QStringLiteral("F1"), &priorCert)) {
        priorCertId = priorCert.value(QStringLiteral("certificateId")).toString();
    }

    const QString fVersion = QStringLiteral("F1.1.%1").arg(act.iteration);
    const QJsonArray requirements{ QStringLiteral("f1-evidence-artifact") };
    const QJsonObject context{
        {QStringLiteral("foundation"), QStringLiteral("F1")},
        {QStringLiteral("foundationName"), QStringLiteral("Memory & Evidence Foundation")},
        {QStringLiteral("foundationVersion"), fVersion},
        {QStringLiteral("iteration"), act.iteration},
        {QStringLiteral("sourceRevision"), sourceRevision},
        {QStringLiteral("verificationLevel"), QStringLiteral("HOST_TEST")},
        {QStringLiteral("evidenceArtifact"), artRelPath},
        {QStringLiteral("evidenceFingerprint"), artSha256}
    };

    QJsonObject startedCert;
    if (!certService.start(projectRoot, QStringLiteral("F1"), QStringLiteral("FOUNDATION"),
                           QStringLiteral("project"), QStringLiteral("HOST_TEST"),
                           requirements, context, &startedCert, error)) {
        return false;
    }

    // Bind exact fields to certificate
    startedCert.insert(QStringLiteral("sourceRevision"), sourceRevision);
    startedCert.insert(QStringLiteral("foundationVersion"), fVersion);
    startedCert.insert(QStringLiteral("iteration"), act.iteration);
    startedCert.insert(QStringLiteral("foundationName"), QStringLiteral("Memory & Evidence Foundation"));
    startedCert.insert(QStringLiteral("evidenceArtifact"), artRelPath);
    startedCert.insert(QStringLiteral("evidenceFingerprint"), artSha256);
    if (!priorCertId.isEmpty()) {
        startedCert.insert(QStringLiteral("previousCertificateId"), priorCertId);
        startedCert.insert(QStringLiteral("supersedesCertificateId"), priorCertId);
    }

    const QJsonArray evidenceRefs{
        QJsonObject{
            {QStringLiteral("reference"), artRelPath},
            {QStringLiteral("fingerprint"), artSha256},
            {QStringLiteral("verified"), true},
            {QStringLiteral("type"), QStringLiteral("HOST_TEST")}
        }
    };

    QJsonObject issuedCert;
    if (!certService.issue(projectRoot, startedCert, QStringLiteral("PASS"), evidenceRefs, &issuedCert, error)) {
        return false;
    }

    // 5. Verify issued certificate is durable and rediscoverable
    QJsonObject foundCert;
    if (!certService.latestForSubject(projectRoot, QStringLiteral("F1"), &foundCert, error)) {
        if (error && error->isEmpty()) *error = QStringLiteral("Failed to rediscover issued F1 certificate.");
        return false;
    }
    if (foundCert.value(QStringLiteral("result")).toString() != QStringLiteral("PASS")
        || foundCert.value(QStringLiteral("certificationStatus")).toString() != QStringLiteral("CERTIFIED")
        || !foundCert.value(QStringLiteral("evidenceComplete")).toBool()
        || foundCert.value(QStringLiteral("sourceRevision")).toString() != sourceRevision) {
        if (error) *error = QStringLiteral("Rediscovered F1 certificate does not satisfy PASS/CERTIFIED/evidenceComplete criteria.");
        return false;
    }

    // Verify certificate in ledger
    const auto allCerts = certService.certificates(projectRoot, error);
    bool inLedger = false;
    for (const auto& c : allCerts) {
        if (c.value(QStringLiteral("certificateId")).toString() == issuedCert.value(QStringLiteral("certificateId")).toString()) {
            inLedger = true;
            break;
        }
    }
    if (!inLedger) {
        if (error) *error = QStringLiteral("Issued certificate not found in certificates.jsonl.");
        return false;
    }

    // Verify current certification state has F1 pointing to this certificate
    const auto curCertState = certService.currentState(projectRoot, error);
    const auto subjObj = curCertState.value(QStringLiteral("subjects")).toObject().value(QStringLiteral("F1")).toObject();
    if (subjObj.value(QStringLiteral("certificateId")).toString() != issuedCert.value(QStringLiteral("certificateId")).toString()) {
        if (error) *error = QStringLiteral("Current certification state does not point to the newly issued F1 certificate.");
        return false;
    }

    // 6. Perform lifecycle certification: certifyCurrentProcessIteration
    if (!model.certifyCurrentProcessIteration(error)) {
        return false;
    }
    if (!persistence.save(model, resolved, error)) {
        return false;
    }
    if (!synchronizeProjectJson(projectRoot, model, error)) {
        return false;
    }

    const auto& newAct = model.processVersionState().activeProcess;
    if (!newAct.isFoundation() || newAct.foundationNumber() != 1 || newAct.certification != 1 || newAct.done != 0) {
        if (error) *error = QStringLiteral("Active process is not certified (cert=1, done=0) after certification.");
        return false;
    }

    if (issuedCertificate) *issuedCertificate = issuedCert;
    return true;
}

bool FoundationCertificationService::completeF1(const QString& projectRoot,
                                                const QString& projectFilePath,
                                                QString* error)
{
    const QString resolved = QFileInfo(projectFilePath).isAbsolute()
        ? projectFilePath : QDir(projectRoot).filePath(projectFilePath);
    ProjectModel model;
    ProjectPersistence persistence;
    if (!persistence.load(&model, resolved, error)) return false;

    if (!model.processVersionState().hasActiveProcess) {
        if (error) *error = QStringLiteral("No active process to complete.");
        return false;
    }

    const auto& act = model.processVersionState().activeProcess;
    if (!act.isFoundation() || act.foundationNumber() != 1) {
        if (error) *error = QStringLiteral("Active process '%1' is not F1.").arg(model.processVersionState().activeIdentifier());
        return false;
    }

    if (act.certification != 1) {
        if (error) *error = QStringLiteral("F1 is not certified (cert=0); cannot complete uncertified.");
        return false;
    }

    // Post-certification physical validation must pass
    const auto memRep = MemoryEvidenceFoundation::validate(projectRoot, error);
    if (!memRep.valid || !memRep.certificatesIntact) {
        if (error && error->isEmpty()) *error = QStringLiteral("Post-certification physical evidence validation failed.");
        return false;
    }

    // Certificate must exist and be PASS
    CertificationService certService;
    QJsonObject latestCert;
    if (!certService.latestForSubject(projectRoot, QStringLiteral("F1"), &latestCert, error)) {
        if (error && error->isEmpty()) *error = QStringLiteral("No F1 certificate found for completion.");
        return false;
    }
    if (latestCert.value(QStringLiteral("result")).toString() != QStringLiteral("PASS")
        || latestCert.value(QStringLiteral("certificationStatus")).toString() != QStringLiteral("CERTIFIED")) {
        if (error) *error = QStringLiteral("F1 certificate is not PASS/CERTIFIED.");
        return false;
    }

    if (!model.completeActiveProcess(error)) return false;
    if (!persistence.save(model, resolved, error)) return false;
    if (!synchronizeProjectJson(projectRoot, model, error)) return false;

    // Verify final state invariants
    const auto& pvState = model.processVersionState();
    if (!pvState.isFoundationComplete(1)) {
        if (error) *error = QStringLiteral("F1 is not marked complete in history.");
        return false;
    }
    if (pvState.hasActiveProcess) {
        if (error) *error = QStringLiteral("Active process is not null after completion.");
        return false;
    }
    if (pvState.nextIdentifier() != QStringLiteral("F2.1.0.0.0")) {
        if (error) *error = QStringLiteral("Next process is not F2.1.0.0.0 (found: %1).").arg(pvState.nextIdentifier());
        return false;
    }
    if (pvState.isFoundationComplete(2)) {
        if (error) *error = QStringLiteral("F2 must not be completed.");
        return false;
    }
    if (pvState.foundationIntegrationValid) {
        if (error) *error = QStringLiteral("foundationIntegrationValid must remain false.");
        return false;
    }
    QString p6Reason;
    if (pvState.isP6Eligible(&p6Reason)) {
        if (error) *error = QStringLiteral("P6 must remain blocked.");
        return false;
    }

    return true;
}

bool FoundationCertificationService::synchronizeProjectJson(const QString& projectRoot,
                                                           const ProjectModel& model,
                                                           QString* error)
{
    return updateProjectConfiguration(projectRoot, model, error);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CLI Runner
// ═══════════════════════════════════════════════════════════════════════════════

int runFoundationCommand(const QStringList& arguments, QTextStream& output, QTextStream& error)
{
    QString projectRoot = QStringLiteral(".");
    QString subcommand;
    QString foundation = QStringLiteral("F1");
    QString projectFile = QStringLiteral("ARAMF_WORKER.aramf.json");
    QString sourceRevision;
    QString evidencePath;

    for (int i = 1; i < arguments.size(); ++i) {
        if (arguments[i] == QStringLiteral("--project") && i + 1 < arguments.size()) {
            projectRoot = arguments[++i];
        } else if (arguments[i] == QStringLiteral("--file") && i + 1 < arguments.size()) {
            projectFile = arguments[++i];
        } else if (arguments[i] == QStringLiteral("--foundation") && i + 1 < arguments.size()) {
            foundation = arguments[++i];
        } else if (arguments[i] == QStringLiteral("--source-revision") && i + 1 < arguments.size()) {
            sourceRevision = arguments[++i];
        } else if (arguments[i] == QStringLiteral("--evidence") && i + 1 < arguments.size()) {
            evidencePath = arguments[++i];
        } else if (arguments[i] == QStringLiteral("--help")) {
            output << "Usage: aramf foundation <validate|status|integration|report|certify|complete|start|rework|write-f1-evidence> [--project <path>] [--file <file>] [--foundation <name>] [--source-revision <sha>] [--evidence <path>]\n";
            return 0;
        } else if (subcommand.isEmpty() && !arguments[i].startsWith(QLatin1Char('-'))) {
            subcommand = arguments[i];
        }
    }

    if (subcommand.isEmpty() || subcommand == QStringLiteral("status")) {
        const auto pvSummary = LifecycleCertificationFoundation::lifecycleSummary(projectRoot);
        const auto completed = pvSummary.value(QStringLiteral("completedIdentifiers")).toArray();
        auto isFoundationCertified = [&completed](int num) -> bool {
            const QString prefix = QStringLiteral("F%1.").arg(num);
            for (const auto& item : completed) {
                if (item.toString().startsWith(prefix) && item.toString().endsWith(QStringLiteral(".1.1")))
                    return true;
            }
            return false;
        };
        output << "=== ARAMF Foundation Status ===\n";
        output << "F1 (Memory & Evidence): " << (isFoundationCertified(1) ? "Certified" : "Ready (Pre-Certification)") << "\n";
        output << "F2 (Identity, Provenance & Trust): " << (isFoundationCertified(2) ? "Certified" : "Ready (Pre-Certification)") << "\n";
        output << "F3 (Scope, State & Integrity): " << (isFoundationCertified(3) ? "Certified" : "Ready (Pre-Certification)") << "\n";
        output << "F4 (Lifecycle & Certification): " << (isFoundationCertified(4) ? "Certified" : "Ready (Pre-Certification)") << "\n";
        output << "P6 Gating: " << (pvSummary.value(QStringLiteral("p6Eligible")).toBool() ? "ELIGIBLE" : "BLOCKED") << "\n";
        output << "P6 Reason: " << pvSummary.value(QStringLiteral("p6Reason")).toString() << "\n";
        return 0;
    }

    if (subcommand == QStringLiteral("rework")) {
        if (foundation != QStringLiteral("F1")) {
            error << "error=Foundation rework is only supported for F1 (requested: " << foundation << ")\n";
            return 2;
        }
        QString rewErr;
        if (!FoundationCertificationService::reworkF1(projectRoot, projectFile, &rewErr)) {
            error << "error=" << rewErr << "\n";
            return 1;
        }
        ProjectModel m;
        ProjectPersistence p;
        const QString resolved = QFileInfo(projectFile).isAbsolute() ? projectFile : QDir(projectRoot).filePath(projectFile);
        p.load(&m, resolved, nullptr);
        output << "=== ARAMF Foundation Rework ===\n";
        output << "Active Identifier: " << m.processVersionState().activeIdentifier() << "\n";
        output << "Next Identifier: " << m.processVersionState().nextIdentifier() << "\n";
        return 0;
    }

    if (subcommand == QStringLiteral("certify")) {
        if (foundation != QStringLiteral("F1")) {
            error << "error=Foundation certification is only supported for F1 (requested: " << foundation << ")\n";
            return 2;
        }
        QJsonObject issuedCert;
        QString certErr;
        if (!FoundationCertificationService::certifyF1(projectRoot, projectFile, sourceRevision, evidencePath, &issuedCert, &certErr)) {
            error << "error=" << certErr << "\n";
            return 1;
        }
        ProjectModel m;
        ProjectPersistence p;
        const QString resolved = QFileInfo(projectFile).isAbsolute() ? projectFile : QDir(projectRoot).filePath(projectFile);
        p.load(&m, resolved, nullptr);
        output << "=== ARAMF Foundation Certification ===\n";
        output << "Foundation: F1\n";
        output << "Foundation Name: Memory & Evidence Foundation\n";
        output << "Foundation Version: " << issuedCert.value(QStringLiteral("foundationVersion")).toString() << "\n";
        output << "Source Revision: " << sourceRevision << "\n";
        output << "Certificate ID: " << issuedCert.value(QStringLiteral("certificateId")).toString() << "\n";
        if (issuedCert.contains(QStringLiteral("supersedesCertificateId"))) {
            output << "Supersedes Certificate ID: " << issuedCert.value(QStringLiteral("supersedesCertificateId")).toString() << "\n";
        }
        output << "Certification Status: " << issuedCert.value(QStringLiteral("certificationStatus")).toString() << "\n";
        output << "Result: " << issuedCert.value(QStringLiteral("result")).toString() << "\n";
        output << "Evidence Complete: " << (issuedCert.value(QStringLiteral("evidenceComplete")).toBool() ? "true" : "false") << "\n";
        output << "Active Identifier: " << m.processVersionState().activeIdentifier() << "\n";
        output << "Next Identifier: " << m.processVersionState().nextIdentifier() << "\n";
        return 0;
    }

    if (subcommand == QStringLiteral("complete")) {
        if (foundation != QStringLiteral("F1")) {
            error << "error=Foundation completion is only supported for F1 (requested: " << foundation << ")\n";
            return 2;
        }
        QString compErr;
        if (!FoundationCertificationService::completeF1(projectRoot, projectFile, &compErr)) {
            error << "error=" << compErr << "\n";
            return 1;
        }
        ProjectModel m;
        ProjectPersistence p;
        const QString resolved = QFileInfo(projectFile).isAbsolute() ? projectFile : QDir(projectRoot).filePath(projectFile);
        p.load(&m, resolved, nullptr);
        const auto& hist = m.processVersionState().completedHistory;
        const QString compId = (!hist.isEmpty()) ? hist.last().identifier() : QStringLiteral("none");
        output << "=== ARAMF Foundation Completion ===\n";
        output << "Foundation: F1\n";
        output << "Completed Identifier: " << compId << "\n";
        output << "Active Identifier: " << (m.processVersionState().hasActiveProcess ? m.processVersionState().activeIdentifier() : QStringLiteral("none")) << "\n";
        output << "Next Identifier: " << m.processVersionState().nextIdentifier() << "\n";
        output << "P6 Gating: BLOCKED\n";
        return 0;
    }

    if (subcommand == QStringLiteral("start")) {
        if (foundation != QStringLiteral("F1")) {
            error << "error=Foundation start is only supported for F1 (requested: " << foundation << ")\n";
            return 2;
        }
        QString startErr;
        if (!FoundationCertificationService::startF1(projectRoot, projectFile, &startErr)) {
            error << "error=" << startErr << "\n";
            return 1;
        }
        ProjectModel m;
        ProjectPersistence p;
        const QString resolved = QFileInfo(projectFile).isAbsolute() ? projectFile : QDir(projectRoot).filePath(projectFile);
        p.load(&m, resolved, nullptr);
        output << "=== ARAMF Foundation Start ===\n";
        output << "Active Identifier: " << m.processVersionState().activeIdentifier() << "\n";
        output << "Next Identifier: " << m.processVersionState().nextIdentifier() << "\n";
        return 0;
    }

    if (subcommand == QStringLiteral("write-f1-evidence")) {
        if (sourceRevision.trimmed().isEmpty()) {
            error << "error=--source-revision is required to write evidence artifact\n";
            return 2;
        }
        F1CertificationEvidence ev;
        QString suiteErr;
        const bool suiteOk = FoundationCertificationService::executeF1VerificationSuite(projectRoot, sourceRevision, &ev, &suiteErr);
        ev.evidenceFingerprint = MemoryEvidenceFoundation::evidenceFingerprint(projectRoot);

        QString relPath, sha;
        QString wErr;
        if (!FoundationCertificationService::writeEvidenceArtifact(projectRoot, ev, evidencePath, &relPath, &sha, &wErr)) {
            error << "error=" << wErr << "\n";
            return 1;
        }
        output << "=== ARAMF F1 Evidence Written ===\n";
        output << "Artifact Path: " << relPath << "\n";
        output << "Fingerprint: " << sha << "\n";
        output << "Source Revision: " << sourceRevision << "\n";
        output << "Overall Status: "
               << (ev.isCompleteWithEvidence(projectRoot, sourceRevision, nullptr) ? "PASS" : "FAIL")
               << "\n";
        if (!suiteOk) {
            error << "Verification suite had failures: " << suiteErr << "\n";
            return 1;
        }
        return 0;
    }

    if (subcommand == QStringLiteral("f1-validate")) {
        QString valErr;
        const auto r = MemoryEvidenceFoundation::validate(projectRoot, &valErr);
        output << "=== ARAMF F1 Physical Evidence Validation ===\n";
        output << "F1-VALIDATION: " << (r.valid ? "PASS" : "FAIL") << "\n";
        output << "Ledger Intact: " << (r.ledgerIntact ? "PASS" : "FAIL") << "\n";
        output << "Sequence Monotonic: " << (r.sequenceMonotonic ? "PASS" : "FAIL") << "\n";
        output << "Manifest Consistent: " << (r.manifestConsistent ? "PASS" : "FAIL") << "\n";
        output << "Certificates Intact: " << (r.certificatesIntact ? "PASS" : "FAIL") << "\n";
        output << "Total Events: " << r.totalEvents << "\n";
        output << "Evidence Fingerprint: " << r.evidenceFingerprint << "\n";
        if (!r.errors.isEmpty()) {
            output << "Errors:\n";
            for (const auto& err : r.errors) output << "  - " << err << "\n";
        }
        return r.valid ? 0 : 1;
    }

    if (subcommand == QStringLiteral("f1-recover")) {
        QString recErr;
        const bool ok = MemoryEvidenceFoundation::recoverPhysicalState(projectRoot, &recErr);
        output << "F1-RECOVERY: " << (ok ? "PASS" : "FAIL") << "\n";
        if (!ok && !recErr.isEmpty()) error << recErr << "\n";
        return ok ? 0 : 1;
    }

    if (subcommand == QStringLiteral("validate")) {
        QString valErr;
        const auto report = FoundationIntegrationService::validate(projectRoot, nullptr, &valErr);
        output << "=== ARAMF Foundation Validation ===\n";
        output << "F1 Evidence: " << (report.f1.valid ? "PASS" : "FAIL") << "\n";
        output << "F2 Trust: " << (report.f2.valid ? "PASS" : "FAIL") << "\n";
        output << "F3 Integrity: " << (report.f3.valid ? "PASS" : "FAIL") << "\n";
        output << "F4 Lifecycle: " << (report.f4.valid ? "PASS" : "FAIL") << "\n";
        output << "Bootstrap Order: " << (report.bootstrapOrderValid ? "PASS" : "FAIL") << "\n";
        output << "No Circular Authority: " << (report.noCircularAuthority ? "PASS" : "FAIL") << "\n";
        output << "Overall: " << (report.valid ? "PASS" : "FAIL") << "\n";
        if (!report.errors.isEmpty()) {
            output << "Errors:\n";
            for (const auto& err : report.errors) output << "  - " << err << "\n";
        }
        return report.valid ? 0 : 1;
    }

    if (subcommand == QStringLiteral("integration")) {
        QString valErr;
        auto report = FoundationIntegrationService::validate(projectRoot, nullptr, &valErr);
        report.acceptanceType = QStringLiteral("DIAGNOSTIC_RESULT");
        report.diagnosticOnly = true;
        QString writeErr;
        if (FoundationIntegrationService::writeIntegrationEvidence(projectRoot, report, &writeErr)) {
            output << "Wrote canonical foundation integration artifact to ARAMF_WORKER/verification/foundation-integration.json\n";
            output << "Acceptance: DIAGNOSTIC_RESULT (authoritative=false, foundations pre-certification)\n";
            return 0;
        } else {
            error << "Failed to write foundation integration artifact: " << writeErr << "\n";
            return 1;
        }
    }

    if (subcommand == QStringLiteral("report")) {
        const auto report = FoundationIntegrationService::validate(projectRoot);
        output << QJsonDocument(report.fullReport).toJson(QJsonDocument::Indented);
        return 0;
    }

    error << "Unknown foundation subcommand: " << subcommand << "\n";
    return 1;
}
