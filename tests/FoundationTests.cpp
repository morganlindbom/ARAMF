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
#include "core/ProjectPersistence.h"
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

    // Save the project model file (ARAMF_WORKER.aramf.json)
    bool saveProjectFile(const QString& relativeName = QStringLiteral("ARAMF_WORKER.aramf.json")) {
        const QString resolved = QDir(path()).filePath(relativeName);
        ProjectPersistence persistence;
        QString err;
        return persistence.save(model, resolved, &err);
    }

    // Write a valid processVersion block into project.json and sync model
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

        // Also update ARAMF_WORKER.aramf.json and reload model
        const QString paramf = QDir(path()).filePath(QStringLiteral("ARAMF_WORKER.aramf.json"));
        ProjectPersistence persistence;
        if (QFile::exists(paramf)) {
            QFile pf(paramf);
            if (pf.open(QIODevice::ReadOnly)) {
                auto pDoc = QJsonDocument::fromJson(pf.readAll()).object();
                pf.close();
                pDoc.insert(QStringLiteral("processVersion"), processVersionStateToJson(pvState));
                if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    pf.write(QJsonDocument(pDoc).toJson());
                    pf.close();
                }
            }
        } else {
            persistence.save(model, paramf, nullptr);
            QFile pf(paramf);
            if (pf.open(QIODevice::ReadOnly)) {
                auto pDoc = QJsonDocument::fromJson(pf.readAll()).object();
                pf.close();
                pDoc.insert(QStringLiteral("processVersion"), processVersionStateToJson(pvState));
                if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    pf.write(QJsonDocument(pDoc).toJson());
                    pf.close();
                }
            }
        }
        persistence.load(&model, paramf, nullptr);
        return true;
    }
};

struct GitTestFixture : public TestFixture {
    QString gitSha;

    GitTestFixture() : TestFixture() {
        if (valid) {
            initGitRepo();
        }
    }

