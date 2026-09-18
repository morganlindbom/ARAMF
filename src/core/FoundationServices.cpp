// FoundationServices.cpp
// Implementation of the F1-F4 Foundation services.
// These are thin coordination layers that integrate existing ARAMF core services
// under the canonical foundation responsibility model.

#include "FoundationServices.h"
#include "ProjectMemory.h"
#include "ProjectMemoryCompaction.h"
#include "CertificationService.h"
#include "ProcessVersion.h"
#include "ValidationRouting.h"
#include "WorkerContextResolver.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════════
// F1: Memory & Evidence Foundation
// ═══════════════════════════════════════════════════════════════════════════════

F1EvidenceReport MemoryEvidenceFoundation::validate(const QString& projectRoot, QString* error)
{
    F1EvidenceReport report;
    ProjectMemory memory;

    // 1. Validate the memory consistency (includes ledger, sequence, manifest checks)
    const auto memReport = memory.validate(projectRoot, error, false);
    report.fullReport.insert(QStringLiteral("memoryValidation"), memReport);

    const auto checks = memReport.value(QStringLiteral("checks")).toArray();
    auto isCheckPass = [&checks](const QString& name) -> bool {
        for (const auto& v : checks) {
            const auto obj = v.toObject();
            if (obj.value(QStringLiteral("name")).toString() == name)
                return obj.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
        }
        return false;
    };

    // Sequence monotonicity
    report.sequenceMonotonic = isCheckPass(QStringLiteral("sequence-order"));
    if (!report.sequenceMonotonic)
        report.errors.append(QStringLiteral("F1: Event sequence is not monotonically increasing"));

    // Manifest consistency
    report.manifestConsistent = isCheckPass(QStringLiteral("manifest-next-sequence"))
                             && isCheckPass(QStringLiteral("manifest-event-count"));
    if (!report.manifestConsistent)
        report.errors.append(QStringLiteral("F1: Manifest is inconsistent with event log"));

    // Event identifier uniqueness (ledger integrity)
    report.ledgerIntact = isCheckPass(QStringLiteral("event-identifiers-unique"));
    if (!report.ledgerIntact)
        report.errors.append(QStringLiteral("F1: Ledger integrity violated - duplicate event IDs"));

    // 2. Cold-start validation
    const auto csReport = memory.validateColdStart(projectRoot, error);
    report.fullReport.insert(QStringLiteral("coldStartValidation"), csReport);
    report.coldStartFresh = csReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
    report.coldStartFingerprint = csReport.value(QStringLiteral("fingerprint")).toString();
    if (!report.coldStartFresh)
        report.errors.append(QStringLiteral("F1: Cold-start reconstruction is stale or failed"));

    // 3. Count events
    const QString eventLogPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    const auto events = readJsonlEvents(eventLogPath);
    report.totalEvents = events.size();

    // 4. Certification ledger integrity
    CertificationService certService;
    const auto certs = certService.certificates(projectRoot, error);
    report.totalCertificates = certs.size();
    report.certificatesIntact = true; // If we got this far, certificates parsed without error
    for (const auto& cert : certs) {
        if (cert.value(QStringLiteral("certificateId")).toString().isEmpty()) {
            report.certificatesIntact = false;
            report.errors.append(QStringLiteral("F1: Certificate missing ID"));
            break;
        }
    }

    // Overall validity
    report.valid = report.ledgerIntact && report.coldStartFresh
                && report.sequenceMonotonic && report.manifestConsistent
                && report.certificatesIntact;

    return report;
}

QJsonObject MemoryEvidenceFoundation::evidenceSummary(const QString& projectRoot, QString* error)
{
    QJsonObject summary;
    ProjectMemory memory;

    const auto events = memory.events(projectRoot, error);
    summary.insert(QStringLiteral("totalEvents"), events.size());

    const auto decisions = memory.currentDecisions(projectRoot, error);
    summary.insert(QStringLiteral("currentDecisions"), decisions.size());

    const auto checkpoints = memory.checkpoints(projectRoot, error);
    summary.insert(QStringLiteral("checkpoints"), checkpoints.size());

    CertificationService certService;
    const auto certs = certService.certificates(projectRoot, error);
    summary.insert(QStringLiteral("certificates"), certs.size());

    summary.insert(QStringLiteral("memoryUsageBytes"), memory.memoryUsageBytes(projectRoot));
    summary.insert(QStringLiteral("foundation"), QStringLiteral("F1"));
    summary.insert(QStringLiteral("name"), QStringLiteral("Memory & Evidence Foundation"));

    return summary;
}

bool MemoryEvidenceFoundation::canReconstructFromColdStart(const QString& projectRoot, QString* error)
{
    ProjectMemory memory;
    const auto csReport = memory.validateColdStart(projectRoot, error);
    return csReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
}

