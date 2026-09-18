#include "core/AramfPaths.h"
#include "core/ProcessVersion.h"
#include "core/ProjectMemory.h"
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <iostream>

namespace {
bool check(bool condition, const char* tag, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL [" << tag << "]: " << message << "\n";
        return false;
    }
    std::cout << "PASS [" << tag << "]: " << message << "\n";
    return true;
}
} // namespace

bool runProcessNamespaceMigrationTests(const QString& selfRepoPath)
{
    std::cout << "Starting Process Namespace Migration test matrix (MIG-001 through MIG-020)...\n";
    const QString repoRoot = (selfRepoPath.isEmpty() || !QDir(selfRepoPath).exists(QStringLiteral("ARAMF_WORKER")))
        ? AramfPaths::programRoot()
        : selfRepoPath;
    bool allPass = true;
    QString error;

    // MIG-001: Legacy P0 maps deterministically to canonical P1 (Task Execution Governance)
    {
        int canonical = 0;
        bool ok = check(ProcessVersion::mapLegacyToCanonical(0, &canonical, &error) && canonical == 1,
                        "MIG-001", "Legacy P0 maps deterministically to canonical P1");
        ok &= check(ProcessVersion::processName(0, ProcessNamespace::LegacyV1) == QStringLiteral("Task Execution Governance"),
                    "MIG-001", "Legacy P0 name is Task Execution Governance");
        ok &= check(ProcessVersion::processName(1, ProcessNamespace::CanonicalV2) == QStringLiteral("Task Execution Governance"),
                    "MIG-001", "Canonical P1 name is Task Execution Governance");
        allPass &= ok;
    }

    // MIG-002: Legacy P1 maps deterministically to canonical P2 (Context Coordination)
    {
        int canonical = 0;
        bool ok = check(ProcessVersion::mapLegacyToCanonical(1, &canonical, &error) && canonical == 2,
                        "MIG-002", "Legacy P1 maps deterministically to canonical P2");
        ok &= check(ProcessVersion::processName(1, ProcessNamespace::LegacyV1) == QStringLiteral("Context Coordination"),
                    "MIG-002", "Legacy P1 name is Context Coordination");
        ok &= check(ProcessVersion::processName(2, ProcessNamespace::CanonicalV2) == QStringLiteral("Context Coordination"),
                    "MIG-002", "Canonical P2 name is Context Coordination");
        allPass &= ok;
    }

    // MIG-003: Legacy P2 maps deterministically to canonical P3 (Execution Orchestration)
    {
        int canonical = 0;
        bool ok = check(ProcessVersion::mapLegacyToCanonical(2, &canonical, &error) && canonical == 3,
                        "MIG-003", "Legacy P2 maps deterministically to canonical P3");
        ok &= check(ProcessVersion::processName(2, ProcessNamespace::LegacyV1) == QStringLiteral("Execution Orchestration"),
                    "MIG-003", "Legacy P2 name is Execution Orchestration");
        ok &= check(ProcessVersion::processName(3, ProcessNamespace::CanonicalV2) == QStringLiteral("Execution Orchestration"),
                    "MIG-003", "Canonical P3 name is Execution Orchestration");
        allPass &= ok;
    }

    // MIG-004: Legacy P3 maps deterministically to canonical P4 (Predictive Task Optimization)
    {
        int canonical = 0;
        bool ok = check(ProcessVersion::mapLegacyToCanonical(3, &canonical, &error) && canonical == 4,
                        "MIG-004", "Legacy P3 maps deterministically to canonical P4");
        ok &= check(ProcessVersion::processName(3, ProcessNamespace::LegacyV1) == QStringLiteral("Predictive Task Optimization"),
                    "MIG-004", "Legacy P3 name is Predictive Task Optimization");
        ok &= check(ProcessVersion::processName(4, ProcessNamespace::CanonicalV2) == QStringLiteral("Predictive Task Optimization"),
                    "MIG-004", "Canonical P4 name is Predictive Task Optimization");
        allPass &= ok;
    }

    // MIG-005: Legacy P4 maps deterministically to canonical P5 (Self-Adjusting Routing)
    {
        int canonical = 0;
        bool ok = check(ProcessVersion::mapLegacyToCanonical(4, &canonical, &error) && canonical == 5,
                        "MIG-005", "Legacy P4 maps deterministically to canonical P5");
        ok &= check(ProcessVersion::processName(4, ProcessNamespace::LegacyV1) == QStringLiteral("Self-Adjusting Routing"),
                    "MIG-005", "Legacy P4 name is Self-Adjusting Routing");
        ok &= check(ProcessVersion::processName(5, ProcessNamespace::CanonicalV2) == QStringLiteral("Self-Adjusting Routing"),
                    "MIG-005", "Canonical P5 name is Self-Adjusting Routing");
        allPass &= ok;
    }

    // MIG-006: Legacy P5-P12 map deterministically to canonical P6-P13
    {
        bool ok = true;
        for (int p = 5; p <= 12; ++p) {
            int canonical = 0;
            ok &= check(ProcessVersion::mapLegacyToCanonical(p, &canonical, &error) && canonical == (p + 1),
                        "MIG-006", qPrintable(QStringLiteral("Legacy P%1 maps to canonical P%2").arg(p).arg(p + 1)));
            const QString legacyName = ProcessVersion::processName(p, ProcessNamespace::LegacyV1);
            const QString canonicalName = ProcessVersion::processName(p + 1, ProcessNamespace::CanonicalV2);
            ok &= check(legacyName == canonicalName,
                        "MIG-006", qPrintable(QStringLiteral("Name for legacy P%1 matches canonical P%2: %3").arg(p).arg(p + 1).arg(legacyName)));
        }
        allPass &= ok;
    }

    // MIG-007: P14 has no false legacy mapping
    {
        int legacy = -1;
        bool maps = ProcessVersion::mapCanonicalToLegacy(14, &legacy, &error);
        bool ok = check(!maps, "MIG-007", "Canonical P14 has no legacy mapping");
        ok &= check(!error.isEmpty(), "MIG-007", "Error explains P14 has no legacy process equivalent");
        QString legacyStr = ProcessNamespaceService::canonicalToLegacy(QStringLiteral("P14.1.0.0.0"), &error);
        ok &= check(legacyStr.isEmpty(), "MIG-007", "canonicalToLegacy string conversion fails for P14");
        allPass &= ok;
    }

    // MIG-008: F1 is parsed as a foundation identity, not a P process
    {
        ProcessVersion v;
        bool parsed = ProcessVersion::parse(QStringLiteral("F1.1.0.0.0"), &v, &error);
        bool ok = check(parsed, "MIG-008", "F1.1.0.0.0 parsed successfully");
        ok &= check(v.isFoundation(), "MIG-008", "v.isFoundation() is true");
        ok &= check(!v.isProcess(), "MIG-008", "v.isProcess() is false");
        ok &= check(v.prefix() == QStringLiteral("F"), "MIG-008", "prefix is F");
        ok &= check(v.foundationNumber() == 1, "MIG-008", "foundationNumber is 1");
        ok &= check(v.processNumber() == 0, "MIG-008", "processNumber is 0");
        ok &= check(v.canonicalName() == QStringLiteral("Memory & Evidence Foundation"),
                    "MIG-008", "F1 canonicalName is Memory & Evidence Foundation");
        allPass &= ok;
    }

    // MIG-009: F1.1.0.0.0 serializes/deserializes deterministically
    {
        ProcessVersion v(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        QJsonObject json = processVersionToJson(v);
        bool ok = check(json.contains(QStringLiteral("foundation")), "MIG-009", "JSON contains foundation key");
        ok &= check(!json.contains(QStringLiteral("process")), "MIG-009", "JSON does NOT contain process key");
        ok &= check(json.value(QStringLiteral("foundation")).toInt() == 1, "MIG-009", "foundation number is 1");
        ok &= check(json.value(QStringLiteral("loop")).toInt() == 1, "MIG-009", "loop is 1");
        ok &= check(json.value(QStringLiteral("iteration")).toInt() == 0, "MIG-009", "iteration is 0");
        ok &= check(json.value(QStringLiteral("certification")).toInt() == 0, "MIG-009", "certification is 0");
        ok &= check(json.value(QStringLiteral("done")).toInt() == 0, "MIG-009", "done is 0");

        ProcessVersion deserialized;
        bool loaded = processVersionFromJson(json, &deserialized, &error);
        ok &= check(loaded, "MIG-009", "deserialized successfully");
        ok &= check(deserialized == v, "MIG-009", "deserialized object matches original");
        ok &= check(deserialized.identifier() == QStringLiteral("F1.1.0.0.0"), "MIG-009", "deserialized identifier is F1.1.0.0.0");
        allPass &= ok;
    }

    // MIG-010: Historical recorder entries preserve their original legacy process identity
    {
        QFile eventLog(QDir(repoRoot).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl")));
        bool ok = check(eventLog.open(QIODevice::ReadOnly | QIODevice::Text), "MIG-010", "event-log.jsonl exists and opened");
        if (ok) {
            QString content = QString::fromUtf8(eventLog.readAll());
            ok &= check(content.contains(QStringLiteral("P3.1.4.1.1")), "MIG-010", "Historical events contain original P3.1.4.1.1");
            ok &= check(content.contains(QStringLiteral("P4.1.1")), "MIG-010", "Historical events contain original P4.1.1");
            ok &= check(content.contains(QStringLiteral("P4.1.4.1.1")), "MIG-010", "Historical events contain original P4.1.4.1.1");
            ok &= check(!content.contains(QStringLiteral("canonical:P4.1.4.1.1")), "MIG-010", "Historical events were NOT rewritten with synthetic canonical labels");
        }
        allPass &= ok;
    }

    // MIG-011: Canonical readers correctly resolve legacy identities
    {
        bool ok = check(ProcessNamespaceService::resolveToCanonical(QStringLiteral("P0.1.2.1.1")) == QStringLiteral("P1.1.2.1.1"),
                        "MIG-011", "P0.1.2.1.1 resolves to P1.1.2.1.1");
        ok &= check(ProcessNamespaceService::resolveToCanonical(QStringLiteral("legacy:P3.1.4.1.1")) == QStringLiteral("P4.1.4.1.1"),
                    "MIG-011", "legacy:P3.1.4.1.1 resolves to P4.1.4.1.1");
        ok &= check(ProcessNamespaceService::resolveToCanonical(QStringLiteral("legacy:P4.1.4.1.1")) == QStringLiteral("P5.1.4.1.1"),
                    "MIG-011", "legacy:P4.1.4.1.1 resolves to P5.1.4.1.1");
        ok &= check(ProcessNamespaceService::resolveToCanonical(QStringLiteral("canonical:P5.1.4.1.1")) == QStringLiteral("P5.1.4.1.1"),
                    "MIG-011", "canonical:P5.1.4.1.1 resolves to P5.1.4.1.1");
        ok &= check(ProcessNamespaceService::resolveToCanonical(QStringLiteral("F1.1.0.0.0")) == QStringLiteral("F1.1.0.0.0"),
                    "MIG-011", "F1.1.0.0.0 resolves to F1.1.0.0.0");
        allPass &= ok;
    }

    // MIG-012: Current process state exposes canonical P1-P5 numbering
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        const QStringList completed = state.completedIdentifiers();
        bool ok = check(completed.contains(QStringLiteral("P1.1.2.1.1")), "MIG-012", "Completed history contains canonical P1.1.2.1.1");
        ok &= check(completed.contains(QStringLiteral("P2.1.1.1.1")), "MIG-012", "Completed history contains canonical P2.1.1.1.1");
        ok &= check(completed.contains(QStringLiteral("P3.1.4.1.1")), "MIG-012", "Completed history contains canonical P3.1.4.1.1");
        ok &= check(completed.contains(QStringLiteral("P4.1.4.1.1")), "MIG-012", "Completed history contains canonical P4.1.4.1.1");
        ok &= check(completed.contains(QStringLiteral("P5.1.4.1.1")), "MIG-012", "Completed history contains canonical P5.1.4.1.1");
        allPass &= ok;
    }

    // MIG-013: Current next foundation state is F1.1.0.0.0
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        bool ok = check(state.nextIdentifier() == QStringLiteral("F1.1.0.0.0"), "MIG-013", "nextIdentifier() is F1.1.0.0.0");
        ok &= check(state.nextProcess.isFoundation(), "MIG-013", "nextProcess is Foundation");
        ok &= check(state.nextProcess.foundationNumber() == 1, "MIG-013", "nextProcess foundation number is 1");
        ok &= check(state.nextProcess.iteration == 0 && state.nextProcess.certification == 0 && state.nextProcess.done == 0,
                    "MIG-013", "nextProcess is unstarted (iter=0, cert=0, done=0)");
        allPass &= ok;
    }

    // MIG-014: Future Canonical Code Bank resolves to P6, not P5
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        bool ok = check(ProcessVersion::processName(6, ProcessNamespace::CanonicalV2) == QStringLiteral("Canonical Code Bank"),
                        "MIG-014", "Canonical P6 name is Canonical Code Bank");
        ok &= check(state.futureIdentifier() == QStringLiteral("P6.1.0.0.0"), "MIG-014", "futureIdentifier() is P6.1.0.0.0");
        ok &= check(state.futureProcess.processNumber() == 6, "MIG-014", "futureProcess is process 6");
        allPass &= ok;
    }

    // MIG-015: No ambiguity exists between legacy P5 and canonical P5
    {
        const QString legacyP5 = ProcessVersion::processName(5, ProcessNamespace::LegacyV1);
        const QString canonicalP5 = ProcessVersion::processName(5, ProcessNamespace::CanonicalV2);
        const QString canonicalP6 = ProcessVersion::processName(6, ProcessNamespace::CanonicalV2);
        bool ok = check(legacyP5 == QStringLiteral("Canonical Code Bank"), "MIG-015", "Legacy P5 is Canonical Code Bank");
        ok &= check(canonicalP5 == QStringLiteral("Self-Adjusting Routing"), "MIG-015", "Canonical P5 is Self-Adjusting Routing");
        ok &= check(canonicalP6 == QStringLiteral("Canonical Code Bank"), "MIG-015", "Canonical P6 is Canonical Code Bank");
        ok &= check(legacyP5 != canonicalP5, "MIG-015", "Legacy P5 and Canonical P5 names are completely distinct");
        allPass &= ok;
    }

    // MIG-016: Certification freshness survives identity migration without fabricating fresh certification
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        bool ok = true;
        for (const auto& v : state.completedHistory) {
            ok &= check(v.certification == 1, "MIG-016", qPrintable(QStringLiteral("Version %1 has certification=1").arg(v.identifier())));
            ok &= check(v.done == 1, "MIG-016", qPrintable(QStringLiteral("Version %1 has done=1").arg(v.identifier())));
        }
        allPass &= ok;
    }

    // MIG-017: Historical certification evidence retains original provenance
    {
        ProjectMemory memory;
        error.clear();
        QList<QJsonObject> events = memory.events(repoRoot, &error);
        bool ok = check(error.isEmpty() && !events.isEmpty(), "MIG-017", "Recovered events from ProjectMemory");
        bool hasP4CertEvent = false;
        for (const auto& ev : events) {
            if (ev.value(QStringLiteral("sequenceNumber")).toInt() == 329) {
                hasP4CertEvent = true;
                ok &= check(ev.value(QStringLiteral("actor")).toString() == QStringLiteral("agent"), "MIG-017", "Provenance actor is agent");
                ok &= check(ev.value(QStringLiteral("agentId")).toString() == QStringLiteral("antigravity"), "MIG-017", "Provenance agentId is antigravity");
                ok &= check(ev.value(QStringLiteral("tool")).toString() == QStringLiteral("aramf-cli"), "MIG-017", "Provenance tool is aramf-cli");
                ok &= check(ev.value(QStringLiteral("scope")).toString() == QStringLiteral("project"), "MIG-017", "Provenance scope is project");
            }
        }
        ok &= check(hasP4CertEvent, "MIG-017", "Verified sequence 329 certification event provenance");
        allPass &= ok;
    }

    // MIG-018: Cold-start reconstructs the canonical lifecycle correctly from legacy + migrated state
    {
        ProjectMemory memory;
        error.clear();
        QJsonObject report = memory.validateColdStart(repoRoot, &error);
        bool ok = check(error.isEmpty() && report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                        "MIG-018", "validateColdStart reports PASS on canonical state");
        allPass &= ok;
    }

    // MIG-019: Memory consistency accepts legitimate legacy records and current canonical state
    {
        ProjectMemory memory;
        error.clear();
        QJsonObject report = memory.validate(repoRoot, &error, false);
        bool ok = check(error.isEmpty() && report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                        "MIG-019", "Memory consistency validate reports PASS on canonical state");
        allPass &= ok;
    }

    // MIG-020: P1-P5 certified dependency chain remains valid after migration
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        bool ok = check(state.completedIdentifiers().contains(QStringLiteral("P1.1.2.1.1")), "MIG-020", "P1 (Task Execution Governance) is certified");
        ok &= check(state.completedIdentifiers().contains(QStringLiteral("P2.1.1.1.1")), "MIG-020", "P2 (Context Coordination) is certified");
        ok &= check(state.completedIdentifiers().contains(QStringLiteral("P3.1.4.1.1")), "MIG-020", "P3 (Execution Orchestration) is certified");
        ok &= check(state.completedIdentifiers().contains(QStringLiteral("P4.1.4.1.1")), "MIG-020", "P4 (Predictive Task Optimization) is certified");
        ok &= check(state.completedIdentifiers().contains(QStringLiteral("P5.1.4.1.1")), "MIG-020", "P5 (Self-Adjusting Routing) is certified");
        ok &= check(state.nextIdentifier() == QStringLiteral("F1.1.0.0.0"), "MIG-020", "F1 Foundation is scheduled before P6 Code Bank");
        error.clear();
        ok &= check(state.isValid(&error), "MIG-020", "Canonical ProcessVersionState is fully valid");
        allPass &= ok;
    }

    std::cout << "Process Namespace Migration test matrix completed: " << (allPass ? "ALL PASS" : "FAILURES DETECTED") << "\n";
    return allPass;
}
