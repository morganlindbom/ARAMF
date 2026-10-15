#include "core/FoundationCertificationCampaign.h"
#include "core/FoundationServices.h"
#include "core/CertificationFreshness.h"
#include "core/CertificationService.h"
#include "core/CertificationRevalidation.h"
#include "core/ProjectMemory.h"
#include "core/ProjectPersistence.h"
#include "core/ProjectModel.h"
#include "core/AramfPaths.h"
#include "core/WorkerTaskServices.h"
#include "core/ContextCoordinationService.h"
#include "core/Services.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QProcess>
#include <QTextStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QLockFile>
#include <QDirIterator>
#include <QUuid>
#include <QThread>
#include <algorithm>
#include <functional>

#include "core/ConfigurationUpdateService.h"
#include "core/WorkerContextResolver.h"
#include "foundations/F2/IdentityTrustFoundation.h"
#include <QSet>
#include <QSaveFile>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif


// WorkerTaskTests.cpp
#include "core/WorkerTaskServices.h"
#include "core/CertificationFreshness.h"
#include "core/ContextCoordinationService.h"
#include "core/WorkerContextResolver.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "core/CertificationService.h"
#include "core/ProjectMemory.h"
#include "core/AramfPaths.h"
#include "core/RuntimeOwnershipService.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QMap>
#include <QProcess>
#include <QTemporaryDir>
#include <QDateTime>
#include <atomic>
#include <algorithm>
#include <functional>
#include <iostream>
#include <thread>

QJsonObject canonicalCertificationOperationAssertions();

namespace {
bool write(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray bytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
QMap<QString, QByteArray> treeBytes(const QString& root)
{
    QMap<QString, QByteArray> result;
    QDirIterator iterator(root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        result.insert(QDir(root).relativeFilePath(path), bytes(path));
    }
    return result;
}
bool hasCode(const QJsonObject& value, const QString& code)
{
    auto errors = value.value("errors").toArray();
    if (value.contains("preflight")) errors = value.value("preflight").toObject().value("errors").toArray();
    for (const auto& item : errors) if (item.toObject().value("code").toString() == code) return true;
    return false;
}
bool ready(const QJsonObject& contract) { return contract.value("preflight").toObject().value("status").toString().startsWith("READY"); }

// Adversarial fixture contracts need valid checksums so the tested authority
// boundary, rather than an unrelated checksum failure, decides the result.
QJsonValue canonicalValue(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject result;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) result.insert(it.key(), canonicalValue(it.value()));
        return result;
    }
    if (value.isArray()) {
        QList<QByteArray> values;
        for (const auto& entry : value.toArray())
            values.append(QJsonDocument(QJsonArray{canonicalValue(entry)}).toJson(QJsonDocument::Compact));
        std::sort(values.begin(), values.end());
        QJsonArray result;
        for (const auto& entry : values) result.append(QJsonDocument::fromJson(entry).array().first());
        return result;
    }
    return value;
}
void checksumFixtureContract(QJsonObject& contract)
{
    contract.remove("contractId");
    contract.insert("contractId", QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(canonicalValue(contract).toObject()).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex()));
}
}


namespace {
QJsonObject prerequisiteReadAssertions()
{
    QJsonObject results;
    const auto expect = [&](bool ok, const QString& name) { results.insert(name, ok && results.value(name).toBool(true)); return ok; };
    QTemporaryDir fixture; fixture.setAutoRemove(false); const QString root = fixture.path();
    const auto hash = [](const QByteArray& b) { return QString::fromLatin1(QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex()); };
    const auto run = [&](const QString& program, const QStringList& args) {
        QProcess process; process.setWorkingDirectory(root); process.start(program, args);
        return process.waitForFinished(120000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    };
    const auto profile = WorkerTaskServices::acceptanceProfile("task-p6-requirements-operations/v1");
    QSet<QString> files;
    for (const auto& subject : profile.value("prerequisiteSubjects").toArray())
        for (const auto& file : CertificationSourceManifestProvider::manifest(subject.toString()).files) files.insert(file);
    for (const auto& file : files) write(root + '/' + file, "// isolated manifest observation fixture\n");
    // Fixed isolated build logic exists before task preparation and measurement.
    // The ignored arm token enables exactly the deliberate mutation run; the
    // actual CMake child performs it after runCheck has captured its baseline.
    const QByteArray mutationHelper = R"cmake(
set(arm "${CMAKE_CURRENT_LIST_DIR}/build/prerequisite-mutation.arm")
if(NOT EXISTS "${arm}")
    return()
endif()
file(READ "${arm}" token)
if(NOT token MATCHES "^[0-9a-f-]+$")
    message(FATAL_ERROR "Invalid isolated mutation token")
endif()
set(source "${CMAKE_CURRENT_LIST_DIR}/src/core/CertificationService.cpp")
set(expected "// isolated manifest observation fixture\n")
file(READ "${source}" before)
if(NOT before STREQUAL expected)
    message(FATAL_ERROR "Isolated prerequisite did not have the original bytes")
endif()
file(APPEND "${source}" "// changed during real build")
file(READ "${source}" after)
string(APPEND expected "// changed during real build")
if(NOT after STREQUAL expected)
    message(FATAL_ERROR "Isolated prerequisite mutation did not persist exactly")
endif()
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/build/prerequisite-mutation.observed" "${token}")
message(STATUS "PREREQUISITE-MUTATION:${token}")
)cmake";
    const QByteArray fixtureCmake = R"cmake(cmake_minimum_required(VERSION 3.21)
project(ReadBinding NONE)
include(CTest)
add_custom_target(observed ALL
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/fixture-observed.cmake"
    VERBATIM)
)cmake";
    if (!write(root + "/fixture-observed.cmake", mutationHelper) || !write(root + "/CMakeLists.txt", fixtureCmake))
        return {{"prerequisite real build control passes", false}};
    write(root + "/.gitignore", "build/\n");
    ProjectModel model; model.setProjectPath(root); model.setProjectId("isolated-read-binding");
    auto ai = model.aiConfiguration(); ai.permissions = {"read-project-files", "modify-files", "create-files", "run-project-tools", "run-builds", "run-tests"}; model.setAiConfiguration(ai);
    auto rules = model.ruleConfiguration(); rules.projectScopes = {"source-code"};
    rules.scopeMetadata = {{"source-code", QJsonObject{{"files", profile.value("files")}, {"tests", QJsonArray{"aramf_core_tests::--worker-tasks"}}}}};
    model.setRuleConfiguration(rules);
    if (!GenerationServices().generate(model, model.generationOptions()).success) return {{"prerequisite fixture generation", false}};
    run("git", {"init", "-q"}); run("git", {"add", "."});
    run("git", {"-c", "user.name=Read Fixture", "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false", "commit", "-qm", "isolated read dependencies"});
    const auto refresh = [&]() { VerificationServices().verify(model, model.generationOptions()); ContextCoordinationService::generate(model); return VerificationServices().verify(model, model.generationOptions()).overallStatus == VerificationStatus::Pass; };
    refresh();
    WorkerTaskRequest task; task.goal = "Verify exact prerequisite source observations"; task.type = "validation"; task.scopes = {"source-code"};
    task.files = {"tests/WorkerTaskTests.cpp"}; task.acceptanceProfile = profile.value("id").toString();
    for (const auto& value : profile.value("requirements").toArray()) task.definitionOfDone << value.toString();
    const auto contract = WorkerTaskServices::prepare(model, task);
    if (!expect(ready(contract), "prerequisite fixture exact profile prepares READY")) return results;
    bool complete = true;
    for (const auto& file : files) {
        complete &= contract.value("taskDependencies").toArray().contains(file);
        for (const auto& check : QStringList{"production-build", "ctest", "acceptance-criteria"})
            complete &= contract.value("evidenceDependencies").toObject().value(check).toArray().contains(file);
    }
    expect(complete, "prerequisite verification binds every canonical F1-F4 and P1-P5 manifest file");
    expect(contract.value("taskDependencies").toArray().contains("CMakeLists.txt") && !contract.value("permittedFiles").toArray().contains("CMakeLists.txt"), "prerequisite read binding includes protected CMake without write permission");
    const QString dependency = "src/core/CertificationService.cpp", source = root + '/' + dependency; const auto original = bytes(source);
    write(source, original + "// unauthorized write\n");
    expect(hasCode(WorkerTaskServices::postflight(model, contract), "TASK_SCOPE_VIOLATION"), "unmapped prerequisite source remains forbidden to modify");
    write(source, original);
    const QString retained = source + ".retained-fixture";
    QFile::rename(source, retained);
    bool unsafe = hasCode(WorkerTaskServices::prepare(model, task), "PREREQUISITE_READ_INVALID");
    QDir().mkpath(source); unsafe &= hasCode(WorkerTaskServices::prepare(model, task), "PREREQUISITE_READ_INVALID");
    QDir().rmdir(source); QFile::rename(retained, source);
    expect(unsafe, "prerequisite read binding rejects missing unsafe or unreadable source");
    auto forged = contract; auto readset = forged.value("prerequisiteReadSet").toObject(); readset.insert("caller", QJsonArray{"LICENSE"}); forged.insert("prerequisiteReadSet", readset); checksumFixtureContract(forged);
    expect(hasCode(WorkerTaskServices::postflight(model, forged), "TASK_CONTRACT_INVALID"), "prerequisite read binding rejects caller supplied read-set expansion");
    bool omissions = true;
    for (const auto& check : QStringList{"production-build", "ctest"}) for (const auto& path : QStringList{"CMakeLists.txt", dependency}) {
        auto broken = contract; auto all = broken.value("evidenceDependencies").toObject(); auto values = all.value(check).toArray();
        for (int i = values.size() - 1; i >= 0; --i) if (values.at(i) == path) values.removeAt(i);
        all.insert(check, values); broken.insert("evidenceDependencies", all); checksumFixtureContract(broken);
        omissions &= hasCode(WorkerTaskServices::postflight(model, broken), "TASK_CONTRACT_INVALID")
            && WorkerTaskServices::runCheck(model, broken, check, "build", 120000).value("status") == "BLOCKED";
    }
    expect(omissions, "prerequisite build and CTest each reject omitted manifest coverage");
    auto legacyTask = task; legacyTask.acceptanceProfile.clear(); legacyTask.definitionOfDone = {"Observe only the mapped test fixture"};
    const auto legacy = WorkerTaskServices::prepare(model, legacyTask);
    expect(ready(legacy) && !legacy.contains("prerequisiteReadSet") && !legacy.value("request").toObject().contains("acceptanceProfile")
        && !legacy.value("taskDependencies").toArray().contains("CMakeLists.txt"), "ordinary legacy contracts retain original dependency serialization");
    auto relabeled = legacy; relabeled.insert("request", contract.value("request")); relabeled.insert("prerequisiteReadSet", contract.value("prerequisiteReadSet")); checksumFixtureContract(relabeled);
    expect(hasCode(WorkerTaskServices::postflight(model, relabeled), "TASK_CONTRACT_INVALID"), "legacy verification proof cannot be relabeled with new read dependencies");
    if (!run("cmake", {"-S", root, "-B", root + "/build", "-G", "Ninja", "-DBUILD_TESTING=ON"})) return {{"prerequisite fixture configure", false}};
    const auto produced = WorkerTaskServices::runCheck(model, contract, "production-build", "build", 120000);
    if (!expect(produced.value("status") == "PASS", "prerequisite real build control passes")) { QTextStream(stderr) << QJsonDocument(produced).toJson(); return results; }
    const auto observedEvidence = produced.value("evidence").toArray();
    const auto freshProof = WorkerTaskServices::postflight(model, contract, observedEvidence);
    write(source, original + "// stale source\n");
    const auto stale = WorkerTaskServices::postflight(model, contract, observedEvidence);
    expect(freshProof.value("acceptedEvidence").toArray() == observedEvidence && !hasCode(freshProof, "STALE_EVIDENCE")
        && hasCode(stale, "STALE_EVIDENCE") && stale.value("missingEvidence").toArray().contains("production-build")
        && stale.value("acceptedEvidence").toArray().isEmpty(), "prerequisite source change invalidates measured evidence before operation");
    const bool restoredBeforeRun = write(source, original) && bytes(source) == original;
    const auto controlEntry = observedEvidence.isEmpty() ? QJsonObject{} : observedEvidence.first().toObject();
    const auto controlRecordBytes = bytes(root + '/' + controlEntry.value("artifact").toString());
    const auto controlIndexBytes = bytes(root + '/' + produced.value("index").toString());
    const QByteArray mutationToken = QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8();
    const QString armPath = root + "/build/prerequisite-mutation.arm";
    const QString markerPath = root + "/build/prerequisite-mutation.observed";
    const bool armed = restoredBeforeRun && !QFileInfo::exists(armPath) && !QFileInfo::exists(markerPath)
        && write(armPath, mutationToken) && bytes(armPath) == mutationToken && bytes(source) == original;
    const auto changed = armed ? WorkerTaskServices::runCheck(model, contract, "production-build", "build", 120000) : QJsonObject{};
    const auto changedRecord = changed.value("record").toObject();
    const auto changedEvidence = changed.value("evidence").toArray();
    const auto changedEntry = changedEvidence.isEmpty() ? QJsonObject{} : changedEvidence.first().toObject();
    const QString recordId = changedRecord.value("recordId").toString();
    const QString recordDirectory = "build/aramf-task-evidence/" + contract.value("contractId").toString() + '/' + recordId;
    const auto persistedRecord = bytes(root + '/' + recordDirectory + "/record.json");
    const auto persistedIndex = bytes(root + '/' + recordDirectory + "/index.json");
    const bool executionEvidenceRetained = !recordId.isEmpty() && changedEvidence.size() == 1
        && changed.value("index") == recordDirectory + "/index.json"
        && changedEntry.value("artifact") == recordDirectory + "/record.json"
        && changedEntry.value("check") == "production-build" && changedEntry.value("artifactFingerprint") == hash(persistedRecord)
        && !persistedRecord.isEmpty() && QJsonDocument::fromJson(persistedRecord).object() == changedRecord
        && QJsonDocument::fromJson(persistedIndex).object() == QJsonObject{{"schemaVersion", 1},
            {"contractId", contract.value("contractId")}, {"evidence", changedEvidence}}
        && changedRecord.value("producer") == "WorkerTaskServices/run-check/v1" && changedRecord.value("check") == "production-build"
        && changedRecord.value("contractId") == contract.value("contractId") && changedRecord.value("projectId") == model.projectId()
        && changedRecord.value("projectRoot") == root && !changedRecord.value("observationId").toString().isEmpty()
        && changedRecord.value("dependencyFingerprint") == produced.value("record").toObject().value("dependencyFingerprint");
    const auto observations = changedRecord.value("observations").toArray();
    const auto command = observations.isEmpty() ? QJsonObject{} : observations.first().toObject();
    const QByteArray marker = "PREREQUISITE-MUTATION:" + mutationToken;
    const bool commandSucceeded = observations.size() == 1 && command.value("kind") == "command"
        && command.value("status") == "PASS" && command.value("started").toBool() && command.value("finished").toBool()
        && command.value("normalExit").toBool() && command.value("exitCode").isDouble() && command.value("exitCode").toInt() == 0
        && command.value("arguments") == QJsonArray{"--build", root + "/build", "--parallel", "2"}
        && command.value("workingDirectory") == root
        && QByteArray::fromBase64(command.value("stdoutBase64").toString().toLatin1()).count(marker) == 1;
    const bool mutatedDuringCommand = armed && commandSucceeded && bytes(markerPath) == mutationToken
        && bytes(source) == original + "// changed during real build";
    const bool sourceRestored = write(source, original) && bytes(source) == original;
    const bool immutableEvidenceRetained = bytes(root + '/' + recordDirectory + "/record.json") == persistedRecord
        && bytes(root + '/' + recordDirectory + "/index.json") == persistedIndex
        && bytes(root + '/' + controlEntry.value("artifact").toString()) == controlRecordBytes
        && bytes(root + '/' + produced.value("index").toString()) == controlIndexBytes;
    const bool executionFailure = mutatedDuringCommand && executionEvidenceRetained && sourceRestored && immutableEvidenceRetained
        && changed.value("status") == "FAIL" && changedRecord.value("status") == "FAIL"
        && changedRecord.value("dependenciesUnchanged").isBool() && !changedRecord.value("dependenciesUnchanged").toBool();
    if (!executionFailure) QTextStream(stderr) << "PREREQUISITE EXECUTION FIXTURE "
        << QJsonDocument(QJsonObject{{"armed", armed}, {"commandSucceeded", commandSucceeded}, {"mutatedDuringCommand", mutatedDuringCommand},
            {"retained", executionEvidenceRetained}, {"sourceRestored", sourceRestored}, {"immutableEvidenceRetained", immutableEvidenceRetained},
            {"status", changed.value("status")}, {"error", changed.value("error")}, {"recordId", recordId},
            {"recordStatus", changedRecord.value("status")}, {"dependenciesUnchanged", changedRecord.value("dependenciesUnchanged")}})
            .toJson(QJsonDocument::Compact) << Qt::endl;
    expect(executionFailure, "prerequisite source change during producer execution records failure");
    return results;
}
}


namespace {
QJsonObject linkedGrantAssertions()
{
    QJsonObject result; const auto expect = [&](bool ok, const QString& name) { result.insert(name, ok && result.value(name).toBool(true)); return ok; };
    const auto hash = [](const QByteArray& data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()); };
    QTemporaryDir fixture; fixture.setAutoRemove(false); const QString root = fixture.path();
    ProjectModel model; model.setProjectPath(root); model.setProjectId("isolated-linked-grant");
    auto ai = model.aiConfiguration(); ai.permissions = {"read-project-files", "modify-files", "create-files", "run-project-tools", "run-builds", "run-tests"}; model.setAiConfiguration(ai);
    auto memory = model.memoryConfiguration(); memory.validationOptions = {"memory-consistency", "cold-start-validation"}; model.setMemoryConfiguration(memory);
    WorkerTaskRequest task; task.goal = "Implement P6 Canonical Code Bank"; task.type = "coding"; task.scopes = {"source-code"};
    task.files = {"CMakeLists.txt", "src/processes/P6/CodeBank.h", "src/processes/P6/CodeBank.cpp", "src/processes/P6/CodeBankStorage.cpp", "src/processes/P6/CodeBankVerification.cpp", "src/processes/P6/README.md", "tests/P6CodeBankTests.cpp"};
    task.acceptanceProfile = "task-p6-build-registration/v1";
    const auto registrationProfile = WorkerTaskServices::acceptanceProfile(task.acceptanceProfile);
    for (const auto& requirement : registrationProfile.value("requirements").toArray()) task.definitionOfDone << requirement.toString();
    auto rules = model.ruleConfiguration(); rules.projectScopes = {"source-code"};
    rules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray::fromStringList(task.files + QStringList{"fixture.cpp", "fixture-gui.cpp", "src/core/WorkerTaskServices.h", "src/core/WorkerTaskServices.cpp"})}, {"tests", QJsonArray{"aramf_core_tests::--worker-tasks"}}}}}; model.setRuleConfiguration(rules);
    ProjectResource cmake; cmake.id = "protected-cmake"; cmake.location = "CMakeLists.txt"; cmake.role = "source-of-truth"; cmake.scopes = {"source-code"}; model.setResources({cmake});
    QJsonObject fixtureAssertions;
    for (const auto& criterion : registrationProfile.value("criteria").toArray()) for (const auto& name : criterion.toObject().value("assertions").toArray()) fixtureAssertions.insert(name.toString(), "PASS");
    const auto payload = QJsonDocument(QJsonObject{{"profile", registrationProfile.value("id")}, {"assertions", fixtureAssertions}, {"syntheticVerifierFixture", true}}).toJson(QJsonDocument::Compact);
    write(root + "/fixture.cpp", QByteArray("#include <cstdio>\nint main(){puts(R\"fixture(WORKER-TASK-ACCEPTANCE ") + payload + ")fixture\");puts(\"Synthetic verifier fixture completed=250 passed=250 failed=0 automatedCompleted=250 automatedPass=250 failures=0\");return 0;}\n");
    write(root + "/fixture-gui.cpp", "#include <QApplication>\n#include <QWidget>\nint main(int argc,char**argv){QApplication app(argc,argv);QWidget window;window.resize(300,200);window.show();return app.exec();}\n");
    for (const auto& path : task.files) if (path != "CMakeLists.txt") write(root + '/' + path, path.endsWith("Tests.cpp") ? QByteArray("int main(){return 0;}\n") : QByteArray("// synthetic registration verifier input\n"));
    write(root + "/src/core/WorkerTaskServices.h", "// synthetic declaration\n"); write(root + "/src/core/WorkerTaskServices.cpp", "// synthetic implementation\n");
    write(root + "/CMakeLists.txt", "cmake_minimum_required(VERSION 3.21)\nproject(ProtectedRegistrationFixture LANGUAGES CXX)\ninclude(CTest)\nfind_package(Qt6 REQUIRED COMPONENTS Core Widgets)\n"
        "add_executable(aramf fixture-gui.cpp)\ntarget_link_libraries(aramf PRIVATE Qt6::Widgets)\nforeach(name aramf_core_tests aramf_campaign_runner aramf_release_validation_runner)\nadd_executable(${name} fixture.cpp)\nendforeach()\nforeach(name aramf_core_tests aramf_template_tests aramf_workflow_tests aramf_configuration_update)\nadd_test(NAME ${name} COMMAND aramf_core_tests)\nendforeach()\n");
    write(root + "/.gitignore", "build/\n");
    if (!GenerationServices().generate(model, model.generationOptions()).success) return {{"linked grant fixture generation", false}};
    // The contract delegates memory/context refresh, not an unrelated rewrite
    // of validation reports. Inspect current canonical verification read-only.
    const auto refresh = [&](bool persistVerification = false) { QString e; ProjectMemory().validateColdStart(root, &e); ProjectMemory().validate(root, &e, true); ContextCoordinationService::generate(model); return VerificationServices().verify(model, model.generationOptions(), persistVerification).overallStatus == VerificationStatus::Pass; };
    const auto run = [&](const QString& program, const QStringList& args) { QProcess p; p.setWorkingDirectory(root); p.start(program, args); return p.waitForFinished(120000) && p.exitCode() == 0; };
    run("git", {"init", "-q"}); run("git", {"add", "."}); run("git", {"-c", "user.name=Grant Fixture", "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false", "commit", "-qm", "isolated registration inputs"});
    refresh(true); QString error; // Persist the initial PASS before any grant or TaskContract exists.
    const QString instruction = "Admin Morgan Lindbom explicitly authorizes a narrowly scoped ARAMF override for the P6 preparation remediation. Authorize deterministic folder-resource fingerprinting with explicit governed handling of generated contents, and an auditable, fail-closed, single-use grant mechanism consumed by both task preparation and postflight. The protected-path exception is limited to CMakeLists.txt for the exact P6 build and test registration in the P6 task. Preserve default protection for all other paths. Create the exact governed repair contract, run required validation and revalidation, regenerate context through its canonical service, and start P6 only if the repair contract is READY and every applicable gate passes. Do not certify or complete P6. Do not push changes.";
    const QString scope = "P6-PREP-REMEDIATION-2026-10-02-01";
    const QString action = "Prepare exact source/test repair contract using canonical configuration/context services; implement only after READY; no CMake write until separately enforced P6 grant; no P6 start until every gate passes";
    const QString expiry = QDateTime::currentDateTimeUtc().addSecs(3600).toString(Qt::ISODate);
    if (!ProjectMemory().recordAdministrativeOverride(root, instruction, "isolated synthetic protected policy", "verifier fixture only", scope, action, {"CMakeLists.txt"}, {"P6"}, false, {}, nullptr, &error)) return {{"linked grant fixture original authority: " + error, false}};
    refresh(); QJsonObject authority;
    for (const auto& event : ProjectMemory().events(root, &error)) if (event.value("eventType") == "ADMIN_OVERRIDE") authority = event;
    const QString authorityId = authority.value("eventId").toString();
    const auto restore = [&](const QMap<QString,QByteArray>& tree) { for (auto it = tree.begin(); it != tree.end(); ++it) write(root + '/' + it.key(), it.value()); };
    const auto original = treeBytes(root);
    bool invalidAuthorities = WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, "missing", expiry).value("status") == "BLOCKED";
    for (const auto& key : QStringList{"administrator", "provenance", "instruction", "scope", "requestedAction", "preColdStartValidation"}) {
        auto fields = authority; for (const auto& remove : QStringList{"eventId", "sequence", "timestamp", "eventType", "task"}) fields.remove(remove);
        fields.insert(key, key == "provenance" ? QJsonValue(QJsonObject{{"actor", "agent"}, {"agentId", "fixture"}, {"tool", "fixture"}}) : QJsonValue("invalid"));
        ProjectMemory().appendEvent(root, "ADMIN_OVERRIDE", "synthetic invalid original authority", fields, &error); refresh();
        const auto events = ProjectMemory().events(root, &error); const auto before = treeBytes(root);
        invalidAuthorities &= WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, events.last().value("eventId").toString(), expiry).value("status") == "BLOCKED" && treeBytes(root) == before;
        restore(original);
    }
    model.setWorkerNameSuffix("foreign");
    invalidAuthorities &= WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, authorityId, expiry).value("status") == "BLOCKED"
        && WorkerTaskServices::revokeP6GrantAuthority(model, "authority", authorityId, authorityId).value("status") == "BLOCKED" && treeBytes(root) == original;
    model.setWorkerNameSuffix({}); const QString oldSuffix = AramfPaths::detail::workerSuffixOverride(); AramfPaths::setRuntimeWorkerNameSuffix("foreign");
    invalidAuthorities &= WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, authorityId, expiry).value("status") == "BLOCKED"
        && WorkerTaskServices::revokeP6GrantAuthority(model, "authority", authorityId, authorityId).value("status") == "BLOCKED" && treeBytes(root) == original;
    AramfPaths::setRuntimeWorkerNameSuffix(oldSuffix);
    // A duplicate source identity is independently denied, with its exact
    // original bytes restored only inside this disposable fixture.
    write(root + "/ARAMF_WORKER/memory/event-log.jsonl", original.value("ARAMF_WORKER/memory/event-log.jsonl") + QJsonDocument(authority).toJson(QJsonDocument::Compact) + '\n');
    invalidAuthorities &= WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, authorityId, expiry).value("status") == "BLOCKED"; restore(original);
    expect(invalidAuthorities, "grant issuance rejects each invalid or nonscoped original authority");
    QFile::rename(root + "/ARAMF_WORKER/context/task-dag.json", root + "/retained-dag-fixture"); const auto noDag = treeBytes(root);
    expect(WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, authorityId, expiry).value("status") == "BLOCKED" && treeBytes(root) == noDag, "grant refresh rejects missing task DAG before mutation");
    QFile::rename(root + "/retained-dag-fixture", root + "/ARAMF_WORKER/context/task-dag.json");
    QLockFile lock(QDir::tempPath() + "/aramf-protected-grant-" + hash(QDir::cleanPath(QFileInfo(root).canonicalFilePath()).toUtf8()) + ".lock"); lock.setStaleLockTime(0);
    bool expiryLock = lock.tryLock(0) && WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, authorityId, expiry).value("status") == "BLOCKED"; lock.unlock();
    expiryLock &= WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, authorityId, "2000-01-01T00:00:00Z").value("status") == "BLOCKED";
    const auto issued = WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, authorityId, expiry);
    if (!expect(issued.value("status") == "PASS", "linked grant valid control issues")) { QTextStream(stderr) << QJsonDocument(issued).toJson(); return result; }
    const auto grant = issued.value("grant").toObject(); task.protectedGrantId = grant.value("grantId").toString();
    QJsonObject issuedEvent; int admins = 0;
    for (const auto& event : ProjectMemory().events(root, &error)) { if (event.value("eventType") == "ADMIN_OVERRIDE") ++admins; if (event.value("eventType") == "PROTECTED_GRANT_ISSUED") issuedEvent = event; }
    expect(admins == 1 && bytes(root + "/ARAMF_WORKER/memory/event-log.jsonl").startsWith(original.value("ARAMF_WORKER/memory/event-log.jsonl")), "existing authority issuance preserves original human event and count");
    expect(issuedEvent.value("provenance") == QJsonObject{{"actor", "system"}, {"agentId", "none"}, {"tool", "WorkerTaskServices"}}
        && issuedEvent.value("authorityEventId") == authorityId && issuedEvent.value("authorityEventFingerprint") == hash(QJsonDocument(authority).toJson(QJsonDocument::Compact)), "issued grant records actual service provenance and exact original event hash");
    bool sourcePreserved = true; for (const auto& path : task.files) sourcePreserved &= bytes(root + '/' + path) == original.value(path);
    expect(sourcePreserved && bytes(root + "/ARAMF_WORKER/project.json") == original.value("ARAMF_WORKER/project.json"), "grant issuance changes no CMake source or configuration bytes");
    auto retry = task; retry.protectedGrantId.clear(); const auto issuedTree = treeBytes(root);
    bool replay = WorkerTaskServices::issueP6BuildGrantFromEvent(model, retry, authorityId, expiry).value("status") == "BLOCKED"; retry.goal += " different request";
    replay &= WorkerTaskServices::issueP6BuildGrantFromEvent(model, retry, authorityId, expiry).value("status") == "BLOCKED" && treeBytes(root) == issuedTree;
    expect(replay, "grant issuance rejects second use of original authority");
    const auto contract = WorkerTaskServices::prepare(model, task);
    expect(ready(contract), "linked grant exact preparation control is READY");
    expiryLock &= lock.tryLock(0) && WorkerTaskServices::consumeP6BuildGrant(model, contract, {}).value("status") == "BLOCKED"; lock.unlock();
    expect(expiryLock, "grant expiry and concurrent issuance or consumption are rejected");
    const auto memoryReport = ProjectMemory().validate(root, &error, false);
    expect(memoryReport.value("status") == "PASS" && ContextCoordinationService::validate(model).value("valid").toBool(), "grant mutation refreshes cold-start and consistency before current verification");
    bool mismatches = true;
    for (const auto& field : QStringList{"projectId", "projectRoot", "workerIdentity", "requestFingerprint", "path", "operation", "baseFingerprint", "resultFingerprint", "registrationFingerprint", "expiresAt", "authorityEventFingerprint"}) {
        auto changed = issuedEvent; auto altered = changed.value("protectedPathGrant").toObject(); altered.insert(field, "independent-mismatch"); changed.insert("protectedPathGrant", altered);
        // Rebind the synthetic validation link to isolate each grant field;
        // authority validity still has to be independently reconstructed.
        QByteArray log;
        for (const auto& line : issuedTree.value("ARAMF_WORKER/memory/event-log.jsonl").split('\n')) {
            if (line.trimmed().isEmpty()) continue; auto event = QJsonDocument::fromJson(line).object();
            if (event.value("eventId") == issuedEvent.value("eventId")) event = changed;
            else if (event.value("eventType") == "PROTECTED_GRANT_VALIDATED") event.insert("issuedEventFingerprint", hash(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            else { log += line + '\n'; continue; }
            log += QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
        }
        write(root + "/ARAMF_WORKER/memory/event-log.jsonl", log);
        ProjectMemory().appendEvent(root, "TEST_RESULT", "synthetic mutation refresh", {{"status", "PASS"}}, &error); refresh();
        mismatches &= hasCode(WorkerTaskServices::prepare(model, task), "PROTECTED_GRANT_INVALID"); restore(issuedTree);
    }
    expect(mismatches && ready(WorkerTaskServices::prepare(model, task)), "grant independently rejects identity scope request operation and content fingerprint mismatches");
    // A persisted ISSUED event without its validation completion remains
    // inert, including after a later independent canonical refresh.
    restore(original);
    auto partialFields = issuedEvent;
    for (const auto& key : QStringList{"eventId", "sequence", "timestamp", "eventType", "task"}) partialFields.remove(key);
    const bool partialAppended = ProjectMemory().appendEvent(root, "PROTECTED_GRANT_ISSUED", "isolated interrupted issuance witness", partialFields, &error);
    const bool partialCoherent = refresh() && ProjectMemory().validate(root, &error, false).value("status") == "PASS"
        && ContextCoordinationService::validate(model).value("valid").toBool();
    expect(partialAppended && partialCoherent && hasCode(WorkerTaskServices::prepare(model, task), "PROTECTED_GRANT_INVALID"), "grant partial append never activates unvalidated authority");
    const auto partialTree = treeBytes(root); retry = task; retry.protectedGrantId.clear();
    expect(WorkerTaskServices::issueP6BuildGrantFromEvent(model, retry, authorityId, expiry).value("status") == "BLOCKED" && treeBytes(root) == partialTree, "grant retry preserves original partial event"); restore(issuedTree);
    bool revocations = true, wrongRevocation = true;
    for (const auto& kind : QStringList{"grant", "authority"}) {
        const QString target = kind == "grant" ? task.protectedGrantId : authorityId;
        const QString revokeText = "Admin Morgan Lindbom explicitly revokes protected P6 " + kind + ' ' + target + " with this administrative override.";
        ProjectMemory().recordAdministrativeOverride(root, revokeText, "isolated protected revocation", "verifier fixture", "P6-PROTECTED-GRANT-REVOCATION", "Revoke " + kind + ' ' + target, {"CMakeLists.txt"}, {"P6"}, false, {}, nullptr, &error); refresh();
        const auto events = ProjectMemory().events(root, &error); const QString revokeId = events.last().value("eventId").toString(); const auto before = treeBytes(root);
        wrongRevocation &= WorkerTaskServices::revokeP6GrantAuthority(model, kind, target + "wrong", revokeId).value("status") == "BLOCKED"
            && WorkerTaskServices::revokeP6GrantAuthority(model, kind, target, authorityId).value("status") == "BLOCKED" && treeBytes(root) == before;
        revocations &= WorkerTaskServices::revokeP6GrantAuthority(model, kind, target, revokeId).value("status") == "PASS"
            && hasCode(WorkerTaskServices::prepare(model, task), "PROTECTED_GRANT_INVALID")
            && hasCode(WorkerTaskServices::postflight(model, contract), "PROTECTED_GRANT_INVALID")
            && WorkerTaskServices::consumeP6BuildGrant(model, contract, {}).value("status") == "BLOCKED"
            && WorkerTaskServices::issueP6BuildGrantFromEvent(model, retry, authorityId, expiry).value("status") == "BLOCKED";
        restore(issuedTree);
    }
    expect(revocations, "protected authority and grant revocation independently reject every lifecycle boundary");
    expect(wrongRevocation, "protected revocation rejects forged or mismatched links");
    write(root + "/CMakeLists.txt", original.value("CMakeLists.txt") + issued.value("registration").toString().toUtf8()); refresh();
    const auto currentBoundary = WorkerTaskServices::postflight(model, contract);
    bool measured = expect(currentBoundary.value("status") == "PASS", "linked grant exact result control passes current boundary")
        && run("cmake", {"-S", root, "-B", root + "/build", "-G", "Ninja", "-DBUILD_TESTING=ON"});
    if (!measured) QTextStream(stderr) << "Linked grant initial boundary: " << QJsonDocument(currentBoundary).toJson();
    QStringList checks; QSet<QString> covered;
    for (const auto& check : contract.value("requiredEvidence").toArray()) if (check != "diff-boundary" && check != "acceptance-criteria") checks << check.toString();
    checks << "acceptance-criteria";
    for (const auto& check : checks) {
        if (!measured || covered.contains(check)) continue;
        const auto observation = WorkerTaskServices::runCheck(model, contract, check, "build", 120000);
        measured &= observation.value("status") == "PASS";
        if (!measured) QTextStream(stderr) << "Linked grant actual producer " << check << " at " << root << ": " << QJsonDocument(observation).toJson()
            << "Linked grant boundary: " << QJsonDocument(WorkerTaskServices::postflight(model, contract)).toJson();
        for (const auto& entry : observation.value("evidence").toArray()) covered.insert(entry.toObject().value("check").toString());
    }
    const auto index = WorkerTaskServices::checkEvidence(model, contract);
    const auto consumed = measured ? WorkerTaskServices::consumeP6BuildGrant(model, contract, index.value("evidence").toArray()) : QJsonObject{};
    if (consumed.value("status") != "PASS") QTextStream(stderr) << "Linked grant consumption: " << QJsonDocument(consumed).toJson();
    expect(measured && consumed.value("status") == "PASS" && WorkerTaskServices::postflight(model, contract, index.value("evidence").toArray()).value("completionState") == "VERIFIED",
        "schema2 linked grant consumes actual measured fixture proof");
    const auto consumedTree = treeBytes(root);
    expect(WorkerTaskServices::postflight(model, contract, index.value("evidence").toArray()).value("completionState") == "VERIFIED" && treeBytes(root) == consumedTree,
        "schema2 consumed inspection preserves every retained byte");
    return result;
}
}

