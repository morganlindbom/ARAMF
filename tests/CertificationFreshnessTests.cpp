#include "core/CertificationFreshness.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <iostream>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::cerr << "FAIL: " << message << '\n';
    return value;
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
    QString projectRoot = QDir(originalDirectory).absolutePath();
    if (QFileInfo(projectRoot).fileName().compare(QStringLiteral("build"), Qt::CaseInsensitive) == 0)
        projectRoot = QDir(projectRoot).absoluteFilePath("..");
    projectRoot = QDir(projectRoot).absolutePath();
    const bool changedDirectory = QDir::setCurrent(projectRoot);
    const bool rootedFingerprint = CertificationSourceManifestProvider::fingerprint(projectRoot, "S1", &rootFingerprint);
    if (changedDirectory) QDir::setCurrent(originalDirectory);
    ok &= check(rootedFingerprint && !rootFingerprint.isEmpty(), "source manifest uses supplied project root, not caller CWD");

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

    auto incomplete = CertificationFreshnessService::evaluateSnapshot("S6", historical, "rev", "source", "contract", "evidence");
    ok &= check(incomplete.status == CertificationFreshnessStatus::DependencyBindingIncomplete, "missing direct bindings are conservative");

    const auto manifestA = CertificationSourceManifestProvider::manifest("S6");
    const auto manifestB = CertificationSourceManifestProvider::manifest("S6");
    ok &= check(QJsonDocument(manifestA.toJson()).toJson(QJsonDocument::Compact)
                    == QJsonDocument(manifestB.toJson()).toJson(QJsonDocument::Compact), "source manifest serialization is deterministic");
    ok &= check(CertificationFreshnessService::dependencyManifest("P0").directDependencies.isEmpty(), "legacy P0 is not a current identifier");
    ok &= check(certificationFreshnessStatusName(CertificationFreshnessStatus::DependencyBindingIncomplete) == "DEPENDENCY_BINDING_INCOMPLETE", "status names are canonical");
    if (app.arguments().contains(QStringLiteral("--live-root"))) {
        QString error;
        const auto liveS6 = CertificationFreshnessService::evaluate(QStringLiteral("S6"), QDir::currentPath(), &error);
        ok &= check(error.isEmpty(), "live freshness evaluation is read-only and parseable");
        ok &= check(liveS6.status == CertificationFreshnessStatus::DependencyBindingIncomplete,
                    "historical S6 without upstream bindings remains binding-incomplete");
    }
    return ok ? 0 : 1;
}
