#include "structure/S1/ResponsibilityOwnership.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <iostream>

using namespace S1;
static int failures = 0;
static void check(bool condition, const char* message) { if (!condition) { ++failures; std::cerr << "S1 FAIL: " << message << '\n'; } }

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Configuration c;
    c.rootId = "project";
    c.responsibilities = {
        {"project", "project", "Project", ResponsibilityKind::Project, {}, {}, {"report"}, {}, {}},
        {"report", "report", "Report", ResponsibilityKind::Document, {}, "project", {}, {}, {}}
    };
    c.artifacts = {
        {"report-doc", "docs/report.md", "document", "report", OwnershipMode::Exclusive},
        {"report-image", "docs/report/image.png", "image", "report", OwnershipMode::Exclusive},
        {"shared-test", "tests/report_test.cpp", "test", "report", OwnershipMode::SharedExplicit, {"report", "project"}, "cross-responsibility evidence"}
    };
    check(audit(c).valid, "valid recursive ownership model audits cleanly");
    const auto json = toJson(c);
    Configuration loaded; QString error;
    check(fromJson(json, &loaded, &error), "ownership model loads");
    check(QJsonDocument(toJson(loaded)).toJson(QJsonDocument::Compact) == QJsonDocument(json).toJson(QJsonDocument::Compact), "ownership model serialization is deterministic");

    auto invalid = c; invalid.artifacts[0].primaryOwnerId.clear();
    const auto missing = audit(invalid);
    check(!missing.valid && missing.diagnostics.first().ruleCode == "S1.BLOCKER.MISSING_PRIMARY_OWNER", "missing owner is diagnosed");
    invalid = c; invalid.responsibilities[1].parentId = "missing";
    check(!audit(invalid).valid, "invalid parent is diagnosed");
    invalid = c; invalid.responsibilities[0].parentId = "report";
    check(!audit(invalid).valid, "root cycle is diagnosed");
    invalid = c; invalid.artifacts[2].sharedReason.clear();
    check(!audit(invalid).valid, "shared ownership requires a reason");

    ProcessVersionState lifecycle = ProcessVersionState::empty();
    lifecycle.hasNextStructure = true;
    lifecycle.nextStructure = ProcessVersion(ProcessKind::Structure, 1, 1, 0, 0, 0);
    check(ProcessVersionLifecycle::startNextStructure(&lifecycle, &error), "S1 starts at S1.1.1.0.0");
    check(lifecycle.activeStructure.identifier() == "S1.1.1.0.0", "S1 first material iteration is correct");
    check(ProcessVersionLifecycle::certifyCurrentStructureIteration(&lifecycle, &error), "S1 certification transition succeeds");
    check(ProcessVersionLifecycle::completeActiveStructure(&lifecycle, &error), "S1 completion transition succeeds");
    check(lifecycle.structureHistory.first().identifier() == "S1.1.1.1.1", "S1 completion is five-part and certified");
    check(ProcessVersionLifecycle::reworkCompletedStructure(&lifecycle, 1, &error), "S1 rework opens a new iteration");
    check(lifecycle.activeStructure.identifier() == "S1.1.2.0.0", "S1 material change resets certification and done");

    std::cout << "S1 responsibility checks=" << (failures == 0 ? "PASS" : "FAIL") << '\n';
    return failures == 0 ? 0 : 1;
}
