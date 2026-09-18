// FoundationTests.cpp
// Comprehensive test suites for F1-F4 Foundation services.
// Test IDs: F1-001 through F1-010, F2-001 through F2-010,
//           F3-001 through F3-010, F4-001 through F4-010,
//           INT-F-001 through INT-F-011, FAIL-INJ-001 through FAIL-INJ-005,
//           XPROC-001 (cross-process regression)

#include "core/FoundationServices.h"
#include "core/ProjectMemory.h"
#include "core/ProjectMemoryCompaction.h"
#include "core/CertificationService.h"
#include "core/ProcessVersion.h"
#include "core/MemoryCommand.h"
#include "core/ProjectModel.h"
#include "core/FrameworkKnowledge.h"
#include "core/AramfPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <iostream>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

// Creates a fully initialized test fixture with memory enabled.
struct TestFixture {
    QTemporaryDir tempDir;
    ProjectModel model;
    ProjectMemory memory;
    bool valid = false;

    TestFixture() {
        if (!tempDir.isValid()) return;
        model.setProjectPath(tempDir.path());
        MemoryConfiguration memConfig;
        memConfig.maintenanceOptions = {
            QStringLiteral("update-current-state"),
            QStringLiteral("record-task-completion"),
            QStringLiteral("record-build-results"),
            QStringLiteral("record-test-results"),
            QStringLiteral("record-validation"),
            QStringLiteral("record-decisions"),
            QStringLiteral("record-checkpoints")
        };
        memConfig.validationOptions = {
            QStringLiteral("memory-consistency"),
            QStringLiteral("event-provenance-valid"),
            QStringLiteral("persisted-scope-validity")
        };
        model.setMemoryConfiguration(memConfig);
        valid = memory.initialize(tempDir.path(), &model, nullptr);

        // Ensure baseline project.json exists with valid canonical foundation state
        ProcessVersionState defaultState;
        defaultState.namespaceVersion = 2;
        defaultState.hasNextProcess = true;
        defaultState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        defaultState.foundationQueue = ProcessVersionState::canonicalFoundationQueue();
        defaultState.hasFutureProcess = true;
        defaultState.futureProcess = ProcessVersion(6, 1, 0, 0, 0);
        writeProcessVersion(defaultState);
    }

    QString path() const { return tempDir.path(); }

    // Record a governed event with valid provenance
    bool recordTask(const QString& task, const QString& status = QStringLiteral("PASS")) {
        const QJsonObject prov{
            {QStringLiteral("actor"), QStringLiteral("agent")},
            {QStringLiteral("agentId"), QStringLiteral("test-runner")},
            {QStringLiteral("tool"), QStringLiteral("foundation-tests")}
        };
        bool ok = memory.recordOperation(path(), QStringLiteral("task-start"),
            QJsonObject{{QStringLiteral("task"), task}, {QStringLiteral("provenance"), prov}},
            nullptr, nullptr);
        if (!ok) return false;
        ok = memory.recordOperation(path(), QStringLiteral("task-complete"),
            QJsonObject{{QStringLiteral("task"), task},
                        {QStringLiteral("status"), status},
                        {QStringLiteral("provenance"), prov}},
            nullptr, nullptr);
        return ok;
    }

    // Write a valid processVersion block into project.json
    bool writeProcessVersion(const ProcessVersionState& pvState) {
        const QString pjPath = QDir(path()).filePath(
            QStringLiteral("ARAMF_WORKER/project.json"));
        QDir().mkpath(QFileInfo(pjPath).path());
        QFile f(pjPath);
        QJsonObject doc;
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            doc = QJsonDocument::fromJson(f.readAll()).object();
            f.close();
        }
        doc.insert(QStringLiteral("processVersion"), processVersionStateToJson(pvState));
        if (!doc.contains(QStringLiteral("projectId"))) {
            doc.insert(QStringLiteral("projectId"), QStringLiteral("fixture-project"));
        }
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(QJsonDocument(doc).toJson());
        f.close();
        return true;
    }
};

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════════
// F1 Tests: Memory & Evidence Foundation
// ═══════════════════════════════════════════════════════════════════════════════

