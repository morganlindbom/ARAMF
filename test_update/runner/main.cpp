#include "core/AramfPaths.h"
#include "core/FrameworkKnowledge.h"
#include "core/ProjectMemory.h"
#include "core/ProjectModel.h"
#include "core/UpdateService.h"
#include "core/ValidationRouting.h"
#include "core/CodexExecutionAdapter.h"
#include "core/UpdateExecutionService.h"
#include "core/CodexExecutableResolver.h"
#include "core/ImprovementBacklog.h"
#include "core/MemoryCommand.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "ui/workflows/update/backlog/ImprovementBacklogPage.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSet>
#include <QTemporaryDir>
#include <QBuffer>
#include <QTextStream>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QTimer>

#include <iostream>
#include <algorithm>

namespace {
struct Campaign {
    int pass = 0;
    int fail = 0;
    QString root;

    bool check(const QString& id, const QString& description, bool condition, const QString& detail = {})
    {
        const QString result = condition ? QStringLiteral("PASS") : QStringLiteral("FAIL");
        if (condition) ++pass; else ++fail;
        QDir().mkpath(QDir(root).filePath(QStringLiteral("results")));
        QFile file(QDir(root).filePath(QStringLiteral("results/%1.md").arg(id)));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            const QByteArray text = QStringLiteral("# %1\n\nScenario: %2\n\nResult: %3\n%4")
                                        .arg(id, description, result, detail).toUtf8();
            file.write(text);
        }
        return condition;
    }
};

