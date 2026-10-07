// WorkerContextResolver.h
#pragma once

#include <QJsonObject>
#include <QStringList>

struct RuleConfiguration;
class ProjectModel;

// Bound resource observations to one synchronous service operation.

// Nested index, verification and task checks share one double-read observation;
// leaving the outermost scope discards every cached observation. No observation
// is reused by a later API operation or persisted as authority.
class WorkerResourceObservation final
{
public:
    WorkerResourceObservation();
    ~WorkerResourceObservation();
    WorkerResourceObservation(const WorkerResourceObservation&) = delete;
    WorkerResourceObservation& operator=(const WorkerResourceObservation&) = delete;
};

// Resolves the generated routing model. It is intentionally stateless: the
// Worker files remain the canonical routing source and no second route store
// is maintained in memory.
class WorkerContextResolver final
{
public:
    // Inspect a resource with a versioned content manifest.

    // Folder snapshots include all entries unless exact generated-file paths
    // are declared in canonical folderResourcePolicies. Exclusion never grants
    // write permission; task postflight still checks their canonical producers.
    // Schema: scopeMetadata.<owner>.folderResourcePolicies.<resourceId> is
    // {schemaVersion:1, generatedPaths:[exact worker-relative files]}. Only the
    // nine context/validation/generation producer outputs accepted here may
    // be marked generated. There are no globs, implicit ignore rules or general
    // directory exclusions. Snapshots expose resourceId, type, policy, entries,
    // fingerprint, valid and error. Entry paths are NFC, case-unambiguous and
    // root-relative; SHA-256 binds raw file bytes and compact manifest JSON.
    // A 100000-entry limit fails inspection rather than truncating evidence.
    // generation-state.json must be explicitly declared for self-host updates:
    // GenerationServices writes it after context collection. Its producer
    // readback and ordinary task protection remain mandatory and independent.
    static QJsonObject resourceSnapshot(const ProjectModel& model, const QString& resourceId);
    static QJsonObject resolve(const QString& workerRoot, QStringList scopes);
    // Directional change propagation is owned by the same canonical routes.
    static QJsonObject resolveImpact(const QString& workerRoot, QStringList scopes);
    static QJsonObject scopeRoutes(const RuleConfiguration& rules, const QString& fingerprint);
    static QJsonObject taskRoutes(const RuleConfiguration& rules, const QString& fingerprint);
};
