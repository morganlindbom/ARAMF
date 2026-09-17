#include "core/ProcessVersion.h"
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"

#include <QDir>
#include <QJsonArray>
#include <QTemporaryDir>

#include <iostream>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool sameState(const ProcessVersionState& left, const ProcessVersionState& right)
{
    return left.completedHistory == right.completedHistory
        && left.legacyHistory == right.legacyHistory
        && left.hasActiveProcess == right.hasActiveProcess
        && left.activeProcess == right.activeProcess
        && left.hasNextProcess == right.hasNextProcess
        && left.nextProcess == right.nextProcess;
}
}

bool runProcessVersionTests()
{
    bool ok = true;
    QString error;
    ProcessVersion parsed;
    ok &= check(ProcessVersion::parse(QStringLiteral("P12.4.9.0.0"), &parsed, &error), "five-stage process version parses");
    ok &= check(parsed.identifier() == QStringLiteral("P12.4.9.0.0"), "five-stage process version formats canonically");
    ProcessVersion roundTrip;
    ok &= check(ProcessVersion::parse(parsed.identifier(), &roundTrip) && roundTrip == parsed, "five-stage round trip");
    ok &= check(!ProcessVersion::parse(QStringLiteral("P12.4.9.0"), &roundTrip, &error), "legacy four-stage string is not canonical");
    ok &= check(!ProcessVersion::parse(QStringLiteral("P12.0.1.0.0"), &roundTrip, &error), "zero loop is rejected");
    ok &= check(!ProcessVersion::parse(QStringLiteral("P12.1.3.2.0"), &roundTrip, &error), "non-binary certification is rejected");
    ok &= check(!ProcessVersion::parse(QStringLiteral("P12.1.3.0.1"), &roundTrip, &error), "done without certification is rejected");

    ProcessVersionState initial;
    initial.hasNextProcess = true;
    initial.nextProcess = {0, 1, 0, 0, 0};
    ok &= check(initial.isValid(&error), "five-stage initial state is valid");
    ok &= check(ProcessVersionLifecycle::startNextProcess(&initial, &error)
                    && initial.activeIdentifier() == QStringLiteral("P0.1.1.0.0"), "explicit process start");
    ok &= check(ProcessVersionLifecycle::advanceIteration(&initial, &error)
                    && initial.activeIdentifier() == QStringLiteral("P0.1.2.0.0"), "material iteration resets certification");
    ok &= check(ProcessVersionLifecycle::certifyCurrentIteration(&initial, &error)
                    && initial.activeIdentifier() == QStringLiteral("P0.1.2.1.0"), "explicit certification is separate from completion");
    ok &= check(ProcessVersionLifecycle::advanceIteration(&initial, &error)
                    && initial.activeIdentifier() == QStringLiteral("P0.1.3.0.0"), "material correction invalidates certification");
    ok &= check(ProcessVersionLifecycle::certifyCurrentIteration(&initial, &error)
                    && ProcessVersionLifecycle::completeActiveProcess(&initial, &error)
                    && initial.completedIdentifiers().last() == QStringLiteral("P0.1.3.1.1"), "explicit certified completion");
    ok &= check(!ProcessVersionLifecycle::completeActiveProcess(&initial, &error), "closed process cannot be completed again");

    ProcessVersionState dependency;
    dependency.completedHistory = {{0, 1, 1, 1, 1}};
    dependency.hasNextProcess = true;
    dependency.nextProcess = {1, 1, 0, 0, 0};
    ok &= check(dependency.isValid(&error), "dependent process state is valid");

    ProcessVersionState p0Rework;
    p0Rework.completedHistory = {{0, 1, 1, 1, 1}, {1, 1, 1, 1, 1}};
    p0Rework.hasNextProcess = true;
    p0Rework.nextProcess = {2, 1, 0, 0, 0};
    ok &= check(ProcessVersionLifecycle::reworkCompletedProcess(&p0Rework, 0, &error)
                    && p0Rework.activeIdentifier() == QStringLiteral("P0.1.2.0.0")
                    && p0Rework.nextIdentifier() == QStringLiteral("P2.1.0.0.0"), "same-loop certified P0 rework preserves P1 and next process");

    QTemporaryDir temporary;
    ok &= check(temporary.isValid(), "temporary persistence directory is available");
    if (temporary.isValid()) {
        ProjectPersistence persistence;
        ProjectModel source;
        auto projectJson = persistence.toJson(source);
        projectJson.insert(QStringLiteral("processVersion"), processVersionStateToJson(dependency));
        ProjectModel loaded;
        ok &= check(persistence.fromJson(&loaded, projectJson, &error) && sameState(loaded.processVersionState(), dependency), "five-stage state loads");
        const QString path = QDir(temporary.path()).filePath(QStringLiteral("process.aramf.json"));
        ok &= check(persistence.save(loaded, path, &error), "five-stage state saves");
        ProjectModel reloaded;
        ok &= check(persistence.load(&reloaded, path, &error) && sameState(reloaded.processVersionState(), dependency), "five-stage state survives reload");

        auto legacy = persistence.toJson(source);
        legacy.insert(QStringLiteral("processVersion"), QJsonObject{
            {QStringLiteral("completedHistory"), QJsonArray{QJsonObject{{"process", 0}, {"loop", 1}, {"iteration", 1}, {"done", 1}}}},
            {QStringLiteral("active"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("next"), QJsonObject{{"process", 1}, {"loop", 1}, {"iteration", 0}, {"done", 0}}}});
        ProjectModel legacyLoaded;
        ok &= check(persistence.fromJson(&legacyLoaded, legacy, &error)
                        && legacyLoaded.processVersionState().completedHistory.isEmpty()
                        && legacyLoaded.processVersionState().legacyHistory == QStringList{"P0.1.1.1", "P1.1.0.0"}, "legacy four-stage history is preserved separately");
        ok &= check(legacyLoaded.resetForFiveStageProcessCampaign(&error)
                        && legacyLoaded.processVersionState().nextIdentifier() == QStringLiteral("P0.1.0.0.0"), "explicit five-stage reset creates P0 next state");

        auto malformed = persistence.toJson(source);
        malformed.insert(QStringLiteral("processVersion"), QJsonObject{
            {QStringLiteral("completedHistory"), QJsonArray{}},
            {QStringLiteral("active"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("next"), QJsonObject{{"process", 2}, {"loop", 1}, {"iteration", 0}, {"certification", 0}, {"done", 1}}}});
        ProjectModel malformedLoaded;
        ok &= check(!persistence.fromJson(&malformedLoaded, malformed, &error), "illegal done-before-certification is rejected");
    }
    return ok;
}