    bool initGitRepo() {
        QProcess git;
        git.setWorkingDirectory(path());
        git.start(QStringLiteral("git"), {QStringLiteral("init")});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;

        git.start(QStringLiteral("git"), {QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@aramf.org")});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;

        git.start(QStringLiteral("git"), {QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("ARAMF Test")});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;

        git.start(QStringLiteral("git"), {QStringLiteral("add"), QStringLiteral("-A")});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;

        git.start(QStringLiteral("git"), {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial")});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;

        git.start(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;
        gitSha = QString::fromLocal8Bit(git.readAllStandardOutput()).trimmed();
        return !gitSha.isEmpty();
    }

    bool commitAll(const QString& msg = QStringLiteral("update")) {
        QProcess git;
        git.setWorkingDirectory(path());
        git.start(QStringLiteral("git"), {QStringLiteral("add"), QStringLiteral("-A")});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;
        git.start(QStringLiteral("git"), {QStringLiteral("commit"), QStringLiteral("-m"), msg});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;
        git.start(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
        if (!git.waitForFinished(5000) || git.exitCode() != 0) return false;
        gitSha = QString::fromLocal8Bit(git.readAllStandardOutput()).trimmed();
        return !gitSha.isEmpty();
    }
};

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════════
// F1 Tests: Memory & Evidence Foundation
// ═══════════════════════════════════════════════════════════════════════════════

bool runF1MemoryEvidenceTests(const QString& selfRepoPath = QString())
{
    bool ok = true;
    std::cerr << "=== F1: Memory & Evidence Foundation Tests ===\n";

    const auto writeJsonFile = [](const QString& path, const QJsonObject& object) {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        file.close();
        return true;
    };
    const auto readFileBytes = [](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QByteArray();
        return file.readAll();
    };

    QString resolvedRepoPath = selfRepoPath;
    if (resolvedRepoPath.isEmpty() || !QFileInfo(QDir(resolvedRepoPath).filePath(QStringLiteral("CMakeLists.txt"))).isFile()) {
        resolvedRepoPath = AramfPaths::programRoot();
    }

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

    // F1-011: Zero upward/peer dependency architecture test
    {
        const QString hPath = QDir(resolvedRepoPath).filePath(QStringLiteral("src/core/MemoryEvidenceFoundation.h"));
        const QString cppPath = QDir(resolvedRepoPath).filePath(QStringLiteral("src/core/MemoryEvidenceFoundation.cpp"));
        ok &= require(QFile::exists(hPath), "F1-011: MemoryEvidenceFoundation.h exists");
        ok &= require(QFile::exists(cppPath), "F1-011: MemoryEvidenceFoundation.cpp exists");

        QFile hFile(hPath);
        ok &= require(hFile.open(QIODevice::ReadOnly | QIODevice::Text), "F1-011: hFile opens");
        const QString hContent = QString::fromUtf8(hFile.readAll());
        hFile.close();

        QFile cppFile(cppPath);
        ok &= require(cppFile.open(QIODevice::ReadOnly | QIODevice::Text), "F1-011: cppFile opens");
        const QString cppContent = QString::fromUtf8(cppFile.readAll());
        cppFile.close();

        const QString combined = hContent + "\n" + cppContent;
        const QStringList forbiddenSubsystems = {
            QStringLiteral("IdentityTrustFoundation"),
            QStringLiteral("ScopeIntegrityFoundation"),
            QStringLiteral("LifecycleCertificationFoundation"),
            QStringLiteral("WorkerTaskServices"),
            QStringLiteral("WorkerContextResolver"),
            QStringLiteral("ContextCoordinationService"),
            QStringLiteral("ExecutionOrchestrator"),
            QStringLiteral("PredictiveOptimizationService"),
            QStringLiteral("AdaptiveRoutingService"),
            QStringLiteral("ValidationRouting")
        };
        for (const auto& forbidden : forbiddenSubsystems) {
            ok &= require(!combined.contains(forbidden),
                           qPrintable(QStringLiteral("F1-011: F1 has zero dependency on %1").arg(forbidden)));
        }
    }

    // F1-012: True process-boundary durability test
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-012: Fixture initializes");
        const QString aramfExe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("aramf.exe"));
        ok &= require(QFile::exists(aramfExe), "F1-012: aramf.exe must exist in build directory");

        // Process A: independent OS process writes governed evidence and exits
        QProcess procA;
        procA.start(aramfExe, {
            QStringLiteral("memory"), QStringLiteral("record"),
            QStringLiteral("--project"), fx.path(),
            QStringLiteral("--operation"), QStringLiteral("task-complete"),
            QStringLiteral("--task"), QStringLiteral("F1-ProcessA-Durability-Task"),
            QStringLiteral("--status"), QStringLiteral("PASS"),
            QStringLiteral("--actor"), QStringLiteral("agent"),
            QStringLiteral("--agent-id"), QStringLiteral("proc-a-agent"),
            QStringLiteral("--tool"), QStringLiteral("f1-durability"),
            QStringLiteral("--scope"), QStringLiteral("source-code")
        });
        bool procAOk = procA.waitForFinished(15000) && procA.exitStatus() == QProcess::NormalExit && procA.exitCode() == 0;
        ok &= require(procAOk, "F1-012: Process A writes evidence and exits with code 0");

        // Process B: independent OS process validates F1 evidence
        QProcess procB;
        procB.start(aramfExe, {
            QStringLiteral("foundation"), QStringLiteral("f1-validate"),
            QStringLiteral("--project"), fx.path()
        });
        bool procBOk = procB.waitForFinished(15000) && procB.exitStatus() == QProcess::NormalExit && procB.exitCode() == 0;
        ok &= require(procBOk, "F1-012: Process B executes f1-validate and exits with code 0");
        const QString outB = QString::fromUtf8(procB.readAllStandardOutput());
        ok &= require(outB.contains(QStringLiteral("F1-VALIDATION: PASS")),
                       "F1-012: Process B reports machine-readable F1-VALIDATION: PASS");
    }

    // F1-013: Hardened manifest reconstruction tests
    {
        TestFixture fx;
        ok &= require(fx.valid, "F1-013: Fixture initializes");
        fx.recordTask(QStringLiteral("F1-013 task 1"));
        fx.recordTask(QStringLiteral("F1-013 task 2"));

        const QString logPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        const QString manifestPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));

        // Case 1: Valid ledger reconstructs manifest
        QFile::remove(manifestPath);
        QString err1;
        bool ok1 = MemoryEvidenceFoundation::reconstructManifestFromLedger(fx.path(), &err1);
        ok &= require(ok1, "F1-013: Reconstructs valid manifest when file is missing");
        auto rep1 = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(rep1.manifestConsistent, "F1-013: Reconstructed manifest is consistent with ledger");

        // Case 2: Malformed JSON line is never skipped, causes failure
        {
            QFile lf(logPath);
            if (lf.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                lf.write("{malformed-json-never-skipped\n");
                lf.close();
            }
            QString err2;
            bool ok2 = MemoryEvidenceFoundation::reconstructManifestFromLedger(fx.path(), &err2);
            ok &= require(!ok2, "F1-013: Rejects reconstruction when malformed JSON line present");
            ok &= require(err2.contains(QStringLiteral("malformed JSON")), "F1-013: Returns malformed JSON error");
        }
    }

    // F1-014: Durability Recovery Tests (Cases A, B, C, D)
    {
        // Case A: Missing manifest
        {
            TestFixture fx;
            fx.recordTask(QStringLiteral("Case A task"));
            const QString manifestPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
            QFile::remove(manifestPath);
            QString err;
            bool recOk = MemoryEvidenceFoundation::recoverPhysicalState(fx.path(), &err);
            ok &= require(recOk, "F1-014: Case A recovery succeeds for missing manifest");
            ok &= require(QFile::exists(manifestPath), "F1-014: Manifest restored");
            auto rep = MemoryEvidenceFoundation::validate(fx.path());
            ok &= require(rep.valid, "F1-014: Validation passes after Case A recovery");
        }

        // Case B: Missing metrics
        {
            TestFixture fx;
            fx.recordTask(QStringLiteral("Case B task"));
            const QString metricsPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/metrics.json"));
            QFile::remove(metricsPath);
            QString err;
            bool recOk = MemoryEvidenceFoundation::recoverPhysicalState(fx.path(), &err);
            ok &= require(recOk, "F1-014: Case B recovery succeeds for missing metrics");
            ok &= require(QFile::exists(metricsPath), "F1-014: Metrics restored");
            auto rep = MemoryEvidenceFoundation::validate(fx.path());
            ok &= require(rep.metricsConsistent, "F1-014: Metrics consistent after Case B recovery");
        }

        // Case C: Missing derived current-state
        {
            TestFixture fx;
            fx.recordTask(QStringLiteral("Case C task"));
            const QString csPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/current-state.md"));
            QFile::remove(csPath);
            QString err;
            bool recOk = MemoryEvidenceFoundation::recoverPhysicalState(fx.path(), &err);
            ok &= require(recOk, "F1-014: Case C recovery succeeds for missing current-state");
            ok &= require(QFile::exists(csPath), "F1-014: Current state restored");
        }

        // Case D: Corrupt authoritative ledger fails closed
        {
            TestFixture fx;
            const QString logPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
            QFile lf(logPath);
            if (lf.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                lf.write("corrupt-ledger-entry\n");
                lf.close();
            }
            QString err;
            bool recOk = MemoryEvidenceFoundation::recoverPhysicalState(fx.path(), &err);
            ok &= require(!recOk, "F1-014: Case D recovery fails closed on corrupt ledger");
        }
    }

    // F1-015: Physical Certification Ledger Tests
    {
        // Malformed JSONL
        {
            TestFixture fx;
            const QString certPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
            QDir(fx.path()).mkpath(QStringLiteral("ARAMF_WORKER/certification"));
            QFile cf(certPath);
            if (cf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                cf.write("{corrupt unclosed json\n");
                cf.close();
            }
            auto rep = MemoryEvidenceFoundation::validate(fx.path());
            ok &= require(!rep.certificatesIntact, "F1-015: Malformed certification JSONL detected");
        }
        // Duplicate certificateId
        {
            TestFixture fx;
            const QString certPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
            QDir(fx.path()).mkpath(QStringLiteral("ARAMF_WORKER/certification"));
            QFile cf(certPath);
            if (cf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                QJsonObject c1{{QStringLiteral("certificateId"), QStringLiteral("cert-dup-1")},
                               {QStringLiteral("subject"), QStringLiteral("P1")}};
                QJsonObject c2{{QStringLiteral("certificateId"), QStringLiteral("cert-dup-1")},
                               {QStringLiteral("subject"), QStringLiteral("P2")}};
                cf.write(QJsonDocument(c1).toJson(QJsonDocument::Compact) + "\n");
                cf.write(QJsonDocument(c2).toJson(QJsonDocument::Compact) + "\n");
                cf.close();
            }
            auto rep = MemoryEvidenceFoundation::validate(fx.path());
            ok &= require(!rep.certificatesIntact, "F1-015: Duplicate certificateId detected");
        }
        // Missing certificateId
        {
            TestFixture fx;
            const QString certPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
            QDir(fx.path()).mkpath(QStringLiteral("ARAMF_WORKER/certification"));
            QFile cf(certPath);
            if (cf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                QJsonObject c1{{QStringLiteral("subject"), QStringLiteral("P1")}};
                cf.write(QJsonDocument(c1).toJson(QJsonDocument::Compact) + "\n");
                cf.close();
            }
            auto rep = MemoryEvidenceFoundation::validate(fx.path());
            ok &= require(!rep.certificatesIntact, "F1-015: Missing certificateId detected");
        }
    }

    // F1-016: F1 Query API Tests
    {
        TestFixture fx;
        fx.recordTask(QStringLiteral("Query API task 1"));
        fx.recordTask(QStringLiteral("Query API task 2"));

        const auto records = MemoryEvidenceFoundation::evidenceRecords(fx.path());
        ok &= require(records.size() >= 4, "F1-016: evidenceRecords returns parsed events");

        const QString firstId = records.first().value(QStringLiteral("eventId")).toString();
        QJsonObject found;
        bool findOk = MemoryEvidenceFoundation::evidenceRecordById(fx.path(), firstId, &found);
        ok &= require(findOk && found.value(QStringLiteral("eventId")).toString() == firstId,
                       "F1-016: evidenceRecordById finds existing record");

        bool fakeFind = MemoryEvidenceFoundation::evidenceRecordById(fx.path(), QStringLiteral("non-existent-id"));
        ok &= require(!fakeFind, "F1-016: evidenceRecordById fails for missing ID");

        const auto rangeRecords = MemoryEvidenceFoundation::evidenceRecordsBySequenceRange(fx.path(), 1, 2);
        ok &= require(!rangeRecords.isEmpty() && rangeRecords.size() <= 2,
                       "F1-016: evidenceRecordsBySequenceRange filters by sequence");

        const QString fp = MemoryEvidenceFoundation::evidenceFingerprint(fx.path());
        ok &= require(!fp.isEmpty() && fp.length() == 64, "F1-016: evidenceFingerprint returns SHA-256 hex");

        const auto meta = MemoryEvidenceFoundation::sourceBindingMetadata(fx.path());
        ok &= require(meta.contains(QStringLiteral("evidenceFingerprint")), "F1-016: sourceBindingMetadata has fingerprint");
        ok &= require(meta.contains(QStringLiteral("nextSequenceNumber")), "F1-016: sourceBindingMetadata has sequence");

        const auto recon = MemoryEvidenceFoundation::reconstructionStatus(fx.path());
        ok &= require(recon.value(QStringLiteral("manifestConsistent")).toBool(), "F1-016: reconstructionStatus reports manifest consistent");

        const auto corrupt = MemoryEvidenceFoundation::physicalCorruptionStatus(fx.path());
        ok &= require(corrupt.value(QStringLiteral("physicallyValid")).toBool(), "F1-016: physicalCorruptionStatus reports physically valid");
    }

    // F1-017: Corrupted manifest recovery
    {
        TestFixture fx;
        fx.recordTask(QStringLiteral("F1-017 task"));
        const QString manifestPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
        QFile mf(manifestPath);
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            mf.write("{corrupt-manifest-json-data\n");
            mf.close();
        }

        auto reportBefore = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!reportBefore.manifestConsistent, "F1-017: Malformed manifest detected as inconsistent");
        ok &= require(reportBefore.recoveryRequired, "F1-017: Recovery flagged as required");

        bool recOk = MemoryEvidenceFoundation::recoverPhysicalState(fx.path());
        ok &= require(recOk, "F1-017: Recovery succeeds for malformed manifest");

        auto reportAfter = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(reportAfter.valid && reportAfter.manifestConsistent, "F1-017: Validation passes after recovery");
    }

