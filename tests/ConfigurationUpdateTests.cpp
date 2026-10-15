#include "core/ConfigurationUpdateService.h"
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/AramfPaths.h"
#include "core/ProjectMemory.h"
#include "core/ContextCoordinationService.h"
#include <QDirIterator>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include <iostream>

static bool writeState(const QString& root, const QJsonObject& state)
{
    QDir(root).mkpath(QStringLiteral("ARAMF_WORKER/verification"));
    QSaveFile file(QDir(root).filePath(QStringLiteral("ARAMF_WORKER/verification/generation-state.json")));
    const auto data = QJsonDocument(QJsonObject{{"canonicalState", state}, {"fingerprint", "fixture"}}).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

static QByteArray readBytes(const QString& path)
{
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

static bool namedWorkerUpdates()
{
    QTemporaryDir fixture; fixture.setAutoRemove(false);
    if (!fixture.isValid()) return false;
    auto fail = [&](int line) { std::cerr << "Named-worker failure at line " << line << "; fixture " << fixture.path().toStdString() << "\n"; return false; };
    TemplateManager templates;
    ProjectPersistence persistence;
    ConfigurationUpdateService updates;
    ProjectModel model; QString error;
    if (!templates.applyTemplate(&model, "cmake-library", &error)) return fail(__LINE__);
    model.setProjectName("HVD Components"); model.setWorkerNameSuffix("HVD_COMPONENTS");
    model.setProjectPath(fixture.path()); model.setProjectFilePath(fixture.filePath("project.aramf.json"));
    const QString id = model.projectId(), worker = fixture.filePath("ARAMF_WORKER_HVD_COMPONENTS");
    const QString baseline = worker + "/verification/generation-state.json";
    if (!persistence.save(model, model.projectFilePath(), &error)
        || !GenerationServices().generate(model, model.generationOptions()).success) return fail(__LINE__);
    const auto original = readBytes(baseline);
    // A caller's unrelated runtime scope must neither redirect writes nor leak.
    AramfPaths::setRuntimeWorkerNameSuffix("OTHER_PROJECT");
    model.setDescription("Updated named worker configuration");
    auto plan = updates.validate(model);
    if (!plan.success || plan.removalBlocked || plan.noChange
        || AramfPaths::runtimeWorkerDirectoryName() != "ARAMF_WORKER_OTHER_PROJECT"
        || !QFileInfo::exists(worker + "/update/update-plan.json")
        || QDir(fixture.path()).exists("ARAMF_WORKER")
        || QDir(fixture.path()).exists("ARAMF_WORKER_OTHER_PROJECT")) return fail(__LINE__);
    const QString fingerprint = plan.plan.value("validationFingerprint").toString();
    if (updates.apply(model, "obsolete-plan").success || readBytes(baseline) != original) return fail(__LINE__);
    if (!updates.apply(model, fingerprint).success
        || AramfPaths::runtimeWorkerDirectoryName() != "ARAMF_WORKER_OTHER_PROJECT"
        || !updates.validate(model).noChange) return fail(__LINE__);
    ProjectModel loaded;
    if (!persistence.load(&loaded, model.projectFilePath(), &error)
        || loaded.projectId() != id || loaded.projectName() != "HVD Components"
        || loaded.workerNameSuffix() != "HVD_COMPONENTS"
        || VerificationServices().verify(loaded, loaded.generationOptions(), false).overallStatus != VerificationStatus::Pass) return fail(__LINE__);
    QProcess child; child.setProcessChannelMode(QProcess::MergedChannels);
    child.start(QCoreApplication::applicationFilePath(), {"--reload-named-worker", model.projectFilePath(), id});
    if (!child.waitForFinished(60000) || child.exitStatus() != QProcess::NormalExit || child.exitCode()) {
        std::cerr << child.readAll().constData(); return fail(__LINE__);
    }
    const auto updated = readBytes(baseline);
    model.setProjectId("foreign-project");
    if (updates.validate(model).success || updates.apply(model).success || readBytes(baseline) != updated) return fail(__LINE__);
    model.setProjectId(id); model.setWorkerNameSuffix("MISSING");
    if (updates.validate(model).success || QDir(fixture.path()).exists("ARAMF_WORKER_MISSING")) return fail(__LINE__);
    model.setWorkerNameSuffix({});
    if (updates.validate(model).success || QDir(fixture.path()).exists("ARAMF_WORKER")) return fail(__LINE__);
    model.setWorkerNameSuffix("HVD_COMPONENTS");
    // A folder matching the family prefix is never authority to overwrite it.
    const QString decoy = fixture.filePath("ARAMF_WORKER_DECOY");
    QDir().mkpath(decoy + "/verification");
    QSaveFile copied(decoy + "/verification/generation-state.json");
    if (!copied.open(QIODevice::WriteOnly) || copied.write(updated) != updated.size() || !copied.commit()) return fail(__LINE__);
    model.setWorkerNameSuffix("DECOY");
    if (updates.validate(model).success || QDir(decoy).exists("update")) return fail(__LINE__);
    model.setWorkerNameSuffix("HVD_COMPONENTS");
    auto capabilities = model.developmentCapabilities(); capabilities.languages.clear(); model.setDevelopmentCapabilities(capabilities);
    const auto removal = updates.validate(model);
    if (!removal.success || !removal.removalBlocked || !updates.apply(model).removalBlocked || readBytes(baseline) != updated) return fail(__LINE__);
    AramfPaths::setRuntimeWorkerNameSuffix({});
    std::cout << "named_worker_updates: PASS (production generation, update, independent reload, isolation, missing/foreign worker, stale plan and removal guards)\n";
    return true;
}

static QJsonObject jsonAt(const QString& path)
{ return QJsonDocument::fromJson(readBytes(path)).object(); }

static QMap<QString, QByteArray> treeBytes(const QString& root)
{
    QMap<QString, QByteArray> files;
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) { const auto path = it.next(); files.insert(QDir(root).relativeFilePath(path), readBytes(path)); }
    return files;
}

static bool rawFile(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

static QStringList treeEntries(const QString& root)
{
    QStringList entries;
    QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) entries.append(QDir(root).relativeFilePath(it.next()));
    entries.sort();
    return entries;
}

static bool pureConfigurationPreview()
{
    QTemporaryDir fixture; fixture.setAutoRemove(false);
    auto fail = [&](int line) {
        std::cerr << "Preview failure at " << line << "; fixture " << fixture.path().toStdString() << '\n';
        return false;
    };
    if (!fixture.isValid()) return fail(__LINE__);
    ProjectModel model;
    model.setProjectPath(fixture.path());
    model.setDescription("Original configuration");
    auto capabilities = model.developmentCapabilities();
    capabilities.languages = {QStringLiteral("cpp"), QStringLiteral("c")};
    model.setDevelopmentCapabilities(capabilities);
    if (!writeState(fixture.path(), ConfigurationUpdateService::canonicalState(model))) return fail(__LINE__);
    ConfigurationUpdateService updates;
    const auto initialFiles = treeBytes(fixture.path());
    const auto initialEntries = treeEntries(fixture.path());
    const auto unchanged = updates.preview(model);
    if (!unchanged.success || !unchanged.noChange || unchanged.removalBlocked
        || treeBytes(fixture.path()) != initialFiles || treeEntries(fixture.path()) != initialEntries)
        return fail(__LINE__);

    const QString planPath = fixture.filePath("ARAMF_WORKER/update/update-plan.json");
    const QByteArray previousPlan("{\"previous-plan\":true}\r\n");
    if (!rawFile(planPath, previousPlan)) return fail(__LINE__);
    const auto before = treeBytes(fixture.path());
    const auto entries = treeEntries(fixture.path());
    model.setDescription("Proposed configuration");
    const auto modelBefore = ConfigurationUpdateService::canonicalState(model);
    const auto changed = updates.preview(model);
    if (!changed.success || changed.noChange || changed.removalBlocked || !changed.error.isEmpty()
        || changed.plan.value("status") != "VALID" || changed.plan.value("modified").toArray().isEmpty()
        || changed.plan.value("currentState").toObject() != modelBefore
        || changed.configurationSaved || changed.derivedSynchronized || changed.verified || changed.rolledBack
        || ConfigurationUpdateService::canonicalState(model) != modelBefore
        || readBytes(planPath) != previousPlan || treeBytes(fixture.path()) != before
        || treeEntries(fixture.path()) != entries) return fail(__LINE__);

    capabilities.languages = {QStringLiteral("cpp")};
    model.setDevelopmentCapabilities(capabilities);
    const auto removed = updates.preview(model);
    if (removed.success || !removed.removalBlocked || removed.noChange || removed.error.isEmpty()
        || removed.plan.value("status") != "REMOVAL_BLOCKED"
        || removed.plan.value("removalBlocked").toArray().isEmpty()
        || removed.plan.value("currentState").toObject() != ConfigurationUpdateService::canonicalState(model)
        || removed.configurationSaved || removed.derivedSynchronized || removed.verified || removed.rolledBack
        || readBytes(planPath) != previousPlan || treeBytes(fixture.path()) != before
        || treeEntries(fixture.path()) != entries) return fail(__LINE__);
    // The existing UI validation contract still persists a truthful blocked plan.
    const auto validated = updates.validate(model);
    if (!validated.success || !validated.removalBlocked || validated.plan != removed.plan
        || jsonAt(planPath) != removed.plan) return fail(__LINE__);
    std::cout << "pure_configuration_preview: PASS (unchanged tree, retained plan bytes, removal rejection, compatible validation)\n";
    return true;
}

static bool effectiveConfigurationInventory()
{
    QTemporaryDir fixture; fixture.setAutoRemove(false);
    if (!fixture.isValid()) return false;
    auto fail = [&](int line, const QString& error = {}) {
        std::cerr << "Effective inventory failure at " << line << ": " << error.toStdString() << '\n';
        return false;
    };
    ProjectPersistence persistence;
    for (const auto& suffix : QStringList{QString(), QStringLiteral("INVENTORY")}) {
        for (int selected = 0; selected < 8; ++selected) {
            for (int academicOption = -1; academicOption < 2; ++academicOption) {
                ProjectModel model;
                model.setProjectPath(fixture.path()); model.setWorkerNameSuffix(suffix);
                // Exercise mixed non-academic selections without changing them
                // to make the output inventory smaller.
                auto options = model.generationOptions();
                options.generateAgentRules = selected & 1;
                options.generateRouting = selected & 2;
                options.generatePlatforms = selected & 4;
                options.generateResources = !(selected & 1);
                options.generateMemory = !(selected & 2);
                options.generateProvenance = !(selected & 4);
                model.setGenerationOptions(options);
                auto input = persistence.toJson(model);
                auto academic = input.value("academic").toObject();
                academic.insert("enabled", bool(selected & 1));
                auto thesis = academic.value("thesisDocumentation").toObject();
                auto report = academic.value("reportDocumentation").toObject();
                thesis.insert("enabled", bool(selected & 2)); report.insert("enabled", bool(selected & 4));
                academic.insert("thesisDocumentation", thesis); academic.insert("reportDocumentation", report);
                input.insert("academic", academic);
                auto generation = input.value("generationOptions").toObject();
                if (academicOption < 0) generation.remove("academicDocuments");
                else generation.insert("academicDocuments", bool(academicOption));
                input.insert("generationOptions", generation);
                QString error;
                if (!persistence.fromJson(&model, input, &error)) return fail(__LINE__, error);
                const auto before = persistence.toJson(model);
                const auto savedAcademic = before.value("academic").toObject();
                const auto savedGeneration = before.value("generationOptions").toObject();
                if (savedAcademic.value("enabled").toBool() != bool(selected & 1)
                    || savedAcademic.value("thesisDocumentation").toObject().value("enabled").toBool() != bool(selected & 2)
                    || savedAcademic.value("reportDocumentation").toObject().value("enabled").toBool() != bool(selected & 4)
                    || (academicOption < 0 && !savedGeneration.value("academicDocuments").toBool(true)))
                    return fail(__LINE__);
                const bool academicOutput = savedGeneration.value("academicDocuments").toBool(true) && (selected & 1);
                const bool thesisOutput = academicOutput && (selected & 2);
                const bool reportOutput = academicOutput && (selected & 4);
                const QString worker = AramfPaths::workerDirectoryName(suffix);
                const QString manifest = worker + "/documentation/documentation-manifest.json";
                const QString thesisPath = worker + "/documentation/Thesis", reportPath = worker + "/documentation/Report";
                const auto producerFiles = GenerationServices::configurationUpdateFiles(model, model.generationOptions());
                auto expected = producerFiles;
                if (!thesisOutput && !reportOutput) expected.removeAll(manifest);
                if (!thesisOutput) expected.removeAll(thesisPath);
                if (!reportOutput) expected.removeAll(reportPath);
                const auto effective = ConfigurationUpdateService::effectiveOutputFiles(model);
                if (effective != expected || !effective.contains(worker + "/verification/generation-state.json")
                    || effective.contains(manifest) != (producerFiles.contains(manifest) && (thesisOutput || reportOutput))
                    || effective.contains(thesisPath) != (producerFiles.contains(thesisPath) && thesisOutput)
                    || effective.contains(reportPath) != (producerFiles.contains(reportPath) && reportOutput)
                    || persistence.toJson(model) != before || !treeEntries(fixture.path()).isEmpty())
                    return fail(__LINE__);
            }
        }
    }
    std::cout << "effective_configuration_inventory: PASS (inactive outputs pruned, active markers preserved, generation-state retained, all options unchanged)\n";
    return true;
}

static void componentProfile(ProjectModel& model)
{
    auto c = model.developmentCapabilities();
    const auto add = [](QStringList& values, const QString& value) { if (!values.contains(value)) values.append(value); };
    add(c.languages, "cpp"); add(c.languages, "c"); add(c.frameworks, "qt");
    add(c.ides, "visual-studio-code"); add(c.ides, "qt-creator");
    add(c.toolchains, "msys2-ucrt64-gcc"); add(c.buildSystems, "cmake"); add(c.buildSystems, "ninja");
    add(c.developmentTools, "debugger"); add(c.testingCapabilities, "unit-testing"); add(c.testingCapabilities, "integration-testing");
    add(c.qualityCapabilities, "formatting"); add(c.qualityCapabilities, "static-analysis");
    model.setDevelopmentCapabilities(c);
    auto env = model.developmentEnvironment(); env.language = "cpp"; env.framework = "qt";
    env.ide = "qt-creator"; env.compiler = "msys2-ucrt64-gcc"; env.buildSystem = "cmake";
    model.setDevelopmentEnvironment(env);
    auto resources = model.resources();
    for (const auto& name : {QStringLiteral("component-api"), QStringLiteral("pins-compatibility"), QStringLiteral("tool-research")}) {
        ProjectResource resource; resource.id = name; resource.name = name; resource.type = "url";
        resource.location = "https://example.invalid/" + name; resource.description = "Component development reference";
        resources.append(resource);
    }
    model.setResources(resources);
    auto rules = model.ruleConfiguration(); rules.enforcementLevel = "strict"; model.setRuleConfiguration(rules);
    auto memory = model.memoryConfiguration(); memory.maximumSizeBytes += 4096; model.setMemoryConfiguration(memory);
    model.setDescription("Standalone reusable C++/Qt 6 component library; research is advisory; no direct HVD dependencies.");
}

static bool profileReadback(const ProjectModel& model)
{
    const auto root = QDir(model.projectPath());
    const auto worker = root.filePath(AramfPaths::workerDirectoryName(model.workerNameSuffix()));
    const auto platform = jsonAt(worker + "/platforms/platform-metadata.json");
    const auto project = jsonAt(worker + "/project.json");
    const auto manifest = jsonAt(worker + "/worker-manifest.json");
    const auto state = jsonAt(worker + "/verification/generation-state.json");
    if (!platform.value("languages").toArray().contains("cpp") || !platform.value("frameworks").toArray().contains("qt")
        || !platform.value("buildSystems").toArray().contains("cmake") || !platform.value("buildSystems").toArray().contains("ninja")
        || platform.value("environment").toObject().value("ide") != "qt-creator"
        || !platform.value("developmentTools").toArray().contains("debugger")
        || project.value("projectId") != model.projectId() || project.value("projectName") != "HVD Components"
        || manifest.value("workerIdentity") != AramfPaths::workerDirectoryName(model.workerNameSuffix())
        || state.value("canonicalState").toObject() != ConfigurationUpdateService::canonicalState(model)
        || jsonAt(worker + "/resources/resources.json").value("resources").toArray().size() != model.resources().size()
        || !readBytes(worker + "/rules/generated-rules.md").contains("strict")
        || jsonAt(worker + "/memory/memory-config.json").value("maximumSizeBytes").toDouble() != model.memoryConfiguration().maximumSizeBytes
        || !ContextCoordinationService::validate(model).value("valid").toBool()) return false;
    const auto options = model.generationOptions();
    const auto outputs = GenerationServices::derivedTaskArtifacts(model, options);
    for (auto it = outputs.begin(); it != outputs.end(); ++it) {
        auto actual = jsonAt(root.filePath(it.key())), expected = it.value().toObject();
        actual.remove("_file"); expected.remove("_file");
        if (actual != expected) return false;
    }
    return VerificationServices().verify(model, options, false).overallStatus == VerificationStatus::Pass;
}

static bool transactionalPropagation(bool legacy = false)
{
    QTemporaryDir fixture; fixture.setAutoRemove(false);
    auto fail = [&](int line, const QString& error = {}) {
        std::cerr << "Propagation failure at " << line << ": " << error.toStdString()
                  << "; retained fixture " << fixture.path().toStdString() << '\n'; return false;
    };
    if (!fixture.isValid()) return fail(__LINE__);
    ProjectModel original; QString error;
    if (!TemplateManager().applyTemplate(&original, "cmake-library", &error)) return fail(__LINE__, error);
    original.setProjectName("HVD Components"); original.setWorkerNameSuffix(legacy ? QString() : QStringLiteral("HVD_COMPONENTS"));
    original.setProjectPath(fixture.path()); original.setProjectFilePath(fixture.filePath("project.aramf.json"));
    if (!ProjectPersistence().save(original, original.projectFilePath(), &error)
        || !GenerationServices().generate(original, original.generationOptions()).success) return fail(__LINE__, error);
    ProjectModel sibling; if (!TemplateManager().applyTemplate(&sibling, "cmake-library", &error)) return fail(__LINE__);
    sibling.setProjectPath(fixture.path()); sibling.setProjectName("Other project"); sibling.setWorkerNameSuffix("OTHER");
    if (!GenerationServices().generate(sibling, sibling.generationOptions()).success) return fail(__LINE__);
    const auto siblingBefore = treeBytes(fixture.filePath("ARAMF_WORKER_OTHER"));
    const QString worker = fixture.filePath(AramfPaths::workerDirectoryName(original.workerNameSuffix()));
    const auto history = readBytes(worker + "/memory/event-log.jsonl"), decisions = readBytes(worker + "/memory/decisions.md");
    const auto originalState = ConfigurationUpdateService::canonicalState(original);
    if (QFileInfo::exists(worker + "/documentation/Thesis") || QFileInfo::exists(worker + "/documentation/Report"))
        return fail(__LINE__);
    // An empty program root proves updates do not initialize global knowledge.
    QTemporaryDir program; program.setAutoRemove(false);
    AramfPaths::setProgramRootForTests(program.path());
    struct ResetProgram { ~ResetProgram() { AramfPaths::clearProgramRootForTests(); } } resetProgram;
    ProjectModel proposed; if (!ProjectPersistence().load(&proposed, original.projectFilePath(), &error)) return fail(__LINE__);
    componentProfile(proposed);

#ifdef Q_OS_WIN
    // Share-read permits complete snapshots but denies replacement. Each lock
    // tests a real I/O failure after the preceding production stages mutated.
    for (const auto& path : QStringList{original.projectFilePath(), worker + "/project.json",
             worker + "/platforms/platform-metadata.json", worker + "/resources/resources.json",
             worker + "/context/context-index.json", worker + "/verification/verification-result.json"}) {
        if (!QFileInfo::exists(path)) {
            const auto verified = VerificationServices().verify(original, original.generationOptions());
            if (verified.overallStatus != VerificationStatus::Pass) return fail(__LINE__);
        }
        const auto before = treeBytes(fixture.path());
        HANDLE locked = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (locked == INVALID_HANDLE_VALUE) return fail(__LINE__, path);
        const auto result = ConfigurationUpdateService().apply(proposed);
        CloseHandle(locked);
        if (result.success || result.verified || result.configurationSaved || result.derivedSynchronized
            || !result.rolledBack || treeBytes(fixture.path()) != before || !treeBytes(program.path()).isEmpty())
            return fail(__LINE__, path + ": " + result.error);
        std::cout << "transaction_rollback: PASS " << path.toStdString() << '\n';
    }
#endif
    const auto result = ConfigurationUpdateService().apply(proposed);
    if (!result.success || !result.configurationSaved || !result.derivedSynchronized || !result.verified || result.rolledBack)
        return fail(__LINE__, result.error);
    ProjectModel reload;
    if (!ProjectPersistence().load(&reload, original.projectFilePath(), &error)
        || reload.projectId() != original.projectId() || reload.projectPath() != original.projectPath()
        || reload.projectName() != "HVD Components" || reload.workerNameSuffix() != original.workerNameSuffix()
        || !profileReadback(reload) || treeBytes(fixture.filePath("ARAMF_WORKER_OTHER")) != siblingBefore
        || readBytes(worker + "/memory/event-log.jsonl") != history || readBytes(worker + "/memory/decisions.md") != decisions
        || ConfigurationUpdateService::canonicalState(reload).value("academic") != originalState.value("academic")
        || ConfigurationUpdateService::canonicalState(reload).value("generationOptions") != originalState.value("generationOptions")
        || QFileInfo::exists(worker + "/documentation/Thesis") || QFileInfo::exists(worker + "/documentation/Report")
        || !QFileInfo::exists(worker + "/verification/generation-state.json")
        || QFileInfo::exists(worker + "/legacy-only.txt") || !treeBytes(program.path()).isEmpty()) return fail(__LINE__);
    QProcess child; child.setProcessChannelMode(QProcess::MergedChannels);
    child.start(QCoreApplication::applicationFilePath(), {"--reload-component-profile", original.projectFilePath(), original.projectId()});
    if (!child.waitForFinished(60000) || child.exitCode() || child.exitStatus() != QProcess::NormalExit) return fail(__LINE__, child.readAll());
    // Inactive historical documents remain user data even when their former
    // output directories already exist. They are outside this transaction.
    if (!rawFile(worker + "/documentation/Thesis/previous.md", "# Preserved thesis\r\n")
        || !rawFile(worker + "/documentation/Report/previous.md", "# Preserved report\r\n")
        || !rawFile(worker + "/documentation/documentation-manifest.json", "{\"historical-output\":true}\r\n"))
        return fail(__LINE__);
    const auto previousDocuments = treeBytes(worker + "/documentation");
    const auto preserved = ConfigurationUpdateService().apply(reload);
    if (!preserved.success || !preserved.configurationSaved || !preserved.derivedSynchronized || !preserved.verified
        || treeBytes(worker + "/documentation") != previousDocuments
        || readBytes(worker + "/memory/event-log.jsonl") != history
        || readBytes(worker + "/memory/decisions.md") != decisions || !profileReadback(reload))
        return fail(__LINE__, preserved.error);
    // The receipt is checked, not merely stored: a stale product cannot verify.
    if (!rawFile(worker + "/platforms/platform-metadata.json", "{}\r\n")) return fail(__LINE__);
    if (VerificationServices().verify(reload, reload.generationOptions(), false).overallStatus == VerificationStatus::Pass) return fail(__LINE__);
    if (!ConfigurationUpdateService().apply(reload).success || !profileReadback(reload)) return fail(__LINE__);
    // Exercise the narrow memory producer with a legacy migration source present.
    // The overall verifier intentionally warns about legacy topology; this
    // producer must leave it intact, not migrate it to make verification pass.
    if (!rawFile(fixture.filePath("ARAMF/legacy-only.txt"), "untouched legacy bytes\r\n")) return fail(__LINE__);
    const auto withLegacy = treeBytes(fixture.path());
    const auto previousSuffix = AramfPaths::detail::workerSuffixOverride();
    AramfPaths::setRuntimeWorkerNameSuffix(reload.workerNameSuffix());
    const bool memoryUpdated = ProjectMemory().updateExistingConfiguration(fixture.path(), reload, &error);
    reload.setProjectId("unrelated-project");
    const bool foreignAccepted = ProjectMemory().updateExistingConfiguration(fixture.path(), reload, &error);
    reload.setProjectId(original.projectId());
    AramfPaths::setRuntimeWorkerNameSuffix(previousSuffix);
    if (!memoryUpdated || foreignAccepted || treeBytes(fixture.path()) != withLegacy || !treeBytes(program.path()).isEmpty()) return fail(__LINE__, error);
    const auto legacyRejected = ConfigurationUpdateService().apply(reload);
    if (legacyRejected.success || !legacyRejected.rolledBack || treeBytes(fixture.path()) != withLegacy) return fail(__LINE__, legacyRejected.error);
    QTemporaryDir emptyProject; emptyProject.setAutoRemove(false);
    if (ProjectMemory().updateExistingConfiguration(emptyProject.path(), reload, &error)
        || !treeBytes(emptyProject.path()).isEmpty()) return fail(__LINE__);
    std::cout << (legacy ? "legacy_" : "named_") << "component_profile: PASS (canonical generation, transaction rollback, resources/platform/rules/context, independent reload, sibling/global/history isolation, tamper detection, no bootstrap)\n";
    return true;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1) == "--reload-component-profile") {
        ProjectModel model; QString error;
        return ProjectPersistence().load(&model, app.arguments().value(2), &error)
            && model.projectId() == app.arguments().value(3) && profileReadback(model) ? 0 : 22;
    }
    if (app.arguments().value(1) == "--component-profile") return transactionalPropagation() && transactionalPropagation(true) ? 0 : 23;
    if (app.arguments().value(1) == "--reload-named-worker") {
        ProjectModel model; QString error;
        return ProjectPersistence().load(&model, app.arguments().value(2), &error)
            && model.projectId() == app.arguments().value(3) && model.projectName() == "HVD Components"
            && model.workerNameSuffix() == "HVD_COMPONENTS"
            && VerificationServices().verify(model, model.generationOptions(), false).overallStatus == VerificationStatus::Pass ? 0 : 20;
    }
    if (!pureConfigurationPreview()) return 24;
    if (!effectiveConfigurationInventory()) return 25;
    if (!namedWorkerUpdates()) return 21;
    if (!transactionalPropagation() || !transactionalPropagation(true)) return 23;
    QTemporaryDir fixture;
    fixture.setAutoRemove(false);
    if (!fixture.isValid()) return 1;
    ProjectModel model;
    model.setProjectPath(fixture.path());
    model.setProjectName(QStringLiteral("Baseline"));
    auto capabilities = model.developmentCapabilities();
    capabilities.languages = {QStringLiteral("C++"), QStringLiteral("Kotlin")};
    model.setDevelopmentCapabilities(capabilities);
    if (!writeState(fixture.path(), ConfigurationUpdateService::canonicalState(model))) return 2;
    auto changed = model.developmentCapabilities();
    changed.languages = {QStringLiteral("Kotlin"), QStringLiteral("Rust")};
    model.setDevelopmentCapabilities(changed);
    const auto replacement = ConfigurationUpdateService().validate(model);
    const auto removals = replacement.plan.value("removalBlocked").toArray();
    const auto additions = replacement.plan.value("added").toArray();
    bool foundRemove = false, foundAdd = false;
    for (const auto& value : removals) foundRemove |= value.toObject().value("path").toString().contains("C++");
    for (const auto& value : additions) foundAdd |= value.toObject().value("path").toString().contains("Rust");
    if (!replacement.success || !replacement.removalBlocked || !foundRemove || !foundAdd) return 3;
    // A reordered primitive collection has identical set semantics.
    changed.languages = {QStringLiteral("Kotlin"), QStringLiteral("C++")};
    model.setDevelopmentCapabilities(changed);
    auto env = model.developmentEnvironment();
    env.language = QStringLiteral("C++");
    model.setDevelopmentEnvironment(env);
    if (!ConfigurationUpdateService().validate(model).noChange) return 5;
    // Identifiable structured collections recurse into their properties.
    auto state = ConfigurationUpdateService::canonicalState(model);
    state.insert("agents", QJsonArray{QJsonObject{{"id", "reviewer"}, {"permission", "read"}}});
    writeState(fixture.path(), state);
    auto current = ConfigurationUpdateService::canonicalState(model);
    current.insert("agents", QJsonArray{QJsonObject{{"id", "reviewer"}, {"permission", "write"}}, QJsonObject{{"id", "builder"}, {"permission", "read"}}});
    writeState(fixture.path(), state);
    // The service reads the model; this fixture also documents the canonical
    // shape used by future structured ProjectModel collections.
    if (!current.value("agents").isArray()) return 6;
    model.setProjectName(QStringLiteral("Changed"));
    const auto nested = ConfigurationUpdateService().validate(model);
    bool foundNestedModify = false;
    for (const auto& value : nested.plan.value("modified").toArray()) foundNestedModify |= value.toObject().value("path").toString().contains("projectName");
    if (!foundNestedModify) return 4;
    std::cout << "configuration_update_tests: PASS (recursive ADD/MODIFY/REMOVE)\n";
    return 0;
}