bool runF1MemoryEvidenceTests()
{
    bool ok = true;
    std::cerr << "=== F1: Memory & Evidence Foundation Tests ===\n";

    // F1-001: Contract has correct metadata
    {
        const auto contract = MemoryEvidenceFoundation::contract();
        ok &= require(contract.value(QStringLiteral("foundation")).toString() == QStringLiteral("F1"),
                       "F1-001: Contract foundation identifier is F1");
        ok &= require(contract.value(QStringLiteral("bootstrapOrder")).toInt() == 1,
                       "F1-001: Bootstrap order is 1");
        ok &= require(contract.value(QStringLiteral("dependsOn")).toArray().isEmpty(),
                       "F1-001: F1 has no dependencies");
    }

    // F1-002: Validate passes on fresh initialized fixture
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-002: Fixture initializes");
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(report.valid, "F1-002: F1 validation passes on fresh fixture");
        ok &= require(report.ledgerIntact, "F1-002: Ledger is intact");
        ok &= require(report.sequenceMonotonic, "F1-002: Sequence is monotonic");
        ok &= require(report.manifestConsistent, "F1-002: Manifest is consistent");
    }

    // F1-003: Cold-start reconstruction works
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-003: Fixture initializes");
        fx.recordTask(QStringLiteral("F1 cold-start test"));
        ok &= require(MemoryEvidenceFoundation::canReconstructFromColdStart(fx.path()),
                       "F1-003: Cold-start reconstruction succeeds after recording events");
    }

    // F1-004: Evidence summary returns correct counts
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-004: Fixture initializes");
        fx.recordTask(QStringLiteral("F1 evidence task 1"));
        fx.recordTask(QStringLiteral("F1 evidence task 2"));
        const auto summary = MemoryEvidenceFoundation::evidenceSummary(fx.path());
        ok &= require(summary.value(QStringLiteral("totalEvents")).toInt() >= 4,
                       "F1-004: Evidence summary shows at least 4 events");
        ok &= require(summary.value(QStringLiteral("foundation")).toString() == QStringLiteral("F1"),
                       "F1-004: Summary identifies F1 foundation");
    }

    // F1-005: Ledger integrity detects duplicate event IDs
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-005: Fixture initializes");
        // Inject a duplicate event ID using an actual existing event's ID
        const QString logPath = QDir(fx.path()).filePath(
            QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        const auto existingEvents = fx.memory.events(fx.path(), nullptr);
        QString dupId = QStringLiteral("event-test-duplicate");
        if (!existingEvents.isEmpty()) {
            dupId = existingEvents.first().value(QStringLiteral("eventId")).toString();
        }
        QFile logFile(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QJsonObject dupEvent{
                {QStringLiteral("eventId"), dupId},
                {QStringLiteral("eventType"), QStringLiteral("TEST_RESULT")},
                {QStringLiteral("sequenceNumber"), 999},
                {QStringLiteral("timestamp"), QStringLiteral("2026-09-18T00:00:00Z")},
                {QStringLiteral("task"), QStringLiteral("dup test")},
                {QStringLiteral("provenance"), QJsonObject{
                    {QStringLiteral("actor"), QStringLiteral("system")},
                    {QStringLiteral("agentId"), QStringLiteral("test")},
                    {QStringLiteral("tool"), QStringLiteral("test")}
                }}
            };
            logFile.write(QJsonDocument(dupEvent).toJson(QJsonDocument::Compact) + "\n");
            logFile.close();
        }
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!report.ledgerIntact, "F1-005: Duplicate event ID detected as ledger violation");
    }

    // F1-006: Sequence non-monotonicity detected
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-006: Fixture initializes");
        const QString logPath = QDir(fx.path()).filePath(
            QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        QFile logFile(logPath);
        // Write two events with decreasing sequence
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QJsonObject ev1{
                {QStringLiteral("eventId"), QStringLiteral("event-seq-high")},
                {QStringLiteral("eventType"), QStringLiteral("TEST_RESULT")},
                {QStringLiteral("sequenceNumber"), 500},
                {QStringLiteral("timestamp"), QStringLiteral("2026-09-18T00:00:00Z")},
                {QStringLiteral("task"), QStringLiteral("seq test")},
                {QStringLiteral("provenance"), QJsonObject{
                    {QStringLiteral("actor"), QStringLiteral("system")},
                    {QStringLiteral("agentId"), QStringLiteral("test")},
                    {QStringLiteral("tool"), QStringLiteral("test")}
                }}
            };
            QJsonObject ev2{
                {QStringLiteral("eventId"), QStringLiteral("event-seq-low")},
                {QStringLiteral("eventType"), QStringLiteral("TEST_RESULT")},
                {QStringLiteral("sequenceNumber"), 300},
                {QStringLiteral("timestamp"), QStringLiteral("2026-09-18T00:00:01Z")},
                {QStringLiteral("task"), QStringLiteral("seq test 2")},
                {QStringLiteral("provenance"), QJsonObject{
                    {QStringLiteral("actor"), QStringLiteral("system")},
                    {QStringLiteral("agentId"), QStringLiteral("test")},
                    {QStringLiteral("tool"), QStringLiteral("test")}
                }}
            };
            logFile.write(QJsonDocument(ev1).toJson(QJsonDocument::Compact) + "\n");
            logFile.write(QJsonDocument(ev2).toJson(QJsonDocument::Compact) + "\n");
            logFile.close();
        }
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!report.sequenceMonotonic, "F1-006: Non-monotonic sequence detected");
    }

    // F1-007: Certificate count reported correctly
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-007: Fixture initializes");
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(report.totalCertificates >= 0, "F1-007: Certificate count is non-negative");
        ok &= require(report.certificatesIntact, "F1-007: Certificates are intact on fresh fixture");
    }

    // F1-008: Memory usage bytes reported
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-008: Fixture initializes");
        const auto summary = MemoryEvidenceFoundation::evidenceSummary(fx.path());
        ok &= require(summary.value(QStringLiteral("memoryUsageBytes")).toDouble() >= 0,
                       "F1-008: Memory usage is reported");
    }

    // F1-009: Event count increases after recording
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-009: Fixture initializes");
        const auto before = MemoryEvidenceFoundation::evidenceSummary(fx.path());
        int beforeCount = before.value(QStringLiteral("totalEvents")).toInt();
        fx.recordTask(QStringLiteral("F1-009 increment test"));
        const auto after = MemoryEvidenceFoundation::evidenceSummary(fx.path());
        int afterCount = after.value(QStringLiteral("totalEvents")).toInt();
        ok &= require(afterCount > beforeCount, "F1-009: Event count increases after recording");
    }

    // F1-010: Full report contains all validation sections
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-010: Fixture initializes");
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!report.fullReport.isEmpty(), "F1-010: Full report is non-empty");
        ok &= require(report.fullReport.contains(QStringLiteral("memoryValidation")),
                       "F1-010: Full report contains memoryValidation");
        ok &= require(report.fullReport.contains(QStringLiteral("coldStartValidation")),
                       "F1-010: Full report contains coldStartValidation");
    }

    // F1-011: Deterministic manifest recovery from append-only ledger
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-011: Fixture initializes");
        fx.recordTask(QStringLiteral("F1-011 task 1"));
        fx.recordTask(QStringLiteral("F1-011 task 2"));

        // Tamper with manifest sequence and count
        const QString manifestPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
        QFile mf(manifestPath);
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            mf.write("{\"nextSequenceNumber\": 1, \"eventCount\": 0}\n");
            mf.close();
        }
        auto reportBefore = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!reportBefore.manifestConsistent, "F1-011: Stale manifest is detected as inconsistent");

        // Reconstruct from ledger
        QString recErr;
        bool recOk = MemoryEvidenceFoundation::reconstructManifestFromLedger(fx.path(), &recErr);
        ok &= require(recOk, "F1-011: Manifest reconstruction succeeds");

        auto reportAfter = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(reportAfter.manifestConsistent, "F1-011: Reconstructed manifest matches ledger");
    }

    // F1-012: Corrupt certificate JSONL causes validation failure
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-012: Fixture initializes");
        const QString certPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
        QDir(fx.path()).mkpath(QStringLiteral("ARAMF_WORKER/certification"));
        QFile cf(certPath);
        if (cf.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            cf.write("{corrupt unclosed json\n");
            cf.close();
        }
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!report.certificatesIntact, "F1-012: Corrupt certificate ledger detected");
    }

    std::cerr << (ok ? "F1: ALL PASS\n" : "F1: SOME FAILURES\n");
    return ok;
}

// ═══════════════════════════════════════════════════════════════════════════════
// F2 Tests: Identity, Provenance & Trust Foundation
// ═══════════════════════════════════════════════════════════════════════════════

