#include "core/ProcessVersion.h"
#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
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
        && left.hasActiveProcess == right.hasActiveProcess
        && left.activeProcess == right.activeProcess
        && left.hasNextProcess == right.hasNextProcess
        && left.nextProcess == right.nextProcess;
}
}

bool runProcessVersionTests()
{
    bool ok = true;

    ProcessVersion parsed;
    QString error;
    ok &= check(ProcessVersion::parse(QStringLiteral("P12.1.3.0"), &parsed, &error), "process version parses");
    ok &= check(parsed.identifier() == QStringLiteral("P12.1.3.0"), "process version formats canonically");
    ProcessVersion roundTrip;
    ok &= check(ProcessVersion::parse(parsed.identifier(), &roundTrip) && roundTrip == parsed,
                 "process version format/parse round trip");
    ok &= check(!ProcessVersion::parse(QStringLiteral("P12.0.1.0"), &roundTrip, &error),
                 "zero loop is rejected");
    ok &= check(!ProcessVersion::parse(QStringLiteral("P12.1.3.2"), &roundTrip, &error),
                 "non-binary done flag is rejected");

    ProcessVersionState authoritative;
    authoritative.completedHistory = {{0, 1, 1, 1}, {1, 1, 1, 1}};
    authoritative.hasNextProcess = true;
    authoritative.nextProcess = {2, 1, 0, 0};
    ok &= check(authoritative.isValid(&error), "authoritative process state is valid");
    ok &= check(authoritative.completedIdentifiers() == QStringList{QStringLiteral("P0.1.1.1"), QStringLiteral("P1.1.1.1")}
                    && authoritative.nextIdentifier() == QStringLiteral("P2.1.0.0"),
                "authoritative P0/P1 history and P2 next state");
    ProcessVersionState restored;
    ok &= check(processVersionStateFromJson(processVersionStateToJson(authoritative), &restored, &error)
                    && sameState(authoritative, restored),
                "structured process state round trips");

    ProcessVersionState lifecycle;
    lifecycle.hasNextProcess = true;
    lifecycle.nextProcess = {12, 1, 0, 0};
    ok &= check(ProcessVersionLifecycle::startNextProcess(&lifecycle, &error)
                    && lifecycle.activeIdentifier() == QStringLiteral("P12.1.1.0")
                    && lifecycle.nextIdentifier() == QStringLiteral("P13.1.0.0"),
                "explicit process start creates first iteration");
    ok &= check(ProcessVersionLifecycle::advanceIteration(&lifecycle, &error)
                    && lifecycle.activeIdentifier() == QStringLiteral("P12.1.2.0"),
                "explicit iteration advance is legal while open");
    ok &= check(ProcessVersionLifecycle::completeActiveProcess(&lifecycle, &error)
                    && lifecycle.completedIdentifiers().last() == QStringLiteral("P12.1.2.1"),
                "explicit completion closes the active process loop");
    ok &= check(!ProcessVersionLifecycle::advanceIteration(&lifecycle, &error),
                "closed process loop cannot be advanced");
    ok &= check(!ProcessVersionLifecycle::completeActiveProcess(&lifecycle, &error),
                "closed process loop cannot be completed again");

    QTemporaryDir temporary;
    ok &= check(temporary.isValid(), "temporary persistence directory is available");
    if (temporary.isValid()) {
        ProjectPersistence persistence;
        ProjectModel source;
        auto projectJson = persistence.toJson(source);
        projectJson.insert(QStringLiteral("processVersion"), processVersionStateToJson(authoritative));
        ProjectModel loaded;
        ok &= check(persistence.fromJson(&loaded, projectJson, &error)
                        && sameState(loaded.processVersionState(), authoritative),
                    "authoritative process state loads into ProjectModel");
        const QString path = QDir(temporary.path()).filePath(QStringLiteral("process.aramf.json"));
        ok &= check(persistence.save(loaded, path, &error), "process state saves through ProjectPersistence");
        ProjectModel reloaded;
        ok &= check(persistence.load(&reloaded, path, &error)
                        && sameState(reloaded.processVersionState(), authoritative),
                    "process state survives disk save/reload");

        auto oldProject = persistence.toJson(source);
        oldProject.remove(QStringLiteral("processVersion"));
        ProjectModel oldLoaded;
        ok &= check(persistence.fromJson(&oldLoaded, oldProject, &error)
                        && oldLoaded.processVersionState().completedHistory.isEmpty()
                        && !oldLoaded.processVersionState().hasNextProcess,
                    "projects without process-version data load without fabricated history");

        auto malformed = persistence.toJson(source);
        malformed.insert(QStringLiteral("processVersion"), QJsonObject{
            {QStringLiteral("completedHistory"), QJsonArray{}},
            {QStringLiteral("active"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("next"), QJsonObject{{QStringLiteral("process"), 2}, {QStringLiteral("loop"), 1},
                                                  {QStringLiteral("iteration"), 0}, {QStringLiteral("done"), 2}}}});
        ProjectModel malformedLoaded;
        ok &= check(!persistence.fromJson(&malformedLoaded, malformed, &error),
                    "malformed process-version data is rejected");
    }
    return ok;
}
