#include "core/CertificationFreshness.h"
#include "core/CertificationRevalidation.h"

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

    auto contractProject = QJsonObject{{"structure", QJsonObject{
        {"s1ResponsibilityOwnership", QJsonObject{{"rootId", "root"}}},
        {"s2PhysicalStructure", QJsonObject{{"boundaries", QJsonArray{QJsonObject{{"rootRelativePath", "src"}}}}}},
        {"s3DependencyInterfaces", QJsonObject{{"dependencies", QJsonArray{QJsonObject{{"metadata", QJsonObject{{"allowSelf", false}}}}}}}},
        {"s4CompositionEncapsulation", QJsonObject{{"compositionContracts", QJsonArray{QJsonObject{{"metadata", QJsonObject{{"multiplicityReason", "one"}}}}}}}},
        {"s5DecompositionModularity", QJsonObject{{"assessments", QJsonArray{QJsonObject{{"metadata", QJsonObject{{"unrelatedConcerns", false}}}}}}}},
        {"s6StructuralEvolutionEnforcement", QJsonObject{{"enforcementPolicies", QJsonArray{QJsonObject{{"mode", "AUDIT"}}}}}}
    }}};
    QString s1Fingerprint;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S1", contractProject, &s1Fingerprint), "S1 semantic projection available");
    auto contractStructure = contractProject.value("structure").toObject();
    auto s1Semantic = contractStructure.value("s1ResponsibilityOwnership").toObject();
    s1Semantic["rootId"] = "different-root";
    contractStructure["s1ResponsibilityOwnership"] = s1Semantic;
    contractProject["structure"] = contractStructure;
    QString changedS1Fingerprint;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S1", contractProject, &changedS1Fingerprint)
                    && changedS1Fingerprint != s1Fingerprint, "S1 semantic change changes contract fingerprint");

    QString s5Fingerprint;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S5", contractProject, &s5Fingerprint), "S5 semantic projection available");
    auto s5Config = contractStructure.value("s5DecompositionModularity").toObject();
    auto assessments = s5Config.value("assessments").toArray();
    auto assessment = assessments.first().toObject();
    assessment["metadata"] = QJsonObject{{"unrelatedConcerns", true}};
    assessments[0] = assessment;
    s5Config["assessments"] = assessments;
    contractStructure["s5DecompositionModularity"] = s5Config;
    contractProject["structure"] = contractStructure;
    QString changedS5Fingerprint;
    ok &= check(CertificationContractManifestProvider::fingerprintFromProjectJson("S5", contractProject, &changedS5Fingerprint)
                    && changedS5Fingerprint != s5Fingerprint, "S5 audit metadata changes contract fingerprint");

    const auto semanticFingerprint = [&](const QString& subject, const QJsonObject& value) {
        QString fingerprint;
        return CertificationContractManifestProvider::fingerprintFromProjectJson(subject, value, &fingerprint) ? fingerprint : QString{};
    };
    const QString s2Before = semanticFingerprint("S2", contractProject);
    auto s2Config = contractStructure.value("s2PhysicalStructure").toObject();
    auto boundaries = s2Config.value("boundaries").toArray();
    auto boundary = boundaries.first().toObject(); boundary["rootRelativePath"] = "tests"; boundaries[0] = boundary;
    s2Config["boundaries"] = boundaries; contractStructure["s2PhysicalStructure"] = s2Config; contractProject["structure"] = contractStructure;
    ok &= check(!s2Before.isEmpty() && semanticFingerprint("S2", contractProject) != s2Before, "S2 semantic change changes contract fingerprint");

    contractStructure = contractProject.value("structure").toObject();
    const QString s3Before = semanticFingerprint("S3", contractProject);
    auto s3Config = contractStructure.value("s3DependencyInterfaces").toObject();
    auto dependencies = s3Config.value("dependencies").toArray(); auto dependency = dependencies.first().toObject();
    dependency["metadata"] = QJsonObject{{"allowSelf", true}}; dependencies[0] = dependency;
    s3Config["dependencies"] = dependencies; contractStructure["s3DependencyInterfaces"] = s3Config; contractProject["structure"] = contractStructure;
    ok &= check(!s3Before.isEmpty() && semanticFingerprint("S3", contractProject) != s3Before, "S3 semantic metadata changes contract fingerprint");

    contractStructure = contractProject.value("structure").toObject();
    const QString s4Before = semanticFingerprint("S4", contractProject);
    auto s4Config = contractStructure.value("s4CompositionEncapsulation").toObject(); auto contracts = s4Config.value("compositionContracts").toArray();
    auto compositionContract = contracts.first().toObject(); compositionContract["metadata"] = QJsonObject{{"multiplicityReason", "many"}}; contracts[0] = compositionContract;
    s4Config["compositionContracts"] = contracts; contractStructure["s4CompositionEncapsulation"] = s4Config; contractProject["structure"] = contractStructure;
    ok &= check(!s4Before.isEmpty() && semanticFingerprint("S4", contractProject) != s4Before, "S4 semantic metadata changes contract fingerprint");

    contractStructure = contractProject.value("structure").toObject();
    const QString s6Before = semanticFingerprint("S6", contractProject);
    auto s6Config = contractStructure.value("s6StructuralEvolutionEnforcement").toObject(); auto policies = s6Config.value("enforcementPolicies").toArray();
    auto policy = policies.first().toObject(); policy["mode"] = "ENFORCE"; policies[0] = policy;
    s6Config["enforcementPolicies"] = policies; contractStructure["s6StructuralEvolutionEnforcement"] = s6Config; contractProject["structure"] = contractStructure;
    ok &= check(!s6Before.isEmpty() && semanticFingerprint("S6", contractProject) != s6Before, "S6 policy change changes contract fingerprint");

    auto dependencyManifest = CertificationFreshnessService::dependencyManifest("S6");
    const QString originalManifestFingerprint = dependencyManifest.computedFingerprint();
    dependencyManifest.directDependencies.removeLast();
    ok &= check(originalManifestFingerprint != dependencyManifest.computedFingerprint(), "dependency graph changes manifest fingerprint");

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
        QJsonObject currentRevalidation;
        const bool hasCurrentRevalidation = CertificationRevalidationService::latest(QDir::currentPath(), QStringLiteral("S6"), &currentRevalidation);
        ok &= check(hasCurrentRevalidation
                        ? liveS6.status == CertificationFreshnessStatus::Fresh
                        : liveS6.status == CertificationFreshnessStatus::DependencyBindingIncomplete,
                    "S6 distinguishes historical incomplete binding from current revalidated freshness");
    }
    return ok ? 0 : 1;
}