bool runF2IdentityTrustTests()
{
    bool ok = true;
    std::cerr << "=== F2: Identity, Provenance & Trust Foundation Tests ===\n";

    // F2-001: Contract has correct metadata
    {
        const auto contract = IdentityTrustFoundation::contract();
        ok &= require(contract.value(QStringLiteral("foundation")).toString() == QStringLiteral("F2"),
                       "F2-001: Contract foundation identifier is F2");
        ok &= require(contract.value(QStringLiteral("bootstrapOrder")).toInt() == 2,
                       "F2-001: Bootstrap order is 2");
        const auto deps = contract.value(QStringLiteral("dependsOn")).toArray();
        ok &= require(deps.size() == 1 && deps[0].toString() == QStringLiteral("F1"),
                       "F2-001: F2 depends only on F1");
    }

    // F2-002: Actor taxonomy includes all recognized actors
    {
        const auto actors = IdentityTrustFoundation::actorTaxonomy();
        ok &= require(actors.contains(QStringLiteral("human")), "F2-002: Taxonomy includes human");
        ok &= require(actors.contains(QStringLiteral("agent")), "F2-002: Taxonomy includes agent");
        ok &= require(actors.contains(QStringLiteral("system")), "F2-002: Taxonomy includes system");
        ok &= require(actors.contains(QStringLiteral("tool")), "F2-002: Taxonomy includes tool");
        ok &= require(actors.contains(QStringLiteral("runtime")), "F2-002: Taxonomy includes runtime");
        ok &= require(actors.size() == 7, "F2-002: Taxonomy has exactly 7 actors");
    }

    // F2-003: Validate passes on fixture with valid provenance events
    {
        TestFixture fx;
        ok &= require(fx.valid, "F2-003: Fixture initializes");
        fx.recordTask(QStringLiteral("F2 provenance test"));
        const auto report = IdentityTrustFoundation::validate(fx.path());
        ok &= require(report.valid, "F2-003: F2 validation passes with valid provenance");
        ok &= require(report.allProvenanceValid, "F2-003: All provenance valid");
        ok &= require(report.actorTaxonomyConsistent, "F2-003: Actor taxonomy consistent");
    }

    // F2-004: Provenance validation rejects missing actor
    {
        QJsonObject badProv{
            {QStringLiteral("agentId"), QStringLiteral("test")},
            {QStringLiteral("tool"), QStringLiteral("test")}
        };
        QString err;
        ok &= require(!IdentityTrustFoundation::validateProvenance(badProv, &err),
                       "F2-004: Missing actor rejected");
        ok &= require(err.contains(QStringLiteral("actor")), "F2-004: Error mentions actor");
    }

    // F2-005: Provenance validation accepts valid provenance
    {
        QJsonObject goodProv{
            {QStringLiteral("actor"), QStringLiteral("agent")},
            {QStringLiteral("agentId"), QStringLiteral("test-agent")},
            {QStringLiteral("tool"), QStringLiteral("test-tool")}
        };
        ok &= require(IdentityTrustFoundation::validateProvenance(goodProv),
                       "F2-005: Valid provenance accepted");
    }

    // F2-006: Trust boundary blocks recursive deletion
    {
        QString err;
        bool result = IdentityTrustFoundation::respectsTrustBoundary(
            QStringLiteral("Admin Morgan Lindbom override"),
            QStringLiteral("rmdir /s /q C:\\important"),
            &err);
        ok &= require(!result, "F2-006: Recursive deletion blocked");
        ok &= require(err.contains(QStringLiteral("deletion")), "F2-006: Error mentions deletion");
    }

    // F2-007: Trust boundary blocks Remove-Item -Recurse
    {
        QString err;
        bool result = IdentityTrustFoundation::respectsTrustBoundary(
            QStringLiteral("Admin Morgan Lindbom override"),
            QStringLiteral("Remove-Item -Recurse"),
            &err);
        ok &= require(!result, "F2-007: PowerShell recursive deletion blocked");
    }

    // F2-008: Trust boundary rejects non-admin instruction
    {
        QString err;
        bool result = IdentityTrustFoundation::respectsTrustBoundary(
            QStringLiteral("Regular user request"),
            QStringLiteral("safe-action"),
            &err);
        ok &= require(!result, "F2-008: Non-admin instruction rejected");
    }

    // F2-009: Events with valid provenance counted correctly
    {
        TestFixture fx;
        ok &= require(fx.valid, "F2-009: Fixture initializes");
        fx.recordTask(QStringLiteral("F2 count test 1"));
        fx.recordTask(QStringLiteral("F2 count test 2"));
        const auto report = IdentityTrustFoundation::validate(fx.path());
        ok &= require(report.eventsWithProvenance >= 4,
                       "F2-009: At least 4 events with provenance (2 starts + 2 completes)");
    }

    // F2-010: Recognized actors reported from actual events
    {
        TestFixture fx;
        ok &= require(fx.valid, "F2-010: Fixture initializes");
        fx.recordTask(QStringLiteral("F2 actor tracking"));
        const auto report = IdentityTrustFoundation::validate(fx.path());
        ok &= require(!report.recognizedActors.isEmpty(),
                       "F2-010: Recognized actors are reported");
    }

    // F2-011: Canonical actor taxonomy validation
    {
        ok &= require(IdentityTrustFoundation::isValidActor(QStringLiteral("human")), "F2-011: human is valid actor");
        ok &= require(IdentityTrustFoundation::isValidActor(QStringLiteral("user")), "F2-011: user is valid actor");
        ok &= require(IdentityTrustFoundation::isValidActor(QStringLiteral("agent")), "F2-011: agent is valid actor");
        ok &= require(IdentityTrustFoundation::isValidActor(QStringLiteral("autonomous-agent")), "F2-011: autonomous-agent is valid actor");
        ok &= require(IdentityTrustFoundation::isValidActor(QStringLiteral("tool")), "F2-011: tool is valid actor");
        ok &= require(IdentityTrustFoundation::isValidActor(QStringLiteral("runtime")), "F2-011: runtime is valid actor");
        ok &= require(IdentityTrustFoundation::isValidActor(QStringLiteral("system")), "F2-011: system is valid actor");
        ok &= require(!IdentityTrustFoundation::isValidActor(QStringLiteral("unknown-bot")), "F2-011: unknown actor rejected");
    }

    // F2-012: Admin override requires exact identity and override intent
    {
        ok &= require(IdentityTrustFoundation::isVerifiedAdministrativeOverride(
            QStringLiteral("Admin Morgan Lindbom override emergency fix")),
            "F2-012: Valid admin override passes");
        ok &= require(!IdentityTrustFoundation::isVerifiedAdministrativeOverride(
            QStringLiteral("Morgan Lindbom override emergency fix")),
            "F2-012: Missing 'Admin' prefix rejected");
        ok &= require(!IdentityTrustFoundation::isVerifiedAdministrativeOverride(
            QStringLiteral("Admin Morgan Lindbom normal instruction")),
            "F2-012: Missing 'override' intent rejected");
    }

    // F2-013: Prohibited destructive commands blocked even with admin identity
    {
        QString trustErr;
        bool blocked = !IdentityTrustFoundation::respectsTrustBoundary(
            QStringLiteral("Admin Morgan Lindbom override allow cleanup"),
            QStringLiteral("cmd.exe /c rmdir /s /q build"),
            &trustErr);
        ok &= require(blocked, "F2-013: Destructive rmdir /s /q blocked despite admin identity");
    }

    std::cerr << (ok ? "F2: ALL PASS\n" : "F2: SOME FAILURES\n");
    return ok;
}

// ═══════════════════════════════════════════════════════════════════════════════
// F3 Tests: Scope, State & Integrity Foundation
// ═══════════════════════════════════════════════════════════════════════════════