    // F1-018: Truncated JSONL line detected
    {
        TestFixture fx;
        const QString logPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        QFile lf(logPath);
        if (lf.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            lf.write("{\"eventId\": \"truncated-event\", \"eventType\": \"TASK_START\"\n");
            lf.close();
        }

        auto rep = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!rep.ledgerIntact, "F1-018: Truncated JSON line detected as ledger violation");
        ok &= require(!rep.valid, "F1-018: Overall validation fails on truncated line");
    }

    // F1-019: Duplicate sequence number detected
    {
        TestFixture fx;
        fx.recordTask(QStringLiteral("F1-019 task"));
        const QString logPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        QFile lf(logPath);
        if (lf.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QJsonObject dupSeqEvent{
                {QStringLiteral("eventId"), QStringLiteral("event-dup-seq-1234")},
                {QStringLiteral("eventType"), QStringLiteral("TEST_RESULT")},
                {QStringLiteral("sequenceNumber"), 1}, // already used sequence
                {QStringLiteral("timestamp"), QStringLiteral("2026-09-18T00:00:00Z")},
                {QStringLiteral("task"), QStringLiteral("dup seq test")}
            };
            lf.write(QJsonDocument(dupSeqEvent).toJson(QJsonDocument::Compact) + "\n");
            lf.close();
        }

        auto rep = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!rep.sequenceMonotonic, "F1-019: Duplicate sequence number detected");
    }

    // F1-020: Unsupported future schema version
    {
        TestFixture fx;
        const QString manifestPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
        QFile mf(manifestPath);
        if (mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            auto doc = QJsonDocument::fromJson(mf.readAll()).object();
            mf.close();
            doc.insert(QStringLiteral("memoryVersion"), QStringLiteral("99.0"));
            if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                mf.write(QJsonDocument(doc).toJson(QJsonDocument::Indented));
                mf.close();
            }
        }

        auto rep = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!rep.schemaCompatible, "F1-020: Unsupported future memory version detected");
        ok &= require(!rep.valid, "F1-020: Overall validation fails on incompatible schema");
    }

    // F1-021: Canonical metrics activeEvents is validated and repaired without rewriting history
    {
        TestFixture fx;
        fx.recordTask(QStringLiteral("F1-021 metrics task"));
        const QString metricsPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/metrics.json"));
        QFile metricsFile(metricsPath);
        QJsonObject metrics;
        if (metricsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            metrics = QJsonDocument::fromJson(metricsFile.readAll()).object();
            metricsFile.close();
        }
        const int originalCreated = metrics.value(QStringLiteral("totalEventsCreated")).toInt();
        metrics.insert(QStringLiteral("activeEvents"), 0);
        ok &= require(writeJsonFile(metricsPath, metrics), "F1-021: Writes stale canonical metrics fixture");
        const auto stale = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!stale.metricsConsistent, "F1-021: Stale activeEvents is detected");
        ok &= require(MemoryEvidenceFoundation::recoverPhysicalState(fx.path()),
                       "F1-021: Recovery repairs stale activeEvents");
        const auto repaired = QJsonDocument::fromJson(readFileBytes(metricsPath)).object();
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(report.metricsConsistent, "F1-021: Metrics are consistent after repair");
        ok &= require(repaired.value(QStringLiteral("activeEvents")).toInt() == report.totalEvents,
                       "F1-021: activeEvents equals physical ledger count");
        ok &= require(repaired.value(QStringLiteral("totalEventsCreated")).toInt() == originalCreated,
                       "F1-021: totalEventsCreated semantic history is preserved");
        ok &= require(!repaired.contains(QStringLiteral("durableSequence"))
                          && !repaired.contains(QStringLiteral("totalEvents")),
                      "F1-021: F1 does not invent parallel metric fields");
    }

    // F1-022: Certification evidence references are physical, object-shaped, and fingerprint checked
    {
        const auto makeCertificate = [](const QJsonArray& evidence) {
            return QJsonObject{
                {QStringLiteral("certificateId"), QStringLiteral("cert-f1-evidence")},
                {QStringLiteral("subject"), QStringLiteral("F1 evidence fixture")},
                {QStringLiteral("evidenceReferences"), evidence}
            };
        };
        const auto certificatePath = [](const QString& root) {
            return QDir(root).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
        };

        {
            TestFixture fx;
            const QString certPath = certificatePath(fx.path());
            QDir().mkpath(QFileInfo(certPath).path());
            QFile certFile(certPath);
            certFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
            certFile.write(QJsonDocument(makeCertificate(QJsonArray{
                QJsonObject{{QStringLiteral("reference"), QStringLiteral("ARAMF_WORKER/certification/missing-evidence.json")},
                            {QStringLiteral("verified"), true}}
            })).toJson(QJsonDocument::Compact) + "\n");
            certFile.close();
            ok &= require(!MemoryEvidenceFoundation::validate(fx.path()).certificatesIntact,
                          "F1-022: Missing physical evidence file is detected");
        }

        {
            TestFixture fx;
            const QString certPath = certificatePath(fx.path());
            QDir().mkpath(QFileInfo(certPath).path());
            QFile certFile(certPath);
            certFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
            certFile.write(QJsonDocument(makeCertificate(QJsonArray{QStringLiteral("not-an-object")})).toJson(QJsonDocument::Compact) + "\n");
            certFile.close();
            ok &= require(!MemoryEvidenceFoundation::validate(fx.path()).certificatesIntact,
                          "F1-022: Malformed evidence reference is detected");
        }

        {
            TestFixture fx;
            const QString evidencePath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence.txt"));
            QFile evidenceFile(evidencePath);
            QDir().mkpath(QFileInfo(evidencePath).path());
            evidenceFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
            evidenceFile.write("physical evidence\n");
            evidenceFile.close();
            const QString certPath = certificatePath(fx.path());
            const QJsonObject certificate = makeCertificate(QJsonArray{
                QJsonObject{{QStringLiteral("reference"), QStringLiteral("ARAMF_WORKER/certification/evidence.txt")},
                            {QStringLiteral("verified"), true},
                            {QStringLiteral("fingerprint"), QStringLiteral("00")}}
            });
            QFile certFile(certPath);
            QDir().mkpath(QFileInfo(certPath).path());
            certFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
            certFile.write(QJsonDocument(certificate).toJson(QJsonDocument::Compact) + "\n");
            certFile.close();
            ok &= require(!MemoryEvidenceFoundation::validate(fx.path()).certificatesIntact,
                          "F1-022: Evidence fingerprint mismatch is detected");
        }
    }

    // F1-023: Current certification state resolves certificates under subjects
    {
        TestFixture fx;
        const QString certPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
        const QString statePath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/current-certification-state.json"));
        const QJsonObject certificate{
            {QStringLiteral("certificateId"), QStringLiteral("cert-existing")},
            {QStringLiteral("subject"), QStringLiteral("F1 state fixture")},
            {QStringLiteral("evidenceReferences"), QJsonArray{}}
        };
        QDir().mkpath(QFileInfo(certPath).path());
        QFile certFile(certPath);
        certFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        certFile.write(QJsonDocument(certificate).toJson(QJsonDocument::Compact) + "\n");
        certFile.close();
        ok &= require(writeJsonFile(statePath, QJsonObject{
            {QStringLiteral("version"), 1},
            {QStringLiteral("subjects"), QJsonObject{
                {QStringLiteral("F1 state fixture"), QJsonObject{{QStringLiteral("certificateId"), QStringLiteral("cert-missing")}}}
            }}
        }), "F1-023: Writes current certification state fixture");
        ok &= require(!MemoryEvidenceFoundation::validate(fx.path()).certificatesIntact,
                      "F1-023: Missing certificate referenced under subjects is detected");
    }

    // F1-024: Evidence query API fails closed on a malformed later ledger line
    {
        TestFixture fx;
        fx.recordTask(QStringLiteral("F1-024 query task"));
        const QString logPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
        QFile logFile(logPath);
        logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        logFile.write("{malformed-query-ledger-line\n");
        logFile.close();
        QString queryError;
        const auto records = MemoryEvidenceFoundation::evidenceRecords(fx.path(), &queryError);
        ok &= require(records.isEmpty() && !queryError.isEmpty(),
                      "F1-024: Malformed ledger causes evidenceRecords to return no partial records");
    }

    // F1-025: Certification ledger interruption preserves bytes and fails recovery closed
    {
        TestFixture fx;
        const QString certPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
        QDir().mkpath(QFileInfo(certPath).path());
        const QByteArray historical = QJsonDocument(QJsonObject{
            {QStringLiteral("certificateId"), QStringLiteral("cert-history")},
            {QStringLiteral("subject"), QStringLiteral("F1 interruption fixture")},
            {QStringLiteral("evidenceReferences"), QJsonArray{}}
        }).toJson(QJsonDocument::Compact) + "\n";
        const QByteArray truncated = "{\"certificateId\":\"cert-interrupted\",\"subject\":\"F1 interruption fixture\"";
        QFile certFile(certPath);
        certFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        certFile.write(historical);
        certFile.write(truncated);
        certFile.close();
        const QByteArray before = readFileBytes(certPath);
        const auto report = MemoryEvidenceFoundation::validate(fx.path());
        ok &= require(!report.certificatesIntact && !report.valid,
                      "F1-025: Interrupted certification ledger fails validation closed");
        QString recoveryError;
        ok &= require(!MemoryEvidenceFoundation::recoverPhysicalState(fx.path(), &recoveryError),
                      "F1-025: Recovery refuses ambiguous certification history");
        ok &= require(readFileBytes(certPath) == before,
                      "F1-025: Recovery preserves historical certification bytes");
    }

    // F1-026: Metrics persistence failure fails recovery closed
    {
        TestFixture fx;
        fx.recordTask(QStringLiteral("F1-026 metrics write failure task"));
        const QString metricsPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/metrics.json"));
        QFile::remove(metricsPath);
        ok &= require(QDir().mkdir(metricsPath), "F1-026: Creates blocking metrics path");
        QString recoveryError;
        ok &= require(!MemoryEvidenceFoundation::recoverPhysicalState(fx.path(), &recoveryError),
                      "F1-026: Recovery fails when metrics cannot be opened");
        ok &= require(!recoveryError.isEmpty(), "F1-026: Metrics write failure reports an error");
    }

    std::cerr << (ok ? "F1: ALL PASS\n" : "F1: SOME FAILURES\n");
    return ok;
}