QJsonObject MemoryEvidenceFoundation::contract()
{
    return QJsonObject{
        {QStringLiteral("foundation"), QStringLiteral("F1")},
        {QStringLiteral("name"), QStringLiteral("Memory & Evidence Foundation")},
        {QStringLiteral("responsibility"), QStringLiteral("Stores and reconstructs evidence")},
        {QStringLiteral("owns"), QJsonArray{
            QStringLiteral("append-only-ledger"),
            QStringLiteral("cold-start-reconstruction"),
            QStringLiteral("certification-evidence"),
            QStringLiteral("memory-consistency"),
            QStringLiteral("compaction-governance")
        }},
        {QStringLiteral("bootstrapOrder"), 1},
        {QStringLiteral("dependsOn"), QJsonArray{}},
        {QStringLiteral("requiredBy"), QJsonArray{
            QStringLiteral("F2"), QStringLiteral("F3"), QStringLiteral("F4")
        }}
    };
}

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

    // 3. Trust boundary enforcement - verify admin override events have correct provenance
    report.trustBoundariesEnforced = true;
    for (const auto& ev : events) {
        if (ev.value(QStringLiteral("eventType")).toString() == QStringLiteral("ADMIN_OVERRIDE")) {
            const auto prov = ev.value(QStringLiteral("provenance")).toObject();
            if (prov.value(QStringLiteral("actor")).toString() != QStringLiteral("human")) {
                report.trustBoundariesEnforced = false;
                report.errors.append(QStringLiteral("F2: ADMIN_OVERRIDE event has non-human actor"));
            }
        }
    }

    report.valid = report.allProvenanceValid && report.actorTaxonomyConsistent
                && report.trustBoundariesEnforced;
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

bool IdentityTrustFoundation::validateProvenance(const QJsonObject& provenance, QString* error)
{
    return ProjectMemory::validateProvenanceObject(provenance, error);
}

bool IdentityTrustFoundation::respectsTrustBoundary(const QString& instruction,
                                                     const QString& requestedAction,
                                                     QString* error)
{
    // Trust boundary: admin overrides must come from verified admin identity
    ProjectMemory memory;
    if (!memory.isVerifiedAdministrativeOverride(instruction)) {
        if (error) *error = QStringLiteral("F2: Instruction does not pass administrative verification");
        return false;
    }

    // Block dangerous shell operations regardless of admin status
    static const QStringList dangerousPatterns = {
        QStringLiteral("rmdir /s /q"),
        QStringLiteral("rd /s /q"),
        QStringLiteral("rm -rf"),
        QStringLiteral("Remove-Item -Recurse")
    };
    for (const auto& pattern : dangerousPatterns) {
        if (requestedAction.contains(pattern, Qt::CaseInsensitive)) {
            if (error) *error = QStringLiteral("F2: Recursive deletion blocked by trust boundary");
            return false;
        }
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

    // 6. Validation routing consistency
    report.validationRoutingConsistent = true;
    const auto policy = ValidationRouting::policy();
    if (policy.isEmpty()) {
        report.validationRoutingConsistent = false;
        report.errors.append(QStringLiteral("F3: Validation routing policy is empty"));
    }

    report.valid = report.scopeTaxonomyValid && report.scopeCombinationsLegal
                && report.crossScopeFilesValid && report.projectStateIntegral
                && report.validationRoutingConsistent;

    return report;
}

QStringList ScopeIntegrityFoundation::canonicalScopes()
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
            QStringLiteral("validation-routing-consistency")
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

    // 2. Process history validity - all completed entries must have cert=1, done=1
    report.processHistoryValid = true;
    report.completedProcesses = 0;
    report.completedFoundations = 0;
    for (const auto& pv : pvState.completedHistory) {
        if (pv.certification != 1 || pv.done != 1) {
            report.processHistoryValid = false;
            report.errors.append(QStringLiteral("F4: Completed entry %1 has cert=%2 done=%3")
                .arg(pv.identifier()).arg(pv.certification).arg(pv.done));
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

    // 7. Certification semantics: cert=1 requires done=1 for completed items
    report.certificationSemanticsValid = true;
    for (const auto& pv : pvState.completedHistory) {
        if (pv.certification == 1 && pv.done != 1) {
            report.certificationSemanticsValid = false;
            report.errors.append(QStringLiteral("F4: %1 has cert=1 but done=0").arg(pv.identifier()));
        }
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

    // Cannot be certified without being done
    if (cert == 1 && done != 1) {
        if (error) *error = QStringLiteral("F4: Certification requires done=1");
        return false;
    }

    // Cannot be done without at least one iteration
    if (done == 1 && iteration < 1) {
        if (error) *error = QStringLiteral("F4: Done requires at least one iteration");
        return false;
    }

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

    // Execute in strict bootstrap order: F1 → F2 → F3 → F4
    // Each foundation depends on its predecessors but not on successors.

    // F1: Memory & Evidence (no dependencies)
    report.f1 = MemoryEvidenceFoundation::validate(projectRoot, error);
    if (!report.f1.valid) {
        report.errors.append(QStringLiteral("Integration: F1 failed - cannot proceed to F2"));
        // F2-F4 depend on F1, but we still run them to collect all errors
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
    // does not create a cycle
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

    // Build full report
    report.fullReport.insert(QStringLiteral("f1"), report.f1.fullReport);
    report.fullReport.insert(QStringLiteral("f2"), report.f2.fullReport);
    report.fullReport.insert(QStringLiteral("f3"), report.f3.fullReport);
    report.fullReport.insert(QStringLiteral("f4"), report.f4.fullReport);
    report.fullReport.insert(QStringLiteral("bootstrapOrderValid"), report.bootstrapOrderValid);
    report.fullReport.insert(QStringLiteral("noCircularAuthority"), report.noCircularAuthority);

    report.valid = report.f1.valid && report.f2.valid && report.f3.valid && report.f4.valid
                && report.bootstrapOrderValid && report.noCircularAuthority;

    return report;
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