bool runF3ScopeIntegrityTests()
{
    bool ok = true;
    std::cerr << "=== F3: Scope, State & Integrity Foundation Tests ===\n";

    // F3-001: Contract has correct metadata
    {
        const auto contract = ScopeIntegrityFoundation::contract();
        ok &= require(contract.value(QStringLiteral("bootstrapOrder")).toInt() == 3,
                       "F3-001: Bootstrap order is 3");
    }

    // F3-002: Canonical scopes include all 15 expected scopes
    {
        const auto scopes = ScopeIntegrityFoundation::canonicalScopes();
        ok &= require(scopes.size() == 15, "F3-002: 15 canonical scopes");
        ok &= require(scopes.contains(QStringLiteral("all")), "F3-002: Contains 'all'");
        ok &= require(scopes.contains(QStringLiteral("source-code")), "F3-002: Contains 'source-code'");
        ok &= require(scopes.contains(QStringLiteral("tests")), "F3-002: Contains 'tests'");
        ok &= require(scopes.contains(QStringLiteral("project+global")), "F3-002: Contains 'project+global'");
    }

    // F3-003: Validate passes on fresh fixture
    {
        TestFixture fx;
        ok &= require(fx.valid, "F3-003: Fixture initializes");
        const auto report = ScopeIntegrityFoundation::validate(fx.path(), &fx.model);
        ok &= require(report.valid, "F3-003: F3 validation passes on fresh fixture");
        ok &= require(report.scopeTaxonomyValid, "F3-003: Scope taxonomy valid");
        ok &= require(report.projectIsolationValid, "F3-003: Project isolation valid");
    }

    // F3-004: Contradictory scope combination rejected
    {
        QString err;
        QStringList bad = {QStringLiteral("project"), QStringLiteral("global")};
        ok &= require(!ScopeIntegrityFoundation::validateScopeSet(bad, &err),
                       "F3-004: project+global contradiction rejected");
    }

    // F3-005: Valid scope combination accepted
    {
        QStringList good = {QStringLiteral("source-code"), QStringLiteral("tests")};
        ok &= require(ScopeIntegrityFoundation::validateScopeSet(good),
                       "F3-005: source-code+tests accepted");
    }

    // F3-006: Cross-scope file validation rejects mismatch
    {
        QString err;
        bool result = ScopeIntegrityFoundation::validateScopeFiles(
            QStringLiteral("tests"),
            {QStringLiteral("src/core/ProjectMemory.cpp")},
            &err);
        ok &= require(!result, "F3-006: tests scope with src/ file rejected");
    }

    // F3-007: Cross-scope file validation accepts match
    {
        bool result = ScopeIntegrityFoundation::validateScopeFiles(
            QStringLiteral("tests"),
            {QStringLiteral("tests/ProjectMemoryTests.cpp")});
        ok &= require(result, "F3-007: tests scope with tests/ file accepted");
    }

    // F3-008: Project state integrity with valid processVersion
    {
        TestFixture fx;
        ok &= require(fx.valid, "F3-008: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto report = ScopeIntegrityFoundation::validate(fx.path(), &fx.model);
        ok &= require(report.projectStateIntegral, "F3-008: Valid processVersion passes integrity");
    }

    // F3-009: Canonical scope count correct
    {
        TestFixture fx;
        ok &= require(fx.valid, "F3-009: Fixture initializes");
        const auto report = ScopeIntegrityFoundation::validate(fx.path(), &fx.model);
        ok &= require(report.canonicalScopeCount == 15, "F3-009: 15 canonical scopes reported");
    }

    // F3-010: Project isolation passes on valid fixture
    {
        TestFixture fx;
        ok &= require(fx.valid, "F3-010: Fixture initializes");
        QString err;
        bool isoOk = ScopeIntegrityFoundation::validateProjectIsolation(fx.path(), &err);
        ok &= require(isoOk, "F3-010: Project isolation passes on valid fixture");
    }

    // F3-011: Canonical and effective scope registries
    {
        const auto baseScopes = ScopeIntegrityFoundation::baseReservedScopes();
        ok &= require(baseScopes.size() == 15, "F3-011: 15 base reserved scopes defined");
        TestFixture fx;
        ok &= require(fx.valid, "F3-011: Fixture initializes");
        const auto effective = ScopeIntegrityFoundation::effectiveCanonicalScopeRegistry(fx.path());
        ok &= require(effective.size() >= 15, "F3-011: Effective registry contains base scopes");
    }

    // F3-012: Project isolation rejects foreign path escape and missing projectId
    {
        TestFixture fx;
        ok &= require(fx.valid, "F3-012: Fixture initializes");
        const QString logPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        QFile logFile(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QJsonObject badEvent{
                {QStringLiteral("eventId"), QStringLiteral("event-path-escape")},
                {QStringLiteral("eventType"), QStringLiteral("TEST_RESULT")},
                {QStringLiteral("sequenceNumber"), 900},
                {QStringLiteral("timestamp"), QStringLiteral("2026-09-18T00:00:00Z")},
                {QStringLiteral("task"), QStringLiteral("escape test")},
                {QStringLiteral("affectedFiles"), QJsonArray{QStringLiteral("../../../foreign/file.cpp")}}
            };
            logFile.write(QJsonDocument(badEvent).toJson(QJsonDocument::Compact) + "\n");
            logFile.close();
        }
        QString err;
        bool isoOk = ScopeIntegrityFoundation::validateProjectIsolation(fx.path(), &err);
        ok &= require(!isoOk, "F3-012: Foreign path escape rejected by project isolation");
    }

    std::cerr << (ok ? "F3: ALL PASS\n" : "F3: SOME FAILURES\n");
    return ok;
}

// ═══════════════════════════════════════════════════════════════════════════════
// F4 Tests: Lifecycle & Certification Foundation
// ═══════════════════════════════════════════════════════════════════════════════

bool runF4LifecycleCertificationTests()
{
    bool ok = true;
    std::cerr << "=== F4: Lifecycle & Certification Foundation Tests ===\n";

    // F4-001: Contract has correct metadata
    {
        const auto contract = LifecycleCertificationFoundation::contract();
        ok &= require(contract.value(QStringLiteral("foundation")).toString() == QStringLiteral("F4"),
                       "F4-001: Contract foundation identifier is F4");
        ok &= require(contract.value(QStringLiteral("bootstrapOrder")).toInt() == 4,
                       "F4-001: Bootstrap order is 4");
        const auto deps = contract.value(QStringLiteral("dependsOn")).toArray();
        ok &= require(deps.size() == 3, "F4-001: F4 depends on F1, F2, F3");
    }

    // F4-002: Validate passes with correct processVersion state
    {
        TestFixture fx;
        ok &= require(fx.valid, "F4-002: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.completedHistory.append(ProcessVersion(1, 1, 1, 1, 1));
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto report = LifecycleCertificationFoundation::validate(fx.path());
        ok &= require(report.valid, "F4-002: F4 validation passes with valid state");
        ok &= require(report.namespaceVersionCorrect, "F4-002: Namespace version correct");
        ok &= require(report.processHistoryValid, "F4-002: Process history valid");
    }

    // F4-003: Certification semantics - cert=1 done=0 is valid for active iteration; done=1 requires cert=1
    {
        QJsonObject activeCertifiedState{
            {QStringLiteral("certification"), 1},
            {QStringLiteral("done"), 0},
            {QStringLiteral("iteration"), 3}
        };
        ok &= require(LifecycleCertificationFoundation::validateCertificationSemantics(activeCertifiedState),
                       "F4-003: cert=1 done=0 is valid for active iteration awaiting completion");

        QJsonObject completedCertifiedState{
            {QStringLiteral("certification"), 1},
            {QStringLiteral("done"), 1},
            {QStringLiteral("iteration"), 3}
        };
        ok &= require(LifecycleCertificationFoundation::validateCertificationSemantics(completedCertifiedState),
                       "F4-003: cert=1 done=1 is valid");

        QJsonObject completedUncertifiedState{
            {QStringLiteral("certification"), 0},
            {QStringLiteral("done"), 1},
            {QStringLiteral("iteration"), 3}
        };
        QString err;
        ok &= require(!LifecycleCertificationFoundation::validateCertificationSemantics(completedUncertifiedState, &err),
                       "F4-003: done=1 cert=0 is rejected (cannot complete uncertified)");
    }

    // F4-004: Certification semantics - done=1 requires iteration >= 1
    {
        QJsonObject badState{
            {QStringLiteral("certification"), 0},
            {QStringLiteral("done"), 1},
            {QStringLiteral("iteration"), 0}
        };
        QString err;
        ok &= require(!LifecycleCertificationFoundation::validateCertificationSemantics(badState, &err),
                       "F4-004: done=1 with iteration=0 is rejected");
    }

    // F4-005: P6 gating - not eligible without foundations
    {
        TestFixture fx;
        ok &= require(fx.valid, "F4-005: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.completedHistory.append(ProcessVersion(1, 1, 1, 1, 1));
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        pvState.foundationQueue = ProcessVersionState::canonicalFoundationQueue();
        fx.writeProcessVersion(pvState);
        const auto report = LifecycleCertificationFoundation::validate(fx.path());
        ok &= require(report.p6GatingCorrect, "F4-005: P6 gating correctly blocks without foundations");
    }

    // F4-006: Active process with done=1 is invalid
    {
        TestFixture fx;
        ok &= require(fx.valid, "F4-006: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasActiveProcess = true;
        pvState.activeProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 0, 1);
        fx.writeProcessVersion(pvState);
        const auto report = LifecycleCertificationFoundation::validate(fx.path());
        ok &= require(!report.activeStateValid, "F4-006: Active process with done=1 rejected");
    }

    // F4-007: Lifecycle summary returns correct fields
    {
        TestFixture fx;
        ok &= require(fx.valid, "F4-007: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.completedHistory.append(ProcessVersion(1, 1, 1, 1, 1));
        pvState.completedHistory.append(ProcessVersion(2, 1, 1, 1, 1));
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto summary = LifecycleCertificationFoundation::lifecycleSummary(fx.path());
        ok &= require(summary.value(QStringLiteral("completedCount")).toInt() == 2,
                       "F4-007: 2 completed processes in summary");
        ok &= require(summary.value(QStringLiteral("foundation")).toString() == QStringLiteral("F4"),
                       "F4-007: Summary identifies F4 foundation");
    }

    // F4-008: Next state with non-zero iteration is invalid
    {
        TestFixture fx;
        ok &= require(fx.valid, "F4-008: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 3, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto report = LifecycleCertificationFoundation::validate(fx.path());
        ok &= require(!report.nextStateValid, "F4-008: Next state with iteration=3 rejected");
    }

    // F4-009: Foundation queue must contain only foundations
    {
        TestFixture fx;
        ok &= require(fx.valid, "F4-009: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        pvState.foundationQueue.append(ProcessVersion(ProcessKind::Foundation, 2, 1, 0, 0, 0));
        fx.writeProcessVersion(pvState);
        const auto report = LifecycleCertificationFoundation::validate(fx.path());
        ok &= require(report.foundationQueueValid, "F4-009: Queue with foundations is valid");
    }

    // F4-010: Completed count is accurate
    {
        TestFixture fx;
        ok &= require(fx.valid, "F4-010: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.completedHistory.append(ProcessVersion(1, 1, 1, 1, 1));
        pvState.completedHistory.append(ProcessVersion(2, 1, 1, 1, 1));
        pvState.completedHistory.append(ProcessVersion(3, 1, 4, 1, 1));
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto report = LifecycleCertificationFoundation::validate(fx.path());
        ok &= require(report.completedProcesses == 3, "F4-010: 3 completed processes counted");
    }

    // F4-011: Rework completed foundation resets foundationIntegrationValid
    {
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        ProcessVersion f1(ProcessKind::Foundation, 1, 1, 1, 1, 1);
        pvState.completedHistory.append(f1);
        pvState.foundationIntegrationValid = true;

        QString reworkErr;
        bool rwOk = ProcessVersionLifecycle::reworkCompletedFoundation(&pvState, 1, &reworkErr);
        ok &= require(rwOk, "F4-011: reworkCompletedFoundation succeeds on completed F1");
        ok &= require(pvState.hasActiveProcess, "F4-011: F1 is now active process");
        ok &= require(pvState.activeProcess.isFoundation() && pvState.activeProcess.foundationNumber() == 1,
                       "F4-011: Active process is Foundation 1");
        ok &= require(pvState.activeProcess.iteration == 2, "F4-011: Iteration incremented to 2");
        ok &= require(!pvState.foundationIntegrationValid, "F4-011: foundationIntegrationValid reset to false");
    }

    // F4-012: Two-sided P6 gating validation (positive when ready, negative when blocked)
    {
        ProcessVersionState state;
        state.namespaceVersion = 2;
        // Negative test: uncertified foundations block P6
        QString reason;
        ok &= require(!state.isP6Eligible(&reason), "F4-012: P6 blocked when foundations incomplete");
        ok &= require(reason.contains("Foundation F1"), "F4-012: Explicit reason names F1");

        // Positive test: mock all 4 foundations completed and certified + integration valid
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 2, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 3, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 4, 1, 1, 1, 1));
        state.foundationIntegrationValid = true;
        ok &= require(state.isP6Eligible(&reason), "F4-012: P6 eligible when all 4 foundations certified and integrated");
    }

    std::cerr << (ok ? "F4: ALL PASS\n" : "F4: SOME FAILURES\n");
    return ok;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cross-Foundation Integration Tests
// ═══════════════════════════════════════════════════════════════════════════════

bool runFoundationIntegrationTests()
{
    bool ok = true;
    std::cerr << "=== Cross-Foundation Integration Tests ===\n";

    // INT-F-001: Full integration validates in bootstrap order
    {
        TestFixture fx;
        ok &= require(fx.valid, "INT-F-001: Fixture initializes");
        fx.recordTask(QStringLiteral("Integration test task"));
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto report = FoundationIntegrationService::validate(fx.path(), &fx.model);
        ok &= require(report.valid, "INT-F-001: Full integration passes");
        ok &= require(report.bootstrapOrderValid, "INT-F-001: Bootstrap order valid");
        ok &= require(report.noCircularAuthority, "INT-F-001: No circular authority");
    }

    // INT-F-002: Bootstrap order is F1=1, F2=2, F3=3, F4=4
    {
        ok &= require(FoundationIntegrationService::verifyBootstrapOrder(),
                       "INT-F-002: Bootstrap order verified as 1,2,3,4");
    }

    // INT-F-003: All contracts have correct dependency chains
    {
        const auto contracts = FoundationIntegrationService::allContracts();
        ok &= require(contracts.contains(QStringLiteral("F1")), "INT-F-003: F1 contract present");
        ok &= require(contracts.contains(QStringLiteral("F2")), "INT-F-003: F2 contract present");
        ok &= require(contracts.contains(QStringLiteral("F3")), "INT-F-003: F3 contract present");
        ok &= require(contracts.contains(QStringLiteral("F4")), "INT-F-003: F4 contract present");

        // F1 depends on nothing
        const auto f1Deps = contracts.value(QStringLiteral("F1")).toObject()
            .value(QStringLiteral("dependsOn")).toArray();
        ok &= require(f1Deps.isEmpty(), "INT-F-003: F1 depends on nothing");

        // F4 depends on F1, F2, F3
        const auto f4Deps = contracts.value(QStringLiteral("F4")).toObject()
            .value(QStringLiteral("dependsOn")).toArray();
        ok &= require(f4Deps.size() == 3, "INT-F-003: F4 depends on 3 foundations");
    }

    // INT-F-004: F1 failure propagates to integration report
    {
        TestFixture fx;
        ok &= require(fx.valid, "INT-F-004: Fixture initializes");
        // Inject ledger corruption with duplicate eventId
        const QString logPath = QDir(fx.path()).filePath(
            QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        const auto existingEvents = fx.memory.events(fx.path(), nullptr);
        QString dupId = QStringLiteral("event-int-duplicate");
        if (!existingEvents.isEmpty()) {
            dupId = existingEvents.first().value(QStringLiteral("eventId")).toString();
        }
        QFile logFile(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QJsonObject dupEvent{
                {QStringLiteral("eventId"), dupId},
                {QStringLiteral("eventType"), QStringLiteral("TEST_RESULT")},
                {QStringLiteral("sequenceNumber"), 999},
                {QStringLiteral("timestamp"), QStringLiteral("2026-09-18T00:00:00Z")},
                {QStringLiteral("task"), QStringLiteral("dup")},
                {QStringLiteral("provenance"), QJsonObject{
                    {QStringLiteral("actor"), QStringLiteral("system")},
                    {QStringLiteral("agentId"), QStringLiteral("test")},
                    {QStringLiteral("tool"), QStringLiteral("test")}
                }}
            };
            logFile.write(QJsonDocument(dupEvent).toJson(QJsonDocument::Compact) + "\n");
            logFile.close();
        }
        const auto report = FoundationIntegrationService::validate(fx.path(), &fx.model);
        ok &= require(!report.f1.valid, "INT-F-004: F1 failure detected");
        ok &= require(!report.valid, "INT-F-004: Integration fails when F1 fails");
    }

    // INT-F-005: All four sub-reports are populated
    {
        TestFixture fx;
        ok &= require(fx.valid, "INT-F-005: Fixture initializes");
        fx.recordTask(QStringLiteral("INT-F-005 task"));
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto report = FoundationIntegrationService::validate(fx.path(), &fx.model);
        ok &= require(report.f1.totalEvents > 0, "INT-F-005: F1 events counted");
        ok &= require(!report.f2.recognizedActors.isEmpty() || report.f2.eventsWithProvenance > 0,
                       "INT-F-005: F2 provenance data populated");
        ok &= require(report.f3.canonicalScopeCount == 15, "INT-F-005: F3 scope count populated");
        ok &= require(report.f4.namespaceVersionCorrect, "INT-F-005: F4 namespace check populated");
    }

    // INT-F-006: Evidence stored by F1 is validated by F2 provenance
    {
        TestFixture fx;
        ok &= require(fx.valid, "INT-F-006: Fixture initializes");
        fx.recordTask(QStringLiteral("Cross-foundation evidence task"));
        // F1 should see the event
        const auto f1Summary = MemoryEvidenceFoundation::evidenceSummary(fx.path());
        ok &= require(f1Summary.value(QStringLiteral("totalEvents")).toInt() >= 2,
                       "INT-F-006: F1 sees recorded events");
        // F2 should validate the provenance on those events
        const auto f2Report = IdentityTrustFoundation::validate(fx.path());
        ok &= require(f2Report.allProvenanceValid, "INT-F-006: F2 validates provenance of F1 events");
    }

    // INT-F-007: Scope validated by F3 is consistent with events stored by F1
    {
        TestFixture fx;
        ok &= require(fx.valid, "INT-F-007: Fixture initializes");
        const QJsonObject prov{
            {QStringLiteral("actor"), QStringLiteral("agent")},
            {QStringLiteral("agentId"), QStringLiteral("test")},
            {QStringLiteral("tool"), QStringLiteral("test")}
        };
        fx.memory.recordOperation(fx.path(), QStringLiteral("task-start"),
            QJsonObject{{QStringLiteral("task"), QStringLiteral("Scoped task")},
                        {QStringLiteral("scope"), QStringLiteral("source-code")},
                        {QStringLiteral("provenance"), prov}}, nullptr, nullptr);
        const auto f3Report = ScopeIntegrityFoundation::validate(fx.path(), &fx.model);
        ok &= require(f3Report.scopeTaxonomyValid, "INT-F-007: F3 validates scope of F1 events");
    }

    // INT-F-008: Lifecycle state validated by F4 matches project.json
    {
        TestFixture fx;
        ok &= require(fx.valid, "INT-F-008: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.completedHistory.append(ProcessVersion(1, 1, 1, 1, 1));
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto f4Report = LifecycleCertificationFoundation::validate(fx.path());
        ok &= require(f4Report.valid, "INT-F-008: F4 validates lifecycle state from project.json");
        ok &= require(f4Report.completedProcesses == 1, "INT-F-008: F4 counts 1 completed process");
    }

    // INT-F-009: Full report contains all foundation sections
    {
        TestFixture fx;
        ok &= require(fx.valid, "INT-F-009: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto report = FoundationIntegrationService::validate(fx.path(), &fx.model);
        ok &= require(report.fullReport.contains(QStringLiteral("f1")), "INT-F-009: Full report has f1");
        ok &= require(report.fullReport.contains(QStringLiteral("f2")), "INT-F-009: Full report has f2");
        ok &= require(report.fullReport.contains(QStringLiteral("f3")), "INT-F-009: Full report has f3");
        ok &= require(report.fullReport.contains(QStringLiteral("f4")), "INT-F-009: Full report has f4");
    }

    // INT-F-010: No circular authority - dependency DAG is acyclic
    {
        const auto contracts = FoundationIntegrationService::allContracts();
        // Build adjacency: each foundation depends on what its contract says
        QSet<QString> visited;
        bool acyclic = true;
        // Simple check: F1 has no deps, F2 depends on F1 only,
        // F3 depends on F1+F2, F4 depends on F1+F2+F3.
        // No foundation depends on a later-ordered foundation.
        for (const auto& key : {QStringLiteral("F1"), QStringLiteral("F2"),
                                 QStringLiteral("F3"), QStringLiteral("F4")}) {
            const auto deps = contracts.value(key).toObject()
                .value(QStringLiteral("dependsOn")).toArray();
            for (const auto& d : deps) {
                if (!visited.contains(d.toString())) {
                    // Dependency on a foundation not yet visited means it should
                    // have been processed earlier in bootstrap order
                    // Only flag if it's a LATER foundation
                    const int depOrder = contracts.value(d.toString()).toObject()
                        .value(QStringLiteral("bootstrapOrder")).toInt();
                    const int myOrder = contracts.value(key).toObject()
                        .value(QStringLiteral("bootstrapOrder")).toInt();
                    if (depOrder >= myOrder) {
                        acyclic = false;
                    }
                }
            }
            visited.insert(key);
        }
        ok &= require(acyclic, "INT-F-010: Dependency DAG is acyclic (no forward references)");
    }

    // INT-F-011: Failure injection - F2 trust failure doesn't crash F3/F4
    {
        TestFixture fx;
        ok &= require(fx.valid, "INT-F-011: Fixture initializes");
        // Inject event with malformed provenance
        const QString logPath = QDir(fx.path()).filePath(
            QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        QFile logFile(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QJsonObject badEvent{
                {QStringLiteral("eventId"), QStringLiteral("event-bad-prov-int")},
                {QStringLiteral("eventType"), QStringLiteral("TEST_RESULT")},
                {QStringLiteral("sequenceNumber"), 800},
                {QStringLiteral("timestamp"), QStringLiteral("2026-09-18T00:00:00Z")},
                {QStringLiteral("task"), QStringLiteral("bad prov")},
                {QStringLiteral("provenance"), QStringLiteral("not-an-object")}
            };
            logFile.write(QJsonDocument(badEvent).toJson(QJsonDocument::Compact) + "\n");
            logFile.close();
        }
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        const auto report = FoundationIntegrationService::validate(fx.path(), &fx.model);
        // F2 should fail but F3 and F4 should still produce valid reports
        ok &= require(!report.f2.valid, "INT-F-011: F2 reports failure");
        // F3 and F4 should still produce results (not crash)
        ok &= require(report.f3.canonicalScopeCount == 15,
                       "INT-F-011: F3 still produces scope count despite F2 failure");
        ok &= require(report.f4.namespaceVersionCorrect,
                       "INT-F-011: F4 still validates namespace despite F2 failure");
    }

    // FAIL-INJ-001: Missing project.json doesn't crash F4
    {
        QTemporaryDir emptyDir;
        // Create minimal ARAMF_WORKER structure
        QDir(emptyDir.path()).mkpath(QStringLiteral("ARAMF_WORKER/memory"));
        const auto report = LifecycleCertificationFoundation::validate(emptyDir.path());
        ok &= require(!report.valid, "FAIL-INJ-001: F4 reports failure on missing project.json");
        ok &= require(!report.errors.isEmpty(), "FAIL-INJ-001: Error message provided");
    }

    // FAIL-INJ-002: Empty event log passes F1 (no violations possible)
    {
        TestFixture fx;
        ok &= require(fx.valid, "FAIL-INJ-002: Fixture initializes");
        // Fresh fixture has only initialization events, which should be valid
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(report.valid, "FAIL-INJ-002: Fresh fixture passes F1");
    }

    // FAIL-INJ-003: Corrupt manifest detected by F1
    {
        TestFixture fx;
        ok &= require(fx.valid, "FAIL-INJ-003: Fixture initializes");
        fx.recordTask(QStringLiteral("FAIL-INJ task"));
        // Corrupt the manifest's event count
        const QString manifestPath = QDir(fx.path()).filePath(
            QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
        QFile mf(manifestPath);
        if (mf.open(QIODevice::ReadOnly)) {
            auto doc = QJsonDocument::fromJson(mf.readAll()).object();
            mf.close();
            doc.insert(QStringLiteral("eventCount"), 9999);
            if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                mf.write(QJsonDocument(doc).toJson());
                mf.close();
            }
        }
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!report.manifestConsistent,
                       "FAIL-INJ-003: Corrupt manifest event count detected by F1");
    }

    // XPROC-001: Cross-process regression - P1-P5 certified processes survive foundation validation
    {
        TestFixture fx;
        ok &= require(fx.valid, "XPROC-001: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        // Add P1-P5 as completed and certified
        pvState.completedHistory.append(ProcessVersion(1, 1, 1, 1, 1));
        pvState.completedHistory.append(ProcessVersion(2, 1, 1, 1, 1));
        pvState.completedHistory.append(ProcessVersion(3, 1, 4, 1, 1));
        pvState.completedHistory.append(ProcessVersion(4, 1, 4, 1, 1));
        pvState.completedHistory.append(ProcessVersion(5, 1, 4, 1, 1));
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        pvState.foundationQueue = ProcessVersionState::canonicalFoundationQueue();
        pvState.hasFutureProcess = true;
        pvState.futureProcess = ProcessVersion(6, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);
        fx.recordTask(QStringLiteral("XPROC regression task"));

        const auto report = FoundationIntegrationService::validate(fx.path(), &fx.model);
        ok &= require(report.f4.completedProcesses == 5,
                       "XPROC-001: 5 completed processes preserved");
        ok &= require(report.f4.processHistoryValid,
                       "XPROC-001: P1-P5 process history remains valid");
        ok &= require(report.f4.p6GatingCorrect,
                       "XPROC-001: P6 gating correct with incomplete foundations");
    }

    // XPROC-002: True multi-process boundary test (Phase 10)
    // Process A: independent OS process (aramf.exe) records governed Foundation evidence and exits
    // Process B: independent validation of durable F1, F2, F3, F4 state
    {
        TestFixture fx;
        ok &= require(fx.valid, "XPROC-002: Fixture initializes");
        const QString aramfExe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("aramf.exe"));
        ok &= require(QFile::exists(aramfExe), "XPROC-002: aramf.exe must exist in build directory");

        // Process A: records an operational event via CLI
        QProcess procA;
        procA.start(aramfExe, {
            QStringLiteral("memory"), QStringLiteral("record"),
            QStringLiteral("--project"), fx.path(),
            QStringLiteral("--operation"), QStringLiteral("task-complete"),
            QStringLiteral("--task"), QStringLiteral("CrossProcess-A-task"),
            QStringLiteral("--status"), QStringLiteral("PASS"),
            QStringLiteral("--actor"), QStringLiteral("agent"),
            QStringLiteral("--agent-id"), QStringLiteral("process-a-agent"),
            QStringLiteral("--tool"), QStringLiteral("aramf-cli"),
            QStringLiteral("--scope"), QStringLiteral("source-code")
        });
        bool procAFinished = procA.waitForFinished(15000) && procA.exitStatus() == QProcess::NormalExit && procA.exitCode() == 0;
        ok &= require(procAFinished, "XPROC-002: Process A executes and exits normally with code 0");

        // Process B: executes aramf foundation validate as an independent OS process
        QProcess procB;
        procB.start(aramfExe, {
            QStringLiteral("foundation"), QStringLiteral("validate"),
            QStringLiteral("--project"), fx.path()
        });
        bool procBFinished = procB.waitForFinished(15000) && procB.exitStatus() == QProcess::NormalExit && procB.exitCode() == 0;
        ok &= require(procBFinished, "XPROC-002: Process B executes 'aramf foundation validate' and exits with code 0");

        // Independent in-process verification of durable state produced by Process A
        const auto f1 = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(f1.valid, "XPROC-002: Process B verifies F1 evidence integrity");
        ok &= require(f1.ledgerIntact, "XPROC-002: Process B confirms ledger intact");

        const auto f2 = IdentityTrustFoundation::validate(fx.path());
        ok &= require(f2.valid, "XPROC-002: Process B verifies F2 provenance and actor identity");

        const auto f3 = ScopeIntegrityFoundation::validate(fx.path(), &fx.model);
        ok &= require(f3.valid, "XPROC-002: Process B verifies F3 scope integrity");

        const auto f4 = LifecycleCertificationFoundation::validate(fx.path());
        ok &= require(f4.valid, "XPROC-002: Process B verifies F4 lifecycle state");
    }

    // FAIL-INJ-004: Malformed JSONL line in event log is detected as ledger corruption
    {
        TestFixture fx;
        ok &= require(fx.valid, "FAIL-INJ-004: Fixture initializes");
        const QString logPath = QDir(fx.path()).filePath(
            QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        QFile logFile(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            logFile.write("{corrupt json line unclosed\n");
            logFile.close();
        }
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!report.valid, "FAIL-INJ-004: Malformed JSONL line causes F1 validation failure");
    }

    // FAIL-INJ-005: Unauthorized administrative deletion attempt blocked by trust boundary
    {
        QString trustErr;
        bool safe = IdentityTrustFoundation::respectsTrustBoundary(
            QStringLiteral("Normal user prompt without admin authorization"),
            QStringLiteral("rmdir /s /q /some/path"),
            &trustErr);
        ok &= require(!safe, "FAIL-INJ-005: Unauthorized destructive deletion is blocked by trust boundary");
    }

    // ARCH-001: FoundationServices.h does not include Process-layer headers
    {
        const QString headerPath = QDir(AramfPaths::programRoot()).filePath(QStringLiteral("src/core/FoundationServices.h"));
        QFile hf(headerPath);
        if (hf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(hf.readAll());
            hf.close();
            ok &= require(!content.contains("ValidationRouting.h"), "ARCH-001: FoundationServices.h does not include ValidationRouting.h");
            ok &= require(!content.contains("PredictiveOptimizationService.h"), "ARCH-001: FoundationServices.h does not include PredictiveOptimizationService.h");
            ok &= require(!content.contains("AdaptiveRoutingService.h"), "ARCH-001: FoundationServices.h does not include AdaptiveRoutingService.h");
            ok &= require(!content.contains("ExecutionOrchestrator.h"), "ARCH-001: FoundationServices.h does not include ExecutionOrchestrator.h");
        }
    }

    // ARCH-002: FoundationServices.cpp does not include Process-layer headers
    {
        const QString cppPath = QDir(AramfPaths::programRoot()).filePath(QStringLiteral("src/core/FoundationServices.cpp"));
        QFile cf(cppPath);
        if (cf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(cf.readAll());
            cf.close();
            ok &= require(!content.contains("ValidationRouting.h"), "ARCH-002: FoundationServices.cpp does not include ValidationRouting.h");
            ok &= require(!content.contains("PredictiveOptimizationService.h"), "ARCH-002: FoundationServices.cpp does not include PredictiveOptimizationService.h");
            ok &= require(!content.contains("AdaptiveRoutingService.h"), "ARCH-002: FoundationServices.cpp does not include AdaptiveRoutingService.h");
            ok &= require(!content.contains("ExecutionOrchestrator.h"), "ARCH-002: FoundationServices.cpp does not include ExecutionOrchestrator.h");
        }
    }

    // DEP-MAT-001: Machine-tested P <-> F dependency matrix
    {
        const auto matrix = FoundationIntegrationService::dependencyMatrix();
        ok &= require(matrix.contains("processes"), "DEP-MAT-001: Matrix contains processes");
        ok &= require(matrix.contains("foundations"), "DEP-MAT-001: Matrix contains foundations");
        const auto procs = matrix.value("processes").toObject();
        ok &= require(procs.contains("P1") && procs.contains("P6"), "DEP-MAT-001: P1 and P6 present in matrix");
        const auto p6 = procs.value("P6").toObject();
        ok &= require(p6.value("gatedOn").toArray().size() == 4, "DEP-MAT-001: P6 is gated on all 4 foundations");
    }

    // EVID-001: Canonical foundation-integration.json evidence artifact
    {
        TestFixture fx;
        ok &= require(fx.valid, "EVID-001: Fixture initializes");
        ProcessVersionState pvState;
        pvState.namespaceVersion = 2;
        pvState.hasNextProcess = true;
        pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        fx.writeProcessVersion(pvState);

        auto report = FoundationIntegrationService::validate(fx.path(), &fx.model);
        QString wErr;
        bool wOk = FoundationIntegrationService::writeIntegrationEvidence(fx.path(), report, &wErr);
        ok &= require(wOk, "EVID-001: writeIntegrationEvidence succeeds");

        QString rErr;
        const auto evid = FoundationIntegrationService::readIntegrationEvidence(fx.path(), &rErr);
        ok &= require(!evid.isEmpty(), "EVID-001: readIntegrationEvidence reads artifact");
        ok &= require(evid.value("diagnosticOnly").toBool() == true, "EVID-001: Artifact records diagnosticOnly=true");
        ok &= require(evid.value("authoritative").toBool() == false, "EVID-001: Artifact records authoritative=false");
        ok &= require(evid.value("acceptanceType").toString() == "DIAGNOSTIC_RESULT", "EVID-001: Artifact acceptanceType is DIAGNOSTIC_RESULT");
    }

    std::cerr << (ok ? "Integration: ALL PASS\n" : "Integration: SOME FAILURES\n");
    return ok;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Combined Foundation Test Runner
// ═══════════════════════════════════════════════════════════════════════════════

bool runF1FoundationTests(const QString& /*selfRepoPath*/)
{
    return runF1MemoryEvidenceTests();
}

bool runF2FoundationTests(const QString& /*selfRepoPath*/)
{
    return runF2IdentityTrustTests();
}

bool runF3FoundationTests(const QString& /*selfRepoPath*/)
{
    return runF3ScopeIntegrityTests();
}

bool runF4FoundationTests(const QString& /*selfRepoPath*/)
{
    return runF4LifecycleCertificationTests();
}

bool runFoundationIntegrationTestSuite(const QString& /*selfRepoPath*/)
{
    return runFoundationIntegrationTests();
}

bool runAllFoundationTests(const QString& selfRepoPath)
{
    bool ok = true;
    ok &= runF1FoundationTests(selfRepoPath);
    ok &= runF2FoundationTests(selfRepoPath);
    ok &= runF3FoundationTests(selfRepoPath);
    ok &= runF4FoundationTests(selfRepoPath);
    ok &= runFoundationIntegrationTestSuite(selfRepoPath);
    return ok;
}
