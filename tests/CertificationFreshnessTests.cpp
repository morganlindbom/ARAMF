#include "core/CertificationFreshness.h"
#include "core/CertificationRevalidation.h"
#include "core/AramfPaths.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <iostream>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::cerr << "FAIL: " << message << '\n';
    return value;
}

bool fixtureCheck(bool value, const QString& message)
{
    return check(value, message.toUtf8().constData());
}

QString fixtureHash(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool fixtureWrite(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

bool fixtureRead(const QString& path, QByteArray* bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    *bytes = file.readAll();
    return file.error() == QFileDevice::NoError;
}

bool fixtureGit(const QString& root, const QStringList& arguments, QByteArray* output = nullptr)
{
    QProcess process;
    process.setWorkingDirectory(root);
    process.start(QStringLiteral("git"), arguments);
    if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        std::cerr << "FAIL: isolated fixture git " << arguments.join(' ').toStdString()
                  << ": " << process.readAllStandardError().toStdString() << '\n';
        return false;
    }
    if (output) *output = process.readAllStandardOutput();
    return true;
}

bool fixtureTree(const QString& root, QMap<QString, QByteArray>* snapshot)
{
    snapshot->clear();
    if (!QFileInfo(root).isDir()) return false;
    QDirIterator iterator(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                                  | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QString relative = QDir(root).relativeFilePath(path);
        if (iterator.fileInfo().isDir()) {
            snapshot->insert(QStringLiteral("directory:") + relative, {});
        } else {
            QByteArray bytes;
            if (!fixtureRead(path, &bytes)) return false;
            snapshot->insert(QStringLiteral("file:") + relative, bytes);
        }
    }
    return true;
}

// This independent literal is the approved P1 inventory: the original four
// files followed by the original 62 helpers and two direct F2 trust inputs.
// Do not derive the expectation from filesFor.
QStringList expectedP1HelperManifest()
{
    return {
        "src/core/WorkerTaskServices.h", "src/core/WorkerTaskServices.cpp",
        "tests/WorkerTaskTests.cpp", "tests/P2ExecutionTests.cpp",
        "src/core/AiCatalog.h", "src/core/AiCatalog.cpp",
        "src/core/CertificationFreshness.h", "src/core/CertificationFreshness.cpp",
        "src/core/CertificationRevalidation.h", "src/core/CertificationRevalidation.cpp",
        "src/core/ComponentVersion.h", "src/core/ComponentVersion.cpp",
        "src/core/ConfigurationUpdateService.h", "src/core/ConfigurationUpdateService.cpp",
        "src/core/ContextCoordinationService.h", "src/core/ContextCoordinationService.cpp",
        "src/core/ControlPlaneMigration.h", "src/core/ControlPlaneMigration.cpp",
        "src/core/DocumentInstruction.h", "src/core/DocumentInstruction.cpp",
        "src/core/DocumentTemplate.h", "src/core/DocumentTemplate.cpp",
        "src/core/DocumentTemplateInspector.h", "src/core/DocumentTemplateInspector.cpp",
        "src/core/EnvironmentCatalog.h", "src/core/EnvironmentCatalog.cpp",
        "src/core/FoundationCertificationCampaign.h", "src/core/FoundationCertificationCampaign.cpp",
        "src/core/GitIgnoreService.h", "src/core/GitIgnoreService.cpp",
        "src/core/MemoryCatalog.h", "src/core/MemoryCatalog.cpp",
        "src/core/ProcessVersion.h", "src/core/ProcessVersion.cpp",
        "src/core/ProjectModel.h", "src/core/ProjectModel.cpp",
        "src/core/ProjectPersistence.h", "src/core/ProjectPersistence.cpp",
        "src/core/RuleCatalog.h", "src/core/RuleCatalog.cpp",
        "src/core/Services.h", "src/core/Services.cpp",
        "src/core/TemplateValidation.h", "src/core/TemplateValidation.cpp",
        "src/core/ValidationRouting.h", "src/core/ValidationRouting.cpp",
        "src/core/WorkerContextResolver.h", "src/core/WorkerContextResolver.cpp",
        "src/core/WorkflowPageMetadata.h", "src/core/WorkflowPageMetadata.cpp",
        "src/core/AramfPaths.h", "src/core/ProjectSchema.h",
        "src/foundations/F4/ProcessVersion.h", "src/foundations/F4/ProcessVersion.cpp",
        "src/structure/S1/ResponsibilityOwnership.h", "src/structure/S1/ResponsibilityOwnership.cpp",
        "src/structure/S2/PhysicalStructure.h", "src/structure/S2/PhysicalStructure.cpp",
        "src/structure/S3/DependencyInterfaces.h", "src/structure/S3/DependencyInterfaces.cpp",
        "src/structure/S4/CompositionEncapsulation.h", "src/structure/S4/CompositionEncapsulation.cpp",
        "src/structure/S5/DecompositionModularity.h", "src/structure/S5/DecompositionModularity.cpp",
        "src/structure/S6/StructuralEvolutionEnforcement.h", "src/structure/S6/StructuralEvolutionEnforcement.cpp",
        "src/foundations/F2/IdentityTrustFoundation.h", "src/foundations/F2/IdentityTrustFoundation.cpp"
    };
}

bool expectedRawFingerprint(const QString& root, const QStringList& files, QString* fingerprint)
{
    QByteArray material;
    for (const auto& relative : files) {
        QByteArray bytes;
        if (!fixtureRead(QDir(root).filePath(relative), &bytes)) return false;
        material += relative.toUtf8();
        material += '\0';
        material += bytes;
        material += '\0';
    }
    *fingerprint = fixtureHash(material);
    return true;
}

bool p1HelperManifestRegression()
{
    const QStringList acceptanceNames{
        "P1 helper manifest is exact and versioned",
        "P1 helper changes invalidate source independently",
        "P1 helper omission fails closed and unrelated files remain irrelevant",
        "P1 helper source changes stale standalone and transitive freshness",
        "P1 helper restoration preserves historical evidence and graph independence"
    };
    // Unreached groups remain FAIL, including early fixture setup failures.
    bool observed[5] = {false, false, false, false, false};
    const auto finish = [&]() {
        bool passed = true;
        for (int i = 0; i < acceptanceNames.size(); ++i) {
            const QJsonObject observation{{"id", acceptanceNames.at(i)},
                                         {"status", observed[i] ? "PASS" : "FAIL"}};
            std::cout << "FRESHNESS-ACCEPTANCE "
                      << QJsonDocument(observation).toJson(QJsonDocument::Compact).constData() << '\n';
            passed &= observed[i];
        }
        return passed;
    };

    const QStringList expectedFiles = expectedP1HelperManifest();
    const auto actualManifest = CertificationSourceManifestProvider::manifest("P1");
    const QSet<QString> uniqueFiles(expectedFiles.cbegin(), expectedFiles.cend());
    observed[0] = check(expectedFiles.size() == 68 && uniqueFiles.size() == 68
                            && actualManifest.subject == "P1" && actualManifest.manifestVersion == "3"
                            && actualManifest.files == expectedFiles,
                        "P1 manifest is exactly the approved ordered 68 files at version 3");
    observed[0] &= check(actualManifest.toJson().value("manifestVersion").toString() == "3"
                            && actualManifest.toJson().value("files").toArray() == QJsonArray::fromStringList(expectedFiles),
                        "serialized P1 manifest retains exact files and version");

    const QMap<QString, QStringList> expectedGraph{
        {"F1", {}}, {"F2", {}}, {"F3", {}}, {"F4", {}}, {"S1", {}},
        {"S2", {"S1"}}, {"S3", {"S1", "S2"}}, {"S4", {"S1", "S2", "S3"}},
        {"S5", {"S1", "S2", "S3", "S4"}}, {"S6", {"S1", "S2", "S3", "S4", "S5"}},
        {"P1", {"F1"}}, {"P2", {"P1"}}, {"P3", {"P1", "P2"}},
        {"P4", {"P1"}}, {"P5", {"P1", "P4"}}
    };
    QMap<QString, QJsonObject> originalGraph;
    bool independentGraph = true;
    for (auto it = expectedGraph.cbegin(); it != expectedGraph.cend(); ++it) {
        CertificationDependencyManifest expected;
        expected.subject = it.key();
        expected.lifecycle = it.key().startsWith('S') ? it.key() + ".1" : it.key();
        for (const auto& dependency : it.value()) {
            CertificationDependencyBinding binding;
            binding.subject = dependency;
            expected.directDependencies.append(binding);
        }
        expected.manifestFingerprint = expected.computedFingerprint();
        const auto actual = CertificationFreshnessService::dependencyManifest(it.key());
        originalGraph.insert(it.key(), actual.toJson());
        independentGraph &= fixtureCheck(actual.toJson() == expected.toJson(),
            "unchanged exact dependency graph for " + it.key());
        if (it.key() != "P1") {
            independentGraph &= fixtureCheck(CertificationSourceManifestProvider::manifest(it.key()).manifestVersion == "1",
                "P1 helper expansion does not change other source manifest versions: " + it.key());
        }
    }
    for (const auto& subject : QStringList{"F1", "F2", "F3", "F4"}) {
        independentGraph &= fixtureCheck(CertificationFreshnessService::dependencyManifest(subject).directDependencies.isEmpty(),
            "Foundation remains independent of peer Foundations: " + subject);
    }

    QTemporaryDir fixture(QDir::tempPath() + "/aramf-p1-helper-freshness-XXXXXX");
    fixture.setAutoRemove(false);
    if (!check(fixture.isValid(), "isolated P1 helper fixture created")) return finish();
    const QString root = fixture.path();
    std::cout << "P1 helper fixture retained at " << root.toStdString() << '\n';
    const QStringList subjects{"F1", "P1", "P2", "P3", "P4", "P5"};
    QSet<QString> sourceFiles(expectedFiles.cbegin(), expectedFiles.cend());
    for (const auto& subject : subjects) {
        for (const auto& relative : CertificationSourceManifestProvider::manifest(subject).files)
            sourceFiles.insert(relative);
    }
    bool setup = true;
    for (const auto& relative : sourceFiles) {
        setup &= fixtureCheck(fixtureWrite(QDir(root).filePath(relative),
            QByteArray("// Synthetic isolated source bytes: ") + relative.toUtf8() + "\r\n"),
            "fixture source written: " + relative);
    }
    const QString unrelated = QDir(root).filePath("src/unrelated-helper-fixture.txt");
    const QByteArray unrelatedBytes("unrelated fixture source\r\n");
    setup &= check(fixtureWrite(unrelated, unrelatedBytes), "unrelated fixture source written");
    if (!setup) return finish();

    QString baselineFingerprint;
    QString rawFingerprint;
    QString error;
    setup &= check(CertificationSourceManifestProvider::fingerprint(root, "P1", &baselineFingerprint, &error)
                       && error.isEmpty() && expectedRawFingerprint(root, expectedFiles, &rawFingerprint)
                       && !baselineFingerprint.isEmpty() && baselineFingerprint == rawFingerprint,
                   "P1 baseline fingerprint equals independent raw path-and-byte hash");
    if (!setup) return finish();

    bool mutationChecks = true;
    bool omissionChecks = true;
    bool allRestored = true;
    // Every helper gets its own one-file mutation and one-file omission.
    for (const auto& relative : expectedFiles.mid(4)) {
        const QString path = QDir(root).filePath(relative);
        QByteArray original;
        if (!fixtureCheck(fixtureRead(path, &original), "read helper before mutation: " + relative)) {
            return finish();
        }
        // Include a NUL and CRLF to ensure the source hash covers raw bytes.
        const QByteArray mutation = original + QByteArray(1, '\0') + "helper-change\r\n";
        QString changed;
        QString expectedChanged;
        const bool wrote = fixtureWrite(path, mutation);
        const bool hashed = CertificationSourceManifestProvider::fingerprint(root, "P1", &changed, &error);
        mutationChecks &= fixtureCheck(wrote && hashed && error.isEmpty()
            && expectedRawFingerprint(root, expectedFiles, &expectedChanged)
            && changed == expectedChanged && changed != baselineFingerprint,
            "individual helper raw-byte change invalidates P1: " + relative);
        const bool restored = fixtureWrite(path, original);
        QString restoredFingerprint;
        allRestored &= fixtureCheck(restored
            && CertificationSourceManifestProvider::fingerprint(root, "P1", &restoredFingerprint, &error)
            && error.isEmpty() && restoredFingerprint == baselineFingerprint,
            "individual helper bytes restore baseline: " + relative);
        mutationChecks &= allRestored;
        if (!restored) return finish();

        const QString heldPath = path + ".omitted-for-fixture";
        const bool omitted = QFile::rename(path, heldPath);
        QString missingFingerprint;
        const bool missingHashed = CertificationSourceManifestProvider::fingerprint(root, "P1", &missingFingerprint, &error);
        omissionChecks &= fixtureCheck(omitted && !missingHashed && missingFingerprint.isEmpty()
            && error.contains(relative), "individual missing helper fails closed: " + relative);
        const bool returned = omitted && QFile::rename(heldPath, path);
        restoredFingerprint.clear();
        allRestored &= fixtureCheck(returned
            && CertificationSourceManifestProvider::fingerprint(root, "P1", &restoredFingerprint, &error)
            && error.isEmpty() && restoredFingerprint == baselineFingerprint,
            "omitted helper restores exact fingerprint: " + relative);
        omissionChecks &= allRestored;
        if (!returned) return finish();
    }
    observed[1] = mutationChecks;
    QString unrelatedFingerprint;
    omissionChecks &= check(fixtureWrite(unrelated, unrelatedBytes + "changed\n")
        && CertificationSourceManifestProvider::fingerprint(root, "P1", &unrelatedFingerprint, &error)
        && error.isEmpty() && unrelatedFingerprint == baselineFingerprint,
        "unrelated file modification is irrelevant to P1 source fingerprint");
    allRestored &= check(fixtureWrite(unrelated, unrelatedBytes), "unrelated source bytes restored");
    observed[2] = omissionChecks && allRestored;
    if (!allRestored) return finish();

    // JSON certificates and the PASS regression payload below are explicitly
    // synthetic isolated inputs, not claims of product certification. The
    // evaluator, Git observations, and revalidation persistence are real.
    const QJsonObject project{{"schemaVersion", 2}, {"projectId", "isolated-p1-helper-freshness"},
                              {"syntheticFreshnessFixture", true}};
    setup &= check(fixtureWrite(QDir(root).filePath("ARAMF_WORKER.aramf.json"),
        QJsonDocument(project).toJson(QJsonDocument::Compact) + '\n'), "minimal isolated project JSON written");
    setup &= check(fixtureGit(root, {"init", "-q"}), "isolated Git repository initialized");
    setup &= check(fixtureGit(root, {"config", "core.autocrlf", "false"}), "isolated raw source bytes retained by Git");
    setup &= check(fixtureGit(root, {"add", "."}), "isolated source baseline staged");
    setup &= check(fixtureGit(root, {"-c", "user.name=ARAMF Test", "-c", "user.email=aramf-test@example.invalid",
        "-c", "commit.gpgsign=false", "commit", "-q", "-m", "synthetic freshness fixture"}),
        "isolated source baseline committed");
    QByteArray revisionBytes;
    setup &= check(fixtureGit(root, {"rev-parse", "HEAD"}, &revisionBytes), "isolated Git revision read");
    const QString revision = QString::fromUtf8(revisionBytes).trimmed();
    if (!setup || !check(!revision.isEmpty(), "isolated Git revision is nonempty")) return finish();

    QByteArray ledger;
    for (const auto& subject : subjects) {
        QString sourceFingerprint;
        QString contractFingerprint;
        setup &= fixtureCheck(CertificationSourceManifestProvider::fingerprint(root, subject, &sourceFingerprint, &error)
            && error.isEmpty(), "fixture source fingerprint: " + subject);
        setup &= fixtureCheck(CertificationContractManifestProvider::fingerprint(subject, root, &contractFingerprint, &error)
            && error.isEmpty(), "fixture contract fingerprint: " + subject);
        const auto bindings = CertificationDependencyBindingProvider::currentBindings(subject, root, &error);
        setup &= fixtureCheck(error.isEmpty(), "fixture dependency bindings: " + subject);
        const QJsonObject evidence{{"schemaVersion", 1}, {"syntheticFreshnessFixture", true},
            {"subject", subject}, {"status", "PASS"}, {"lifecycle", subject + ".1.1.1.1"},
            {"sourceRevision", revision}, {"sourceFingerprint", sourceFingerprint},
            {"contractFingerprint", contractFingerprint}, {"dependencyBindings", bindings}};
        const QByteArray evidenceBytes = QJsonDocument(evidence).toJson(QJsonDocument::Compact) + '\n';
        const QString reference = "ARAMF_WORKER/certification/evidence/historical-" + subject + ".json";
        setup &= fixtureCheck(fixtureWrite(QDir(root).filePath(reference), evidenceBytes),
            "synthetic historical evidence persisted: " + subject);
        const QString evidenceFingerprint = fixtureHash(evidenceBytes);
        const QJsonObject certificate{{"certificateId", "cert-synthetic-helper-" + subject.toLower()},
            {"subject", subject}, {"sourceRevision", revision}, {"syntheticFreshnessFixture", true},
            {"evidenceArtifact", reference}, {"evidenceFingerprint", evidenceFingerprint},
            {"evidenceReferences", QJsonArray{QJsonObject{{"reference", reference}, {"fingerprint", evidenceFingerprint}}}}};
        ledger += QJsonDocument(certificate).toJson(QJsonDocument::Compact) + '\n';
    }
    setup &= check(fixtureWrite(QDir(root).filePath("ARAMF_WORKER/certification/certificates.jsonl"), ledger),
                   "isolated historical certificate ledger persisted");
    setup &= check(fixtureWrite(QDir(root).filePath("ARAMF_WORKER/memory/event-log.jsonl"),
        "{\"syntheticFreshnessFixture\":true,\"sequence\":1,\"status\":\"FAIL\"}\n"
        "{\"syntheticFreshnessFixture\":true,\"sequence\":2,\"status\":\"PASS\"}\n"),
        "isolated failed and successful historical events retained as exact bytes");
    setup &= check(fixtureWrite(QDir(root).filePath("ARAMF_WORKER/certification/historical-report.json"),
        "{\"syntheticFreshnessFixture\":true,\"historicalReport\":\"preserve exact bytes\"}\r\n"),
        "isolated historical report persisted");
    if (!setup) return finish();

    const auto allFresh = [&]() {
        bool fresh = true;
        for (const auto& subject : subjects) {
            QString evaluationError;
            const auto result = CertificationFreshnessService::evaluate(subject, root, &evaluationError);
            fresh &= fixtureCheck(evaluationError.isEmpty() && result.status == CertificationFreshnessStatus::Fresh
                && result.sourceFresh && result.sourceBindingComplete && result.contractFresh && result.evidenceFresh
                && result.bindingComplete && result.directDependencyFresh && result.transitiveDependencyFresh
                && result.historicalCertificateId == "cert-synthetic-helper-" + subject.toLower()
                && result.historicalLifecycle == subject + ".1.1.1.1"
                && result.currentSourceRevision == revision,
                "actual isolated evaluator FRESH control: " + subject + " (" + evaluationError + ")");
        }
        return fresh;
    };
    QMap<QString, QByteArray> historicalTree;
    setup &= check(fixtureTree(root, &historicalTree), "historical fixture bytes captured");
    setup &= allFresh();
    QMap<QString, QByteArray> afterControl;
    setup &= check(fixtureTree(root, &afterControl) && afterControl == historicalTree,
                   "historical-certificate freshness control is read-only");
    if (!setup) return finish();

    for (const auto& subject : subjects) {
        CertificationRevalidationResult revalidation;
        setup &= fixtureCheck(CertificationRevalidationService::revalidate(root, subject,
            {{"status", "PASS"}, {"suite", "synthetic-isolated-helper-freshness"}, {"syntheticFreshnessFixture", true}},
            &revalidation, &error) && error.isEmpty() && revalidation.status == "FRESH"
            && revalidation.sourceRevision == revision && revalidation.historicalLifecycle == subject + ".1.1.1.1",
            "real isolated revalidation records valid source and dependency bindings: " + subject + " (" + error + ")");
        QJsonObject persisted;
        QByteArray persistedEvidence;
        setup &= fixtureCheck(CertificationRevalidationService::latest(root, subject, &persisted, &error)
            && error.isEmpty() && persisted.value("revalidationId").toString() == revalidation.revalidationId
            && fixtureRead(QDir(root).filePath(revalidation.evidenceArtifact), &persistedEvidence)
            && fixtureHash(persistedEvidence) == revalidation.evidenceFingerprint
            && QJsonDocument::fromJson(persistedEvidence).object().value("regressionEvidence").toObject()
                .value("syntheticFreshnessFixture").toBool(),
            "real revalidation record and exact evidence reload with explicit synthetic fixture provenance: " + subject);
    }
    QMap<QString, QByteArray> baselineTree;
    setup &= check(fixtureTree(root, &baselineTree), "revalidated fixture tree captured");
    bool historicalPreserved = true;
    for (auto it = historicalTree.cbegin(); it != historicalTree.cend(); ++it) {
        historicalPreserved &= fixtureCheck(baselineTree.contains(it.key()) && baselineTree.value(it.key()) == it.value(),
            "revalidation retains pre-existing fixture bytes: " + it.key());
    }
    setup &= allFresh();
    setup &= check(fixtureTree(root, &afterControl) && afterControl == baselineTree,
                   "revalidated freshness control is read-only");
    if (!setup) return finish();

    bool freshnessChecks = true;
    for (const auto& relative : QStringList{"src/core/ConfigurationUpdateService.h", "src/core/TemplateValidation.cpp",
                                          "src/core/Services.cpp", "src/core/CertificationRevalidation.cpp",
                                          "src/foundations/F2/IdentityTrustFoundation.h", "src/foundations/F2/IdentityTrustFoundation.cpp"}) {
        const QString path = QDir(root).filePath(relative);
        QByteArray original;
        if (!fixtureCheck(fixtureRead(path, &original), "read actual freshness mutation source: " + relative)) {
            return finish();
        }
        freshnessChecks &= fixtureCheck(fixtureWrite(path, original + "// isolated semantic helper mutation\r\n"),
            "introduce one helper source mutation: " + relative);
        QMap<QString, QByteArray> changedTree;
        freshnessChecks &= check(fixtureTree(root, &changedTree), "mutated fixture tree captured before evaluation");
        for (const auto& subject : subjects) {
            QString evaluationError;
            const auto result = CertificationFreshnessService::evaluate(subject, root, &evaluationError);
            const auto expectedStatus = subject == "F1" ? CertificationFreshnessStatus::Fresh
                : subject == "P1" ? CertificationFreshnessStatus::SourceStale
                                  : CertificationFreshnessStatus::TransitiveDependencyStale;
            freshnessChecks &= fixtureCheck(evaluationError.isEmpty() && result.status == expectedStatus
                && result.sourceFresh == (subject != "P1") && result.sourceBindingComplete
                && result.contractFresh && result.evidenceFresh && result.bindingComplete
                && result.historicalCertificateId == "cert-synthetic-helper-" + subject.toLower()
                && result.historicalLifecycle == subject + ".1.1.1.1",
                relative + " yields " + certificationFreshnessStatusName(expectedStatus) + " for " + subject
                    + ", observed " + certificationFreshnessStatusName(result.status) + " (" + evaluationError + ")");
        }
        freshnessChecks &= check(fixtureTree(root, &afterControl) && afterControl == changedTree,
                            "stale and transitive evaluations leave the entire fixture byte-identical");
        const bool restored = fixtureWrite(path, original);
        allRestored &= fixtureCheck(restored, "restore helper after stale evaluation: " + relative);
        if (!restored) return finish();
        allRestored &= allFresh();
        allRestored &= check(fixtureTree(root, &afterControl) && afterControl == baselineTree,
                            "restoration recovers FRESH without changing certificates, evidence, reports, or history");
    }
    observed[3] = freshnessChecks;
    for (auto it = originalGraph.cbegin(); it != originalGraph.cend(); ++it) {
        independentGraph &= fixtureCheck(CertificationFreshnessService::dependencyManifest(it.key()).toJson() == it.value(),
            "source mutations and restoration preserve dependency graph: " + it.key());
    }
    observed[4] = independentGraph && allRestored && historicalPreserved;
    return finish();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);
    bool ok = true;

    const auto s1 = CertificationFreshnessService::dependencyManifest(QStringLiteral("S1"));
    const auto s2 = CertificationFreshnessService::dependencyManifest(QStringLiteral("S2"));
    const auto s6 = CertificationFreshnessService::dependencyManifest(QStringLiteral("S6"));
    const auto p1 = CertificationFreshnessService::dependencyManifest(QStringLiteral("P1"));
    const auto p5 = CertificationFreshnessService::dependencyManifest(QStringLiteral("P5"));
    ok &= check(s1.directDependencies.isEmpty(), "S1 is a dependency root");
    ok &= check(s2.directDependencies.size() == 1 && s2.directDependencies.first().subject == "S1", "S2 binds S1 directly");
    ok &= check(s6.directDependencies.size() == 5, "S6 binds S1-S5 directly");
    ok &= check(p1.directDependencies.size() == 1 && p1.directDependencies.first().subject == "F1", "P1 binds F1");
    ok &= check(p5.directDependencies.size() == 2 && p5.directDependencies.first().subject == "P1"
                    && p5.directDependencies.last().subject == "P4", "P5 binds only true direct process dependencies");
    ok &= check(s6.manifestFingerprint == s6.computedFingerprint(), "dependency manifest fingerprint is deterministic");

    QJsonObject historical{{"certificateId", "cert-test"}, {"sourceRevision", "rev"},
                           {"sourceFingerprint", "source"}, {"evidenceFingerprint", "evidence"},
                           {"s1ContractFingerprint", "contract"}};
    auto fresh = CertificationFreshnessService::evaluateSnapshot("S1", historical, "rev", "source", "contract", "evidence");
    ok &= check(fresh.status == CertificationFreshnessStatus::Fresh, "complete root snapshot is fresh");
    auto revisionChanged = CertificationFreshnessService::evaluateSnapshot("S1", historical, "new-revision", "source", "contract", "evidence");
    ok &= check(revisionChanged.status == CertificationFreshnessStatus::Fresh && revisionChanged.sourceFresh,
                "repository revision change alone does not stale an identical source manifest");

    QString rootFingerprint;
    const QString originalDirectory = QDir::currentPath();
    const QString projectRoot = AramfPaths::programRoot();
    ok &= check(AramfPaths::isDevelopmentRoot(projectRoot), "freshness test resolves its source checkout independently of build-directory name");
    QString originalFingerprint;
    ok &= check(CertificationSourceManifestProvider::fingerprint(projectRoot, "S1", &originalFingerprint),
                "source manifest fingerprints the resolved checkout before changing caller CWD");
    QTemporaryDir unrelatedCaller;
    const bool changedDirectory = unrelatedCaller.isValid() && QDir::setCurrent(unrelatedCaller.path());
    ok &= check(changedDirectory, "source-root regression uses an unrelated caller CWD");
    const bool rootedFingerprint = CertificationSourceManifestProvider::fingerprint(projectRoot, "S1", &rootFingerprint);
    if (changedDirectory) ok &= check(QDir::setCurrent(originalDirectory), "source-root regression restores caller CWD");
    ok &= check(rootedFingerprint && !rootFingerprint.isEmpty() && rootFingerprint == originalFingerprint,
                "source manifest uses supplied project root, not caller CWD");

    auto staleDependency = fresh;
    staleDependency.status = CertificationFreshnessStatus::DependencyStale;
    QHash<QString, CertificationFreshnessResult> direct;
    direct.insert("S1", staleDependency);
    auto consumer = CertificationFreshnessService::evaluateSnapshot("S2", historical, "rev", "source", "contract", "evidence", direct);
    ok &= check(consumer.status == CertificationFreshnessStatus::TransitiveDependencyStale, "stale dependency with matching contract is transitive");

    auto transitiveDependency = staleDependency;
    transitiveDependency.status = CertificationFreshnessStatus::TransitiveDependencyStale;
    direct["S1"] = transitiveDependency;
    consumer = CertificationFreshnessService::evaluateSnapshot("S2", historical, "rev", "source", "contract", "evidence", direct);
    ok &= check(consumer.status == CertificationFreshnessStatus::TransitiveDependencyStale, "transitive dependency stale remains distinct");

    direct["S1"].currentContractFingerprint = "different-contract";
    consumer = CertificationFreshnessService::evaluateSnapshot("S2", historical, "rev", "source", "contract", "evidence", direct);
    ok &= check(consumer.status == CertificationFreshnessStatus::DependencyStale, "contract mismatch is direct dependency stale");

    QJsonObject withoutSource{{"certificateId", "cert-old"}, {"sourceRevision", "old-revision"},
                              {"evidenceFingerprint", "evidence"}};
    const auto unavailable = CertificationFreshnessService::evaluateSnapshot("F1", withoutSource,
        "new-revision", "current-source", "contract", "evidence");
    ok &= check(unavailable.status == CertificationFreshnessStatus::FreshnessIndeterminate,
                "missing historical source manifest is indeterminate, not revision-stale");
    ok &= check(!unavailable.sourceBindingComplete, "missing historical source binding is explicit");

    QJsonObject project{{"structure", QJsonObject{{"s1ResponsibilityOwnership", QJsonObject{
        {"schemaVersion", 1}, {"lifecycle", QJsonObject{{"iteration", 1}}},
        {"lifecycleHistory", QJsonArray{QJsonObject{{"iteration", 0}}}},
        {"rootId", "root"}, {"responsibilities", QJsonArray{QJsonObject{{"id", "root"}}}}}}}}};
    QString contractA;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S1", project, &contractA),
                "semantic contract projection fingerprints");
    auto structure = project.value("structure").toObject();
    auto s1Config = structure.value("s1ResponsibilityOwnership").toObject();
    s1Config["lifecycle"] = QJsonObject{{"iteration", 99}};
    s1Config["lifecycleHistory"] = QJsonArray{QJsonObject{{"iteration", 98}}};
    structure["s1ResponsibilityOwnership"] = s1Config;
    project["structure"] = structure;
    QString contractB;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S1", project, &contractB)
                    && contractA == contractB, "lifecycle state does not alter contract fingerprint");

    auto contractProject = QJsonObject{{"structure", QJsonObject{
        {"s1ResponsibilityOwnership", QJsonObject{{"rootId", "root"}}},
        {"s2PhysicalStructure", QJsonObject{{"boundaries", QJsonArray{QJsonObject{{"rootRelativePath", "src"}}}}}},
        {"s3DependencyInterfaces", QJsonObject{{"dependencies", QJsonArray{QJsonObject{{"metadata", QJsonObject{{"allowSelf", false}}}}}}}},
        {"s4CompositionEncapsulation", QJsonObject{{"compositionContracts", QJsonArray{QJsonObject{{"metadata", QJsonObject{{"multiplicityReason", "one"}}}}}}}},
        {"s5DecompositionModularity", QJsonObject{{"assessments", QJsonArray{QJsonObject{{"metadata", QJsonObject{{"unrelatedConcerns", false}}}}}}}},
        {"s6StructuralEvolutionEnforcement", QJsonObject{{"enforcementPolicies", QJsonArray{QJsonObject{{"mode", "AUDIT"}}}}}}
    }}};
    QString s1Fingerprint;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S1", contractProject, &s1Fingerprint), "S1 semantic projection available");
    auto contractStructure = contractProject.value("structure").toObject();
    auto s1Semantic = contractStructure.value("s1ResponsibilityOwnership").toObject();
    s1Semantic["rootId"] = "different-root";
    contractStructure["s1ResponsibilityOwnership"] = s1Semantic;
    contractProject["structure"] = contractStructure;
    QString changedS1Fingerprint;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S1", contractProject, &changedS1Fingerprint)
                    && changedS1Fingerprint != s1Fingerprint, "S1 semantic change changes contract fingerprint");

    QString s5Fingerprint;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S5", contractProject, &s5Fingerprint), "S5 semantic projection available");
    auto s5Config = contractStructure.value("s5DecompositionModularity").toObject();
    auto assessments = s5Config.value("assessments").toArray();
    auto assessment = assessments.first().toObject();
    assessment["metadata"] = QJsonObject{{"unrelatedConcerns", true}};
    assessments[0] = assessment;
    s5Config["assessments"] = assessments;
    contractStructure["s5DecompositionModularity"] = s5Config;
    contractProject["structure"] = contractStructure;
    QString changedS5Fingerprint;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S5", contractProject, &changedS5Fingerprint)
                    && changedS5Fingerprint != s5Fingerprint, "S5 audit metadata changes contract fingerprint");

    const auto semanticFingerprint = [&](const QString& subject, const QJsonObject& value) {
        QString fingerprint;
        return CertificationContractManifestProvider::fingerprintFromProjectJson(subject, value, &fingerprint) ? fingerprint : QString{};
    };
    const QString s2Before = semanticFingerprint("S2", contractProject);
    auto s2Config = contractStructure.value("s2PhysicalStructure").toObject();
    auto boundaries = s2Config.value("boundaries").toArray();
    auto boundary = boundaries.first().toObject(); boundary["rootRelativePath"] = "tests"; boundaries[0] = boundary;
    s2Config["boundaries"] = boundaries; contractStructure["s2PhysicalStructure"] = s2Config; contractProject["structure"] = contractStructure;
    ok &= check(!s2Before.isEmpty() && semanticFingerprint("S2", contractProject) != s2Before, "S2 semantic change changes contract fingerprint");

    contractStructure = contractProject.value("structure").toObject();
    const QString s3Before = semanticFingerprint("S3", contractProject);
    auto s3Config = contractStructure.value("s3DependencyInterfaces").toObject();
    auto dependencies = s3Config.value("dependencies").toArray(); auto dependency = dependencies.first().toObject();
    dependency["metadata"] = QJsonObject{{"allowSelf", true}}; dependencies[0] = dependency;
    s3Config["dependencies"] = dependencies; contractStructure["s3DependencyInterfaces"] = s3Config; contractProject["structure"] = contractStructure;
    ok &= check(!s3Before.isEmpty() && semanticFingerprint("S3", contractProject) != s3Before, "S3 semantic metadata changes contract fingerprint");

    contractStructure = contractProject.value("structure").toObject();
    const QString s4Before = semanticFingerprint("S4", contractProject);
    auto s4Config = contractStructure.value("s4CompositionEncapsulation").toObject(); auto contracts = s4Config.value("compositionContracts").toArray();
    auto compositionContract = contracts.first().toObject(); compositionContract["metadata"] = QJsonObject{{"multiplicityReason", "many"}}; contracts[0] = compositionContract;
    s4Config["compositionContracts"] = contracts; contractStructure["s4CompositionEncapsulation"] = s4Config; contractProject["structure"] = contractStructure;
    ok &= check(!s4Before.isEmpty() && semanticFingerprint("S4", contractProject) != s4Before, "S4 semantic metadata changes contract fingerprint");

    contractStructure = contractProject.value("structure").toObject();
    const QString s6Before = semanticFingerprint("S6", contractProject);
    auto s6Config = contractStructure.value("s6StructuralEvolutionEnforcement").toObject(); auto policies = s6Config.value("enforcementPolicies").toArray();
    auto policy = policies.first().toObject(); policy["mode"] = "ENFORCE"; policies[0] = policy;
    s6Config["enforcementPolicies"] = policies; contractStructure["s6StructuralEvolutionEnforcement"] = s6Config; contractProject["structure"] = contractStructure;
    ok &= check(!s6Before.isEmpty() && semanticFingerprint("S6", contractProject) != s6Before, "S6 policy change changes contract fingerprint");

    auto dependencyManifest = CertificationFreshnessService::dependencyManifest("S6");
    const QString originalManifestFingerprint = dependencyManifest.computedFingerprint();
    dependencyManifest.directDependencies.removeLast();
    ok &= check(originalManifestFingerprint != dependencyManifest.computedFingerprint(), "dependency graph changes manifest fingerprint");

    auto incomplete = CertificationFreshnessService::evaluateSnapshot("S6", historical, "rev", "source", "contract", "evidence");
    ok &= check(incomplete.status == CertificationFreshnessStatus::DependencyBindingIncomplete, "missing direct bindings are conservative");

    const auto manifestA = CertificationSourceManifestProvider::manifest("S6");
    const auto manifestB = CertificationSourceManifestProvider::manifest("S6");
    ok &= check(QJsonDocument(manifestA.toJson()).toJson(QJsonDocument::Compact)
                    == QJsonDocument(manifestB.toJson()).toJson(QJsonDocument::Compact), "source manifest serialization is deterministic");
    ok &= check(CertificationFreshnessService::dependencyManifest("P0").directDependencies.isEmpty(), "legacy P0 is not a current identifier");
    ok &= check(certificationFreshnessStatusName(CertificationFreshnessStatus::DependencyBindingIncomplete) == "DEPENDENCY_BINDING_INCOMPLETE", "status names are canonical");
    ok &= p1HelperManifestRegression();
    if (app.arguments().contains(QStringLiteral("--live-root"))) {
        QString error;
        const auto liveS6 = CertificationFreshnessService::evaluate(QStringLiteral("S6"), QDir::currentPath(), &error);
        ok &= check(error.isEmpty(), "live freshness evaluation is read-only and parseable");
        QJsonObject currentRevalidation;
        const bool hasCurrentRevalidation = CertificationRevalidationService::latest(QDir::currentPath(), QStringLiteral("S6"), &currentRevalidation);
        ok &= check(hasCurrentRevalidation
                        ? liveS6.status == CertificationFreshnessStatus::Fresh
                        : liveS6.status == CertificationFreshnessStatus::DependencyBindingIncomplete,
                    "S6 distinguishes historical incomplete binding from current revalidated freshness");
    }
    return ok ? 0 : 1;
}