// ═══════════════════════════════════════════════════════════════════════════════
// F1 Certification Tests: Evidence-Bound Certification & Lifecycle Completion
// Test IDs: F1-CERT-001 through F1-CERT-030
// ═══════════════════════════════════════════════════════════════════════════════

static F1CertificationEvidence makeCompleteF1Evidence(const QString& sourceRev = QStringLiteral("rev-test-12345"),
                                                      const QString& projectRoot = QString())
{
    F1CertificationEvidence ev;
    ev.foundation = QStringLiteral("F1");
    ev.foundationName = QStringLiteral("Memory & Evidence Foundation");
    ev.foundationVersion = QStringLiteral("F1.1.3");
    ev.sourceRevision = sourceRev;
    ev.verificationLevel = QStringLiteral("HOST_TEST");
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
    if (!projectRoot.isEmpty()) {
        ev.evidenceFingerprint = MemoryEvidenceFoundation::evidenceFingerprint(projectRoot);
    } else {
        ev.evidenceFingerprint = QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }
    ev.timestamp = QStringLiteral("2026-09-18T12:00:00Z");

    // SHA-256 of empty content (matching the empty log files we create below)
    const QString emptyHash = QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // Create physical evidence log files when projectRoot is provided
    if (!projectRoot.isEmpty()) {
        const QString checksDir = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence/checks"));
        QDir().mkpath(checksDir);
    }

    const auto req = F1CertificationEvidence::requiredCheckNames();
    for (const auto& name : req) {
        F1VerificationCheck c;
        c.name = name;
        c.command = name;
        F1CommandSpec spec;
        if (FoundationCertificationService::canonicalCommandSpec(name, &spec, nullptr)) {
            c.commandIdentity = spec.identity;
        }
        c.status = QStringLiteral("PASS");
        c.exitCode = 0;
        c.timestamp = ev.timestamp;
        c.sourceRevision = sourceRev;
        c.evidenceReference = QStringLiteral("ARAMF_WORKER/certification/evidence/checks/%1.log").arg(name);
        c.evidenceFingerprint = emptyHash;

        // Create physical empty log file to satisfy evidence chain validation
        if (!projectRoot.isEmpty()) {
            const QString logPath = QDir(projectRoot).filePath(c.evidenceReference);
            QFile logFile(logPath);
            if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                logFile.close(); // empty file — SHA-256 matches emptyHash
            }
        }

        ev.checks.append(c);
    }
    ev.updateDerivedFlags();
    return ev;
}

