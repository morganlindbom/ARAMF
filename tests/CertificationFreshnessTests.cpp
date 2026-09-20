#include "core/CertificationFreshness.h"

#include <QCoreApplication>
#include <QDir>
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
    ok &= check(p5.directDependencies.size() == 4 && p5.directDependencies.last().subject == "P4", "P5 binds the prior process chain");
    ok &= check(s6.manifestFingerprint == s6.computedFingerprint(), "dependency manifest fingerprint is deterministic");

    QJsonObject historical{{"certificateId", "cert-test"}, {"sourceRevision", "rev"},
                           {"sourceFingerprint", "source"}, {"evidenceFingerprint", "evidence"},
                           {"s1ContractFingerprint", "contract"}};
    auto fresh = CertificationFreshnessService::evaluateSnapshot("S1", historical, "rev", "source", "contract", "evidence");
    ok &= check(fresh.status == CertificationFreshnessStatus::Fresh, "complete root snapshot is fresh");

    auto staleDependency = fresh;
    staleDependency.status = CertificationFreshnessStatus::DependencyStale;
    QHash<QString, CertificationFreshnessResult> direct;
    direct.insert("S1", staleDependency);
    auto consumer = CertificationFreshnessService::evaluateSnapshot("S2", historical, "rev", "source", "contract", "evidence", direct);
    ok &= check(consumer.status == CertificationFreshnessStatus::DependencyStale, "direct dependency stale is distinct");

    auto transitiveDependency = staleDependency;
    transitiveDependency.status = CertificationFreshnessStatus::TransitiveDependencyStale;
    direct["S1"] = transitiveDependency;
    consumer = CertificationFreshnessService::evaluateSnapshot("S2", historical, "rev", "source", "contract", "evidence", direct);
    ok &= check(consumer.status == CertificationFreshnessStatus::TransitiveDependencyStale, "transitive dependency stale is distinct");

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
