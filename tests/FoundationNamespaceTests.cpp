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

bool runFoundationNamespaceTests(const QString& selfRepoPath)
{
    std::cout << "Starting Foundation Namespace test matrix (FOUND-001 through FOUND-021)...\n";
    const QString repoRoot = (selfRepoPath.isEmpty() || !QDir(selfRepoPath).exists(QStringLiteral("ARAMF_WORKER")))
        ? AramfPaths::programRoot()
        : selfRepoPath;
    bool allPass = true;
    QString error;

    // FOUND-001: F1 parses with exact canonical name "Memory & Evidence Foundation"
    {
        ProcessVersion v;
        QString err;
        bool ok = check(ProcessVersion::parse(QStringLiteral("F1.1.0.0.0"), &v, &err),
                        "FOUND-001", "F1.1.0.0.0 parses successfully");
        ok &= check(v.isFoundation(), "FOUND-001", "v is foundation");
        ok &= check(v.foundationNumber() == 1, "FOUND-001", "v foundation number is 1");
        ok &= check(ProcessVersion::foundationName(1) == QStringLiteral("Memory & Evidence Foundation"),
                    "FOUND-001", "F1 name is Memory & Evidence Foundation");
        allPass &= ok;
    }

    // FOUND-002: F2 parses with exact canonical name "Identity, Provenance & Trust Foundation"
    {
        ProcessVersion v;
        QString err;
        bool ok = check(ProcessVersion::parse(QStringLiteral("F2.1.0.0.0"), &v, &err),
                        "FOUND-002", "F2.1.0.0.0 parses successfully");
        ok &= check(v.isFoundation(), "FOUND-002", "v is foundation");
        ok &= check(v.foundationNumber() == 2, "FOUND-002", "v foundation number is 2");
        ok &= check(ProcessVersion::foundationName(2) == QStringLiteral("Identity, Provenance & Trust Foundation"),
                    "FOUND-002", "F2 name is Identity, Provenance & Trust Foundation");
        allPass &= ok;
    }

    // FOUND-003: F3 parses with exact canonical name "Scope, State & Integrity Foundation"
    {
        ProcessVersion v;
        QString err;
        bool ok = check(ProcessVersion::parse(QStringLiteral("F3.1.0.0.0"), &v, &err),
                        "FOUND-003", "F3.1.0.0.0 parses successfully");
        ok &= check(v.isFoundation(), "FOUND-003", "v is foundation");
        ok &= check(v.foundationNumber() == 3, "FOUND-003", "v foundation number is 3");
        ok &= check(ProcessVersion::foundationName(3) == QStringLiteral("Scope, State & Integrity Foundation"),
                    "FOUND-003", "F3 name is Scope, State & Integrity Foundation");
        allPass &= ok;
    }

    // FOUND-004: F4 parses with exact canonical name "Lifecycle & Certification Foundation"
    {
        ProcessVersion v;
        QString err;
        bool ok = check(ProcessVersion::parse(QStringLiteral("F4.1.0.0.0"), &v, &err),
                        "FOUND-004", "F4.1.0.0.0 parses successfully");
        ok &= check(v.isFoundation(), "FOUND-004", "v is foundation");
        ok &= check(v.foundationNumber() == 4, "FOUND-004", "v foundation number is 4");
        ok &= check(ProcessVersion::foundationName(4) == QStringLiteral("Lifecycle & Certification Foundation"),
                    "FOUND-004", "F4 name is Lifecycle & Certification Foundation");
        allPass &= ok;
    }

    // FOUND-005: F1..F4 serialize/deserialize deterministically
    {
        bool ok = true;
        for (int f = 1; f <= 4; ++f) {
            ProcessVersion orig(ProcessKind::Foundation, f, 1, 0, 0, 0);
            const QString str = orig.identifier();
            ProcessVersion parsedStr;
            QString err;
            ok &= check(ProcessVersion::parse(str, &parsedStr, &err),
                        "FOUND-005", qPrintable(QStringLiteral("String parse %1").arg(str)));
            ok &= check(parsedStr == orig,
                        "FOUND-005", qPrintable(QStringLiteral("String equality %1").arg(str)));

            QJsonObject json = processVersionToJson(orig);
            ProcessVersion parsedJson;
            ok &= check(processVersionFromJson(json, &parsedJson, &err),
                        "FOUND-005", qPrintable(QStringLiteral("JSON parse %1").arg(str)));
            ok &= check(parsedJson == orig,
                        "FOUND-005", qPrintable(QStringLiteral("JSON equality %1").arg(str)));
        }
        allPass &= ok;
    }

    // FOUND-006: Foundations remain distinct from P processes
    {
        ProcessVersion f1(ProcessKind::Foundation, 1, 1, 0, 0, 0);
        ProcessVersion p1(ProcessKind::Process, 1, 1, 0, 0, 0);
        bool ok = check(f1.isFoundation() && !f1.isProcess(), "FOUND-006", "f1 isFoundation and not isProcess");
        ok &= check(!p1.isFoundation() && p1.isProcess(), "FOUND-006", "p1 isProcess and not isFoundation");
        ok &= check(f1 != p1, "FOUND-006", "f1 != p1");
        ok &= check(f1.identifier() == QStringLiteral("F1.1.0.0.0"), "FOUND-006", "f1 identifier is F1.1.0.0.0");
        ok &= check(p1.identifier() == QStringLiteral("P1.1.0.0.0"), "FOUND-006", "p1 identifier is P1.1.0.0.0");
        allPass &= ok;
    }

    // FOUND-007: Numbers 1..4 resolve to exact names; 0 and 5 are rejected
    {
        bool ok = check(ProcessVersion::isCanonicalFoundation(1), "FOUND-007", "1 is canonical foundation");
        ok &= check(ProcessVersion::isCanonicalFoundation(2), "FOUND-007", "2 is canonical foundation");
        ok &= check(ProcessVersion::isCanonicalFoundation(3), "FOUND-007", "3 is canonical foundation");
        ok &= check(ProcessVersion::isCanonicalFoundation(4), "FOUND-007", "4 is canonical foundation");
        ok &= check(!ProcessVersion::isCanonicalFoundation(0), "FOUND-007", "0 is not canonical foundation");
        ok &= check(!ProcessVersion::isCanonicalFoundation(5), "FOUND-007", "5 is not canonical foundation");
        ok &= check(!ProcessVersion::isCanonicalFoundation(-1), "FOUND-007", "-1 is not canonical foundation");
        ok &= check(ProcessVersion::foundationName(0).isEmpty(), "FOUND-007", "foundationName(0) is empty");
        ok &= check(ProcessVersion::foundationName(5).isEmpty(), "FOUND-007", "foundationName(5) is empty");
        allPass &= ok;
    }

    // FOUND-008: Current next state remains F1.1.0.0.0, remaining foundation queue has F2..F4
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        bool ok = check(state.nextIdentifier() == QStringLiteral("F1.1.0.0.0"),
                        "FOUND-008", "nextIdentifier() is F1.1.0.0.0");
        const QStringList expectedQueue = {QStringLiteral("F2.1.0.0.0"), QStringLiteral("F3.1.0.0.0"), QStringLiteral("F4.1.0.0.0")};
        ok &= check(state.foundationQueueIdentifiers() == expectedQueue,
                    "FOUND-008", "foundationQueue contains [F2, F3, F4]");
        ok &= check(state.futureIdentifier() == QStringLiteral("P6.1.0.0.0"),
                    "FOUND-008", "futureIdentifier is P6.1.0.0.0");
        allPass &= ok;
    }

    // FOUND-009: F2 follows F1 in sequential lifecycle progression
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        QString err;
        bool ok = check(ProcessVersionLifecycle::startNextProcess(&state, &err),
                        "FOUND-009", "startNextProcess activates F1");
        ok &= check(state.activeIdentifier() == QStringLiteral("F1.1.1.0.0"),
                    "FOUND-009", "F1 is now active");
        ok &= check(state.nextIdentifier() == QStringLiteral("F2.1.0.0.0"),
                    "FOUND-009", "F2 is now next process");
        ok &= check(state.foundationQueueIdentifiers() == QStringList({QStringLiteral("F3.1.0.0.0"), QStringLiteral("F4.1.0.0.0")}),
                    "FOUND-009", "Remaining queue has F3 and F4");
        allPass &= ok;
    }

    // FOUND-010: F3 follows F2 in sequential lifecycle progression
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        QString err;
        ProcessVersionLifecycle::startNextProcess(&state, &err); // starts F1
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 1, 1));
        state.hasActiveProcess = false;
        state.activeProcess = {};
        bool ok = check(ProcessVersionLifecycle::startNextProcess(&state, &err),
                        "FOUND-010", "startNextProcess activates F2");
        ok &= check(state.activeIdentifier() == QStringLiteral("F2.1.1.0.0"),
                    "FOUND-010", "F2 is now active");
        ok &= check(state.nextIdentifier() == QStringLiteral("F3.1.0.0.0"),
                    "FOUND-010", "F3 is now next process");
        ok &= check(state.foundationQueueIdentifiers() == QStringList({QStringLiteral("F4.1.0.0.0")}),
                    "FOUND-010", "Remaining queue has F4");
        allPass &= ok;
    }

    // FOUND-011: F4 follows F3 in sequential lifecycle progression
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        QString err;
        ProcessVersionLifecycle::startNextProcess(&state, &err); // starts F1
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 1, 1));
        state.hasActiveProcess = false;
        state.activeProcess = {};
        ProcessVersionLifecycle::startNextProcess(&state, &err); // starts F2
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 2, 1, 1, 1, 1));
        state.hasActiveProcess = false;
        state.activeProcess = {};
        bool ok = check(ProcessVersionLifecycle::startNextProcess(&state, &err),
                        "FOUND-011", "startNextProcess activates F3");
        ok &= check(state.activeIdentifier() == QStringLiteral("F3.1.1.0.0"),
                    "FOUND-011", "F3 is now active");
        ok &= check(state.nextIdentifier() == QStringLiteral("F4.1.0.0.0"),
                    "FOUND-011", "F4 is now next process");
        ok &= check(state.foundationQueueIdentifiers().isEmpty(),
                    "FOUND-011", "Remaining queue is empty");
        allPass &= ok;
    }

    // FOUND-012: P6 cannot activate when only F1 is complete
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 1, 1));
        QString reason;
        bool ok = check(!state.isP6Eligible(&reason), "FOUND-012", "isP6Eligible is false when only F1 complete");
        ok &= check(reason.contains(QStringLiteral("F2")), "FOUND-012", "Reason identifies F2 as missing");
        state.hasNextProcess = true;
        state.nextProcess = ProcessVersion(ProcessKind::Process, 6, 1, 0, 0, 0);
        QString startErr;
        ok &= check(!ProcessVersionLifecycle::startNextProcess(&state, &startErr),
                    "FOUND-012", "startNextProcess fails to start P6 when only F1 complete");
        allPass &= ok;
    }

    // FOUND-013: P6 cannot activate when only F1-F2 are complete
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 2, 1, 1, 1, 1));
        QString reason;
        bool ok = check(!state.isP6Eligible(&reason), "FOUND-013", "isP6Eligible is false when F1-F2 complete");
        ok &= check(reason.contains(QStringLiteral("F3")), "FOUND-013", "Reason identifies F3 as missing");
        state.hasNextProcess = true;
        state.nextProcess = ProcessVersion(ProcessKind::Process, 6, 1, 0, 0, 0);
        QString startErr;
        ok &= check(!ProcessVersionLifecycle::startNextProcess(&state, &startErr),
                    "FOUND-013", "startNextProcess fails to start P6 when only F1-F2 complete");
        allPass &= ok;
    }

    // FOUND-014: P6 cannot activate when only F1-F3 are complete
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 2, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 3, 1, 1, 1, 1));
        QString reason;
        bool ok = check(!state.isP6Eligible(&reason), "FOUND-014", "isP6Eligible is false when F1-F3 complete");
        ok &= check(reason.contains(QStringLiteral("F4")), "FOUND-014", "Reason identifies F4 as missing");
        state.hasNextProcess = true;
        state.nextProcess = ProcessVersion(ProcessKind::Process, 6, 1, 0, 0, 0);
        QString startErr;
        ok &= check(!ProcessVersionLifecycle::startNextProcess(&state, &startErr),
                    "FOUND-014", "startNextProcess fails to start P6 when only F1-F3 complete");
        allPass &= ok;
    }

    // FOUND-015: P6 becomes eligible only after F1-F4 complete AND foundationIntegrationValid is true
    {
        ProcessVersionState state = ProcessVersionState::currentCanonicalState();
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 1, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 2, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 3, 1, 1, 1, 1));
        state.completedHistory.append(ProcessVersion(ProcessKind::Foundation, 4, 1, 1, 1, 1));
        state.foundationIntegrationValid = false;
        QString reason;
        bool ok = check(!state.isP6Eligible(&reason), "FOUND-015",
                        "isP6Eligible is false when F1-F4 complete but foundationIntegrationValid is false");
        ok &= check(reason.contains(QStringLiteral("Integrated foundation validation")),
                    "FOUND-015", "Reason identifies foundation integration validation");

        state.foundationIntegrationValid = true;
        ok &= check(state.isP6Eligible(&reason), "FOUND-015",
                    "isP6Eligible is true when F1-F4 complete AND foundationIntegrationValid is true");
        state.hasNextProcess = true;
        state.nextProcess = ProcessVersion(ProcessKind::Process, 6, 1, 0, 0, 0);
        QString startErr;
        ok &= check(ProcessVersionLifecycle::startNextProcess(&state, &startErr),
                    "FOUND-015", "startNextProcess successfully starts P6 once eligible");
        ok &= check(state.activeIdentifier() == QStringLiteral("P6.1.1.0.0"),
                    "FOUND-015", "P6 is now active");
        allPass &= ok;
    }

    // FOUND-016: Canonical P1-P14 definitions and mappings remain unchanged
    {
        bool ok = true;
        for (int p = 1; p <= 14; ++p) {
            ok &= check(ProcessVersion::isCanonicalProcess(p), "FOUND-016",
                        qPrintable(QStringLiteral("P%1 is canonical process").arg(p)));
            ok &= check(!ProcessVersion::processName(p, ProcessNamespace::CanonicalV2).isEmpty(), "FOUND-016",
                        qPrintable(QStringLiteral("P%1 has non-empty canonical name").arg(p)));
        }
        ok &= check(!ProcessVersion::isCanonicalProcess(0), "FOUND-016", "P0 is not canonical process");
        ok &= check(!ProcessVersion::isCanonicalProcess(15), "FOUND-016", "P15 is not canonical process");
        const auto roadmap = ProcessNamespaceService::canonicalRoadmapNames();
        ok &= check(roadmap.size() == 18, "FOUND-016", "Roadmap has 18 entries (4 foundations + 14 processes)");
        allPass &= ok;
    }

    // FOUND-017: Legacy P0-P12 mappings remain unchanged
    {
        bool ok = true;
        for (int p = 0; p <= 12; ++p) {
            int canonical = 0;
            QString err;
            ok &= check(ProcessVersion::mapLegacyToCanonical(p, &canonical, &err), "FOUND-017",
                        qPrintable(QStringLiteral("Map legacy P%1").arg(p)));
            ok &= check(canonical == p + 1, "FOUND-017",
                        qPrintable(QStringLiteral("Legacy P%1 maps to canonical P%2").arg(p).arg(p + 1)));
        }
        allPass &= ok;
    }

    // FOUND-018: Historical recorder events byte-preserved and intact
    {
        ProjectMemory memory;
        error.clear();
        QList<QJsonObject> events = memory.events(repoRoot, &error);
        bool ok = check(error.isEmpty() && !events.isEmpty(), "FOUND-018", "Recovered events from ProjectMemory");
        bool has329 = false;
        for (const auto& ev : events) {
            if (ev.value(QStringLiteral("sequenceNumber")).toInt() == 329) {
                has329 = true;
                ok &= check(ev.value(QStringLiteral("actor")).toString() == QStringLiteral("agent"),
                            "FOUND-018", "Event 329 actor is agent");
            }
        }
        ok &= check(has329, "FOUND-018", "Found event sequence 329");
        allPass &= ok;
    }

    // FOUND-019: Cold-start reconstructs F1-F4 lifecycle queue correctly
    {
        ProjectMemory memory;
        error.clear();
        QJsonObject report = memory.validateColdStart(repoRoot, &error);
        bool ok = check(error.isEmpty() && report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                        "FOUND-019", "validateColdStart reports PASS on canonical state");
        allPass &= ok;
    }

    // FOUND-020: Memory consistency validate reports PASS on canonical state
    {
        ProjectMemory memory;
        error.clear();
        QJsonObject report = memory.validate(repoRoot, &error, false);
        bool ok = check(error.isEmpty() && report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"),
                        "FOUND-020", "Memory consistency validate reports PASS on canonical state");
        allPass &= ok;
    }

    // FOUND-021: Persisted ProjectModel and generated Worker lifecycle state cannot drift
    {
        ProjectPersistence persistence;
        ProjectModel persistedModel;
        QString persistedError;
        const QString persistedPath = QDir(repoRoot).filePath(QStringLiteral("ARAMF_WORKER.aramf.json"));
        bool ok = check(persistence.load(&persistedModel, persistedPath, &persistedError),
                        "FOUND-021", "Persisted ProjectModel loads through ProjectPersistence");
        QString persistedStateError;
        ok &= check(persistedModel.processVersionState().isValid(&persistedStateError),
                    "FOUND-021", "Persisted ProjectModel lifecycle state is valid");
        ok &= check(persistedModel.processVersionState().namespaceVersion
                        == static_cast<int>(ProcessNamespace::CanonicalV2),
                    "FOUND-021", "Persisted ProjectModel uses CanonicalV2");

        const QString generatedPath = QDir(repoRoot).filePath(QStringLiteral("ARAMF_WORKER/project.json"));
        QFile generatedFile(generatedPath);
        QJsonObject generatedRoot;
        QJsonParseError generatedParseError;
        if (generatedFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            generatedRoot = QJsonDocument::fromJson(generatedFile.readAll(), &generatedParseError).object();
            generatedFile.close();
        }
        ok &= check(generatedParseError.error == QJsonParseError::NoError
                        && generatedRoot.value(QStringLiteral("processVersion")).isObject(),
                    "FOUND-021", "Generated Worker project contains processVersion state");

        ProcessVersionState generatedState;
        QString generatedError;
        ok &= check(processVersionStateFromJson(generatedRoot.value(QStringLiteral("processVersion")),
                                                &generatedState, &generatedError),
                    "FOUND-021", "Generated Worker lifecycle state parses canonically");
        QString generatedStateError;
        ok &= check(generatedState.isValid(&generatedStateError),
                    "FOUND-021", "Generated Worker lifecycle state is valid");
        ok &= check(generatedState.namespaceVersion
                        == static_cast<int>(ProcessNamespace::CanonicalV2),
                    "FOUND-021", "Generated Worker uses CanonicalV2");
        ok &= check(processVersionStateToJson(persistedModel.processVersionState())
                        == processVersionStateToJson(generatedState),
                    "FOUND-021", "Persisted and generated lifecycle states are identical");

        QTemporaryDir fixtureDirectory;
        ok &= check(fixtureDirectory.isValid(),
                    "FOUND-021", "Lifecycle progression fixture directory is available");
        if (fixtureDirectory.isValid()) {
            ProcessVersionState progressed = ProcessVersionState::currentCanonicalState();
            QString progressionError;
            ok &= check(ProcessVersionLifecycle::startNextProcess(&progressed, &progressionError),
                        "FOUND-021", "Fixture starts F1 without changing live lifecycle state");
            ok &= check(progressed.activeIdentifier() == QStringLiteral("F1.1.1.0.0"),
                        "FOUND-021", "Fixture active state is F1.1.1.0.0 after start");
            ok &= check(ProcessVersionLifecycle::certifyCurrentIteration(&progressed, &progressionError),
                        "FOUND-021", "Fixture certifies F1 without changing live lifecycle state");
            ok &= check(progressed.activeIdentifier() == QStringLiteral("F1.1.1.1.0"),
                        "FOUND-021", "Fixture active state is F1.1.1.1.0 after certification");
            ok &= check(ProcessVersionLifecycle::completeActiveProcess(&progressed, &progressionError),
                        "FOUND-021", "Fixture completes F1 without changing live lifecycle state");
            ok &= check(progressed.completedIdentifiers().contains(QStringLiteral("F1.1.1.1.1"))
                            && progressed.nextIdentifier() == QStringLiteral("F2.1.0.0.0"),
                        "FOUND-021", "Fixture advances to completed F1 and next F2");

            QFile persistedSource(persistedPath);
            QFile generatedSource(generatedPath);
            QJsonParseError persistedParseError;
            QJsonParseError generatedFixtureParseError;
            QJsonObject persistedFixtureRoot;
            QJsonObject generatedFixtureRoot;
            if (persistedSource.open(QIODevice::ReadOnly | QIODevice::Text)) {
                persistedFixtureRoot = QJsonDocument::fromJson(
                    persistedSource.readAll(), &persistedParseError).object();
                persistedSource.close();
            }
            if (generatedSource.open(QIODevice::ReadOnly | QIODevice::Text)) {
                generatedFixtureRoot = QJsonDocument::fromJson(
                    generatedSource.readAll(), &generatedFixtureParseError).object();
                generatedSource.close();
            }
            persistedFixtureRoot.insert(QStringLiteral("processVersion"),
                                        processVersionStateToJson(progressed));
            generatedFixtureRoot.insert(QStringLiteral("processVersion"),
                                        processVersionStateToJson(progressed));

            const QString fixturePersistedPath = QDir(fixtureDirectory.path())
                .filePath(QStringLiteral("fixture.aramf.json"));
            const QString fixtureGeneratedPath = QDir(fixtureDirectory.path())
                .filePath(QStringLiteral("project.json"));
            QFile fixturePersistedFile(fixturePersistedPath);
            QFile fixtureGeneratedFile(fixtureGeneratedPath);
            if (fixturePersistedFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                fixturePersistedFile.write(QJsonDocument(persistedFixtureRoot)
                                               .toJson(QJsonDocument::Indented));
                fixturePersistedFile.close();
            } else {
                ok = check(false, "FOUND-021", "Progression fixture persisted state can be written");
            }
            if (fixtureGeneratedFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                fixtureGeneratedFile.write(QJsonDocument(generatedFixtureRoot)
                                               .toJson(QJsonDocument::Indented));
                fixtureGeneratedFile.close();
            } else {
                ok = check(false, "FOUND-021", "Progression fixture generated state can be written");
            }

            ProjectModel progressedPersistedModel;
            QString progressedPersistedError;
            ok &= check(persistence.load(&progressedPersistedModel, fixturePersistedPath,
                                         &progressedPersistedError),
                        "FOUND-021", "Progression fixture loads through ProjectPersistence");
            QString progressedPersistedStateError;
            ok &= check(progressedPersistedModel.processVersionState().isValid(
                            &progressedPersistedStateError),
                        "FOUND-021", "Progression fixture persisted state is valid");

            QFile progressedGeneratedFile(fixtureGeneratedPath);
            QJsonObject progressedGeneratedRoot;
            QJsonParseError progressedGeneratedParseError;
            if (progressedGeneratedFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                progressedGeneratedRoot = QJsonDocument::fromJson(
                    progressedGeneratedFile.readAll(), &progressedGeneratedParseError).object();
                progressedGeneratedFile.close();
            }
            ProcessVersionState progressedGeneratedState;
            QString progressedGeneratedError;
            ok &= check(processVersionStateFromJson(
                            progressedGeneratedRoot.value(QStringLiteral("processVersion")),
                            &progressedGeneratedState, &progressedGeneratedError),
                        "FOUND-021", "Progression fixture generated state parses");
            QString progressedGeneratedStateError;
            ok &= check(progressedGeneratedState.isValid(&progressedGeneratedStateError),
                        "FOUND-021", "Progression fixture generated state is valid");
            ok &= check(processVersionStateToJson(progressedPersistedModel.processVersionState())
                            == processVersionStateToJson(progressedGeneratedState),
                        "FOUND-021", "Progressed persisted/generated states remain equivalent");
        }
        allPass &= ok;
    }

    std::cout << "Foundation Namespace test matrix completed: " << (allPass ? "ALL PASS" : "FAILURES DETECTED") << "\n";
    return allPass;
}
