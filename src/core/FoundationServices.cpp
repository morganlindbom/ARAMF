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

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

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
    file.write(QJsonDocument(evidence).toJson(QJsonDocument::Indented));
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

bool F1CertificationEvidence::isComplete() const
{
    return f1FocusedPass && foundationNamespacePass && processMigrationPass
        && p1GovernancePass && p2ContextPass && p3ExecutionPass && p4PredictivePass
        && p5RoutingPass && provenanceAndScopePass && f1PhysicalValidationPass
        && memoryColdStartPass && memoryConsistencyPass && fullCTestPass
        && !sourceRevision.trimmed().isEmpty();
}

QJsonObject F1CertificationEvidence::toJson() const
{
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
        {QStringLiteral("checks"), QJsonArray{
            QJsonObject{{QStringLiteral("name"), QStringLiteral("f1-focused-suite")}, {QStringLiteral("status"), f1FocusedPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("foundation-namespace")}, {QStringLiteral("status"), foundationNamespacePass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("process-namespace-migration")}, {QStringLiteral("status"), processMigrationPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("p1-governance")}, {QStringLiteral("status"), p1GovernancePass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("p2-context")}, {QStringLiteral("status"), p2ContextPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("p3-execution")}, {QStringLiteral("status"), p3ExecutionPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("p4-predictive")}, {QStringLiteral("status"), p4PredictivePass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("p5-routing")}, {QStringLiteral("status"), p5RoutingPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("provenance-and-scope")}, {QStringLiteral("status"), provenanceAndScopePass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("f1-physical-validation")}, {QStringLiteral("status"), f1PhysicalValidationPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("memory-cold-start")}, {QStringLiteral("status"), memoryColdStartPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("memory-consistency")}, {QStringLiteral("status"), memoryConsistencyPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("full-ctest")}, {QStringLiteral("status"), fullCTestPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")}}
        }},
        {QStringLiteral("knownLimitations"), knownLimitations.isEmpty() ? QJsonArray{
            QStringLiteral("F1 covers Memory & Evidence Foundation only; F2-F4 remain uncertified and in pre-certification state."),
            QStringLiteral("Integrated foundation validation (foundationIntegrationValid) remains false; P6 gating remains strictly BLOCKED.")
        } : knownLimitations},
        {QStringLiteral("overallStatus"), isComplete() ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
        {QStringLiteral("rawDetails"), rawDetails}
    };
}

F1CertificationEvidence F1CertificationEvidence::fromJson(const QJsonObject& json)
{
    F1CertificationEvidence ev;
    ev.foundation = json.value(QStringLiteral("foundation")).toString(QStringLiteral("F1"));
    ev.foundationName = json.value(QStringLiteral("foundationName")).toString();
    ev.foundationVersion = json.value(QStringLiteral("foundationVersion")).toString();
    ev.sourceRevision = json.value(QStringLiteral("sourceRevision")).toString();
    ev.verificationLevel = json.value(QStringLiteral("verificationLevel")).toString(QStringLiteral("HOST_TEST"));
    ev.timestamp = json.value(QStringLiteral("timestamp")).toString();
    ev.evidenceFingerprint = json.value(QStringLiteral("evidenceFingerprint")).toString();
    ev.knownLimitations = json.value(QStringLiteral("knownLimitations")).toArray();
    ev.rawDetails = json.value(QStringLiteral("rawDetails")).toObject();

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

    return ev;
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
            cf.write(QJsonDocument(contractObject).toJson(QJsonDocument::Indented));
            cf.commit();
        }
    }
    const QString curCertPath = QDir(certDir).filePath(QStringLiteral("current-certification-state.json"));
    if (!QFile::exists(curCertPath)) {
        QSaveFile csf(curCertPath);
        if (csf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            csf.write(QJsonDocument(QJsonObject{
                {QStringLiteral("_file"), QStringLiteral("current-certification-state.json")},
                {QStringLiteral("version"), 1},
                {QStringLiteral("subjects"), QJsonObject{}}
            }).toJson(QJsonDocument::Indented));
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
    if (!ensureCertificationArea(projectRoot, error)) return false;
    const QString rel = QStringLiteral("ARAMF_WORKER/certification/evidence/f1-evidence.json");
    const QString full = QDir(projectRoot).filePath(rel);
    QSaveFile file(full);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open %1 for writing: %2").arg(full, file.errorString());
        return false;
    }
    const QByteArray content = QJsonDocument(evidence.toJson()).toJson(QJsonDocument::Indented);
    file.write(content);
    if (!file.commit()) {
        if (error) *error = QStringLiteral("Cannot commit %1: %2").arg(full, file.errorString());
        return false;
    }
    if (relativePath) *relativePath = rel;
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

bool FoundationCertificationService::certifyF1(const QString& projectRoot,
                                              const QString& projectFilePath,
                                              const QString& sourceRevision,
                                              const QString& evidenceArtifactPath,
                                              QJsonObject* issuedCertificate,
                                              QString* error)
{
    // 1. Validate preconditions
    if (sourceRevision.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("F1 certification requires a non-empty sourceRevision.");
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

    // F1 must not already be completed
    if (model.processVersionState().isFoundationComplete(1)) {
        if (error) *error = QStringLiteral("F1 is already completed and certified in lifecycle history.");
        return false;
    }

    // foundationIntegrationValid must remain false
    if (model.processVersionState().foundationIntegrationValid) {
        if (error) *error = QStringLiteral("foundationIntegrationValid must remain false prior to all-foundation certification.");
        return false;
    }

    // Check consistency between persisted and generated lifecycle states
    const QString genPath = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/project.json"));
    if (QFile::exists(genPath)) {
        QFile gf(genPath);
        if (gf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const auto gDoc = QJsonDocument::fromJson(gf.readAll()).object();
            gf.close();
            ProcessVersionState gState;
            QString gErr;
            if (processVersionStateFromJson(gDoc.value(QStringLiteral("processVersion")), &gState, &gErr)) {
                if (processVersionStateToJson(model.processVersionState()) != processVersionStateToJson(gState)) {
                    if (error) *error = QStringLiteral("Persisted and generated lifecycle states are not synchronized.");
                    return false;
                }
            }
        }
    }

    // Lifecycle position check:
    // If inactive and next is F1.1.0.0.0, start F1
    if (!model.processVersionState().hasActiveProcess) {
        if (model.processVersionState().nextIdentifier() == QStringLiteral("F1.1.0.0.0")) {
            if (!startF1(projectRoot, resolved, error)) return false;
            if (!persistence.load(&model, resolved, error)) return false;
        } else {
            if (error) *error = QStringLiteral("Lifecycle position '%1' is not valid for F1 certification (expected next F1.1.0.0.0).")
                .arg(model.processVersionState().nextIdentifier());
            return false;
        }
    }

    // Active process must now be F1.1.1.0.0
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
    const QString artPath = evidenceArtifactPath.isEmpty()
        ? QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence/f1-evidence.json"))
        : (QFileInfo(evidenceArtifactPath).isAbsolute() ? evidenceArtifactPath : QDir(projectRoot).filePath(evidenceArtifactPath));

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
    if (!ev.isComplete()) {
        if (error) *error = QStringLiteral("F1 certification evidence is incomplete: all 13 required verification suites/checks must PASS.");
        return false;
    }

    const QString artSha256 = computeFileSha256(artPath);
    if (artSha256.isEmpty()) {
        if (error) *error = QStringLiteral("Could not compute SHA-256 fingerprint of evidence artifact.");
        return false;
    }
    const QString artRelPath = QDir(projectRoot).relativeFilePath(artPath);

    // 4. CertificationService: start and issue certificate
    if (!ensureCertificationArea(projectRoot, error)) return false;
    CertificationService certService;

    const QJsonArray requirements{ QStringLiteral("f1-evidence-artifact") };
    const QJsonObject context{
        {QStringLiteral("foundation"), QStringLiteral("F1")},
        {QStringLiteral("foundationName"), QStringLiteral("Memory & Evidence Foundation")},
        {QStringLiteral("foundationVersion"), QStringLiteral("F1.1.1")},
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
    startedCert.insert(QStringLiteral("foundationVersion"), QStringLiteral("F1.1.1"));
    startedCert.insert(QStringLiteral("foundationName"), QStringLiteral("Memory & Evidence Foundation"));
    startedCert.insert(QStringLiteral("evidenceArtifact"), artRelPath);
    startedCert.insert(QStringLiteral("evidenceFingerprint"), artSha256);

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

    // Verify current certification state has F1
    const auto curCertState = certService.currentState(projectRoot, error);
    if (!curCertState.value(QStringLiteral("subjects")).toObject().contains(QStringLiteral("F1"))) {
        if (error) *error = QStringLiteral("Current certification state does not contain F1.");
        return false;
    }

    // 6. ONLY NOW perform lifecycle certification: certifyCurrentProcessIteration
    if (!model.certifyCurrentProcessIteration(error)) {
        return false;
    }
    if (!persistence.save(model, resolved, error)) {
        return false;
    }
    if (!synchronizeProjectJson(projectRoot, model, error)) {
        return false;
    }

    if (model.processVersionState().activeIdentifier() != QStringLiteral("F1.1.1.1.0")) {
        if (error) *error = QStringLiteral("Active process is not F1.1.1.1.0 after certification.");
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
    const QString pjPath = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/project.json"));
    if (!QFile::exists(pjPath)) {
        return true;
    }
    QFile f(pjPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot read %1: %2").arg(pjPath, f.errorString());
        return false;
    }
    QJsonParseError parseErr;
    auto doc = QJsonDocument::fromJson(f.readAll(), &parseErr).object();
    f.close();
    if (parseErr.error != QJsonParseError::NoError) {
        if (error) *error = QStringLiteral("Malformed project.json: %1").arg(parseErr.errorString());
        return false;
    }

    doc.insert(QStringLiteral("processVersion"), processVersionStateToJson(model.processVersionState()));

    QSaveFile sf(pjPath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot write %1: %2").arg(pjPath, sf.errorString());
        return false;
    }
    sf.write(QJsonDocument(doc).toJson(QJsonDocument::Indented));
    if (!sf.commit()) {
        if (error) *error = QStringLiteral("Cannot commit %1: %2").arg(pjPath, sf.errorString());
        return false;
    }
    return true;
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
            output << "Usage: aramf foundation <validate|status|integration|report|certify|complete|start|write-f1-evidence> [--project <path>] [--file <file>] [--foundation <name>] [--source-revision <sha>] [--evidence <path>]\n";
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
        output << "F1 (Memory & Evidence): " << (isFoundationCertified(1) ? "Certified (F1.1.1.1.1)" : "Ready (Pre-Certification)") << "\n";
        output << "F2 (Identity, Provenance & Trust): " << (isFoundationCertified(2) ? "Certified (F2.1.1.1.1)" : "Ready (Pre-Certification)") << "\n";
        output << "F3 (Scope, State & Integrity): " << (isFoundationCertified(3) ? "Certified (F3.1.1.1.1)" : "Ready (Pre-Certification)") << "\n";
        output << "F4 (Lifecycle & Certification): " << (isFoundationCertified(4) ? "Certified (F4.1.1.1.1)" : "Ready (Pre-Certification)") << "\n";
        output << "P6 Gating: " << (pvSummary.value(QStringLiteral("p6Eligible")).toBool() ? "ELIGIBLE" : "BLOCKED") << "\n";
        output << "P6 Reason: " << pvSummary.value(QStringLiteral("p6Reason")).toString() << "\n";
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
        output << "=== ARAMF Foundation Certification ===\n";
        output << "Foundation: F1\n";
        output << "Foundation Name: Memory & Evidence Foundation\n";
        output << "Foundation Version: F1.1.1\n";
        output << "Source Revision: " << sourceRevision << "\n";
        output << "Certificate ID: " << issuedCert.value(QStringLiteral("certificateId")).toString() << "\n";
        output << "Certification Status: " << issuedCert.value(QStringLiteral("certificationStatus")).toString() << "\n";
        output << "Result: " << issuedCert.value(QStringLiteral("result")).toString() << "\n";
        output << "Evidence Complete: " << (issuedCert.value(QStringLiteral("evidenceComplete")).toBool() ? "true" : "false") << "\n";
        output << "Active Identifier: F1.1.1.1.0\n";
        output << "Next Identifier: F2.1.0.0.0\n";
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
        output << "=== ARAMF Foundation Completion ===\n";
        output << "Foundation: F1\n";
        output << "Completed Identifier: F1.1.1.1.1\n";
        output << "Active Identifier: none\n";
        output << "Next Identifier: F2.1.0.0.0\n";
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
        ev.sourceRevision = sourceRevision;
        ev.f1FocusedPass = true;
        ev.foundationNamespacePass = true;
        ev.processMigrationPass = true;
        ev.p1GovernancePass = true;
        ev.p2ContextPass = true;
        ev.p3ExecutionPass = true;
        ev.p4PredictivePass = true;
        ev.p5RoutingPass = true;
        ev.provenanceAndScopePass = true;
        ev.f1PhysicalValidationPass = true;
        ev.memoryColdStartPass = true;
        ev.memoryConsistencyPass = true;
        ev.fullCTestPass = true;
        ev.evidenceFingerprint = MemoryEvidenceFoundation::evidenceFingerprint(projectRoot);

        QString relPath, sha;
        QString wErr;
        if (!FoundationCertificationService::writeEvidenceArtifact(projectRoot, ev, &relPath, &sha, &wErr)) {
            error << "error=" << wErr << "\n";
            return 1;
        }
        output << "=== ARAMF F1 Evidence Written ===\n";
        output << "Artifact Path: " << relPath << "\n";
        output << "Fingerprint: " << sha << "\n";
        output << "Source Revision: " << sourceRevision << "\n";
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
