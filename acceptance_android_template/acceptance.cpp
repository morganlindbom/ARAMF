#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "core/ValidationRouting.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

static bool writeFile(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    f.write(data); return true;
}
static QByteArray readFile(const QString& path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return f.readAll();
}
static bool has(const QByteArray& data, const QByteArray& needle) { return data.contains(needle); }

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QString root = argc > 1 ? QDir::cleanPath(QString::fromLocal8Bit(argv[1])) : QDir::cleanPath(QDir::current().filePath("generated"));
    if (argc > 2 && QString::fromLocal8Bit(argv[2]) == "reload") {
        ProjectModel loaded;
        const bool ok = ProjectPersistence().load(&loaded, QDir(root).filePath("ARAMF_ANDROID_TEMPLATE_ACCEPTANCE.aramf.json"));
        QTextStream out(stdout);
        out << QJsonDocument(QJsonObject{{"loaded", ok}, {"profile", loaded.templateId()}, {"language", loaded.developmentEnvironment().language}, {"buildSystem", loaded.developmentEnvironment().buildSystem}, {"ide", loaded.developmentEnvironment().ide}, {"agent", loaded.aiConfiguration().primaryAgent}, {"resources", loaded.resources().size()}, {"certification", loaded.certificationConfiguration().enabled}}).toJson(QJsonDocument::Indented);
        return ok && loaded.templateId() == "android-studio-kotlin-gemini" && loaded.aiConfiguration().primaryAgent == "gemini" ? 0 : 1;
    }
    QDir().mkpath(root);
    QTextStream out(stdout);
    QJsonObject checks;
    auto check = [&](const QString& id, bool pass, const QString& detail) { checks.insert(id, QJsonObject{{"status", pass ? "PASS" : "FAIL"}, {"detail", detail}}); };

    const QString course = QDir(root).filePath("course-requirements.md");
    const QByteArray courseText = "# Course requirements\n- Kotlin is mandatory\n- Android Studio is the official IDE\n- minimum SDK = 26\n- XML layouts are mandatory for this test\n- Jetpack Compose must NOT be used\n- local persistence must use Room\n- unit tests are required\n- lint must pass before task completion\n";
    check("ANDROID-GEN-006", writeFile(course, courseText), "controlled primary Source of Truth fixture");

    ProjectModel model;
    model.setProjectName("ARAMF_ANDROID_TEMPLATE_ACCEPTANCE");
    model.setProjectPath(root);
    model.setProjectFilePath(QDir(root).filePath("ARAMF_ANDROID_TEMPLATE_ACCEPTANCE.aramf.json"));
    TemplateManager templates;
    const bool applied = templates.applyTemplate(&model, "android-studio-kotlin-gemini");
    check("ANDROID-TEMPLATE-001", applied && templates.definition("android-studio-kotlin-gemini").displayName == "Android Studio/Kotlin/Gemini", "real renamed template application");
    check("ANDROID-TEMPLATE-002", model.templateId() == "android-studio-kotlin-gemini", "canonical internal identifier");
    check("ANDROID-GEN-001", applied && model.templateId() == "android-studio-kotlin-gemini", "real Android template project generated");
    auto env = model.developmentEnvironment(); env.ide = "android-studio"; env.buildSystem = "gradle"; env.language = "kotlin"; model.setDevelopmentEnvironment(env);
    auto ai = model.aiConfiguration(); ai.primaryAgent = "gemini"; ai.additionalAgents = {"openai-codex"}; model.setAiConfiguration(ai);
    check("ANDROID-TEMPLATE-006", model.developmentEnvironment().language == "kotlin" && model.developmentEnvironment().buildSystem == "gradle" && model.developmentEnvironment().ide == "android-studio" && model.aiConfiguration().primaryAgent == "gemini", "template Android defaults");
    GenerationOptions userSelection; userSelection.generateMemory = false;
    check("ANDROID-TEMPLATE-007", !userSelection.generateMemory && userSelection.generateAgentRules, "user ARAMF product selection remains independent");
    ProjectResource source; source.id = "course-requirements"; source.name = "Course requirements"; source.type = "markdown"; source.location = "course-requirements.md"; source.description = QString::fromUtf8(courseText); source.authorityLevel = "primary-source-of-truth"; source.scopes = {"source-code", "academic-content"}; source.status = "available";
    model.setResources({source});
    const auto android = model.androidConstraints();
    check("ANDROID-CONSTRAINT-001", android.minSdk == 26 && android.minSdkSource == "course-requirements", "structured minimum SDK derived from Source of Truth");
    check("ANDROID-CONSTRAINT-004", templates.definition("android-studio-kotlin-gemini").capabilities.frameworks.contains("jetpack-compose"), "Compose remains globally available");
    check("ANDROID-CONSTRAINT-005", !android.composeAllowed, "Compose prohibited by project Source of Truth");
    check("ANDROID-CONSTRAINT-006", !android.composeSelected && !model.developmentCapabilities().frameworks.contains("jetpack-compose"), "prohibited Compose is not effective");
    check("ANDROID-CONSTRAINT-007", android.xmlRequired && android.uiTechnology == "xml", "XML required");
    check("ANDROID-CONSTRAINT-008", android.roomRequired, "Room required");
    check("ANDROID-CONSTRAINT-009", android.unitTestsRequired, "unit tests required");
    check("ANDROID-CONSTRAINT-010", android.lintRequired, "lint required");
    GenerationOptions options;
    const auto saved = ProjectPersistence().save(model, model.projectFilePath());
    const auto savedJson = QJsonDocument::fromJson(readFile(model.projectFilePath())).object();
    QJsonObject legacyJson = savedJson; legacyJson.insert("templateId", "android-kotlin-lite");
    const QString legacyFile = QDir(root).filePath("legacy.aramf.json");
    check("ANDROID-TEMPLATE-003", writeFile(legacyFile, QJsonDocument(legacyJson).toJson(QJsonDocument::Indented)), "legacy fixture created");
    ProjectModel migrated; const bool migratedOk = ProjectPersistence().load(&migrated, legacyFile);
    check("ANDROID-TEMPLATE-003", migratedOk && migrated.templateId() == "android-studio-kotlin-gemini" && migrated.androidConstraints().minSdk == 26, "legacy profile migrated safely");
    check("ANDROID-TEMPLATE-009", migratedOk && migrated.resources().size() == 1, "Source of Truth survives migration");
    check("ANDROID-TEMPLATE-010", migratedOk && migrated.memoryConfiguration().writerMode == "agent-direct", "Project Memory configuration survives migration");
    check("ANDROID-TEMPLATE-011", migratedOk && QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/memory/framework-knowledge.json")), "Framework Knowledge compatibility survives migration");
    check("ANDROID-TEMPLATE-012", migratedOk && migrated.androidConstraints().minSdk == 26, "effective Android config survives migration");
    check("ANDROID-TEMPLATE-013", migratedOk && migrated.aiConfiguration().primaryAgent == "gemini", "Gemini selection survives migration");
    const auto generated = GenerationServices().generate(model, options);
    check("ANDROID-TEMPLATE-010", generated.success && QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/memory/memory-contract.json")), "Project Memory survives migration");
    check("ANDROID-TEMPLATE-011", generated.success && QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/memory/framework-knowledge.json")), "Framework Knowledge compatibility survives migration");
    check("ANDROID-TEMPLATE-012", generated.success && QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/platforms/android-effective-config.json")), "effective Android config survives migration");
    check("ANDROID-GEN-002", QFileInfo(QDir(root).filePath("AGENTS.md")).size() < 1000 && has(readFile(QDir(root).filePath("AGENTS.md")), "ARAMF_WORKER/AGENTS.md"), "root is a small router");
    const QByteArray agent = readFile(QDir(root).filePath("ARAMF_WORKER/AGENTS.md"));
    check("ANDROID-GEN-003", has(agent, "Authority order") && has(agent, "Android / Kotlin") && has(agent, "PROJECT_STATUS.md") && has(agent, "Gemini") && has(agent, "VS Code"), "canonical Android governance");
    check("ANDROID-GEN-004", model.aiConfiguration().primaryAgent == "gemini", "Gemini selected");
    check("ANDROID-GEN-005", !has(agent, "Gemini is the governing") && has(agent, "governance remains agent-independent"), "Gemini is execution agent only");
    check("ANDROID-GEN-007", has(agent, "course-required XML overrides a Compose preference"), "course authority text generated");
    check("ANDROID-GEN-008", model.developmentEnvironment().language == "kotlin", "Kotlin");
    check("ANDROID-GEN-009", model.developmentEnvironment().buildSystem == "gradle", "Gradle");
    check("ANDROID-GEN-010", model.developmentEnvironment().ide == "android-studio", "Android Studio");
    const QByteArray platform = readFile(QDir(root).filePath("ARAMF_WORKER/platforms/platform-metadata.json"));
    check("ANDROID-GEN-011", has(platform, "android-sdk"), "Android SDK capability persisted in platform metadata");
    const QByteArray effectiveConfig = readFile(QDir(root).filePath("ARAMF_WORKER/platforms/android-effective-config.json"));
    check("ANDROID-GEN-012", has(effectiveConfig, "\"minSdk\": 26"), "minimum SDK 26 represented");
    check("ANDROID-GEN-013", has(platform, "Room") && has(agent, "Room"), "Room represented");
    check("ANDROID-CONSTRAINT-011", has(effectiveConfig, "course-requirements"), "constraint provenance preserved");
    check("ANDROID-CONSTRAINT-013", has(agent, "XML") && has(agent, "Compose allowed=NO") && has(agent, "unit tests required"), "governance reflects effective constraints");
    check("ANDROID-CONSTRAINT-014", has(agent, "VS Code") && has(agent, "primary IDE is not exclusive"), "external tooling explicitly permitted");
    check("ANDROID-CONSTRAINT-015", has(agent, "primary IDE android-studio"), "Android Studio remains primary");
    QDir worker(QDir(root).filePath("ARAMF_WORKER"));
    const QStringList workerFiles = worker.entryList(QDir::Files, QDir::Name);
    const QStringList workerDirs = worker.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    const bool selectedDirs = workerDirs.contains("memory") && workerDirs.contains("platforms") && workerDirs.contains("resources") && workerDirs.contains("routing") && workerDirs.contains("rules") && workerDirs.contains("verification");
    check("ANDROID-GEN-014", selectedDirs, "selected ARAMF products generate the worker subset");
    check("ANDROID-TEMPLATE-004", !has(readFile(QDir(root).filePath("ARAMF_WORKER/AGENTS.md")), "ARAMF Lite") && !has(readFile(QDir(root).filePath("ARAMF_WORKER/platforms/platform-metadata.json")), "governanceProfile"), "no separate product architecture wording");
    check("ANDROID-TEMPLATE-005", selectedDirs && !QDir(root).exists("ARAMF") && !QDir(root).exists("aramf"), "single ARAMF_WORKER control plane");
    check("ANDROID-GEN-015", !QDir(root).exists("ARAMF") && !QDir(root).exists("aramf"), "no parallel control plane");
    check("ANDROID-TEMPLATE-015", !has(agent, "ARAMF Lite") && !has(agent, "Lite project") && !has(agent, "Lite profile") && !has(agent, "Full ARAMF"), "no stale product-edition wording");
    check("ANDROID-GEN-016", generated.success && saved, "generation and persisted save succeeded");
    check("ANDROID-GEN-017", has(readFile(QDir(root).filePath("ARAMF_WORKER/routing/validation-policy.json")), "gradlew.bat build"), "Android validation routing");
    check("ANDROID-GEN-018", has(agent, "never fabricate emulator/device/runtime evidence"), "runtime states unclaimed");

    ProjectModel reloaded; const bool reloadedOk = ProjectPersistence().load(&reloaded, model.projectFilePath());
    check("ANDROID-GEN-019", reloadedOk && reloaded.templateId() == "android-studio-kotlin-gemini" && reloaded.aiConfiguration().primaryAgent == "gemini" && reloaded.androidConstraints().minSdk == 26 && !reloaded.androidConstraints().composeAllowed, "fresh persisted reload");
    check("ANDROID-CONSTRAINT-002", reloadedOk && reloaded.androidConstraints().minSdk == 26, "minimum SDK survives reload");
    check("ANDROID-GEN-020", has(agent, "VS Code") && has(agent, "external") || has(agent, "other agents"), "external tooling allowed");
    check("ANDROID-TEMPLATE-014", templates.builtInTemplates().first() == "pico-2w-visual-designer", "PVD template remains available");

    const QByteArray beforeCourse = readFile(QDir(root).filePath("ARAMF_WORKER/resources/resources.json"));
    auto expandedMemory = reloaded.memoryConfiguration(); expandedMemory.maintenanceOptions << "record-checkpoints"; reloaded.setMemoryConfiguration(expandedMemory);
    auto cert = reloaded.certificationConfiguration(); cert.enabled = true; reloaded.setCertificationConfiguration(cert);
    ProjectPersistence().save(reloaded, reloaded.projectFilePath());
    const auto expanded = GenerationServices().generate(reloaded, options);
    const bool expandedOk = expanded.success && QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/certification/certification-contract.json"));
    check("ANDROID-TEMPLATE-008", expandedOk, "selected ARAMF capability expansion in place");
    check("ANDROID-GEN-021", expandedOk, "selected ARAMF capability expansion in place");
    check("ANDROID-GEN-022", QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/AGENTS.md")) && QFileInfo(QDir(root).filePath("AGENTS.md")).size() < 1000, "authoritative files not relocated");
    check("ANDROID-GEN-023", readFile(QDir(root).filePath("ARAMF_WORKER/resources/resources.json")) == beforeCourse, "Source of Truth preserved through expansion");
    check("ANDROID-GEN-024", QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/memory/memory-contract.json")) && has(readFile(QDir(root).filePath("ARAMF_WORKER/memory/memory-contract.json")), "agent-direct"), "shared Project Memory architecture");
    check("ANDROID-GEN-025", QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/memory/framework-knowledge.json")) && !QDir(root).exists("GEMINI_MEMORY"), "shared Framework Knowledge architecture");

    const QByteArray memoryBeforeHandoff = readFile(QDir(root).filePath("ARAMF_WORKER/memory/memory-contract.json"));
    const QString projectFile = QDir(root).filePath("ARAMF_ANDROID_TEMPLATE_ACCEPTANCE.aramf.json");
    check("ANDROID-HANDOFF-001", reloaded.aiConfiguration().primaryAgent == "gemini", "Gemini initial state persists");
    auto codex = reloaded.aiConfiguration(); codex.primaryAgent = "openai-codex"; reloaded.setAiConfiguration(codex);
    const bool codexSaved = ProjectPersistence().save(reloaded, projectFile);
    const QJsonObject savedForHandoff = QJsonDocument::fromJson(readFile(projectFile)).object();
    ProjectModel codexReloaded; const bool codexLoaded = ProjectPersistence().load(&codexReloaded, projectFile);
    const bool codexHandoff = codexSaved && codexLoaded && codexReloaded.aiConfiguration().primaryAgent == "openai-codex";
    auto gemini = codexReloaded.aiConfiguration(); gemini.primaryAgent = "gemini"; codexReloaded.setAiConfiguration(gemini);
    const bool geminiSaved = ProjectPersistence().save(codexReloaded, projectFile);
    ProjectModel geminiReloaded; const bool geminiLoaded = ProjectPersistence().load(&geminiReloaded, projectFile);
    check("ANDROID-HANDOFF-002", codexHandoff, QString("Gemini to Codex persisted handoff save=%1 load=%2 inMemory=%3 file=%4 agent=%5").arg(codexSaved).arg(codexLoaded).arg(reloaded.aiConfiguration().primaryAgent).arg(savedForHandoff.value("ai").toObject().value("primaryAgent").toString()).arg(codexReloaded.aiConfiguration().primaryAgent));
    check("ANDROID-HANDOFF-003", codexLoaded && codexReloaded.androidConstraints().minSdk == 26, "Codex fresh rediscovery");
    check("ANDROID-HANDOFF-004", geminiSaved && geminiLoaded && geminiReloaded.aiConfiguration().primaryAgent == "gemini", "Codex to Gemini persisted handoff");
    check("ANDROID-HANDOFF-005", memoryBeforeHandoff == readFile(QDir(root).filePath("ARAMF_WORKER/memory/memory-contract.json")) && !QDir(root).exists("GEMINI_MEMORY"), "no agent-specific memory");
    check("ANDROID-HANDOFF-006", readFile(QDir(root).filePath("AGENTS.md")).contains("ARAMF_WORKER/AGENTS.md") && QFileInfo::exists(QDir(root).filePath("ARAMF_WORKER/AGENTS.md")), "governance unchanged through handoff");
    check("ANDROID-HANDOFF-007", geminiLoaded && geminiReloaded.androidConstraints().minSdk == 26 && !geminiReloaded.androidConstraints().composeAllowed, "effective constraints unchanged through handoff");
    check("ANDROID-CONSTRAINT-003", geminiLoaded && geminiReloaded.androidConstraints().minSdk == 26, "minimum SDK survives capability expansion");
    check("ANDROID-CONSTRAINT-012", effectiveConfig.contains("\"composeAvailable\": true") && effectiveConfig.contains("\"composeAllowed\": false"), "Source of Truth is not converted to Framework Knowledge");

    const auto route = ValidationRouting::route({"app/src/main/java/MainActivity.kt"}, "coding");
    const auto states = ValidationRouting::policy().value("android").toObject().value("states").toArray();
    check("validation-states", route.requiredChecks.contains("gradle-compile") && states.size() >= 8, "distinct Android validation states");
    check("source-boundary", !QFileInfo::exists(QDir(root).filePath("settings.gradle")) && !QFileInfo::exists(QDir(root).filePath("app/src/main/AndroidManifest.xml")), "governance/profile generation only");

    out << QJsonDocument(checks).toJson(QJsonDocument::Indented);
    return std::any_of(checks.begin(), checks.end(), [](const auto& v) { return v.toObject().value("status").toString() == "FAIL"; }) ? 1 : 0;
}