namespace {
bool p6ReqWrite(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QString p6ReqDigest(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QJsonValue p6ReqCanonical(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject result; const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) result.insert(it.key(), p6ReqCanonical(it.value()));
        return result;
    }
    if (value.isArray()) {
        QList<QByteArray> entries;
        for (const auto& item : value.toArray()) entries.append(QJsonDocument(QJsonArray{p6ReqCanonical(item)}).toJson(QJsonDocument::Compact));
        std::sort(entries.begin(), entries.end()); QJsonArray result;
        for (const auto& entry : entries) result.append(QJsonDocument::fromJson(entry).array().first());
        return result;
    }
    return value;
}
QByteArray p6ReqBytes(const QString& path)
{
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
QString p6ReqGit(const QString& root, const QStringList& args)
{
    QProcess process;
    process.setWorkingDirectory(root);
    process.start("git", args);
    if (!process.waitForFinished(30000) || process.exitCode() != 0) return {};
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}
// Deliberately synthetic fixture evidence: tests the verifier, never production
// certification. No fixture evidence leaves this isolated temporary project.
QJsonObject p6ReqEvidence(const QString& root, const QString& subject, const QString& revision)
{
    const QString lifecycle = subject + ".1.1.0.0";
    const QString ns = "ARAMF_WORKER/certification/evidence/" + subject.toLower() + "/" + lifecycle + "/fixture";
#ifdef Q_OS_WIN
    const QString suffix = ".exe";
#else
    const QString suffix;
#endif
    const QString core = "aramf_core_tests" + suffix + "::";
    const QString domain = subject == "F2" ? "--f2-identity-trust" : subject == "F3" ? "--f3-scope-integrity" : "--f4-lifecycle-certification";
    const QMap<QString,QString> identities{
        {"domain", core + domain}, {"independence", "aramf_" + subject.toLower() + "_independence" + suffix + "::" + QDir(root).absolutePath()},
        {"certification", core + "--foundation-certification"}, {"provenance-scope", core + "--provenance-and-scope"},
        {"namespace", core + "--foundation-namespace"}, {"migration", core + "--process-migration"},
        {"integration", core + "--foundation-integration"}, {"full-ctest", "ctest::--test-dir " + QDir(root).absoluteFilePath("build") + " --output-on-failure"}};
    QJsonArray checks;
    for (auto it = identities.begin(); it != identities.end(); ++it) {
        const QByteArray bytes = "synthetic verifier fixture, not a real command result\n";
        const QString reference = ns + "/checks/" + it.key() + ".log";
        p6ReqWrite(root + "/" + reference, bytes);
        checks.append(QJsonObject{{"name", it.key()}, {"commandIdentity", it.value()}, {"sourceRevision", revision},
            {"lifecycle", lifecycle}, {"namespace", ns}, {"exitCode", 0}, {"status", "PASS"},
            {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"reference", reference}, {"fingerprint", p6ReqDigest(bytes)}});
    }
    QString source, contract;
    CertificationSourceManifestProvider::fingerprint(root, subject, &source);
    CertificationContractManifestProvider::fingerprint(subject, root, &contract);
    return {{"schemaVersion", 1}, {"verificationLevel", "HOST_TEST"},
        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"provenance", QJsonObject{{"actor", "tool"}, {"tool", "FoundationCertificationCampaign"}}},
        {"subject", subject}, {"lifecycle", lifecycle}, {"namespace", ns}, {"sourceRevision", revision},
        {"sourceFingerprint", source}, {"contractFingerprint", contract}, {"status", "PASS"}, {"checks", checks}};
}


bool p6ReqReady(const QJsonObject& contract)
{ return contract.value("preflight").toObject().value("status").toString().startsWith("READY"); }
bool p6ReqHasCode(const QJsonObject& result, const QString& code)
{
    const auto errors = result.contains("preflight") ? result.value("preflight").toObject().value("errors").toArray() : result.value("errors").toArray();
    for (const auto& error : errors) if (error.toObject().value("code") == code) return true;
    return false;
}
QMap<QString, QByteArray> p6ReqTree(const QString& root)
{
    QMap<QString, QByteArray> result;
    QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto path = it.next(); const auto relative = QDir(root).relativeFilePath(path);
        // Read-only assertions observe directory existence as well as file bytes.
        result.insert((QFileInfo(path).isDir() ? "directory:" : "file:") + relative,
                      QFileInfo(path).isDir() ? QByteArray{} : p6ReqBytes(path));
    }
    return result;
}
void p6ReqChecksum(QJsonObject& contract)
{
    contract.remove("contractId");
    contract.insert("contractId", p6ReqDigest(QJsonDocument(p6ReqCanonical(contract).toObject()).toJson(QJsonDocument::Compact)));
}
struct P6ReqFixture {
    QTemporaryDir directory;
    ProjectModel model;
    QJsonObject software;
    QJsonObject revalidation;
    QJsonObject integration;
    P6ReqFixture() { directory.setAutoRemove(false); }
};
QJsonObject p6ReqRevalidationIntent()
{
    const QStringList subjects{"F1", "P1", "P2", "P3", "P4", "P5"};
    QJsonObject semantics; for (const auto& subject : subjects) semantics.insert(subject, false);
    return {{"kind", "revalidate-current/v1"}, {"subjects", QJsonArray::fromStringList(subjects)},
        {"semanticChanges", semantics}, {"reason", "Isolated requirements coordinator regression fixture"}};
}
QJsonObject p6ReqMeasuredSoftware(ProjectModel& model, const QString& profileId)
{
    const auto refresh = [&]() {
        VerificationServices().verify(model, model.generationOptions());
        const bool generated = ContextCoordinationService::generate(model).value("success").toBool();
        const auto verified = VerificationServices().verify(model, model.generationOptions());
        if (!generated || verified.overallStatus != VerificationStatus::Pass)
            QTextStream(stderr) << "Operation fixture refresh: " << QJsonDocument(verified.summary).toJson();
        return generated && verified.overallStatus == VerificationStatus::Pass;
    };
    const auto measuredSoftware = [&]() {
        if (!refresh()) return QJsonObject{};
        WorkerTaskRequest request; request.goal = "Synthetic requirements-operation verifier fixture"; request.type = "validation";
        request.scopes = {"source-code", "tests"}; request.files = {"tests/WorkerTaskTests.cpp"};
        request.acceptanceProfile = profileId;
        if (profileId == "task-p6-activation-governance/v1") {
            request.files.clear();
            for (const auto& path : WorkerTaskServices::acceptanceProfile(profileId).value("files").toArray()) request.files.append(path.toString());
        }
        for (const auto& value : WorkerTaskServices::acceptanceProfile(request.acceptanceProfile).value("requirements").toArray()) request.definitionOfDone << value.toString();
        const auto contract = WorkerTaskServices::prepare(model, request);
        if (!p6ReqReady(contract)) { QTextStream(stderr) << QJsonDocument(contract).toJson(); return QJsonObject{}; }
        QSet<QString> covered;
        QStringList checks;
        for (const auto& item : contract.value("requiredEvidence").toArray())
            if (item != "diff-boundary" && item != "acceptance-criteria") checks.append(item.toString());
        checks.append("acceptance-criteria");
        for (const auto& check : checks) {
            if (covered.contains(check)) continue;
            const auto result = WorkerTaskServices::runCheck(model, contract, check, "build", 120000);
            if (result.value("status") != "PASS") {
                QTextStream(stderr) << "Fixture real producer failed " << check << ": " << QJsonDocument(result).toJson(); break;
            }
            for (const auto& entry : result.value("evidence").toArray()) covered.insert(entry.toObject().value("check").toString());
        }
        return contract;
    };
    return measuredSoftware();
}
bool p6ReqInitialize(P6ReqFixture& f, QString* setupError,
                     const QString& profileId = "task-p6-requirements-operations/v1")
{
    const auto fail = [&](const QString& message) {
        if (setupError) *setupError = message;
        QTextStream(stderr) << "P6 REQUIREMENTS FIXTURE SETUP: " << message << Qt::endl;
        return false;
    };
    auto& fixture = f.directory;
    if (!fixture.isValid()) return fail("Temporary fixture directory unavailable");
    const QString root = fixture.path(), config = root + "/ARAMF_WORKER.aramf.json";
    QTextStream(stdout) << "Operation fixture preserved: " << root << Qt::endl;
    QString error;
    const QStringList subjects{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"};
    QSet<QString> fileSet;
    for (const auto& subject : subjects)
        for (const auto& path : CertificationSourceManifestProvider::manifest(subject).files) fileSet.insert(path);
    for (const auto& path : WorkerTaskServices::acceptanceProfile("task-p6-requirements-operations/v1").value("files").toArray()) fileSet.insert(path.toString());
    QStringList files = fileSet.values(); std::sort(files.begin(), files.end());
    for (const auto& path : files)
        if (!p6ReqWrite(root + '/' + path, p6ReqBytes(QDir(AramfPaths::programRoot()).filePath(path)))) return fail("operation fixture source copy");
    QByteArray runner("#include <cstdio>\nint main(){\n");
    for (const auto& id : QStringList{"task-p6-requirements-operations/v1", "task-p6-activation-governance/v1"}) {
        QJsonObject syntheticAssertions;
        const auto fixtureProfile = WorkerTaskServices::acceptanceProfile(id);
        for (const auto& criterion : fixtureProfile.value("criteria").toArray())
            for (const auto& assertion : criterion.toObject().value("assertions").toArray()) syntheticAssertions.insert(assertion.toString(), "PASS");
        const auto payload = QJsonDocument(QJsonObject{{"profile", fixtureProfile.value("id")}, {"assertions", syntheticAssertions},
            {"syntheticVerifierFixture", true}}).toJson(QJsonDocument::Compact);
        runner += QByteArray("puts(R\"fixture(WORKER-TASK-ACCEPTANCE ") + payload + ")fixture\");\n";
    }
    runner += "puts(\"Synthetic verifier fixture; completed=250 passed=250 failed=0; automatedCompleted=250 automatedPass=250 failures=0\");return 0;}\n";
    p6ReqWrite(root + "/fixture-runner.cpp", runner);
    p6ReqWrite(root + "/fixture-gui.cpp", "#include <QApplication>\n#include <QWidget>\nint main(int argc,char**argv){QApplication app(argc,argv);QWidget window;window.resize(300,200);window.show();return app.exec();}\n");
    p6ReqWrite(root + "/CMakeLists.txt", "cmake_minimum_required(VERSION 3.21)\nproject(SyntheticOperationVerifier LANGUAGES CXX)\ninclude(CTest)\n"
        "find_package(Qt6 REQUIRED COMPONENTS Widgets)\nadd_executable(aramf fixture-gui.cpp)\ntarget_link_libraries(aramf PRIVATE Qt6::Widgets)\n"
        "foreach(name aramf_core_tests aramf_campaign_runner aramf_release_validation_runner)\nadd_executable(${name} fixture-runner.cpp)\nendforeach()\n"
        "foreach(name aramf_core_tests aramf_template_tests aramf_workflow_tests aramf_configuration_update)\nadd_test(NAME ${name} COMMAND aramf_core_tests)\nendforeach()\n");
    p6ReqWrite(root + "/.gitignore", "build/\n");
    p6ReqGit(root, {"init", "-q"}); p6ReqGit(root, {"config", "core.autocrlf", "false"}); p6ReqGit(root, {"add", "."});
    p6ReqGit(root, {"-c", "user.name=Operation Test", "-c", "user.email=operation@example.invalid", "-c", "commit.gpgsign=false", "commit", "-q", "-m", "isolated operation sources"});
    const QString revision = p6ReqGit(root, {"rev-parse", "HEAD"});
    if (revision.isEmpty() || !p6ReqWrite(root + "/unrelated-user.txt", "pre-existing untracked user bytes\n")) return fail("Fixture git or dirty-file setup failed");
    auto& model = f.model; model.setProjectPath(root); model.setProjectId("isolated-p6-requirements-operation");
    auto memoryConfiguration = model.memoryConfiguration();
    memoryConfiguration.validationOptions = {"memory-consistency", "conflicting-decisions", "stale-current-state"};
    if (!memoryConfiguration.maintenanceOptions.contains("record-decisions")) memoryConfiguration.maintenanceOptions.append("record-decisions");
    if (!memoryConfiguration.maintenanceOptions.contains("record-task-completion")) memoryConfiguration.maintenanceOptions.append("record-task-completion");
    if (!memoryConfiguration.maintenanceOptions.contains("update-current-state")) memoryConfiguration.maintenanceOptions.append("update-current-state");
    model.setMemoryConfiguration(memoryConfiguration);
    auto ai = model.aiConfiguration(); ai.permissions = {"read-project-files", "modify-files", "create-files", "run-builds", "run-tests", "run-project-tools"}; model.setAiConfiguration(ai);
    files << "fixture-runner.cpp" << "fixture-gui.cpp";
    auto rules = model.ruleConfiguration(); rules.projectScopes = {"source-code", "tests"};
    for (const auto& scope : rules.projectScopes)
        rules.scopeMetadata.insert(scope, QJsonObject{{"files", QJsonArray::fromStringList(files)},
            {"tests", QJsonArray{"aramf_core_tests::--worker-tasks"}}, {"riskTraits", QJsonArray{"governance", "persistence"}},
            {"affects", QJsonArray{}}, {"generatedArtifacts", QJsonArray{}}});
    auto sourceMetadata = rules.scopeMetadata.value("source-code").toObject();
    sourceMetadata.insert("folderResourcePolicies", QJsonObject{{"operation-worker", QJsonObject{{"schemaVersion", 1},
        {"generatedPaths", QJsonArray{"context/context-index.json", "context/compressed-context.json", "context/freshness.json",
            "context/agent-adapters.json", "verification/latest-validation.json", "verification/verification-result.json",
            "verification/generation-state.json", "memory/cold-start-validation.json", "memory/memory-consistency-validation.json"}}}}});
    rules.scopeMetadata.insert("source-code", sourceMetadata);
    model.setRuleConfiguration(rules);
    ProjectResource workerResource; workerResource.id = "operation-worker"; workerResource.type = "folder";
    workerResource.location = root + "/ARAMF_WORKER"; workerResource.role = "source-of-truth";
    workerResource.scopes = {"source-code"}; model.setResources({workerResource});
    auto lifecycle = ProcessVersionState::currentCanonicalState();
    // CanonicalV2's initial state already retains completed P1-P5 history.
    for (int n = 0; n < 4; ++n) {
        if (!ProcessVersionLifecycle::startNextProcess(&lifecycle, &error)
            || !ProcessVersionLifecycle::certifyCurrentIteration(&lifecycle, &error)
            || !ProcessVersionLifecycle::completeActiveProcess(&lifecycle, &error)) return fail("operation fixture lifecycle: " + error);
    }
    lifecycle.foundationIntegrationValid = true;
    auto project = ProjectPersistence().toJson(model); project.insert("processVersion", processVersionStateToJson(lifecycle));
    if (!ProjectPersistence().fromJson(&model, project, &error) || !ProjectPersistence().save(model, config, &error)
        || !ProjectPersistence().load(&model, config, &error) || !GenerationServices().generate(model, model.generationOptions()).success)
        return fail("operation fixture generation");
    if (!ProjectMemory().recordDecision(root, "fixture-original-decision", "Original fixture decision",
        "Keep the original isolated decision and event prefix.", "current", {}, &error,
        {{"actor", "agent"}, {"agentId", "fixture/setup"}, {"tool", "isolated-requirements-fixture"}}, "source-code")) return fail(error);
    p6ReqGit(root, {"add", ".gitignore"});
    p6ReqGit(root, {"-c", "user.name=Operation Test", "-c", "user.email=operation@example.invalid", "-c", "commit.gpgsign=false", "commit", "-q", "-m", "canonical fixture ignore output"});
    QMap<QString, QString> historicalArtifacts;
    for (const auto& subject : subjects) {
        const bool foundation = QStringList{"F2", "F3", "F4"}.contains(subject);
        QString source, contract;
        CertificationSourceManifestProvider::fingerprint(root, subject, &source, &error);
        CertificationContractManifestProvider::fingerprint(subject, root, &contract, &error);
        auto evidence = foundation ? p6ReqEvidence(root, subject, revision)
            : QJsonObject{{"schemaVersion", 1}, {"status", "PASS"}, {"subject", subject}, {"lifecycle", subject + ".1.1.1.1"},
                {"sourceRevision", revision}, {"sourceFingerprint", source}, {"contractFingerprint", contract}};
        if (foundation) evidence.insert("sourceSnapshot", FoundationCertificationCampaign::sourceSnapshot(root, subject, revision, &error));
        const auto bindings = CertificationDependencyBindingProvider::currentBindings(subject, root, &error);
        for (auto it = bindings.begin(); it != bindings.end(); ++it) evidence.insert(it.key(), it.value());
        const QString artifact = foundation ? evidence.value("namespace").toString() + "/evidence.json"
            : "ARAMF_WORKER/certification/evidence/operation-fixture/" + subject + ".json";
        historicalArtifacts.insert(subject, artifact);
        const auto bytes = QJsonDocument(evidence).toJson(); p6ReqWrite(root + '/' + artifact, bytes);
        const QJsonObject context{{"sourceRevision", revision}, {"evidenceArtifact", artifact}, {"evidenceFingerprint", p6ReqDigest(bytes)}, {"lifecycle", subject + ".1.1.0.0"}};
        QJsonObject started, certificate; CertificationService service;
        if (!service.start(root, subject, foundation ? "FOUNDATION" : "HOST_TEST", "project", "HOST_TEST", {"fixture"}, context, &started, &error))
            return fail("operation fixture certification start: " + error);
        for (auto it = context.begin(); it != context.end(); ++it) started.insert(it.key(), it.value());
        if (!service.issue(root, started, "PASS", {QJsonObject{{"reference", artifact}, {"fingerprint", p6ReqDigest(bytes)}, {"verified", true}, {"type", "HOST_TEST"}}}, &certificate, &error))
            return fail("operation fixture certification: " + error);
    }
    QProcess configure; configure.setWorkingDirectory(root);
    configure.start("cmake", {"-S", root, "-B", root + "/build", "-G", "Ninja", "-DBUILD_TESTING=ON"});
    if (!configure.waitForFinished(120000) || configure.exitCode() != 0) {
        QTextStream(stderr) << configure.readAllStandardOutput() << configure.readAllStandardError();
        return fail("operation fixture real build configuration");
    }
    f.software = p6ReqMeasuredSoftware(model, profileId);
    const auto measured = WorkerTaskServices::checkEvidence(model, f.software);
    const auto softwarePostflight = WorkerTaskServices::postflight(model, f.software, measured.value("evidence").toArray());
    if (measured.value("status") != "PASS" || softwarePostflight.value("completionState") != "VERIFIED")
        return fail("Real isolated software observations did not verify: " + QString::fromUtf8(QJsonDocument(measured).toJson()));
    f.revalidation = WorkerTaskServices::prepareCertificationOperation(model, p6ReqRevalidationIntent(), f.software);
    const auto revalidated = WorkerTaskServices::applyCertificationOperation(model, f.revalidation);
    if (!p6ReqReady(f.revalidation) || revalidated.value("status") != "PASS"
        || revalidated.value("postflight").toObject().value("completionState") != "VERIFIED")
        return fail("Initial revalidation failed: " + QString::fromUtf8(QJsonDocument(revalidated).toJson()));
    const QJsonObject integrationIntent{{"kind", "accept-current-integration/v1"}, {"subjects", QJsonArray{}},
        {"semanticChanges", QJsonObject{}}, {"reason", "Isolated prerequisite integration for requirements fixture"}};
    f.integration = WorkerTaskServices::prepareCertificationOperation(model, integrationIntent, f.revalidation);
    const auto integrated = WorkerTaskServices::applyCertificationOperation(model, f.integration);
    if (!p6ReqReady(f.integration) || integrated.value("status") != "PASS"
        || integrated.value("postflight").toObject().value("completionState") != "VERIFIED"
        || !ProjectPersistence().load(&model, config, &error))
        return fail("Initial integration failed: " + QString::fromUtf8(QJsonDocument(integrated).toJson()) + error);
    return true;
}
QJsonObject p6ActIntent()
{
    return {{"kind", "activate-p6/v1"}, {"agentId", "fixture/p6-activation"},
        {"reason", "Isolated activation-only regression; synthetic historical certification inputs"}};
}
bool p6ActIntegrate(P6ReqFixture& fixture, const QJsonObject& refresh, QJsonObject* integration)
{
    auto& model = fixture.model;
    *integration = WorkerTaskServices::prepareCertificationOperation(model,
        {{"kind", "accept-current-integration/v1"}, {"subjects", QJsonArray{}}, {"semanticChanges", QJsonObject{}},
         {"reason", "Current isolated integration after registered P6 requirements"}}, refresh);
    if (!p6ReqReady(*integration)) { QTextStream(stderr) << QJsonDocument(*integration).toJson(); return false; }
    const auto result = WorkerTaskServices::applyCertificationOperation(model, *integration);
    QString error;
    const bool ok = result.value("status") == "PASS" && result.value("postflight").toObject().value("completionState") == "VERIFIED"
        && ProjectPersistence().load(&model, model.projectFilePath(), &error);
    if (!ok) QTextStream(stderr) << "Activation fixture integration: " << QJsonDocument(result).toJson() << error;
    return ok;
}
bool p6ActInitial(const ProjectModel& model)
{
    const auto state = processVersionStateToJson(model.processVersionState());
    const QJsonObject active{{"process", 6}, {"loop", 1}, {"iteration", 1}, {"certification", 0}, {"done", 0}};
    const QJsonObject next{{"process", 7}, {"loop", 1}, {"iteration", 0}, {"certification", 0}, {"done", 0}};
    return model.processVersionState().hasActiveProcess && !model.processVersionState().hasActiveStructure
        && state.value("active") == active && state.value("next") == next;
}
QJsonObject p6ActStage(const QJsonObject& receipt, const QString& name)
{
    QJsonObject found;
    for (const auto& item : receipt.value("stages").toArray()) {
        const auto stage = item.toObject();
        if (stage.value("stage") == name) { if (!found.isEmpty()) return {}; found = stage; }
    }
    return found;
}
void p6ActivationAssertions(P6ReqFixture& fixture, const QJsonObject& refreshPlan, QJsonObject* assertions)
{
    const auto expect = [&](bool pass, const QString& name) {
        assertions->insert(name, pass);
        if (!pass) QTextStream(stderr) << "P6 ACTIVATION FIXTURE FAIL: " << name << Qt::endl;
        return pass;
    };
    auto& model = fixture.model;
    const QString root = fixture.directory.path(), config = model.projectFilePath();
    const auto profile = WorkerTaskServices::acceptanceProfile("task-p6-activation-governance/v1");
    QStringList profileFiles; for (const auto& path : profile.value("files").toArray()) profileFiles.append(path.toString());
    auto expectedFiles = QStringList{"src/core/WorkerTaskServices.cpp", "src/core/WorkerTaskServices.h", "tests/WorkerTaskTests.cpp"};
    profileFiles.sort(); expectedFiles.sort();
    expect(profileFiles == expectedFiles && profile.value("criteria").toArray().size() == 13,
        "activation profile binds exact three implementation files");
    bool readUnion = p6ReqReady(fixture.software);
    for (const auto& subject : QStringList{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"}) {
        for (const auto& file : CertificationSourceManifestProvider::manifest(subject).files) {
            readUnion &= fixture.software.value("taskDependencies").toArray().contains(file);
            for (const auto& check : QStringList{"production-build", "ctest", "acceptance-criteria"})
                readUnion &= fixture.software.value("evidenceDependencies").toObject().value(check).toArray().contains(file);
            if (!expectedFiles.contains(file)) readUnion &= !fixture.software.value("permittedFiles").toArray().contains(file);
        }
    }
    expect(readUnion, "activation profile derives complete prerequisite read union without write expansion");
    QJsonObject integration;
    if (!p6ActIntegrate(fixture, refreshPlan, &integration)) return;
    ProjectPersistence persistence; QString error;
    const auto beforeModel = persistence.toJson(model), beforeLifecycle = processVersionStateToJson(model.processVersionState());
    const auto beforeTree = p6ReqTree(root);
    WorkerTaskRequest legacyRequest; legacyRequest.goal = "Legacy serialization control"; legacyRequest.type = "validation";
    // The fixture maps this shared file to both enabled owners. The legacy
    // control must declare both scopes before testing its serialization.
    legacyRequest.scopes = {"source-code", "tests"}; legacyRequest.files = {"tests/WorkerTaskTests.cpp"}; legacyRequest.definitionOfDone = {"Observe the mapped fixture"};
    const auto legacy = WorkerTaskServices::prepare(model, legacyRequest);
    expect(p6ReqReady(legacy) && !legacy.contains("prerequisiteReadSet")
        && !legacy.value("request").toObject().contains("acceptanceProfile") && p6ReqTree(root) == beforeTree,
        "activation profile preserves legacy contract serialization");
    const auto intent = p6ActIntent();
    QElapsedTimer prepareTimer; prepareTimer.start();
    auto plan = WorkerTaskServices::prepareActivationOperation(model, intent, integration);
    qint64 measuredPrepareMs = prepareTimer.elapsed();
    expect(p6ReqReady(plan) && p6ReqTree(root) == beforeTree, "P6 activation preparation is read-only");
    if (!p6ReqReady(plan)) { QTextStream(stderr) << QJsonDocument(plan).toJson(); return; }
    prepareTimer.restart();
    const auto otherPlan = WorkerTaskServices::prepareActivationOperation(model, intent, integration);
    measuredPrepareMs = qMax(measuredPrepareMs, prepareTimer.elapsed());
    const auto timing = [](const QString& stage, QJsonObject observation) {
        observation.insert("stage", stage);
        observation.insert("observedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        QTextStream(stdout) << "P6-ACTIVATION-TIMING " << QJsonDocument(observation).toJson(QJsonDocument::Compact) << Qt::endl;
    };
    timing("initial-preparations", {{"maxPrepareMs", double(measuredPrepareMs)}, {"preflight", otherPlan.value("preflight")}});
    const auto waitPast = [](const QDateTime& deadline, qint64 maximumWaitMs) {
        if (!deadline.isValid()) return false;
        QElapsedTimer waiting; waiting.start();
        while (QDateTime::currentDateTimeUtc() <= deadline && waiting.elapsed() < maximumWaitMs) QThread::msleep(25);
        return QDateTime::currentDateTimeUtc() > deadline;
    };
    ProjectModel projected;
    if (!persistence.fromJson(&projected, beforeModel, &error) || !projected.startNextProcess(&error)) return;
    const auto expectedModel = persistence.toJson(projected);
    QJsonObject owners;
    for (const auto& path : ConfigurationUpdateService::effectiveOutputFiles(projected)) owners.insert(path, "ConfigurationUpdateService");
    owners.insert(QDir(root).relativeFilePath(config), "ProjectPersistence");
    for (const auto& file : QStringList{"event-log.jsonl", "memory-manifest.json", "current-state.md", "metrics.json",
                                     "memory-consistency-validation.json", "cold-start-validation.json"})
        owners.insert("ARAMF_WORKER/memory/" + file, "ProjectMemory");
    owners.insert("ARAMF_WORKER/PROJECT_STATUS.md", "ProjectMemory");
    for (const auto& file : QStringList{"context-index.json", "compressed-context.json", "freshness.json", "agent-adapters.json"})
        owners.insert("ARAMF_WORKER/context/" + file, "ContextCoordinationService");
    for (const auto& path : QStringList{"ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json",
                                      "ARAMF_WORKER/memory/memory-consistency-validation.json"}) owners.insert(path, "VerificationServices");
    const auto expectedPermissions = [&](const QJsonObject& contract) {
        auto all = owners;
        const QString prefix = "ARAMF_WORKER/verification/activation/" + contract.value("operationId").toString() + '/';
        for (const auto& file : QStringList{"proof.json", "receipt.json", "seal.json"}) all.insert(prefix + file, "WorkerTaskServices");
        QJsonArray permissions;
        for (auto it = all.begin(); it != all.end(); ++it)
            permissions.append(QJsonObject{{"path", it.key()}, {"owner", it.value()},
                {"actions", QJsonArray{"READ", it.key().startsWith(prefix) ? "GENERATE_ONCE" : "UPDATE_DERIVED"}}});
        return QJsonObject{{"files", QJsonArray::fromStringList(all.keys())}, {"permissions", permissions}};
    };
    const auto inventory = expectedPermissions(plan);
    bool finite = p6ReqReady(otherPlan) && otherPlan.value("operationId") != plan.value("operationId")
        && plan.value("permittedFiles") == inventory.value("files") && plan.value("filePermissions") == inventory.value("permissions");
    for (const auto& path : plan.value("permittedFiles").toArray())
        finite &= !path.toString().contains("..") && !QFileInfo(root + '/' + path.toString()).isDir()
            && path != "CMakeLists.txt" && !path.toString().startsWith("src/") && !path.toString().startsWith("tests/")
            && !path.toString().startsWith("ARAMF_WORKER/code-bank/");
    expect(finite && p6ReqTree(root) == beforeTree, "P6 activation derives unique fixed paths and finite producer-owned outputs");
    const auto acceptance = plan.value("acceptance").toObject();
    const auto acceptanceBytes = p6ReqBytes(root + '/' + acceptance.value("reference").toString());
    bool bound = plan.value("beforeModel") == beforeModel && plan.value("proposedModel") == expectedModel
        && plan.value("verificationContract") == integration && plan.value("softwareProof") == integration.value("softwareProof")
        && plan.value("sourceBindings") == integration.value("sourceBindings")
        && !acceptanceBytes.isEmpty() && p6ReqDigest(acceptanceBytes) == acceptance.value("fingerprint")
        && QJsonDocument::fromJson(acceptanceBytes).object() == acceptance.value("record").toObject();
    for (const auto& subject : QStringList{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"}) {
        const auto current = CertificationFreshnessService::evaluate(subject, root, &error);
        const QJsonObject expected{{"certificateId", current.historicalCertificateId},
            {"sourceFingerprint", current.currentSourceFingerprint}, {"contractFingerprint", current.currentContractFingerprint},
            {"evidenceFingerprint", current.currentEvidenceFingerprint}, {"dependencyManifestFingerprint", current.dependencyManifestFingerprint}};
        bound &= error.isEmpty() && current.status == CertificationFreshnessStatus::Fresh
            && plan.value("prerequisiteBindings").toObject().value(subject) == expected;
    }
    expect(bound, "P6 activation binds original proof all prerequisites acceptance and projected lifecycle");
    const auto proof = plan.value("softwareProof").toObject();
    const auto software = proof.value("softwareContract").toObject();
    bool measured = software.value("request").toObject().value("acceptanceProfile") == profile.value("id")
        && proof.value("postflight").toObject().value("completionState") == "VERIFIED" && !proof.value("records").toObject().isEmpty();
    for (const auto& check : QStringList{"production-build", "ctest", "acceptance-criteria"})
        measured &= software.value("requiredEvidence").toArray().contains(check);
    expect(measured, "activation software contract requires production build complete CTest and named observations");
    bool negativeBytes = true;
    const auto rejectsPlan = [&](QJsonObject forged) {
        p6ReqChecksum(forged);
        const auto tree = p6ReqTree(root);
        const bool blocked = WorkerTaskServices::applyActivationOperation(model, forged).value("status") == "BLOCKED"
            && p6ReqTree(root) == tree;
        negativeBytes &= blocked;
        return blocked;
    };
    bool authority = true;
    for (const auto& key : QStringList{"authority", "permittedFiles", "filePermissions", "proposedModel", "sourceBindings", "prerequisiteBindings", "acceptance", "softwareProof"}) {
        auto forged = plan;
        if (key == "authority") forged.insert(key, "HUMAN");
        else if (key == "permittedFiles") { auto paths = forged.value(key).toArray(); paths.append("CMakeLists.txt"); forged.insert(key, paths); }
        else if (key == "filePermissions") forged.insert(key, QJsonArray{QJsonObject{{"path", "CMakeLists.txt"}, {"owner", "caller"}, {"actions", QJsonArray{"WRITE"}}}});
        else { auto object = forged.value(key).toObject(); object.insert("callerAuthority", "PASS"); forged.insert(key, object); }
        authority &= rejectsPlan(forged);
    }
    expect(authority, "P6 activation rejects forged authority permissions paths results and lifecycle");
    bool malformed = true;
    for (const auto& key : QStringList{"unexpected", "expiresAt", "integrationReceiptTimestamp", "operationId", "projectBinding"}) {
        auto forged = plan;
        if (key == "projectBinding") { auto identity = forged.value(key).toObject(); identity.insert("projectId", "foreign-fixture"); forged.insert(key, identity); }
        else forged.insert(key, key == "operationId" ? "../../outside" : "caller-substitution");
        malformed &= rejectsPlan(forged);
    }
    bool typed = true;
    for (const auto& key : QStringList{"permissions", "path", "authority", "sourceFingerprint", "result", "lifecycle", "issuedAt", "expiresAt", "integrationReceiptTimestamp"}) {
        auto invalid = intent; invalid.insert(key, "caller-value");
        typed &= !p6ReqReady(WorkerTaskServices::prepareActivationOperation(model, invalid, integration)) && p6ReqTree(root) == beforeTree;
    }
    for (const auto& value : QList<QJsonValue>{0, 3601, 1.5, "1", QJsonValue(QJsonValue::Null), true}) {
        auto invalid = intent; invalid.insert("validitySeconds", value);
        typed &= !p6ReqReady(WorkerTaskServices::prepareActivationOperation(model, invalid, integration)) && p6ReqTree(root) == beforeTree;
    }
    const auto anchor = QDateTime::fromString(plan.value("integrationReceiptTimestamp").toString(), Qt::ISODateWithMs);
    const auto expiry = QDateTime::fromString(plan.value("expiresAt").toString(), Qt::ISODateWithMs);
    expect(typed && anchor.isValid() && expiry.isValid() && anchor.msecsTo(expiry) == 3600000
        && anchor <= QDateTime::currentDateTimeUtc(), "P6 activation validity is bounded with service-derived timestamps");
    bool required = !p6ReqReady(WorkerTaskServices::prepareActivationOperation(model, intent, fixture.integration));
    for (const auto& remove : QStringList{"p6CodeBankRequirements", "criteria"}) {
        auto changed = beforeModel; auto rules = changed.value("rules").toObject(); auto metadata = rules.value("scopeMetadata").toObject();
        auto owner = metadata.value("source-code").toObject();
        if (remove == "p6CodeBankRequirements") owner.remove(remove);
        else { auto baseline = owner.value("p6CodeBankRequirements").toObject(); baseline.insert("criteria", QJsonArray{}); owner.insert("p6CodeBankRequirements", baseline); }
        metadata.insert("source-code", owner); rules.insert("scopeMetadata", metadata); changed.insert("rules", rules);
        ProjectModel invalid;
        required &= persistence.fromJson(&invalid, changed, &error)
            && !p6ReqReady(WorkerTaskServices::prepareActivationOperation(invalid, intent, integration)) && p6ReqTree(root) == beforeTree;
    }
    ProjectModel missingStateUpdater;
    bool missingUpdaterRejected = persistence.fromJson(&missingStateUpdater, beforeModel, &error);
    if (missingUpdaterRejected) {
        auto memory = missingStateUpdater.memoryConfiguration(); memory.maintenanceOptions.removeAll("update-current-state");
        missingStateUpdater.setMemoryConfiguration(memory);
        const auto rejected = WorkerTaskServices::prepareActivationOperation(missingStateUpdater, intent, integration);
        bool auditGateRejected = false;
        for (const auto& item : rejected.value("preflight").toObject().value("errors").toArray())
            auditGateRejected |= item.toObject().value("code") == "ACTIVATION_AUDIT_INVALID";
        // The explicit audit error is required, independently of the saved
        // model mismatch that this pure in-memory negative also introduces.
        missingUpdaterRejected = auditGateRejected && !p6ReqReady(rejected) && p6ReqTree(root) == beforeTree;
    }
    required &= missingUpdaterRejected;
    expect(required, "P6 activation requires verified integration and complete registered requirements");
    bool lifecycleRejected = true;
    for (const auto& key : QStringList{"projectId", "workerNameSuffix", "namespaceVersion", "active", "activeStructure", "next"}) {
        auto changed = beforeModel; auto lifecycle = beforeLifecycle;
        if (key == "projectId" || key == "workerNameSuffix") changed.insert(key, "foreign-fixture");
        else {
            if (key == "namespaceVersion") lifecycle.insert(key, 1);
            else if (key == "activeStructure") lifecycle.insert(key, QJsonObject{{"structure", 1}, {"loop", 1}, {"iteration", 1}, {"certification", 0}, {"done", 0}});
            else lifecycle.insert(key, processVersionStateToJson(projected.processVersionState()).value(key));
            changed.insert("processVersion", lifecycle);
        }
        ProjectModel invalid;
        const bool loaded = persistence.fromJson(&invalid, changed, &error);
        lifecycleRejected &= (!loaded || !p6ReqReady(WorkerTaskServices::prepareActivationOperation(invalid, intent, integration))) && p6ReqTree(root) == beforeTree;
    }
    expect(lifecycleRejected, "P6 activation rejects wrong identity namespace active lifecycle or next process");
    const auto staleFile = [&](const QString& relative) {
        const QString path = root + '/' + relative; const auto original = p6ReqBytes(path);
        if (original.isEmpty() || !p6ReqWrite(path, original + " \n")) return false;
        const auto changedTree = p6ReqTree(root);
        const bool blocked = WorkerTaskServices::applyActivationOperation(model, plan).value("status") == "BLOCKED"
            && p6ReqTree(root) == changedTree;
        const bool restored = p6ReqWrite(path, original);
        negativeBytes &= blocked && restored;
        return blocked && restored;
    };
    bool stale = true;
    for (const auto& relative : QStringList{"src/core/ConfigurationUpdateService.h", "ARAMF_WORKER/certification/certificates.jsonl",
             QDir(root).relativeFilePath(config), acceptance.value("reference").toString()}) stale &= staleFile(relative);
    // Read-only preparation must reject legacy schema without invoking the
    // migration-capable persistence loader or changing any project byte.
    const auto currentConfig = p6ReqBytes(config);
    for (const bool missingVersion : {false, true}) {
        auto legacySaved = beforeModel;
        if (missingVersion) legacySaved.remove("schemaVersion"); else legacySaved.insert("schemaVersion", 1);
        const bool wrote = p6ReqWrite(config, QJsonDocument(legacySaved).toJson());
        const auto legacyTree = p6ReqTree(root);
        const bool rejected = wrote && !p6ReqReady(WorkerTaskServices::prepareActivationOperation(model, intent, integration))
            && p6ReqTree(root) == legacyTree;
        const bool restored = p6ReqWrite(config, currentConfig);
        malformed &= rejected && restored;
        negativeBytes &= rejected && restored;
    }
    expect(malformed && typed && stale && p6ReqTree(root) == beforeTree,
        "P6 activation rejects malformed unknown foreign or stale plans before writes");
    auto omitted = plan; auto missingProof = proof; auto records = missingProof.value("records").toObject();
    bool missing = !records.isEmpty(); if (missing) records.remove(records.keys().first()); missingProof.insert("records", records); omitted.insert("softwareProof", missingProof);
    expect(missing && rejectsPlan(omitted) && stale, "activation software evidence rejects missing observations and covered source drift");
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + p6ReqDigest(root.toUtf8()) + ".lock");
    lock.setStaleLockTime(0); const bool locked = lock.tryLock(0);
    const bool concurrent = locked && WorkerTaskServices::applyActivationOperation(model, plan).value("status") == "BLOCKED" && p6ReqTree(root) == beforeTree;
    if (locked) lock.unlock();
    expect(concurrent, "P6 activation lock rejects concurrent apply");
    auto expiringIntent = intent;
    const qint64 elapsedSeconds = (anchor.msecsTo(QDateTime::currentDateTimeUtc()) + 999) / 1000;
    // Real preparation includes nested canonical freshness and integration
    // checks. Budget from observed latency while keeping the wall-clock wait
    // finite; a slow or out-of-range attempt remains a genuine test failure.
    const qint64 expiryMargin = qBound<qint64>(qint64(5), 3 * ((measuredPrepareMs + 999) / 1000) + 2, qint64(120));
    const qint64 expiryDuration = elapsedSeconds + expiryMargin;
    const bool expiryRange = anchor.isValid() && expiryDuration >= 1 && expiryDuration <= 3600;
    expiringIntent.insert("validitySeconds", int(expiryDuration));
    const auto expiryPrepareStarted = QDateTime::currentDateTimeUtc(); prepareTimer.restart();
    const auto expiring = expiryRange ? WorkerTaskServices::prepareActivationOperation(model, expiringIntent, integration) : QJsonObject{};
    const qint64 expiryPrepareMs = prepareTimer.elapsed();
    measuredPrepareMs = qMax(measuredPrepareMs, expiryPrepareMs);
    const auto expiresAt = QDateTime::fromString(expiring.value("expiresAt").toString(), Qt::ISODateWithMs);
    const bool expiryReady = expiryRange && p6ReqReady(expiring) && expiresAt.isValid();
    bool expired = expiryReady;
    QJsonObject expiryResult;
    bool expiredBeforeApply = false;
    if (expired) {
        expiredBeforeApply = waitPast(expiresAt, (expiryMargin + 2) * 1000);
        if (expiredBeforeApply) expiryResult = WorkerTaskServices::applyActivationOperation(model, expiring);
        expired &= expiredBeforeApply && expiryResult.value("status") == "BLOCKED" && p6ReqTree(root) == beforeTree;
    }
    timing("expired-plan", {{"prepareStartedAt", expiryPrepareStarted.toString(Qt::ISODateWithMs)},
        {"prepareMs", double(expiryPrepareMs)}, {"marginSeconds", double(expiryMargin)}, {"validitySeconds", double(expiryDuration)},
        {"validityInRange", expiryRange}, {"readyBeforeWait", expiryReady}, {"expiresAt", expiring.value("expiresAt")},
        {"expiredBeforeApply", expiredBeforeApply}, {"preflight", expiring.value("preflight")}, {"result", expiryResult}});
    expect(expired, "P6 activation actual expiry rejects without writes");
    negativeBytes &= authority && malformed && typed && required && lifecycleRejected && stale && missing && concurrent && expired;

    // Allow real preparation, apply rederivation and eligibility checks to
    // finish, then exercise historical postflight after this bounded window.
    auto successfulIntent = intent;
    const qint64 successMargin = qBound<qint64>(qint64(120), 6 * ((measuredPrepareMs + 999) / 1000) + 30, qint64(240));
    const qint64 successDuration = (anchor.msecsTo(QDateTime::currentDateTimeUtc()) + 999) / 1000 + successMargin;
    const bool successRange = anchor.isValid() && successDuration >= 1 && successDuration <= 3600;
    successfulIntent.insert("validitySeconds", int(successDuration));
    const auto successPrepareStarted = QDateTime::currentDateTimeUtc(); prepareTimer.restart();
    plan = successRange ? WorkerTaskServices::prepareActivationOperation(model, successfulIntent, integration) : QJsonObject{};
    timing("successful-plan", {{"prepareStartedAt", successPrepareStarted.toString(Qt::ISODateWithMs)},
        {"prepareMs", double(prepareTimer.elapsed())}, {"marginSeconds", double(successMargin)}, {"validitySeconds", double(successDuration)},
        {"validityInRange", successRange}, {"expiresAt", plan.value("expiresAt")}, {"preflight", plan.value("preflight")}});
    if (!p6ReqReady(plan)) { QTextStream(stderr) << QJsonDocument(plan).toJson(); return; }
    const auto applyStarted = QDateTime::currentDateTimeUtc(); QElapsedTimer applyTimer; applyTimer.start();
    const auto result = WorkerTaskServices::applyActivationOperation(model, plan);
    timing("actual-apply", {{"applyStartedAt", applyStarted.toString(Qt::ISODateWithMs)}, {"applyMs", double(applyTimer.elapsed())},
        {"expiresAt", plan.value("expiresAt")}, {"status", result.value("status")}, {"error", result.value("error")}});
    const QString prefix = "ARAMF_WORKER/verification/activation/" + plan.value("operationId").toString();
    const auto receipt = QJsonDocument::fromJson(p6ReqBytes(root + '/' + prefix + "/receipt.json")).object();
    const auto seal = QJsonDocument::fromJson(p6ReqBytes(root + '/' + prefix + "/seal.json")).object();
    ProjectModel restarted;
    error.clear();
    const bool loaded = persistence.load(&restarted, config, &error);
    if (result.value("status") != "PASS" || !loaded) QTextStream(stderr) << QJsonDocument(result).toJson() << error;
    const auto afterModel = persistence.toJson(restarted), afterLifecycle = processVersionStateToJson(restarted.processVersionState());
    const auto afterTree = p6ReqTree(root);
    const auto gates = receipt.value("gates").toObject();
    const bool actualGates = gates.value("foundationEligibility").toObject().value("success").toBool()
        && gates.value("f4Eligibility").toObject().value("success").toBool()
        && p6ActStage(receipt, "foundation-eligibility").value("success").toBool()
        && p6ActStage(receipt, "f4-eligibility").value("success").toBool();
    expect(actualGates && stale && result.value("status") == "PASS",
        "P6 activation rederives frozen inputs and records both actual eligibility gates");
    auto beforeOther = beforeModel, afterOther = afterModel; beforeOther.remove("processVersion"); afterOther.remove("processVersion");
    const bool saved = loaded && result.value("status") == "PASS" && afterModel == expectedModel
        && p6ActStage(receipt, "configuration").value("saved").toBool()
        && p6ActStage(receipt, "configuration").value("derivedSynchronized").toBool()
        && p6ActStage(receipt, "configuration").value("verified").toBool();
    expect(saved && beforeOther == afterOther, "P6 activation persists only the canonical F4 start delta");
    expect(saved && p6ActInitial(restarted), "P6 activation reloads initial P6.1.1.0.0 and next P7.1.0.0.0");
    expect(saved && beforeOther == afterOther && beforeModel.value("academic") == afterModel.value("academic")
        && beforeModel.value("generationOptions") == afterModel.value("generationOptions"), "P6 activation preserves unrelated model and academic settings");
    QStringList allowed; for (const auto& path : plan.value("permittedFiles").toArray()) allowed.append(path.toString());
    bool boundary = plan.value("permittedFiles") == expectedPermissions(plan).value("files")
        && plan.value("filePermissions") == expectedPermissions(plan).value("permissions");
    QStringList paths = beforeTree.keys() + afterTree.keys(); paths.removeDuplicates();
    for (const auto& key : paths) {
        if (beforeTree.contains(key) && afterTree.contains(key) && beforeTree.value(key) == afterTree.value(key)) continue;
        if (key.startsWith("file:")) boundary &= allowed.contains(key.mid(5)) && afterTree.contains(key);
        else { bool parent = false; for (const auto& path : allowed) parent |= path.startsWith(key.mid(10) + '/'); boundary &= parent && afterTree.contains(key); }
    }
    expect(saved && boundary, "P6 activation writes only exact canonical and immutable operation outputs");
    bool protectedBytes = true;
    for (auto it = beforeTree.begin(); it != beforeTree.end(); ++it)
        if (it.key().startsWith("file:src/") || it.key().startsWith("file:tests/") || it.key() == "file:CMakeLists.txt"
            || it.key().startsWith("file:ARAMF_WORKER/certification/") || it.key() == "file:ARAMF_WORKER/context/task-dag.json")
            protectedBytes &= afterTree.contains(it.key()) && afterTree.value(it.key()) == it.value();
    protectedBytes &= !QFileInfo::exists(root + "/ARAMF_WORKER/code-bank") && !QFileInfo::exists(root + "/src/processes/P6");
    expect(saved && protectedBytes, "P6 activation preserves code CMake bank certificate and task DAG bytes");
    const auto eventsBefore = beforeTree.value("file:ARAMF_WORKER/memory/event-log.jsonl"), eventsAfter = p6ReqBytes(root + "/ARAMF_WORKER/memory/event-log.jsonl");
    bool audit = eventsAfter.startsWith(eventsBefore); int matchingEvents = 0;
    for (const auto& line : eventsAfter.mid(eventsBefore.size()).split('\n')) {
        if (line.trimmed().isEmpty()) continue;
        const auto event = QJsonDocument::fromJson(line).object();
        if (event.value("operationId") != plan.value("operationId")) continue;
        ++matchingEvents;
        audit &= event.value("eventType") == "TASK_STARTED" && event.value("contractId") == plan.value("contractId")
            && event.value("startedAt") == receipt.value("startedAt") && event.value("scope") == "project"
            && event.value("provenance") == QJsonObject{{"actor", "agent"}, {"agentId", intent.value("agentId")}, {"tool", "WorkerTaskServices/p6-activation-operation/v1"}};
    }
    const bool receiptValid = actualGates && receipt.value("outcome") == "P6_STARTED"
        && p6ActStage(receipt, "start").value("success").toBool() && p6ActStage(receipt, "audit").value("success").toBool()
        && receipt.value("observedLifecycle") == afterLifecycle && receipt.value("stages").toArray().size() == 5
        && seal.value("receiptFingerprint") == p6ReqDigest(p6ReqBytes(root + '/' + prefix + "/receipt.json"));
    expect(saved && receiptValid && audit && matchingEvents == 1, "P6 activation receipt records actual gates and start without certification");
    expect(saved && eventsAfter.startsWith(eventsBefore)
        && beforeTree.value("file:ARAMF_WORKER/memory/decisions.md") == afterTree.value("file:ARAMF_WORKER/memory/decisions.md")
        && beforeLifecycle.value("completedHistory") == afterLifecycle.value("completedHistory")
        && beforeLifecycle.value("structureHistory") == afterLifecycle.value("structureHistory") && protectedBytes,
        "P6 activation preserves original event decision and completed lifecycle history");
    const bool replay = WorkerTaskServices::applyActivationOperation(restarted, plan).value("status") == "BLOCKED"
        && !p6ReqReady(WorkerTaskServices::prepareActivationOperation(restarted, intent, integration)) && p6ReqTree(root) == afterTree;
    expect(replay, "P6 activation replay preserves original artifacts");
    const auto postflight = WorkerTaskServices::postflightActivationOperation(restarted, plan);
    expect(saved && postflight.value("completionState") == "VERIFIED" && p6ActInitial(restarted),
        "P6 activation postflight verifies initial lifecycle requirements acceptance and current prerequisites");
    const auto tamper = [&](const QString& relative, const QByteArray& changed) {
        const QString path = root + '/' + relative; const auto original = p6ReqBytes(path);
        if (original.isEmpty() || !p6ReqWrite(path, changed)) return false;
        const auto tree = p6ReqTree(root);
        const bool rejected = WorkerTaskServices::postflightActivationOperation(restarted, plan).value("status") == "FAIL" && p6ReqTree(root) == tree;
        const bool restored = p6ReqWrite(path, original); return rejected && restored;
    };
    bool tampered = true;
    QStringList tamperPaths{prefix + "/proof.json", prefix + "/receipt.json", prefix + "/seal.json",
        QDir(root).relativeFilePath(config), "ARAMF_WORKER/project.json", "ARAMF_WORKER/verification/generation-state.json",
        "ARAMF_WORKER/context/context-index.json", "ARAMF_WORKER/context/compressed-context.json", "ARAMF_WORKER/context/freshness.json",
        "ARAMF_WORKER/context/agent-adapters.json", "ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json",
        "ARAMF_WORKER/memory/memory-consistency-validation.json", "ARAMF_WORKER/context/task-dag.json",
        "ARAMF_WORKER/certification/certificates.jsonl", "src/core/ConfigurationUpdateService.h", acceptance.value("reference").toString()};
    const auto measuredRecords = proof.value("records").toObject();
    bool disposableNeutral = false, capturedRejected = false;
    if (!measuredRecords.isEmpty()) {
        const QString reference = measuredRecords.keys().first();
        const QString disposablePath = root + '/' + reference;
        const auto original = p6ReqBytes(disposablePath);
        // The authenticated record bytes are retained inside the sealed proof.
        // An ignored original report is disposable after capture; changing it
        // must neither invalidate the captured proof nor mutate that proof.
        if (reference.startsWith("build/aramf-task-evidence/") && !original.isEmpty()
            && p6ReqWrite(disposablePath, original + "\nchanged disposable original report\n")) {
            const auto changedTree = p6ReqTree(root);
            disposableNeutral = WorkerTaskServices::postflightActivationOperation(restarted, plan).value("completionState") == "VERIFIED"
                && p6ReqTree(root) == changedTree
                && p6ReqBytes(root + '/' + prefix + "/proof.json") == afterTree.value("file:" + prefix + "/proof.json");
            const bool restored = p6ReqWrite(disposablePath, original);
            disposableNeutral &= restored && p6ReqTree(root) == afterTree;
        }
        auto changedRecords = measuredRecords;
        auto captured = changedRecords.value(reference).toObject();
        const auto capturedBytes = QByteArray::fromBase64(captured.value("bytesBase64").toString().toLatin1()) + "\nchanged captured record\n";
        captured.insert("bytesBase64", QString::fromLatin1(capturedBytes.toBase64()));
        captured.insert("fingerprint", p6ReqDigest(capturedBytes));
        changedRecords.insert(reference, captured);
        auto changedProof = proof; changedProof.insert("records", changedRecords);
        auto changedContract = plan; changedContract.insert("softwareProof", changedProof); p6ReqChecksum(changedContract);
        capturedRejected = WorkerTaskServices::postflightActivationOperation(restarted, changedContract).value("status") == "FAIL"
            && p6ReqTree(root) == afterTree;
        auto persistedProof = QJsonDocument::fromJson(p6ReqBytes(root + '/' + prefix + "/proof.json")).object();
        auto embeddedContract = persistedProof.value("operationContract").toObject();
        embeddedContract.insert("softwareProof", changedProof); p6ReqChecksum(embeddedContract);
        persistedProof.insert("operationContract", embeddedContract);
        capturedRejected &= tamper(prefix + "/proof.json", QJsonDocument(persistedProof).toJson());
    }
    tampered &= disposableNeutral && capturedRejected;
    for (const auto& relative : tamperPaths) {
        const auto original = p6ReqBytes(root + '/' + relative);
        const auto document = QJsonDocument::fromJson(original);
        auto changed = original + " \n";
        if (document.isObject()) {
            auto object = document.object(); object.insert("fixtureTamperedCanonicalOutput", true);
            changed = QJsonDocument(object).toJson();
        }
        tampered &= tamper(relative, changed);
    }
    // Each missing input is temporarily held outside the observed project, so
    // the absence itself is tested independently of an added out-of-scope file.
    bool absent = true;
    for (const auto& relative : QStringList{prefix + "/receipt.json", "ARAMF_WORKER/memory/memory-consistency-validation.json",
                                         "ARAMF_WORKER/context/task-dag.json"}) {
        const QString path = root + '/' + relative, held = root + "-missing-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const bool moved = QFile::rename(path, held);
        const auto missingTree = p6ReqTree(root);
        const bool rejected = moved && WorkerTaskServices::postflightActivationOperation(restarted, plan).value("status") == "FAIL"
            && p6ReqTree(root) == missingTree;
        const bool returned = moved && QFile::rename(held, path);
        absent &= rejected && returned;
    }
    auto alteredConsistency = QJsonDocument::fromJson(p6ReqBytes(root + "/ARAMF_WORKER/memory/memory-consistency-validation.json")).object();
    alteredConsistency.insert("fixtureForgedSemanticResult", true);
    tampered &= tamper("ARAMF_WORKER/memory/memory-consistency-validation.json", QJsonDocument(alteredConsistency).toJson());
    for (const bool missingVersion : {false, true}) {
        auto legacySaved = afterModel;
        if (missingVersion) legacySaved.remove("schemaVersion"); else legacySaved.insert("schemaVersion", 1);
        tampered &= tamper(QDir(root).relativeFilePath(config), QJsonDocument(legacySaved).toJson());
    }
    auto advanced = afterModel; auto wrongLifecycle = afterLifecycle; auto active = wrongLifecycle.value("active").toObject(); active.insert("iteration", 2);
    wrongLifecycle.insert("active", active); advanced.insert("processVersion", wrongLifecycle);
    tampered &= tamper(QDir(root).relativeFilePath(config), QJsonDocument(advanced).toJson());
    expect(tampered && absent && p6ReqTree(root) == afterTree,
        "P6 activation postflight rejects independent receipt model output baseline and evidence tampering");
    const bool outside = tamper("unrelated-user.txt", "outside-scope changed bytes\n");
    expect(postflight.value("completionState") == "VERIFIED" && boundary && outside && p6ReqTree(root) == afterTree,
        "P6 activation postflight verifies canonical outputs and outside-scope preservation");
    QThread::msleep(1100);
    const auto repeated = WorkerTaskServices::postflightActivationOperation(restarted, plan);
    QThread::msleep(1100);
    const auto repeatedAgain = WorkerTaskServices::postflightActivationOperation(restarted, plan);
    expect(repeated.value("completionState") == "VERIFIED" && repeatedAgain.value("completionState") == "VERIFIED"
        && p6ReqTree(root) == afterTree && p6ActInitial(restarted), "P6 activation repeated delayed postflight performs no writes or lifecycle transition");
    const auto startedAt = QDateTime::fromString(receipt.value("startedAt").toString(), Qt::ISODateWithMs);
    const auto successfulExpiry = QDateTime::fromString(plan.value("expiresAt").toString(), Qt::ISODateWithMs);
    const bool within = startedAt.isValid() && successfulExpiry.isValid() && startedAt >= anchor && startedAt < successfulExpiry;
    const bool pastSuccessfulExpiry = within && waitPast(successfulExpiry, (successMargin + 2) * 1000);
    timing("historical-postflight", {{"startedAt", receipt.value("startedAt")}, {"expiresAt", plan.value("expiresAt")},
        {"startedWithinWindow", within}, {"pastExpiryBeforePostflight", pastSuccessfulExpiry}});
    expect(within && pastSuccessfulExpiry && WorkerTaskServices::postflightActivationOperation(restarted, plan).value("completionState") == "VERIFIED"
        && p6ReqTree(root) == afterTree, "P6 activation completed postflight verifies start before expiry");
    expect(saved && p6ActInitial(restarted) && receiptValid && postflight.value("completionState") == "VERIFIED",
        "P6 activation real requirements refresh and integration lineage reaches restart readback");
    expect(negativeBytes && replay && tampered && absent && outside && p6ReqTree(root) == afterTree,
        "P6 activation negative controls preserve original bytes and retained failures");
}
void p6ActivationFailureAssertions(P6ReqFixture& fixture, QJsonObject* assertions, bool priorNegativeControls)
{
    const auto expect = [&](bool pass, const QString& name) {
        assertions->insert(name, pass);
        if (!pass) QTextStream(stderr) << "P6 ACTIVATION FAILURE FIXTURE: " << name << Qt::endl;
    };
    auto& model = fixture.model; const QString root = fixture.directory.path(), config = model.projectFilePath();
    const auto failedRequirements = p6ReqTree(root); QString error;
    // The previous failed requirements receipt remains immutable. Its older
    // integration cannot authorize new work over the changed tree. Measure a
    // new software proof using this fixture's existing real CMake build.
    fixture.software = p6ReqMeasuredSoftware(model, "task-p6-activation-governance/v1");
    const auto evidence = WorkerTaskServices::checkEvidence(model, fixture.software);
    if (evidence.value("status") != "PASS" || WorkerTaskServices::postflight(model, fixture.software,
        evidence.value("evidence").toArray()).value("completionState") != "VERIFIED") return;
    fixture.revalidation = WorkerTaskServices::prepareCertificationOperation(model, p6ReqRevalidationIntent(), fixture.software);
    if (!p6ReqReady(fixture.revalidation) || WorkerTaskServices::applyCertificationOperation(model, fixture.revalidation).value("status") != "PASS"
        || !p6ActIntegrate(fixture, fixture.revalidation, &fixture.integration)) return;
    const QJsonObject requirementsIntent{{"kind", "p6-requirements/v1"}, {"baselineVersion", "P6-CODEBANK-REQ-1"},
        {"agentId", "fixture/p6-requirements"}, {"reason", "New canonical attempt retaining the original isolated failed operation"}};
    const auto requirements = WorkerTaskServices::prepareRequirementsOperation(model, requirementsIntent, fixture.integration);
    if (!p6ReqReady(requirements) || WorkerTaskServices::applyRequirementsOperation(model, requirements).value("status") != "PASS"
        || !ProjectPersistence().load(&model, config, &error)) return;
    const auto refreshed = WorkerTaskServices::prepareCertificationOperation(model, p6ReqRevalidationIntent(), requirements);
    QJsonObject integration;
    if (!p6ReqReady(refreshed) || WorkerTaskServices::applyCertificationOperation(model, refreshed).value("status") != "PASS"
        || !p6ActIntegrate(fixture, refreshed, &integration)) return;
    const auto plan = WorkerTaskServices::prepareActivationOperation(model, p6ActIntent(), integration);
    if (!p6ReqReady(plan)) { QTextStream(stderr) << QJsonDocument(plan).toJson(); return; }
    const auto beforeModel = ProjectPersistence().toJson(model);
    const auto beforeTree = p6ReqTree(root);
    const QString eventPath = root + "/ARAMF_WORKER/memory/event-log.jsonl";
    QJsonObject applied; bool denial = false;
#ifdef Q_OS_WIN
    // Existing configuration regeneration reads history without appending it.
    // The later actual task-start append is denied by the OS, after the F4
    // transition has really been saved. This is not a production test hook.
    HANDLE eventHandle = CreateFileW(reinterpret_cast<LPCWSTR>(eventPath.utf16()), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    denial = eventHandle != INVALID_HANDLE_VALUE;
    if (denial) applied = WorkerTaskServices::applyActivationOperation(model, plan);
    if (denial) CloseHandle(eventHandle);
#else
    const auto permissions = QFile::permissions(eventPath);
    denial = QFile::setPermissions(eventPath, permissions & ~(QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::WriteGroup | QFileDevice::WriteOther));
    if (denial) applied = WorkerTaskServices::applyActivationOperation(model, plan);
    denial &= QFile::setPermissions(eventPath, permissions);
#endif
    const QString prefix = root + "/ARAMF_WORKER/verification/activation/" + plan.value("operationId").toString();
    const auto receipt = QJsonDocument::fromJson(p6ReqBytes(prefix + "/receipt.json")).object();
    const auto seal = QJsonDocument::fromJson(p6ReqBytes(prefix + "/seal.json")).object();
    ProjectModel restarted;
    const bool loaded = ProjectPersistence().load(&restarted, config, &error);
    bool retained = denial && loaded && p6ActInitial(restarted) && applied.value("status") != "PASS"
        && receipt.value("outcome") == "FAILED" && !receipt.value("error").toString().isEmpty()
        && p6ActStage(receipt, "start").value("success").toBool()
        && p6ActStage(receipt, "configuration").value("success").toBool()
        && p6ActStage(receipt, "configuration").value("saved").toBool()
        && !p6ActStage(receipt, "audit").isEmpty() && !p6ActStage(receipt, "audit").value("success").toBool()
        && receipt.value("observedLifecycle") == processVersionStateToJson(restarted.processVersionState())
        && !p6ReqBytes(prefix + "/proof.json").isEmpty()
        && seal.value("receiptFingerprint") == p6ReqDigest(p6ReqBytes(prefix + "/receipt.json"))
        && p6ReqBytes(eventPath) == beforeTree.value("file:ARAMF_WORKER/memory/event-log.jsonl")
        && ProjectPersistence().toJson(restarted).value("processVersion").toObject().value("completedHistory")
            == beforeModel.value("processVersion").toObject().value("completedHistory");
    for (auto it = failedRequirements.begin(); it != failedRequirements.end(); ++it)
        if (it.key().startsWith("file:ARAMF_WORKER/verification/requirements/"))
            retained &= p6ReqBytes(root + '/' + it.key().mid(5)) == it.value();
    expect(retained, "P6 activation partial saved failure retains honest immutable stage evidence");
    const auto retainedTree = p6ReqTree(root);
    const bool noReplay = loaded && WorkerTaskServices::postflightActivationOperation(restarted, plan).value("status") == "FAIL"
        && WorkerTaskServices::applyActivationOperation(restarted, plan).value("status") == "BLOCKED"
        && !p6ReqReady(WorkerTaskServices::prepareActivationOperation(restarted, p6ActIntent(), integration))
        && p6ReqTree(root) == retainedTree && p6ActInitial(restarted);
    expect(retained && noReplay, "P6 activation failed operation cannot report success or start twice");
    const QString preservation = "P6 activation negative controls preserve original bytes and retained failures";
    expect(priorNegativeControls && retained && noReplay, preservation);
    if (!retained || !noReplay) QTextStream(stderr) << QJsonDocument(applied).toJson() << QJsonDocument(receipt).toJson();
}
} // namespace

QJsonObject p6RequirementsOperationAssertions()
{
    QJsonObject assertions;
    // Missing fixture setup or an early failure must leave every owned assertion
    // false; this function never manufactures a PASS for an unexecuted stage.
    const auto criteria = WorkerTaskServices::acceptanceProfile("task-p6-requirements-operations/v1").value("criteria").toArray();
    for (int i = 2; i < 8; ++i)
        for (const auto& name : criteria.at(i).toObject().value("assertions").toArray()) assertions.insert(name.toString(), false);
    for (const auto& criterion : WorkerTaskServices::acceptanceProfile("task-p6-activation-governance/v1").value("criteria").toArray())
        for (const auto& name : criterion.toObject().value("assertions").toArray()) assertions.insert(name.toString(), false);
    const auto expect = [&](bool pass, const QString& name) {
        assertions.insert(name, pass);
        if (!pass) QTextStream(stderr) << "P6 REQUIREMENTS FIXTURE FAIL: " << name << Qt::endl;
        return pass;
    };
    P6ReqFixture fixture; QString error;
    if (!p6ReqInitialize(fixture, &error, "task-p6-activation-governance/v1")) return assertions;
    auto& model = fixture.model;
    const QString root = fixture.directory.path(), config = model.projectFilePath();
    ProjectPersistence persistence;
    const auto beforeModel = persistence.toJson(model);
    const auto originalDecisions = p6ReqBytes(root + "/ARAMF_WORKER/memory/decisions.md");
    const auto originalEvents = p6ReqBytes(root + "/ARAMF_WORKER/memory/event-log.jsonl");
    const auto originalLedger = p6ReqBytes(root + "/ARAMF_WORKER/certification/certificates.jsonl");
    const auto originalDag = p6ReqBytes(root + "/ARAMF_WORKER/context/task-dag.json");
    const auto originalDirty = p6ReqBytes(root + "/unrelated-user.txt");
    const QJsonObject intent{{"kind", "p6-requirements/v1"}, {"baselineVersion", "P6-CODEBANK-REQ-1"},
        {"agentId", "fixture/p6-requirements"}, {"reason", "Synthetic isolated requirements-operation regression; not product certification"}};
    const auto beforePrepare = p6ReqTree(root);
    const auto plan = WorkerTaskServices::prepareRequirementsOperation(model, intent, fixture.integration);
    expect(p6ReqReady(plan) && p6ReqTree(root) == beforePrepare, "P6 requirements preparation is read-only");
    if (!p6ReqReady(plan)) { QTextStream(stderr) << QJsonDocument(plan).toJson(); return assertions; }

    ProjectModel proposed;
    if (!persistence.fromJson(&proposed, plan.value("proposedModel").toObject(), &error)) return assertions;
    const auto effective = ConfigurationUpdateService::effectiveOutputFiles(proposed);
    const QString operationPrefix = "ARAMF_WORKER/verification/requirements/" + plan.value("operationId").toString();
    QJsonObject owners;
    for (const auto& path : effective) owners.insert(path, "ConfigurationUpdateService");
    owners.insert(QDir(root).relativeFilePath(config), "ProjectPersistence");
    for (const auto& file : QStringList{"decisions.md", "event-log.jsonl", "memory-manifest.json", "current-state.md",
            "metrics.json", "memory-consistency-validation.json", "cold-start-validation.json"})
        owners.insert("ARAMF_WORKER/memory/" + file, "ProjectMemory");
    for (const auto& file : QStringList{"context-index.json", "compressed-context.json", "freshness.json", "agent-adapters.json"})
        owners.insert("ARAMF_WORKER/context/" + file, "ContextCoordinationService");
    for (const auto& path : QStringList{"ARAMF_WORKER/verification/latest-validation.json",
            "ARAMF_WORKER/verification/verification-result.json", "ARAMF_WORKER/memory/memory-consistency-validation.json"})
        owners.insert(path, "VerificationServices");
    for (const auto& file : QStringList{"proof.json", "receipt.json", "seal.json"}) owners.insert(operationPrefix + '/' + file, "WorkerTaskServices");
    QJsonArray expectedPermissions;
    for (auto it = owners.begin(); it != owners.end(); ++it)
        expectedPermissions.append(QJsonObject{{"path", it.key()}, {"owner", it.value()},
            {"actions", QJsonArray{"READ", it.key().startsWith(operationPrefix + '/') ? "GENERATE_ONCE" : "UPDATE_DERIVED"}}});
    expect(plan.value("permittedFiles") == QJsonArray::fromStringList(owners.keys())
        && plan.value("filePermissions") == expectedPermissions && !owners.contains("CMakeLists.txt")
        && !owners.contains("src/core/WorkerTaskServices.cpp"), "P6 requirements plan derives exact producer-owned files");
    bool inactiveInventory = effective.contains("ARAMF_WORKER/verification/generation-state.json");
    for (const auto& path : effective) inactiveInventory &= !path.startsWith("ARAMF_WORKER/documentation/") && !QFileInfo(root + '/' + path).isDir();
    expect(inactiveInventory, "P6 requirements inventory excludes inactive academic directories");

    ProjectModel removed; persistence.fromJson(&removed, beforeModel, &error);
    auto removedRules = removed.ruleConfiguration(); removedRules.scopeMetadata.remove("tests"); removed.setRuleConfiguration(removedRules);
    const auto previewTree = p6ReqTree(root);
    // Static facade call: avoids constructing the QObject in the core-only target.
    const auto removal = ConfigurationUpdateService::preview(removed);
    expect(!removal.success && removal.removalBlocked && !removal.error.isEmpty()
        && !removal.plan.value("removalBlocked").toArray().isEmpty() && p6ReqTree(root) == previewTree,
        "P6 requirements preview rejects removals without writing update-plan");

    ProjectModel inactive; persistence.fromJson(&inactive, beforeModel, &error);
    auto inactiveRules = inactive.ruleConfiguration(); inactiveRules.projectScopes.removeAll("source-code"); inactive.setRuleConfiguration(inactiveRules);
    const auto inactiveResult = WorkerTaskServices::prepareRequirementsOperation(inactive, intent, fixture.integration);
    expect(!p6ReqReady(inactiveResult) && p6ReqHasCode(inactiveResult, "REQUIREMENTS_SCOPE_INVALID")
        && p6ReqTree(root) == beforePrepare, "P6 requirements rejects inactive scope owner");
    auto unrelatedIntent = intent; auto unrelatedModel = beforeModel; unrelatedModel.insert("description", "caller unrelated delta");
    unrelatedIntent.insert("proposedModel", unrelatedModel);
    auto unrelatedPlan = plan; auto proposedState = unrelatedPlan.value("proposedModel").toObject();
    proposedState.insert("description", "caller unrelated delta"); unrelatedPlan.insert("proposedModel", proposedState); p6ReqChecksum(unrelatedPlan);
    expect(p6ReqHasCode(WorkerTaskServices::prepareRequirementsOperation(model, unrelatedIntent, fixture.integration), "REQUIREMENTS_INTENT_INVALID")
        && WorkerTaskServices::applyRequirementsOperation(model, unrelatedPlan).value("status") == "BLOCKED"
        && p6ReqTree(root) == beforePrepare, "P6 requirements rejects unrelated configuration delta");

    auto wrongVersion = intent; wrongVersion.insert("baselineVersion", "P6-CODEBANK-REQ-0");
    auto partialIntent = intent; partialIntent.insert("criteria", QJsonArray{});
    auto incomplete = plan; auto incompleteRequirements = incomplete.value("requirements").toObject();
    auto incompleteCriteria = incompleteRequirements.value("criteria").toArray(); incompleteCriteria.removeLast();
    incompleteRequirements.insert("criteria", incompleteCriteria); incomplete.insert("requirements", incompleteRequirements);
    incomplete.insert("requirementsFingerprint", p6ReqDigest(QJsonDocument(incompleteRequirements).toJson(QJsonDocument::Compact))); p6ReqChecksum(incomplete);
    expect(p6ReqHasCode(WorkerTaskServices::prepareRequirementsOperation(model, wrongVersion, fixture.integration), "REQUIREMENTS_INTENT_INVALID")
        && p6ReqHasCode(WorkerTaskServices::prepareRequirementsOperation(model, partialIntent, fixture.integration), "REQUIREMENTS_INTENT_INVALID")
        && WorkerTaskServices::applyRequirementsOperation(model, incomplete).value("status") == "BLOCKED"
        && plan.value("requirements").toObject().value("criteria").toArray().size() == 12
        && p6ReqTree(root) == beforePrepare, "P6 requirements rejects incomplete baseline criteria");
    bool forgedPermissions = true;
    for (int mutation = 0; mutation < 3; ++mutation) {
        auto forged = plan;
        if (mutation == 0) { auto files = forged.value("permittedFiles").toArray(); files.append("CMakeLists.txt"); forged.insert("permittedFiles", files); }
        else {
            auto permissions = forged.value("filePermissions").toArray(); auto permission = permissions.first().toObject();
            if (mutation == 1) permission.insert("owner", "caller"); else permission.insert("actions", QJsonArray{"READ", "MODIFY"});
            permissions[0] = permission; forged.insert("filePermissions", permissions);
        }
        p6ReqChecksum(forged);
        forgedPermissions &= WorkerTaskServices::applyRequirementsOperation(model, forged).value("status") == "BLOCKED" && p6ReqTree(root) == beforePrepare;
    }
    expect(forgedPermissions, "P6 requirements rejects forged permissions");
    bool staleRejected = true;
    const QString sourcePath = root + "/src/core/WorkerTaskServices.cpp";
    for (const auto& path : QStringList{sourcePath, config}) {
        const auto original = p6ReqBytes(path);
        staleRejected &= p6ReqWrite(path, original + " \n"); const auto tamperedTree = p6ReqTree(root);
        staleRejected &= WorkerTaskServices::applyRequirementsOperation(model, plan).value("status") == "BLOCKED" && p6ReqTree(root) == tamperedTree;
        staleRejected &= p6ReqWrite(path, original);
    }
    expect(staleRejected && p6ReqTree(root) == beforePrepare, "P6 requirements rejects stale source or base before writes");
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + p6ReqDigest(QFileInfo(root).canonicalFilePath().toUtf8()) + ".lock");
    lock.setStaleLockTime(0); const bool locked = lock.tryLock(0);
    expect(locked && WorkerTaskServices::applyRequirementsOperation(model, plan).value("status") == "BLOCKED"
        && p6ReqTree(root) == beforePrepare, "P6 requirements lock rejects concurrent apply");
    if (locked) lock.unlock();

    const auto applied = WorkerTaskServices::applyRequirementsOperation(model, plan);
    if (applied.value("status") != "PASS" || applied.value("postflight").toObject().value("completionState") != "VERIFIED"
        || !persistence.load(&model, config, &error)) {
        QTextStream(stderr) << "Requirements apply failed: " << QJsonDocument(applied).toJson() << error; return assertions;
    }
    const auto afterModel = persistence.toJson(model);
    const auto baseline = model.ruleConfiguration().scopeMetadata.value("source-code").toObject().value("p6CodeBankRequirements").toObject();
    auto canonicalBaseline = baseline; canonicalBaseline.remove("provenance");
    bool baselineRouted = canonicalBaseline == WorkerTaskServices::p6RequirementsBaseline();
    bool routeFound = false;
    for (const auto& entry : WorkerContextResolver::resolve(root + "/ARAMF_WORKER", {"source-code"}).value("resolvedRoutes").toArray()) {
        const auto route = entry.toObject();
        if (route.value("id") == "source-code") routeFound |= route.value("taskMetadata").toObject().value("p6CodeBankRequirements") == baseline;
    }
    expect(baselineRouted && routeFound && baseline == plan.value("requirements").toObject(), "P6 requirements saved baseline matches source-code route");
    bool canonicalOutputs = afterModel == plan.value("proposedModel").toObject();
    const auto expectedArtifacts = GenerationServices::derivedTaskArtifacts(model, model.generationOptions());
    for (auto it = expectedArtifacts.begin(); it != expectedArtifacts.end(); ++it) {
        auto actual = QJsonDocument::fromJson(p6ReqBytes(root + '/' + it.key())).object(); auto expected = it.value().toObject();
        actual.remove("_file"); expected.remove("_file"); canonicalOutputs &= actual == expected;
    }
    const auto generationState = QJsonDocument::fromJson(p6ReqBytes(root + "/ARAMF_WORKER/verification/generation-state.json")).object();
    canonicalOutputs &= generationState.value("canonicalState") == ConfigurationUpdateService::canonicalState(model)
        && !generationState.value("configurationArtifactHashes").toObject().isEmpty()
        && VerificationServices().verify(model, model.generationOptions(), false).overallStatus == VerificationStatus::Pass;
    expect(canonicalOutputs, "P6 requirements configuration save reload and verification agree");
    const auto academic = afterModel.value("academic").toObject();
    expect(academic == beforeModel.value("academic").toObject() && afterModel.value("generationOptions") == beforeModel.value("generationOptions")
        && !academic.value("enabled").toBool() && !academic.value("thesisDocumentation").toObject().value("enabled").toBool()
        && !academic.value("reportDocumentation").toObject().value("enabled").toBool()
        && !QFileInfo::exists(root + "/ARAMF_WORKER/documentation/Thesis") && !QFileInfo::exists(root + "/ARAMF_WORKER/documentation/Report"),
        "P6 requirements disabled academic options remain unchanged");

    const auto eventBytes = p6ReqBytes(root + "/ARAMF_WORKER/memory/event-log.jsonl");
    const auto decisionBytes = p6ReqBytes(root + "/ARAMF_WORKER/memory/decisions.md");
    QJsonObject event; int newEvents = 0;
    for (const auto& line : eventBytes.mid(originalEvents.size()).split('\n')) if (!line.trimmed().isEmpty()) { ++newEvents; event = QJsonDocument::fromJson(line).object(); }
    const QString decisionId = "p6-code-bank-requirements-" + plan.value("operationId").toString();
    QJsonObject recordedDecision;
    const bool recorded = ProjectMemory().decisionById(root, decisionId, &recordedDecision, &error);
    const QJsonObject expectedProvenance{{"actor", "agent"}, {"agentId", intent.value("agentId")}, {"tool", "WorkerTaskServices/p6-requirements-operation/v1"}};
    expect(newEvents == 1 && event.value("eventType") == "DECISION_RECORDED" && event.value("decisionId") == decisionId
        && event.value("topic") == "Canonical P6 Code Bank requirements" && event.value("decisionStatus") == "current"
        && event.value("scope") == "source-code" && event.value("provenance") == expectedProvenance
        && recorded && recordedDecision.value("decisionId") == decisionId && recordedDecision.value("status") == "current"
        && recordedDecision.value("scope") == "source-code" && recordedDecision.value("summary") == event.value("task")
        && event.value("task").toString().contains(plan.value("requirementsFingerprint").toString()),
        "P6 requirements decision records actual agent provenance");
    QJsonObject originalDecision;
    expect(eventBytes.startsWith(originalEvents) && decisionBytes.startsWith(originalDecisions)
        && ProjectMemory().decisionById(root, "fixture-original-decision", &originalDecision, &error)
        && originalDecision.value("summary") == "Keep the original isolated decision and event prefix.",
        "P6 requirements preserves original decisions and event prefix");
    auto onlyExpectedDelta = afterModel; auto afterRules = onlyExpectedDelta.value("rules").toObject();
    auto metadata = afterRules.value("scopeMetadata").toObject(); auto sourceMetadata = metadata.value("source-code").toObject();
    sourceMetadata.remove("p6CodeBankRequirements"); metadata.insert("source-code", sourceMetadata); afterRules.insert("scopeMetadata", metadata); onlyExpectedDelta.insert("rules", afterRules);
    expect(onlyExpectedDelta == beforeModel && afterModel.value("processVersion") == beforeModel.value("processVersion")
        && p6ReqBytes(root + "/ARAMF_WORKER/certification/certificates.jsonl") == originalLedger
        && p6ReqBytes(root + "/ARAMF_WORKER/context/task-dag.json") == originalDag
        && p6ReqBytes(root + "/unrelated-user.txt") == originalDirty, "P6 requirements preserves unrelated dirty files and lifecycle");

    const auto completedTree = p6ReqTree(root);
    expect(WorkerTaskServices::applyRequirementsOperation(model, plan).value("status") == "BLOCKED"
        && !p6ReqReady(WorkerTaskServices::prepareRequirementsOperation(model, intent, fixture.integration))
        && p6ReqTree(root) == completedTree, "P6 requirements replay preserves original artifacts");
    QThread::msleep(1100);
    const auto firstPostflight = WorkerTaskServices::postflightRequirementsOperation(model, plan);
    QThread::msleep(1100);
    const auto secondPostflight = WorkerTaskServices::postflightRequirementsOperation(model, plan);
    expect(firstPostflight.value("completionState") == "VERIFIED" && secondPostflight.value("completionState") == "VERIFIED"
        && p6ReqTree(root) == completedTree, "P6 requirements repeated postflight is read-only");
    const auto rejectsTamper = [&](const QString& relative, const QByteArray& changed) {
        const QString path = root + '/' + relative; const auto original = p6ReqBytes(path);
        if (!p6ReqWrite(path, changed)) return false;
        const auto tamperedTree = p6ReqTree(root);
        const auto result = WorkerTaskServices::postflightRequirementsOperation(model, plan);
        const bool rejected = result.value("status") == "FAIL" && p6ReqTree(root) == tamperedTree;
        return p6ReqWrite(path, original) && rejected;
    };
    bool receiptAndBaseline = rejectsTamper(operationPrefix + "/receipt.json", p6ReqBytes(root + '/' + operationPrefix + "/receipt.json") + " ");
    auto changedModel = afterModel; auto changedRules = changedModel.value("rules").toObject(); auto changedMetadata = changedRules.value("scopeMetadata").toObject();
    auto changedSource = changedMetadata.value("source-code").toObject(); auto changedBaseline = changedSource.value("p6CodeBankRequirements").toObject();
    changedBaseline.insert("criteria", QJsonArray{}); changedSource.insert("p6CodeBankRequirements", changedBaseline);
    changedMetadata.insert("source-code", changedSource); changedRules.insert("scopeMetadata", changedMetadata); changedModel.insert("rules", changedRules);
    receiptAndBaseline &= rejectsTamper(QDir(root).relativeFilePath(config), QJsonDocument(changedModel).toJson());
    expect(receiptAndBaseline && p6ReqTree(root) == completedTree, "P6 requirements postflight rejects changed receipt or baseline");
    bool rejectsOutputs = true;
    for (const auto& relative : QStringList{"ARAMF_WORKER/project.json", "ARAMF_WORKER/routing/scope-routes.json",
            "ARAMF_WORKER/verification/generation-state.json", "ARAMF_WORKER/context/context-index.json",
            "ARAMF_WORKER/context/compressed-context.json", "ARAMF_WORKER/context/freshness.json", "ARAMF_WORKER/context/agent-adapters.json",
            "ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json",
            "ARAMF_WORKER/memory/memory-consistency-validation.json"}) {
        auto value = QJsonDocument::fromJson(p6ReqBytes(root + '/' + relative)).object(); value.insert("fixtureTampered", true);
        rejectsOutputs &= rejectsTamper(relative, QJsonDocument(value).toJson());
    }
    expect(rejectsOutputs && p6ReqTree(root) == completedTree, "P6 requirements postflight rejects changed canonical outputs");

    const auto sourceBytes = p6ReqBytes(sourcePath); p6ReqWrite(sourcePath, sourceBytes + "\n// changed covered prerequisite\n");
    const auto sourceChangedTree = p6ReqTree(root);
    bool badLineage = !p6ReqReady(WorkerTaskServices::prepareCertificationOperation(model, p6ReqRevalidationIntent(), plan)) && p6ReqTree(root) == sourceChangedTree;
    badLineage &= p6ReqWrite(sourcePath, sourceBytes);
    auto alteredProof = plan; auto frozenProof = alteredProof.value("softwareProof").toObject(); auto records = frozenProof.value("records").toObject();
    if (records.isEmpty()) badLineage = false;
    else {
        const auto key = records.keys().first(); auto record = records.value(key).toObject();
        const auto original = QByteArray::fromBase64(record.value("bytesBase64").toString().toLatin1());
        record.insert("bytesBase64", QString::fromLatin1((original + " ").toBase64())); records.insert(key, record);
        frozenProof.insert("records", records); alteredProof.insert("softwareProof", frozenProof); p6ReqChecksum(alteredProof);
        badLineage &= !p6ReqReady(WorkerTaskServices::prepareCertificationOperation(model, p6ReqRevalidationIntent(), alteredProof)) && p6ReqTree(root) == completedTree;
    }
    for (const auto& key : QStringList{"projectId", "projectPath", "workerNameSuffix", "projectName"}) {
        auto alteredModelPlan = plan; auto proof = plan.value("softwareProof").toObject(); auto frozenModel = proof.value("softwareModel").toObject();
        if (frozenModel.isEmpty()) { badLineage = false; continue; }
        frozenModel.insert(key, key == "projectPath" ? root + "/foreign" : "altered-frozen-configuration");
        proof.insert("softwareModel", frozenModel); alteredModelPlan.insert("softwareProof", proof); p6ReqChecksum(alteredModelPlan);
        badLineage &= !p6ReqReady(WorkerTaskServices::prepareCertificationOperation(model, p6ReqRevalidationIntent(), alteredModelPlan)) && p6ReqTree(root) == completedTree;
    }
    expect(badLineage, "P6 requirements lineage rejects changed source or altered original proof");
    const auto refreshPlan = WorkerTaskServices::prepareCertificationOperation(model, p6ReqRevalidationIntent(), plan);
    const bool refreshReady = p6ReqReady(refreshPlan) && refreshPlan.value("softwareProof") == plan.value("softwareProof")
        && refreshPlan.value("sourceBindings") == plan.value("sourceBindings") && p6ReqTree(root) == completedTree;
    const auto refreshed = refreshReady ? WorkerTaskServices::applyCertificationOperation(model, refreshPlan) : QJsonObject{};
    expect(refreshReady && refreshed.value("status") == "PASS"
        && refreshed.value("postflight").toObject().value("completionState") == "VERIFIED"
        && p6ReqBytes(root + "/ARAMF_WORKER/certification/certificates.jsonl") == originalLedger
        && p6ReqBytes(root + '/' + operationPrefix + "/proof.json") == completedTree.value("file:" + operationPrefix + "/proof.json")
        && p6ReqBytes(root + '/' + operationPrefix + "/receipt.json") == completedTree.value("file:" + operationPrefix + "/receipt.json"),
        "P6 requirements verified lineage permits exact prerequisite refresh");
    if (refreshReady && refreshed.value("status") == "PASS") p6ActivationAssertions(fixture, refreshPlan, &assertions);

    // Separate fixture: a real replacement denial after immutable proof creation
    // must retain failed stage receipts, never relabel the failed operation PASS.
    P6ReqFixture failedFixture;
    bool retainedFailure = p6ReqInitialize(failedFixture, &error, "task-p6-activation-governance/v1");
    if (retainedFailure) {
        auto& failureModel = failedFixture.model; const QString failureRoot = failedFixture.directory.path();
        const auto failurePlan = WorkerTaskServices::prepareRequirementsOperation(failureModel, intent, failedFixture.integration);
        const QString failurePrefix = failureRoot + "/ARAMF_WORKER/verification/requirements/" + failurePlan.value("operationId").toString();
        const auto configBefore = p6ReqBytes(failureModel.projectFilePath());
        const auto decisionsBefore = p6ReqBytes(failureRoot + "/ARAMF_WORKER/memory/decisions.md");
        const auto eventsBefore = p6ReqBytes(failureRoot + "/ARAMF_WORKER/memory/event-log.jsonl");
        QJsonObject failed;
#ifdef Q_OS_WIN
        HANDLE lockedConfiguration = CreateFileW(reinterpret_cast<LPCWSTR>(failureModel.projectFilePath().utf16()),
            GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        retainedFailure &= p6ReqReady(failurePlan) && lockedConfiguration != INVALID_HANDLE_VALUE;
        if (retainedFailure) failed = WorkerTaskServices::applyRequirementsOperation(failureModel, failurePlan);
        if (lockedConfiguration != INVALID_HANDLE_VALUE) CloseHandle(lockedConfiguration);
#else
        // Restrict only the fixture root: Worker subdirectories remain writable
        // for failure receipts, while atomic configuration replacement is denied.
        const auto permissions = QFile::permissions(failureRoot);
        retainedFailure &= p6ReqReady(failurePlan) && QFile::setPermissions(failureRoot,
            permissions & ~(QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::WriteGroup | QFileDevice::WriteOther));
        if (retainedFailure) failed = WorkerTaskServices::applyRequirementsOperation(failureModel, failurePlan);
        retainedFailure &= QFile::setPermissions(failureRoot, permissions);
#endif
        const auto receipt = QJsonDocument::fromJson(p6ReqBytes(failurePrefix + "/receipt.json")).object();
        const auto seal = QJsonDocument::fromJson(p6ReqBytes(failurePrefix + "/seal.json")).object();
        const auto stages = receipt.value("stages").toArray();
        retainedFailure &= failed.value("status") == "FAIL" && receipt.value("outcome") == "FAILED"
            && !receipt.value("error").toString().isEmpty() && stages.size() == 1
            && stages.first().toObject().value("stage") == "configuration" && !stages.first().toObject().value("success").toBool()
            && stages.first().toObject().value("rolledBack").toBool()
            && !p6ReqBytes(failurePrefix + "/proof.json").isEmpty()
            && seal.value("receiptFingerprint") == p6ReqDigest(p6ReqBytes(failurePrefix + "/receipt.json"))
            && p6ReqBytes(failureModel.projectFilePath()) == configBefore
            && p6ReqBytes(failureRoot + "/ARAMF_WORKER/memory/decisions.md") == decisionsBefore
            && p6ReqBytes(failureRoot + "/ARAMF_WORKER/memory/event-log.jsonl") == eventsBefore;
        const auto failedTree = p6ReqTree(failureRoot);
        retainedFailure &= WorkerTaskServices::postflightRequirementsOperation(failureModel, failurePlan).value("status") == "FAIL"
            && WorkerTaskServices::applyRequirementsOperation(failureModel, failurePlan).value("status") == "BLOCKED"
            && p6ReqTree(failureRoot) == failedTree;
        if (!retainedFailure) QTextStream(stderr) << QJsonDocument(failed).toJson() << QJsonDocument(receipt).toJson();
    }
    expect(retainedFailure, "P6 requirements partial failure remains failed with retained stage evidence");
    const QString negativeControls = "P6 activation negative controls preserve original bytes and retained failures";
    const bool priorNegativeControls = assertions.value(negativeControls).toBool();
    assertions.insert(negativeControls, false);
    if (retainedFailure) p6ActivationFailureAssertions(failedFixture, &assertions, priorNegativeControls);
    return assertions;
}

QJsonObject repairSuccessorAssertions(bool failFinalReceipt, const QString& profileId = "task-p6-activation-campaign-isolation/v1")
{
    QJsonObject results;
    const auto expect = [&](bool ok, const QString& name) { results.insert(name, ok && results.value(name).toBool(true)); return ok; };
    const QString instruction = "Admin Morgan Lindbom confirms the explicit, temporary authority already granted to restore ARAMF. "
        "Temporary bootstrap repair authority requires exact purpose and file scope. Preserve all existing work, contracts, baselines, reports, and evidence; "
        "do not discard or rewrite history. This does not suspend ARAMF rules. Return to the ordinary process: READY TaskContracts, exact scope, current evidence, required validation and PASS postflight.";
    expect(IdentityTrustFoundation::isExplicitTemporaryGovernanceRepairAuthority(instruction)
        && !IdentityTrustFoundation::isVerifiedAdministrativeOverride(instruction)
        && IdentityTrustFoundation::isVerifiedAdministrativeOverride("Admin Morgan Lindbom override")
        && IdentityTrustFoundation::contract().value("dependsOn").toArray().isEmpty(), "repair authority is owned by independent F2 with unchanged legacy override");
    bool rejectedIntent = true;
    for (const auto& text : QStringList{"Admin Morgan Lindbom", "Example: " + instruction,
        QString(instruction).replace("Admin Morgan Lindbom", "Admin Someone Else"), instruction + " Skip required validation.",
        instruction + " No current evidence is required.", instruction + " Do not preserve all existing work.", instruction + " rm -rf fixture"})
        rejectedIntent &= !IdentityTrustFoundation::isExplicitTemporaryGovernanceRepairAuthority(text);
    expect(rejectedIntent, "repair authority rejects missing negated foreign and unsafe intent");
    const bool v2 = profileId == "task-p6-activation-campaign-isolation/v2";
    const auto profile = WorkerTaskServices::acceptanceProfile(profileId), activation = WorkerTaskServices::acceptanceProfile("task-p6-activation-governance/v1");
    QStringList files, requirements;
    for (const auto& file : profile.value("files").toArray()) files << file.toString();
    for (const auto& value : profile.value("requirements").toArray()) requirements << value.toString();
    bool complete = files.size() == (v2 ? 9 : 8) && !files.contains("test_update/campaign.json") && !files.contains("CMakeLists.txt");
    for (const auto& criterion : activation.value("criteria").toArray()) complete &= profile.value("criteria").toArray().contains(criterion);
    bool freshness = false;
    for (const auto& criterion : profile.value("criteria").toArray()) freshness |= criterion.toObject().value("id") == "P1-SOURCE-01" && criterion.toObject().value("assertions").toArray().size() == 5;
    expect(complete && freshness && profile.value("checks").toArray().contains("aramf_update_campaign"), "combined repair profile retains activation and freshness criteria and exact scope");
    QTemporaryDir fixture, external; fixture.setAutoRemove(false); external.setAutoRemove(false);
    const QString root = fixture.path(); std::cerr << "Repair successor fixture: " << root.toStdString() << '\n';
    ProjectModel model; model.setProjectPath(root); model.setProjectId("isolated-repair-successor"); model.setProjectFilePath(root + "/fixture.aramf.json");
    auto ai = model.aiConfiguration(); ai.permissions = {"read-project-files", "modify-files", "create-files", "run-builds", "run-tests", "run-project-tools"}; model.setAiConfiguration(ai);
    auto memory = model.memoryConfiguration(); memory.maintenanceOptions = {"update-current-state", "record-test-results"};
    memory.validationOptions = {"memory-consistency", "cold-start-validation"}; model.setMemoryConfiguration(memory);
    QSet<QString> reads;
    for (const auto& subject : profile.value("prerequisiteSubjects").toArray())
        for (const auto& path : CertificationSourceManifestProvider::manifest(subject.toString()).files) reads.insert(path);
    for (const auto& path : files) reads.insert(path);
    bool setup = true;
    for (const auto& path : reads) setup &= write(root + '/' + path, "// synthetic isolated source authority input\n");
    setup &= write(root + "/test_update/campaign.json", "{\"historicalFixture\":true}\n") && write(root + "/unrelated.txt", "preserved\n") && write(root + "/.gitignore", "build/\n");
    auto rules = model.ruleConfiguration(); rules.projectScopes = {"source-code", "tests"};
    rules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray::fromStringList(files)}, {"tests", QJsonArray{"aramf_core_tests::--worker-tasks"}}}},
        {"tests", QJsonObject{{"files", QJsonArray{}}, {"tests", QJsonArray{"aramf_core_tests::--worker-tasks"}}}}}; model.setRuleConfiguration(rules);
    QString error;
    setup &= ProjectPersistence().save(model, model.projectFilePath(), &error) && GenerationServices().generate(model, model.generationOptions()).success;
    const auto run = [&](const QStringList& args) { QProcess process; process.start("git", QStringList{"-C", root} + args); return process.waitForFinished(30000) && process.exitCode() == 0; };
    setup &= run({"init", "-q"}) && run({"add", "."}) && run({"-c", "user.name=Repair Fixture", "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false", "commit", "-qm", "isolated repair authority"});
    const auto refresh = [&](bool persist = false) {
        QString e;
        return ProjectMemory().refreshDerivedState(root, &e) && e.isEmpty()
            && ProjectMemory().validateColdStart(root, &e).value("status") == "PASS" && e.isEmpty()
            && ProjectMemory().validate(root, &e, true).value("status") == "PASS" && e.isEmpty()
            && ContextCoordinationService::generate(model).value("success").toBool()
            && VerificationServices().verify(model, model.generationOptions(), persist).overallStatus == VerificationStatus::Pass;
    };
    setup &= refresh(true) && refresh();
    WorkerTaskRequest source; source.goal = "Isolated exact source implementation authority"; source.type = "implementation"; source.scopes = {"source-code", "tests"}; source.files = files; source.definitionOfDone = {"Preserve isolated source boundary"}; source.history = true;
    const auto sourceAuthority = WorkerTaskServices::prepare(model, source);
    WorkerTaskRequest old = source; old.goal = "Original isolated failed task"; old.files = {"tests/WorkerTaskTests.cpp"};
    auto origin = WorkerTaskServices::prepare(model, old);
    setup &= ready(sourceAuthority) && ready(origin);
    const auto originalObservation = setup ? WorkerTaskServices::runCheck(model, origin, "memory-consistency") : QJsonObject{};
    setup &= originalObservation.value("status") == "PASS";
    setup &= write(root + "/test_update/campaign.json", "{\"actualOutOfScopeFixtureChange\":true}\n");
    auto failed = WorkerTaskServices::postflight(model, origin);
    setup &= failed.value("status") == "FAIL" && hasCode(failed, "TASK_SCOPE_VIOLATION");
    if (!setup) { results.insert("repair successor fixture setup: " + error, false); std::cerr << QJsonDocument(originalObservation).toJson().toStdString() << QJsonDocument(sourceAuthority.value("preflight").toObject()).toJson().toStdString() << QJsonDocument(origin.value("preflight").toObject()).toJson().toStdString() << QJsonDocument(failed).toJson().toStdString(); return results; }
    auto originalBytes = QJsonDocument(origin).toJson(), failedBytes = QJsonDocument(failed).toJson();
    const auto originalRecord = originalObservation.value("evidence").toArray().isEmpty() ? QJsonObject{} : originalObservation.value("evidence").toArray().first().toObject();
    WorkerTaskRequest successorTask = source; successorTask.goal = "Verify exact linked successor with fresh measured observations"; successorTask.acceptanceProfile = profileId; successorTask.definitionOfDone = requirements;
    QJsonObject request{{"task", successorTask.toJson()}, {"sourceAuthorities", QJsonArray{sourceAuthority}}, {"origins", QJsonArray{QJsonObject{{"contract", origin}, {"report", failed}}}},
        {"instruction", instruction}, {"recordingAgentId", "isolated-repair-agent"}, {"validityMinutes", 60}};
    int call = 0;
    const auto approve = [&](const QJsonObject& input) {
        const QString path = external.path() + "/request-" + QString::number(++call) + ".json";
        write(path, QJsonDocument(input).toJson()); QString output, errors; QTextStream out(&output), err(&errors);
        runWorkerTaskCommand({"task", "reauthorize-repair", "--config", model.projectFilePath(), "--request", path}, out, err);
        auto result = QJsonDocument::fromJson(output.toUtf8()).object(); if (result.isEmpty()) result.insert("commandError", errors); return result;
    };
    if (v2) {
        const auto v1 = WorkerTaskServices::acceptanceProfile("task-p6-activation-campaign-isolation/v1");
        const auto v1Digest = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(v1).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
        bool unchanged = v1Digest == "3a7ead567fd45e9b9c81becd0008446db38e039d5834bfa7567f31f57b8e6cb9";
        for (const auto& criterion : v1.value("criteria").toArray()) unchanged &= profile.value("criteria").toArray().contains(criterion);
        expect(unchanged && files.size() == 9 && files.contains("tests/FoundationCertificationTests.cpp")
            && !v1.value("files").toArray().contains("tests/FoundationCertificationTests.cpp"),
            "v2 repair profile preserves v1 and binds exact nine source files");
        // A real isolated v1 successor becomes the failed origin of v2. Its
        // original authority, baseline, report and evidence are never relabeled.
        auto v1Source = source; v1Source.files.clear();
        for (const auto& file : v1.value("files").toArray()) v1Source.files << file.toString();
        const auto v1Authority = WorkerTaskServices::prepare(model, v1Source);
        auto v1Task = v1Source; v1Task.acceptanceProfile = v1.value("id").toString(); v1Task.definitionOfDone.clear();
        for (const auto& value : v1.value("requirements").toArray()) v1Task.definitionOfDone << value.toString();
        auto firstRequest = request; firstRequest.insert("task", v1Task.toJson()); firstRequest.insert("sourceAuthorities", QJsonArray{v1Authority});
        const auto firstGrant = approve(firstRequest);
        const auto firstSuccessor = firstGrant.value("status") == "PASS"
            ? WorkerTaskServices::prepareReauthorized(model, firstGrant.value("authorizationId").toString(), origin, failed) : QJsonObject{};
        const QString changedPath = root + "/tests/FoundationCertificationTests.cpp";
        const bool changed = ready(firstSuccessor) && write(changedPath, bytes(changedPath) + "// necessary isolated v2 fixture correction\n");
        const auto firstFailure = changed ? WorkerTaskServices::postflight(model, firstSuccessor) : QJsonObject{};
        if (!ready(v1Authority) || !changed || firstFailure.value("status") != "FAIL" || !hasCode(firstFailure, "TASK_SCOPE_VIOLATION")) {
            results.insert("v2 authentic failed successor setup", false);
            std::cerr << QJsonDocument(firstGrant).toJson().toStdString() << QJsonDocument(firstFailure).toJson().toStdString(); return results;
        }
        origin = firstSuccessor; failed = firstFailure;
        originalBytes = QJsonDocument(origin).toJson(); failedBytes = QJsonDocument(failed).toJson();
        request.insert("origins", QJsonArray{QJsonObject{{"contract", origin}, {"report", failed}}});
    }
    if (failFinalReceipt) {
        // An isolated occupied storage parent makes the real final receipt
        // commit fail after both real canonical audit events have been written.
        const bool occupied = write(root + "/build/aramf-repair-authority", "retained fixture storage obstruction\n");
        const auto attempt = approve(request); QJsonObject partialGrant; QString partialId; int issued = 0, validated = 0;
        for (const auto& event : ProjectMemory().events(root)) {
            if (event.value("eventType") == "TASK_REPAIR_SCOPE_AUTHORIZED") { ++issued; partialGrant = event.value("taskReauthorizationGrant").toObject(); partialId = partialGrant.value("authorizationId").toString(); }
            if (event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED") ++validated;
        }
        const auto retainedHistory = bytes(root + "/ARAMF_WORKER/memory/event-log.jsonl");
        const bool refreshed = refresh(); const auto retainedTree = treeBytes(root);
        const auto blocked = WorkerTaskServices::prepareReauthorized(model, partialId, origin, failed);
        expect(occupied && attempt.value("status") == "BLOCKED" && issued == 1 && validated == 1 && !partialId.isEmpty()
            && !QFileInfo::exists(root + '/' + partialGrant.value("issuanceCompletionPath").toString()) && refreshed
            && !ready(blocked) && treeBytes(root) == retainedTree && bytes(root + "/ARAMF_WORKER/memory/event-log.jsonl") == retainedHistory,
            "repair successor retains incomplete authority as inactive");
        return results;
    }
    const auto beforeDenied = treeBytes(root); bool denied = true;
    QList<QJsonObject> invalid;
    auto bad = request; bad.insert("permissions", QJsonArray{"*"}); invalid << bad;
    bad = request; bad.insert("sourceAuthorities", QJsonArray{}); invalid << bad;
    bad = request; bad.insert("origins", QJsonArray{}); invalid << bad;
    bad = request; bad.insert("instruction", instruction + " Skip required validation."); invalid << bad;
    for (const auto& key : QStringList{"type", "scopes", "history"}) {
        auto wrongTask = successorTask.toJson(); wrongTask.insert(key, key == "scopes" ? QJsonValue(QJsonArray{"source-code"}) : key == "history" ? QJsonValue(false) : QJsonValue("planning"));
        bad = request; bad.insert("task", wrongTask); invalid << bad;
    }
    auto changedReport = failed; changedReport.insert("status", "PASS"); bad = request; bad.insert("origins", QJsonArray{QJsonObject{{"contract", origin}, {"report", changedReport}}}); invalid << bad;
    auto widened = sourceAuthority; widened.insert("permittedFiles", QJsonArray{"unrelated.txt"}); checksumFixtureContract(widened); bad = request; bad.insert("sourceAuthorities", QJsonArray{widened}); invalid << bad;
    for (const auto& otherProfile : QStringList{"task-p6-activation-campaign-isolation/v1", "task-p6-activation-campaign-isolation/v2", "task-p6-activation-campaign-isolation/v3"}) {
        if (otherProfile == profileId) continue;
        auto wrongTask = successorTask.toJson(); wrongTask.insert("acceptanceProfile", otherProfile);
        bad = request; bad.insert("task", wrongTask); invalid << bad;
    }
    for (const bool omit : {true, false}) {
        auto wrongTask = successorTask.toJson(); auto wrongFiles = wrongTask.value("files").toArray();
        if (omit) wrongFiles.removeLast(); else wrongFiles.append("unrelated.txt");
        wrongTask.insert("files", wrongFiles); bad = request; bad.insert("task", wrongTask); invalid << bad;
    }
    for (const auto& value : invalid) denied &= approve(value).value("status") == "BLOCKED" && treeBytes(root) == beforeDenied;
    expect(denied, "repair successor rejects forged omitted widened or changed origins");
    const auto authorized = approve(request);
    const QString id = authorized.value("authorizationId").toString();
    if (authorized.value("status") != "PASS") { results.insert("repair successor actual issuance: " + authorized.value("error").toString(authorized.value("commandError").toString()), false); return results; }
    QJsonObject authority, validation;
    for (const auto& event : ProjectMemory().events(root)) {
        if (event.value("taskReauthorizationGrant").toObject().value("authorizationId") == id) authority = event;
        if (event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED" && event.value("authorizationId") == id) validation = event;
    }
    const auto grant = authority.value("taskReauthorizationGrant").toObject();
    const QString completionPath = root + '/' + grant.value("issuanceCompletionPath").toString();
    const auto completionBytes = bytes(completionPath); const auto completion = QJsonDocument::fromJson(completionBytes).object();
    expect(authority.value("eventType") == "TASK_REPAIR_SCOPE_AUTHORIZED" && authority.value("provenance").toObject().value("actor") == "agent"
        && authority.value("provenance").toObject().value("agentId") == "isolated-repair-agent" && authority.value("instruction") == instruction
        && validation.value("postMemoryValidation").toObject().value("status") == "PASS"
        && authorized.value("issuanceCommitted").toBool() && completion.value("status") == "ISSUANCE_COMPLETED"
        && completion.value("authorityEventId") == authority.value("eventId") && completion.value("validationEventId") == validation.value("eventId"),
        "repair successor records actual agent provenance without new human override");
    expect(grant.value("origins").toArray().first().toObject().value("contract") == origin && grant.value("origins").toArray().first().toObject().value("report") == failed
        && grant.value("sourceAuthorities").toArray() == QJsonArray{sourceAuthority}, "repair successor binds original READY sources and reproduced FAIL origins");
    const auto successor = WorkerTaskServices::prepareReauthorized(model, id, origin, failed);
    if (!ready(successor)) { results.insert("repair successor actual preparation", false); std::cerr << QJsonDocument(successor).toJson().toStdString(); return results; }
    expect(successor.value("permittedFiles") == sourceAuthority.value("permittedFiles") && !successor.value("permittedFiles").toArray().contains("test_update/campaign.json")
        && grant.value("canonicalInputs").toArray().isEmpty() && grant.value("operations").toObject().value("test_update/campaign.json") == "PRESERVE",
        "repair successor retains campaign and canonical observations without write expansion");
    if (v2) expect(denied && successor.value("request").toObject().value("acceptanceProfile") == profileId
        && successor.value("permittedFiles").toArray().size() == 9 && !origin.value("reauthorization").toObject().isEmpty()
        && grant.value("origins").toArray().first().toObject().value("contract") == origin
        && grant.value("origins").toArray().first().toObject().value("report") == failed,
        "v2 repair successor enforces selected profile scope and real failed successor lineage");
    const auto controlTree = treeBytes(root); const auto control = WorkerTaskServices::postflight(model, successor);
    expect(control.value("status") == "PASS" && WorkerTaskServices::postflight(model, successor) == control && treeBytes(root) == controlTree
        && QJsonDocument(origin).toJson() == originalBytes && QJsonDocument(failed).toJson() == failedBytes,
        "repair successor postflight is pure and permits actual canonical refresh");
    auto legacy = successorTask; legacy.acceptanceProfile.clear();
    const auto legacyContract = WorkerTaskServices::prepare(model, legacy);
    const auto mismatched = WorkerTaskServices::postflight(model, successor, QJsonArray{originalRecord});
    expect(WorkerTaskServices::runCheck(model, legacyContract, "acceptance-criteria").value("status") == "BLOCKED"
        && !originalRecord.isEmpty() && mismatched.value("acceptedEvidence").toArray().isEmpty() && mismatched.value("completionState") != "VERIFIED",
        "combined repair evidence cannot inherit old profile or contract evidence");
    const auto afterControl = treeBytes(root); bool preservedDenial = true;
    for (const auto& path : QStringList{"test_update/campaign.json", "unrelated.txt", "ARAMF_WORKER/memory/event-log.jsonl", grant.value("issuanceCompletionPath").toString()}) {
        const auto prior = bytes(root + '/' + path); write(root + '/' + path, "independent tampered fixture bytes\n");
        preservedDenial &= WorkerTaskServices::postflight(model, successor).value("status") == "FAIL";
        write(root + '/' + path, prior);
    }
    expect(preservedDenial && treeBytes(root) == afterControl && WorkerTaskServices::postflight(model, successor).value("status") == "PASS",
        "repair successor rejects retained bytes history and undeclared changes");
    const auto append = [&](const QString& type, QJsonObject fields) {
        for (const auto& key : {"eventId", "sequenceNumber", "timestamp", "eventType", "task"}) fields.remove(key);
        QString e; if (!ProjectMemory().appendEvent(root, type, "explicit synthetic negative repair authority fixture", fields, &e)) return QJsonObject{};
        return ProjectMemory().events(root).last();
    };
    QJsonObject originalBound;
    for (const auto& event : ProjectMemory().events(root))
        if (event.value("eventType") == "TASK_REAUTHORIZATION_BOUND" && event.value("authorizationId") == id) originalBound = event;
    // This entirely synthetic cloned chain first passes, then each single-field
    // corruption is checked independently. Only this isolated fixture rewrites
    // its synthetic suffix; the production recorder never rewrites history.
    const auto grantHash = [](const QJsonObject& value) {
        return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(canonicalValue(value).toObject()).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    };
    const QString cloneId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto cloneGrant = grant; cloneGrant.insert("authorizationId", cloneId);
    cloneGrant.insert("issuanceCompletionPath", "build/aramf-repair-authority/" + cloneId + "/issuance.json");
    auto cloneAuthority = authority; cloneAuthority.insert("taskReauthorizationGrant", cloneGrant);
    cloneAuthority = append("TASK_REPAIR_SCOPE_AUTHORIZED", cloneAuthority);
    auto cloneValidation = validation; cloneValidation.insert("authorizationId", cloneId); cloneValidation.insert("authorityEventId", cloneAuthority.value("eventId"));
    cloneValidation.insert("grantFingerprint", grantHash(cloneGrant)); cloneValidation = append("TASK_REAUTHORIZATION_AUTHORIZED", cloneValidation);
    auto cloneCompletion = completion; cloneCompletion.insert("authorizationId", cloneId); cloneCompletion.insert("authorityEventId", cloneAuthority.value("eventId"));
    cloneCompletion.insert("validationEventId", cloneValidation.value("eventId")); cloneCompletion.insert("grantFingerprint", grantHash(cloneGrant)); cloneCompletion.insert("validationTimestamp", cloneValidation.value("timestamp"));
    const QString cloneCompletionPath = root + '/' + cloneGrant.value("issuanceCompletionPath").toString();
    const bool cloneReceiptWritten = write(cloneCompletionPath, QJsonDocument(cloneCompletion).toJson());
    auto cloneContract = successor; auto cloneLink = cloneContract.value("reauthorization").toObject(); cloneLink.insert("authorizationId", cloneId); cloneLink.insert("authorityEventId", cloneAuthority.value("eventId")); cloneContract.insert("reauthorization", cloneLink);
    auto cloneTask = cloneContract.value("request").toObject(); cloneTask.insert("reauthorizationId", cloneId); cloneContract.insert("request", cloneTask); checksumFixtureContract(cloneContract);
    auto cloneBound = originalBound; cloneBound.insert("authorizationId", cloneId); cloneBound.insert("authorityEventId", cloneAuthority.value("eventId")); cloneBound.insert("contractId", cloneContract.value("contractId")); cloneBound = append("TASK_REAUTHORIZATION_BOUND", cloneBound);
    const QString historyPath = root + "/ARAMF_WORKER/memory/event-log.jsonl";
    bool bindingLinks = !originalBound.isEmpty() && cloneReceiptWritten && refresh();
    bindingLinks &= WorkerTaskServices::postflight(model, cloneContract).value("status") == "PASS";
    const auto cloneHistory = bytes(historyPath);
    const auto cloneControlTree = treeBytes(root);
    const auto restoreCloneControl = [&]() {
        bool restored = true;
        for (auto it = cloneControlTree.begin(); it != cloneControlTree.end(); ++it)
            if (bytes(root + '/' + it.key()) != it.value()) restored &= write(root + '/' + it.key(), it.value());
        return restored && treeBytes(root) == cloneControlTree
            && WorkerTaskServices::postflight(model, cloneContract).value("status") == "PASS";
    };
    const auto replaceSyntheticEvent = [&](const QJsonObject& event) {
        auto lines = bytes(historyPath).split('\n'); int matches = 0;
        for (auto& line : lines) if (QJsonDocument::fromJson(line).object().value("eventId") == event.value("eventId")) {
            line = QJsonDocument(event).toJson(QJsonDocument::Compact); ++matches;
        }
        return matches == 1 && write(historyPath, lines.join('\n'));
    };
    for (const auto& field : QStringList{"originContractId", "originContractFingerprint", "originPostflightFingerprint", "projectId", "provenance"}) {
        auto changed = cloneBound;
        changed.insert(field, field == "provenance" ? QJsonValue(QJsonObject{{"actor", "agent"}, {"agentId", "wrong-agent"}, {"tool", "WorkerTaskServices"}}) : QJsonValue("independently-altered-bound-field"));
        bindingLinks &= replaceSyntheticEvent(changed) && refresh();
        const auto rejected = WorkerTaskServices::postflight(model, cloneContract);
        bindingLinks &= rejected.value("status") == "FAIL" && hasCode(rejected, "REAUTHORIZATION_AUDIT_INVALID");
        bindingLinks &= write(historyPath, cloneHistory) && refresh() && WorkerTaskServices::postflight(model, cloneContract).value("status") == "PASS";
    }
    for (const auto& field : QStringList{"projectId", "recordingAgentId", "operations", "approvedPathFingerprints", "grantFingerprint"}) {
        auto changedGrant = cloneGrant; auto changedAuthority = cloneAuthority; auto changedValidation = cloneValidation;
        if (field == "operations") { auto values = changedGrant.value(field).toObject(); values.insert("test_update/campaign.json", "MODIFY"); changedGrant.insert(field, values); }
        else if (field == "approvedPathFingerprints") { auto values = changedGrant.value(field).toObject(); values.insert(files.first(), QString(64, '0')); changedGrant.insert(field, values); }
        else if (field != "grantFingerprint") changedGrant.insert(field, "independently-altered-authority-field");
        changedAuthority.insert("taskReauthorizationGrant", changedGrant);
        changedValidation.insert("grantFingerprint", field == "grantFingerprint" ? QString(64, '0') : grantHash(changedGrant));
        auto changedCompletion = cloneCompletion; changedCompletion.insert("grantFingerprint", grantHash(changedGrant));
        bindingLinks &= replaceSyntheticEvent(changedAuthority) && replaceSyntheticEvent(changedValidation)
            && write(cloneCompletionPath, QJsonDocument(changedCompletion).toJson()) && refresh();
        const auto rejected = WorkerTaskServices::postflight(model, cloneContract);
        bindingLinks &= rejected.value("status") == "FAIL" && hasCode(rejected, "REAUTHORIZATION_AUDIT_INVALID");
        bindingLinks &= write(historyPath, cloneHistory) && write(cloneCompletionPath, QJsonDocument(cloneCompletion).toJson())
            && refresh() && WorkerTaskServices::postflight(model, cloneContract).value("status") == "PASS";
    }
    expect(bindingLinks, "repair successor rejects forged omitted widened or changed origins");
    bool duplicateChains = true;
    for (const auto& event : QList<QJsonObject>{cloneAuthority, cloneValidation, cloneBound}) {
        const bool appended = !append(event.value("eventType").toString(), event).isEmpty();
        const bool coherent = refresh();
        const auto rejected = WorkerTaskServices::postflight(model, cloneContract);
        const bool deniedDuplicate = rejected.value("status") == "FAIL" && hasCode(rejected, "REAUTHORIZATION_AUDIT_INVALID");
        const bool restored = restoreCloneControl();
        duplicateChains &= appended && coherent && deniedDuplicate && restored;
        if (!appended || !coherent || !deniedDuplicate || !restored)
            std::cerr << "Repair duplicate control " << event.value("eventType").toString().toStdString()
                << " appended=" << appended << " coherent=" << coherent << " denied=" << deniedDuplicate << " restored=" << restored
                << '\n' << QJsonDocument(rejected).toJson().toStdString();
    }
    auto partial = grant; partial.insert("authorizationId", QUuid::createUuid().toString(QUuid::WithoutBraces));
    partial.insert("issuanceCompletionPath", "build/aramf-repair-authority/" + partial.value("authorizationId").toString() + "/issuance.json");
    auto partialAuthority = authority; partialAuthority.insert("taskReauthorizationGrant", partial); append("TASK_REPAIR_SCOPE_AUTHORIZED", partialAuthority); refresh();
    const auto partialTree = treeBytes(root);
    expect(!ready(WorkerTaskServices::prepareReauthorized(model, partial.value("authorizationId").toString(), origin, failed)) && treeBytes(root) == partialTree,
        "repair successor retains incomplete authority as inactive");
    auto expired = grant; expired.insert("authorizationId", QUuid::createUuid().toString(QUuid::WithoutBraces));
    expired.insert("issuanceCompletionPath", "build/aramf-repair-authority/" + expired.value("authorizationId").toString() + "/issuance.json");
    expired.insert("issuedAt", QDateTime::currentDateTimeUtc().addSecs(-3600).toString(Qt::ISODateWithMs)); expired.insert("expiresAt", QDateTime::currentDateTimeUtc().addSecs(-1).toString(Qt::ISODateWithMs));
    auto expiredAuthority = authority; expiredAuthority.insert("taskReauthorizationGrant", expired); expiredAuthority = append("TASK_REPAIR_SCOPE_AUTHORIZED", expiredAuthority);
    auto expiredValidation = validation; expiredValidation.insert("authorizationId", expired.value("authorizationId")); expiredValidation.insert("authorityEventId", expiredAuthority.value("eventId"));
    expiredValidation.insert("grantFingerprint", grantHash(expired)); expiredValidation = append("TASK_REAUTHORIZATION_AUTHORIZED", expiredValidation);
    auto expiredCompletion = completion; expiredCompletion.insert("authorizationId", expired.value("authorizationId")); expiredCompletion.insert("authorityEventId", expiredAuthority.value("eventId")); expiredCompletion.insert("validationEventId", expiredValidation.value("eventId"));
    expiredCompletion.insert("grantFingerprint", grantHash(expired)); expiredCompletion.insert("validationTimestamp", expiredValidation.value("timestamp"));
    write(root + '/' + expired.value("issuanceCompletionPath").toString(), QJsonDocument(expiredCompletion).toJson()); refresh();
    const bool expiredDenied = !ready(WorkerTaskServices::prepareReauthorized(model, expired.value("authorizationId").toString(), origin, failed));
    const bool boundReplayDenied = !ready(WorkerTaskServices::prepareReauthorized(model, id, origin, failed));
    const bool issuanceReplayDenied = approve(request).value("status") == "BLOCKED";
    const auto terminalControl = WorkerTaskServices::postflight(model, successor);
    bool terminal = duplicateChains && expiredDenied && boundReplayDenied && issuanceReplayDenied && terminalControl.value("status") == "PASS";
    if (!terminal) std::cerr << "Repair terminal controls duplicates=" << duplicateChains << " expiry=" << expiredDenied
        << " boundReplay=" << boundReplayDenied << " issuanceReplay=" << issuanceReplayDenied << '\n'
        << QJsonDocument(terminalControl).toJson().toStdString();
    append("TASK_REAUTHORIZATION_REVOKED", {{"authorizationId", id}, {"provenance", QJsonObject{{"actor", "system"}, {"agentId", "fixture"}, {"tool", "test"}}}}); refresh();
    terminal &= WorkerTaskServices::postflight(model, successor).value("status") == "FAIL";
    expect(terminal, "repair successor rejects expiry duplication revocation and replay");
    return results;
}

QJsonObject repairSuccessorAssertions()
{
    auto result = repairSuccessorAssertions(false);
    const auto failedCommit = repairSuccessorAssertions(true);
    for (auto it = failedCommit.begin(); it != failedCommit.end(); ++it)
        result.insert(it.key(), it.value().toBool() && result.value(it.key()).toBool(true));
    const auto v2 = repairSuccessorAssertions(false, "task-p6-activation-campaign-isolation/v2");
    for (auto it = v2.begin(); it != v2.end(); ++it)
        result.insert(it.key(), it.value().toBool() && result.value(it.key()).toBool(true));
    return result;
}

namespace {
QJsonObject scopeRegistrationAssertions()
{
    QJsonObject results;
    const auto expect = [&](bool ok, const QString& name) { results.insert(name, ok && results.value(name).toBool(true)); return ok; };
    QTemporaryDir fixture; fixture.setAutoRemove(false); const QString root = fixture.path();
    QElapsedTimer fixtureTimer; fixtureTimer.start();
    const auto timing = [&](const QString& stage, QJsonObject fields = {}) {
        fields.insert("stage", stage); fields.insert("fixture", root);
        fields.insert("elapsedMs", double(fixtureTimer.elapsed()));
        fields.insert("observedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        QTextStream(stderr) << "SCOPE-REGISTRATION-TIMING " << QJsonDocument(fields).toJson(QJsonDocument::Compact) << '\n';
    };
    timing("fixture-start");
    const QString profileId = "task-scope-registration-operations/v1";
    const auto profile = WorkerTaskServices::acceptanceProfile(profileId);
    const auto run = [&](const QString& program, const QStringList& args) {
        QProcess process; process.setWorkingDirectory(root); process.start(program, args);
        const bool ok = process.waitForFinished(120000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (!ok) QTextStream(stderr) << "SCOPE FIXTURE " << program << ' ' << args.join(' ') << '\n' << process.readAllStandardOutput() << process.readAllStandardError();
        return ok;
    };
    // Synthetic verifier input exercises real producer execution and semantic
    // admission in an isolated project. It is never evidence for this project.
    QJsonObject synthetic;
    for (const auto& criterion : profile.value("criteria").toArray())
        for (const auto& name : criterion.toObject().value("assertions").toArray()) synthetic.insert(name.toString(), "PASS");
    const auto payload = QJsonDocument(QJsonObject{{"profile", profileId}, {"assertions", synthetic}, {"syntheticVerifierFixture", true}}).toJson(QJsonDocument::Compact);
    if (!write(root + "/fixture.cpp", QByteArray("#include <cstdio>\nint main(){puts(R\"fixture(WORKER-TASK-ACCEPTANCE ") + payload + ")fixture\");return 0;}\n")
        || !write(root + "/CMakeLists.txt", "cmake_minimum_required(VERSION 3.21)\nproject(ScopeRegistrationFixture LANGUAGES CXX)\ninclude(CTest)\nadd_executable(aramf_core_tests fixture.cpp)\nforeach(name aramf_core_tests aramf_template_tests aramf_workflow_tests)\nadd_test(NAME ${name} COMMAND aramf_core_tests)\nendforeach()\n")
        || !write(root + "/.gitignore", "build/\n") || !write(root + "/src/new-source.cpp", "// existing unmapped source, preserve its bytes\n")
        || !write(root + "/src/protected.h", "// external source of truth\n") || !write(root + "/unrelated.txt", "unrelated dirty work\n")) return {{"scope fixture files", false}};
    for (const auto& file : profile.value("files").toArray()) if (!write(root + '/' + file.toString(), "// isolated registration service observation\n")) return {{"scope fixture source files", false}};
    ProjectModel model; model.setProjectPath(root); model.setProjectFilePath(root + "/fixture.aramf.json"); model.setProjectId("isolated-scope-registration");
    auto ai = model.aiConfiguration(); ai.permissions = {"read-project-files", "modify-files", "create-files", "run-project-tools", "run-builds", "run-tests"}; model.setAiConfiguration(ai);
    auto rules = model.ruleConfiguration(); rules.projectScopes = {"source-code", "tests"};
    rules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray{"src/core/WorkerTaskServices.h", "src/core/WorkerTaskServices.cpp"}},
        {"affects", QJsonArray{"tests"}}, {"tests", QJsonArray{"aramf_core_tests::--worker-tasks", "production-build", "ctest"}}}},
        {"tests", QJsonObject{{"files", QJsonArray{"tests/WorkerTaskTests.cpp"}}, {"tests", QJsonArray{"aramf_core_tests::--worker-tasks"}}}}}; model.setRuleConfiguration(rules);
    ProjectResource resource; resource.id = "scope-protected-source"; resource.type = "file"; resource.location = "src/protected.h";
    resource.role = "source-of-truth"; resource.scopes = {"source-code"}; model.setResources({resource});
    QString error;
    if (!ProjectPersistence().save(model, model.projectFilePath(), &error)
        || !ProjectPersistence().load(&model, model.projectFilePath(), &error)
        || !GenerationServices().generate(model, model.generationOptions()).success
        || !run("git", {"init", "-q"}) || !run("git", {"add", "."})
        || !run("git", {"-c", "user.name=Scope Fixture", "-c", "user.email=scope@example.invalid", "-c", "commit.gpgsign=false", "commit", "-qm", "isolated scope registration"})
        || !run("cmake", {"-S", root, "-B", root + "/build", "-G", "Ninja", "-DBUILD_TESTING=ON"})) return {{"scope fixture setup: " + error, false}};
    VerificationServices().verify(model, model.generationOptions()); ContextCoordinationService::generate(model);
    if (VerificationServices().verify(model, model.generationOptions()).overallStatus != VerificationStatus::Pass) return {{"scope fixture current verification", false}};
    timing("fixture-ready");
    WorkerTaskRequest software; software.goal = "Verify isolated scope registration service"; software.type = "implementation"; software.scopes = {"source-code", "tests"};
    for (const auto& file : profile.value("files").toArray()) software.files.append(file.toString());
    for (const auto& requirement : profile.value("requirements").toArray()) software.definitionOfDone.append(requirement.toString());
    const auto verification = WorkerTaskServices::prepare(model, software);
    const auto verificationBytes = QJsonDocument(verification).toJson();
    if (!expect(ready(verification) && !verification.value("request").toObject().contains("acceptanceProfile"), "scope registration exact legacy requirements retain contract authority")) return results;
    for (const auto& value : verification.value("requiredEvidence").toArray()) {
        const QString check = value.toString(); if (check == "diff-boundary" || check == "acceptance-criteria") continue;
        const auto existing = WorkerTaskServices::checkEvidence(model, verification); bool covered = false;
        for (const auto& entry : existing.value("evidence").toArray()) covered |= entry.toObject().value("check") == check;
        if (covered) continue;
        const auto measured = WorkerTaskServices::runCheck(model, verification, check, root + "/build", 120000);
        timing("prerequisite", {{"check", check}, {"status", measured.value("status")}});
        if (measured.value("status") != "PASS") { QTextStream(stderr) << "SCOPE CHECK " << check << ' ' << QJsonDocument(measured).toJson(); return {{"scope fixture measured prerequisite " + check, false}}; }
    }
    const auto accepted = WorkerTaskServices::runCheck(model, verification, "acceptance-criteria", root + "/build", 120000);
    const auto index = WorkerTaskServices::checkEvidence(model, verification);
    if (!expect(accepted.value("status") == "PASS" && index.value("status") == "PASS"
        && WorkerTaskServices::postflight(model, verification, index.value("evidence").toArray()).value("completionState") == "VERIFIED",
        "scope registration measured acceptance completes")) return results;
    timing("software-verified", {{"acceptanceFinishedAt", accepted.value("record").toObject().value("finishedAt")}});
    WorkerTaskRequest task; task.goal = "Implement exact new source behavior"; task.type = "implementation"; task.scopes = {"source-code", "tests"};
    task.files = {"src/core/WorkerTaskServices.cpp", "src/new-source.cpp", "src/new-component.h"}; task.definitionOfDone = {"New source behavior is covered by existing focused tests"};
    const auto original = WorkerTaskServices::prepare(model, task); const auto originalBytes = QJsonDocument(original).toJson();
    const QJsonObject intent{{"kind", "add-scope-files/v1"}, {"scope", "source-code"}, {"files", QJsonArray{"src/new-source.cpp", "src/new-component.h"}},
        {"agentId", "fixture/scope-registration"}, {"reason", "Register exact isolated missing task mappings"}, {"validityMinutes", 1440}};
    const auto beforeTree = treeBytes(root);
    QElapsedTimer prepareTimer; prepareTimer.start();
    const auto plan = WorkerTaskServices::prepareScopeRegistrationOperation(model, intent, original, verification);
    const qint64 measuredPrepareMs = prepareTimer.elapsed();
    timing("initial-plan", {{"prepareMs", double(measuredPrepareMs)}, {"preflight", plan.value("preflight")}});
    if (!expect(ready(plan) && treeBytes(root) == beforeTree, "scope registration preparation is read-only")) {
        QTextStream(stderr) << "SCOPE PLAN " << QJsonDocument(plan.value("preflight").toObject()).toJson(); return results;
    }
    const auto outputs = plan.value("shape").toObject().value("outputs").toObject();
    expect(plan.value("originalTask") == original && plan.value("permittedFiles").toArray().size() == outputs.size()
        && outputs.contains("fixture.aramf.json") && !outputs.contains("src/new-source.cpp") && !outputs.contains("CMakeLists.txt")
        && !outputs.contains("ARAMF_WORKER/certification/certificates.jsonl"), "scope registration binds original task and exact outputs");
    // Each invalid saved input starts from the same authentic fixture. Never
    // let a rejection migrate the file, or change any other retained byte.
    const auto invalidSavedInputs = [](const QByteArray& current) {
        QList<QPair<QString, QByteArray>> inputs;
        const auto object = QJsonDocument::fromJson(current).object();
        for (const auto& variant : QList<QPair<QString, QJsonValue>>{{"legacy schema", 1},
            {"missing schema", QJsonValue(QJsonValue::Undefined)}, {"string schema", "2"},
            {"fractional schema", ProjectPersistence::currentSchemaVersion() + 0.5},
            {"future schema", ProjectPersistence::currentSchemaVersion() + 1}}) {
            auto changed = object;
            if (variant.second.isUndefined()) changed.remove("schemaVersion"); else changed.insert("schemaVersion", variant.second);
            inputs.append({variant.first, QJsonDocument(changed).toJson()});
        }
        inputs.append(QPair<QString, QByteArray>{QStringLiteral("malformed saved JSON"), QByteArray("{invalid-json")});
        inputs.append(QPair<QString, QByteArray>{QStringLiteral("malformed saved JSON"), QByteArray("[]")});
        auto changed = object; changed.insert("projectName", "Independent saved-model mismatch");
        inputs.append({"saved model mismatch", QJsonDocument(changed).toJson()});
        return inputs;
    };
    QTemporaryDir commandInputs;
    const QString inputRoot = commandInputs.path();
    if (!write(inputRoot + "/intent.json", QJsonDocument(intent).toJson())
        || !write(inputRoot + "/original.json", originalBytes)
        || !write(inputRoot + "/verification.json", verificationBytes)
        || !write(inputRoot + "/plan.json", QJsonDocument(plan).toJson())) return {{"scope fixture CLI inputs", false}};
    const QList<QStringList> inspectionCommands{
        {"task", "prepare-scope-registration", "--config", model.projectFilePath(), "--request", inputRoot + "/intent.json",
            "--blocked-contract", inputRoot + "/original.json", "--verification-contract", inputRoot + "/verification.json"},
        {"task", "apply-scope-registration", "--config", model.projectFilePath(), "--contract", inputRoot + "/plan.json"},
        {"task", "postflight-scope-registration", "--config", model.projectFilePath(), "--contract", inputRoot + "/plan.json"},
        {"task", "postflight", "--config", model.projectFilePath(), "--contract", inputRoot + "/plan.json"}};
    const auto configBytes = bytes(model.projectFilePath());
    for (const auto& variant : invalidSavedInputs(configBytes)) {
        const bool wrote = write(model.projectFilePath(), variant.second);
        const auto invalidTree = treeBytes(root);
        const auto rejected = WorkerTaskServices::prepareScopeRegistrationOperation(model, intent, original, verification);
        const bool rejectedPurely = !ready(rejected) && hasCode(rejected, "SCOPE_REGISTRATION_CONFIGURATION_INVALID")
            && treeBytes(root) == invalidTree;
        const auto refusedApply = WorkerTaskServices::applyScopeRegistrationOperation(model, plan);
        const bool applyPure = refusedApply.value("status") == "BLOCKED" && treeBytes(root) == invalidTree;
        if (variant.first != "saved model mismatch") for (const auto& arguments : inspectionCommands) {
            QString commandOutput, commandError; QTextStream output(&commandOutput), errors(&commandError);
            const int exit = runWorkerTaskCommand(arguments, output, errors);
            expect(wrote && exit == 2 && commandError.contains("current-schema saved model") && treeBytes(root) == invalidTree,
                "scope registration CLI rejects invalid saved input without writes");
        }
        const bool restored = write(model.projectFilePath(), configBytes);
        expect(wrote && rejectedPurely && applyPure && restored && treeBytes(root) == beforeTree,
            "scope registration rejects " + variant.first + " without writes");
        expect(rejectedPurely && !hasCode(rejected, "SCOPE_REGISTRATION_EXPIRED"),
            "scope registration does not misreport missing proof as expiry");
        timing("saved-input-rejection", {{"case", variant.first}, {"preflight", rejected.value("preflight")}});
    }
    QString preparedOutput, preparedError; QTextStream preparedStream(&preparedOutput), errorStream(&preparedError);
    const int preparedExit = runWorkerTaskCommand(inspectionCommands.first(), preparedStream, errorStream);
    const auto cliPlan = QJsonDocument::fromJson(preparedOutput.toUtf8()).object().value("contract").toObject();
    expect(preparedExit == 0 && preparedError.isEmpty() && ready(cliPlan)
        && cliPlan.value("beforeModel") == ProjectPersistence().toJson(model) && treeBytes(root) == beforeTree,
        "scope registration preparation is read-only");
    const auto rejectsIntent = [&](QJsonObject candidate, const QString& code, const QString& name, const QJsonObject& origin) {
        const auto before = treeBytes(root);
        const auto rejected = WorkerTaskServices::prepareScopeRegistrationOperation(model, candidate, origin, verification);
        expect(!ready(rejected) && hasCode(rejected, code) && treeBytes(root) == before, name);
    };
    auto extra = intent; extra.insert("riskTraits", QJsonArray{}); rejectsIntent(extra, "SCOPE_REGISTRATION_INTENT_INVALID", "scope registration rejects extra intent fields", original);
    extra = intent; extra.insert("files", QJsonArray{"src/undeclared.h"}); rejectsIntent(extra, "SCOPE_REGISTRATION_FILES_INVALID", "scope registration rejects undeclared source", original);
    for (const auto& pair : QList<QPair<QString, QString>>{{"CMakeLists.txt", "CMake"}, {"fixture.aramf.json", "saved configuration"},
        {"ARAMF_WORKER/project.json", "Worker output"}, {"../escape.cpp", "traversal"}, {"src/*.cpp", "wildcard"}, {"src/protected.h", "protected resource"}}) {
        auto unsafeTask = task; unsafeTask.files = {pair.first}; const auto blocked = WorkerTaskServices::prepare(model, unsafeTask);
        auto unsafe = intent; unsafe.insert("files", QJsonArray{pair.first});
        rejectsIntent(unsafe, "SCOPE_REGISTRATION_PATH_INVALID", "scope registration rejects " + pair.second, blocked);
    }
    extra = intent; extra.insert("scope", "configuration"); rejectsIntent(extra, "SCOPE_REGISTRATION_SCOPE_INVALID", "scope registration rejects inactive scope", original);
    extra = intent; extra.insert("files", QJsonArray{"src/core/WorkerTaskServices.cpp"}); rejectsIntent(extra, "SCOPE_REGISTRATION_FILES_INVALID", "scope registration rejects conflicting mapping", original);
    extra = intent; extra.insert("files", QJsonArray{"src/new-source.cpp"}); rejectsIntent(extra, "SCOPE_REGISTRATION_FILES_INVALID", "scope registration rejects incomplete mapping", original);
    const auto unmeasured = WorkerTaskServices::prepareScopeRegistrationOperation(model, intent, original, QJsonObject{});
    expect(!ready(unmeasured) && hasCode(unmeasured, "SCOPE_REGISTRATION_VERIFICATION_INVALID") && treeBytes(root) == beforeTree, "scope registration rejects unmeasured verification");
    expect(!ready(unmeasured) && hasCode(unmeasured, "SCOPE_REGISTRATION_VERIFICATION_INVALID")
        && !hasCode(unmeasured, "SCOPE_REGISTRATION_EXPIRED"), "scope registration does not misreport missing proof as expiry");
    const auto rejectsApply = [&](const ProjectModel& inputModel, const QJsonObject& candidate, const QString& name) {
        const auto before = treeBytes(root); const auto rejected = WorkerTaskServices::applyScopeRegistrationOperation(inputModel, candidate);
        expect(rejected.value("status") == "BLOCKED" && treeBytes(root) == before, name);
    };
    auto forged = plan; forged.insert("contractId", "forged"); rejectsApply(model, forged, "scope registration rejects forged operation");
    forged = plan; auto permissions = forged.value("permittedFiles").toArray(); permissions.append("unrelated.txt"); forged.insert("permittedFiles", permissions); checksumFixtureContract(forged);
    rejectsApply(model, forged, "scope registration rejects recomputed expanded operation");
    const auto sourceBytes = bytes(root + "/src/core/WorkerTaskServices.cpp"); write(root + "/src/core/WorkerTaskServices.cpp", sourceBytes + "// newer source\n");
    rejectsApply(model, plan, "scope registration rejects changed source"); write(root + "/src/core/WorkerTaskServices.cpp", sourceBytes);
    ProjectModel changed; ProjectPersistence().fromJson(&changed, ProjectPersistence().toJson(model), &error); changed.setProjectName("Changed config");
    rejectsApply(changed, plan, "scope registration rejects changed configuration");
    ProjectPersistence().fromJson(&changed, ProjectPersistence().toJson(model), &error); changed.setProjectId("different-project");
    rejectsApply(changed, plan, "scope registration rejects wrong project");
    // Use the shortest whole-minute lifetime covering the actual observation
    // age, measured preparation cost and a bounded margin; then really wait.
    // Neither timestamps nor retained prerequisite evidence are manufactured.
    const auto anchor = QDateTime::fromString(plan.value("issuedAt").toString(), Qt::ISODateWithMs);
    const qint64 ageSeconds = (anchor.msecsTo(QDateTime::currentDateTimeUtc()) + 999) / 1000;
    const qint64 prepareSeconds = (measuredPrepareMs + 999) / 1000;
    const qint64 marginSeconds = qBound<qint64>(qint64(5), 2 * prepareSeconds + 2, qint64(120));
    const qint64 validityMinutes = (ageSeconds + prepareSeconds + marginSeconds + 59) / 60;
    const bool validityInRange = anchor.isValid() && ageSeconds >= 0 && validityMinutes >= 1 && validityMinutes <= 1440;
    auto shortIntent = intent; shortIntent.insert("validityMinutes", int(validityMinutes));
    prepareTimer.restart();
    const auto expiringPlan = validityInRange ? WorkerTaskServices::prepareScopeRegistrationOperation(model, shortIntent, original, verification) : QJsonObject{};
    timing("expiring-plan", {{"prepareMs", double(prepareTimer.elapsed())}, {"validityMinutes", double(validityMinutes)},
        {"marginSeconds", double(marginSeconds)}, {"preflight", expiringPlan.value("preflight")}, {"expiresAt", expiringPlan.value("expiresAt")}});
    if (!expect(validityInRange && ready(expiringPlan) && treeBytes(root) == beforeTree, "scope registration rejects expired operation")) return results;
    const auto expiry = QDateTime::fromString(expiringPlan.value("expiresAt").toString(), Qt::ISODateWithMs);
    QElapsedTimer expiryWait; expiryWait.start();
    const qint64 waitBudgetMs = measuredPrepareMs + (marginSeconds + 60) * 1000;
    while (QDateTime::currentDateTimeUtc() < expiry && expiryWait.elapsed() < waitBudgetMs)
        QThread::msleep(50);
    const bool expiredBeforeInspection = expiry.isValid() && QDateTime::currentDateTimeUtc() >= expiry;
    timing("actual-expiry", {{"waitMs", double(expiryWait.elapsed())}, {"expiresAt", expiringPlan.value("expiresAt")},
        {"expiredBeforeInspection", expiredBeforeInspection}});
    const auto expired = WorkerTaskServices::prepareScopeRegistrationOperation(model, shortIntent, original, verification);
    bool onlyExpired = expiredBeforeInspection && !ready(expired) && hasCode(expired, "SCOPE_REGISTRATION_EXPIRED");
    for (const auto& value : expired.value("preflight").toObject().value("errors").toArray()) {
        const auto entry = value.toObject();
        if (entry.value("severity") != "WARNING") onlyExpired &= entry.value("code") == "SCOPE_REGISTRATION_EXPIRED";
    }
    expect(onlyExpired && treeBytes(root) == beforeTree, "scope registration rejects expired operation");
    rejectsApply(model, expiringPlan, "scope registration rejects expired operation");
    const auto digest = QString::fromLatin1(QCryptographicHash::hash(QDir(model.projectPath()).absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex());
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + digest + ".lock"); lock.setStaleLockTime(0);
    if (lock.tryLock(0)) { rejectsApply(model, plan, "scope registration rejects lock contention"); lock.unlock(); }
    else expect(false, "scope registration rejects lock contention");
    const auto priorEvents = ProjectMemory().events(root, &error);
    timing("apply-start");
    const auto applied = WorkerTaskServices::applyScopeRegistrationOperation(model, plan);
    timing("apply-finished", {{"status", applied.value("status")}, {"error", applied.value("error")}});
    ProjectModel after; const bool loaded = ProjectPersistence().load(&after, model.projectFilePath(), &error);
    const auto post = loaded ? WorkerTaskServices::postflightScopeRegistrationOperation(after, plan) : QJsonObject{};
    timing("postflight-finished", {{"status", post.value("status")}, {"errors", post.value("errors")}});
    const bool completed = applied.value("status") == "PASS" && loaded && post.value("completionState") == "VERIFIED";
    if (!expect(completed, "scope registration canonical apply and readback pass")) {
        QTextStream(stderr) << "SCOPE APPLY " << QJsonDocument(applied).toJson() << "SCOPE POST " << QJsonDocument(post).toJson(); return results;
    }
    auto expected = model.ruleConfiguration(); auto mapped = expected.scopeMetadata.value("source-code").toObject();
    QStringList files;
    for (const auto& file : mapped.value("files").toArray()) files.append(file.toString());
    files.append("src/new-source.cpp"); files.append("src/new-component.h"); std::sort(files.begin(), files.end());
    mapped.insert("files", QJsonArray::fromStringList(files)); expected.scopeMetadata.insert("source-code", mapped);
    ProjectPersistence().fromJson(&changed, ProjectPersistence().toJson(model), &error); changed.setRuleConfiguration(expected);
    expect(ProjectPersistence().toJson(after) == ProjectPersistence().toJson(changed) && bytes(root + "/unrelated.txt") == beforeTree.value("unrelated.txt")
        && bytes(root + "/src/new-source.cpp") == beforeTree.value("src/new-source.cpp") && !QFileInfo::exists(root + "/src/new-component.h"),
        "scope registration preserves unrelated state and lifecycle");
    const auto receiptPath = plan.value("shape").toObject().value("receipt").toString(), proofPath = plan.value("shape").toObject().value("proof").toString();
    const auto receiptBytes = bytes(root + '/' + receiptPath), proofBytes = bytes(root + '/' + proofPath);
    const auto receipt = QJsonDocument::fromJson(receiptBytes).object();
    const auto audit = receipt.value("audit").toObject();
    const auto applyStarted = QDateTime::fromString(receipt.value("applyStartedAt").toString(), Qt::ISODateWithMs);
    const auto completedAt = QDateTime::fromString(receipt.value("timestamp").toString(), Qt::ISODateWithMs);
    expect(applyStarted.isValid() && completedAt.isValid()
        && applyStarted >= QDateTime::fromString(plan.value("issuedAt").toString(), Qt::ISODateWithMs)
        && completedAt >= applyStarted && completedAt < QDateTime::fromString(plan.value("expiresAt").toString(), Qt::ISODateWithMs),
        "scope registration rejects expired operation");
    bool noAdminOverride = true;
    for (const auto& event : ProjectMemory().events(root, &error))
        if (event.value("eventType").toString().startsWith("ADMIN_OVERRIDE")) noAdminOverride &= priorEvents.contains(event);
    const QJsonObject expectedProvenance{{"actor", "agent"}, {"agentId", "fixture/scope-registration"},
        {"tool", "WorkerTaskServices/scope-registration-operation/v1"}};
    expect(QJsonDocument(original).toJson() == originalBytes && QJsonDocument(verification).toJson() == verificationBytes
        && audit.value("eventType") == "SCOPE_MAPPING_REGISTERED" && audit.value("provenance") == expectedProvenance
        && noAdminOverride && !audit.contains("authority") && error.isEmpty()
        && QJsonDocument::fromJson(proofBytes).object().value("operationContract") == plan,
        "scope registration retains immutable original and agent audit");
    const auto completedTree = treeBytes(root);
    const auto completedConfig = bytes(model.projectFilePath());
    for (const auto& variant : invalidSavedInputs(completedConfig)) {
        const bool wrote = write(model.projectFilePath(), variant.second);
        const auto invalidTree = treeBytes(root);
        const auto rejected = WorkerTaskServices::postflightScopeRegistrationOperation(after, plan);
        const bool rejectedPurely = rejected.value("status") == "FAIL"
            && hasCode(rejected, "SCOPE_REGISTRATION_CONFIGURATION_INVALID") && treeBytes(root) == invalidTree;
        const bool restored = write(model.projectFilePath(), completedConfig);
        expect(wrote && rejectedPurely && restored && treeBytes(root) == completedTree,
            "scope registration postflight rejects invalid saved input without writes");
        timing("saved-postflight-rejection", {{"case", variant.first}, {"status", rejected.value("status")}});
    }
    expect(WorkerTaskServices::postflightScopeRegistrationOperation(after, plan).value("completionState") == "VERIFIED"
        && treeBytes(root) == completedTree, "scope registration canonical apply and readback pass");
    expect(ready(WorkerTaskServices::prepare(after, task)) && treeBytes(root) == completedTree, "scope registration successor prepares normally");
    rejectsApply(after, plan, "scope registration rejects replay");
    write(root + '/' + receiptPath, receiptBytes + " ");
    expect(hasCode(WorkerTaskServices::postflightScopeRegistrationOperation(after, plan), "SCOPE_REGISTRATION_RECEIPT_INVALID"), "scope registration rejects altered receipt");
    write(root + '/' + receiptPath, receiptBytes);
    write(root + '/' + proofPath, proofBytes + " ");
    expect(hasCode(WorkerTaskServices::postflightScopeRegistrationOperation(after, plan), "SCOPE_REGISTRATION_RECEIPT_INVALID"), "scope registration rejects altered proof");
    write(root + '/' + proofPath, proofBytes);
    for (const QString& file : {QStringLiteral("context-index.json"), QStringLiteral("compressed-context.json"),
        QStringLiteral("freshness.json"), QStringLiteral("agent-adapters.json")}) {
        const auto path = root + "/ARAMF_WORKER/context/" + file;
        const auto originalBytes = bytes(path);
        auto changedContext = QJsonDocument::fromJson(originalBytes).object(); changedContext.insert("unexpectedFixtureField", true);
        const bool changed = write(path, QJsonDocument(changedContext).toJson());
        const auto tamperedTree = treeBytes(root);
        const auto rejected = WorkerTaskServices::postflightScopeRegistrationOperation(after, plan);
        const bool rejectedPurely = hasCode(rejected, "SCOPE_REGISTRATION_CONTEXT_INVALID") && treeBytes(root) == tamperedTree;
        const bool restored = write(path, originalBytes);
        expect(changed && rejectedPurely && restored && treeBytes(root) == completedTree, "scope registration rejects altered context");
    }
    const auto rejectsRetainedBytes = [&](const QString& relative, const QString& code, const QString& name, bool remove) {
        const auto path = root + '/' + relative;
        const auto originalBytes = bytes(path);
        const bool changed = remove ? QFile::remove(path) : write(path, originalBytes + " ");
        const auto tamperedTree = treeBytes(root);
        const auto rejected = WorkerTaskServices::postflightScopeRegistrationOperation(after, plan);
        const bool rejectedPurely = hasCode(rejected, code) && treeBytes(root) == tamperedTree;
        const bool restored = write(path, originalBytes);
        expect(changed && rejectedPurely && restored && treeBytes(root) == completedTree, name);
    };
    rejectsRetainedBytes("ARAMF_WORKER/memory/cold-start-validation.json", "SCOPE_REGISTRATION_RECEIPT_INVALID",
        "scope registration rejects altered cold-start", false);
    const auto retainedRecords = plan.value("softwareProof").toObject().value("records").toObject();
    if (retainedRecords.isEmpty()) expect(false, "scope registration rejects altered retained evidence");
    else for (const bool remove : {false, true})
        rejectsRetainedBytes(retainedRecords.keys().first(), "SCOPE_REGISTRATION_VERIFICATION_INVALID",
            "scope registration rejects altered retained evidence", remove);
    const auto addedSource = bytes(root + "/src/new-source.cpp"); write(root + "/src/new-source.cpp", addedSource + "// unauthorized implementation\n");
    expect(hasCode(WorkerTaskServices::postflightScopeRegistrationOperation(after, plan), "SCOPE_REGISTRATION_CONTRACT_INVALID"), "scope registration never permits source mutation");
    write(root + "/src/new-source.cpp", addedSource);
    timing("fixture-finished", {{"observedAssertions", results.size()}});
    return results;
}
}

