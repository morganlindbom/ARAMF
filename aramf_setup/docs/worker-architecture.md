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

## P0 task execution governance

`WorkerTaskServices` derives schema-v1 task contracts; it does not replace
Worker schema v1, ProjectModel, WorkerContextResolver, ValidationRouting,
ProjectMemory, or CertificationService. The production headless commands are:

```
aramf task prepare --config <saved-project> --request <request.json>
aramf task postflight --config <saved-project> --contract <prepared.json> --evidence <evidence.json>
```

Prepare emits `{ "contract": ... }`. Requests contain `goal`, `type`, `scopes`,
exact project-relative `files`, `definitionOfDone`, and optional `history` and
`destructive` booleans. Intent never grants permission. The canonical saved
`rules.scopeMetadata` must map each active scope to string sets:

```
"thesis": {
  "files": ["docs/thesis.md"],
  "affects": [],
  "tests": ["thesis-focused-tests"],
  "riskTraits": [],
  "generatedArtifacts": []
}
```

`affects` is directional change propagation, resolved through the existing
scope routes, with cycle-safe traversal. Direct, transitive, potential, and
unrelated scope/file sets remain separate. Shared files cannot be edited
until all owning scopes are resolved. Resources use their canonical scopes;
unscoped resources are global. Thesis/Report instruction IDs are checked
against DocumentInstructions. Legacy unscoped durable decisions remain global;
P0 does not guess decision applicability from prose.

Metadata sets are normalized and persisted deterministically. Older projects
load with empty metadata (safe, not implicit write permission). New Project
and template replacement clear old task metadata. Workers with legacy routes
remain readable by the resolver, but must be regenerated from the saved model
before task execution. Contract hashes bind normalized content, project ID,
root, Worker suffix, configuration, canonical sources, and the observed baseline.

### Permissions and preflight

Ownership comes from the manifest, memory contract, canonical resource registry,
and service boundaries. Only exact mapped source files selected by the request
receive MODIFY; new sources also require canonical `create-files` permission.
Derived artifacts receive REGENERATE only through their owning service.
External resources, Sources of Truth, custom files and durable decisions remain
protected. History is APPEND_ONLY/NEVER_REWRITE through its canonical recorder.
Deletion requires a separate governed maintenance workflow and is denied here.

Preflight returns READY, READY_WITH_WARNINGS, BLOCKED, or FATAL. It checks
canonical generation content, read-only verification, scope/test mappings,
permissions, sources, authority conflicts, parallel state and repository safety.
Unknown mappings/traits and unavailable authoritative sources block; unsafe
paths, Git-root mismatches and unmerged index entries are fatal. Non-Git
projects receive a warning and filesystem checks. Remote resources need verified
local availability; P0 does not fetch them or invent their contents.

Negative constraints prohibit parallel state, out-of-scope changes, authority
overrides, external overwrite, history rewrite, full-Worker fallback, and
validation weakening. File/scope/ownership constraints are machine checked;
behavioral requirements such as preserving public behavior still require genuine
test and review evidence, not lexical guessing about source changes.

### Risk and validation

Risk takes the strongest applicable rule. LOW is isolated mapped work. MEDIUM
is multiple scopes or an existing subsystem validation route. HIGH includes
memory, persistence, hardware, topology, three or more scopes, or an existing
full-regression route. CRITICAL includes governance, schema, migration,
cross-project impact or destructive intent (the latter remains blocked).
Reasons are emitted. `runtime` and `ui` traits add launch evidence.

`ValidationRouting::taskPlan` extends its existing route with mapped focused
tests and acceptance/diff/impacted-validation evidence. MEDIUM/HIGH add workflow
and targeted regression; HIGH/CRITICAL require CTest and topology. CRITICAL uses
the existing full-regression policy, including memory, cold start, launch and
historical automated campaigns. Persistence/migration/hardware traits add their
specific checks. Global pre-commit policy is never reduced by iterative routing.

### Baseline, regeneration and postflight

The baseline fingerprints actual tracked/untracked non-ignored files, the Worker,
explicit mapped dependencies even when ignored, and local external resources.
Unchanged pre-existing dirt is not attributed to the task. New edits to dirty
files are still checked. Ignored build products are not blanket-scanned.
Fingerprint scanning is not a mandatory agent context read set; event history
remains excluded from ordinary context and `history: true` opts it in explicitly.

Postflight checks the contract hash and rederives permission, impact, risk and
evidence requirements from canonical state. It observes actual file bytes,
not a caller-provided changed-file list. Declared generated outputs are accepted
only when ownership permits them and their content equals the current canonical
producer: GenerationServices for project/manifest/routes, VerificationServices
for summary/evidence. Ownership alone, arbitrary content and unrelated generated
changes fail. Recorder changes require append-prefix and existing memory
consistency checks. Certification state is compared with CertificationService's
own derivation; historical append integrity remains independent of freshness.

Diagnostics contain stable codes, human messages, paths and severity. Distinct
families cover invalid/tampered contracts, unknown ownership, scope/permission
violations, unavailable routes/sources, authority conflicts, stale canonical
state, unexpected regeneration, GENERATED_CONTENT_MISMATCH, STALE_EVIDENCE,
EVIDENCE_INTEGRITY_INVALID, missing evidence, validation failure and certification.

### Current evidence and completion

An evidence index contains `evidence: [{check, artifact, artifactFingerprint}]`.
Artifacts are local JSON records with `check`, `status`, `contractId` and
`dependencyFingerprint`. Postflight exposes `evidenceFingerprints` for producers
to capture **before** executing each check; producers must confirm dependencies
did not change during execution. A prior result-wide `resultFingerprint` remains
supported conservatively. SHA-256 binds file content and dependency sets.
Mapped focused tests depend on their owning scopes and resources; shared build,
workflow, launch and validation checks cover the task dependency union. A Report
edit stales Report/shared checks without staling independent Thesis tests.
Mapped names cannot narrow built-in shared checks. Accepted/missing evidence and
observed modifications are returned as structured postflight evidence.

Derived task/evidence envelopes may be retained under `verification/tasks/`;
these are not a second memory store. Recognized envelopes are excluded from
their own baseline to avoid circular checksums, while reserved canonical filenames
remain subject to parallel-state detection. Keep the original prepared contract
in the trusted caller/review workflow. These APIs validate producer attestations
and detect accidental/unauthorized drift; hashes are not signatures, an OS
sandbox, or proof that a caller really ran a command. A caller able to replace
both trusted inputs and all evidence is outside this integrity threat model.

Prepare starts at NOT_STARTED. Postflight returns IN_PROGRESS when no changes
and incomplete evidence exist, IMPLEMENTED_UNVERIFIED for changed but insufficiently
verified work, BLOCKED for safety violations, and VERIFIED only for complete
current software evidence. Software VERIFIED does not require a physical test.
CERTIFIED additionally requires a current PASS certificate from CertificationService
for the contract ID and result fingerprint, with intact hashed evidence references.
Hardware tasks require HARDWARE_CERTIFIED plus physical/on-target evidence;
arbitrary test JSON cannot supply that certification. Stale physical evidence
removes CERTIFIED without invalidating independent current software verification.
The command returns zero only for READY preparation or VERIFIED/CERTIFIED
postflight; invalid arguments, blocked preparation and unverified completion
return 2. Commands do not run tests, modify source, commit or push.

P0 regression is included in `aramf_core_tests`; `--worker-tasks` runs its focused
matrix independently. Certificate fixtures test the software gate with synthetic
evidence and do not claim that physical hardware was tested. Context indexing,
compression, task DAGs, handoffs, adapters and agent-effectiveness evaluation
remain outside P0.