bool runF1CertificationTests()
{
    bool ok = true;
    std::cerr << "=== F1: Certification & Lifecycle Completion Tests ===\n";

    const QString defaultProjectFile = QStringLiteral("ARAMF_WORKER.aramf.json");
    const auto writeJsonFile = [](const QString& path, const QJsonObject& object) {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        file.close();
        return true;
    };

    // F1-CERT-001: Missing evidence cannot produce lifecycle cert=1
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-001: Fixture initializes");
        QString err;
        bool started = FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        ok &= require(started, "F1-CERT-001: startF1 succeeds");

        QJsonObject certObj;
        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            fx.gitSha, QString(), &certObj, &err);
        ok &= require(!certOk, "F1-CERT-001: certifyF1 fails without evidence artifact");
        ok &= require(!err.isEmpty(), "F1-CERT-001: error explains missing evidence artifact");

        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().hasActiveProcess, "F1-CERT-001: Still has active process");
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-001: Active process cert remains 0");
    }

    // F1-CERT-002: Incomplete evidence cannot produce lifecycle cert=1
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-002: Fixture initializes");
        QString err;
        bool started = FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        ok &= require(started, "F1-CERT-002: startF1 succeeds");

        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        ev.checks.last().status = QStringLiteral("FAIL");
        ev.checks.last().exitCode = 1;
        ev.updateDerivedFlags();
        QString relPath, sha;
        bool wOk = FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev, &relPath, &sha, &err);
        ok &= require(wOk, "F1-CERT-002: writeEvidenceArtifact succeeds");

        QJsonObject certObj;
        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            fx.gitSha, QString(), &certObj, &err);
        ok &= require(!certOk, "F1-CERT-002: certifyF1 fails with incomplete evidence");
        ok &= require(err.contains(QStringLiteral("incomplete")) || err.contains(QStringLiteral("did not PASS")),
                      "F1-CERT-002: error mentions incomplete evidence or failed check");

        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-002: Active process cert remains 0");
    }

    // F1-CERT-003: Failed CertificationService issue cannot produce lifecycle cert=1
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-003: Fixture initializes");
        QString err;
        bool started = FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        ok &= require(started, "F1-CERT-003: startF1 succeeds");

        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        bool wOk = FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev, nullptr, nullptr, &err);
        ok &= require(wOk, "F1-CERT-003: writeEvidenceArtifact succeeds");

        // Block CertificationService::issue by making certificates.jsonl a directory
        const QString certDir = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification"));
        QDir().mkpath(certDir);
        const QString certPath = QDir(certDir).filePath(QStringLiteral("certificates.jsonl"));
        QFile::remove(certPath);
        QDir().mkdir(certPath);

        QJsonObject certObj;
        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            fx.gitSha, QString(), &certObj, &err);
        ok &= require(!certOk, "F1-CERT-003: certifyF1 fails when issue fails");

        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-003: Active process cert remains 0");
    }

    // F1-CERT-004: Wrong Foundation (F2, F3, F4, P1) is rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-004: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);

        for (const auto& wrongFoundation : {QStringLiteral("F2"), QStringLiteral("F3"), QStringLiteral("F4"), QStringLiteral("P1")}) {
            auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
            ev.foundation = wrongFoundation;
            const QString artPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence/wrong-evidence.json"));
            QDir().mkpath(QFileInfo(artPath).path());
            QFile af(artPath);
            if (af.open(QIODevice::WriteOnly | QIODevice::Text)) {
                af.write(QJsonDocument(ev.toJson()).toJson());
                af.close();
            }

            QJsonObject certObj;
            bool certOk = FoundationCertificationService::certifyF1(
                fx.path(), defaultProjectFile,
                fx.gitSha, artPath, &certObj, &err);
            ok &= require(!certOk, QString("F1-CERT-004: certifyF1 rejects foundation %1").arg(wrongFoundation).toUtf8().constData());
        }

        // Also test CLI rejects --foundation F2
        QString cliOut, cliErr;
        QTextStream outStr(&cliOut), errStr(&cliErr);
        int cliRes = runFoundationCommand({QStringLiteral("foundation"), QStringLiteral("certify"),
                                           QStringLiteral("--project"), fx.path(),
                                           QStringLiteral("--foundation"), QStringLiteral("F2"),
                                           QStringLiteral("--source-revision"), fx.gitSha},
                                          outStr, errStr);
        ok &= require(cliRes != 0, "F1-CERT-004: CLI certify rejects --foundation F2");

        cliOut.clear(); cliErr.clear();
        int cliCompRes = runFoundationCommand({QStringLiteral("foundation"), QStringLiteral("complete"),
                                               QStringLiteral("--project"), fx.path(),
                                               QStringLiteral("--foundation"), QStringLiteral("F2")},
                                              outStr, errStr);
        ok &= require(cliCompRes != 0, "F1-CERT-004: CLI complete rejects --foundation F2");
    }

    // F1-CERT-005: Wrong lifecycle position is rejected
    {
        // Case A: F1 is already complete in history
        {
            GitTestFixture fx;
            ProcessVersionState pvState;
            pvState.namespaceVersion = 2;
            pvState.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 1, 1));
            pvState.hasNextProcess = true;
            pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 2, 1, 0, 0, 0);
            fx.writeProcessVersion(pvState);

            auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
            FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

            QString err;
            bool certOk = FoundationCertificationService::certifyF1(
                fx.path(), defaultProjectFile,
                fx.gitSha, QString(), nullptr, &err);
            ok &= require(!certOk, "F1-CERT-005: certifyF1 fails when F1 is already complete in history");
        }
        // Case B: Active process is not F1 (e.g. active is F2)
        {
            GitTestFixture fx;
            ProcessVersionState pvState;
            pvState.namespaceVersion = 2;
            pvState.hasActiveProcess = true;
            pvState.activeProcess = ProcessVersion(ProcessKind::Foundation, 2, 1, 1, 0, 0);
            fx.writeProcessVersion(pvState);

            auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
            FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

            QString err;
            bool certOk = FoundationCertificationService::certifyF1(
                fx.path(), defaultProjectFile,
                fx.gitSha, QString(), nullptr, &err);
            ok &= require(!certOk, "F1-CERT-005: certifyF1 fails when active process is F2");
        }
        // Case C: Inactive but next is not F1
        {
            GitTestFixture fx;
            ProcessVersionState pvState;
            pvState.namespaceVersion = 2;
            pvState.hasActiveProcess = false;
            pvState.hasNextProcess = true;
            pvState.nextProcess = ProcessVersion(ProcessKind::Foundation, 2, 1, 0, 0, 0);
            fx.writeProcessVersion(pvState);

            auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
            FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

            QString err;
            bool certOk = FoundationCertificationService::certifyF1(
                fx.path(), defaultProjectFile,
                fx.gitSha, QString(), nullptr, &err);
            ok &= require(!certOk, "F1-CERT-005: certifyF1 fails when next process is not F1");
        }
    }

    // F1-CERT-006: Empty sourceRevision is rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-006: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);

        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

        bool certEmpty = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            QStringLiteral(""), QString(), nullptr, &err);
        ok &= require(!certEmpty, "F1-CERT-006: Empty sourceRevision is rejected");

        bool certSpaces = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            QStringLiteral("   "), QString(), nullptr, &err);
        ok &= require(!certSpaces, "F1-CERT-006: Whitespace sourceRevision is rejected");
    }

    // F1-CERT-007: sourceRevision is persisted in certification evidence and bound in certificate
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-007: Fixture initializes");
        const QString testRev = fx.gitSha;
        auto ev = makeCompleteF1Evidence(testRev, fx.path());
        QString relPath, sha, err;
        bool wOk = FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev, &relPath, &sha, &err);
        ok &= require(wOk, "F1-CERT-007: Evidence artifact written");

        F1CertificationEvidence readBack;
        bool rOk = FoundationCertificationService::readEvidenceArtifact(QDir(fx.path()).filePath(relPath), &readBack, &err);
        ok &= require(rOk && readBack.sourceRevision == testRev, "F1-CERT-007: sourceRevision persisted in artifact");

        QJsonObject issuedCert;
        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            testRev, QString(), &issuedCert, &err);
        ok &= require(certOk, "F1-CERT-007: certifyF1 succeeds");
        ok &= require(issuedCert.value(QStringLiteral("sourceRevision")).toString() == testRev,
                      "F1-CERT-007: Certificate bound with sourceRevision");
        ok &= require(issuedCert.value(QStringLiteral("evidenceFingerprint")).toString().length() == 64,
                      "F1-CERT-007: Certificate bound with evidenceFingerprint");

        CertificationService certService;
        QJsonObject latest;
        ok &= require(certService.latestForSubject(fx.path(), QStringLiteral("F1"), &latest, &err),
                      "F1-CERT-007: Rediscover certificate");
        ok &= require(latest.value(QStringLiteral("sourceRevision")).toString() == testRev,
                      "F1-CERT-007: Rediscovered certificate contains sourceRevision");
    }

    // F1-CERT-008: CertificationService PASS precedes lifecycle certification
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-008: Fixture initializes");
        const QString testRev = fx.gitSha;
        auto ev = makeCompleteF1Evidence(testRev, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

        QString err;
        QJsonObject issuedCert;
        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            testRev, QString(), &issuedCert, &err);
        ok &= require(certOk, "F1-CERT-008: certifyF1 succeeds");

        ok &= require(issuedCert.value(QStringLiteral("result")).toString() == QStringLiteral("PASS"),
                      "F1-CERT-008: Certificate result is PASS");
        ok &= require(issuedCert.value(QStringLiteral("certificationStatus")).toString() == QStringLiteral("CERTIFIED"),
                      "F1-CERT-008: Certificate status is CERTIFIED");

        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 1,
                      "F1-CERT-008: Lifecycle activeProcess certification is 1");
    }

    // F1-CERT-009: F1 completion before certification is rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-009: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);

        bool compOk = FoundationCertificationService::completeF1(
            fx.path(), defaultProjectFile, &err);
        ok &= require(!compOk, "F1-CERT-009: completeF1 fails when cert=0");
        ok &= require(err.contains(QStringLiteral("not certified")),
                      "F1-CERT-009: error notes uncertified state");

        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().hasActiveProcess, "F1-CERT-009: Active process remains");
        ok &= require(m.processVersionState().activeProcess.done == 0, "F1-CERT-009: Active process done remains 0");
        ok &= require(!m.processVersionState().isFoundationComplete(1), "F1-CERT-009: F1 is not marked complete in history");
    }

    // F1-CERT-010: Successful fixture progression produces F1.1.1.0.0 -> F1.1.1.1.0 -> F1.1.1.1.1
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-010: Fixture initializes");
        ProjectModel m;
        ProjectPersistence p;
        const QString absPj = QDir(fx.path()).filePath(defaultProjectFile);

        // Step 1: Initial state (next F1.1.0.0.0, no active)
        p.load(&m, absPj, nullptr);
        ok &= require(m.processVersionState().nextIdentifier() == QStringLiteral("F1.1.0.0.0"),
                      "F1-CERT-010: Initial next is F1.1.0.0.0");
        ok &= require(!m.processVersionState().hasActiveProcess, "F1-CERT-010: Initial active is none");

        // Step 2: Start F1 -> active F1.1.1.0.0
        QString err;
        ok &= require(FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err),
                      "F1-CERT-010: startF1 succeeds");
        p.load(&m, absPj, nullptr);
        ok &= require(m.processVersionState().activeIdentifier() == QStringLiteral("F1.1.1.0.0"),
                      "F1-CERT-010: Active is F1.1.1.0.0 after start");

        // Step 3: Certify F1 -> active F1.1.1.1.0
        const QString testRev = fx.gitSha;
        auto ev = makeCompleteF1Evidence(testRev, fx.path());
        ok &= require(FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev, nullptr, nullptr, &err),
                      "F1-CERT-010: write evidence succeeds");
        bool c10Ok = FoundationCertificationService::certifyF1(fx.path(), defaultProjectFile, testRev, QString(), nullptr, &err);
        ok &= require(c10Ok, "F1-CERT-010: certifyF1 succeeds");
        p.load(&m, absPj, nullptr);
        ok &= require(m.processVersionState().activeIdentifier() == QStringLiteral("F1.1.1.1.0"),
                      "F1-CERT-010: Active is F1.1.1.1.0 after certification");

        // Step 4: Complete F1 -> completed F1.1.1.1.1
        ok &= require(FoundationCertificationService::completeF1(fx.path(), defaultProjectFile, &err),
                      "F1-CERT-010: completeF1 succeeds");
        p.load(&m, absPj, nullptr);
        ok &= require(m.processVersionState().isFoundationComplete(1),
                      "F1-CERT-010: F1 is complete in history");
        const auto& hist = m.processVersionState().completedHistory;
        ok &= require(!hist.isEmpty() && hist.last().identifier() == QStringLiteral("F1.1.1.1.1"),
                      "F1-CERT-010: Completed history record is F1.1.1.1.1");
    }

    // F1-CERT-011: After fixture F1 completion: active = null, next = F2.1.0.0.0, F2 remains not started
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-011: Fixture initializes");
        const QString absPj = QDir(fx.path()).filePath(defaultProjectFile);
        const QString testRev = fx.gitSha;

        auto ev = makeCompleteF1Evidence(testRev, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);
        FoundationCertificationService::certifyF1(fx.path(), defaultProjectFile, testRev);
        FoundationCertificationService::completeF1(fx.path(), defaultProjectFile);

        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, absPj, nullptr);
        const auto& st = m.processVersionState();

        ok &= require(!st.hasActiveProcess, "F1-CERT-011: Active process is null");
        ok &= require(st.hasNextProcess, "F1-CERT-011: Has next process");
        ok &= require(st.nextIdentifier() == QStringLiteral("F2.1.0.0.0"),
                      "F1-CERT-011: Next identifier is F2.1.0.0.0");
        ok &= require(st.nextProcess.isFoundation() && st.nextProcess.foundationNumber() == 2,
                      "F1-CERT-011: Next process is Foundation 2");
        ok &= require(st.nextProcess.certification == 0 && st.nextProcess.done == 0,
                      "F1-CERT-011: Next process F2 has cert=0, done=0 (not started)");
        ok &= require(!st.foundationIntegrationValid,
                      "F1-CERT-011: foundationIntegrationValid remains false");
        ok &= require(st.hasFutureProcess && st.futureProcess.processNumber() == 6,
                      "F1-CERT-011: P6 remains future");
    }

    // F1-CERT-012: Persisted/generated lifecycle representations remain equivalent throughout progression
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-012: Fixture initializes");
        const QString absPj = QDir(fx.path()).filePath(defaultProjectFile);
        const QString genPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/project.json"));
        const QString testRev = fx.gitSha;

        const auto checkEquivalence = [&](const char* phaseName) -> bool {
            Q_UNUSED(phaseName);
            ProjectModel m;
            ProjectPersistence p;
            p.load(&m, absPj, nullptr);
            QFile gf(genPath);
            if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
            const auto gDoc = QJsonDocument::fromJson(gf.readAll()).object();
            gf.close();
            ProcessVersionState gState;
            QString gErr;
            if (!processVersionStateFromJson(gDoc.value(QStringLiteral("processVersion")), &gState, &gErr)) return false;
            return processVersionStateToJson(m.processVersionState()) == processVersionStateToJson(gState);
        };

        ok &= require(checkEquivalence("Initial"), "F1-CERT-012: Equivalence at Initial phase");

        FoundationCertificationService::startF1(fx.path(), defaultProjectFile);
        ok &= require(checkEquivalence("Started"), "F1-CERT-012: Equivalence at Started phase");

        auto ev = makeCompleteF1Evidence(testRev, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);
        FoundationCertificationService::certifyF1(fx.path(), defaultProjectFile, testRev);
        ok &= require(checkEquivalence("Certified"), "F1-CERT-012: Equivalence at Certified phase");

        FoundationCertificationService::completeF1(fx.path(), defaultProjectFile);
        ok &= require(checkEquivalence("Completed"), "F1-CERT-012: Equivalence at Completed phase");
    }

    // F1-CERT-013: Fake source revision rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-013: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            QStringLiteral("0123456789abcdef0123456789abcdef01234567"), QString(), nullptr, &err);
        ok &= require(!certOk, "F1-CERT-013: Fake commit SHA rejected");
        ok &= require(err.contains(QStringLiteral("not a valid commit")), "F1-CERT-013: Error explains commit invalid");
        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-013: Active process cert remains 0");
    }

    // F1-CERT-014: Revision different from HEAD rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-014: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        const QString firstCommitSha = fx.gitSha;

        // Make a second commit so firstCommitSha is no longer HEAD
        QFile dummy(QDir(fx.path()).filePath(QStringLiteral("dummy.txt")));
        dummy.open(QIODevice::WriteOnly);
        dummy.write("second commit");
        dummy.close();
        QProcess git;
        git.setWorkingDirectory(fx.path());
        git.start(QStringLiteral("git"), {QStringLiteral("add"), QStringLiteral("dummy.txt")});
        git.waitForFinished(5000);
        git.start(QStringLiteral("git"), {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("second")});
        git.waitForFinished(5000);

        auto ev = makeCompleteF1Evidence(firstCommitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);
        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            firstCommitSha, QString(), nullptr, &err);
        ok &= require(!certOk, "F1-CERT-014: Revision different from HEAD rejected");
        ok &= require(err.contains(QStringLiteral("does not match current HEAD")), "F1-CERT-014: Error explains HEAD mismatch");
        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-014: Active process cert remains 0");
    }

    // F1-CERT-015: Dirty working tree rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-015: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

        // Create an untracked dirty file outside permitted certification files
        QFile dirty(QDir(fx.path()).filePath(QStringLiteral("dirty.txt")));
        dirty.open(QIODevice::WriteOnly);
        dirty.write("dirty content");
        dirty.close();

        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            fx.gitSha, QString(), nullptr, &err);
        ok &= require(!certOk, "F1-CERT-015: Dirty working tree rejected");
        ok &= require(err.contains(QStringLiteral("working tree is dirty")), "F1-CERT-015: Error explains dirty tree");
        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-015: Active process cert remains 0");
    }

    // F1-CERT-016: Result evidence bound to another source revision rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-016: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        // Mismatch one check's sourceRevision
        ev.checks[0].sourceRevision = QStringLiteral("other-revision-sha");
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            fx.gitSha, QString(), nullptr, &err);
        ok &= require(!certOk, "F1-CERT-016: Result evidence bound to another source revision rejected");
        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-016: Active process cert remains 0");
    }

    // F1-CERT-017: Missing generated state blocks certificate fail-closed
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-017: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);
        QFile::remove(QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/project.json")));

        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            fx.gitSha, QString(), nullptr, &err);
        ok &= require(!certOk, "F1-CERT-017: Missing generated state blocks certificate");
        ok &= require(err.contains(QStringLiteral("project.json")), "F1-CERT-017: Error mentions project.json");
        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-017: Active process cert remains 0");
    }

    // F1-CERT-018: Malformed generated state blocks certificate fail-closed
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-018: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);
        QFile f(QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/project.json")));
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write("{ broken json syntax :::: ");
        f.close();

        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            fx.gitSha, QString(), nullptr, &err);
        ok &= require(!certOk, "F1-CERT-018: Malformed generated state blocks certificate");
        ok &= require(err.contains(QStringLiteral("malformed")), "F1-CERT-018: Error explains malformed");
        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-018: Active process cert remains 0");
    }

    // F1-CERT-019: One FAIL check blocks certificate, lifecycle cert remains 0
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-019: Fixture initializes");
        QString err;
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        // Check 6 is p4-predictive - fail it
        ev.checks[6].status = QStringLiteral("FAIL");
        ev.checks[6].exitCode = 1;
        ev.updateDerivedFlags();
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev);

        bool certOk = FoundationCertificationService::certifyF1(
            fx.path(), defaultProjectFile,
            fx.gitSha, QString(), nullptr, &err);
        ok &= require(!certOk, "F1-CERT-019: One FAIL check blocks certificate");
        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, QDir(fx.path()).filePath(defaultProjectFile), nullptr);
        ok &= require(m.processVersionState().activeProcess.certification == 0, "F1-CERT-019: Active process cert remains 0");
    }

    // F1-CERT-020: Canonical rework of completed F1 preserves history, sets F1.1.2.0.0, supersedes certificate, and completes F1.1.2.1.1
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-020: Fixture initializes");
        QString err;
        const QString absPj = QDir(fx.path()).filePath(defaultProjectFile);

        // Step 1: Initial run: start, certify, complete F1.1.1
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile, &err);
        auto ev1 = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev1);
        QJsonObject cert1;
        bool cert1Ok = FoundationCertificationService::certifyF1(fx.path(), defaultProjectFile, fx.gitSha, QString(), &cert1, &err);
        ok &= require(cert1Ok, "F1-CERT-020: First certification succeeds");
        const QString cert1Id = cert1.value(QStringLiteral("certificateId")).toString();
        ok &= require(!cert1Id.isEmpty(), "F1-CERT-020: First certificate ID present");
        bool comp1Ok = FoundationCertificationService::completeF1(fx.path(), defaultProjectFile, &err);
        ok &= require(comp1Ok, "F1-CERT-020: First complete succeeds");

        ProjectModel m;
        ProjectPersistence p;
        p.load(&m, absPj, nullptr);
        ok &= require(m.processVersionState().isFoundationComplete(1), "F1-CERT-020: F1 complete in history");
        ok &= require(m.processVersionState().completedHistory.first().identifier() == QStringLiteral("F1.1.1.1.1"),
                      "F1-CERT-020: Completed history has F1.1.1.1.1");

        // Step 2: Rework completed F1 via reworkF1
        bool reworkOk = FoundationCertificationService::reworkF1(fx.path(), defaultProjectFile, &err);
        ok &= require(reworkOk, "F1-CERT-020: reworkF1 succeeds");
        p.load(&m, absPj, nullptr);
        ok &= require(m.processVersionState().hasActiveProcess, "F1-CERT-020: Has active process after rework");
        ok &= require(m.processVersionState().activeIdentifier() == QStringLiteral("F1.1.2.0.0"),
                      "F1-CERT-020: Active identifier is F1.1.2.0.0");
        ok &= require(m.processVersionState().completedHistory.first().identifier() == QStringLiteral("F1.1.1.1.1"),
                      "F1-CERT-020: Historical F1.1.1.1.1 remains intact in completedHistory");
        ok &= require(m.processVersionState().nextIdentifier() == QStringLiteral("F2.1.0.0.0"),
                      "F1-CERT-020: Next remains F2.1.0.0.0");
        ok &= require(m.processVersionState().nextProcess.certification == 0 && m.processVersionState().nextProcess.done == 0,
                      "F1-CERT-020: F2 not started");

        // Commit repository state for rework iteration so working tree is clean for fx.gitSha
        ok &= require(fx.commitAll(QStringLiteral("Rework F1 to F1.1.2.0.0")), "F1-CERT-020: Commit rework state");

        // Step 3: Certify rework iteration
        auto ev2 = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev2);
        QJsonObject cert2;
        bool cert2Ok = FoundationCertificationService::certifyF1(fx.path(), defaultProjectFile, fx.gitSha, QString(), &cert2, &err);
        ok &= require(cert2Ok, "F1-CERT-020: Certify rework succeeds");
        p.load(&m, absPj, nullptr);
        ok &= require(m.processVersionState().activeIdentifier() == QStringLiteral("F1.1.2.1.0"),
                      "F1-CERT-020: Active identifier is F1.1.2.1.0 after rework certification");
        ok &= require(cert2.value(QStringLiteral("previousCertificateId")).toString() == cert1Id,
                      "F1-CERT-020: New certificate references previous certificate");
        ok &= require(cert2.value(QStringLiteral("supersedesCertificateId")).toString() == cert1Id,
                      "F1-CERT-020: New certificate supersedes previous certificate");

        // Step 4: Verify current-certification-state resolves the corrected certificate
        CertificationService certService;
        QJsonObject latestCert;
        ok &= require(certService.latestForSubject(fx.path(), QStringLiteral("F1"), &latestCert),
                      "F1-CERT-020: latestForSubject succeeds");
        ok &= require(latestCert.value(QStringLiteral("certificateId")).toString() == cert2.value(QStringLiteral("certificateId")).toString(),
                      "F1-CERT-020: current-certification-state resolves the corrected certificate");

        // Step 5: Complete rework iteration
        bool comp2Ok = FoundationCertificationService::completeF1(fx.path(), defaultProjectFile, &err);
        ok &= require(comp2Ok, "F1-CERT-020: Complete rework succeeds");
        p.load(&m, absPj, nullptr);
        ok &= require(!m.processVersionState().hasActiveProcess, "F1-CERT-020: Active is null after complete");
        ok &= require(m.processVersionState().completedHistory.size() == 2, "F1-CERT-020: Completed history has 2 entries");
        ok &= require(m.processVersionState().completedHistory.first().identifier() == QStringLiteral("F1.1.1.1.1"),
                      "F1-CERT-020: Historical F1.1.1.1.1 preserved");
        ok &= require(m.processVersionState().completedHistory.last().identifier() == QStringLiteral("F1.1.2.1.1"),
                      "F1-CERT-020: Latest completed is F1.1.2.1.1");
        ok &= require(m.processVersionState().nextIdentifier() == QStringLiteral("F2.1.0.0.0"),
                      "F1-CERT-020: Next remains F2.1.0.0.0");
        ok &= require(m.processVersionState().nextProcess.certification == 0 && m.processVersionState().nextProcess.done == 0,
                      "F1-CERT-020: F2 remains not started");
        ok &= require(!m.processVersionState().foundationIntegrationValid,
                      "F1-CERT-020: foundationIntegrationValid is false");
    }

    // F1-CERT-021: Persisted and generated states remain equal throughout rework progression
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-021: Fixture initializes");
        const QString absPj = QDir(fx.path()).filePath(defaultProjectFile);
        const QString genPath = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/project.json"));

        const auto checkEquivalence = [&]() -> bool {
            ProjectModel m;
            ProjectPersistence p;
            p.load(&m, absPj, nullptr);
            QFile gf(genPath);
            if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
            const auto gDoc = QJsonDocument::fromJson(gf.readAll()).object();
            gf.close();
            ProcessVersionState gState;
            QString gErr;
            if (!processVersionStateFromJson(gDoc.value(QStringLiteral("processVersion")), &gState, &gErr)) return false;
            return processVersionStateToJson(m.processVersionState()) == processVersionStateToJson(gState);
        };

        // Complete F1 iteration 1
        FoundationCertificationService::startF1(fx.path(), defaultProjectFile);
        auto ev1 = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev1);
        FoundationCertificationService::certifyF1(fx.path(), defaultProjectFile, fx.gitSha);
        FoundationCertificationService::completeF1(fx.path(), defaultProjectFile);
        ok &= require(checkEquivalence(), "F1-CERT-021: Equivalence after F1.1.1 completion");

        // Rework F1
        FoundationCertificationService::reworkF1(fx.path(), defaultProjectFile);
        ok &= require(checkEquivalence(), "F1-CERT-021: Equivalence after rework to F1.1.2.0.0");
        ok &= require(fx.commitAll(QStringLiteral("Rework F1 in F1-CERT-021")), "F1-CERT-021: Commit rework state");

        // Certify rework
        auto ev2 = makeCompleteF1Evidence(fx.gitSha, fx.path());
        FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev2);
        FoundationCertificationService::certifyF1(fx.path(), defaultProjectFile, fx.gitSha);
        ok &= require(checkEquivalence(), "F1-CERT-021: Equivalence after certify rework F1.1.2.1.0");

        // Complete rework
        FoundationCertificationService::completeF1(fx.path(), defaultProjectFile);
        ok &= require(checkEquivalence(), "F1-CERT-021: Equivalence after complete rework F1.1.2.1.1");
    }


    // ═══════════════════════════════════════════════════════════════════════════
    // Evidence-Chain Integrity Tests (F1-CERT-022 through F1-CERT-030)
    // ═══════════════════════════════════════════════════════════════════════════

    // F1-CERT-022: Missing evidenceReference rejects isPassWithEvidence
    {
        F1VerificationCheck c;
        c.name = QStringLiteral("p1-governance");
        c.commandIdentity = QStringLiteral("aramf_core_tests::--p1-governance");
        c.status = QStringLiteral("PASS");
        c.exitCode = 0;
        c.sourceRevision = QStringLiteral("rev-test-12345");
        c.evidenceFingerprint = QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        c.evidenceReference = QString(); // empty

        QString err;
        ok &= require(!c.isPassWithEvidence(QStringLiteral("."), QStringLiteral("rev-test-12345"), &err),
                       "F1-CERT-022: Empty evidenceReference rejected");
        ok &= require(err.contains(QStringLiteral("empty evidenceReference")),
                       "F1-CERT-022: Error explains empty reference");
    }

    // F1-CERT-023: Missing log file rejects isPassWithEvidence
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-023: Fixture initializes");

        F1VerificationCheck c;
        c.name = QStringLiteral("p1-governance");
        c.commandIdentity = QStringLiteral("aramf_core_tests::--p1-governance");
        c.status = QStringLiteral("PASS");
        c.exitCode = 0;
        c.sourceRevision = fx.gitSha;
        c.evidenceReference = QStringLiteral("ARAMF_WORKER/certification/evidence/checks/nonexistent.log");
        c.evidenceFingerprint = QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

        QString err;
        ok &= require(!c.isPassWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-023: Missing log file rejected");
        ok &= require(err.contains(QStringLiteral("does not exist")),
                       "F1-CERT-023: Error explains missing file");
    }

    // F1-CERT-024: Tampered log (wrong fingerprint) rejects isPassWithEvidence
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-024: Fixture initializes");

        const QString checksDir = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence/checks"));
        QDir().mkpath(checksDir);
        const QString logPath = QDir(checksDir).filePath(QStringLiteral("tampered.log"));
        QFile logFile(logPath);
        logFile.open(QIODevice::WriteOnly);
        logFile.write("this is real output");
        logFile.close();

        F1VerificationCheck c;
        c.name = QStringLiteral("p1-governance");
        c.commandIdentity = QStringLiteral("aramf_core_tests::--p1-governance");
        c.status = QStringLiteral("PASS");
        c.exitCode = 0;
        c.sourceRevision = fx.gitSha;
        c.evidenceReference = QStringLiteral("ARAMF_WORKER/certification/evidence/checks/tampered.log");
        c.evidenceFingerprint = QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000"); // wrong

        QString err;
        ok &= require(!c.isPassWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-024: Tampered log rejected");
        ok &= require(err.contains(QStringLiteral("mismatch")),
                       "F1-CERT-024: Error explains fingerprint mismatch");
    }

    // F1-CERT-025: Path traversal rejects isPassWithEvidence
    {
        F1VerificationCheck c;
        c.name = QStringLiteral("p1-governance");
        c.commandIdentity = QStringLiteral("aramf_core_tests::--p1-governance");
        c.status = QStringLiteral("PASS");
        c.exitCode = 0;
        c.sourceRevision = QStringLiteral("rev-test-12345");
        c.evidenceFingerprint = QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        c.evidenceReference = QStringLiteral("ARAMF_WORKER/certification/evidence/../../memory/event-log.jsonl");

        QString err;
        ok &= require(!c.isPassWithEvidence(QStringLiteral("."), QStringLiteral("rev-test-12345"), &err),
                       "F1-CERT-025: Path traversal rejected");
        ok &= require(err.contains(QStringLiteral("outside permitted")),
                       "F1-CERT-025: Error explains scope violation");

        // Also test absolute path injection
        c.evidenceReference = QStringLiteral("/etc/passwd");
        ok &= require(!c.isPassWithEvidence(QStringLiteral("."), QStringLiteral("rev-test-12345"), &err),
                       "F1-CERT-025: Absolute path injection rejected");
    }

    // F1-CERT-026: Duplicate check name rejects isCompleteWithEvidence
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-026: Fixture initializes");

        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        // Add a duplicate of the first check
        auto dup = ev.checks.first();
        ev.checks.append(dup);

        QString err;
        ok &= require(!ev.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-026: Duplicate check name rejected");
        ok &= require(err.contains(QStringLiteral("Duplicate")),
                       "F1-CERT-026: Error explains duplicate check");
    }

    // F1-CERT-027: Stale/wrong source revision in individual check rejects
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-027: Fixture initializes");

        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        ev.checks[5].sourceRevision = QStringLiteral("stale-revision-does-not-match");
        ev.updateDerivedFlags();

        QString err;
        ok &= require(!ev.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-027: Stale source revision rejected");
        ok &= require(err.contains(QStringLiteral("does not match")),
                       "F1-CERT-027: Error explains revision mismatch");
    }

    // F1-CERT-028: Validation-generated dirty artifacts allowed by verifyGitSourceRevision
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-028: Fixture initializes");

        // Create the two validation files that the verification suite generates
        const QString memDir = QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory"));
        QDir().mkpath(memDir);
        QFile coldStart(QDir(memDir).filePath(QStringLiteral("cold-start-validation.json")));
        coldStart.open(QIODevice::WriteOnly);
        coldStart.write("{\"status\":\"PASS\"}");
        coldStart.close();
        QFile memConsist(QDir(memDir).filePath(QStringLiteral("memory-consistency-validation.json")));
        memConsist.open(QIODevice::WriteOnly);
        memConsist.write("{\"status\":\"PASS\"}");
        memConsist.close();

        QString err;
        bool gitOk = FoundationCertificationService::verifyGitSourceRevision(fx.path(), fx.gitSha, &err);
        ok &= require(gitOk, "F1-CERT-028: Validation-generated dirty artifacts allowed");
    }

    // F1-CERT-029: Ordinary dirty source file rejected by verifyGitSourceRevision
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-029: Fixture initializes");

        // Create a dirty source file that should NOT be allowed
        QFile dirty(QDir(fx.path()).filePath(QStringLiteral("src/dirty-code.cpp")));
        QDir().mkpath(QDir(fx.path()).filePath(QStringLiteral("src")));
        dirty.open(QIODevice::WriteOnly);
        dirty.write("// malicious code");
        dirty.close();

        QString err;
        bool gitOk = FoundationCertificationService::verifyGitSourceRevision(fx.path(), fx.gitSha, &err);
        ok &= require(!gitOk, "F1-CERT-029: Ordinary dirty source file rejected");
        ok &= require(err.contains(QStringLiteral("working tree is dirty")),
                       "F1-CERT-029: Error explains dirty tree");

        // Also verify that ARAMF_WORKER/memory/decisions.md is rejected (not in allowlist)
        QFile::remove(QDir(fx.path()).filePath(QStringLiteral("src/dirty-code.cpp")));
        QFile memDirty(QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/decisions.md")));
        QDir().mkpath(QDir(fx.path()).filePath(QStringLiteral("ARAMF_WORKER/memory")));
        memDirty.open(QIODevice::WriteOnly);
        memDirty.write("# dirty decision");
        memDirty.close();

        bool gitOk2 = FoundationCertificationService::verifyGitSourceRevision(fx.path(), fx.gitSha, &err);
        ok &= require(!gitOk2, "F1-CERT-029: Modified decisions.md rejected");
    }

    // F1-CERT-030: Complete evidence chain round-trip with isCompleteWithEvidence
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-030: Fixture initializes");

        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());

        QString err;
        ok &= require(ev.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-030: Complete evidence chain validates");

        // Verify all 13 required checks present
        ok &= require(ev.checks.size() == 13, "F1-CERT-030: Exactly 13 checks present");

        // Write and read back evidence artifact
        QString relPath, sha;
        bool wOk = FoundationCertificationService::writeEvidenceArtifact(fx.path(), ev, &relPath, &sha, &err);
        ok &= require(wOk, "F1-CERT-030: Evidence artifact written");

        F1CertificationEvidence readBack;
        bool rOk = FoundationCertificationService::readEvidenceArtifact(
            QDir(fx.path()).filePath(relPath), &readBack, &err);
        ok &= require(rOk, "F1-CERT-030: Evidence artifact read back");
        ok &= require(readBack.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-030: Read-back evidence chain validates");
    }

    // F1-CERT-031: Unexpected check names are rejected; evidence contains exactly the required checks
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-031: Fixture initializes");

        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        F1VerificationCheck extra;
        extra.name = QStringLiteral("unexpected-check");
        extra.command = QStringLiteral("unexpected");
        extra.status = QStringLiteral("PASS");
        extra.exitCode = 0;
        extra.sourceRevision = fx.gitSha;
        extra.evidenceReference = QStringLiteral(
            "ARAMF_WORKER/certification/evidence/checks/unexpected-check.log");
        extra.evidenceFingerprint = QStringLiteral(
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        QFile extraLog(QDir(fx.path()).filePath(extra.evidenceReference));
        ok &= require(extraLog.open(QIODevice::WriteOnly),
                       "F1-CERT-031: Extra evidence log can be created");
        extraLog.close();
        ev.checks.append(extra);

        QString err;
        ok &= require(!ev.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-031: Unexpected check rejected");
        ok &= require(err.contains(QStringLiteral("Unexpected")),
                       "F1-CERT-031: Error explains unexpected check");
    }

    // F1-CERT-032: Arbitrary command identity is rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-032: Fixture initializes");
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        ev.checks[3].commandIdentity = QStringLiteral("arbitrary-command");
        QString err;
        ok &= require(!ev.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-032: Arbitrary command identity rejected");
    }

    // F1-CERT-033: Command identity from another required check is rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-033: Fixture initializes");
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        F1CommandSpec other;
        FoundationCertificationService::canonicalCommandSpec(QStringLiteral("p2-context"), &other);
        ev.checks[3].commandIdentity = other.identity;
        QString err;
        ok &= require(!ev.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-033: Cross-check command identity rejected");
    }

    // F1-CERT-034: Missing command identity is rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-034: Fixture initializes");
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        ev.checks[3].commandIdentity.clear();
        QString err;
        ok &= require(!ev.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-034: Missing command identity rejected");
    }

    // F1-CERT-035: Canonical mapping is complete, unique, and path-independent
    {
        const auto names = F1CertificationEvidence::requiredCheckNames();
        QSet<QString> identities;
        bool mappingOk = true;
        for (const auto& name : names) {
            F1CommandSpec spec;
            mappingOk = FoundationCertificationService::canonicalCommandSpec(name, &spec, nullptr) && mappingOk;
            mappingOk = !spec.identity.isEmpty() && !identities.contains(spec.identity) && mappingOk;
            identities.insert(spec.identity);
        }
        F1CommandSpec unknown;
        ok &= require(mappingOk && identities.size() == names.size(),
                       "F1-CERT-035: All required checks have unique canonical identities");
        ok &= require(!FoundationCertificationService::canonicalCommandSpec(
                           QStringLiteral("unknown-check"), &unknown, nullptr),
                       "F1-CERT-035: Unknown check has no canonical identity");
        ok &= require(names.size() == 13, "F1-CERT-035: Exactly 13 canonical mappings exist");
        F1CommandSpec physical;
        FoundationCertificationService::canonicalCommandSpec(
            QStringLiteral("f1-physical-validation"), &physical, nullptr);
        ok &= require(physical.identity.contains(QStringLiteral("{PROJECT_ROOT}")),
                       "F1-CERT-035: Variable project path uses a semantic placeholder");
    }

    // F1-CERT-036: Altered canonical arguments are rejected
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-036: Fixture initializes");
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        ev.checks[3].commandIdentity = QStringLiteral("aramf_core_tests::--p1-governance --altered");
        QString err;
        ok &= require(!ev.isCompleteWithEvidence(fx.path(), fx.gitSha, &err),
                       "F1-CERT-036: Altered canonical arguments rejected");
    }

    // F1-CERT-037: loadVerificationChecks uses the same physical and identity validator
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-037: Fixture initializes");
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        const QString checksDir = QDir(fx.path()).filePath(
            QStringLiteral("ARAMF_WORKER/certification/evidence/checks"));
        for (const auto& check : ev.checks) {
            writeJsonFile(QDir(checksDir).filePath(check.name + QStringLiteral(".json")), check.toJson());
        }
        QList<F1VerificationCheck> loaded;
        QString err;
        ok &= require(FoundationCertificationService::loadVerificationChecks(
                           checksDir, fx.gitSha, &loaded, &err),
                       "F1-CERT-037: Canonical records load successfully");
        auto bad = ev.checks[3];
        bad.commandIdentity = QStringLiteral("arbitrary-command");
        writeJsonFile(QDir(checksDir).filePath(bad.name + QStringLiteral(".json")), bad.toJson());
        loaded.clear();
        ok &= require(!FoundationCertificationService::loadVerificationChecks(
                           checksDir, fx.gitSha, &loaded, &err),
                       "F1-CERT-037: Mismatched command identity rejected on load");
    }

    // F1-CERT-038: Certification rejects a command-identity mismatch
    {
        GitTestFixture fx;
        ok &= require(fx.valid, "F1-CERT-038: Fixture initializes");
        auto ev = makeCompleteF1Evidence(fx.gitSha, fx.path());
        ev.checks[3].commandIdentity = QStringLiteral("arbitrary-command");
        QString artifactPath;
        ok &= require(FoundationCertificationService::writeEvidenceArtifact(
                           fx.path(), ev, &artifactPath, nullptr, nullptr),
                       "F1-CERT-038: Mismatched artifact written");
        QString err;
        ok &= require(!FoundationCertificationService::certifyF1(
                           fx.path(), defaultProjectFile, fx.gitSha, artifactPath, nullptr, &err),
                       "F1-CERT-038: Certification rejects command mismatch");
    }

    std::cerr << (ok ? "F1 Certification: ALL PASS\n" : "F1 Certification: SOME FAILURES\n");
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

bool runF1FoundationTests(const QString& selfRepoPath)
{
    bool ok = true;
    ok &= runF1MemoryEvidenceTests(selfRepoPath);
    ok &= runF1CertificationTests();
    return ok;
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
