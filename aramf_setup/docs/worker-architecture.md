# Generated Worker Architecture

`ARAMF_WORKER/worker-manifest.json` is the single topology entry point for a
generated control plane. It declares schema version `1`, canonical owners,
derived outputs, the generated configuration fingerprint, and the mandatory
cold-start read set.

`ARAMF_WORKER/project.json` is the compact canonical project identity and
stable configuration snapshot. It does not contain event history, instruction
bodies, resources, or mutable agent notes.

The startup contract is deliberately ordered: project identity, topology and
ownership, current state, validation, then task-scope routing. The event log is
historical and excluded from ordinary startup. Scope routes carry deterministic
required/optional files and a `historyRequired` flag; merged scopes are a union
of those sets and must not load unrelated context.

Both files are generated from `ProjectModel` and the generation fingerprint.
They are derived and must not become independent sources of truth. Unknown
future worker schema versions must be rejected by topology validation rather
than silently interpreted as schema `1`.

## Routing and ownership

`WorkerContextResolver::resolve(workerRoot, scopes)` is the only context
resolver. It consumes the generated scope route index and resource manifest,
sorts and de-duplicates merged paths, returns validation references, and
reports unknown scopes or missing metadata as diagnostics. It never falls
back to reading the entire Worker. Thesis and Report routes carry their
canonical instruction IDs independently; resources are selected only when
enabled and scope-relevant, retaining role and authority for conflict review.

The manifest is the central file classification point. Canonical state remains
owned by project configuration, routing, Project Memory, decisions, resource
manifests, and validation. Status, cold-start validation, and generation state
are derived. Event logs and certificate histories remain append-only and are
deferred from normal startup.

Generation uses atomic writes and existing semantic idempotence helpers. Safe
self-healing is limited to generated topology/routing/validation outputs;
custom files, external resources, decisions, and history are never replaced.
Legacy migration remains non-destructive and preserves the authoritative
`ARAMF_WORKER/` path.

The resolver accepts the previous string-array scope route representation as a
read-only compatibility form and marks it with `legacyRouteFormat`; newly
generated Workers always use the versioned object representation.

## Validation, repair, and baselines

`verification/latest-validation.json` is a derived, canonical entry point for
current validation status. It summarizes topology, routing, cold start,
memory, reachability, stale count, and conflicts; detailed verification files
remain the evidence owners. The summary is checked for schema and fingerprint
integrity and is never trusted over the detailed validators.

`GenerationServices::repairDerivedArtifacts` may regenerate only project
configuration, routing/read-set metadata, the Worker manifest, and the latest
validation summary. Missing or corrupt event history, decisions, external
templates, source-of-truth resources, and custom files are reported as invalid
and are never fabricated or overwritten. Verification detects duplicate
canonical roots and ad-hoc status/memory stores while allowing files under
`custom/` and legitimate detailed evidence.

Generated JSON uses stable semantic input fingerprints and atomic delta writes;
unchanged bytes are not rewritten. Efficiency measurements use a deterministic
UTF-8 approximation of `ceil(bytes / 4)` tokens, labeled approximate rather
than exact tokenizer output. The current fixture benchmark baseline is 12/12
valid fixtures, resolver median approximately 199 microseconds, generation
median approximately 176 milliseconds, and aggregate fixture output of 384
files. Timing is environment-sensitive and future campaigns should compare
the same benchmark method.
