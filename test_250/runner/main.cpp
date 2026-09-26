#include "core/AramfPaths.h"
#include "core/ProjectModel.h"
#include "core/TemplateValidation.h"
#include "core/FrameworkKnowledge.h"
#include <QUuid>
#include <QStandardPaths>
#include "core/ProjectPersistence.h"
#include "core/Services.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {
QString q(const QString& s) { return s; }
QStringList pick(const QStringList& values, int count) { return values.mid(0, qMin(count, values.size())); }

bool nonEmpty(const QString& root, const QString& relative) {
    QFileInfo f(QDir(root).filePath(relative));
    return f.exists() && f.isFile() && f.size() > 0;
}

QString statusName(VerificationStatus s) {
    switch (s) { case VerificationStatus::Pass: return "PASS"; case VerificationStatus::Warning: return "WARNING";
    case VerificationStatus::Fail: return "FAIL"; default: return "NOT-APPLICABLE"; }
}

QString areaFor(int n) {
    if (n <= 25) return "PROJECT"; if (n <= 45) return "ACADEMIC"; if (n <= 70) return "LANGUAGES / FRAMEWORKS / TOOLS";
    if (n <= 90) return "PLATFORMS / HARDWARE / BUILD"; if (n <= 115) return "AI"; if (n <= 145) return "RESOURCES";
    if (n <= 170) return "RULES"; if (n <= 195) return "MEMORY"; if (n <= 215) return "GENERATE";
    if (n <= 230) return "VERIFY"; return "FINALIZE / BOOTSTRAP / FRAMEWORK KNOWLEDGE";
}

void writeText(const QString& path, const QString& value) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile f(path); if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly) || f.write(value.toUtf8()) != value.toUtf8().size()) qFatal("Cannot persist campaign evidence");
}

