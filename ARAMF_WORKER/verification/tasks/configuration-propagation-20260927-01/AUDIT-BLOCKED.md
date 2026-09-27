# Phase 2B: configuration propagation audit / blocked PREPARE

Starting and current HEAD: `0d9aa5950255fc101eb22b42b271a98bac98b529`.
Actor: assistant, following the explicit Phase 2B user request.
This is an additive audit record, not implementation or certification evidence.
No previous task evidence was edited. No Phase 2B product edits were made.

## Canonical preflight

Command:
`build/aramf.exe task prepare --config ARAMF_WORKER.aramf.json --request build/phase2b-request.json`

The first request incorrectly used scope `ui`. The canonical scope is `ui-ux`.
The request was corrected, not the routing rules. The second PREPARE still
returned `BLOCKED`, contract ID:
`1c3753f7b69694439a53bcf49929d7eb2173a085377d010d2431bab5a31ea06e`.

Both `FILE_OWNERSHIP_UNKNOWN` and `TASK_SCOPE_VIOLATION` apply to:

- `src/core/ConfigurationUpdateService.h`
- `src/core/Services.h`
- `src/ui/workflows/update/configuration/UpdateConfigurationPage.cpp`

Permitted files were only ConfigurationUpdateService.cpp, Services.cpp,
TemplateManager.cpp, ConfigurationUpdateTests.cpp and TemplateConfigurationTests.cpp.
WorkerTaskServices.cpp checks canonical scopeMetadata and refuses unmapped files.
The UI file is relevant because it saves the new root configuration before the
update service runs; leaving that call outside the transaction cannot meet the
requested failure invariant. Service headers were proposed for the coordinated
update route and explicit result-stage contract. No permissions were fabricated.

Required next boundary: register the exact Phase 2B files in canonical scope
metadata through the approved configuration/governance producer, preserve all
pre-existing changes, refresh only affected derived governance state, and obtain
a READY contract before implementation. Do not broaden ownership to directories.

## Existing four-file change classification

- ConfigurationUpdateService.cpp: relevant named-worker scope restoration,
  exact suffix-based paths, baseline UUID/root checks and manifest identity
  checks. Final generic verification is relevant but insufficient to prove
  semantic propagation. The existing apply route still lacks an enclosing
  persistence/generation transaction. Retain for review; not ready to commit.
- TemplateManager.cpp: relevant preservation of workerNameSuffix during
  template application and exclusion from reusable saved templates. These
  implement the explicit instance/template distinction; retain for Phase 2B.
- ConfigurationUpdateTests.cpp: relevant production generation, name isolation,
  stale-plan/removal guards and subprocess reload. Does not cover complete
  Qt/resource/tool propagation or partial-failure transactional restoration.
- TemplateConfigurationTests.cpp: relevant instance identity preservation and
  exclusion from reusable templates. Retain for Phase 2B.

These hunks fit Phase 2B; no unrelated hunk was identified in the four-file diff.
None was discarded, edited, staged or committed by this audit. Existing prior
named-worker evidence remains historical, not proof that Phase 2B is complete.

## Propagation findings

Authoritative configuration is ProjectModel, persisted by ProjectPersistence.
Project UUID/root, logical project name and worker suffix remain distinct.
The update service's canonicalState uses the persistence serializer, excluding
storage paths and workflow progress. That baseline is a derived comparison,
not authority to assert that every output actually matches.

| Domain/output | Canonical producer | Current update path |
| --- | --- | --- |
| Persisted root model | ProjectPersistence | UI saves before apply; apply itself does not own save/reload |
| Worker project.json and manifest | GenerationServices | repairDerivedArtifacts rewrites these |
| Task/scope routing | WorkerContextResolver via GenerationServices | repair rewrites these |
| Rules and configuration-dependent AGENTS text | GenerationServices::generate | repair only upserts the P1 context section |
| Platform/environment/tools/frameworks | GenerationServices::generate | repair omits product regeneration |
| Resource list and loading strategy | GenerationServices::generate | repair omits product regeneration |
| Selection effects and provenance | GenerationServices::generate | repair omits product regeneration |
| Documentation manifest | GenerationServices::generate | repair omits product regeneration |
| Context index/compression/freshness | ContextCoordinationService | repair regenerates after generic verification |
| Memory derived state | ProjectMemory | repair refreshes current derived memory |
| Verification/current generation baseline | VerificationServices / update service | baseline can describe new model while products remain old |

Services.h explicitly limits repairDerivedArtifacts to topology/validation
repair. Calling it as full configuration synchronization is the central defect.
Generic verification checks JSON validity/existence and the model fingerprint;
these checks do not establish semantic equality for all configuration products.
Calling generate directly is not a safe substitute: it rejects an existing
named-worker directory and performs broader initialization. Canonical producers
must be reused through a deliberate update path, not duplicated or bypassed.

The previously captured isolated reproduction is preserved at
`../named-worker-update/configuration-propagation-observation.json`:
reported SUCCESS with expected frameworks [qt] versus actual [], expected
qt-creator versus visual-studio-code, expected resources 3 versus actual 0,
and rulesUpdated=false. This audit did not rerun or relabel that historical test.

Phase 2A's snapshot helpers are local to ProjectMemory operations; they do not
currently enclose the UI save plus configuration generation. No changes to
those verified helpers or their historical evidence were made.

## Results and boundary

- Phase 2B implementation, new regression fixtures, clean build, CTest and both
  campaigns: NOT RUN; blocked before EXECUTE. No new Qt/C++/CMake/Ninja/resource,
  rules/context/routing or independent-reload PASS is claimed.
- Phase 2B memory/cold-start/worker/postflight validation: NOT RUN.
- Read-only `build/rollback-closeout.exe freshness`: F1-F4, P1-P5 and S1-S6
  all FRESH. No revalidation or certificate modification performed.
- `git diff --check` and `git diff --cached --check`: PASS. Git emits existing
  LF/CRLF checkout warnings for the prior dirty configuration/four source files.
- Index empty; no commit; HEAD unchanged.
- Read-only target enumeration: HVD Components exists with zero entries.
  No target creation/configuration or incident investigation performed.
- LICENSE and test_250/runs and test_550/runs were not modified or staged.
- Phase 2A historical evidence unchanged by this task.
- No F/P/S/C migration, P6, C1, component implementation or HVD integration.

The only additions by this audit are this report and the ignored build request
`build/phase2b-request.json`. Existing tracked/untracked work is preserved.