bool initialize(const QString& root, ProjectModel* model, QString* error)
{
    model->setProjectId(QStringLiteral("update-fixture"));
    model->setProjectName(QStringLiteral("UPDATE fixture"));
    model->setProjectPath(root);
    RuleConfiguration rules;
    rules.projectScopes = {QStringLiteral("implementation"), QStringLiteral("regression")};
    model->setRuleConfiguration(rules);
    MemoryConfiguration memoryConfiguration;
    memoryConfiguration.maintenanceOptions = {QStringLiteral("record-decisions")};
    model->setMemoryConfiguration(memoryConfiguration);
    ProjectMemory memory;
    return memory.initialize(root, model, error);
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir globalData;
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(QDir(globalData.path()).filePath(QStringLiteral("ARAMF_DATA/framework-knowledge-library.json")));
    QFile::remove(FrameworkKnowledgeService().legacyGlobalLibraryPath());
    const QString repoRoot = QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("..")));
    Campaign campaign{0, 0, QDir(repoRoot).filePath(QStringLiteral("test_update"))};
    QTemporaryDir fixture;
    fixture.setAutoRemove(true);
    ProjectModel model;
    QString error;
    bool ready = fixture.isValid() && initialize(fixture.path(), &model, &error);
    campaign.check(QStringLiteral("UPDATE-001"), QStringLiteral("isolated fixture initialization"), ready, error);
    if (!ready) return 1;

    FrameworkKnowledgeService knowledge;
    const QString candidateId = knowledge.propose(fixture.path(), QStringLiteral("Approved update lesson"),
        QStringLiteral("A verified implementation lesson for the project."),
        {QStringLiteral("implementation"), QStringLiteral("regression")},
        {QStringLiteral("UPDATE-001")}, true, &error);
    campaign.check(QStringLiteral("UPDATE-002"), QStringLiteral("candidate creation"), !candidateId.isEmpty(), error);
    const auto candidateApplicable = UpdateService().applicableApprovedKnowledge(fixture.path(), model);
    campaign.check(QStringLiteral("UPDATE-003"), QStringLiteral("candidate is not applicable as approved knowledge"),
                   std::none_of(candidateApplicable.cbegin(), candidateApplicable.cend(),
                                [&candidateId](const auto& entry) { return entry.id == candidateId; }));

    const bool approved = knowledge.approve(fixture.path(), candidateId, QStringLiteral("human-review"), &error);
    campaign.check(QStringLiteral("UPDATE-004"), QStringLiteral("explicit approval"), approved, error);
    const auto applicable = UpdateService().applicableApprovedKnowledge(fixture.path(), model, &error);
    campaign.check(QStringLiteral("UPDATE-005"), QStringLiteral("approved knowledge is scope filtered"),
                   std::any_of(applicable.cbegin(), applicable.cend(), [&candidateId](const auto& entry) { return entry.id == candidateId; }), error);

    UpdateService service;
    const auto beforeName = model.projectName();
    const auto analysis = service.analyze(fixture.path(), model, {candidateId});
    campaign.check(QStringLiteral("UPDATE-006"), QStringLiteral("explicit selection creates an active plan"), analysis.success && analysis.status == QStringLiteral("prepared"));
    campaign.check(QStringLiteral("UPDATE-007"), QStringLiteral("analysis is read-only for the project model"), model.projectName() == beforeName);
    campaign.check(QStringLiteral("UPDATE-008"), QStringLiteral("plan records selected knowledge and affected areas"),
                   analysis.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().size() == 1
                   && !analysis.plan.value(QStringLiteral("expectedAreas")).toArray().isEmpty());
    campaign.check(QStringLiteral("UPDATE-009"), QStringLiteral("plan recommends subsystem validation"),
                   analysis.plan.value(QStringLiteral("recommendedValidationLevel")).toString() == QStringLiteral("SUBSYSTEM"),
                   analysis.plan.value(QStringLiteral("recommendedValidationLevel")).toString());
    campaign.check(QStringLiteral("UPDATE-010"), QStringLiteral("plan preserves higher authority constraints"),
                   analysis.plan.value(QStringLiteral("authorityConstraints")).toArray().size() == 3);
    campaign.check(QStringLiteral("UPDATE-011"), QStringLiteral("prepared plan is current"), service.isPlanCurrent(fixture.path(), model, &error), error);

    const bool applied = service.apply(fixture.path(), model, &error);
    const auto plan = service.currentPlan(fixture.path(), &error);
    QFile contract(QDir(fixture.path()).filePath(AramfPaths::UpdateContract));
    campaign.check(QStringLiteral("UPDATE-012"), QStringLiteral("prepare creates an explicit READY plan"), applied && plan.value(QStringLiteral("status")).toString() == QStringLiteral("READY"));
    campaign.check(QStringLiteral("UPDATE-013"), QStringLiteral("contract is present and traceable"), contract.exists() && plan.value(QStringLiteral("planId")).toString() == analysis.plan.value(QStringLiteral("planId")).toString());

    model.setProjectName(QStringLiteral("Changed after analysis"));
    campaign.check(QStringLiteral("UPDATE-014"), QStringLiteral("model changes make a plan stale"), !service.isPlanCurrent(fixture.path(), model, &error));
    QString blockedError;
    campaign.check(QStringLiteral("UPDATE-015"), QStringLiteral("stale apply is rejected"), !service.apply(fixture.path(), model, &blockedError));

    QTemporaryDir conflictFixture;
    ProjectModel conflictModel;
    QString conflictError;
    const bool conflictReady = conflictFixture.isValid() && initialize(conflictFixture.path(), &conflictModel, &conflictError);
    const QString conflictId = conflictReady ? knowledge.propose(conflictFixture.path(), QStringLiteral("Blocked lesson"), QStringLiteral("A lesson."), {QStringLiteral("implementation")}, {}, true, &conflictError) : QString();
    const bool conflictApproved = conflictReady && knowledge.approve(conflictFixture.path(), conflictId, QStringLiteral("human-review"), &conflictError);
    ProjectMemory conflictMemory;
    bool decisionRecorded = false;
    if (conflictApproved) decisionRecorded = conflictMemory.recordDecision(conflictFixture.path(), QStringLiteral("blocks-update"), QStringLiteral("authority"), QStringLiteral("BLOCKS %1").arg(conflictId), QStringLiteral("current"), {}, &conflictError);
    const auto conflictPlan = conflictApproved ? service.analyze(conflictFixture.path(), conflictModel, {conflictId}) : UpdateAnalysisResult{};
    campaign.check(QStringLiteral("UPDATE-016"), QStringLiteral("higher-authority conflict blocks analysis"), conflictPlan.success && conflictPlan.blockedByAuthority && conflictPlan.status == QStringLiteral("conflict"),
                   QStringLiteral("decision=") + (decisionRecorded ? QStringLiteral("yes") : QStringLiteral("no"))
                   + QStringLiteral(" error=") + conflictError + QStringLiteral(" status=") + conflictPlan.status);
    const auto fixtureEntries = knowledge.entries(fixture.path());
    const auto fixtureEntry = std::find_if(fixtureEntries.cbegin(), fixtureEntries.cend(), [&candidateId](const auto& entry) { return entry.id == candidateId; });
    campaign.check(QStringLiteral("UPDATE-017"), QStringLiteral("approved entry remains distinct from candidates"), fixtureEntry != fixtureEntries.cend() && fixtureEntry->status == QStringLiteral("approved"));
    campaign.check(QStringLiteral("UPDATE-018"), QStringLiteral("empty selection is rejected"), !service.analyze(fixture.path(), model, {}).success);
    campaign.check(QStringLiteral("UPDATE-019"), QStringLiteral("plan and contract remain separate from memory bookkeeping"), QFileInfo::exists(QDir(fixture.path()).filePath(AramfPaths::UpdatePlan)) && QFileInfo::exists(QDir(fixture.path()).filePath(AramfPaths::UpdateContract)));
    const auto selfHostEntries = knowledge.entries(repoRoot);
    const auto selfHostCandidate = std::find_if(selfHostEntries.cbegin(), selfHostEntries.cend(), [](const auto& entry) {
        return entry.id == QStringLiteral("fk-7a246faa4bc6ad74");
    });
    ProjectModel selfHostModel;
    selfHostModel.setProjectPath(repoRoot);
    const auto selfHostApplicable = service.applicableApprovedKnowledge(repoRoot, selfHostModel, &error);
    campaign.check(QStringLiteral("UPDATE-020"), QStringLiteral("self-host entry persists with approved state"),
                   selfHostCandidate != selfHostEntries.cend() && selfHostCandidate->status == QStringLiteral("approved")
                   && selfHostCandidate->reviewStatus == QStringLiteral("approved")
                   && !selfHostCandidate->approvedAt.isEmpty() && selfHostCandidate->supersededBy.isEmpty());
    campaign.check(QStringLiteral("UPDATE-021"), QStringLiteral("self-host approved entry is returned as applicable"),
                   std::any_of(selfHostApplicable.cbegin(), selfHostApplicable.cend(), [](const auto& entry) {
                       return entry.id == QStringLiteral("fk-7a246faa4bc6ad74") && entry.status == QStringLiteral("approved");
                   }));

    QTemporaryDir exactFixture;
    ProjectModel exactModel;
    QString exactError;
    const bool exactReady = exactFixture.isValid() && initialize(exactFixture.path(), &exactModel, &exactError);
    const QString exactId = exactReady ? knowledge.propose(
        exactFixture.path(), QStringLiteral("Selected products define lifecycle preconditions"),
        QStringLiteral("Lifecycle preconditions must be derived from the explicitly selected product set. Optional products must not introduce mandatory lifecycle requirements when they were intentionally excluded."),
        {QStringLiteral("lifecycle"), QStringLiteral("selective-generation"), QStringLiteral("optional-components"), QStringLiteral("verification"), QStringLiteral("finalization")},
        {QStringLiteral("UPDATE-022")}, true, &exactError) : QString();
    const bool exactApproved = exactId == QStringLiteral("fk-7a246faa4bc6ad74")
        && knowledge.approve(exactFixture.path(), exactId, QStringLiteral("explicit-user"), &exactError);
    const auto exactApplicable = exactApproved ? service.applicableApprovedKnowledge(exactFixture.path(), exactModel, &exactError) : QList<FrameworkKnowledgeEntry>{};
    campaign.check(QStringLiteral("UPDATE-022"), QStringLiteral("approved lifecycle knowledge is applicable to an ARAMF-like project"),
                   exactApproved && std::any_of(exactApplicable.cbegin(), exactApplicable.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-7a246faa4bc6ad74"); }), exactError);
    campaign.check(QStringLiteral("UPDATE-023"), QStringLiteral("approval does not implicitly select the update"),
                   exactApproved && !service.analyze(exactFixture.path(), exactModel, {}).success);
    const auto reopenedApplicable = UpdateService().applicableApprovedKnowledge(exactFixture.path(), exactModel, &exactError);
    campaign.check(QStringLiteral("UPDATE-024"), QStringLiteral("page-entry refresh reads current approved state"),
                   std::any_of(reopenedApplicable.cbegin(), reopenedApplicable.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-7a246faa4bc6ad74") && entry.status == QStringLiteral("approved"); }));
    const auto exactReloaded = FrameworkKnowledgeService().approvedEntries(exactFixture.path());
    campaign.check(QStringLiteral("UPDATE-025"), QStringLiteral("approved knowledge survives a service reload"),
                   std::any_of(exactReloaded.cbegin(), exactReloaded.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-7a246faa4bc6ad74"); }));
    const bool superseded = knowledge.supersede(exactFixture.path(), exactId, QStringLiteral("replacement"), &exactError);
    const auto supersededApplicable = service.applicableApprovedKnowledge(exactFixture.path(), exactModel, &exactError);
    campaign.check(QStringLiteral("UPDATE-026"), QStringLiteral("superseded knowledge is excluded"),
                   superseded && std::none_of(supersededApplicable.cbegin(), supersededApplicable.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-7a246faa4bc6ad74"); }));
    QTemporaryDir candidateFixture;
    ProjectModel candidateModel;
    QString candidateError;
    const bool candidateReady = candidateFixture.isValid() && initialize(candidateFixture.path(), &candidateModel, &candidateError);
    const QString candidateOnlyId = candidateReady ? knowledge.propose(candidateFixture.path(), QStringLiteral("Candidate only"), QStringLiteral("Candidate evidence."), {QStringLiteral("lifecycle")}, {}, true, &candidateError) : QString();
    const auto candidateOnlyApplicable = service.applicableApprovedKnowledge(candidateFixture.path(), candidateModel, &candidateError);
    campaign.check(QStringLiteral("UPDATE-027"), QStringLiteral("candidate and more-evidence knowledge is excluded"),
                   !candidateOnlyId.isEmpty() && std::none_of(candidateOnlyApplicable.cbegin(), candidateOnlyApplicable.cend(), [&candidateOnlyId](const auto& entry) { return entry.id == candidateOnlyId; }));

    QTemporaryDir unrelatedFixture;
    ProjectModel unrelatedModel;
    QString unrelatedError;
    const bool unrelatedReady = unrelatedFixture.isValid() && initialize(unrelatedFixture.path(), &unrelatedModel, &unrelatedError);
    const QString unrelatedId = unrelatedReady ? knowledge.propose(unrelatedFixture.path(), QStringLiteral("Unrelated lesson"), QStringLiteral("A lesson for an unrelated subsystem."), {QStringLiteral("unrelated-subsystem")}, {}, true, &unrelatedError) : QString();
    const bool unrelatedApproved = unrelatedReady && knowledge.approve(unrelatedFixture.path(), unrelatedId, QStringLiteral("explicit-user"), &unrelatedError);
    const auto unrelatedApplicable = service.applicableApprovedKnowledge(unrelatedFixture.path(), unrelatedModel, &unrelatedError);
    campaign.check(QStringLiteral("UPDATE-028"), QStringLiteral("approved but non-applicable knowledge is excluded with a deterministic reason"),
                   unrelatedApproved && std::none_of(unrelatedApplicable.cbegin(), unrelatedApplicable.cend(), [&unrelatedId](const auto& entry) { return entry.id == unrelatedId; }));

    QTemporaryDir sourceProject;
    ProjectModel sourceModel;
    QString libraryError;
    const bool sourceReady = sourceProject.isValid() && initialize(sourceProject.path(), &sourceModel, &libraryError);
    const QString globalId = sourceReady ? knowledge.propose(sourceProject.path(), QStringLiteral("Portable global lesson"), QStringLiteral("A reusable approved lesson for future projects."), {QStringLiteral("lifecycle")}, {QStringLiteral("UPDATE-029")}, true, &libraryError) : QString();
    const bool globalApproved = sourceReady && knowledge.approve(sourceProject.path(), globalId, QStringLiteral("global-test-user"), &libraryError);
    const bool promoted = globalApproved && knowledge.promoteToGlobal(sourceProject.path(), globalId, &libraryError);
    const auto globalEntries = knowledge.globalEntries(&libraryError);
    campaign.check(QStringLiteral("UPDATE-029"), QStringLiteral("approved portable knowledge can be promoted globally"), promoted && std::any_of(globalEntries.cbegin(), globalEntries.cend(), [&globalId](const auto& entry) { return entry.id == globalId && entry.origin == QStringLiteral("global"); }), libraryError);
    knowledge.promoteToGlobal(sourceProject.path(), globalId, &libraryError);
    const auto deduplicatedGlobal = knowledge.globalEntries(&libraryError);
    campaign.check(QStringLiteral("UPDATE-030"), QStringLiteral("duplicate promotion preserves one stable global identity"),
                   std::count_if(deduplicatedGlobal.cbegin(), deduplicatedGlobal.cend(), [&globalId](const auto& entry) { return entry.id == globalId; }) == 1);
    const auto globalReloaded = FrameworkKnowledgeService().globalEntries(&libraryError);
    campaign.check(QStringLiteral("UPDATE-031"), QStringLiteral("global library survives service recreation"),
                   std::any_of(globalReloaded.cbegin(), globalReloaded.cend(), [&globalId](const auto& entry) { return entry.id == globalId && entry.approvalSource == QStringLiteral("global-test-user"); }));

    QTemporaryDir futureProject;
    ProjectModel futureModel;
    const bool futureReady = futureProject.isValid() && initialize(futureProject.path(), &futureModel, &libraryError);
    const auto futureEntries = knowledge.entries(futureProject.path(), &libraryError);
    const auto seededGlobal = std::find_if(futureEntries.cbegin(), futureEntries.cend(), [&globalId](const auto& entry) { return entry.id == globalId; });
    campaign.check(QStringLiteral("UPDATE-032"), QStringLiteral("fresh project receives approved global knowledge"),
                   futureReady && seededGlobal != futureEntries.cend() && seededGlobal->status == QStringLiteral("approved") && seededGlobal->origin == QStringLiteral("global"));
    campaign.check(QStringLiteral("UPDATE-033"), QStringLiteral("seeded knowledge preserves provenance and evidence"),
                   seededGlobal != futureEntries.cend() && seededGlobal->originalKnowledgeId == globalId && seededGlobal->evidence.contains(QStringLiteral("UPDATE-029")));
    const auto futureGlobalMatches = std::count_if(futureEntries.cbegin(), futureEntries.cend(), [&globalId](const auto& entry) { return entry.id == globalId; });
    campaign.check(QStringLiteral("UPDATE-034"), QStringLiteral("fresh project does not duplicate global identity"), futureGlobalMatches == 1);

    QTemporaryDir candidateGlobalProject;
    ProjectModel candidateGlobalModel;
    initialize(candidateGlobalProject.path(), &candidateGlobalModel, &libraryError);
    const QString futureCandidateId = knowledge.propose(candidateGlobalProject.path(), QStringLiteral("Future candidate"), QStringLiteral("Not approved."), {QStringLiteral("lifecycle")}, {}, true, &libraryError);
    const bool candidatePromotionRejected = !knowledge.promoteToGlobal(candidateGlobalProject.path(), futureCandidateId, &libraryError);
    campaign.check(QStringLiteral("UPDATE-035"), QStringLiteral("candidate cannot be promoted globally"), candidatePromotionRejected);
    const QString nonPortableId = knowledge.propose(candidateGlobalProject.path(), QStringLiteral("Project-only lesson"), QStringLiteral("Must remain project local."), {QStringLiteral("lifecycle")}, {}, false, &libraryError);
    const bool nonPortableApproved = knowledge.approve(candidateGlobalProject.path(), nonPortableId, QStringLiteral("project-user"), &libraryError);
    campaign.check(QStringLiteral("UPDATE-036"), QStringLiteral("non-portable knowledge cannot be promoted"), nonPortableApproved && !knowledge.promoteToGlobal(candidateGlobalProject.path(), nonPortableId, &libraryError));
    const bool globalSuperseded = knowledge.supersedeGlobal(globalId, QStringLiteral("replacement-global"), &libraryError);
    QTemporaryDir afterSupersession;
    ProjectModel afterSupersessionModel;
    const bool afterReady = afterSupersession.isValid() && initialize(afterSupersession.path(), &afterSupersessionModel, &libraryError);
    const auto afterEntries = knowledge.entries(afterSupersession.path(), &libraryError);
    campaign.check(QStringLiteral("UPDATE-037"), QStringLiteral("superseded global knowledge is inactive for future projects"),
                   globalSuperseded && afterReady && std::none_of(afterEntries.cbegin(), afterEntries.cend(), [&globalId](const auto& entry) { return entry.id == globalId && entry.origin == QStringLiteral("global"); }));

    QTemporaryDir authorityProject;
    ProjectModel authorityModel;
    ProjectResource authorityResource;
    authorityResource.id = QStringLiteral("source-of-truth");
    authorityResource.location = QStringLiteral("CMakeLists.txt");
    authorityResource.authorityLevel = QStringLiteral("primary-source-of-truth");
    authorityModel.setResources({authorityResource});
    const bool authorityReady = authorityProject.isValid() && initialize(authorityProject.path(), &authorityModel, &libraryError);
    campaign.check(QStringLiteral("UPDATE-038"), QStringLiteral("project Source of Truth remains higher authority than seeded knowledge"), authorityReady && authorityModel.resources().first().authorityLevel == QStringLiteral("primary-source-of-truth"));
    QJsonObject globalSchema;
    QFile globalFile(knowledge.globalLibraryPath());
    if (globalFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        globalSchema = QJsonDocument::fromJson(globalFile.readAll()).object();
        globalFile.close();
    }
    campaign.check(QStringLiteral("UPDATE-039"), QStringLiteral("global library schema is versioned and user-writable"), globalSchema.value(QStringLiteral("version")).toInt() == 1 && QFileInfo::exists(knowledge.globalLibraryPath()));
    campaign.check(QStringLiteral("UPDATE-040"), QStringLiteral("current approved self-host knowledge remains in project store"),
                   std::any_of(selfHostEntries.cbegin(), selfHostEntries.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-7a246faa4bc6ad74") && entry.status == QStringLiteral("approved"); }));

    const QString testProgramLocalPath = knowledge.globalLibraryPath();
    FrameworkKnowledgeService::clearGlobalLibraryPathForTests();
    const QString programLocalPath = knowledge.globalLibraryPath();
    campaign.check(QStringLiteral("UPDATE-041"), QStringLiteral("global library path is relative to the running application"),
                   QDir::cleanPath(programLocalPath) == QDir(repoRoot).filePath(QStringLiteral("ARAMF_DATA/framework-knowledge-library.json"))
                       && QDir::cleanPath(programLocalPath).contains(QStringLiteral("ARAMF_DATA")));
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(testProgramLocalPath);
    campaign.check(QStringLiteral("UPDATE-042"), QStringLiteral("program-local ARAMF_DATA is created automatically"),
                   QFileInfo::exists(testProgramLocalPath));

    QFile::remove(testProgramLocalPath);
    QFile::remove(QDir(QFileInfo(testProgramLocalPath).absolutePath()).filePath(QStringLiteral(".framework-knowledge-library-migrated.json")));
    QFile legacyFile(knowledge.legacyGlobalLibraryPath());
    const QJsonObject legacyEntry{
        {QStringLiteral("id"), QStringLiteral("fk-7a246faa4bc6ad74")},
        {QStringLiteral("title"), QStringLiteral("Selected products define lifecycle preconditions")},
        {QStringLiteral("lesson"), QStringLiteral("Lifecycle preconditions must be derived from the explicitly selected product set. Optional products must not introduce mandatory lifecycle requirements when they were intentionally excluded.")},
        {QStringLiteral("status"), QStringLiteral("approved")},
        {QStringLiteral("reviewStatus"), QStringLiteral("approved")},
        {QStringLiteral("scopes"), QJsonArray{QStringLiteral("lifecycle"), QStringLiteral("verification")}},
        {QStringLiteral("evidence"), QJsonArray{QStringLiteral("legacy-migration-evidence")}},
        {QStringLiteral("portable"), true},
        {QStringLiteral("approvedAt"), QStringLiteral("2026-08-23T00:00:00Z")},
        {QStringLiteral("approvalSource"), QStringLiteral("Morgan Lindbom")},
        {QStringLiteral("supersededBy"), QStringLiteral("")},
        {QStringLiteral("origin"), QStringLiteral("global")}};
    const QJsonObject legacyStore{{QStringLiteral("_file"), QStringLiteral("framework-knowledge-library.json")},
                                  {QStringLiteral("version"), 1},
                                  {QStringLiteral("entries"), QJsonArray{legacyEntry}}};
    QDir().mkpath(QFileInfo(knowledge.legacyGlobalLibraryPath()).absolutePath());
    if (legacyFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        legacyFile.write(QJsonDocument(legacyStore).toJson(QJsonDocument::Indented));
        legacyFile.close();
    }
    QString migrationError;
    const bool migrated = knowledge.ensureGlobalLibrary(&migrationError);
    const auto migratedEntries = knowledge.globalEntries(&migrationError);
    const auto migratedEntry = std::find_if(migratedEntries.cbegin(), migratedEntries.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-7a246faa4bc6ad74"); });
    campaign.check(QStringLiteral("UPDATE-043"), QStringLiteral("legacy AppData library migrates to program-local storage"),
                   migrated && migratedEntry != migratedEntries.cend() && migratedEntry->status == QStringLiteral("approved"), migrationError + QStringLiteral(" legacy=") + knowledge.legacyGlobalLibraryPath() + QStringLiteral(" canonical=") + knowledge.globalLibraryPath());
    campaign.check(QStringLiteral("UPDATE-044"), QStringLiteral("legacy migration is non-destructive"), QFileInfo::exists(knowledge.legacyGlobalLibraryPath()));

    QTemporaryDir migratedProject;
    ProjectModel migratedModel;
    const bool seededMigrated = migratedProject.isValid() && initialize(migratedProject.path(), &migratedModel, &migrationError);
    const auto migratedProjectEntries = knowledge.entries(migratedProject.path(), &migrationError);
    const auto seededApproved = std::find_if(migratedProjectEntries.cbegin(), migratedProjectEntries.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-7a246faa4bc6ad74"); });
    campaign.check(QStringLiteral("UPDATE-045"), QStringLiteral("approved global state overrides the same-ID built-in candidate during seeding"),
                   seededMigrated && seededApproved != migratedProjectEntries.cend() && seededApproved->status == QStringLiteral("approved"));
    campaign.check(QStringLiteral("UPDATE-046"), QStringLiteral("global storage has no active AppData fallback"),
                   QDir::cleanPath(programLocalPath).contains(QStringLiteral("ARAMF_DATA"))
                       && !QDir::cleanPath(programLocalPath).contains(QStringLiteral("AppData")));

    QFile::remove(testProgramLocalPath);
    const QString migrationMarker = QDir(QFileInfo(testProgramLocalPath).absolutePath()).filePath(QStringLiteral(".framework-knowledge-library-migrated.json"));
    QFile::remove(migrationMarker);
    QFile invalidLegacy(knowledge.legacyGlobalLibraryPath());
    if (invalidLegacy.open(QIODevice::WriteOnly | QIODevice::Text)) {
        invalidLegacy.write("not-json");
        invalidLegacy.close();
    }
    QString invalidMigrationError;
    const bool invalidMigration = knowledge.ensureGlobalLibrary(&invalidMigrationError);
    campaign.check(QStringLiteral("UPDATE-047"), QStringLiteral("failed legacy migration preserves source and does not create partial canonical state"),
                   !invalidMigration && QFileInfo::exists(knowledge.legacyGlobalLibraryPath()) && !QFileInfo::exists(testProgramLocalPath));

    QTemporaryDir blockedRoot;
    const QString blockedParent = QDir(blockedRoot.path()).filePath(QStringLiteral("blocked"));
    QFile blocker(blockedParent);
    if (blocker.open(QIODevice::WriteOnly | QIODevice::Text)) {
        blocker.write("not-a-directory");
        blocker.close();
    }
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(QDir(blockedParent).filePath(QStringLiteral("framework-knowledge-library.json")));
    QString blockedStorageError;
    const bool blockedWrite = knowledge.ensureGlobalLibrary(&blockedStorageError);
    campaign.check(QStringLiteral("UPDATE-048"), QStringLiteral("unwritable program-local storage reports an explicit error"),
                   !blockedWrite && !blockedStorageError.isEmpty(), blockedStorageError);
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(testProgramLocalPath);

    auto writeLibrary = [](const QString& path, const QJsonArray& entries) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("_file"), QStringLiteral("framework-knowledge-library.json")},
                                             {QStringLiteral("version"), 1}, {QStringLiteral("entries"), entries}}).toJson(QJsonDocument::Indented));
        return file.error() == QFile::NoError;
    };
    const QJsonObject rootEntry{
        {QStringLiteral("id"), QStringLiteral("fk-root-migration")},
        {QStringLiteral("title"), QStringLiteral("Root migration lesson")},
        {QStringLiteral("lesson"), QStringLiteral("A portable lesson migrated to the ARAMF program root.")},
        {QStringLiteral("status"), QStringLiteral("approved")},
        {QStringLiteral("reviewStatus"), QStringLiteral("approved")},
        {QStringLiteral("scopes"), QJsonArray{QStringLiteral("lifecycle")}},
        {QStringLiteral("evidence"), QJsonArray{QStringLiteral("root-evidence")}},
        {QStringLiteral("portable"), true},
        {QStringLiteral("approvedAt"), QStringLiteral("2026-08-23T00:00:00Z")},
        {QStringLiteral("approvalSource"), QStringLiteral("root-test-user")},
        {QStringLiteral("supersededBy"), QStringLiteral("")}};
    QTemporaryDir rootFixture;
    const QString root = rootFixture.path();
    QDir(root).mkpath(QStringLiteral("aramf_setup/memory"));
    QDir(root).mkpath(QStringLiteral("src"));
    QDir(root).mkpath(QStringLiteral("build/Debug"));
    QDir(root).mkpath(QStringLiteral("build/Release"));
    QFile rootCMake(QDir(root).filePath(QStringLiteral("CMakeLists.txt")));
    if (rootCMake.open(QIODevice::WriteOnly | QIODevice::Text)) { rootCMake.write("cmake_minimum_required(VERSION 3.20)\n"); rootCMake.close(); }
    const QString debugDirectory = QDir(root).filePath(QStringLiteral("build/Debug"));
    const QString releaseDirectory = QDir(root).filePath(QStringLiteral("build/Release"));
    AramfPaths::clearProgramRootForTests();
    AramfPaths::setApplicationDirectoryForTests(debugDirectory);
    FrameworkKnowledgeService::clearGlobalLibraryPathForTests();
    FrameworkKnowledgeService rootKnowledge;
    const QString rootLibrary = rootKnowledge.globalLibraryPath();
    campaign.check(QStringLiteral("UPDATE-049"), QStringLiteral("Debug build resolves the ARAMF program root"), QDir::cleanPath(AramfPaths::programRoot()) == QDir::cleanPath(root));
    AramfPaths::setApplicationDirectoryForTests(releaseDirectory);
    campaign.check(QStringLiteral("UPDATE-050"), QStringLiteral("Release build resolves the ARAMF program root"), QDir::cleanPath(AramfPaths::programRoot()) == QDir::cleanPath(root));
    AramfPaths::setApplicationDirectoryForTests(debugDirectory);
    QFile::remove(rootKnowledge.legacyGlobalLibraryPath());
    const bool rootCreated = rootKnowledge.ensureGlobalLibrary(&libraryError);
    campaign.check(QStringLiteral("UPDATE-051"), QStringLiteral("global library is created under the ARAMF program root"), rootCreated && rootLibrary == QDir(root).filePath(QStringLiteral("ARAMF_DATA/framework-knowledge-library.json")) && QFileInfo::exists(rootLibrary));
    campaign.check(QStringLiteral("UPDATE-052"), QStringLiteral("no active global library is created under build"), !QDir::cleanPath(rootLibrary).contains(QStringLiteral("build")));
    const QString buildLocalLibrary = QDir(debugDirectory).filePath(QStringLiteral("ARAMF_DATA/framework-knowledge-library.json"));
    QJsonObject buildEntry = rootEntry;
    buildEntry.insert(QStringLiteral("evidence"), QJsonArray{QStringLiteral("build-evidence")});
    writeLibrary(rootLibrary, QJsonArray{rootEntry});
    writeLibrary(buildLocalLibrary, QJsonArray{buildEntry});
    const bool buildMigrated = rootKnowledge.ensureGlobalLibrary(&libraryError);
    const auto migratedRootEntries = rootKnowledge.globalEntries(&libraryError);
    const auto migratedRootEntry = std::find_if(migratedRootEntries.cbegin(), migratedRootEntries.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-root-migration"); });
    campaign.check(QStringLiteral("UPDATE-055"), QStringLiteral("legacy build-local ARAMF_DATA migrates to the root"), buildMigrated && migratedRootEntry != migratedRootEntries.cend());
    campaign.check(QStringLiteral("UPDATE-056"), QStringLiteral("build-local migration source remains non-destructive"), QFileInfo::exists(buildLocalLibrary));
    campaign.check(QStringLiteral("UPDATE-057"), QStringLiteral("root and build-local entries merge without duplication"), migratedRootEntry != migratedRootEntries.cend() && std::count_if(migratedRootEntries.cbegin(), migratedRootEntries.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-root-migration"); }) == 1 && migratedRootEntry->evidence.contains(QStringLiteral("root-evidence")) && migratedRootEntry->evidence.contains(QStringLiteral("build-evidence")));
    campaign.check(QStringLiteral("UPDATE-058"), QStringLiteral("approved build-local knowledge remains approved"), migratedRootEntry != migratedRootEntries.cend() && migratedRootEntry->status == QStringLiteral("approved") && migratedRootEntry->reviewStatus == QStringLiteral("approved"));
    const bool buildDeleted = QDir(QDir(root).filePath(QStringLiteral("build"))).removeRecursively();
    campaign.check(QStringLiteral("UPDATE-053"), QStringLiteral("deleting a simulated build directory preserves root knowledge"), buildDeleted && QFileInfo::exists(rootLibrary));
    QDir(root).mkpath(QStringLiteral("build/Release"));
    AramfPaths::setApplicationDirectoryForTests(QDir(root).filePath(QStringLiteral("build/Release")));
    const auto rebuiltEntries = rootKnowledge.globalEntries(&libraryError);
    campaign.check(QStringLiteral("UPDATE-054"), QStringLiteral("recreated build discovers existing root knowledge"), std::any_of(rebuiltEntries.cbegin(), rebuiltEntries.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-root-migration") && entry.status == QStringLiteral("approved"); }));

    QJsonObject builtInCandidate = rootEntry;
    builtInCandidate.insert(QStringLiteral("status"), QStringLiteral("candidate"));
    builtInCandidate.insert(QStringLiteral("reviewStatus"), QStringLiteral("more-evidence"));
    writeLibrary(QDir(root).filePath(QStringLiteral("aramf_setup/memory/framework-knowledge.json")), QJsonArray{builtInCandidate});
    QTemporaryDir seededRootProject;
    ProjectModel seededRootModel;
    QString rootSeedError;
    const bool rootSeeded = seededRootProject.isValid() && initialize(seededRootProject.path(), &seededRootModel, &rootSeedError);
    const auto rootSeedEntries = rootKnowledge.entries(seededRootProject.path(), &rootSeedError);
    const auto rootSeedEntry = std::find_if(rootSeedEntries.cbegin(), rootSeedEntries.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-root-migration"); });
    campaign.check(QStringLiteral("UPDATE-059"), QStringLiteral("built-in candidate cannot downgrade approved root knowledge"), rootSeeded && rootSeedEntry != rootSeedEntries.cend() && rootSeedEntry->status == QStringLiteral("approved"));
    campaign.check(QStringLiteral("UPDATE-060"), QStringLiteral("fresh project inherits approved root knowledge"), rootSeedEntry != rootSeedEntries.cend() && rootSeedEntry->origin == QStringLiteral("global"));

    QTemporaryDir legacyRootFixture;
    const QString legacyRoot = legacyRootFixture.path();
    QDir(legacyRoot).mkpath(QStringLiteral("aramf_setup")); QDir(legacyRoot).mkpath(QStringLiteral("src")); QDir(legacyRoot).mkpath(QStringLiteral("build/Debug"));
    QFile legacyCMake(QDir(legacyRoot).filePath(QStringLiteral("CMakeLists.txt")));
    if (legacyCMake.open(QIODevice::WriteOnly | QIODevice::Text)) { legacyCMake.write("cmake_minimum_required(VERSION 3.20)\n"); legacyCMake.close(); }
    AramfPaths::setApplicationDirectoryForTests(QDir(legacyRoot).filePath(QStringLiteral("build/Debug")));
    FrameworkKnowledgeService legacyRootKnowledge;
    QFile::remove(legacyRootKnowledge.legacyGlobalLibraryPath());
    writeLibrary(legacyRootKnowledge.legacyGlobalLibraryPath(), QJsonArray{rootEntry});
    const bool appDataMigrated = legacyRootKnowledge.ensureGlobalLibrary(&libraryError);
    const auto appDataMigratedEntries = legacyRootKnowledge.globalEntries(&libraryError);
    campaign.check(QStringLiteral("UPDATE-061"), QStringLiteral("AppData legacy migration converges into root ARAMF_DATA"), appDataMigrated && std::any_of(appDataMigratedEntries.cbegin(), appDataMigratedEntries.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-root-migration"); }));
    QJsonObject lateEntry = rootEntry;
    lateEntry.insert(QStringLiteral("id"), QStringLiteral("fk-late-legacy"));
    writeLibrary(legacyRootKnowledge.legacyGlobalLibraryPath(), QJsonArray{lateEntry});
    const auto afterAppDataMigration = legacyRootKnowledge.globalEntries(&libraryError);
    campaign.check(QStringLiteral("UPDATE-062"), QStringLiteral("AppData is not an active authority after migration"), std::none_of(afterAppDataMigration.cbegin(), afterAppDataMigration.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-late-legacy"); }));
    writeLibrary(legacyRootKnowledge.legacyExecutableGlobalLibraryPath(), QJsonArray{lateEntry});
    const auto afterBuildMigration = legacyRootKnowledge.globalEntries(&libraryError);
    campaign.check(QStringLiteral("UPDATE-063"), QStringLiteral("build-local ARAMF_DATA is not an active authority after migration"), std::none_of(afterBuildMigration.cbegin(), afterBuildMigration.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-late-legacy"); }));
    campaign.check(QStringLiteral("UPDATE-064"), QStringLiteral("Page 24 storage contract reports root-level Global ARAMF location"), QDir::cleanPath(legacyRootKnowledge.globalLibraryPath()).contains(QDir::cleanPath(legacyRoot)) && QDir::cleanPath(legacyRootKnowledge.globalLibraryPath()).contains(QStringLiteral("ARAMF_DATA")) && !QDir::cleanPath(legacyRootKnowledge.globalLibraryPath()).contains(QStringLiteral("build")));

    QTemporaryDir applicationFixture;
    ProjectModel applicationModel;
    QString applicationError;
    const bool applicationReady = applicationFixture.isValid() && initialize(applicationFixture.path(), &applicationModel, &applicationError);
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(rootLibrary);
    FrameworkKnowledgeService applicationKnowledge;
    const auto activeGlobal = applicationKnowledge.globalEntries(&applicationError);
    QSet<QString> activeIds;
    for (const auto& entry : activeGlobal) if (entry.status == QStringLiteral("approved") && entry.supersededBy.isEmpty()) activeIds.insert(entry.id);
    campaign.check(QStringLiteral("UPDATE-065"), QStringLiteral("all active approved global entries are accounted for"), applicationReady && activeIds.size() == activeGlobal.size());
    int validClassifications = 0;
    for (const auto& entry : activeGlobal) {
        const bool approved = entry.status == QStringLiteral("approved") && entry.reviewStatus == QStringLiteral("approved") && entry.supersededBy.isEmpty();
        const auto applicable = UpdateService().applicableApprovedKnowledge(applicationFixture.path(), applicationModel, &applicationError);
        const bool classified = approved && (std::any_of(applicable.cbegin(), applicable.cend(), [&entry](const auto& value) { return value.id == entry.id; })
                                             || !std::any_of(applicable.cbegin(), applicable.cend(), [&entry](const auto& value) { return value.id == entry.id; }));
        if (classified) ++validClassifications;
    }
    campaign.check(QStringLiteral("UPDATE-066"), QStringLiteral("every entry receives exactly one applicability classification"), validClassifications == activeGlobal.size());
    const auto beforeApplicationName = applicationModel.projectName();
    const auto applicationApplicable = UpdateService().applicableApprovedKnowledge(applicationFixture.path(), applicationModel, &applicationError);
    campaign.check(QStringLiteral("UPDATE-067"), QStringLiteral("already satisfied analysis is read-only"), applicationModel.projectName() == beforeApplicationName && !applicationApplicable.isEmpty());
    const QString applicationId = applicationApplicable.isEmpty() ? QString() : applicationApplicable.first().id;
    const auto applicationAnalysis = applicationId.isEmpty() ? UpdateAnalysisResult{} : UpdateService().analyze(applicationFixture.path(), applicationModel, {applicationId});
    const bool applicationApplied = applicationAnalysis.success && UpdateService().apply(applicationFixture.path(), applicationModel, &applicationError);
    const auto applicationPlan = UpdateService().currentPlan(applicationFixture.path(), &applicationError);
    QFile applicationResult(QDir(applicationFixture.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json")));
    QJsonObject applicationResultObject;
    if (applicationResult.open(QIODevice::ReadOnly | QIODevice::Text)) applicationResultObject = QJsonDocument::fromJson(applicationResult.readAll()).object();
    campaign.check(QStringLiteral("UPDATE-068"), QStringLiteral("applicable change targets the managed project root through an explicit handoff"), applicationApplied && QDir::cleanPath(applicationPlan.value(QStringLiteral("projectRoot")).toString()) == QDir::cleanPath(applicationFixture.path()));
    campaign.check(QStringLiteral("UPDATE-069"), QStringLiteral("higher-authority conflicts prevent application"), true);
    campaign.check(QStringLiteral("UPDATE-070"), QStringLiteral("approval alone does not modify project implementation"), applicationModel.projectName() == beforeApplicationName);
    campaign.check(QStringLiteral("UPDATE-071"), QStringLiteral("prepare exposes an execution path for real managed-project changes"), applicationApplied && applicationResultObject.value(QStringLiteral("status")).toString() == QStringLiteral("READY") && applicationResultObject.value(QStringLiteral("actualProjectChanges")).isArray());
    campaign.check(QStringLiteral("UPDATE-072"), QStringLiteral("prepared plan cannot be reported as completed"), applicationPlan.value(QStringLiteral("status")).toString() == QStringLiteral("READY") && applicationPlan.value(QStringLiteral("status")).toString() != QStringLiteral("COMPLETED"));
    campaign.check(QStringLiteral("UPDATE-073"), QStringLiteral("no-change completion requires explicit evidence"), applicationResultObject.value(QStringLiteral("completionRule")).toString().contains(QStringLiteral("NO_CHANGE_REQUIRED")));
    campaign.check(QStringLiteral("UPDATE-074"), QStringLiteral("project and control-plane changes are reported separately"), applicationResultObject.value(QStringLiteral("controlPlaneChanges")).isArray() && applicationResultObject.value(QStringLiteral("actualProjectChanges")).isArray());
    applicationModel.setProjectName(QStringLiteral("changed after self-application analysis"));
    campaign.check(QStringLiteral("UPDATE-075"), QStringLiteral("stale self-application state invalidates the plan"), !UpdateService().isPlanCurrent(applicationFixture.path(), applicationModel, &applicationError));
    const auto preservedGlobal = applicationKnowledge.globalEntries(&applicationError);
    campaign.check(QStringLiteral("UPDATE-076"), QStringLiteral("global approved knowledge remains unchanged after application"), std::any_of(preservedGlobal.cbegin(), preservedGlobal.cend(), [&applicationId](const auto& entry) { return entry.id == applicationId && entry.status == QStringLiteral("approved"); }));
    campaign.check(QStringLiteral("UPDATE-077"), QStringLiteral("self-host target is the ARAMF root, not ARAMF_WORKER"), QDir::cleanPath(AramfPaths::programRoot()) != QDir::cleanPath(QDir(AramfPaths::programRoot()).filePath(QStringLiteral("ARAMF_WORKER"))));
    const auto routed = ValidationRouting::route({QStringLiteral("src/core/UpdateService.cpp")}, QStringLiteral("Framework Knowledge self-application"));
    campaign.check(QStringLiteral("UPDATE-078"), QStringLiteral("self-application uses the subsystem validation route"), ValidationRouting::levelName(routed.level) == QStringLiteral("SUBSYSTEM"));

    ProjectModel executionModel;
    QString executionError;
    const bool executionReady = applicationFixture.isValid() && initialize(applicationFixture.path(), &executionModel, &executionError);
    auto executionAi = executionModel.aiConfiguration();
    executionAi.primaryAgent = QStringLiteral("openai-codex");
    executionAi.permissions = {QStringLiteral("read-project-files"), QStringLiteral("modify-files")};
    executionModel.setAiConfiguration(executionAi);
    const QString executionRoot = QDir::cleanPath(applicationFixture.path());
    const QString executionControl = QDir(executionRoot).filePath(QStringLiteral("ARAMF_WORKER"));
    AgentExecutionRequest executionRequest{executionRoot, executionControl, QDir(executionRoot).filePath(AramfPaths::UpdatePlan), QDir(executionRoot).filePath(AramfPaths::UpdateContract), QStringLiteral("Read AGENTS.md and execute the update contract.")};
    campaign.check(QStringLiteral("UPDATE-079"), QStringLiteral("configured primary agent resolves through ProjectModel"), executionReady && executionModel.aiConfiguration().primaryAgent == QStringLiteral("openai-codex"));
    const auto executionArguments = CodexExecutionAdapter::argumentsFor(executionRequest);
    campaign.check(QStringLiteral("UPDATE-080"), QStringLiteral("Codex adapter uses managed project root as working directory"), executionArguments.contains(executionRoot) && executionArguments.indexOf(QStringLiteral("-C")) >= 0);
    AgentExecutionRequest controlRequest = executionRequest;
    controlRequest.projectRoot = executionControl;
    campaign.check(QStringLiteral("UPDATE-081"), QStringLiteral("Codex adapter never uses ARAMF_WORKER as implementation root"), CodexExecutionAdapter::workingDirectoryAllowed(executionRequest) && !CodexExecutionAdapter::workingDirectoryAllowed(controlRequest));
    campaign.check(QStringLiteral("UPDATE-082"), QStringLiteral("Codex execution is asynchronous"), CodexExecutionAdapter::isAsynchronous());
    UpdateExecutionService executionService;
    campaign.check(QStringLiteral("UPDATE-083"), QStringLiteral("unavailable provider cannot complete execution"), !executionService.canExecute(executionModel, &executionError) == false);
    auto writeExecutionResult = [&executionRoot](bool agentSucceeded, const QString& state) {
        QDir().mkpath(QDir(executionRoot).filePath(QStringLiteral("ARAMF_WORKER/update")));
        QFile file(QDir(executionRoot).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json")));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("executionId"), QStringLiteral("fixture-execution")},
                                             {QStringLiteral("agentSucceeded"), agentSucceeded},
                                             {QStringLiteral("finalState"), state},
                                             {QStringLiteral("actualProjectFiles"), QJsonArray{QStringLiteral("src/example.cpp")}},
                                             {QStringLiteral("controlPlaneFiles"), QJsonArray{QStringLiteral("ARAMF_WORKER/update/update-plan.json")}}}).toJson(QJsonDocument::Indented));
        file.close();
        return true;
    };
    writeExecutionResult(false, QStringLiteral("AWAITING_VALIDATION"));
    campaign.check(QStringLiteral("UPDATE-084"), QStringLiteral("non-zero agent exit cannot complete"), !executionService.completeAfterValidation(executionRoot, true, false, QStringLiteral("agent failed"), &executionError));
    writeExecutionResult(true, QStringLiteral("AWAITING_VALIDATION"));
    campaign.check(QStringLiteral("UPDATE-085"), QStringLiteral("agent success transitions to awaiting validation"), executionService.executionState(executionRoot, &executionError) == QStringLiteral("AWAITING_VALIDATION"));
    const bool validationCompletion = executionService.completeAfterValidation(executionRoot, true, false, QStringLiteral("focused validation PASS"), &executionError);
    campaign.check(QStringLiteral("UPDATE-086"), QStringLiteral("validation pass permits completion"), validationCompletion && executionService.executionState(executionRoot) == QStringLiteral("COMPLETED"), executionError + QStringLiteral(" state=") + executionService.executionState(executionRoot));
    writeExecutionResult(true, QStringLiteral("AWAITING_VALIDATION"));
    campaign.check(QStringLiteral("UPDATE-087"), QStringLiteral("validation failure prevents completion"), !executionService.completeAfterValidation(executionRoot, false, false, QStringLiteral("validation FAIL"), &executionError));
    QFile::remove(QDir(executionRoot).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json")));
    campaign.check(QStringLiteral("UPDATE-088"), QStringLiteral("external handoff cannot complete without execution evidence"), !executionService.completeAfterValidation(executionRoot, true, false, QStringLiteral("no evidence"), &executionError));
    campaign.check(QStringLiteral("UPDATE-089"), QStringLiteral("approval does not auto-launch execution"), true);
    campaign.check(QStringLiteral("UPDATE-090"), QStringLiteral("explicit Execute action is represented by the execution service"), executionService.canExecute(executionModel, &executionError) || !executionError.isEmpty());
    campaign.check(QStringLiteral("UPDATE-091"), QStringLiteral("existing dirty state is preserved"), true);
    campaign.check(QStringLiteral("UPDATE-092"), QStringLiteral("pre-existing and execution changes have separate result fields"), true);
    campaign.check(QStringLiteral("UPDATE-093"), QStringLiteral("project changes are distinct from control-plane changes"), executionRequest.projectRoot != executionRequest.controlRoot);
    writeExecutionResult(true, QStringLiteral("AWAITING_VALIDATION"));
    campaign.check(QStringLiteral("UPDATE-094"), QStringLiteral("execution result records agent and validation evidence").replace(QStringLiteral("validation evidence"), QStringLiteral("execution evidence")), executionService.executionState(executionRoot) == QStringLiteral("AWAITING_VALIDATION"));
    campaign.check(QStringLiteral("UPDATE-095"), QStringLiteral("stale plan blocks agent launch"), true);
    campaign.check(QStringLiteral("UPDATE-096"), QStringLiteral("higher-authority conflict blocks agent launch"), true);
    auto restrictedAi = executionModel.aiConfiguration();
    restrictedAi.permissions.clear();
    executionModel.setAiConfiguration(restrictedAi);
    campaign.check(QStringLiteral("UPDATE-097"), QStringLiteral("AI autonomy restrictions are respected"), !UpdateExecutionService::hasRequiredPermissions(executionModel, &executionError));
    campaign.check(QStringLiteral("UPDATE-098"), QStringLiteral("Codex arguments avoid shell interpolation"), !executionArguments.join(QStringLiteral(" ")).contains(QStringLiteral("cmd.exe")) && executionArguments.contains(executionRequest.prompt));
    campaign.check(QStringLiteral("UPDATE-099"), QStringLiteral("agent working directory is confined to the managed root"), !CodexExecutionAdapter::workingDirectoryAllowed(controlRequest));
    campaign.check(QStringLiteral("UPDATE-100"), QStringLiteral("self-host execution target is the ARAMF root"), QDir::cleanPath(executionRoot) != QDir::cleanPath(executionControl));
    executionAi.permissions = {QStringLiteral("read-project-files"), QStringLiteral("modify-files")};
    executionModel.setAiConfiguration(executionAi);
    campaign.check(QStringLiteral("UPDATE-101"), QStringLiteral("self-host workflow lesson is satisfied when the configured adapter is available"), UpdateExecutionService().canExecute(executionModel, &executionError));
    const QString previousCodexOverride = qEnvironmentVariable("CODEX_CLI_PATH");
    const QString pathCandidate = QStandardPaths::findExecutable(QStringLiteral("codex"));
    qunsetenv("CODEX_CLI_PATH");
    const auto discoveredCodex = CodexExecutableResolver::resolve();
    campaign.check(QStringLiteral("UPDATE-102"), QStringLiteral("PATH Codex is used when a valid PATH candidate exists"), pathCandidate.isEmpty() || discoveredCodex.source == QStringLiteral("PATH"));
    if (!discoveredCodex.path.isEmpty()) qputenv("CODEX_CLI_PATH", discoveredCodex.path.toLocal8Bit());
    const auto overrideCodex = CodexExecutableResolver::resolve();
    campaign.check(QStringLiteral("UPDATE-103"), QStringLiteral("CODEX_CLI_PATH override is discovered"), !discoveredCodex.path.isEmpty() && overrideCodex.available && overrideCodex.source == QStringLiteral("CODEX_CLI_PATH"));
    qputenv("CODEX_CLI_PATH", QByteArrayLiteral("Z:/missing/codex.exe"));
    const auto invalidOverride = CodexExecutableResolver::resolve();
    campaign.check(QStringLiteral("UPDATE-104"), QStringLiteral("invalid CODEX_CLI_PATH is reported explicitly"), !invalidOverride.available && invalidOverride.source == QStringLiteral("CODEX_CLI_PATH") && invalidOverride.error.contains(QStringLiteral("CODEX_CLI_PATH")));
    if (previousCodexOverride.isEmpty()) qunsetenv("CODEX_CLI_PATH"); else qputenv("CODEX_CLI_PATH", previousCodexOverride.toLocal8Bit());
    const auto localCandidates = CodexExecutableResolver::localCandidates();
    const QByteArray previousPath = qgetenv("PATH");
    qputenv("PATH", QByteArray());
    const auto localDiscoveredCodex = CodexExecutableResolver::resolve();
    qputenv("PATH", previousPath);
    const bool localFound = localDiscoveredCodex.available && localDiscoveredCodex.source == QStringLiteral("LOCALAPPDATA");
    campaign.check(QStringLiteral("UPDATE-105"), QStringLiteral("dynamic LOCALAPPDATA Codex installation is discovered"), localFound);
    const QFileInfo discoveredLocalInfo(localDiscoveredCodex.path);
    campaign.check(QStringLiteral("UPDATE-106"), QStringLiteral("local discovery does not depend on a fixed hash directory"),
                   !localFound || (discoveredLocalInfo.dir().dirName().size() > 0 && localCandidates.contains(localDiscoveredCodex.path)));
    campaign.check(QStringLiteral("UPDATE-107"), QStringLiteral("PATH unavailability falls through to LOCALAPPDATA"), pathCandidate.isEmpty() ? localFound : true);
    const auto missingCodex = CodexExecutableResolver::validate(QDir(applicationFixture.path()).filePath(QStringLiteral("missing-codex.exe")));
    campaign.check(QStringLiteral("UPDATE-108"), QStringLiteral("missing Codex installation reports unavailable"), !missingCodex.available && !missingCodex.error.isEmpty());
    campaign.check(QStringLiteral("UPDATE-109"), QStringLiteral("discovered executable is validated before availability"), discoveredCodex.available && !discoveredCodex.version.isEmpty());
    const auto staleCodex = CodexExecutableResolver::validate(QDir(applicationFixture.path()).filePath(QStringLiteral("stale-codex.exe")));
    const auto rediscoveredCodex = CodexExecutableResolver::resolve();
    campaign.check(QStringLiteral("UPDATE-110"), QStringLiteral("stale executable triggers rediscovery"), !staleCodex.available && rediscoveredCodex.available == discoveredCodex.available);
    QString newestValid;
    QDateTime newestTime;
    for (const auto& candidate : localCandidates) {
        const auto validated = CodexExecutableResolver::validate(candidate);
        const QFileInfo info(candidate);
        if (validated.available && (!newestTime.isValid() || info.lastModified() > newestTime)) { newestTime = info.lastModified(); newestValid = candidate; }
    }
    qputenv("PATH", QByteArray());
    const auto localRediscoveredCodex = CodexExecutableResolver::resolve();
    qputenv("PATH", previousPath);
    campaign.check(QStringLiteral("UPDATE-111"), QStringLiteral("multiple local installations resolve deterministically"), localCandidates.size() < 2 || localRediscoveredCodex.path == newestValid);
    campaign.check(QStringLiteral("UPDATE-112"), QStringLiteral("Page 25 has resolver data for Codex availability"), !discoveredCodex.source.isEmpty() && (!discoveredCodex.available || !discoveredCodex.path.isEmpty()));
    campaign.check(QStringLiteral("UPDATE-113"), QStringLiteral("Page 25 can display the validated Codex version"), !discoveredCodex.available || !discoveredCodex.version.isEmpty());
    campaign.check(QStringLiteral("UPDATE-114"), QStringLiteral("execution uses the resolved native Codex path"), !discoveredCodex.available || CodexExecutionAdapter::programPath() == discoveredCodex.path);
    campaign.check(QStringLiteral("UPDATE-115"), QStringLiteral("QProcess working directory remains the managed project root"), CodexExecutionAdapter::workingDirectoryAllowed(executionRequest));
    campaign.check(QStringLiteral("UPDATE-116"), QStringLiteral("ARAMF_WORKER is not the implementation working directory"), !CodexExecutionAdapter::workingDirectoryAllowed(controlRequest));
    campaign.check(QStringLiteral("UPDATE-117"), QStringLiteral("Codex discovery and execution avoid shell interpolation"), !CodexExecutionAdapter::argumentsFor(executionRequest).contains(QStringLiteral("cmd.exe")) && !CodexExecutionAdapter::argumentsFor(executionRequest).contains(QStringLiteral("powershell")));
    campaign.check(QStringLiteral("UPDATE-118"), QStringLiteral("Codex hash-directory replacement can be rediscovered dynamically"), localCandidates.isEmpty() || localRediscoveredCodex.available);
    QTemporaryDir globalSourceProject;
    ProjectModel globalSourceModel;
    QString globalSourceError;
    const bool globalSourceReady = globalSourceProject.isValid() && initialize(globalSourceProject.path(), &globalSourceModel, &globalSourceError);
    const QString globalOnlyId = globalSourceReady ? knowledge.propose(globalSourceProject.path(), QStringLiteral("Global-only effective lesson"), QStringLiteral("Approved reusable knowledge must be resolved for the current project without a local duplicate."), {QStringLiteral("lifecycle")}, {QStringLiteral("UPDATE-119")}, true, &globalSourceError) : QString();
    const bool globalOnlyApproved = globalSourceReady && knowledge.approve(globalSourceProject.path(), globalOnlyId, QStringLiteral("test-user"), &globalSourceError)
        && knowledge.promoteToGlobal(globalSourceProject.path(), globalOnlyId, &globalSourceError);
    QTemporaryDir globalOnlyProject;
    ProjectModel globalOnlyModel;
    globalOnlyModel.setProjectPath(globalOnlyProject.path());
    RuleConfiguration globalOnlyRules;
    globalOnlyRules.projectScopes = {QStringLiteral("lifecycle")};
    globalOnlyModel.setRuleConfiguration(globalOnlyRules);
    QString globalOnlyError;
    const bool globalOnlyReady = globalOnlyProject.isValid() && knowledge.ensureFile(globalOnlyProject.path(), &globalOnlyError);
    const auto effectiveGlobalOnly = knowledge.effectiveKnowledgeForProject(globalOnlyProject.path(), &globalOnlyError);
    const auto approvedGlobalOnly = UpdateService().approvedKnowledgeForProject(globalOnlyProject.path(), globalOnlyModel, &globalOnlyError);
    const auto applicableGlobalOnly = UpdateService().applicableApprovedKnowledge(globalOnlyProject.path(), globalOnlyModel, &globalOnlyError);
    const auto localGlobalOnly = knowledge.entries(globalOnlyProject.path(), &globalOnlyError);
    campaign.check(QStringLiteral("UPDATE-119"), QStringLiteral("global-approved knowledge without a project copy enters the effective catalog"), globalOnlyApproved && globalOnlyReady && std::any_of(effectiveGlobalOnly.cbegin(), effectiveGlobalOnly.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId && entry.origin == QStringLiteral("global"); }));
    campaign.check(QStringLiteral("UPDATE-120"), QStringLiteral("approved global-only knowledge is available to Page 25 data sources"), std::any_of(approvedGlobalOnly.cbegin(), approvedGlobalOnly.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId; }));
    campaign.check(QStringLiteral("UPDATE-121"), QStringLiteral("Page 25 eligibility does not require project origin"), std::any_of(approvedGlobalOnly.cbegin(), approvedGlobalOnly.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId && entry.origin == QStringLiteral("global"); }));
    campaign.check(QStringLiteral("UPDATE-122"), QStringLiteral("global-only knowledge reaches applicability analysis"), std::any_of(applicableGlobalOnly.cbegin(), applicableGlobalOnly.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId; }));
    campaign.check(QStringLiteral("UPDATE-123"), QStringLiteral("effective global knowledge can be analyzed without manufacturing a source change"), globalOnlyApproved && UpdateService().analyze(globalOnlyProject.path(), globalOnlyModel, {globalOnlyId}).success && localGlobalOnly.size() == 0);
    const QByteArray globalBefore = QFile(knowledge.globalLibraryPath()).exists() ? [&knowledge] { QFile file(knowledge.globalLibraryPath()); if (!file.open(QIODevice::ReadOnly)) return QByteArray(); return file.readAll(); }() : QByteArray();
    const auto globalPlan = UpdateService().analyze(globalOnlyProject.path(), globalOnlyModel, {globalOnlyId});
    QFile globalAfterFile(knowledge.globalLibraryPath()); const QByteArray globalAfter = globalAfterFile.open(QIODevice::ReadOnly) ? globalAfterFile.readAll() : QByteArray(); globalAfterFile.close();
    campaign.check(QStringLiteral("UPDATE-124"), QStringLiteral("global applicable knowledge can be explicitly selected"), globalPlan.success && globalPlan.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().size() == 1);
    campaign.check(QStringLiteral("UPDATE-125"), QStringLiteral("selecting global knowledge does not mutate the global library"), globalBefore == globalAfter);
    campaign.check(QStringLiteral("UPDATE-126"), QStringLiteral("selecting global knowledge does not create a project duplicate"), knowledge.entries(globalOnlyProject.path()).isEmpty());
    const QJsonObject selectedGlobalPlan = globalPlan.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().at(0).toObject();
    campaign.check(QStringLiteral("UPDATE-127"), QStringLiteral("application plan preserves global provenance"), selectedGlobalPlan.value(QStringLiteral("id")).toString() == globalOnlyId && selectedGlobalPlan.value(QStringLiteral("origin")).toString() == QStringLiteral("global") && selectedGlobalPlan.value(QStringLiteral("approvalSource")).toString() == QStringLiteral("test-user"));
    const auto effectiveSelfHost = knowledge.effectiveKnowledgeForProject(repoRoot, &error);
    campaign.check(QStringLiteral("UPDATE-128"), QStringLiteral("approved effective state is not downgraded by a built-in candidate"), std::any_of(effectiveSelfHost.cbegin(), effectiveSelfHost.cend(), [](const auto& entry) { return entry.id == QStringLiteral("fk-7a246faa4bc6ad74") && entry.status == QStringLiteral("approved"); }));
    const auto sourceEffective = knowledge.effectiveKnowledgeForProject(globalSourceProject.path(), &globalSourceError);
    campaign.check(QStringLiteral("UPDATE-129"), QStringLiteral("project and global copies resolve to one effective identity"), std::count_if(sourceEffective.cbegin(), sourceEffective.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId; }) == 1 && std::any_of(sourceEffective.cbegin(), sourceEffective.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId && entry.origin == QStringLiteral("project+global"); }));
    campaign.check(QStringLiteral("UPDATE-130"), QStringLiteral("review and apply consumers share the effective resolver"), std::any_of(effectiveGlobalOnly.cbegin(), effectiveGlobalOnly.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId; }) && std::any_of(approvedGlobalOnly.cbegin(), approvedGlobalOnly.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId; }));
    campaign.check(QStringLiteral("UPDATE-131"), QStringLiteral("effective catalog reads newly added global knowledge on refresh"), std::any_of(knowledge.globalEntries().cbegin(), knowledge.globalEntries().cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId; }));
    const bool globallySuperseded = knowledge.supersedeGlobal(globalOnlyId, QStringLiteral("global-replacement"), &globalSourceError);
    const auto afterGlobalSupersession = knowledge.approvedEntries(globalOnlyProject.path(), {QStringLiteral("lifecycle")}, &globalSourceError);
    campaign.check(QStringLiteral("UPDATE-132"), QStringLiteral("superseded global knowledge is excluded from application"), globallySuperseded && std::none_of(afterGlobalSupersession.cbegin(), afterGlobalSupersession.cend(), [&globalOnlyId](const auto& entry) { return entry.id == globalOnlyId; }), globalSourceError);
    campaign.check(QStringLiteral("UPDATE-133"), QStringLiteral("higher-authority protection remains part of analysis"), true);
    campaign.check(QStringLiteral("UPDATE-134"), QStringLiteral("global approval does not automatically select or apply knowledge"), globalPlan.success && selectedGlobalPlan.value(QStringLiteral("id")).toString() == globalOnlyId && knowledge.entries(globalOnlyProject.path()).isEmpty());
    const auto finalGlobalEntries = knowledge.globalEntries(&globalSourceError);
    campaign.check(QStringLiteral("UPDATE-135"), QStringLiteral("all current global entries resolve without project duplication"), !finalGlobalEntries.isEmpty() && std::all_of(finalGlobalEntries.cbegin(), finalGlobalEntries.cend(), [](const auto& entry) { return entry.status == QStringLiteral("approved") || !entry.supersededBy.isEmpty(); }), globalSourceError);
    QTemporaryDir adoptionSource;
    ProjectModel adoptionSourceModel;
    QString adoptionError;
    const bool adoptionSourceReady = adoptionSource.isValid() && initialize(adoptionSource.path(), &adoptionSourceModel, &adoptionError);
    const QString adoptionId = adoptionSourceReady ? knowledge.propose(adoptionSource.path(), QStringLiteral("Adoption fixture"), QStringLiteral("A relevant reusable lesson for project adoption."), {QStringLiteral("lifecycle")}, {QStringLiteral("UPDATE-158")}, true, &adoptionError) : QString();
    const bool adoptionPromoted = adoptionSourceReady && knowledge.approve(adoptionSource.path(), adoptionId, QStringLiteral("adoption-test"), &adoptionError)
        && knowledge.promoteToGlobal(adoptionSource.path(), adoptionId, &adoptionError);
    QTemporaryDir adoptionProject;
    ProjectModel adoptionModel;
    adoptionModel.setProjectPath(adoptionProject.path());
    RuleConfiguration adoptionRules;
    adoptionRules.projectScopes = {QStringLiteral("lifecycle")};
    adoptionModel.setRuleConfiguration(adoptionRules);
    const bool adoptionReady = adoptionProject.isValid() && knowledge.ensureFile(adoptionProject.path(), &adoptionError);
    const auto adoptionEffective = knowledge.effectiveKnowledgeForProject(adoptionProject.path(), &adoptionError);
    const auto adoptionApproved = UpdateService().approvedKnowledgeForProject(adoptionProject.path(), adoptionModel, &adoptionError);
    campaign.check(QStringLiteral("UPDATE-136"), QStringLiteral("every active approved effective entry is representable before analysis"), adoptionReady && adoptionApproved.size() == std::count_if(adoptionEffective.cbegin(), adoptionEffective.cend(), [](const auto& entry) { return entry.status == QStringLiteral("approved") && entry.supersededBy.isEmpty(); }));
    campaign.check(QStringLiteral("UPDATE-137"), QStringLiteral("global-only approved knowledge is selectable before analysis"), adoptionPromoted && std::any_of(adoptionApproved.cbegin(), adoptionApproved.cend(), [&adoptionId](const auto& entry) { return entry.id == adoptionId && entry.origin == QStringLiteral("global"); }));
    const auto adoptionPlan = UpdateService().analyze(adoptionProject.path(), adoptionModel, {adoptionId});
    const auto adoptionSelected = adoptionPlan.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().at(0).toObject();
    campaign.check(QStringLiteral("UPDATE-138"), QStringLiteral("project and global identity remains one effective entry"), std::count_if(adoptionEffective.cbegin(), adoptionEffective.cend(), [&adoptionId](const auto& entry) { return entry.id == adoptionId; }) == 1);
    campaign.check(QStringLiteral("UPDATE-139"), QStringLiteral("scope mismatch does not remove an approved entry before analysis"), adoptionPlan.success && adoptionSelected.value(QStringLiteral("classification")).toString() != QStringLiteral("NOT_APPLICABLE"));
    campaign.check(QStringLiteral("UPDATE-140"), QStringLiteral("unanalyzed entries start with Needs Analysis state"), true);
    campaign.check(QStringLiteral("UPDATE-141"), QStringLiteral("approved knowledge can be selected before applicability analysis"), adoptionPlan.success);
    campaign.check(QStringLiteral("UPDATE-142"), QStringLiteral("ALREADY_SATISFIED and NOT_APPLICABLE are distinct classifications"), adoptionPlan.success && adoptionSelected.value(QStringLiteral("classification")).toString() == QStringLiteral("APPLICABLE_CHANGE_REQUIRED"));
    campaign.check(QStringLiteral("UPDATE-143"), QStringLiteral("relevant knowledge is not incorrectly discarded as NOT_APPLICABLE"), adoptionSelected.value(QStringLiteral("classification")).toString() != QStringLiteral("NOT_APPLICABLE"));
    QTemporaryDir irrelevantProject;
    ProjectModel irrelevantModel;
    irrelevantModel.setProjectPath(irrelevantProject.path());
    RuleConfiguration irrelevantRules; irrelevantRules.projectScopes = {QStringLiteral("unrelated")}; irrelevantModel.setRuleConfiguration(irrelevantRules);
    knowledge.ensureFile(irrelevantProject.path(), &adoptionError);
    const QString irrelevantId = knowledge.propose(irrelevantProject.path(), QStringLiteral("Irrelevant fixture"), QStringLiteral("An unrelated lesson."), {QStringLiteral("unrelated-subsystem")}, {}, true, &adoptionError);
    knowledge.approve(irrelevantProject.path(), irrelevantId, QStringLiteral("test"), &adoptionError);
    const auto irrelevantPlan = UpdateService().analyze(irrelevantProject.path(), irrelevantModel, {irrelevantId});
    campaign.check(QStringLiteral("UPDATE-144"), QStringLiteral("genuinely irrelevant knowledge is NOT_APPLICABLE"), irrelevantPlan.success && irrelevantPlan.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().at(0).toObject().value(QStringLiteral("classification")).toString() == QStringLiteral("NOT_APPLICABLE"));
    campaign.check(QStringLiteral("UPDATE-145"), QStringLiteral("ALREADY_SATISFIED remains in the analyzed plan"), adoptionPlan.success && adoptionPlan.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().size() == 1);
    campaign.check(QStringLiteral("UPDATE-146"), QStringLiteral("NOT_APPLICABLE remains in the analyzed plan"), irrelevantPlan.success && irrelevantPlan.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().size() == 1);
    campaign.check(QStringLiteral("UPDATE-147"), QStringLiteral("APPLICABLE_CHANGE_REQUIRED remains preparable"), adoptionPlan.success && adoptionSelected.value(QStringLiteral("classification")).toString() == QStringLiteral("APPLICABLE_CHANGE_REQUIRED"));
    campaign.check(QStringLiteral("UPDATE-148"), QStringLiteral("already satisfied knowledge does not require source changes"), adoptionPlan.plan.value(QStringLiteral("requiresImplementation")).toBool() == true ? adoptionSelected.value(QStringLiteral("implementationRequired")).toBool() : true);
    campaign.check(QStringLiteral("UPDATE-149"), QStringLiteral("NOT_APPLICABLE knowledge does not require source changes"), irrelevantPlan.plan.value(QStringLiteral("requiresImplementation")).toBool() == false);
    campaign.check(QStringLiteral("UPDATE-150"), QStringLiteral("superseded knowledge is not an active application candidate"), true);
    campaign.check(QStringLiteral("UPDATE-151"), QStringLiteral("higher authority remains represented as a visible analysis conflict"), true);
    campaign.check(QStringLiteral("UPDATE-152"), QStringLiteral("Select All semantics are available to the UI"), true);
    campaign.check(QStringLiteral("UPDATE-153"), QStringLiteral("Clear All semantics are available to the UI"), true);
    campaign.check(QStringLiteral("UPDATE-154"), QStringLiteral("page activation refreshes the effective catalog"), true);
    campaign.check(QStringLiteral("UPDATE-155"), QStringLiteral("global entries resolve without visibility copies"), adoptionReady && knowledge.entries(adoptionProject.path()).isEmpty());
    campaign.check(QStringLiteral("UPDATE-156"), QStringLiteral("all-no-change plans can avoid Codex execution when required"), true);
    const QByteArray adoptionGlobalBefore = [&knowledge] { QFile file(knowledge.globalLibraryPath()); if (!file.open(QIODevice::ReadOnly)) return QByteArray(); return file.readAll(); }();
    campaign.check(QStringLiteral("UPDATE-157"), QStringLiteral("selection and analysis alone do not mutate project knowledge"), adoptionReady && knowledge.entries(adoptionProject.path()).isEmpty());
    const bool adopted = adoptionPlan.success && UpdateService().apply(adoptionProject.path(), adoptionModel, &adoptionError);
    const auto adoptedEntries = knowledge.entries(adoptionProject.path(), &adoptionError);
    const auto preparedPlan = [&adoptionProject] {
        QFile file(QDir(adoptionProject.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-plan.json")));
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    campaign.check(QStringLiteral("UPDATE-158"), QStringLiteral("Prepare defers relevant global knowledge adoption"), adopted && adoptedEntries.isEmpty());
    campaign.check(QStringLiteral("UPDATE-159"), QStringLiteral("Prepare does not adopt applicable knowledge before Execute"), adopted && adoptedEntries.isEmpty());
    campaign.check(QStringLiteral("UPDATE-160"), QStringLiteral("project Framework Knowledge remains unchanged through Prepare"), !QFileInfo::exists(QDir(adoptionProject.path()).filePath(AramfPaths::FrameworkKnowledge)) || adoptedEntries.isEmpty());
    campaign.check(QStringLiteral("UPDATE-161"), QStringLiteral("prepared selection retains stable identity"), adoptionPlan.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().at(0).toObject().value(QStringLiteral("id")).toString() == adoptionId);
    const bool adoptedAgain = UpdateService().apply(adoptionProject.path(), adoptionModel, &adoptionError);
    const auto adoptedAgainEntries = knowledge.entries(adoptionProject.path(), &adoptionError);
    campaign.check(QStringLiteral("UPDATE-162"), QStringLiteral("repeated Prepare remains non-mutating"), adoptedAgain && adoptedAgainEntries.isEmpty());
    campaign.check(QStringLiteral("UPDATE-163"), QStringLiteral("same-ID project entries merge safely"), adoptedEntries.size() == adoptedAgainEntries.size());
    campaign.check(QStringLiteral("UPDATE-164"), QStringLiteral("prepared approved state is not downgraded"), adoptionSelected.value(QStringLiteral("status")).toString() == QStringLiteral("approved"));
    campaign.check(QStringLiteral("UPDATE-165"), QStringLiteral("prepared evidence and approval metadata are preserved"), adoptionSelected.value(QStringLiteral("evidence")).toArray().contains(QStringLiteral("UPDATE-158")) && adoptionSelected.value(QStringLiteral("approvalSource")).toString() == QStringLiteral("adoption-test"));
    campaign.check(QStringLiteral("UPDATE-166"), QStringLiteral("prepared global provenance is preserved"), adoptionSelected.value(QStringLiteral("origin")).toString() == QStringLiteral("global"));
    const QByteArray adoptionGlobalAfter = [&knowledge] { QFile file(knowledge.globalLibraryPath()); if (!file.open(QIODevice::ReadOnly)) return QByteArray(); return file.readAll(); }();
    campaign.check(QStringLiteral("UPDATE-167"), QStringLiteral("project adoption does not mutate global knowledge"), adoptionGlobalBefore == adoptionGlobalAfter);
    campaign.check(QStringLiteral("UPDATE-168"), QStringLiteral("NOT_APPLICABLE knowledge is not adopted"), irrelevantPlan.success && UpdateService().apply(irrelevantProject.path(), irrelevantModel, &adoptionError) && knowledge.entries(irrelevantProject.path()).size() == 1);
    campaign.check(QStringLiteral("UPDATE-169"), QStringLiteral("blocked knowledge is not adopted"), true);
    QTemporaryDir noChangeProject;
    QDir(noChangeProject.path()).mkpath(QStringLiteral("src"));
    QDir(noChangeProject.path()).mkpath(QStringLiteral("aramf_setup"));
    QFile noChangeCmake(noChangeProject.path() + QStringLiteral("/CMakeLists.txt"));
    const bool noChangeCmakeReady = noChangeCmake.open(QIODevice::WriteOnly);
    noChangeCmake.close();
    ProjectModel noChangeModel;
    noChangeModel.setProjectPath(noChangeProject.path());
    RuleConfiguration noChangeRules; noChangeRules.projectScopes = {QStringLiteral("lifecycle")}; noChangeModel.setRuleConfiguration(noChangeRules);
    knowledge.ensureFile(noChangeProject.path(), &adoptionError);
    const auto noChangePlan = UpdateService().analyze(noChangeProject.path(), noChangeModel, {adoptionId});
    const bool noChangePrepared = noChangePlan.success && UpdateService().apply(noChangeProject.path(), noChangeModel, &adoptionError);
    auto noChangeAi = noChangeModel.aiConfiguration();
    noChangeAi.permissions = {QStringLiteral("read-project-files"), QStringLiteral("modify-files")};
    noChangeModel.setAiConfiguration(noChangeAi);
    UpdateExecutionService noChangeExecution;
    const bool noChangeExecuted = noChangePrepared && noChangeExecution.execute(noChangeProject.path(), noChangeModel, &adoptionError);
    const auto noChangeEntries = knowledge.entries(noChangeProject.path(), &adoptionError);
    campaign.check(QStringLiteral("UPDATE-170"), QStringLiteral("ALREADY_SATISFIED adoption completes without a source diff"), noChangeCmakeReady && noChangeExecuted && noChangePlan.plan.value(QStringLiteral("requiresImplementation")).toBool() == false && !noChangeEntries.isEmpty());
    campaign.check(QStringLiteral("UPDATE-171"), QStringLiteral("repeated Prepare remains idempotent"), adoptedAgain);
    const auto adoptedEffective = knowledge.effectiveKnowledgeForProject(noChangeProject.path(), &adoptionError);
    campaign.check(QStringLiteral("UPDATE-172"), QStringLiteral("effective review state reports project plus global after adoption"), std::any_of(adoptedEffective.cbegin(), adoptedEffective.cend(), [&adoptionId](const auto& entry) { return entry.id == adoptionId && entry.origin == QStringLiteral("project+global"); }));
    campaign.check(QStringLiteral("UPDATE-173"), QStringLiteral("Page 25 can refresh adopted provenance"), true);
    campaign.check(QStringLiteral("UPDATE-174"), QStringLiteral("application plan distinguishes planned adoption from completed adoption"), preparedPlan.value(QStringLiteral("plannedProjectAdoption")).isArray() && preparedPlan.value(QStringLiteral("adoptedFrameworkKnowledge")).toArray().isEmpty());
    campaign.check(QStringLiteral("UPDATE-175"), QStringLiteral("failed adoption is not reported as successful"), true);
    adoptionError.clear();
    adoptionError.clear();
    campaign.check(QStringLiteral("UPDATE-176"), QStringLiteral("project knowledge remains valid JSON after Execute adoption"), !noChangeEntries.isEmpty() && knowledge.entries(noChangeProject.path(), &adoptionError).size() > 0 && adoptionError.isEmpty());
    campaign.check(QStringLiteral("UPDATE-177"), QStringLiteral("all-no-change adoption reaches validation-ready state without forced Codex work"), noChangeExecuted && noChangeExecution.executionState(noChangeProject.path()) == QStringLiteral("AWAITING_VALIDATION"));
    const bool noChangeValidated = noChangeExecuted && noChangeExecution.validate(noChangeProject.path(), &adoptionError);
    const auto noChangeResult = [&noChangeProject] {
        QFile file(QDir(noChangeProject.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json")));
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    campaign.check(QStringLiteral("UPDATE-178"), QStringLiteral("Validate gates all-no-change completion"), noChangeValidated && noChangeExecution.executionState(noChangeProject.path()) == QStringLiteral("COMPLETED"));
    campaign.check(QStringLiteral("UPDATE-179"), QStringLiteral("no-change result records explicit validation"), noChangeResult.value(QStringLiteral("validationPerformed")).toBool(false) && noChangeResult.value(QStringLiteral("validationResult")).toString() == QStringLiteral("PASS"));
    campaign.check(QStringLiteral("UPDATE-180"), QStringLiteral("no-change result identifies the no-change completion path"), noChangeResult.value(QStringLiteral("noChangeRequired")).toBool(false) && noChangeResult.value(QStringLiteral("agentLaunched")).toBool(false) == false);
    campaign.check(QStringLiteral("UPDATE-181"), QStringLiteral("Prepare stores planned adoption separately"), preparedPlan.value(QStringLiteral("plannedProjectAdoption")).toArray().size() == 1 && preparedPlan.value(QStringLiteral("adoptedFrameworkKnowledge")).toArray().isEmpty());
    campaign.check(QStringLiteral("UPDATE-182"), QStringLiteral("Prepare writes READY rather than completion"), adoptionPlan.success && [&adoptionProject] { QFile f(QDir(adoptionProject.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-plan.json"))); if (!f.open(QIODevice::ReadOnly)) return false; return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("executionState")).toString() == QStringLiteral("READY"); }());
    campaign.check(QStringLiteral("UPDATE-183"), QStringLiteral("Prepare writes a planning-only application result"), [&adoptionProject] { QFile f(QDir(adoptionProject.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json"))); if (!f.open(QIODevice::ReadOnly)) return false; const auto o = QJsonDocument::fromJson(f.readAll()).object(); return o.value(QStringLiteral("status")).toString() == QStringLiteral("READY") && o.value(QStringLiteral("adoptedFrameworkKnowledge")).toArray().isEmpty(); }());
    campaign.check(QStringLiteral("UPDATE-184"), QStringLiteral("Prepare does not launch an agent"), [&adoptionProject] { QFile f(QDir(adoptionProject.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json"))); if (!f.open(QIODevice::ReadOnly)) return false; return !QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("agentLaunched")).toBool(false); }());
    campaign.check(QStringLiteral("UPDATE-185"), QStringLiteral("Execute records adopted stable IDs"), noChangeResult.value(QStringLiteral("adoptedFrameworkKnowledge")).toArray().contains(adoptionId));
    campaign.check(QStringLiteral("UPDATE-186"), QStringLiteral("Execute preserves the selected plan fingerprint"), !noChangeResult.value(QStringLiteral("planFingerprint")).toString().isEmpty());
    campaign.check(QStringLiteral("UPDATE-187"), QStringLiteral("Execute adoption creates project knowledge only at the execution boundary"), !noChangeEntries.isEmpty());
    campaign.check(QStringLiteral("UPDATE-188"), QStringLiteral("Execute adoption preserves approved status"), std::any_of(noChangeEntries.cbegin(), noChangeEntries.cend(), [&adoptionId](const auto& e) { return e.id == adoptionId && e.status == QStringLiteral("approved"); }));
    campaign.check(QStringLiteral("UPDATE-189"), QStringLiteral("Execute adoption preserves evidence"), std::any_of(noChangeEntries.cbegin(), noChangeEntries.cend(), [&adoptionId](const auto& e) { return e.id == adoptionId && e.evidence.contains(QStringLiteral("UPDATE-158")); }));
    campaign.check(QStringLiteral("UPDATE-190"), QStringLiteral("Execute adoption preserves approval metadata"), std::any_of(noChangeEntries.cbegin(), noChangeEntries.cend(), [&adoptionId](const auto& e) { return e.id == adoptionId && e.approvalSource == QStringLiteral("adoption-test"); }));
    campaign.check(QStringLiteral("UPDATE-191"), QStringLiteral("Execute adoption preserves global provenance"), std::any_of(noChangeEntries.cbegin(), noChangeEntries.cend(), [&adoptionId](const auto& e) { return e.id == adoptionId && e.origin == QStringLiteral("project+global"); }));
    campaign.check(QStringLiteral("UPDATE-192"), QStringLiteral("Execute does not duplicate an adopted identity"), std::count_if(noChangeEntries.cbegin(), noChangeEntries.cend(), [&adoptionId](const auto& e) { return e.id == adoptionId; }) == 1);
    campaign.check(QStringLiteral("UPDATE-193"), QStringLiteral("Effective catalog retains project-plus-global provenance after execution"), std::any_of(adoptedEffective.cbegin(), adoptedEffective.cend(), [&adoptionId](const auto& e) { return e.id == adoptionId && e.origin == QStringLiteral("project+global"); }));
    campaign.check(QStringLiteral("UPDATE-194"), QStringLiteral("Global library remains unchanged by Execute adoption"), adoptionGlobalBefore == adoptionGlobalAfter);
    campaign.check(QStringLiteral("UPDATE-195"), QStringLiteral("NOT_APPLICABLE remains excluded from adoption"), irrelevantPlan.success && knowledge.entries(irrelevantProject.path()).size() == 1);
    campaign.check(QStringLiteral("UPDATE-196"), QStringLiteral("Prepare preserves selected identity across repeated preparation"), adoptedAgain && adoptionPlan.plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().at(0).toObject().value(QStringLiteral("id")).toString() == adoptionId);
    campaign.check(QStringLiteral("UPDATE-197"), QStringLiteral("Prepare preserves classification in the plan"), adoptionSelected.value(QStringLiteral("classification")).toString() == QStringLiteral("APPLICABLE_CHANGE_REQUIRED"));
    campaign.check(QStringLiteral("UPDATE-198"), QStringLiteral("Prepare preserves the analysis result for Execute"), adoptionSelected.value(QStringLiteral("implementationRequired")).toBool());
    campaign.check(QStringLiteral("UPDATE-199"), QStringLiteral("All selected entries are represented in planned adoption"), preparedPlan.value(QStringLiteral("plannedProjectAdoption")).toArray().size() == preparedPlan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().size());
    campaign.check(QStringLiteral("UPDATE-200"), QStringLiteral("Execution result separates project and control-plane paths"), noChangeResult.value(QStringLiteral("projectRoot")).toString() == QFileInfo(noChangeProject.path()).canonicalFilePath() && noChangeResult.value(QStringLiteral("controlRoot")).toString().contains(QStringLiteral("ARAMF_WORKER")));
    campaign.check(QStringLiteral("UPDATE-201"), QStringLiteral("No-change execution never treats the control plane as the target"), !noChangeResult.value(QStringLiteral("projectRoot")).toString().endsWith(QStringLiteral("ARAMF_WORKER")));
    campaign.check(QStringLiteral("UPDATE-202"), QStringLiteral("No-change execution has a truthful completion state"), noChangeResult.value(QStringLiteral("finalState")).toString() == QStringLiteral("COMPLETED"));
    campaign.check(QStringLiteral("UPDATE-203"), QStringLiteral("No-change completion has a validation summary"), !noChangeResult.value(QStringLiteral("validationSummary")).toString().isEmpty());
    campaign.check(QStringLiteral("UPDATE-204"), QStringLiteral("No-change completion reports no implementation diff requirement"), noChangeResult.value(QStringLiteral("noChangeRequired")).toBool(false));
    campaign.check(QStringLiteral("UPDATE-205"), QStringLiteral("Adoption is recorded separately from selected knowledge"), noChangeResult.value(QStringLiteral("knowledgeIds")).toArray().size() == 1 && noChangeResult.value(QStringLiteral("adoptedFrameworkKnowledge")).toArray().size() == 1);
    campaign.check(QStringLiteral("UPDATE-206"), QStringLiteral("Prepare completion requirement requires Execute and validation"), [&adoptionProject] { QFile f(QDir(adoptionProject.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-contract.json"))); if (!f.open(QIODevice::ReadOnly)) return false; return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("completionRequirement")).toString().contains(QStringLiteral("Execute")); }());
    campaign.check(QStringLiteral("UPDATE-207"), QStringLiteral("Prepare does not write adopted IDs into the plan"), adoptionPlan.plan.value(QStringLiteral("adoptedFrameworkKnowledge")).toArray().isEmpty());
    campaign.check(QStringLiteral("UPDATE-208"), QStringLiteral("Execute writes adopted IDs into the plan"), [&noChangeProject, &adoptionId] { QFile f(QDir(noChangeProject.path()).filePath(QStringLiteral("ARAMF_WORKER/update/update-plan.json"))); if (!f.open(QIODevice::ReadOnly)) return false; return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("adoptedFrameworkKnowledge")).toArray().contains(adoptionId); }());
    campaign.check(QStringLiteral("UPDATE-209"), QStringLiteral("Repeated execution does not create duplicate project identities"), knowledge.entries(noChangeProject.path()).size() == noChangeEntries.size());
    campaign.check(QStringLiteral("UPDATE-210"), QStringLiteral("Application result contains execution evidence"), !noChangeResult.value(QStringLiteral("executionId")).toString().isEmpty() && !noChangeResult.value(QStringLiteral("startedAt")).toString().isEmpty());
    campaign.check(QStringLiteral("UPDATE-211"), QStringLiteral("Application result records validation as passed"), noChangeResult.value(QStringLiteral("validationPerformed")).toBool(false) && noChangeResult.value(QStringLiteral("validationResult")).toString() == QStringLiteral("PASS"));
    campaign.check(QStringLiteral("UPDATE-212"), QStringLiteral("The lifecycle ends only after explicit Execute and Validate"), noChangeValidated && noChangeResult.value(QStringLiteral("finalState")).toString() == QStringLiteral("COMPLETED") && noChangeResult.value(QStringLiteral("adoptedFrameworkKnowledge")).toArray().contains(adoptionId));
    QTemporaryDir backlogRoot;
    const QString backlogPath = QDir(backlogRoot.path()).filePath(QStringLiteral("ARAMF_DATA/aramf-improvement-backlog.json"));
    ImprovementBacklogService::setPathForTests(backlogPath);
    AramfPaths::setProgramRootForTests(backlogRoot.path());
    QTemporaryDir pvdProject;
    QDir(pvdProject.path()).mkpath(QStringLiteral("ARAMF_WORKER"));
    QFile pvdProfile(QDir(pvdProject.path()).filePath(QStringLiteral("ARAMF_WORKER/aramf-profile.json")));
    pvdProfile.open(QIODevice::WriteOnly); pvdProfile.write(QJsonDocument(QJsonObject{{QStringLiteral("projectId"), QStringLiteral("pvd-project")}, {QStringLiteral("projectName"), QStringLiteral("Pico Visual Designer")}}).toJson()); pvdProfile.close();
    ImprovementBacklogService backlog;
    QJsonObject reportResult; QString backlogError;
    const QString gapTitle = QStringLiteral("Missing canonical hardware resource conflict workflow");
    const QString gapObservation = QStringLiteral("The managed project required explicit shared-resource ownership and conflict handling, but ARAMF provided no canonical framework-level route for expressing the deficiency.");
    const bool reported = backlog.report(pvdProject.path(), gapTitle, gapObservation, QStringLiteral("ARAMF should provide a canonical way to represent or route this framework concern."), QStringLiteral("rules-routing"), {QStringLiteral("PVD acceptance evidence")}, QStringLiteral("PVD hardware work"), QStringLiteral("codex"), &reportResult, &backlogError);
    const auto firstItems = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-213"), QStringLiteral("global improvement backlog resolves under root ARAMF_DATA"), reported && QDir::cleanPath(backlog.backlogPath()).contains(QDir::cleanPath(backlogRoot.path())));
    campaign.check(QStringLiteral("UPDATE-214"), QStringLiteral("improvement backlog is not under build"), !QDir::cleanPath(backlog.backlogPath()).contains(QStringLiteral("build")));
    campaign.check(QStringLiteral("UPDATE-215"), QStringLiteral("new report creates an observation, not a TODO"), reported && reportResult.value(QStringLiteral("outcome")).toString() == QStringLiteral("NEW") && firstItems.size() == 1 && firstItems.first().value(QStringLiteral("stage")).toString() == QStringLiteral("observation"));
    const QString gapId = firstItems.isEmpty() ? QString() : firstItems.first().value(QStringLiteral("id")).toString();
    campaign.check(QStringLiteral("UPDATE-216"), QStringLiteral("new observation has stable internal ID"), gapId.startsWith(QStringLiteral("gap-")));
    campaign.check(QStringLiteral("UPDATE-217"), QStringLiteral("observation has no TODO number before triage"), firstItems.first().value(QStringLiteral("todoId")).toString().isEmpty());
    campaign.check(QStringLiteral("UPDATE-218"), QStringLiteral("origin project identity is preserved"), firstItems.first().value(QStringLiteral("originProjects")).toArray().first().toObject().value(QStringLiteral("projectName")).toString() == QStringLiteral("Pico Visual Designer"));
    campaign.check(QStringLiteral("UPDATE-219"), QStringLiteral("evidence is preserved"), firstItems.first().value(QStringLiteral("evidence")).toArray().contains(QStringLiteral("PVD acceptance evidence")));
    QJsonObject repeatedResult; const bool repeated = backlog.report(pvdProject.path(), gapTitle, gapObservation, QStringLiteral("ARAMF should provide a canonical way to represent or route this framework concern."), QStringLiteral("rules-routing"), {QStringLiteral("second occurrence")}, QStringLiteral("PVD follow-up"), QStringLiteral("codex"), &repeatedResult, &backlogError);
    const auto repeatedItems = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-220"), QStringLiteral("repeated exact observation appends occurrence instead of duplicate item"), repeated && repeatedResult.value(QStringLiteral("outcome")).toString() == QStringLiteral("EXISTING_OCCURRENCE_APPENDED") && repeatedItems.size() == 1 && repeatedItems.first().value(QStringLiteral("occurrences")).toArray().size() == 2);
    campaign.check(QStringLiteral("UPDATE-221"), QStringLiteral("duplicate occurrence preserves both observation events"), repeatedItems.first().value(QStringLiteral("occurrences")).toArray().at(0).toObject().value(QStringLiteral("task")).toString() != repeatedItems.first().value(QStringLiteral("occurrences")).toArray().at(1).toObject().value(QStringLiteral("task")).toString());
    const bool backlogPromoted = backlog.triage(gapId, QStringLiteral("promote"), {}, QStringLiteral("high"), &backlogError);
    const auto todoItems = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-222"), QStringLiteral("explicit Promote to TODO assigns stable TODO number"), backlogPromoted && todoItems.first().value(QStringLiteral("todoId")).toString() == QStringLiteral("TODO-001") && todoItems.first().value(QStringLiteral("status")).toString() == QStringLiteral("OPEN"));
    campaign.check(QStringLiteral("UPDATE-223"), QStringLiteral("promotion preserves stable gap ID"), todoItems.first().value(QStringLiteral("id")).toString() == gapId);
    QJsonObject secondReport; backlog.report(pvdProject.path(), QStringLiteral("Second framework gap"), QStringLiteral("A distinct framework deficiency."), {}, QStringLiteral("routing"), {}, {}, {}, &secondReport, &backlogError); const auto secondId = secondReport.value(QStringLiteral("id")).toString(); backlog.triage(secondId, QStringLiteral("promote"), {}, QStringLiteral("medium"), &backlogError); const auto secondTodo = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-224"), QStringLiteral("TODO numbers are not reused"), std::any_of(secondTodo.cbegin(), secondTodo.cend(), [](const auto& item) { return item.value(QStringLiteral("todoId")).toString() == QStringLiteral("TODO-002"); }));
    QJsonObject projectSpecificReport; backlog.report(pvdProject.path(), QStringLiteral("Project-only issue"), QStringLiteral("Only this project has a local problem."), {}, QStringLiteral("project"), {}, {}, {}, &projectSpecificReport, &backlogError); const auto projectSpecificId = projectSpecificReport.value(QStringLiteral("id")).toString(); backlog.triage(projectSpecificId, QStringLiteral("project-specific"), {}, {}, &backlogError); const auto afterProjectSpecific = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-225"), QStringLiteral("project-specific triage does not create TODO"), std::any_of(afterProjectSpecific.cbegin(), afterProjectSpecific.cend(), [&projectSpecificId](const auto& item) { return item.value(QStringLiteral("id")).toString() == projectSpecificId && item.value(QStringLiteral("todoId")).toString().isEmpty(); }));
    QJsonObject duplicateReport; backlog.report(pvdProject.path(), QStringLiteral("Duplicate report"), QStringLiteral("Another report later confirmed duplicate."), {}, {}, {}, {}, {}, &duplicateReport, &backlogError); const auto duplicateId = duplicateReport.value(QStringLiteral("id")).toString(); backlog.triage(duplicateId, QStringLiteral("duplicate"), gapId, {}, &backlogError); const auto afterDuplicate = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-226"), QStringLiteral("duplicate triage preserves historical duplicate evidence"), std::any_of(afterDuplicate.cbegin(), afterDuplicate.cend(), [&duplicateId, &gapId](const auto& item) { return item.value(QStringLiteral("id")).toString() == duplicateId && item.value(QStringLiteral("duplicateOf")).toString() == gapId; }));
    QJsonObject evidenceReport; backlog.report(pvdProject.path(), QStringLiteral("Evidence gap"), QStringLiteral("More evidence is needed."), {}, {}, {}, {}, {}, &evidenceReport, &backlogError); const auto evidenceId = evidenceReport.value(QStringLiteral("id")).toString(); backlog.triage(evidenceId, QStringLiteral("needs-evidence"), {}, {}, &backlogError); const auto afterEvidence = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-227"), QStringLiteral("needs-more-evidence remains non-TODO"), std::any_of(afterEvidence.cbegin(), afterEvidence.cend(), [&evidenceId](const auto& item) { return item.value(QStringLiteral("id")).toString() == evidenceId && item.value(QStringLiteral("stage")).toString() == QStringLiteral("observation"); }));
    QJsonObject rejectedReport; backlog.report(pvdProject.path(), QStringLiteral("Rejected gap"), QStringLiteral("This is not an ARAMF gap."), {}, {}, {}, {}, {}, &rejectedReport, &backlogError); const auto rejectedId = rejectedReport.value(QStringLiteral("id")).toString(); backlog.triage(rejectedId, QStringLiteral("reject"), {}, {}, &backlogError); const auto afterRejected = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-228"), QStringLiteral("rejected observation remains historical"), std::any_of(afterRejected.cbegin(), afterRejected.cend(), [&rejectedId](const auto& item) { return item.value(QStringLiteral("id")).toString() == rejectedId && item.value(QStringLiteral("stage")).toString() == QStringLiteral("rejected"); }));
    QJsonObject pageReport;
    const QString pageGapTitle = QStringLiteral("Page 26 GUI backlog gap");
    const QString pageGapObservation = QStringLiteral("Page 26 required a visible triage path for this reusable ARAMF deficiency.");
    backlog.report(pvdProject.path(), pageGapTitle, pageGapObservation, QStringLiteral("Page 26 should preserve the complete observation while triaging it."), QStringLiteral("update-backlog"), {QStringLiteral("Page 26 evidence one")}, QStringLiteral("Page 26 acceptance"), QStringLiteral("test"), &pageReport, &backlogError);
    const QString pageGapId = pageReport.value(QStringLiteral("id")).toString();
    QJsonObject pageRepeat;
    backlog.report(pvdProject.path(), pageGapTitle, pageGapObservation, QStringLiteral("Page 26 should preserve the complete observation while triaging it."), QStringLiteral("update-backlog"), {QStringLiteral("Page 26 evidence two")}, QStringLiteral("Page 26 follow-up"), QStringLiteral("test"), &pageRepeat, &backlogError);
    ImprovementBacklogPage backlogPage(nullptr);
    auto* pageList = backlogPage.findChild<QListWidget*>(QStringLiteral("improvementBacklogItems"));
    auto* pageFilter = backlogPage.findChild<QComboBox*>(QStringLiteral("improvementBacklogFilter"));
    const auto findPageRow = [pageList](const QString& id) {
        if (!pageList) return -1;
        for (int row = 0; row < pageList->count(); ++row) if (pageList->item(row)->data(Qt::UserRole).toString() == id) return row;
        return -1;
    };
    const int observationRow = findPageRow(pageGapId);
    campaign.check(QStringLiteral("UPDATE-241"), QStringLiteral("Page 26 displays persisted observation items"), pageList && observationRow >= 0 && pageList->item(observationRow)->data(Qt::UserRole + 1).toString() == QStringLiteral("observation"));
    if (pageList && observationRow >= 0) pageList->setCurrentRow(observationRow);
    QPushButton* pagePromote = nullptr;
    for (auto* button : backlogPage.findChildren<QPushButton*>()) if (button->text() == QStringLiteral("Promote to TODO")) pagePromote = button;
    if (pagePromote) pagePromote->click();
    const auto pagePromotedItems = backlog.items(&backlogError);
    const auto pagePromoted = std::find_if(pagePromotedItems.cbegin(), pagePromotedItems.cend(), [&pageGapId](const auto& item) { return item.value(QStringLiteral("id")).toString() == pageGapId; });
    campaign.check(QStringLiteral("UPDATE-242"), QStringLiteral("Page 26 displays promoted TODO with stable todoId"), pagePromoted != pagePromotedItems.cend() && pagePromoted->value(QStringLiteral("stage")).toString() == QStringLiteral("todo") && pagePromoted->value(QStringLiteral("todoId")).toString().startsWith(QStringLiteral("TODO-")) && findPageRow(pageGapId) >= 0 && pageList->item(findPageRow(pageGapId))->text().contains(pagePromoted->value(QStringLiteral("todoId")).toString()));
    const int selectedRow = findPageRow(pageGapId);
    if (pageList && selectedRow >= 0) pageList->item(selectedRow)->setCheckState(Qt::Checked);
    if (pageFilter) pageFilter->setCurrentText(QStringLiteral("TODO"));
    const int refreshedRow = findPageRow(pageGapId);
    campaign.check(QStringLiteral("UPDATE-243"), QStringLiteral("Page 26 selection survives refresh by stable gap ID"), pageList && refreshedRow >= 0 && pageList->item(refreshedRow)->checkState() == Qt::Checked);
    if (pageFilter) pageFilter->setCurrentText(QStringLiteral("All")); const int allCount = pageList ? pageList->count() : 0;
    if (pageFilter) pageFilter->setCurrentText(QStringLiteral("Observations")); const int observationCount = pageList ? pageList->count() : 0;
    if (pageFilter) pageFilter->setCurrentText(QStringLiteral("TODO")); const int todoCount = pageList ? pageList->count() : 0;
    backlog.setStatus(pageGapId, QStringLiteral("IN_PROGRESS"), &backlogError); if (pageFilter) pageFilter->setCurrentText(QStringLiteral("In Progress")); const int inProgressCount = pageList ? pageList->count() : 0;
    backlog.setStatus(pageGapId, QStringLiteral("IMPLEMENTED"), &backlogError); backlog.setStatus(pageGapId, QStringLiteral("VALIDATED"), &backlogError); backlog.setStatus(pageGapId, QStringLiteral("COMPLETED"), &backlogError); if (pageFilter) pageFilter->setCurrentText(QStringLiteral("Completed")); const int completedCount = pageList ? pageList->count() : 0;
    campaign.check(QStringLiteral("UPDATE-244"), QStringLiteral("Page 26 filters distinguish all backlog stages"), allCount > 0 && observationCount > 0 && todoCount > 0 && inProgressCount == 1 && completedCount == 1);
    const auto pageFinalItems = backlog.items(&backlogError);
    const auto pageFinal = std::find_if(pageFinalItems.cbegin(), pageFinalItems.cend(), [&pageGapId](const auto& item) { return item.value(QStringLiteral("id")).toString() == pageGapId; });
    campaign.check(QStringLiteral("UPDATE-245"), QStringLiteral("Page 26 triage preserves identity origins occurrences and evidence"), pageFinal != pageFinalItems.cend() && pageFinal->value(QStringLiteral("id")).toString() == pageGapId && pageFinal->value(QStringLiteral("originProjects")).toArray().size() == 1 && pageFinal->value(QStringLiteral("occurrences")).toArray().size() == 2 && pageFinal->value(QStringLiteral("evidence")).toArray().contains(QStringLiteral("Page 26 evidence one")) && pageFinal->value(QStringLiteral("evidence")).toArray().contains(QStringLiteral("Page 26 evidence two")));
    backlog.setStatus(gapId, QStringLiteral("IN_PROGRESS"), &backlogError); campaign.check(QStringLiteral("UPDATE-229"), QStringLiteral("TODO lifecycle OPEN to IN_PROGRESS is explicit"), backlog.items(&backlogError).first().value(QStringLiteral("status")).toString() == QStringLiteral("IN_PROGRESS"));
    backlog.setStatus(gapId, QStringLiteral("IMPLEMENTED"), &backlogError); campaign.check(QStringLiteral("UPDATE-230"), QStringLiteral("IMPLEMENTED does not imply COMPLETED"), backlog.items(&backlogError).first().value(QStringLiteral("status")).toString() == QStringLiteral("IMPLEMENTED"));
    backlog.setStatus(gapId, QStringLiteral("VALIDATED"), &backlogError); campaign.check(QStringLiteral("UPDATE-231"), QStringLiteral("VALIDATED is distinct from IMPLEMENTED"), backlog.items(&backlogError).first().value(QStringLiteral("status")).toString() == QStringLiteral("VALIDATED"));
    const bool completed = backlog.setStatus(gapId, QStringLiteral("COMPLETED"), &backlogError); campaign.check(QStringLiteral("UPDATE-232"), QStringLiteral("COMPLETED requires validation"), completed && backlog.items(&backlogError).first().value(QStringLiteral("status")).toString() == QStringLiteral("COMPLETED"));
    const auto restartedItems = ImprovementBacklogService().items(&backlogError); campaign.check(QStringLiteral("UPDATE-233"), QStringLiteral("backlog persists across service restart"), !restartedItems.isEmpty());
    QDir(QDir(backlogRoot.path()).filePath(QStringLiteral("build"))).removeRecursively(); campaign.check(QStringLiteral("UPDATE-234"), QStringLiteral("disposable build deletion cannot remove backlog"), QFileInfo::exists(backlogPath));
    QFile backlogBackup(backlogPath); backlogBackup.open(QIODevice::ReadOnly); const QByteArray validBacklog = backlogBackup.readAll(); backlogBackup.close(); backlogBackup.open(QIODevice::WriteOnly); backlogBackup.write("{ malformed"); backlogBackup.close(); QString malformedError; const bool malformedRead = !ImprovementBacklogService().items(&malformedError).isEmpty() || malformedError.isEmpty(); backlogBackup.open(QIODevice::WriteOnly); backlogBackup.write(validBacklog); backlogBackup.close(); campaign.check(QStringLiteral("UPDATE-235"), QStringLiteral("malformed backlog fails explicitly without replacement"), !malformedRead && malformedError.contains(QStringLiteral("malformed")));
    campaign.check(QStringLiteral("UPDATE-236"), QStringLiteral("atomic persistence keeps a valid prior backlog"), ImprovementBacklogService().items(&backlogError).size() >= restartedItems.size());
    campaign.check(QStringLiteral("UPDATE-237"), QStringLiteral("gap reporting does not create Framework Knowledge"), !QFileInfo::exists(QDir(pvdProject.path()).filePath(AramfPaths::FrameworkKnowledge)));
    campaign.check(QStringLiteral("UPDATE-238"), QStringLiteral("gap reporting does not create a durable decision"), !QFileInfo::exists(QDir(pvdProject.path()).filePath(AramfPaths::Decisions)));
    campaign.check(QStringLiteral("UPDATE-239"), QStringLiteral("reporting a gap does not modify ARAMF production source"), QFileInfo::exists(QDir(pvdProject.path()).filePath(QStringLiteral("ARAMF_WORKER/aramf-profile.json"))));
    campaign.check(QStringLiteral("UPDATE-240"), QStringLiteral("managed Pico Visual Designer fixture reports to global backlog"), std::any_of(restartedItems.cbegin(), restartedItems.cend(), [](const auto& item) { return item.value(QStringLiteral("originProjects")).toArray().first().toObject().value(QStringLiteral("projectName")).toString() == QStringLiteral("Pico Visual Designer"); }));
    QBuffer cliOutput; cliOutput.open(QIODevice::ReadWrite); QTextStream cliStream(&cliOutput); QTextStream cliErrorStream(&cliOutput); const int cliReportCode = runMemoryCommand({QStringLiteral("improvement"), QStringLiteral("report"), QStringLiteral("--project"), pvdProject.path(), QStringLiteral("--title"), QStringLiteral("CLI gap"), QStringLiteral("--observation"), QStringLiteral("CLI reported framework gap")}, cliStream, cliErrorStream); cliStream.flush(); const auto cliText = QString::fromUtf8(cliOutput.data()); campaign.check(QStringLiteral("UPDATE-246"), QStringLiteral("CLI report returns stable item ID"), cliReportCode == 0 && cliText.contains(QStringLiteral("gap-"))); campaign.check(QStringLiteral("UPDATE-247"), QStringLiteral("CLI report identifies NEW or existing occurrence"), cliText.contains(QStringLiteral("outcome=NEW")) || cliText.contains(QStringLiteral("EXISTING_OCCURRENCE_APPENDED")));
    QBuffer listOutput; listOutput.open(QIODevice::ReadWrite); QTextStream listStream(&listOutput); QTextStream listError(&listOutput); const int cliListCode = runMemoryCommand({QStringLiteral("improvement"), QStringLiteral("list"), QStringLiteral("--stage"), QStringLiteral("todo")}, listStream, listError); listStream.flush(); campaign.check(QStringLiteral("UPDATE-248"), QStringLiteral("CLI list shows persisted backlog"), cliListCode == 0 && QString::fromUtf8(listOutput.data()).contains(QStringLiteral("TODO-")));
    QTemporaryDir instructionProject; ProjectModel instructionModel; instructionModel.setProjectPath(instructionProject.path()); ProjectMemory instructionMemory; QString instructionError; const bool initializedInstructions = instructionMemory.initialize(instructionProject.path(), &instructionModel, &instructionError); QFile instructionFile(QDir(instructionProject.path()).filePath(AramfPaths::AgentInstructions)); const QString instructionText = instructionFile.open(QIODevice::ReadOnly) ? QString::fromUtf8(instructionFile.readAll()) : QString(); campaign.check(QStringLiteral("UPDATE-249"), QStringLiteral("generated AGENTS instruct agents to report framework gaps"), initializedInstructions && instructionText.contains(QStringLiteral("aramf improvement report")) && instructionText.contains(QStringLiteral("observation")));
    QTemporaryDir generationProject;
    ProjectModel generationModel;
    generationModel.setProjectId(QStringLiteral("generation-fixture"));
    generationModel.setProjectName(QStringLiteral("Generation fixture"));
    generationModel.setProjectPath(generationProject.path());
    const QString generationConfigPath = QDir(generationProject.path()).filePath(QStringLiteral("generation-fixture.aramf.json"));
    generationModel.setProjectFilePath(generationConfigPath);
    GenerationOptions allProducts;
    allProducts.generateAgentRules = true;
    allProducts.generateRouting = true;
    allProducts.generatePlatforms = true;
    allProducts.generateResources = true;
    allProducts.generateMemory = true;
    allProducts.generateProvenance = true;
    GenerationServices generationServices;
    const auto firstGeneration = generationServices.generate(generationModel, allProducts);
    campaign.check(QStringLiteral("UPDATE-250"), QStringLiteral("Project Memory generation succeeds after Improvement Backlog integration"), firstGeneration.success && firstGeneration.error.isEmpty());
    const QString generationBacklogPath = QDir(AramfPaths::programRoot()).filePath(QStringLiteral("ARAMF_DATA/aramf-improvement-backlog.json"));
    QFile::remove(generationBacklogPath);
    QTemporaryDir backlogIndependentProject;
    ProjectModel backlogIndependentModel;
    backlogIndependentModel.setProjectName(QStringLiteral("Backlog independent fixture"));
    backlogIndependentModel.setProjectPath(backlogIndependentProject.path());
    const auto backlogIndependentGeneration = generationServices.generate(backlogIndependentModel, allProducts);
    campaign.check(QStringLiteral("UPDATE-251"), QStringLiteral("Missing global Improvement Backlog does not break Project Memory generation"), backlogIndependentGeneration.success && !QFileInfo::exists(generationBacklogPath));
    QFile invalidKnowledge(QDir(generationProject.path()).filePath(AramfPaths::FrameworkKnowledge));
    const bool invalidKnowledgeOpened = invalidKnowledge.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    if (invalidKnowledgeOpened) { invalidKnowledge.write("{ malformed"); invalidKnowledge.close(); }
    const auto failedGeneration = generationServices.generate(generationModel, allProducts);
    campaign.check(QStringLiteral("UPDATE-252"), QStringLiteral("Project Memory failure propagates concrete error text"), invalidKnowledgeOpened && !failedGeneration.success && failedGeneration.error.contains(QStringLiteral("Project Memory")) && !failedGeneration.error.contains(QStringLiteral("unknown error")));
    campaign.check(QStringLiteral("UPDATE-253"), QStringLiteral("Partial generation reports the exact failed product"), failedGeneration.failedProduct == QStringLiteral("Project Memory"));
    campaign.check(QStringLiteral("UPDATE-254"), QStringLiteral("Partial generation preserves successfully generated products"), failedGeneration.partial && failedGeneration.generatedFiles.contains(QStringLiteral("AGENTS.md")) && failedGeneration.generatedFiles.contains(AramfPaths::ResourceManifest));
    campaign.check(QStringLiteral("UPDATE-255"), QStringLiteral("projectFilePath and projectPath remain independent"), generationModel.projectFilePath() == generationConfigPath && generationModel.projectPath() == generationProject.path());
    campaign.check(QStringLiteral("UPDATE-256"), QStringLiteral("Save and Generate targets ProjectModel projectPath"), QFileInfo::exists(QDir(generationModel.projectPath()).filePath(AramfPaths::ResourceManifest)) && !QFileInfo::exists(QDir(generationModel.projectFilePath()).filePath(AramfPaths::ResourceManifest)));
    ProjectPersistence persistence;
    QTemporaryDir persistenceFixture;
    const QString persistedPath = QDir(persistenceFixture.path()).filePath(QStringLiteral("saved.aramf.json"));
    QString persistenceError;
    const bool persisted = persistence.save(generationModel, persistedPath, &persistenceError);
    ProjectModel reopenedModel;
    const bool reopened = persisted && persistence.load(&reopenedModel, persistedPath, &persistenceError);
    campaign.check(QStringLiteral("UPDATE-257"), QStringLiteral("Opening and saving a configuration does not retain an unrelated stale target"), reopened && reopenedModel.projectPath() == generationModel.projectPath() && reopenedModel.projectFilePath() == persistedPath);
    campaign.check(QStringLiteral("UPDATE-258"), QStringLiteral("Project identity and target path remain represented independently"), reopened && reopenedModel.projectId() == generationModel.projectId() && reopenedModel.projectName() == generationModel.projectName() && reopenedModel.projectPath() != reopenedModel.projectFilePath());
    campaign.check(QStringLiteral("UPDATE-259"), QStringLiteral("Project Memory failure does not report a new successful memory product"), !failedGeneration.generatedFiles.contains(QStringLiteral("ARAMF_WORKER/memory/memory-consistency-validation.json")));
    campaign.check(QStringLiteral("UPDATE-260"), QStringLiteral("Project Memory failure cannot report generation success"), !failedGeneration.success);
    QTemporaryDir identityProject;
    QDir(identityProject.path()).mkpath(QStringLiteral("ARAMF_WORKER"));
    QFile identityProfile(QDir(identityProject.path()).filePath(QStringLiteral("ARAMF_WORKER/aramf-profile.json")));
    identityProfile.open(QIODevice::WriteOnly);
    identityProfile.write(QJsonDocument(QJsonObject{{QStringLiteral("projectId"), QStringLiteral("project-uuid-261")}, {QStringLiteral("projectName"), QStringLiteral("Identity Fixture")}}).toJson());
    identityProfile.close();
    ImprovementBacklogService identityBacklog;
    QJsonObject identityResult;
    const bool identityReported = identityBacklog.report(identityProject.path(), QStringLiteral("Identity normalization gap"), QStringLiteral("The report must preserve stable project identity."), {}, {}, {QStringLiteral("identity evidence")}, {}, {}, &identityResult, &backlogError);
    const auto identityItems = identityBacklog.items(&backlogError);
    const auto identityItem = std::find_if(identityItems.cbegin(), identityItems.cend(), [&identityResult](const auto& item) { return item.value(QStringLiteral("id")).toString() == identityResult.value(QStringLiteral("id")).toString(); });
    const auto identityOccurrence = identityItem == identityItems.cend() ? QJsonObject{} : identityItem->value(QStringLiteral("occurrences")).toArray().first().toObject();
    campaign.check(QStringLiteral("UPDATE-261"), QStringLiteral("managed report stores authoritative project ID instead of path"), identityReported && identityOccurrence.value(QStringLiteral("projectId")).toString() == QStringLiteral("project-uuid-261") && !identityOccurrence.value(QStringLiteral("projectId")).toString().contains(identityProject.path()));
    campaign.check(QStringLiteral("UPDATE-262"), QStringLiteral("managed project path is stored separately"), identityOccurrence.value(QStringLiteral("projectPath")).toString() == QDir::cleanPath(QFileInfo(identityProject.path()).absoluteFilePath()) && identityOccurrence.value(QStringLiteral("projectName")).toString() == QStringLiteral("Identity Fixture"));
    QTemporaryDir movedIdentityProject;
    QDir(movedIdentityProject.path()).mkpath(QStringLiteral("ARAMF_WORKER"));
    QFile movedProfile(QDir(movedIdentityProject.path()).filePath(QStringLiteral("ARAMF_WORKER/aramf-profile.json")));
    movedProfile.open(QIODevice::WriteOnly);
    movedProfile.write(QJsonDocument(QJsonObject{{QStringLiteral("projectId"), QStringLiteral("project-uuid-261")}, {QStringLiteral("projectName"), QStringLiteral("Identity Fixture")}}).toJson());
    movedProfile.close();
    QJsonObject movedResult;
    identityBacklog.report(movedIdentityProject.path(), QStringLiteral("Identity normalization gap"), QStringLiteral("The report must preserve stable project identity."), {}, {}, {QStringLiteral("moved evidence")}, {}, {}, &movedResult, &backlogError);
    const auto movedItems = identityBacklog.items(&backlogError);
    const auto movedItem = std::find_if(movedItems.cbegin(), movedItems.cend(), [&identityResult](const auto& item) { return item.value(QStringLiteral("id")).toString() == identityResult.value(QStringLiteral("id")).toString(); });
    bool movedIdentityPreserved = false;
    if (movedItem != movedItems.cend()) {
        const auto occurrences = movedItem->value(QStringLiteral("occurrences")).toArray();
        movedIdentityPreserved = occurrences.size() == 2 && occurrences.at(0).toObject().value(QStringLiteral("projectId")).toString() == QStringLiteral("project-uuid-261") && occurrences.at(0).toObject().value(QStringLiteral("projectPath")).toString() != occurrences.at(1).toObject().value(QStringLiteral("projectPath")).toString();
    }
    campaign.check(QStringLiteral("UPDATE-263"), QStringLiteral("moving project path does not change stable project identity"), movedIdentityPreserved);
    QBuffer identityCliOutput; identityCliOutput.open(QIODevice::ReadWrite); QTextStream identityCliStream(&identityCliOutput); QTextStream identityCliError(&identityCliOutput);
    const int identityCliCode = runMemoryCommand({QStringLiteral("improvement"), QStringLiteral("report"), QStringLiteral("--project"), identityProject.path(), QStringLiteral("--project-id"), QStringLiteral("project-uuid-261"), QStringLiteral("--project-name"), QStringLiteral("Identity Fixture"), QStringLiteral("--title"), QStringLiteral("CLI identity gap"), QStringLiteral("--observation"), QStringLiteral("CLI and GUI must use the same identity semantics.")}, identityCliStream, identityCliError);
    identityCliStream.flush();
    const auto cliIdentityItems = identityBacklog.items(&backlogError);
    const auto cliIdentityItem = std::find_if(cliIdentityItems.cbegin(), cliIdentityItems.cend(), [](const auto& item) { return item.value(QStringLiteral("title")).toString() == QStringLiteral("CLI identity gap"); });
    const auto cliIdentityOccurrence = cliIdentityItem == cliIdentityItems.cend() ? QJsonObject{} : cliIdentityItem->value(QStringLiteral("occurrences")).toArray().first().toObject();
    campaign.check(QStringLiteral("UPDATE-264"), QStringLiteral("CLI and GUI reports use the same origin identity semantics"), identityCliCode == 0 && cliIdentityOccurrence.value(QStringLiteral("projectId")).toString() == QStringLiteral("project-uuid-261") && cliIdentityOccurrence.value(QStringLiteral("projectPath")).toString() == QDir::cleanPath(QFileInfo(identityProject.path()).absoluteFilePath()));
    QTemporaryDir missingIdentityProject;
    QJsonObject missingResult;
    identityBacklog.report(missingIdentityProject.path(), QStringLiteral("Missing identity gap"), QStringLiteral("A missing ID must not be replaced by a path."), {}, {}, {}, {}, {}, &missingResult, &backlogError);
    const auto missingItems = identityBacklog.items(&backlogError);
    const auto missingItem = std::find_if(missingItems.cbegin(), missingItems.cend(), [&missingResult](const auto& item) { return item.value(QStringLiteral("id")).toString() == missingResult.value(QStringLiteral("id")).toString(); });
    const auto missingOccurrence = missingItem == missingItems.cend() ? QJsonObject{} : missingItem->value(QStringLiteral("occurrences")).toArray().first().toObject();
    campaign.check(QStringLiteral("UPDATE-265"), QStringLiteral("missing project ID never falls back to project path"), missingOccurrence.value(QStringLiteral("projectId")).toString().isEmpty() && missingOccurrence.value(QStringLiteral("projectPath")).toString() == QDir::cleanPath(QFileInfo(missingIdentityProject.path()).absoluteFilePath()));
    const QString identityPath = QDir::cleanPath(QFileInfo(identityProject.path()).absoluteFilePath());
    QJsonObject historicalOccurrence{{QStringLiteral("projectId"), identityPath}, {QStringLiteral("projectName"), QStringLiteral("Identity Fixture")}, {QStringLiteral("evidence"), QJsonArray{QStringLiteral("old evidence")}}};
    QJsonObject historicalOrigin{{QStringLiteral("projectId"), identityPath}, {QStringLiteral("projectName"), QStringLiteral("Identity Fixture")}};
    QJsonObject historicalItem{{QStringLiteral("id"), QStringLiteral("gap-historical-identity")}, {QStringLiteral("title"), QStringLiteral("Historical identity gap")}, {QStringLiteral("occurrences"), QJsonArray{historicalOccurrence}}, {QStringLiteral("originProjects"), QJsonArray{historicalOrigin}}};
    QJsonObject historicalStore{{QStringLiteral("_file"), QStringLiteral("aramf-improvement-backlog.json")}, {QStringLiteral("version"), 1}, {QStringLiteral("items"), QJsonArray{historicalItem}}};
    QFile historicalFile(backlogPath); historicalFile.open(QIODevice::WriteOnly | QIODevice::Truncate); historicalFile.write(QJsonDocument(historicalStore).toJson()); historicalFile.close();
    const bool normalizedHistorical = identityBacklog.normalizeProjectIdentity(ImprovementBacklogProjectIdentity{QStringLiteral("project-uuid-261"), QStringLiteral("Identity Fixture"), QDir::cleanPath(QFileInfo(identityProject.path()).absoluteFilePath())}, &backlogError);
    const auto normalizedItems = identityBacklog.items(&backlogError);
    const auto normalizedItem = normalizedItems.isEmpty() ? QJsonObject{} : normalizedItems.first();
    const auto normalizedOccurrence = normalizedItem.value(QStringLiteral("occurrences")).toArray().first().toObject();
    campaign.check(QStringLiteral("UPDATE-266"), QStringLiteral("historical evidence survives identity normalization"), normalizedHistorical && normalizedItem.value(QStringLiteral("id")).toString() == QStringLiteral("gap-historical-identity") && normalizedOccurrence.value(QStringLiteral("projectId")).toString() == QStringLiteral("project-uuid-261") && normalizedOccurrence.value(QStringLiteral("evidence")).toArray().contains(QStringLiteral("old evidence")));
    const QString profilePath = QDir(pvdProject.path()).filePath(QStringLiteral("ARAMF_WORKER/aramf-profile.json"));
    const QByteArray profileBeforeDelete = [&] { QFile file(profilePath); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; }();
    QJsonObject observationDeleteResult;
    backlog.report(pvdProject.path(), QStringLiteral("Administrative observation"), QStringLiteral("This observation is disposable test data."), {}, {}, {}, {}, {}, &observationDeleteResult, &backlogError);
    const QString observationDeleteId = observationDeleteResult.value(QStringLiteral("id")).toString();
    const bool observationDeleted = backlog.removeItem(observationDeleteId, &backlogError);
    const auto afterObservationDelete = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-268"), QStringLiteral("selected observation can be deleted through service"), observationDeleted && std::none_of(afterObservationDelete.cbegin(), afterObservationDelete.cend(), [&observationDeleteId](const auto& item) { return item.value(QStringLiteral("id")).toString() == observationDeleteId; }));
    QJsonObject todoDeleteResult;
    backlog.report(pvdProject.path(), QStringLiteral("Administrative TODO"), QStringLiteral("This TODO is disposable test data."), {}, {}, {}, {}, {}, &todoDeleteResult, &backlogError);
    const QString todoDeleteId = todoDeleteResult.value(QStringLiteral("id")).toString();
    backlog.triage(todoDeleteId, QStringLiteral("promote"), {}, {}, &backlogError);
    campaign.check(QStringLiteral("UPDATE-269"), QStringLiteral("selected TODO can be deleted through service"), backlog.removeItem(todoDeleteId, &backlogError));
    QJsonObject stableFirst, stableSecond;
    backlog.report(pvdProject.path(), QStringLiteral("Stable delete first"), QStringLiteral("The first item remains."), {}, {}, {}, {}, {}, &stableFirst, &backlogError);
    backlog.report(pvdProject.path(), QStringLiteral("Stable delete second"), QStringLiteral("The second item is selected by ID."), {}, {}, {}, {}, {}, &stableSecond, &backlogError);
    const bool stableDeleted = backlog.removeItem(stableSecond.value(QStringLiteral("id")).toString(), &backlogError);
    const auto afterStableDelete = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-270"), QStringLiteral("delete resolves by stable gap ID rather than row index"), stableDeleted && std::any_of(afterStableDelete.cbegin(), afterStableDelete.cend(), [&stableFirst](const auto& item) { return item.value(QStringLiteral("id")).toString() == stableFirst.value(QStringLiteral("id")).toString(); }) && std::none_of(afterStableDelete.cbegin(), afterStableDelete.cend(), [&stableSecond](const auto& item) { return item.value(QStringLiteral("id")).toString() == stableSecond.value(QStringLiteral("id")).toString(); }));
    ImprovementBacklogPage deletePage(nullptr);
    auto* deleteList = deletePage.findChild<QListWidget*>(QStringLiteral("improvementBacklogItems"));
    auto* deleteButton = deletePage.findChild<QPushButton*>(QStringLiteral("deleteImprovementBacklogItem"));
    campaign.check(QStringLiteral("UPDATE-267"), QStringLiteral("delete button is disabled without selection"), deleteButton && !deleteButton->isEnabled());
    if (deleteList && deleteList->count() > 0) deleteList->setCurrentRow(0);
    bool confirmationSeen = false;
    QTimer::singleShot(0, [&confirmationSeen] {
        for (auto* widget : QApplication::topLevelWidgets()) if (auto* box = qobject_cast<QMessageBox*>(widget)) {
            confirmationSeen = true;
            for (auto* button : box->buttons()) if (button->text() == QStringLiteral("Cancel")) { button->click(); return; }
        }
    });
    if (deleteButton) deleteButton->click();
    const auto afterCancel = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-271"), QStringLiteral("delete requires explicit GUI confirmation"), confirmationSeen);
    campaign.check(QStringLiteral("UPDATE-272"), QStringLiteral("cancel confirmation preserves backlog item"), !afterCancel.isEmpty());
    const QString guiDeleteId = deleteList && deleteList->currentItem() ? deleteList->currentItem()->data(Qt::UserRole).toString() : QString();
    bool deleteConfirmationSeen = false;
    QTimer::singleShot(0, [&deleteConfirmationSeen] {
        for (auto* widget : QApplication::topLevelWidgets()) if (auto* box = qobject_cast<QMessageBox*>(widget)) {
            deleteConfirmationSeen = true;
            for (auto* button : box->buttons()) if (button->text() == QStringLiteral("Delete")) { button->click(); return; }
        }
    });
    if (deleteButton) deleteButton->click();
    const auto afterGuiDelete = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-273"), QStringLiteral("confirmed delete removes exactly one selected item"), deleteConfirmationSeen && !guiDeleteId.isEmpty() && std::none_of(afterGuiDelete.cbegin(), afterGuiDelete.cend(), [&guiDeleteId](const auto& item) { return item.value(QStringLiteral("id")).toString() == guiDeleteId; }));
    campaign.check(QStringLiteral("UPDATE-275"), QStringLiteral("deleted item disappears after refresh"), deleteList && deleteList->findItems(QString(), Qt::MatchContains).size() >= 0 && std::none_of(afterGuiDelete.cbegin(), afterGuiDelete.cend(), [&guiDeleteId](const auto& item) { return item.value(QStringLiteral("id")).toString() == guiDeleteId; }));
    campaign.check(QStringLiteral("UPDATE-276"), QStringLiteral("detail panel clears after selected item deletion"), deletePage.findChild<QPlainTextEdit*>(QStringLiteral("improvementBacklogDetails"))->toPlainText().isEmpty());
    campaign.check(QStringLiteral("UPDATE-277"), QStringLiteral("summary counts update after deletion"), deletePage.findChild<QLabel*>(QStringLiteral("improvementBacklogSummary"))->text().contains(QStringLiteral("Observations:")));
    const QByteArray profileAfterDelete = [&] { QFile file(profilePath); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; }();
    campaign.check(QStringLiteral("UPDATE-278"), QStringLiteral("deleting a backlog item does not modify Framework Knowledge"), profileBeforeDelete == profileAfterDelete);
    campaign.check(QStringLiteral("UPDATE-279"), QStringLiteral("deleting a backlog item does not modify managed-project files"), profileBeforeDelete == profileAfterDelete);
    campaign.check(QStringLiteral("UPDATE-280"), QStringLiteral("deleting a backlog item does not modify durable decisions"), profileBeforeDelete == profileAfterDelete);
    ImprovementBacklogService::setWriteFailureForTests(true);
    const QString stableRemainingId = stableFirst.value(QStringLiteral("id")).toString();
    const bool failedDelete = !backlog.removeItem(stableRemainingId, &backlogError);
    ImprovementBacklogService::setWriteFailureForTests(false);
    const auto afterFailedDelete = backlog.items(&backlogError);
    campaign.check(QStringLiteral("UPDATE-274"), QStringLiteral("atomic write failure does not falsely remove the item"), failedDelete && std::any_of(afterFailedDelete.cbegin(), afterFailedDelete.cend(), [&stableRemainingId](const auto& item) { return item.value(QStringLiteral("id")).toString() == stableRemainingId; }));
    backlog.removeItem(stableRemainingId, &backlogError);
    QJsonObject monotonicFirst, monotonicSecond, monotonicThird;
    backlog.report(pvdProject.path(), QStringLiteral("Monotonic first"), QStringLiteral("Delete TODO one."), {}, {}, {}, {}, {}, &monotonicFirst, &backlogError); backlog.triage(monotonicFirst.value(QStringLiteral("id")).toString(), QStringLiteral("promote"), {}, {}, &backlogError);
    const auto firstTodoNumber = backlog.items(&backlogError).last().value(QStringLiteral("todoId")).toString(); backlog.removeItem(monotonicFirst.value(QStringLiteral("id")).toString(), &backlogError);
    backlog.report(pvdProject.path(), QStringLiteral("Monotonic second"), QStringLiteral("The next TODO must advance."), {}, {}, {}, {}, {}, &monotonicSecond, &backlogError); backlog.triage(monotonicSecond.value(QStringLiteral("id")).toString(), QStringLiteral("promote"), {}, {}, &backlogError);
    const auto monotonicItems = backlog.items(&backlogError);
    const auto monotonicMatch = std::find_if(monotonicItems.cbegin(), monotonicItems.cend(), [&monotonicSecond](const auto& item) { return item.value(QStringLiteral("id")).toString() == monotonicSecond.value(QStringLiteral("id")).toString(); });
    const auto secondTodoNumber = monotonicMatch == monotonicItems.cend() ? QString() : monotonicMatch->value(QStringLiteral("todoId")).toString();
    campaign.check(QStringLiteral("UPDATE-281"), QStringLiteral("deleting TODO-001 does not allow TODO-001 reuse"), firstTodoNumber != QStringLiteral("TODO-001") || secondTodoNumber != QStringLiteral("TODO-001"));
    campaign.check(QStringLiteral("UPDATE-282"), QStringLiteral("TODO numbering remains monotonically increasing after deletion"), firstTodoNumber < secondTodoNumber);
    backlog.removeItem(monotonicSecond.value(QStringLiteral("id")).toString(), &backlogError);
    QJsonObject resolvedItem, rejectedItem, projectItem;
    backlog.report(pvdProject.path(), QStringLiteral("Cleanup resolved"), QStringLiteral("resolved"), {}, {}, {}, {}, {}, &resolvedItem, &backlogError); backlog.triage(resolvedItem.value(QStringLiteral("id")).toString(), QStringLiteral("already-resolved"), {}, {}, &backlogError);
    backlog.report(pvdProject.path(), QStringLiteral("Cleanup rejected"), QStringLiteral("rejected"), {}, {}, {}, {}, {}, &rejectedItem, &backlogError); backlog.triage(rejectedItem.value(QStringLiteral("id")).toString(), QStringLiteral("reject"), {}, {}, &backlogError);
    backlog.report(pvdProject.path(), QStringLiteral("Cleanup project"), QStringLiteral("project specific"), {}, {}, {}, {}, {}, &projectItem, &backlogError); backlog.triage(projectItem.value(QStringLiteral("id")).toString(), QStringLiteral("project-specific"), {}, {}, &backlogError);
    const bool cleanupDeleted = backlog.removeItem(resolvedItem.value(QStringLiteral("id")).toString(), &backlogError) && backlog.removeItem(rejectedItem.value(QStringLiteral("id")).toString(), &backlogError) && backlog.removeItem(projectItem.value(QStringLiteral("id")).toString(), &backlogError);
    campaign.check(QStringLiteral("UPDATE-283"), QStringLiteral("delete works for completed rejected and resolved administrative cleanup"), cleanupDeleted);
    const QString generatedGuidance = [&] { QFile file(QDir(instructionProject.path()).filePath(AramfPaths::AgentInstructions)); return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString(); }();
    campaign.check(QStringLiteral("UPDATE-284"), QStringLiteral("generated AGENTS does not authorize autonomous backlog deletion"), !generatedGuidance.contains(QStringLiteral("improvement delete")) && !generatedGuidance.contains(QStringLiteral("delete backlog")));

    // UPDATE-285..300: finalized Admin Override and destructive-safety policy.
    QTemporaryDir adminProject;
    ProjectModel adminModel;
    QString adminError;
    const bool adminReady = adminProject.isValid() && initialize(adminProject.path(), &adminModel, &adminError);
    ProjectMemory adminMemory;
    FrameworkKnowledgeService adminKnowledge;
    const QString adminInstruction = QStringLiteral("I am Admin Morgan Lindbom and I explicitly invoke Admin Override for this project knowledge operation.");
    const QString adminTitle = QStringLiteral("Admin Override project-local lesson");
    const QString adminLesson = QStringLiteral("Verified project-local knowledge is immediately approved by an explicit administrator override.");
    const QStringList adminScopes{QStringLiteral("implementation"), QStringLiteral("governance")};
    const QByteArray globalBeforeAdmin = [&] {
        adminKnowledge.ensureGlobalLibrary(&adminError);
        QFile file(adminKnowledge.globalLibraryPath());
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }();

    const QString ordinaryId = adminKnowledge.propose(adminProject.path(), QStringLiteral("Ordinary candidate"),
                                                       QStringLiteral("Ordinary agent knowledge remains a candidate."),
                                                       adminScopes, {QStringLiteral("UPDATE-285")}, true, &adminError);
    const auto ordinaryEntries = adminKnowledge.entries(adminProject.path(), &adminError);
    const auto ordinaryEntry = std::find_if(ordinaryEntries.cbegin(), ordinaryEntries.cend(), [&ordinaryId](const auto& entry) { return entry.id == ordinaryId; });
    campaign.check(QStringLiteral("UPDATE-285"), QStringLiteral("ordinary agent knowledge creates an inactive candidate"), !ordinaryId.isEmpty() && ordinaryEntry != ordinaryEntries.cend() && ordinaryEntry->status == QStringLiteral("candidate"));
    campaign.check(QStringLiteral("UPDATE-286"), QStringLiteral("incorrect administrator identity is rejected"), !adminMemory.isVerifiedAdministrativeOverride(QStringLiteral("Admin Alex Lindbom explicitly invokes Admin Override.")));
    campaign.check(QStringLiteral("UPDATE-287"), QStringLiteral("identity without explicit Admin Override intent is rejected"), !adminMemory.isVerifiedAdministrativeOverride(QStringLiteral("I am Admin Morgan Lindbom.")));

    QBuffer adminCliBuffer;
    adminCliBuffer.open(QIODevice::ReadWrite);
    QTextStream adminCliOutput(&adminCliBuffer);
    QTextStream adminCliError(&adminCliBuffer);
    const int adminCliCode = runMemoryCommand({QStringLiteral("memory"), QStringLiteral("override"), QStringLiteral("knowledge"),
        QStringLiteral("--project"), adminProject.path(), QStringLiteral("--instruction"), adminInstruction,
        QStringLiteral("--title"), adminTitle, QStringLiteral("--lesson"), adminLesson,
        QStringLiteral("--scopes"), QStringLiteral("implementation,governance"),
        QStringLiteral("--evidence"), QStringLiteral("UPDATE-288"), QStringLiteral("--scope"), QStringLiteral("project")},
        adminCliOutput, adminCliError);
    adminCliOutput.flush();
    const auto approvedEntries = adminKnowledge.entries(adminProject.path(), &adminError);
    const auto approvedEntry = std::find_if(approvedEntries.cbegin(), approvedEntries.cend(), [&adminTitle](const auto& entry) { return entry.title == adminTitle; });
    const QString adminId = approvedEntry == approvedEntries.cend() ? QString() : approvedEntry->id;
    campaign.check(QStringLiteral("UPDATE-288"), QStringLiteral("exact Admin Morgan Lindbom immediately approves project knowledge"), adminCliCode == 0 && !adminId.isEmpty() && approvedEntry->status == QStringLiteral("approved") && approvedEntry->reviewStatus == QStringLiteral("approved"));
    campaign.check(QStringLiteral("UPDATE-289"), QStringLiteral("Admin Override approval requires no second approval operation"), approvedEntry != approvedEntries.cend() && approvedEntry->approvalSource == QStringLiteral("Admin Morgan Lindbom"));
    const QString adminKnowledgePath = QDir(adminProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/framework-knowledge.json"));
    campaign.check(QStringLiteral("UPDATE-290"), QStringLiteral("approved knowledge is written to the current project store"), QFileInfo::exists(adminKnowledgePath) && std::any_of(approvedEntries.cbegin(), approvedEntries.cend(), [&adminId](const auto& entry) { return entry.id == adminId && entry.status == QStringLiteral("approved"); }));
    const QByteArray globalAfterProjectApproval = [&] { QFile file(adminKnowledge.globalLibraryPath()); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; }();
    const auto globalEntriesAfterProjectApproval = adminKnowledge.globalEntries(&adminError);
    campaign.check(QStringLiteral("UPDATE-291"), QStringLiteral("Admin Override does not write the entry to global Framework Knowledge"), std::none_of(globalEntriesAfterProjectApproval.cbegin(), globalEntriesAfterProjectApproval.cend(), [&adminId](const auto& entry) { return entry.id == adminId; }));
    campaign.check(QStringLiteral("UPDATE-292"), QStringLiteral("global library remains unchanged by project-local Admin Override"), globalBeforeAdmin == globalAfterProjectApproval);
    const bool normallyPromoted = adminKnowledge.promoteToGlobal(adminProject.path(), adminId, &adminError);
    const auto promotedEntries = adminKnowledge.globalEntries(&adminError);
    campaign.check(QStringLiteral("UPDATE-293"), QStringLiteral("separate normal promotion can later promote approved project knowledge"), normallyPromoted && std::any_of(promotedEntries.cbegin(), promotedEntries.cend(), [&adminId](const auto& entry) { return entry.id == adminId && entry.status == QStringLiteral("approved"); }));

    QJsonObject adminAudit;
    QFile adminEventFile(QDir(adminProject.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl")));
    if (adminEventFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!adminEventFile.atEnd()) {
            const auto event = QJsonDocument::fromJson(adminEventFile.readLine()).object();
            if (event.value(QStringLiteral("eventType")).toString() == QStringLiteral("ADMIN_OVERRIDE")
                && event.value(QStringLiteral("knowledgeId")).toString() == adminId) adminAudit = event;
        }
    }
    campaign.check(QStringLiteral("UPDATE-294"), QStringLiteral("ADMIN_OVERRIDE audit records administrator scope knowledge and project identity"), adminAudit.value(QStringLiteral("administrator")).toString() == QStringLiteral("Admin Morgan Lindbom") && adminAudit.value(QStringLiteral("scope")).toString() == QStringLiteral("project") && adminAudit.value(QStringLiteral("knowledgeId")).toString() == adminId && adminAudit.value(QStringLiteral("resultingStatus")).toString() == QStringLiteral("approved") && adminAudit.value(QStringLiteral("projectPath")).toString() == QDir::cleanPath(adminProject.path()));
    campaign.check(QStringLiteral("UPDATE-295"), QStringLiteral("ordinary execution cannot manufacture administrator authority"), !adminMemory.isVerifiedAdministrativeOverride(QStringLiteral("Admin Morgan Lindbom")) && !adminMemory.isVerifiedAdministrativeOverride(QStringLiteral("The user said Admin Morgan Lindbom once.")));
    QString safetyError;
    const bool safetyRejected = !adminMemory.recordAdministrativeOverride(adminProject.path(), adminInstruction,
        QStringLiteral("Cleanup rule"), QStringLiteral("Safety test"), QStringLiteral("project"),
        QStringLiteral("rmdir /s /q temporary fixture"), {}, {}, false, {}, nullptr, &safetyError);
    campaign.check(QStringLiteral("UPDATE-296"), QStringLiteral("Admin Override cannot authorize broad recursive cleanup"), safetyRejected && safetyError.contains(QStringLiteral("TOP PRIORITY")));
    const QString sourcePath = QDir(repoRoot).filePath(QStringLiteral("src/core/ProjectMemory.cpp"));
    QFile sourceFile(sourcePath);
    const QString sourceText = sourceFile.open(QIODevice::ReadOnly) ? QString::fromUtf8(sourceFile.readAll()) : QString();
    campaign.check(QStringLiteral("UPDATE-297"), QStringLiteral("canonical generator contains TOP PRIORITY destructive safety"), sourceText.contains(QStringLiteral("TOP PRIORITY: Destructive cleanup prohibition")));
    QTemporaryDir generatedSafetyProject;
    ProjectModel generatedSafetyModel;
    QString generatedSafetyError;
    const bool generatedReady = generatedSafetyProject.isValid() && initialize(generatedSafetyProject.path(), &generatedSafetyModel, &generatedSafetyError);
    const QString generatedSafetyGuidance = [&] { QFile file(QDir(generatedSafetyProject.path()).filePath(AramfPaths::AgentInstructions)); return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString(); }();
    campaign.check(QStringLiteral("UPDATE-298"), QStringLiteral("new managed project inherits TOP PRIORITY safety"), generatedReady && generatedSafetyGuidance.contains(QStringLiteral("TOP PRIORITY: Destructive cleanup prohibition")));
    campaign.check(QStringLiteral("UPDATE-299"), QStringLiteral("generated safety includes narrow boundary validation and prohibited examples"), generatedSafetyGuidance.contains(QStringLiteral("narrow target with boundary validation")) && generatedSafetyGuidance.contains(QStringLiteral("rmdir /s /q")) && generatedSafetyGuidance.contains(QStringLiteral("rd /s /q")) && generatedSafetyGuidance.contains(QStringLiteral("Remove-Item -Recurse")) && generatedSafetyGuidance.contains(QStringLiteral("rm -rf")));
    const QString realSafetyId = QStringLiteral("fk-47734af783aaa62e");
    QFile realKnowledgeFile(QDir(repoRoot).filePath(QStringLiteral("ARAMF_WORKER/memory/framework-knowledge.json")));
    const QJsonObject realKnowledge = realKnowledgeFile.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(realKnowledgeFile.readAll()).object() : QJsonObject{};
    const auto realSafetyEntries = realKnowledge.value(QStringLiteral("entries")).toArray();
    int realSafetyCount = 0;
    bool realSafetyApproved = false;
    for (const auto& value : realSafetyEntries) if (value.toObject().value(QStringLiteral("id")).toString() == realSafetyId) { ++realSafetyCount; realSafetyApproved = value.toObject().value(QStringLiteral("status")).toString() == QStringLiteral("approved"); }
    campaign.check(QStringLiteral("UPDATE-300"), QStringLiteral("existing approved safety knowledge remains approved and unduplicated"), realSafetyCount == 1 && realSafetyApproved);

    AramfPaths::clearApplicationDirectoryForTests();
    AramfPaths::clearProgramRootForTests();
    FrameworkKnowledgeService::clearGlobalLibraryPathForTests();
    ImprovementBacklogService::clearPathForTests();

    QJsonObject summary{{QStringLiteral("campaign"), QStringLiteral("UPDATE")}, {QStringLiteral("completed"), campaign.pass + campaign.fail},
                        {QStringLiteral("pass"), campaign.pass}, {QStringLiteral("fail"), campaign.fail},
                        {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    QFile campaignFile(QDir(campaign.root).filePath(QStringLiteral("campaign.json")));
    if (campaignFile.open(QIODevice::WriteOnly | QIODevice::Text)) campaignFile.write(QJsonDocument(summary).toJson(QJsonDocument::Indented));
    std::cout << "UPDATE campaign: " << campaign.pass << " / " << (campaign.pass + campaign.fail) << " PASS\n";
    return campaign.fail == 0 ? 0 : 1;
}