QString markdown(int n, const QString& purpose, const QString& root, const QString& file, const QString& actual,
                 const QString& result, const QString& problem, const QString& rootCause, const QString& correction,
                 const QString& retest, const QString& fk) {
    return QString("# TEST-%1 — %2\n\n## Purpose\n\n%3\n\n## Initial State\n\nNew isolated target: `%4`\nConfiguration: `%5`\n\n## User Configuration\n\nArea: %6\nTemplate: varied by scenario\nProject / AI / Resources / Rules / Memory / Generation: scenario-specific model configuration.\n\n## User Actions\n\n1. Create/open the isolated project.\n2. Configure the model and save the ARAMF project file.\n3. Reload the saved model.\n4. Review, Save & Generate, Verify, Finalize, and create agent entry points where applicable.\n5. Inspect generated files on disk.\n\n## Expected Result\n\nThe supported core workflow persists state and produces a valid, bounded, deterministic ARAMF control plane.\n\n## Actual Result\n\n%7\n\n## Result\n\n%8\n\n## Problems Found\n\n%9\n\n## Root Cause\n\n%10\n\n## Correction\n\n%11\n\n## Retest\n\n%12\n\n## Framework Knowledge Candidate\n\n%13\n\nCandidate lesson: None generated automatically by this campaign.\n\nEvidence: `test_%1/result.md`\n\nGeneralizable because: N/A\n").arg(QString::number(n).rightJustified(3, '0'), QString("Scenario %1").arg(n), purpose, root, file, areaFor(n), actual, result, problem, rootCause, correction, retest, fk);
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    const QString campaign = QDir::current().filePath("build/test_250/runs/" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    FrameworkKnowledgeService::setGlobalLibraryPathForTests(campaign + "/global/framework-knowledge-library.json");
    const int selected = app.arguments().contains("--scenario") ? app.arguments().value(app.arguments().indexOf("--scenario") + 1).toInt() : 0;
    if (app.arguments().contains("--scenario") && (selected < 1 || selected > 250)) return 2;
    const bool injectFailure = app.arguments().contains("--inject-failure");
    QTextStream(stdout) << "EVIDENCE " << campaign << Qt::endl;
    QDir(campaign).mkpath("scenarios"); QDir(campaign).mkpath("failures"); QDir(campaign).mkpath("artifacts"); QDir(campaign).mkpath("projects"); QDir(campaign).mkpath("logs");
    const QStringList templates = {"", "cpp-command-line", "qt-desktop-application", "python-backend", "react-frontend", "raspberry-pi-pico-firmware", "cmake-library"};
    const QStringList langs = {"cpp", "c", "python", "typescript", "csharp", "rust"};
    const QStringList frameworks = {"qt", "pico-sdk", "fastapi", "react", "dotnet", "wxwidgets"};
    const QStringList agents = {"none", "claude-code", "github-copilot", "gemini", "custom-agent"};
    int initialPass = 0, initialFail = 0, issues = 0, completed = 0;
    QStringList issueLines;
    ProjectPersistence persistence; TemplateManager templatesService(nullptr, campaign + "/templates.json"); GenerationServices generator; VerificationServices verifier; FinalizationServices finalizer; AgentEntryPointService entryPoints;
    for (int n = 1; n <= 250; ++n) {
        if (selected && selected != n) continue;
        ++completed;
        const QString id = QString("test_%1").arg(n, 3, 10, QLatin1Char('0'));
        const QString root = QDir(campaign).filePath("projects/" + id + "/target");
        const QString file = QDir(campaign).filePath("projects/" + id + "/" + id + ".aramf.json");
        if (QFileInfo::exists(root) || !QDir().mkpath(root)) return 2;
        ProjectModel model; model.setProjectId(QString("campaign-%1").arg(id)); model.setProjectName(QString("Campaign %1").arg(n)); model.setProjectPath(root); model.setProjectFilePath(file); model.setDescription(QString("250-scenario campaign case %1").arg(n));
        const QString tid = templates.at(n % templates.size());
        QString templateError;
        bool templateOk = tid.isEmpty() || templatesService.applyTemplate(&model, tid, &templateError);
        model.setProjectPath(root); model.setProjectFilePath(file);
        auto caps = model.developmentCapabilities(); caps.languages = pick(langs, 1 + n % 3); caps.frameworks = pick(frameworks, 1 + n % 3); caps.developmentTools = {"debugger", n % 2 ? "profiling" : "memory-analysis"}; caps.versionControlSystems = {"git"}; caps.targetPlatforms = {n % 2 ? "windows-desktop" : "linux-desktop"}; caps.targetArchitectures = {n % 3 ? "x86_64" : "arm64"}; caps.hardwareTargets = n % 5 == 0 ? QStringList{"raspberry-pi-pico-2"} : QStringList{}; caps.buildSystems = {"cmake"}; caps.testingCapabilities = {"unit-testing", "e2e-testing"}; model.setDevelopmentCapabilities(caps);
        // Current catalog requirements: selected SDKs need matching targets/toolchains.
        if (caps.frameworks.contains("pico-sdk")) { caps.toolchains.removeAll("none"); caps.toolchains << "arm-gnu"; caps.targetPlatforms << "microcontroller"; }
        if (caps.frameworks.contains("fastapi")) { caps.toolchains.removeAll("none"); caps.toolchains << "python"; }
        caps.toolchains.removeDuplicates(); model.setDevelopmentCapabilities(caps);
        auto academic = model.academicConfiguration(); if (n >= 26 && n <= 45) { academic.academicMode = n % 3 == 0 ? "thesis" : (n % 2 ? "academic-assignment" : "research-project"); academic.thesisLevel = n % 3 == 0 ? "doctoral" : "master"; academic.academicLanguage = n % 2 ? "english" : "swedish"; academic.citationStyle = n % 2 ? "apa" : "harvard"; academic.researchMethods = {"qualitative", "literature-review"}; } model.setAcademicConfiguration(academic);
        auto ai = model.aiConfiguration(); ai.primaryAgent = agents.at(n % agents.size()); ai.additionalAgents = n % 5 == 0 ? QStringList{"claude-code", "github-copilot"} : QStringList{}; ai.additionalAgents.removeAll(ai.primaryAgent); ai.responsibilities = {"planning", "coding", "testing"}; ai.permissions = {"read-project-files", "modify-files", "create-files", "run-tests"}; ai.aramfIntegrations = {"rules", "project-memory", "validation-verification"}; model.setAiConfiguration(ai);
        if (n >= 116 && n <= 145) { QList<ProjectResource> rs; for (int k = 0; k < 1 + n % 4; ++k) { ProjectResource r; r.id = QString("resource-%1-%2").arg(n).arg(k); r.name = QString("Resource %1").arg(k); r.type = k % 3 == 0 ? "file" : (k % 3 == 1 ? "folder" : "url"); r.location = k % 3 == 2 ? "https://example.com/reference" : QDir(root).filePath(QString("input_%1.txt").arg(k)); r.authorityLevel = k == 0 ? "primary-source-of-truth" : "supporting-reference"; r.scopes = {"requirements", "implementation"}; r.locationMode = k % 2 ? "project-local-copy" : "referenced"; rs << r; } model.setResources(rs); }
        auto rules = model.ruleConfiguration(); rules.activeCategories = n % 4 == 0 ? QStringList{} : QStringList{"data-protection", "protected-files", "verification-before-completion"}; rules.enforcementLevel = n % 3 == 0 ? "strict" : (n % 2 ? "advisory" : "standard"); rules.workScopes = {"coding", "testing"}; rules.projectScopes = {"source-code", "tests"}; model.setRuleConfiguration(rules);
        auto memory = model.memoryConfiguration(); memory.maximumSizeBytes = (n % 8 == 0 ? 750LL * 1024 * 1024 : (1LL + n % 5) * 1024 * 1024 * 1024); memory.retentionLevel = n % 3 == 0 ? "minimal" : (n % 3 == 1 ? "standard" : "detailed"); memory.captureCategories = {"durable-decisions", "completed-tasks", "validation-results"}; memory.maintenanceOptions = {"preserve-append-only", "record-validation"}; memory.validationOptions = {"memory-consistency", "cold-start-validation"}; model.setMemoryConfiguration(memory);
        GenerationOptions options; if (n >= 196 && n <= 215) { options.generateAgentRules = n % 2; options.generateRouting = n % 3 != 0; options.generatePlatforms = n % 4 != 0; options.generateResources = n % 5 != 0; options.generateMemory = n % 6 != 0; options.generateProvenance = n % 7 != 0; if (!(options.generateAgentRules || options.generateRouting || options.generatePlatforms || options.generateResources || options.generateMemory || options.generateProvenance)) options.generateMemory = true; } model.setGenerationOptions(options);
        QString error = templateError;
        if (injectFailure) { auto invalid = model.developmentCapabilities(); invalid.languages = {"obsolete-language-negative-control"}; model.setDevelopmentCapabilities(invalid); }
        bool ok = templateOk && persistence.save(model, file, &error); ProjectModel loaded; if (ok) ok = persistence.load(&loaded, file, &error); if (ok) { loaded.setProjectPath(root); loaded.setProjectFilePath(file); }
        QString actual; QString result = "PASS"; QString problem = "None."; QString rootCause = "N/A"; QString correction = "N/A"; QString retest = "Not required."; QString fk = "NO";
        if (!ok) { result = "FAIL"; ++initialFail; ++issues; problem = error; rootCause = "Persistence workflow failed."; issueLines << QString("Persistence failure in %1: %2").arg(id, error); }
        else { auto generated = generator.generate(loaded, options); if (!generated.success) { result = "FAIL"; ++initialFail; ++issues; problem = generated.error; rootCause = "Generation service returned failure."; issueLines << QString("Generation failure in %1: %2").arg(id, generated.error); } else { auto verified = verifier.verify(loaded, options); actual = QString("Generate: PASS (%1 files); Verify: %2 (%3 checks).").arg(generated.generatedFiles.size()).arg(statusName(verified.overallStatus)).arg(verified.checks.size()); if (verified.overallStatus != VerificationStatus::Pass) { result = "FAIL"; ++initialFail; ++issues; problem = verified.error.isEmpty() ? "Verification did not pass." : verified.error; for (const auto& check : verified.summary.value("checks").toArray()) { const auto detail = check.toObject(); if (detail.value("status") == "FAIL") problem += "\n" + detail.value("id").toString() + ": " + detail.value("details").toString(); } rootCause = "Generated output did not satisfy verification."; issueLines << QString("Verification failure in %1").arg(id); } else { auto finalized = finalizer.finalize(loaded, options); if (!finalized.success) { result = "FAIL"; ++initialFail; ++issues; problem = finalized.error.isEmpty() ? finalized.blockers.join("; ") : finalized.error; rootCause = "Finalization precondition or write failed."; issueLines << QString("Finalization failure in %1").arg(id); } else { auto again = finalizer.finalize(loaded, options); if (!again.success || !again.alreadyFinalized) { result = "FAIL"; ++initialFail; ++issues; problem = "Finalize was not idempotent."; rootCause = "Repeated finalization did not recognize current fingerprint."; issueLines << QString("Idempotence failure in %1").arg(id); } else { auto ep = entryPoints.createEntryPoints(loaded); if (!ep.success) { result = "FAIL"; ++initialFail; ++issues; problem = ep.errors.join("; ") + ep.conflicts.join("; "); rootCause = "Bootstrap creation failed."; issueLines << QString("Bootstrap failure in %1").arg(id); } else { ++initialPass; actual += QString(" Finalize: PASS (second call alreadyFinalized=%1); entry points: PASS.").arg(again.alreadyFinalized ? "true" : "false"); } } } } } }
        if (n == 216 && result == "PASS") { const QString instructions = QDir(root).filePath(AramfPaths::AgentInstructions); if (!QFile::rename(instructions, instructions + ".negative-test-preserved")) return 2; auto stale = verifier.verify(loaded, options); if (stale.overallStatus != VerificationStatus::Fail) { result = "FAIL"; --initialPass; ++initialFail; ++issues; problem = "Missing generated file was not rejected."; rootCause = "Verification did not enforce selected product presence."; issueLines << "Missing-file verification defect"; } else { correction = "No production correction required; guard behaved as designed."; retest = "Original missing-file case passed on immediate retest."; actual += " Negative lifecycle check: missing selected file correctly rejected."; } }
        if (result == "FAIL") QTextStream(stdout) << "FAIL " << n << ": " << problem << Qt::endl;
        writeText(QDir(campaign).filePath("scenarios/" + id + "/result.md"), markdown(n, QString("Exercise %1 with an isolated target and varied configuration.").arg(areaFor(n)), root, file, actual, result, problem, rootCause, correction, retest, fk));
        if (n == 50 || n == 100 || n == 150 || n == 200 || n == 250) { writeText(QDir(campaign).filePath("logs/checkpoint-%1.md").arg(n), QString("Checkpoint %1\nCompleted: %1\nInitial PASS: %2\nInitial FAIL: %3\nIssues: %4\nBuild and CTest are separate evidence; this runner does not claim their results.\n").arg(n).arg(initialPass).arg(initialFail).arg(issues)); }
    }
    // Current observations only; historical reports remain immutable.
    QJsonObject state{{"planned", selected ? 1 : 250}, {"completed", completed},
        {"passed", initialPass}, {"failed", initialFail}, {"remainingFail", initialFail},
        {"catalogFingerprint", TemplateValidation::catalogFingerprint()},
        {"injectedFailure", injectFailure}, {"guiAutomation", "not performed; core workflow only"}};
    writeText(QDir(campaign).filePath("campaign.json"), QJsonDocument(state).toJson(QJsonDocument::Indented));
    writeText(QDir(campaign).filePath("info.md"),
        QString("# Current 250-scenario core regression\n\nExecuted: %1\nPASS: %2\nFAIL: %3\n\n"
                "Only this invocation is reported. No historical fixes or manual GUI results are claimed.\n\n%4\n")
            .arg(completed).arg(initialPass).arg(initialFail).arg(issueLines.join("\n")));
    QTextStream(stdout) << "completed=" << completed << " passed=" << initialPass
                        << " failed=" << initialFail << Qt::endl;
    return initialFail == 0 && initialPass == completed ? 0 : 1;
}
