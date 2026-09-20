#include "structure/S2/PhysicalStructure.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>

namespace {
bool check(bool condition, const char* name)
{
    if (!condition) std::cerr << "FAIL [" << name << "]\n";
    else std::cout << "PASS [" << name << "]\n";
    return condition;
}

S1::Configuration ownershipModel()
{
    S1::Configuration ownership;
    ownership.rootId = QStringLiteral("project");
    ownership.responsibilities.append({QStringLiteral("project"), QStringLiteral("project"), QStringLiteral("Project"), S1::ResponsibilityKind::Project, {}, {}, {}, {}, {}});
    ownership.responsibilities.append({QStringLiteral("lab1"), QStringLiteral("lab1"), QStringLiteral("Lab 1 Report"), S1::ResponsibilityKind::Document, {}, QStringLiteral("project"), {}, {}, {}});
    ownership.artifacts.append({QStringLiteral("report-image"), QStringLiteral("Reports/Lab1/Images/correlation.png"), QStringLiteral("image"), QStringLiteral("lab1"), S1::OwnershipMode::Exclusive, {}, {}, false, {}, {}, {}, {}, {}});
    return ownership;
}

S2::Configuration validModel()
{
    S2::Configuration configuration;
    configuration.rootId = QStringLiteral("physical-root");
    configuration.nodes.append({QStringLiteral("physical-root"), QStringLiteral("physical-root"), QStringLiteral("Project"), {}, {QStringLiteral("lab1-boundary")}, QStringLiteral("project"), {}, QStringLiteral("root"), {}});
    configuration.nodes.append({QStringLiteral("lab1-boundary"), QStringLiteral("lab1-boundary"), QStringLiteral("Lab 1"), QStringLiteral("physical-root"), {}, QStringLiteral("lab1"), QStringLiteral("Reports/Lab1"), QStringLiteral("document"), {}});
    configuration.boundaries.append({QStringLiteral("lab1-boundary"), QStringLiteral("lab1"), QStringLiteral("Reports/Lab1"), QStringLiteral("EXCLUSIVE"), {}, {}, {QStringLiteral("Images"), QStringLiteral("References"), QStringLiteral("Data")}, {}, false, {}});
    configuration.placements.append({QStringLiteral("report-image"), QStringLiteral("lab1"), QStringLiteral("lab1-boundary"), QStringLiteral("Reports/Lab1/Images/correlation.png"), S2::PlacementMode::Exact, QStringLiteral("image"), false, {}, {}});
    return configuration;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool all = true;
    const auto ownership = ownershipModel();
    auto configuration = validModel();
    auto result = S2::audit(configuration, ownership);
    all &= check(result.valid, "valid recursive physical structure audits");
    all &= check(S2::toJson(configuration) == S2::toJson(configuration), "deterministic S2 serialization");
    S2::Configuration roundTrip; QString error;
    all &= check(S2::fromJson(S2::toJson(configuration), &roundTrip, &error) && S2::toJson(roundTrip) == S2::toJson(configuration), "S2 persistence round trip");
    all &= check(configuration.lifecycle.identifier() == QStringLiteral("S2.1.0.0.0"), "default S2 lifecycle");
    ProcessVersionState state = ProcessVersionState::empty(); state.hasNextStructure = true; state.nextStructure = configuration.lifecycle;
    all &= check(ProcessVersionLifecycle::startNextStructure(&state, &error) && state.activeStructure.identifier() == QStringLiteral("S2.1.1.0.0"), "S2 lifecycle start");
    all &= check(ProcessVersionLifecycle::certifyCurrentStructureIteration(&state, &error) && state.activeStructure.identifier() == QStringLiteral("S2.1.1.1.0"), "S2 lifecycle certification");
    all &= check(ProcessVersionLifecycle::completeActiveStructure(&state, &error) && state.structureHistory.last().identifier() == QStringLiteral("S2.1.1.1.1"), "S2 lifecycle completion");
    auto invalid = configuration; invalid.nodes[1].relativePath = QStringLiteral("../escape"); result = S2::audit(invalid, ownership); all &= check(!result.valid && result.diagnostics.first().ruleCode == QStringLiteral("S2.BLOCKER.PATH_TRAVERSAL"), "path traversal rejected");
    invalid = configuration; invalid.placements[0].expectedRelativePath = QStringLiteral("Reports/Lab1/../outside.txt"); result = S2::audit(invalid, ownership); all &= check(!result.valid, "normalized placement escape rejected");
    invalid = configuration; invalid.placements[0].responsibilityId = QStringLiteral("other"); result = S2::audit(invalid, ownership); all &= check(!result.valid, "S1 ownership boundary mismatch rejected");
    invalid = configuration; invalid.boundaries.append(configuration.boundaries.first()); result = S2::audit(invalid, ownership); all &= check(!result.valid, "duplicate and overlapping boundaries rejected");
    invalid = configuration; invalid.placements[0].placementMode = S2::PlacementMode::Generated; invalid.placements[0].canonicalProducer.clear(); result = S2::audit(invalid, ownership); all &= check(!result.valid, "generated producer required");
    invalid = configuration; invalid.placements[0].placementMode = S2::PlacementMode::Mirrored; result = S2::audit(invalid, ownership); all &= check(result.valid, "mirrored placement supported");
    invalid = configuration; invalid.placements[0].placementMode = S2::PlacementMode::External; invalid.placements[0].expectedBoundaryId.clear(); result = S2::audit(invalid, ownership); all &= check(result.valid, "external placement supported");
    invalid = configuration; invalid.nodes[1].parentId = QStringLiteral("missing"); result = S2::audit(invalid, ownership); all &= check(!result.valid, "parent child reciprocity is enforced");
    invalid = configuration; invalid.nodes[0].childIds.append(QStringLiteral("missing")); result = S2::audit(invalid, ownership); all &= check(!result.valid, "unknown child reference is rejected");
    QTemporaryDir fixture; const QString marker = fixture.filePath(QStringLiteral("user.txt")); QFile file(marker); file.open(QIODevice::WriteOnly); file.write("unchanged"); file.close(); const QByteArray before = [&] { QFile read(marker); read.open(QIODevice::ReadOnly); return read.readAll(); }(); S2::audit(configuration, ownership, fixture.path()); QFile read(marker); read.open(QIODevice::ReadOnly); all &= check(read.readAll() == before, "audit does not modify user files");
    std::cout << (all ? "S2 tests PASS\n" : "S2 tests FAIL\n");
    return all ? 0 : 1;
}