bool runWorkerTaskTests()
{
    if (QCoreApplication::arguments().contains("--worker-tasks") && QCoreApplication::arguments().contains("--scope-registration-only")) {
        // Diagnostic selection cannot emit whole-worker acceptance evidence.
        // The canonical producer invokes the unchanged --worker-tasks command.
        const auto observed = scopeRegistrationAssertions();
        QSet<QString> required;
        for (const auto& criterion : WorkerTaskServices::acceptanceProfile("task-scope-registration-operations/v1").value("criteria").toArray())
            for (const auto& name : criterion.toObject().value("assertions").toArray()) required.insert(name.toString());
        QJsonObject assertions; int failed = 0;
        for (auto it = observed.begin(); it != observed.end(); ++it) {
            assertions.insert(it.key(), it.value().toBool() ? "PASS" : "FAIL");
            if (!it.value().toBool()) ++failed;
        }
        QStringList missing;
        for (const auto& name : required) if (!observed.contains(name)) missing.append(name);
        std::sort(missing.begin(), missing.end());
        const bool exact = missing.isEmpty() && observed.size() == required.size();
        std::cout << "SCOPE-REGISTRATION-DIAGNOSTIC " << QJsonDocument(QJsonObject{{"schemaVersion", 1},
            {"diagnosticOnly", true}, {"assertions", assertions}, {"completed", observed.size()}, {"failed", failed},
            {"missing", QJsonArray::fromStringList(missing)}, {"exactCoverage", exact}}).toJson(QJsonDocument::Compact).toStdString() << '\n';
        return exact && failed == 0;
    }
    // Verify task permission and evidence boundaries using isolated fixtures.

    // Grant PASS records below are synthetic test evidence only; they cannot
    // certify the live repository or authorize its protected CMake file.
    int checks = 0, failed = 0;
    QJsonObject acceptanceAssertions;
    QSet<QString> requiredAssertions;
    const QStringList acceptanceProfiles{"task-reauthorization-security/v1", "task-certification-operations/v1", "task-p6-requirements-operations/v1", "task-p6-build-registration/v1", "task-p6-prerequisite-closure/v1", "task-p6-activation-governance/v1", "task-p6-activation-campaign-isolation/v1", "task-p6-activation-campaign-isolation/v2", "task-scope-registration-operations/v1"};
    for (const auto& id : acceptanceProfiles)
        for (const auto& criterion : WorkerTaskServices::acceptanceProfile(id).value("criteria").toArray())
            if (criterion.toObject().value("check") == "aramf_core_tests::--worker-tasks")
                for (const auto& assertion : criterion.toObject().value("assertions").toArray()) requiredAssertions.insert(assertion.toString());
    const auto check = [&](bool pass, const QString& name) {
        ++checks;
        if (!pass) { ++failed; std::cerr << "TASK FAIL: " << name.toStdString() << '\n'; }
        if (requiredAssertions.contains(name))
            acceptanceAssertions.insert(name, !pass || acceptanceAssertions.value(name) == "FAIL" ? "FAIL" : "PASS");
    };
    const auto generate = [&](ProjectModel& model) {
        const auto result = GenerationServices().generate(model, model.generationOptions());
        check(result.success, "fixture generation: " + result.error);
        const auto verified = VerificationServices().verify(model, model.generationOptions());
        check(verified.overallStatus == VerificationStatus::Pass, "fixture verification");
        const auto context = ContextCoordinationService::generate(model);
        check(context.value(QStringLiteral("success")).toBool(), "fixture context generation");
    };
    QTemporaryDir project;
    ProjectModel model;
    model.setProjectPath(project.path());
    model.setProjectId("task-test-A");
    auto ai = model.aiConfiguration();
    ai.permissions = {"read-project-files", "modify-files", "create-files"};
    model.setAiConfiguration(ai);
    auto memoryOptions = model.memoryConfiguration();
    memoryOptions.maintenanceOptions = {"record-test-results", "update-current-state"};
    model.setMemoryConfiguration(memoryOptions);
    auto certification = model.certificationConfiguration(); certification.enabled = true;
    model.setCertificationConfiguration(certification);
    auto rules = model.ruleConfiguration();
    rules.projectScopes = {"generic", "ui", "android", "communication", "pico", "machine-learning", "thesis", "report", "governance", "persistence"};
    for (const auto& scope : rules.projectScopes) {
        const QString file = "source/" + scope + ".cpp";
        check(write(QDir(project.path()).filePath(file), "before\n"), "write canonical source");
        rules.scopeMetadata.insert(scope, QJsonObject{{"files", QJsonArray{file}}, {"tests", QJsonArray{scope + "-tests"}},
            {"riskTraits", QJsonArray{}}, {"affects", QJsonArray{}}, {"generatedArtifacts", QJsonArray{}}});
    }
    const auto annotate = [&](const QString& scope, const QJsonArray& traits, const QJsonArray& affects) {
        auto metadata = rules.scopeMetadata.value(scope).toObject();
        metadata.insert("riskTraits", traits); metadata.insert("affects", affects); rules.scopeMetadata.insert(scope, metadata);
    };
    annotate("ui", {"ui"}, {});
    annotate("pico", {"hardware"}, {});
    annotate("governance", {"schema", "governance"}, {});
    annotate("persistence", {"persistence", "migration"}, {});
    annotate("communication", {"runtime"}, {"android", "pico"});
    model.setRuleConfiguration(rules);
    rules = model.ruleConfiguration();
    generate(model);
    const auto requestFor = [](const QStringList& scopes) {
        WorkerTaskRequest request;
        request.goal = "Implement the requested behavior";
        request.type = "coding";
        request.scopes = scopes;
        for (const auto& scope : scopes) request.files << "source/" + scope + ".cpp";
        request.definitionOfDone = {"Requested behavior is covered by the mapped focused tests."};
        return request;
    };
    const QList<QStringList> matrix{{"generic"}, {"ui", "android"}, {"communication"}, {"pico"}, {"machine-learning"},
        {"thesis"}, {"report"}, {"thesis", "report"}, {"android", "pico"}, {"governance"}, {"persistence"}};

    // PREPARE is a strictly read-only gate. Protect the complete fixture and
    // explicitly name every context artifact whose canonical producer is P1.
    const QStringList protectedContextArtifacts{
        QStringLiteral("ARAMF_WORKER/context/context-index.json"),
        QStringLiteral("ARAMF_WORKER/context/compressed-context.json"),
        QStringLiteral("ARAMF_WORKER/context/freshness.json"),
        QStringLiteral("ARAMF_WORKER/context/agent-adapters.json")};
    const auto freshTreeBefore = treeBytes(project.path());
    QMap<QString, QByteArray> freshContextBefore;
    for (const auto& path : protectedContextArtifacts)
        freshContextBefore.insert(path, bytes(QDir(project.path()).filePath(path)));
    const auto freshPrepare = WorkerTaskServices::prepare(model, requestFor({"ui"}));
    const auto originalRequestBytes = freshPrepare.value("request").toObject();
    check(WorkerTaskRequest::fromJson(originalRequestBytes).toJson() == originalRequestBytes
          && !originalRequestBytes.contains("acceptanceProfile"), "optional acceptance profile preserves original request serialization byte shape");
    auto arbitraryProfileRequest = requestFor({"ui"});
    arbitraryProfileRequest.acceptanceProfile = "task-reauthorization-security/v1";
    check(hasCode(WorkerTaskServices::prepare(model, arbitraryProfileRequest), "ACCEPTANCE_PROFILE_INVALID"),
          "arbitrary freeform requirements and unrelated scope cannot inherit canonical profile acceptance");
    auto forgedProfileContract = freshPrepare;
    auto forgedProfileRequest = forgedProfileContract.value("request").toObject();
    forgedProfileRequest.insert("acceptanceProfile", arbitraryProfileRequest.acceptanceProfile);
    forgedProfileContract.insert("request", forgedProfileRequest);
    checksumFixtureContract(forgedProfileContract);
    check(hasCode(WorkerTaskServices::postflight(model, forgedProfileContract), "TASK_PREFLIGHT_BLOCKED")
          && WorkerTaskServices::runCheck(model, forgedProfileContract, "acceptance-criteria").value("status") == "BLOCKED",
          "recomputed contract checksum cannot bypass exact acceptance-profile requirements");
    check(ready(freshPrepare), "fresh PREPARE is ready");
    check(treeBytes(project.path()) == freshTreeBefore, "fresh PREPARE changes no repository file");
    for (const auto& path : protectedContextArtifacts)
        check(bytes(QDir(project.path()).filePath(path)) == freshContextBefore.value(path),
              "fresh PREPARE preserves " + path);

    const QString staleSourcePath = QDir(project.path()).filePath(
        QStringLiteral("ARAMF_WORKER/memory/current-state.md"));
    const QByteArray currentSource = bytes(staleSourcePath);
    check(write(staleSourcePath, currentSource + "stale context input\n"), "make context stale");
    const auto staleTreeBefore = treeBytes(project.path());
    QMap<QString, QByteArray> staleContextBefore;
    for (const auto& path : protectedContextArtifacts)
        staleContextBefore.insert(path, bytes(QDir(project.path()).filePath(path)));
    const auto stalePrepare = WorkerTaskServices::prepare(model, requestFor({"ui"}));
    check(!ready(stalePrepare) && hasCode(stalePrepare, "STALE_DERIVED_ARTIFACT"),
          "stale PREPARE blocks without regeneration");
    check(treeBytes(project.path()) == staleTreeBefore, "stale PREPARE changes no repository file");
    for (const auto& path : protectedContextArtifacts)
        check(bytes(QDir(project.path()).filePath(path)) == staleContextBefore.value(path),
              "stale PREPARE preserves " + path);
    check(write(staleSourcePath, currentSource), "restore stale input fixture");
    generate(model);

    for (const auto& scopes : matrix) {
        const auto request = requestFor(scopes);
        const auto contract = WorkerTaskServices::prepare(model, request);
        if (!ready(contract)) std::cerr << QJsonDocument(contract.value("preflight").toObject()).toJson().toStdString();
        check(ready(contract), "matrix preflight " + scopes.join(','));
        check(contract == WorkerTaskServices::prepare(model, request), "deterministic contract " + scopes.join(','));
        check(contract.value("permittedFiles").toArray().size() == request.files.size(), "exact allowed set");
        check(!contract.value("protectedFiles").toArray().isEmpty() && !contract.value("negativeConstraints").toArray().isEmpty(), "negative ownership constraints");
        check(!contract.value("requiredEvidence").toArray().isEmpty(), "required evidence mapped");
        const auto impact = contract.value("impact").toObject();
        if (scopes.contains("thesis") && !scopes.contains("report")) check(impact.value("instructions").toArray() == QJsonArray{"aramf-thesis-instruction"}, "Thesis excludes Report instructions");
        if (scopes.contains("communication")) check(impact.value("affectedScopes").toArray() == QJsonArray{"android", "communication", "pico"}, "communication traverses only declared dependencies");
    }
    auto ui = requestFor({"ui"});
    auto uiContract = WorkerTaskServices::prepare(model, ui);
    check(uiContract.value("risk").toObject().value("level").toString() == "LOW", "isolated UI stays LOW");
    check(!uiContract.value("requiredEvidence").toArray().contains("ctest") && !uiContract.value("requiredEvidence").toArray().contains("memory-consistency"), "LOW avoids unrelated regression");
    check(uiContract.value("impact").toObject().value("unrelatedScopes").toArray().contains("governance"), "UI does not imply governance");
    const auto medium = WorkerTaskServices::prepare(model, requestFor({"thesis", "report"}));
    check(medium.value("risk").toObject().value("level").toString() == "MEDIUM" && medium.value("requiredEvidence").toArray().contains("affected-workflow-tests"), "two independent scopes select MEDIUM workflow coverage");
    const auto critical = WorkerTaskServices::prepare(model, requestFor({"governance"}));
    check(critical.value("risk").toObject().value("level").toString() == "CRITICAL" && critical.value("requiredEvidence").toArray().contains("ctest"), "schema has broad required evidence");
    const auto hardware = WorkerTaskServices::prepare(model, requestFor({"pico"}));
    check(hardware.value("risk").toObject().value("level").toString() == "HIGH" && hardware.value("requiredEvidence").toArray().contains("physical-certification"), "hardware requires physical evidence");
    auto unmapped = ui; unmapped.files = {"source/unmapped.cpp"};
    check(hasCode(WorkerTaskServices::prepare(model, unmapped), "TASK_SCOPE_VIOLATION"), "unmapped file blocked");
    auto unsafe = ui; unsafe.files = {"../outside.cpp"};
    check(WorkerTaskServices::prepare(model, unsafe).value("preflight").toObject().value("status").toString() == "FATAL", "traversal rejected");
    auto forbidden = ui; forbidden.files = {"ARAMF_WORKER/memory/event-log.jsonl"};
    check(!ready(WorkerTaskServices::prepare(model, forbidden)), "event history cannot be granted by intent");
    auto destructive = ui; destructive.destructive = true;
    check(!ready(WorkerTaskServices::prepare(model, destructive)), "destructive request does not invent authority");

    const auto initial = WorkerTaskServices::postflight(model, uiContract);
    check(initial.value("completionState").toString() == "IN_PROGRESS", "not verified without evidence");
    check(write(QDir(project.path()).filePath("source/ui.cpp"), "implemented\n"), "modify permitted file");
    auto post = WorkerTaskServices::postflight(model, uiContract);
    check(post.value("status").toString() == "PASS" && post.value("completionState").toString() == "IMPLEMENTED_UNVERIFIED", "implementation differs from verification");
    QJsonArray evidence;
    for (const auto& value : uiContract.value("requiredEvidence").toArray()) {
        if (value.toString() == "diff-boundary") continue;
        const QString path = "ARAMF_WORKER/verification/tasks/" + value.toString() + ".json";
        const QJsonObject record{{"check", value}, {"status", "PASS"}, {"contractId", uiContract.value("contractId")}, {"resultFingerprint", post.value("resultFingerprint")}};
        const QByteArray content = QJsonDocument(record).toJson();
        check(write(QDir(project.path()).filePath(path), content), "persist bound evidence fixture");
        evidence.append(QJsonObject{{"check", value}, {"artifact", path}, {"artifactFingerprint", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())}});
    }
    check(WorkerTaskServices::postflight(model, uiContract, evidence).value("completionState").toString() == "VERIFIED", "all bound software evidence permits VERIFIED");
    check(write(QDir(project.path()).filePath("source/ui.cpp"), "changed after validation\n"), "subsequent source change");
    check(WorkerTaskServices::postflight(model, uiContract, evidence).value("completionState").toString() == "IMPLEMENTED_UNVERIFIED", "stale evidence cannot verify later edits");
    const auto staleUi = WorkerTaskServices::postflight(model, uiContract, evidence);
    for (const QString& id : {QStringLiteral("affected-target-build"), QStringLiteral("ui-tests"), QStringLiteral("application-startup"), QStringLiteral("impacted-validation")})
        check(staleUi.value("missingEvidence").toArray().contains(id), "changed source invalidates " + id);
    auto tampered = uiContract; tampered.insert("permittedFiles", QJsonArray{"source/pico.cpp"});
    check(hasCode(WorkerTaskServices::postflight(model, tampered), "TASK_CONTRACT_TAMPERED"), "contract tampering rejected");
    check(write(QDir(project.path()).filePath("source/pico.cpp"), "out of scope\n"), "out-of-scope fixture edit");
    check(hasCode(WorkerTaskServices::postflight(model, uiContract), "TASK_SCOPE_VIOLATION"), "actual out-of-scope modification detected");
    check(write(QDir(project.path()).filePath("source/pico.cpp"), "before\n"), "restore test-owned source");
    // Baseline records actual bytes, including pre-existing user edits.
    uiContract = WorkerTaskServices::prepare(model, ui);
    check(WorkerTaskServices::postflight(model, uiContract).value("modifiedFiles").toArray().isEmpty(), "pre-existing modified bytes are not task changes");
    const QString logPath = QDir(project.path()).filePath("ARAMF_WORKER/memory/event-log.jsonl");
    const auto log = bytes(logPath);
    check(write(logPath, "fake history"), "test-only history corruption");
    check(hasCode(WorkerTaskServices::postflight(model, uiContract), "FORBIDDEN_FILE_MODIFICATION"), "event history rewriting detected");
    check(write(logPath, log), "restore test-owned history bytes");
    // Canonical recorder mutations remain legal when append-only history and
    // its derived cross-file consistency pass the existing memory validator.
    QString recorderError;
    check(ProjectMemory().recordOperation(project.path(), "test-result", QJsonObject{{"task", ui.goal}, {"status", "PASS"}, {"summary", "fixture evidence"}, {"provenance", QJsonObject{{"actor", "system"}, {"agentId", "system"}, {"tool", "WorkerTaskTests"}}}}, nullptr, &recorderError), "canonical recorder operation");
    check(!hasCode(WorkerTaskServices::postflight(model, uiContract), "FORBIDDEN_FILE_MODIFICATION"), "legitimate recorder writes preserve their ownership boundary");

    // Persistence: metadata-only changes notify, legacy absence clears old state,
    // and reloading a different project never inherits policy from the last one.
    ProjectPersistence persistence;
    auto saved = persistence.toJson(model);
    ProjectModel loaded;
    QString error;
    check(persistence.fromJson(&loaded, saved, &error) && loaded.ruleConfiguration().scopeMetadata == rules.scopeMetadata, "metadata round-trip");
    auto legacy = saved; auto oldRules = legacy.value("rules").toObject(); oldRules.remove("scopeMetadata"); legacy.insert("rules", oldRules);
    check(persistence.fromJson(&loaded, legacy, &error) && loaded.ruleConfiguration().scopeMetadata.isEmpty(), "legacy input clears prior mappings");
    check(persistence.fromJson(&loaded, saved, &error) && loaded.ruleConfiguration().scopeMetadata == rules.scopeMetadata, "A/B/A metadata isolation");
    const auto normalizedSaved = persistence.toJson(loaded);
    check(persistence.fromJson(&loaded, normalizedSaved, &error) && QJsonDocument(persistence.toJson(loaded)).toJson() == QJsonDocument(normalizedSaved).toJson(), "repeated normalized persistence is byte stable");
    auto malformed = normalizedSaved; auto malformedRules = malformed.value("rules").toObject(); malformedRules.insert("scopeMetadata", "invalid"); malformed.insert("rules", malformedRules);
    check(!persistence.fromJson(&loaded, malformed, &error) && persistence.toJson(loaded) == normalizedSaved, "malformed metadata rejected without changing loaded project");
    auto changedRules = model.ruleConfiguration(); auto changedMetadata = changedRules.scopeMetadata.value("ui").toObject(); changedMetadata.insert("tests", QJsonArray{"replacement-tests"}); changedRules.scopeMetadata.insert("ui", changedMetadata);
    model.setRuleConfiguration(changedRules);
    check(model.ruleConfiguration().scopeMetadata == changedRules.scopeMetadata, "metadata-only setter updates model");
    check(hasCode(WorkerTaskServices::prepare(model, ui), "STALE_DERIVED_ARTIFACT"), "changed canonical mapping invalidates generated routes");
    model.setRuleConfiguration(rules);

    ProjectResource resource;
    resource.id = "external-thesis"; resource.location = "external/template.md"; resource.scopes = {"thesis"}; resource.role = "thesis-template";
    model.setResources({resource}); generate(model);
    check(hasCode(WorkerTaskServices::prepare(model, requestFor({"thesis"})), "USER_SOURCE_MISSING"), "missing template blocked without fabrication");
    check(write(QDir(project.path()).filePath(resource.location), "canonical template\n"), "external template fixture");
    const auto thesisContract = WorkerTaskServices::prepare(model, requestFor({"thesis"}));
    check(ready(thesisContract), "available scoped external template");
    check(write(QDir(project.path()).filePath(resource.location), "unauthorized replacement\n"), "test-only external overwrite");
    check(hasCode(WorkerTaskServices::postflight(model, thesisContract), "FORBIDDEN_FILE_MODIFICATION"), "external overwrite detected");
    resource.role = "source-of-truth"; ProjectResource second = resource; second.id = "second-source"; second.location = "external/second.md";
    model.setResources({resource}); generate(model);
    const auto sourceContract = WorkerTaskServices::prepare(model, requestFor({"thesis"}));
    check(write(QDir(project.path()).filePath(resource.location), "changed authoritative source\n"), "Source of Truth mutation fixture");
    check(hasCode(WorkerTaskServices::postflight(model, sourceContract), "FORBIDDEN_FILE_MODIFICATION"), "Source of Truth cannot be modified by task");
    auto missingSource = resource; missingSource.location = "external/missing-source.md"; model.setResources({missingSource}); generate(model);
    check(hasCode(WorkerTaskServices::prepare(model, requestFor({"thesis"})), "SOURCE_OF_TRUTH_MISSING"), "missing Source of Truth blocks without fabrication");
    check(write(QDir(project.path()).filePath(second.location), "second\n"), "second source fixture");
    model.setResources({resource, second}); generate(model);
    check(hasCode(WorkerTaskServices::prepare(model, requestFor({"thesis"})), "RESOURCE_AUTHORITY_CONFLICT"), "overlapping Source of Truth conflict blocks");
    model.setResources({}); generate(model);
    // Dependency-aware evidence across a mixed-scope task.
    const auto collectEvidence = [&](const QJsonObject& contract, const QJsonObject& post, const QString& prefix) {
        QJsonArray collected;
        for (const auto& value : contract.value("requiredEvidence").toArray()) {
            if (value.toString() == "diff-boundary" || value.toString() == "physical-certification") continue;
            const QString path = "ARAMF_WORKER/verification/tasks/" + prefix + '-' + value.toString() + ".json";
            const QJsonObject record{{"check", value}, {"status", "PASS"}, {"contractId", contract.value("contractId")},
                {"dependencyFingerprint", post.value("evidenceFingerprints").toObject().value(value.toString())}, {"resultFingerprint", post.value("resultFingerprint")}};
            const QByteArray content = QJsonDocument(record).toJson();
            check(write(QDir(project.path()).filePath(path), content), "persist result-bound evidence");
            collected.append(QJsonObject{{"check", value}, {"artifact", path}, {"artifactFingerprint", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())}});
        }
        return collected;
    };
    const auto mixedContract = WorkerTaskServices::prepare(model, requestFor({"thesis", "report"}));
    const auto mixedPost = WorkerTaskServices::postflight(model, mixedContract);
    const auto mixedEvidence = collectEvidence(mixedContract, mixedPost, "mixed");
    check(WorkerTaskServices::postflight(model, mixedContract, mixedEvidence).value("completionState").toString() == "VERIFIED", "mixed task software verified");
    check(write(QDir(project.path()).filePath("source/report.cpp"), "report changed\n"), "later report edit");
    const auto changedMixed = WorkerTaskServices::postflight(model, mixedContract, mixedEvidence);
    bool thesisEvidenceRetained = false;
    for (const auto& value : changedMixed.value("acceptedEvidence").toArray()) thesisEvidenceRetained |= value.toObject().value("check").toString() == "thesis-tests";
    check(thesisEvidenceRetained && changedMixed.value("missingEvidence").toArray().contains("report-tests"), "Report edit stales Report evidence but preserves Thesis evidence");
    check(hasCode(changedMixed, "STALE_EVIDENCE"), "stale evidence has stable diagnostic");
    check(changedMixed.value("missingEvidence").toArray().contains("affected-workflow-tests"), "Report change stales shared workflow evidence");

    // Explicitly expected regeneration is accepted only if it matches the
    // canonical service's expected content. Mere generated ownership is not enough.
    auto regenerationRules = model.ruleConfiguration();
    auto uiMetadata = regenerationRules.scopeMetadata.value("ui").toObject();
    uiMetadata.insert("generatedArtifacts", QJsonArray{"ARAMF_WORKER/project.json", "ARAMF_WORKER/worker-manifest.json", "ARAMF_WORKER/routing/task-routes.json", "ARAMF_WORKER/routing/scope-routes.json", "ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json"});
    regenerationRules.scopeMetadata.insert("ui", uiMetadata); model.setRuleConfiguration(regenerationRules); generate(model);
    const QString projectJsonPath = QDir(project.path()).filePath("ARAMF_WORKER/project.json");
    const auto projectJson = QJsonDocument::fromJson(bytes(projectJsonPath));
    for (const auto& path : uiMetadata.value("generatedArtifacts").toArray()) {
        const auto absolute = QDir(project.path()).filePath(path.toString());
        check(write(absolute, QJsonDocument::fromJson(bytes(absolute)).toJson(QJsonDocument::Compact)), "equivalent pre-existing derived formatting: " + path.toString());
    }
    const auto regenerationContract = WorkerTaskServices::prepare(model, ui);
    check(ready(regenerationContract), "regeneration contract READY");
    const auto repaired = GenerationServices().repairDerivedArtifacts(model, model.generationOptions());
    check(repaired.success, "canonical repair runs");
    const auto regenerationPost = WorkerTaskServices::postflight(model, regenerationContract);
    if (regenerationPost.value("status").toString() != "PASS") std::cerr << QJsonDocument(regenerationPost).toJson().toStdString();
    check(regenerationPost.value("status").toString() == "PASS" && !regenerationPost.value("serviceChanges").toArray().isEmpty(), "expected canonical regeneration accepted");
    for (const auto& path : uiMetadata.value("generatedArtifacts").toArray()) {
        bool observed = false;
        for (const auto& change : regenerationPost.value("serviceChanges").toArray()) observed |= change.toObject().value("path") == path;
        check(observed, "canonical producer checked per artifact: " + path.toString());
    }
    const auto wrongGeneratedContract = WorkerTaskServices::prepare(model, ui);
    auto wrongGenerated = projectJson.object(); wrongGenerated.insert("forged", true);
    check(write(projectJsonPath, QJsonDocument(wrongGenerated).toJson()), "wrong generated content fixture");
    check(hasCode(WorkerTaskServices::postflight(model, wrongGeneratedContract), "GENERATED_CONTENT_MISMATCH"), "generated owner cannot authorize arbitrary content");
    check(GenerationServices().repairDerivedArtifacts(model, model.generationOptions()).success, "restore wrong-content fixture");
    const auto noRegenerationContract = WorkerTaskServices::prepare(model, requestFor({"thesis"}));
    check(write(projectJsonPath, projectJson.toJson(QJsonDocument::Compact)), "unrelated derived modification fixture");
    check(hasCode(WorkerTaskServices::postflight(model, noRegenerationContract), "UNEXPECTED_SERVICE_REGENERATION"), "undeclared generated change rejected");
    check(GenerationServices().repairDerivedArtifacts(model, model.generationOptions()).success, "restore canonical test fixture");

    const auto certify = [&](const QJsonObject& contract, const QJsonObject& post, const QString& level, const QString& prefix, bool wrongResult) {
        const QString path = "ARAMF_WORKER/verification/tasks/" + prefix + "-certification.json";
        const QByteArray content = QJsonDocument(QJsonObject{{"check", "certification"}, {"status", "PASS"}, {"contractId", contract.value("contractId")}, {"resultFingerprint", post.value("resultFingerprint")}}).toJson();
        check(write(QDir(project.path()).filePath(path), content), "physical/software certification evidence fixture");
        QJsonObject started; QString error;
        CertificationService certificates;
        check(certificates.start(project.path(), contract.value("contractId").toString(), "worker-task", "task", level, QJsonArray{"task-evidence"},
            QJsonObject{{"resultFingerprint", wrongResult ? QJsonValue("wrong-result") : post.value("resultFingerprint")}}, &started, &error), "canonical certificate start: " + error);
        const QJsonArray references{QJsonObject{{"reference", path}, {"fingerprint", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())},
            {"verified", true}, {"type", level == "HARDWARE_CERTIFIED" ? "physical" : "host-test"}}};
        check(certificates.issue(project.path(), started, "PASS", references, nullptr, &error), "canonical certificate issue: " + error);
        const auto context = ContextCoordinationService::generate(model);
        check(context.value(QStringLiteral("success")).toBool(), "refresh context after canonical certificate issue");
        return path;
    };
    const auto softwareContract = WorkerTaskServices::prepare(model, ui);
    const auto softwarePost = WorkerTaskServices::postflight(model, softwareContract);
    const auto softwareEvidence = collectEvidence(softwareContract, softwarePost, "software");
    check(WorkerTaskServices::postflight(model, softwareContract, softwareEvidence).value("completionState").toString() == "VERIFIED", "software VERIFIED before certification");
    certify(softwareContract, softwarePost, "HOST_TEST", "software", false);
    const auto softwareCertified = WorkerTaskServices::postflight(model, softwareContract, softwareEvidence);
    if (softwareCertified.value("completionState").toString() != "CERTIFIED") std::cerr << QJsonDocument(softwareCertified).toJson().toStdString();
    check(softwareCertified.value("completionState").toString() == "CERTIFIED", "software task CERTIFIED through canonical service");
    const auto physicalContract = WorkerTaskServices::prepare(model, requestFor({"pico"}));
    const auto physicalPost = WorkerTaskServices::postflight(model, physicalContract);
    const auto physicalEvidence = collectEvidence(physicalContract, physicalPost, "physical");
    const auto beforePhysical = WorkerTaskServices::postflight(model, physicalContract, physicalEvidence);
    check(beforePhysical.value("completionState").toString() == "VERIFIED" && beforePhysical.value("missingEvidence").toArray().contains("physical-certification"), "hardware software PASS cannot claim physical certification");
    const auto physicalPath = certify(physicalContract, physicalPost, "HARDWARE_CERTIFIED", "physical", false);
    check(WorkerTaskServices::postflight(model, physicalContract, physicalEvidence).value("completionState").toString() == "CERTIFIED", "hardware CERTIFIED with current canonical physical evidence");
    const auto oldCertification = model.certificationConfiguration(); auto changedCertification = oldCertification; changedCertification.defaultVerificationLevel = "PHYSICAL";
    model.setCertificationConfiguration(changedCertification);
    check(hasCode(WorkerTaskServices::postflight(model, physicalContract, physicalEvidence), "STALE_TASK_CONTRACT"), "certificate cannot validate a changed configuration");
    model.setCertificationConfiguration(oldCertification);
    auto physicalReadout = QJsonDocument::fromJson(bytes(QDir(project.path()).filePath(physicalPath))).object(); physicalReadout.insert("altered", true);
    check(write(QDir(project.path()).filePath(physicalPath), QJsonDocument(physicalReadout).toJson()), "alter physical evidence fixture");
    check(WorkerTaskServices::postflight(model, physicalContract, physicalEvidence).value("completionState").toString() == "VERIFIED", "stale physical certificate preserves software VERIFIED without CERTIFIED");
    const auto wrongContract = WorkerTaskServices::prepare(model, requestFor({"pico"}));
    const auto wrongPost = WorkerTaskServices::postflight(model, wrongContract);
    const auto wrongEvidence = collectEvidence(wrongContract, wrongPost, "wrong");
    certify(wrongContract, wrongPost, "HARDWARE_CERTIFIED", "wrong", true);
    check(WorkerTaskServices::postflight(model, wrongContract, wrongEvidence).value("completionState").toString() != "CERTIFIED", "wrong-result physical certification rejected");

    // Real Git dirty baseline, without creating any fixture commits.
    const auto runGit = [&](const QStringList& args) { QProcess command; command.start("git", QStringList{"-C", project.path()} + args); return command.waitForFinished(10000) && command.exitCode() == 0; };
    check(runGit({"init", "-q"}) && runGit({"add", "source"}), "Git fixture index initialized");
    check(write(QDir(project.path()).filePath("source/machine-learning.cpp"), "user dirty before prepare\n"), "unrelated pre-existing dirty file");
    const auto dirtyContract = WorkerTaskServices::prepare(model, ui);
    check(ready(dirtyContract) && WorkerTaskServices::postflight(model, dirtyContract).value("modifiedFiles").toArray().isEmpty(), "Git dirty baseline preserved and not attributed to task");
    check(write(QDir(project.path()).filePath("source/machine-learning.cpp"), "new unrelated task change\n"), "modify dirty unrelated file again");
    check(hasCode(WorkerTaskServices::postflight(model, dirtyContract), "TASK_SCOPE_VIOLATION"), "subsequent edit to already-dirty unrelated file detected");
    const auto permittedDirty = WorkerTaskServices::prepare(model, ui);
    check(write(QDir(project.path()).filePath("source/ui.cpp"), "allowed edit over pre-existing dirt\n"), "edit previously dirty permitted file");
    check(WorkerTaskServices::postflight(model, permittedDirty).value("status").toString() == "PASS", "pre-existing dirty permitted source remains mutable");
    auto ignoredRules = model.ruleConfiguration(); auto ignoredMetadata = ignoredRules.scopeMetadata.value("ui").toObject();
    ignoredMetadata.insert("files", QJsonArray{"source/ui.cpp", "ignored/input.h"}); ignoredRules.scopeMetadata.insert("ui", ignoredMetadata);
    model.setRuleConfiguration(ignoredRules); generate(model);
    QFile ignoredList(QDir(project.path()).filePath(".git/info/exclude"));
    check(ignoredList.open(QIODevice::Append) && ignoredList.write("\n/ignored/\n") > 0, "Git ignored dependency fixture"); ignoredList.close();
    check(write(QDir(project.path()).filePath("ignored/input.h"), "input A\n"), "ignored canonical input");
    const auto ignoredContract = WorkerTaskServices::prepare(model, ui);
    const auto ignoredBefore = WorkerTaskServices::postflight(model, ignoredContract);
    check(write(QDir(project.path()).filePath("ignored/input.h"), "input B\n"), "change ignored dependency");
    const auto ignoredAfter = WorkerTaskServices::postflight(model, ignoredContract);
    check(ignoredBefore.value("resultFingerprint") != ignoredAfter.value("resultFingerprint") && hasCode(ignoredAfter, "TASK_SCOPE_VIOLATION"), "ignored mapped dependencies cannot evade fingerprints or boundary checks");
    model.setRuleConfiguration(regenerationRules); generate(model);
    loaded.resetForNewProject(); check(loaded.ruleConfiguration().scopeMetadata.isEmpty(), "New Project clears task metadata");
    check(persistence.fromJson(&loaded, saved, &error), "reload A before template transition");
    TemplateManager templates;
    check(templates.applyTemplate(&loaded, "qt-desktop-application", &error) && loaded.ruleConfiguration().scopeMetadata.isEmpty(), "template transition clears former task metadata");
    QTemporaryDir otherProject;
    ProjectModel other; check(persistence.fromJson(&other, saved, &error), "load independent B");
    other.setProjectPath(otherProject.path()); other.setProjectId("task-test-B"); other.setWorkerNameSuffix("B");
    check(write(QDir(otherProject.path()).filePath("source/ui.cpp"), "B only\n"), "isolated B source"); generate(other);
    const auto otherContract = WorkerTaskServices::prepare(other, ui);
    check(ready(otherContract), "suffixed B preflight");
    check(hasCode(WorkerTaskServices::postflight(other, dirtyContract), "STALE_TASK_CONTRACT"), "A contract cannot authorize B");
    check(WorkerTaskServices::prepare(model, ui).value("binding").toObject().value("projectId").toString() == "task-test-A", "return to A restores only A identity");

    // Production CLI, including invalid preflight/postflight exit paths.
    model.setProjectFilePath(QDir(project.path()).filePath("project.aramf.json"));
    check(persistence.save(model, model.projectFilePath(), &error), "save canonical CLI model");
    ProjectModel cliModel; check(persistence.load(&cliModel, model.projectFilePath(), &error), "load canonical CLI model"); generate(cliModel);
    const QString requestPath = QDir(project.path()).filePath("task-request.json"); check(write(requestPath, QJsonDocument(ui.toJson()).toJson()), "CLI request fixture");
    const auto command = [&](const QStringList& args, QByteArray* output) {
        QProcess process; process.start(QDir(QCoreApplication::applicationDirPath()).filePath("aramf.exe"), args);
        if (!process.waitForFinished(30000)) return -1;
        *output = process.readAllStandardOutput(); return process.exitCode();
    };
    QByteArray cliOutput;
    check(command({"task", "prepare", "--config", model.projectFilePath(), "--request", requestPath}, &cliOutput) == 0, "production task prepare succeeds");
    const auto cliContract = QJsonDocument::fromJson(cliOutput).object().value("contract").toObject();
    const QString contractPath = QDir(project.path()).filePath("ARAMF_WORKER/verification/tasks/prepared.json"); check(write(contractPath, cliOutput), "persist derived CLI contract");
    check(command({"task", "postflight", "--config", model.projectFilePath(), "--contract", contractPath}, &cliOutput) == 2
        && QJsonDocument::fromJson(cliOutput).object().value("completionState").toString() != "VERIFIED", "CLI missing evidence returns nonzero");
    const auto cliEvidence = collectEvidence(cliContract, WorkerTaskServices::postflight(cliModel, cliContract), "cli");
    const auto evidencePath = QDir(otherProject.path()).filePath("cli-evidence-index.json");
    check(write(evidencePath, QJsonDocument(QJsonObject{{"evidence", cliEvidence}}).toJson()), "external caller evidence index fixture");
    check(command({"task", "postflight", "--config", model.projectFilePath(), "--contract", contractPath, "--evidence", evidencePath}, &cliOutput) == 0
        && QJsonDocument::fromJson(cliOutput).object().value("completionState").toString() == "VERIFIED", "production postflight returns success only for verified evidence");
    check(write(requestPath, QJsonDocument(unmapped.toJson()).toJson()), "CLI unmapped fixture");
    check(command({"task", "prepare", "--config", model.projectFilePath(), "--request", requestPath}, &cliOutput) == 2, "CLI blocked preflight returns nonzero");
    check(command({"task", "invalid", "--config", model.projectFilePath()}, &cliOutput) == 2, "CLI invalid command returns nonzero");
    check(command({"task", "prepare"}, &cliOutput) == 2, "CLI missing arguments returns nonzero");
    const QJsonObject ownershipContract{{"contractId", "ownership-contract-a"},
                                        {"binding", QJsonObject{{"projectId", model.projectId()}}},
                                        {"preflight", QJsonObject{{"status", "READY"}}},
                                        {"permittedFiles", QJsonArray{"source/ui.cpp"}}};
    const QJsonObject independentContract{{"contractId", "ownership-contract-c"},
                                          {"binding", QJsonObject{{"projectId", model.projectId()}}},
                                          {"preflight", QJsonObject{{"status", "READY"}}},
                                          {"permittedFiles", QJsonArray{"source/generic.cpp"}}};
    const auto claimA = RuntimeOwnershipService::claim(&model, ownershipContract, "task-a", "worker-a", {"source/ui.cpp"});
    check(claimA.value("granted").toBool() && claimA.value("code").toString() == "OWNERSHIP_GRANTED", "canonical runtime ownership grants permitted resource");
    const auto equivalentConflict = RuntimeOwnershipService::claim(&model, ownershipContract, "task-b", "worker-b", {"source/../source/ui.cpp"});
    check(!equivalentConflict.value("granted").toBool() && equivalentConflict.value("code").toString() == "OWNERSHIP_CONFLICT", "canonical ownership normalizes equivalent paths");
    const auto independentClaim = RuntimeOwnershipService::claim(&model, independentContract, "task-c", "worker-c", {"source/generic.cpp"});
    check(independentClaim.value("granted").toBool(), "independent canonical resource is claimable");
    check(RuntimeOwnershipService::release(&model, "task-a", "worker-a").value("code").toString() == "OWNERSHIP_RELEASED", "canonical ownership releases on success");
    check(RuntimeOwnershipService::release(&model, "task-a", "worker-a").value("code").toString() == "OWNERSHIP_ALREADY_RELEASED", "duplicate release is deterministic");
    check(RuntimeOwnershipService::markWorkerUnavailable(&model, "worker-c").value("code").toString() == "OWNERSHIP_RECOVERABLE", "worker loss makes claim recoverable");
    check(RuntimeOwnershipService::recoverWorker(&model, "worker-c").value("code").toString() == "OWNERSHIP_RECOVERED", "stale ownership is explicitly recovered");
    const QJsonObject wrongProject = ownershipContract;
    auto wrongBinding = wrongProject;
    wrongBinding.insert("binding", QJsonObject{{"projectId", "task-test-B"}});
    check(RuntimeOwnershipService::claim(&model, wrongBinding, "task-wrong-project", "worker-d", {"source/ui.cpp"}).value("code").toString() == "TASK_CONTRACT_INVALID", "ownership rejects a contract bound to another project");
    check(RuntimeOwnershipService::claim(&model, ownershipContract, "task-forbidden", "worker-e", {"source/not-permitted.cpp"}).value("code").toString() == "OWNERSHIP_NOT_PERMITTED", "ownership rejects a resource outside the contract");
    check(RuntimeOwnershipService::claim(&model, ownershipContract, "task-escape", "worker-f", {"../outside.cpp"}).value("code").toString() == "OWNERSHIP_NOT_PERMITTED", "ownership rejects a project-root escape");
    ProjectModel concurrentModel;
    concurrentModel.setProjectPath(project.path());
    concurrentModel.setProjectId(model.projectId() + "-concurrent");
    auto concurrentContract = independentContract;
    concurrentContract.insert("binding", QJsonObject{{"projectId", concurrentModel.projectId()}});
    std::atomic<int> grantedClaims{0};
    std::thread claimantA([&] { if (RuntimeOwnershipService::claim(&concurrentModel, concurrentContract, "parallel-a", "worker-a", {"source/generic.cpp"}).value("granted").toBool()) ++grantedClaims; });
    std::thread claimantB([&] { if (RuntimeOwnershipService::claim(&concurrentModel, concurrentContract, "parallel-b", "worker-b", {"source/generic.cpp"}).value("granted").toBool()) ++grantedClaims; });
    claimantA.join(); claimantB.join();
    check(grantedClaims == 1, "concurrent claims have one canonical winner");
    const QString ownershipPath = QDir(project.path()).filePath("ownership.aramf.json");
    ProjectPersistence ownershipPersistence;
    check(ownershipPersistence.save(model, ownershipPath), "runtime ownership persists");
    ProjectModel ownershipReload;
    check(ownershipPersistence.load(&ownershipReload, ownershipPath) && RuntimeOwnershipService::inspect(ownershipReload).value("claims").toArray().size() == 2, "released ownership history survives reload");
    const QString manifestPath = QDir(project.path()).filePath("ARAMF_WORKER/worker-manifest.json");
    const QByteArray manifest = bytes(manifestPath);
    check(write(QDir(project.path()).filePath("ARAMF_WORKER/duplicate/worker-manifest.json"), manifest), "duplicate manifest fixture");
    check(hasCode(WorkerTaskServices::prepare(model, ui), "CANONICAL_OWNER_DUPLICATE"), "parallel state blocked");
    for (const auto& scenario : QStringList{"exact", "delete", "fake", "ambiguous", "unauthorized", "escape", "existing"}) {
        QTemporaryDir relocationFixture; relocationFixture.setAutoRemove(false);
        const QString relocationRoot = relocationFixture.path() + "/project";
        QDir().mkpath(relocationRoot);
        ProjectModel relocated; relocated.setProjectPath(relocationRoot);
        relocated.setAiConfiguration(ai);
        auto relocationRules = relocated.ruleConfiguration();
        relocationRules.projectScopes = {"source-code"};
        relocationRules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray{"src/old.cpp", "src/new.cpp", "src/other.cpp"}},
            {"tests", QJsonArray{"relocation-tests"}}, {"riskTraits", QJsonArray{}}, {"affects", QJsonArray{}}, {"generatedArtifacts", QJsonArray{}}}}};
        relocated.setRuleConfiguration(relocationRules);
        check(write(relocationRoot + "/src/old.cpp", "original governed content\n"), "relocation fixture original");
        const auto runGit = [&](const QStringList& arguments) {
            QProcess process; process.setWorkingDirectory(relocationRoot); process.start("git", arguments);
            return process.waitForFinished(30000) && process.exitCode() == 0;
        };
        check(runGit({"init"}) && runGit({"add", "src/old.cpp"})
            && runGit({"-c", "user.name=Relocation Test", "-c", "user.email=relocation@example.invalid", "-c", "commit.gpgsign=false", "commit", "-m", "isolated relocation baseline"}), "relocation committed baseline");
        generate(relocated);
        WorkerTaskRequest move;
        move.goal = "Relocate the exact governed source"; move.type = "coding"; move.scopes = {"source-code"};
        move.files = {"src/old.cpp", "src/new.cpp", "src/other.cpp"}; move.definitionOfDone = {"Byte identity and scoped authorization"};
        if (scenario != "unauthorized") move.relocations = {QJsonObject{{"oldPath", "src/old.cpp"}, {"newPath", "src/new.cpp"}}};
        if (scenario == "ambiguous") move.relocations.append(QJsonObject{{"oldPath", "src/old.cpp"}, {"newPath", "src/other.cpp"}});
        if (scenario == "escape") move.relocations = {QJsonObject{{"oldPath", "src/old.cpp"}, {"newPath", "../escape.cpp"}}};
        if (scenario == "existing") write(relocationRoot + "/src/new.cpp", "original governed content\n");
        const auto prepared = WorkerTaskServices::prepare(relocated, move);
        if (scenario == "ambiguous" || scenario == "escape" || scenario == "existing") {
            check(!ready(prepared) && hasCode(prepared, "RELOCATION_UNPROVEN"), "reject " + scenario + " relocation at preflight");
            continue;
        }
        check(ready(prepared), "relocation preflight " + scenario);
        const QString target = scenario == "delete" ? relocationFixture.path() + "/preserved-outside-project.cpp" : relocationRoot + "/src/new.cpp";
        check(QFile::rename(relocationRoot + "/src/old.cpp", target), "isolated fixture move, never delete content");
        if (scenario == "fake") write(target, "unrelated replacement\n");
        const auto observed = WorkerTaskServices::postflight(relocated, prepared);
        if (scenario == "exact") {
            if (observed.value("status") != "PASS")
                QTextStream(stderr) << "Relocation postflight diagnostics: " << QJsonDocument(observed).toJson(QJsonDocument::Compact) << '\n';
            check(observed.value("status") == "PASS" && observed.value("relocations").toArray().size() == 1,
                  "authorized exact relocation accepted with audit record");
            check(observed.value("completionState") != "VERIFIED", "relocation alone cannot replace required test evidence");
            check(observed.value("relocations").toArray().first().toObject().value("contractId") == prepared.value("contractId"), "relocation bound to task identity");
        } else check(observed.value("status") == "FAIL" && hasCode(observed, "FORBIDDEN_FILE_MODIFICATION"), "reject " + scenario + " as unauthorized deletion");
    }
    QTemporaryDir folderTaskFixture; folderTaskFixture.setAutoRemove(false);
    ProjectModel folderTask; folderTask.setProjectPath(folderTaskFixture.path()); folderTask.setAiConfiguration(ai);
    auto folderRules = folderTask.ruleConfiguration(); folderRules.projectScopes = {"source-code"};
    folderRules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray{"bundle/allowed.cpp"}}, {"tests", QJsonArray{"folder-tests"}},
        {"generatedArtifacts", QJsonArray{"ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json"}}}}};
    folderTask.setRuleConfiguration(folderRules);
    write(folderTaskFixture.path() + "/bundle/allowed.cpp", "before\n");
    write(folderTaskFixture.path() + "/bundle/protected.cpp", "protected\n");
    ProjectResource bundle; bundle.id = "folder-task"; bundle.type = "folder"; bundle.location = "bundle"; bundle.scopes = {"source-code"};
    folderTask.setResources({bundle}); generate(folderTask);
    WorkerTaskRequest folderRequest; folderRequest.goal = "Modify one mapped folder child"; folderRequest.type = "coding";
    folderRequest.scopes = {"source-code"}; folderRequest.files = {"bundle/allowed.cpp"}; folderRequest.definitionOfDone = {"Only mapped child changes"};
    const auto folderContract = WorkerTaskServices::prepare(folderTask, folderRequest);
    check(ready(folderContract), "folder preflight accepts supported folder type");
    write(folderTaskFixture.path() + "/bundle/allowed.cpp", "after\n");
    ContextCoordinationService::generate(folderTask); VerificationServices().verify(folderTask, folderTask.generationOptions());
    const auto folderPostflight = WorkerTaskServices::postflight(folderTask, folderContract);
    if (folderPostflight.value("status") != "PASS") std::cerr << QJsonDocument(folderPostflight.value("errors").toArray()).toJson().toStdString();
    check(folderPostflight.value("status") == "PASS", "authorized child passes folder aggregate boundary");
    write(folderTaskFixture.path() + "/bundle/protected.cpp", "unauthorized\n");
    ContextCoordinationService::generate(folderTask); VerificationServices().verify(folderTask, folderTask.generationOptions());
    check(WorkerTaskServices::postflight(folderTask, folderContract).value("status") == "FAIL", "unauthorized folder child remains blocked");

    QTemporaryDir grantFixture; grantFixture.setAutoRemove(false);
    ProjectModel grantModel; grantModel.setProjectPath(grantFixture.path()); grantModel.setAiConfiguration(ai);
    WorkerTaskRequest grantTask;
    grantTask.goal = "Implement P6 Canonical Code Bank"; grantTask.type = "coding"; grantTask.scopes = {"source-code"};
    grantTask.files = {"CMakeLists.txt", "src/processes/P6/CodeBank.h", "src/processes/P6/CodeBank.cpp",
        "src/processes/P6/CodeBankStorage.cpp", "src/processes/P6/CodeBankVerification.cpp", "src/processes/P6/README.md", "tests/P6CodeBankTests.cpp"};
    grantTask.definitionOfDone = {"Exact approved P6 build registration"};
    auto grantRules = grantModel.ruleConfiguration(); grantRules.projectScopes = {"source-code"};
    grantRules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray::fromStringList(grantTask.files)}, {"tests", QJsonArray{"grant-tests"}},
        {"generatedArtifacts", QJsonArray{"ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json"}}}}};
    grantModel.setRuleConfiguration(grantRules);
    auto grantMemory = grantModel.memoryConfiguration(); grantMemory.validationOptions.append("cold-start-validation");
    grantModel.setMemoryConfiguration(grantMemory);
    write(grantFixture.path() + "/CMakeLists.txt", "# CMakeLists.txt\n");
    ProjectResource cmakeResource; cmakeResource.id = "cmake"; cmakeResource.location = "CMakeLists.txt";
    cmakeResource.role = "source-of-truth"; cmakeResource.scopes = {"source-code"}; grantModel.setResources({cmakeResource});
    generate(grantModel);
    const QString instruction = "Admin Morgan Lindbom explicitly authorizes override for isolated P6 grant test";
    const QString expires = QDateTime::currentDateTimeUtc().addSecs(3600).toString(Qt::ISODate);
    check(!ready(WorkerTaskServices::prepare(grantModel, grantTask)), "CMake mapping alone never grants protected write");
    check(WorkerTaskServices::issueP6BuildGrant(grantModel, grantTask, "unverified override", expires).value("status") == "BLOCKED", "unverified admin rejected");
    check(WorkerTaskServices::issueP6BuildGrant(grantModel, grantTask, instruction, "2000-01-01T00:00:00Z").value("status") == "BLOCKED", "expired issuance rejected");
    grantModel.setWorkerNameSuffix("other");
    check(WorkerTaskServices::issueP6BuildGrant(grantModel, grantTask, instruction, expires).value("status") == "BLOCKED", "named-worker authority cannot borrow default-worker grant");
    grantModel.setWorkerNameSuffix({});
    const auto issuedGrant = WorkerTaskServices::issueP6BuildGrant(grantModel, grantTask, instruction, expires);
    check(issuedGrant.value("status") == "PASS", "scoped grant issued through administrative workflow");
    check(issuedGrant.value("status") == "PASS", "direct explicit Admin grant branch remains valid");
    check(issuedGrant.value("registration").toString().contains("${ARAMF_CORE_SOURCES} src/core/WorkerTaskServices.h src/core/WorkerTaskServices.cpp"), "P6 exact test registration links WorkerTaskServices");
    const auto grant = issuedGrant.value("grant").toObject();
    grantTask.protectedGrantId = grant.value("grantId").toString();
    ContextCoordinationService::generate(grantModel); VerificationServices().verify(grantModel, grantModel.generationOptions());
    const auto grantContract = WorkerTaskServices::prepare(grantModel, grantTask);
    check(ready(grantContract), "valid exact grant permits protected CMake in preparation");
    auto forgedTask = grantTask; forgedTask.protectedGrantId = "forged-id";
    check(hasCode(WorkerTaskServices::prepare(grantModel, forgedTask), "PROTECTED_GRANT_INVALID"), "forged grant rejected");
    auto wrongTask = grantTask; wrongTask.goal += " unrelated";
    check(hasCode(WorkerTaskServices::prepare(grantModel, wrongTask), "PROTECTED_GRANT_INVALID"), "wrong task rejected");
    auto wrongPath = grantTask; wrongPath.files.append("LICENSE");
    check(hasCode(WorkerTaskServices::prepare(grantModel, wrongPath), "PROTECTED_GRANT_INVALID"), "wrong path set rejected");
    for (const auto& scenario : QStringList{"expired", "wrong-path", "wrong-operation", "wrong-worker"}) {
        auto invalidGrant = grant;
        const QString testId = "negative-" + scenario;
        invalidGrant.insert("grantId", testId);
        if (scenario == "expired") invalidGrant.insert("expiresAt", "2000-01-01T00:00:00Z");
        if (scenario == "wrong-path") invalidGrant.insert("path", "LICENSE");
        if (scenario == "wrong-operation") invalidGrant.insert("operation", "DELETE");
        if (scenario == "wrong-worker") invalidGrant.insert("workerIdentity", "ARAMF_WORKER_other");
        QString authorityError;
        check(ProjectMemory().recordAdministrativeOverride(grantFixture.path(), instruction, "isolated negative grant fixture",
            "Exercise fail-closed validation", "project", "test", {"CMakeLists.txt"}, {"P6"}, false,
            {{"protectedPathGrant", invalidGrant}}, nullptr, &authorityError), "negative audit fixture " + scenario);
        auto invalidTask = grantTask; invalidTask.protectedGrantId = testId;
        const auto invalidContract = WorkerTaskServices::prepare(grantModel, invalidTask);
        check(hasCode(invalidContract, "PROTECTED_GRANT_INVALID"), "reject " + scenario + " grant");
        check(hasCode(WorkerTaskServices::postflight(grantModel, invalidContract), "PROTECTED_GRANT_INVALID"), "postflight rejects " + scenario + " grant");
    }
    write(grantFixture.path() + "/CMakeLists.txt", "unapproved replacement\n");
    check(hasCode(WorkerTaskServices::prepare(grantModel, grantTask), "PROTECTED_GRANT_INVALID"), "grant cannot authorize arbitrary CMake bytes");
    check(hasCode(WorkerTaskServices::postflight(grantModel, grantContract), "PROTECTED_GRANT_INVALID"), "postflight rejects arbitrary CMake bytes");
    write(grantFixture.path() + "/CMakeLists.txt", "# CMakeLists.txt\n" + issuedGrant.value("registration").toString().toUtf8());
    ContextCoordinationService::generate(grantModel); VerificationServices().verify(grantModel, grantModel.generationOptions());
    const auto grantPostflight = WorkerTaskServices::postflight(grantModel, grantContract);
    if (grantPostflight.value("status") != "PASS") std::cerr << QJsonDocument(grantPostflight.value("errors").toArray()).toJson().toStdString();
    check(grantPostflight.value("status") == "PASS", "exact CMake edit accepted by postflight");
    check(WorkerTaskServices::consumeP6BuildGrant(grantModel, grantContract, {}).value("status") == "BLOCKED", "incomplete evidence cannot consume grant");
    QJsonArray grantEvidence;
    for (const auto& value : grantContract.value("requiredEvidence").toArray()) {
        if (value == "diff-boundary") continue;
        const QString path = "ARAMF_WORKER/verification/tasks/grant-" + value.toString() + ".json";
        const QJsonObject record{{"check", value}, {"contractId", grantContract.value("contractId")}, {"status", "PASS"},
            {"dependencyFingerprint", grantPostflight.value("evidenceFingerprints").toObject().value(value.toString())}};
        const auto content = QJsonDocument(record).toJson(); write(grantFixture.path() + '/' + path, content);
        grantEvidence.append(QJsonObject{{"check", value}, {"artifact", path},
            {"artifactFingerprint", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())}});
    }
    const auto consumed = WorkerTaskServices::consumeP6BuildGrant(grantModel, grantContract, grantEvidence);
    if (consumed.value("status") != "PASS") QTextStream(stderr) << QJsonDocument(consumed).toJson();
    check(consumed.value("status") == "PASS", "verified fixture consumes grant once");
    check(hasCode(WorkerTaskServices::prepare(grantModel, grantTask), "PROTECTED_GRANT_INVALID"), "consumed grant rejected in preparation");
    const auto consumedPostflight = WorkerTaskServices::postflight(grantModel, grantContract, grantEvidence);
    check(consumed.value("status") == "PASS" && consumedPostflight.value("completionState") == "VERIFIED", "exact uniquely consumed grant remains valid in read-only postflight");
    const auto consumedTree = treeBytes(grantFixture.path());
    check(consumed.value("status") == "PASS" && WorkerTaskServices::postflight(grantModel, grantContract, grantEvidence).value("completionState") == "VERIFIED"
        && treeBytes(grantFixture.path()) == consumedTree, "repeated consumed grant postflight never appends or consumes again");
    check(WorkerTaskServices::consumeP6BuildGrant(grantModel, grantContract, grantEvidence).value("status") == "BLOCKED", "replay consumption rejected");
    const auto cmakeBytes = bytes(grantFixture.path() + "/CMakeLists.txt");
    write(grantFixture.path() + "/CMakeLists.txt", cmakeBytes + "# drift\n");
    bool driftRejected = hasCode(WorkerTaskServices::postflight(grantModel, grantContract, grantEvidence), "PROTECTED_GRANT_INVALID");
    write(grantFixture.path() + "/CMakeLists.txt", cmakeBytes);
    const QString evidenceArtifact = grantEvidence.first().toObject().value("artifact").toString();
    const auto evidenceBytes = bytes(grantFixture.path() + '/' + evidenceArtifact);
    write(grantFixture.path() + '/' + evidenceArtifact, evidenceBytes + " ");
    driftRejected &= hasCode(WorkerTaskServices::postflight(grantModel, grantContract, grantEvidence), "PROTECTED_GRANT_INVALID");
    write(grantFixture.path() + '/' + evidenceArtifact, evidenceBytes);
    check(driftRejected, "consumed grant source or evidence drift is rejected");
    // Every malformed audit variation is isolated against the same valid
    // completed fixture. Only temporary fixture bytes are restored afterward.
    const auto restoreConsumedFixture = [&]() {
        for (auto it = consumedTree.begin(); it != consumedTree.end(); ++it) write(grantFixture.path() + '/' + it.key(), it.value());
    };
    QString consumeError; QJsonObject consumedEvent;
    for (const auto& event : ProjectMemory().events(grantFixture.path(), &consumeError))
        if (event.value("eventType") == "PROTECTED_GRANT_CONSUMED") consumedEvent = event;
    bool bindingRejected = !consumedEvent.isEmpty();
    for (const auto& field : QStringList{"contractId", "contractFingerprint", "requestFingerprint", "authorityEventId", "authorityEventFingerprint",
             "grantFingerprint", "resultFingerprint", "postflightFingerprint", "eventPrefixFingerprint"}) {
        auto badEvent = consumedEvent; badEvent.insert(field, "wrong-independent-binding");
        QByteArray changedLog;
        for (const auto& line : consumedTree.value("ARAMF_WORKER/memory/event-log.jsonl").split('\n')) {
            if (line.trimmed().isEmpty()) continue;
            auto event = QJsonDocument::fromJson(line).object();
            if (event.value("eventId") == consumedEvent.value("eventId")) changedLog += QJsonDocument(badEvent).toJson(QJsonDocument::Compact) + '\n';
            else changedLog += line + '\n';
        }
        write(grantFixture.path() + "/ARAMF_WORKER/memory/event-log.jsonl", changedLog);
        bindingRejected &= hasCode(WorkerTaskServices::postflight(grantModel, grantContract, grantEvidence), "PROTECTED_GRANT_INVALID");
        restoreConsumedFixture();
    }
    auto duplicateConsume = consumedEvent;
    for (const auto& field : QStringList{"eventId", "sequence", "timestamp", "eventType", "task"}) duplicateConsume.remove(field);
    ProjectMemory().appendEvent(grantFixture.path(), "PROTECTED_GRANT_CONSUMED", "isolated duplicate consumption negative", duplicateConsume, &consumeError);
    bindingRejected &= hasCode(WorkerTaskServices::postflight(grantModel, grantContract, grantEvidence), "PROTECTED_GRANT_INVALID");
    restoreConsumedFixture();
    check(bindingRejected && WorkerTaskServices::postflight(grantModel, grantContract, grantEvidence).value("completionState") == "VERIFIED",
        "consumed grant rejects each incorrect binding and duplicate event");

    // Exercise an approved reauthorization from immutable origin records through successor postflight.
    QTemporaryDir reauthFixture;
    ProjectModel reauthModel;
    reauthModel.setProjectPath(reauthFixture.path());
    reauthModel.setProjectId("reauthorization-test");
    auto reauthAi = reauthModel.aiConfiguration();
    reauthAi.permissions = {"read-project-files", "modify-files", "create-files", "run-project-tools", "run-tests", "run-builds"};
    reauthModel.setAiConfiguration(reauthAi);
    auto reauthMemory = reauthModel.memoryConfiguration();
    reauthMemory.maintenanceOptions = {"record-test-results", "update-current-state"};
    reauthModel.setMemoryConfiguration(reauthMemory);
    check(write(reauthFixture.path() + "/source/ui.cpp", "before ui\n"), "write reauthorization ui source");
    check(write(reauthFixture.path() + "/source/other.cpp", "before other\n"), "write reauthorization other source");
    auto reauthRules = reauthModel.ruleConfiguration();
    reauthRules.projectScopes = {"ui", "other"};
    reauthRules.scopeMetadata = {
        {"ui", QJsonObject{{"files", QJsonArray{"source/ui.cpp"}}, {"tests", QJsonArray{"ui-tests"}}}},
        {"other", QJsonObject{{"files", QJsonArray{"source/other.cpp"}}, {"tests", QJsonArray{"other-tests"}}}}};
    reauthModel.setRuleConfiguration(reauthRules);
    check(write(reauthFixture.path() + "/.gitignore", "build/\n"), "ignore isolated immutable check artifacts");
    generate(reauthModel);
    const auto git = [&](const QStringList& args) {
        QProcess process;
        process.start("git", QStringList{"-C", reauthFixture.path()} + args);
        return process.waitForStarted() && process.waitForFinished() && process.exitCode() == 0;
    };
    check(git({"init"}), "initialize isolated reauthorization repository");
    check(git({"config", "user.email", "worker-task-tests@example.invalid"}), "configure isolated git email");
    check(git({"config", "user.name", "Worker Task Tests"}), "configure isolated git name");
    check(git({"add", "-A"}) && git({"commit", "-m", "fixture baseline"}), "commit isolated reauthorization baseline");
    WorkerTaskRequest originTask;
    originTask.goal = "Implement the ui workflow change";
    originTask.type = "coding";
    originTask.scopes = {"ui"};
    originTask.files = {"source/ui.cpp"};
    originTask.definitionOfDone = {"The ui behavior is implemented and validated."};
    const auto originContract = WorkerTaskServices::prepare(reauthModel, originTask);
    check(ready(originContract), "reauthorization origin contract is READY");
    check(write(reauthFixture.path() + "/source/ui.cpp", "after ui\n"), "edit authorized source for origin report");
    check(write(reauthFixture.path() + "/source/other.cpp", "after other\n"), "edit out-of-scope source for origin report");
    const auto originReport = WorkerTaskServices::postflight(reauthModel, originContract);
    check(originReport.value("status") == "FAIL", "origin postflight records scope failure");
    const QByteArray originContractBytes = QJsonDocument(originContract).toJson(QJsonDocument::Indented);
    const QByteArray originReportBytes = QJsonDocument(originReport).toJson(QJsonDocument::Indented);
    const QByteArray originBaselineBytes = QJsonDocument(originContract.value("baseline").toObject()).toJson(QJsonDocument::Indented);
    const QByteArray originEvidenceBytes = QJsonDocument(originReport.value("acceptedEvidence").toArray()).toJson(QJsonDocument::Indented);
    QTemporaryDir reauthArchive;
    const QString originContractPath = reauthArchive.path() + "/origin-contract-record.json";
    const QString originReportPath = reauthArchive.path() + "/origin-postflight-record.json";
    const QString originBaselinePath = reauthArchive.path() + "/origin-baseline-record.json";
    const QString originEvidencePath = reauthArchive.path() + "/origin-evidence-record.json";
    check(write(originContractPath, originContractBytes) && write(originReportPath, originReportBytes)
          && write(originBaselinePath, originBaselineBytes) && write(originEvidencePath, originEvidenceBytes),
          "persist byte-stability test copies of original records");
    const QJsonObject grantOutputs{{"ARAMF_WORKER/project.json", "REGENERATE"},
        {"ARAMF_WORKER/worker-manifest.json", "REGENERATE"},
        {"ARAMF_WORKER/routing/scope-routes.json", "REGENERATE"},
        {"ARAMF_WORKER/routing/task-routes.json", "REGENERATE"},
        {"ARAMF_WORKER/context/context-index.json", "REGENERATE"},
        {"ARAMF_WORKER/context/compressed-context.json", "REGENERATE"},
        {"ARAMF_WORKER/context/freshness.json", "REGENERATE"},
        {"ARAMF_WORKER/context/agent-adapters.json", "REGENERATE"},
        {"ARAMF_WORKER/context/task-dag.json", "REGENERATE"},
        {"ARAMF_WORKER/verification/latest-validation.json", "REGENERATE"},
        {"ARAMF_WORKER/verification/verification-result.json", "REGENERATE"},
        {"ARAMF_WORKER/verification/generation-state.json", "REGENERATE"}};
    TaskReauthorizationRequest reauthRequest;
    reauthRequest.task.goal = "Reauthorize the exact existing ui and other source edits";
    reauthRequest.task.type = "coding";
    reauthRequest.task.scopes = {"ui", "other"};
    reauthRequest.task.files = {"source/ui.cpp", "source/other.cpp"};
    reauthRequest.task.definitionOfDone = {"Only the reviewed source edits and canonical refreshes are accepted."};
    for (auto it = grantOutputs.begin(); it != grantOutputs.end(); ++it) {
        reauthRequest.canonicalInputs.append(it.key());
        reauthRequest.operations.insert(it.key(), it.value());
    }
    reauthRequest.operations.insert("source/ui.cpp", "MODIFY");
    reauthRequest.operations.insert("source/other.cpp", "MODIFY");
    reauthRequest.validityMinutes = 60;
    const QString reauthInstruction = "Admin Morgan Lindbom explicitly authorizes override for the isolated reauthorization test";
    auto reauthApproval = WorkerTaskServices::authorizeReauthorization(reauthModel, originContract, originReport,
        reauthRequest, reauthInstruction);
    check(reauthApproval.value("status") == "PASS", "exact reauthorization authority is appended");
    auto wrongPreparationOrigin = originContract;
    wrongPreparationOrigin.insert("completionState", "different-origin");
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), wrongPreparationOrigin, originReport),
          "REAUTHORIZATION_BINDING_MISMATCH"), "unused approval rejects a different original contract");
    auto wrongPreparationReport = originReport;
    wrongPreparationReport.insert("completionState", "different-report");
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), originContract, wrongPreparationReport),
          "REAUTHORIZATION_BINDING_MISMATCH"), "unused approval rejects a different original report");
    check(write(reauthFixture.path() + "/source/ui.cpp", "changed after approval\n"), "alter reviewed source fixture");
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), originContract, originReport),
          "REAUTHORIZATION_SOURCE_CHANGED"), "unused approval rejects a changed source fingerprint");
    check(write(reauthFixture.path() + "/source/ui.cpp", "after ui\n"), "restore reviewed fixture bytes");
    QLockFile heldBindingLock(QDir::tempPath() + "/aramf-reauthorization-"
        + QString::fromLatin1(QCryptographicHash::hash(QDir(reauthFixture.path()).canonicalPath().toUtf8(),
            QCryptographicHash::Sha256).toHex()) + ".lock");
    check(heldBindingLock.tryLock(0), "hold the independent reauthorization binding lock");
    const auto beforeBusyPreparation = treeBytes(reauthFixture.path());
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), originContract, originReport), "REAUTHORIZATION_BUSY")
          && treeBytes(reauthFixture.path()) == beforeBusyPreparation, "concurrent binding is blocked without consuming authority");
    heldBindingLock.unlock();
    auto reauthContract = WorkerTaskServices::prepareReauthorized(reauthModel,
        reauthApproval.value("authorizationId").toString(), originContract, originReport);
    if (!ready(reauthContract))
        std::cerr << "REAUTH PREFLIGHT: " << QJsonDocument(reauthContract.value("preflight").toObject().value("errors").toArray()).toJson().toStdString() << '\n';
    check(ready(reauthContract), "authorized successor contract is READY");
    const auto approvedEventList = ProjectMemory().events(reauthFixture.path());
    QJsonObject persistedGrant;
    for (const auto& event : approvedEventList)
        if (event.value("taskReauthorizationGrant").toObject().value("authorizationId") == reauthApproval.value("authorizationId"))
            persistedGrant = event.value("taskReauthorizationGrant").toObject();
    check(persistedGrant.value("originContractSnapshot") == originContract
          && persistedGrant.value("originPostflightSnapshot") == originReport
          && persistedGrant.value("originEvidenceSnapshot") == originReport.value("acceptedEvidence"),
          "grant retains byte-stable origin contract, baseline, report and evidence snapshots");
    check(QJsonDocument(originContract).toJson(QJsonDocument::Indented) == originContractBytes
          && QJsonDocument(originReport).toJson(QJsonDocument::Indented) == originReportBytes
          && bytes(originContractPath) == originContractBytes && bytes(originReportPath) == originReportBytes
          && bytes(originBaselinePath) == originBaselineBytes && bytes(originEvidencePath) == originEvidenceBytes,
          "authorization and preparation preserve original contract, baseline, report, and evidence bytes");
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), originContract, originReport), "REAUTHORIZATION_INVALID"),
          "consumed approval cannot be replayed for preparation");
    const auto refreshed = GenerationServices().generate(reauthModel, reauthModel.generationOptions());
    check(refreshed.success, "governance fixture canonical Generate succeeds: " + refreshed.error);
    const auto refreshedContext = ContextCoordinationService::generate(reauthModel);
    check(refreshedContext.value("success").toBool(), "governance fixture context refresh succeeds");
    const auto refreshedVerification = VerificationServices().verify(reauthModel, reauthModel.generationOptions());
    check(refreshedVerification.overallStatus == VerificationStatus::Pass, "governance fixture Verify succeeds");
    const auto acceptedReauthPostflight = WorkerTaskServices::postflight(reauthModel, reauthContract);
    if (acceptedReauthPostflight.value("status") != "PASS")
        std::cerr << "REAUTH POSTFLIGHT: " << QJsonDocument(acceptedReauthPostflight.value("errors").toArray()).toJson().toStdString() << '\n';
    check(acceptedReauthPostflight.value("status") == "PASS",
          "unique audit-bound reauthorization is accepted by postflight");
    const auto stableReauthTree = treeBytes(reauthFixture.path());
    check(WorkerTaskServices::postflight(reauthModel, reauthContract) == acceptedReauthPostflight
          && WorkerTaskServices::postflight(reauthModel, reauthContract) == acceptedReauthPostflight
          && treeBytes(reauthFixture.path()) == stableReauthTree,
          "repeated valid postflight is read-only and never consumes the bound approval again");

    const auto producerPass = WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check");
    if (producerPass.value("status") != "PASS")
        std::cerr << "PRODUCER: " << QJsonDocument(producerPass).toJson().toStdString() << '\n';
    check(producerPass.value("status") == "PASS", "canonical producer executes real git whitespace checks");
    const auto producedEntries = producerPass.value("evidence").toArray();
    const auto producedEntry = producedEntries.isEmpty() ? QJsonObject{} : producedEntries.first().toObject();
    const QString producedPath = reauthFixture.path() + '/' + producedEntry.value("artifact").toString();
    const QByteArray producedBytes = bytes(producedPath);
    check(!producedBytes.isEmpty() && producerPass.value("record").toObject().value("observations").toArray().size() == 2
          && producerPass.value("record").toObject().value("dependencyFingerprint")
              == acceptedReauthPostflight.value("evidenceFingerprints").toObject().value("git-diff-check"),
          "producer persists command observations and internally derived dependency binding");
    check(WorkerTaskServices::postflight(reauthModel, reauthContract, producedEntries).value("acceptedEvidence").toArray() == producedEntries,
          "read-back producer evidence satisfies only its actual check");
    const auto producerTree = treeBytes(reauthFixture.path());
    check(WorkerTaskServices::runCheck(reauthModel, reauthContract, "acceptance-criteria").value("status") == "BLOCKED"
          && WorkerTaskServices::runCheck(reauthModel, reauthContract, "arbitrary-exit-zero").value("status") == "BLOCKED"
          && WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check", {}, 0).value("status") == "BLOCKED"
          && treeBytes(reauthFixture.path()) == producerTree,
          "unsupported semantic acceptance, arbitrary checks and invalid timeout cannot create PASS evidence");
    auto tamperedProducerContract = reauthContract;
    tamperedProducerContract.insert("completionState", "forged");
    check(WorkerTaskServices::runCheck(reauthModel, tamperedProducerContract, "git-diff-check").value("status") == "BLOCKED"
          && treeBytes(reauthFixture.path()) == producerTree, "tampered contract cannot start evidence execution");
    const auto producerAgain = WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check");
    const auto collected = WorkerTaskServices::checkEvidence(reauthModel, reauthContract);
    check(producerAgain.value("status") == "PASS" && producerAgain.value("index") != producerPass.value("index")
          && bytes(producedPath) == producedBytes && collected.value("status") == "PASS"
          && collected.value("evidence") == producerAgain.value("evidence"),
          "latest immutable observation is selected while previous exact bytes remain intact");
    check(write(producedPath, producedBytes + " "), "alter isolated producer record for integrity negative");
    check(WorkerTaskServices::checkEvidence(reauthModel, reauthContract).value("status") == "BLOCKED"
          && hasCode(WorkerTaskServices::postflight(reauthModel, reauthContract, producedEntries), "EVIDENCE_INTEGRITY_INVALID"),
          "tampered artifact is rejected by collector and postflight independently");
    check(write(producedPath, producedBytes), "restore isolated producer fixture bytes");
    check(write(reauthFixture.path() + "/source/ui.cpp", "after ui \n"), "introduce isolated actual whitespace failure");
    check(ContextCoordinationService::generate(reauthModel).value("success").toBool()
          && VerificationServices().verify(reauthModel, reauthModel.generationOptions()).overallStatus == VerificationStatus::Pass,
          "refresh fixture governance independently of whitespace validation");
    check(hasCode(WorkerTaskServices::postflight(reauthModel, reauthContract, producedEntries), "STALE_EVIDENCE"),
          "changing covered source stales previously real producer evidence");
    const auto producerFailure = WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check");
    check(producerFailure.value("status") == "FAIL" && !producerFailure.value("evidence").toArray().isEmpty(),
          "actual command failure is persisted as FAIL with evidence rather than discarded or promoted");
    const auto failureRecord = producerFailure.value("record").toObject();
    check(failureRecord.value("observations").toArray().first().toObject().value("exitCode").toInt() != 0
          && !failureRecord.value("observations").toArray().first().toObject().value("stdoutBase64").toString().isEmpty(),
          "failed command retains real nonzero exit and raw output bytes");
    check(write(reauthFixture.path() + "/source/ui.cpp", "after ui\n"), "correct isolated whitespace regression");
    check(ContextCoordinationService::generate(reauthModel).value("success").toBool()
          && VerificationServices().verify(reauthModel, reauthModel.generationOptions()).overallStatus == VerificationStatus::Pass,
          "refresh fixture after actual correction");
    const auto correctedProducer = WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check");
    check(correctedProducer.value("status") == "PASS" && bytes(producedPath) == producedBytes
          && WorkerTaskServices::checkEvidence(reauthModel, reauthContract).value("evidence") == correctedProducer.value("evidence"),
          "corrected check has a new observation while original pass and failure artifacts remain retained");
    const QString coldPath = reauthFixture.path() + "/ARAMF_WORKER/memory/cold-start-validation.json";
    auto historicalColdReport = QJsonDocument::fromJson(bytes(coldPath)).object();
    historicalColdReport.insert("checkedAt", "2000-01-01T00:00:00Z");
    check(write(coldPath, QJsonDocument(historicalColdReport).toJson()), "retain old PASS cold-start timestamp in isolated fixture");
    const auto liveColdCheck = WorkerTaskServices::runCheck(reauthModel, reauthContract, "cold-start");
    const auto liveColdObservations = liveColdCheck.value("record").toObject().value("observations").toArray();
    check(liveColdCheck.value("status") == "PASS" && !liveColdObservations.isEmpty()
          && liveColdObservations.first().toObject().value("report").toObject().value("checkedAt") != "2000-01-01T00:00:00Z",
          "cold-start producer reconstructs current state rather than republishing historical PASS");
    const QString disabledBuild = reauthFixture.path() + "/build/disabled";
    check(write(disabledBuild + "/CMakeCache.txt", ("CMAKE_HOME_DIRECTORY:INTERNAL=" + reauthFixture.path()
          + "\nBUILD_TESTING:BOOL=OFF\n").toUtf8()) && write(disabledBuild + "/aramf_core_tests.exe", "stale executable sentinel"),
          "create isolated disabled test configuration with leftover executable");
    const auto beforeDisabled = treeBytes(reauthFixture.path());
    const auto disabledEvidence = WorkerTaskServices::runCheck(reauthModel, reauthContract, "ctest", "build/disabled");
    check(disabledEvidence.value("status") == "BLOCKED" && disabledEvidence.value("error").toString().contains("enable tests")
          && treeBytes(reauthFixture.path()) == beforeDisabled,
          "disabled test configuration cannot run or attest leftover test binaries");
    const QString orphanDirectory = QFileInfo(producedPath).absolutePath() + "/../orphan-failure";
    check(write(orphanDirectory + "/record.json", QJsonDocument(failureRecord).toJson()), "retain isolated unindexed failure observation");
    check(WorkerTaskServices::checkEvidence(reauthModel, reauthContract).value("status") == "BLOCKED",
          "collector cannot skip an orphan failure to select an older PASS");
    // This negative fixture remains retained. Subsequent audit tests do not
    // collect its evidence and therefore cannot mistake it for a valid index.

    // Every case gets new append-only fixture events and a unique authorization.
    // Start from a complete valid audit chain, changing exactly the field under
    // test. No original contract, baseline, report or audit event is rewritten.
    using Mutation = std::function<void(QJsonObject&)>;
    const Mutation unchanged = [](QJsonObject&) {};
    struct AuditChain { QJsonObject contract, grant, approval, validation, bound; QString id; };
    int chainNumber = 0;
    const auto appendFixtureEvent = [&](const QString& type, QJsonObject fields) {
        for (const auto& key : {"eventId", "sequenceNumber", "timestamp", "eventType", "task"}) fields.remove(key);
        QString error;
        check(ProjectMemory().appendEvent(reauthFixture.path(), type, "isolated reauthorization negative fixture", fields, &error),
              "append isolated " + type + " fixture: " + error);
        return ProjectMemory().events(reauthFixture.path()).last();
    };
    QJsonObject approvalTemplate, validationTemplate, boundTemplate;
    for (const auto& event : approvedEventList) {
        if (event.value("taskReauthorizationGrant").toObject().value("authorizationId") == reauthApproval.value("authorizationId"))
            approvalTemplate = event;
        if (event.value("authorizationId") != reauthApproval.value("authorizationId")) continue;
        if (event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED") validationTemplate = event;
        if (event.value("eventType") == "TASK_REAUTHORIZATION_BOUND") boundTemplate = event;
    }
    const auto auditChain = [&](const Mutation& mutateGrant, const Mutation& mutateApproval,
                               const Mutation& mutateValidation, const Mutation& mutateContract,
                               const Mutation& mutateBinding, bool bind = true, bool approve = true) {
        AuditChain chain;
        chain.id = "independent-reauthorization-" + QString::number(++chainNumber);
        chain.grant = persistedGrant;
        chain.grant.insert("authorizationId", chain.id);
        mutateGrant(chain.grant);
        chain.approval = approvalTemplate;
        chain.approval.insert("taskReauthorizationGrant", chain.grant);
        mutateApproval(chain.approval);
        if (approve) chain.approval = appendFixtureEvent("ADMIN_OVERRIDE", chain.approval);
        else chain.approval.insert("eventId", "missing-approval-event");
        chain.validation = validationTemplate;
        chain.validation.insert("authorizationId", chain.id);
        chain.validation.insert("authorityEventId", chain.approval.value("eventId"));
        mutateValidation(chain.validation);
        chain.validation = appendFixtureEvent("TASK_REAUTHORIZATION_AUTHORIZED", chain.validation);
        chain.contract = reauthContract;
        auto link = chain.contract.value("reauthorization").toObject();
        link.insert("authorizationId", chain.id);
        link.insert("authorityEventId", chain.approval.value("eventId"));
        chain.contract.insert("reauthorization", link);
        auto request = chain.contract.value("request").toObject();
        request.insert("reauthorizationId", chain.id);
        chain.contract.insert("request", request);
        mutateContract(chain.contract);
        checksumFixtureContract(chain.contract);
        chain.bound = boundTemplate;
        chain.bound.insert("authorizationId", chain.id);
        chain.bound.insert("authorityEventId", chain.approval.value("eventId"));
        chain.bound.insert("contractId", chain.contract.value("contractId"));
        mutateBinding(chain.bound);
        if (bind) chain.bound = appendFixtureEvent("TASK_REAUTHORIZATION_BOUND", chain.bound);
        return chain;
    };
    const auto validChain = [&] { return auditChain(unchanged, unchanged, unchanged, unchanged, unchanged); };
    const auto auditPostflight = [&](const QJsonObject& contract) {
        // Audit appends legitimately change current-state context. Refresh its
        // canonical producers so stale derived state cannot mask this case.
        check(ContextCoordinationService::generate(reauthModel).value("success").toBool(),
              "refresh isolated audit fixture context");
        check(VerificationServices().verify(reauthModel, reauthModel.generationOptions()).overallStatus
                  == VerificationStatus::Pass, "verify isolated audit fixture canonical state");
        return WorkerTaskServices::postflight(reauthModel, contract);
    };
    const auto rejectedChain = [&](const AuditChain& chain, const QString& code, const QString& name) {
        const auto report = auditPostflight(chain.contract);
        check(report.value("status") == "FAIL" && hasCode(report, code)
              && !hasCode(report, "TASK_CONTRACT_TAMPERED"), name);
    };
    const auto controlChain = validChain();
    const auto controlReport = auditPostflight(controlChain.contract);
    if (controlReport.value("status") != "PASS")
        std::cerr << "CONTROL POSTFLIGHT: " << QJsonDocument(controlReport.value("errors").toArray()).toJson().toStdString() << '\n';
    check(controlReport.value("status") == "PASS",
          "independent audit-chain fixture control passes postflight");

    const auto unusedRevokedChain = auditChain(unchanged, unchanged, unchanged, unchanged, unchanged, false);
    appendFixtureEvent("TASK_REAUTHORIZATION_REVOKED", QJsonObject{{"authorizationId", unusedRevokedChain.id}});
    const auto revokedPreparation = WorkerTaskServices::prepareReauthorized(reauthModel,
        unusedRevokedChain.id, originContract, originReport);
    check(hasCode(revokedPreparation, "REAUTHORIZATION_INVALID")
          && revokedPreparation.value("preflight").toObject().value("errors").toArray().first().toObject()
              .value("message").toString().contains("revoked"), "revoked unused grant is independently rejected before preparation");

    const auto unboundChain = auditChain(unchanged, unchanged, unchanged, unchanged, unchanged, false);
    rejectedChain(unboundChain, "REAUTHORIZATION_AUDIT_INVALID", "missing unique binding is independently rejected");
    const auto unapprovedChain = auditChain(unchanged, unchanged, unchanged, unchanged, unchanged, true, false);
    rejectedChain(unapprovedChain, "REAUTHORIZATION_AUDIT_INVALID", "missing approval for an unknown grant is independently rejected");
    const auto expiredChain = auditChain([](QJsonObject& grant) {
        grant.insert("issuedAt", "2000-01-01T00:00:00Z");
        grant.insert("expiresAt", "2000-01-01T01:00:00Z");
    }, unchanged, unchanged, unchanged, unchanged);
    rejectedChain(expiredChain, "REAUTHORIZATION_AUDIT_INVALID", "complete expired audit chain is independently rejected");
    for (const auto& field : {"originContractId", "originContractFingerprint", "originPostflightFingerprint", "authorityEventId"}) {
        const auto chain = auditChain(unchanged, unchanged, [&](QJsonObject& validation) {
            validation.insert(field, "wrong-original-record");
        }, unchanged, unchanged);
        rejectedChain(chain, "REAUTHORIZATION_AUDIT_INVALID", "authorization event independently rejects wrong " + QString(field));
    }
    const auto identityChain = auditChain(unchanged, [](QJsonObject& approval) {
        approval.insert("administrator", "Different Administrator");
    }, unchanged, unchanged, unchanged);
    rejectedChain(identityChain, "REAUTHORIZATION_AUDIT_INVALID", "wrong administrator is independently rejected");
    const auto projectChain = auditChain([](QJsonObject& grant) {
        grant.insert("projectId", "different-project");
    }, unchanged, unchanged, unchanged, unchanged);
    rejectedChain(projectChain, "REAUTHORIZATION_AUDIT_INVALID", "grant for a different project is independently rejected");
    for (const auto& field : {"originContractId", "originContractFingerprint", "originPostflightFingerprint", "contractId"}) {
        const auto chain = auditChain(unchanged, unchanged, unchanged, unchanged, [&](QJsonObject& binding) {
            binding.insert(field, "wrong-bound-record");
        });
        rejectedChain(chain, "REAUTHORIZATION_AUDIT_INVALID", "binding event independently rejects wrong " + QString(field));
    }
    ProjectModel wrongProjectModel;
    wrongProjectModel.setProjectPath(reauthFixture.path());
    wrongProjectModel.setProjectId("different-project");
    check(hasCode(WorkerTaskServices::postflight(wrongProjectModel, reauthContract), "REAUTHORIZATION_AUDIT_INVALID"),
          "wrong project is rejected at successor postflight");
    const auto originChain = auditChain(unchanged, unchanged, unchanged, [](QJsonObject& contract) {
        auto link = contract.value("reauthorization").toObject();
        link.insert("originContractId", "different-origin");
        contract.insert("reauthorization", link);
    }, unchanged);
    rejectedChain(originChain, "REAUTHORIZATION_AUDIT_INVALID", "wrong origin contract link is independently rejected");
    auto wrongSuccessor = reauthContract;
    wrongSuccessor.insert("completionState", "TAMPERED");
    check(hasCode(WorkerTaskServices::postflight(reauthModel, wrongSuccessor), "TASK_CONTRACT_TAMPERED"),
          "wrong successor contract is rejected");
    for (const auto& field : {"sourceFiles", "canonicalInputs", "operations", "baseline"}) {
        const auto chain = auditChain(unchanged, unchanged, unchanged, [&](QJsonObject& contract) {
            auto link = contract.value("reauthorization").toObject();
            if (QString(field) == "sourceFiles") link.insert(field, QJsonArray{"source/unapproved.cpp"});
            else if (QString(field) == "canonicalInputs") link.insert(field, QJsonArray{"ARAMF_WORKER/custom/unapproved.json"});
            else if (QString(field) == "operations") {
                auto operations = link.value(field).toObject(); operations.insert("source/ui.cpp", "DELETE");
                link.insert(field, operations);
            } else link.insert(field, QJsonObject{});
            contract.insert("reauthorization", link);
        }, unchanged);
        rejectedChain(chain, "REAUTHORIZATION_SCOPE_INVALID", "independent exact-scope rejection for " + QString(field));
    }
    const auto revokedChain = validChain();
    appendFixtureEvent("TASK_REAUTHORIZATION_REVOKED", QJsonObject{{"authorizationId", revokedChain.id},
        {"provenance", QJsonObject{{"actor", "system"}, {"agentId", "none"}, {"tool", "WorkerTaskServices"}}}});
    rejectedChain(revokedChain, "REAUTHORIZATION_AUDIT_INVALID", "revocation independently rejects a previously valid bound grant");
    const auto duplicateGrantChain = validChain();
    appendFixtureEvent("ADMIN_OVERRIDE", duplicateGrantChain.approval);
    rejectedChain(duplicateGrantChain, "REAUTHORIZATION_AUDIT_INVALID", "duplicate approval independently rejects its chain");
    const auto duplicateBindingChain = validChain();
    appendFixtureEvent("TASK_REAUTHORIZATION_BOUND", duplicateBindingChain.bound);
    rejectedChain(duplicateBindingChain, "REAUTHORIZATION_AUDIT_INVALID", "duplicate binding independently rejects its chain");
    const auto duplicateValidationChain = validChain();
    appendFixtureEvent("TASK_REAUTHORIZATION_AUTHORIZED", duplicateValidationChain.validation);
    rejectedChain(duplicateValidationChain, "REAUTHORIZATION_AUDIT_INVALID", "duplicate authorization independently rejects its chain");
    const auto consumedChain = validChain();
    appendFixtureEvent("TASK_REAUTHORIZATION_CONSUMED", consumedChain.bound);
    rejectedChain(consumedChain, "REAUTHORIZATION_AUDIT_INVALID", "extra consumption independently rejects a bound chain");
    check(auditPostflight(controlChain.contract).value("status") == "PASS",
          "unrelated negative chains do not revoke or contaminate the valid control");
    QString configError;
    const QString commandConfig = reauthFixture.path() + "/reauthorization-test.aramf.json";
    check(ProjectPersistence().save(reauthModel, commandConfig, &configError), "save reauthorization command fixture");
    for (const auto& forbidden : QStringList{"--command", "--status", "--dependency-fingerprint", "--baseline"}) {
        QString commandOutput, commandError;
        QTextStream outputStream(&commandOutput), errorStream(&commandError);
        check(runWorkerTaskCommand({"task", "run-check", "--config", commandConfig, "--contract", originContractPath,
              "--check", "git-diff-check", forbidden, "caller-authority"}, outputStream, errorStream) == 2
              && commandError.contains("caller commands"), "producer rejects caller evidence authority option " + forbidden);
    }
    const QString requestFile = reauthFixture.path() + "/reauthorization-request.json";
    for (const auto& forbidden : QStringList{"baseline", "sourceFingerprint", "authorizationId", "grantId", "contractId"}) {
        QJsonObject requestJson = reauthRequest.toJson(); requestJson.insert(forbidden, QJsonObject{});
        write(requestFile, QJsonDocument(requestJson).toJson());
        QString commandOutput, commandError;
        QTextStream outputStream(&commandOutput), errorStream(&commandError);
        check(runWorkerTaskCommand({"task", "reauthorize-approve", "--config", commandConfig,
              "--request", requestFile, "--instruction", reauthInstruction}, outputStream, errorStream) == 2,
              "client supplied " + forbidden + " is rejected");
    }
    QJsonObject badPostflight = originReport; badPostflight.insert("completionState", "VERIFIED");
    check(WorkerTaskServices::authorizeReauthorization(reauthModel, originContract, badPostflight,
          reauthRequest, reauthInstruction).value("status") == "BLOCKED", "mismatched origin report is rejected");
    const auto requirements = p6RequirementsOperationAssertions();
    for (auto it = requirements.begin(); it != requirements.end(); ++it) check(it.value().toBool(), it.key());
    const auto successorAssertions = repairSuccessorAssertions();
    for (auto it = successorAssertions.begin(); it != successorAssertions.end(); ++it) check(it.value().toBool(), it.key());
    const auto linked = linkedGrantAssertions();
    for (auto it = linked.begin(); it != linked.end(); ++it) check(it.value().toBool(), it.key());
    const auto reads = prerequisiteReadAssertions();
    for (auto it = reads.begin(); it != reads.end(); ++it) check(it.value().toBool(), it.key());
    const auto operationAssertions = canonicalCertificationOperationAssertions();
    for (auto it = operationAssertions.begin(); it != operationAssertions.end(); ++it) check(it.value().toBool(), it.key());
    const auto scopeAssertions = scopeRegistrationAssertions();
    for (auto it = scopeAssertions.begin(); it != scopeAssertions.end(); ++it) check(it.value().toBool(), it.key());
    check(requiredAssertions.size() - 1 == acceptanceAssertions.size(), "P6 repair profile exposes every exact named assertion");
    check(requiredAssertions.size() == acceptanceAssertions.size(), "acceptance profile has observations for every exact named assertion");
    for (const auto& id : acceptanceProfiles)
        std::cout << "WORKER-TASK-ACCEPTANCE " << QJsonDocument(QJsonObject{{"profile", id},
            {"assertions", acceptanceAssertions}}).toJson(QJsonDocument::Compact).toStdString() << '\n';
    std::cout << "WORKER-TASK checks=" << checks << " failed=" << failed << " matrix=" << matrix.size() << '\n';
    return failed == 0;
}
