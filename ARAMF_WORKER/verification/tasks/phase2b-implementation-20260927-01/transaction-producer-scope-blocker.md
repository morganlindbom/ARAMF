# Phase 2B: incomplete producer transaction boundary — STOP

Recorded 2026-09-27, following read-only dependency inspection and the first
incremental configuration-update build/test. Actor: coding assistant.

HEAD: `0d9aa5950255fc101eb22b42b271a98bac98b529` (unchanged).
Amended PREPARE contract:
`979ecbe47cc6472c77bcb78d10d45d4a4713efcaf134d095cfcb625325fd519b`, READY.
See `amended-ready-contract.json`; the previous READY contract remains intact.

## Scope blocker

The in-progress GenerationServices configuration regeneration reuses the
canonical producers rather than reproducing their serialization. However,
`ProjectMemory::initializeMemory()` is not an existing-worker-only producer:

1. `src/core/ProjectMemory.cpp:543` calls `prepareControlPlane()`.
2. `src/core/ControlPlaneMigration.cpp:61` discovers a legacy `ARAMF/` tree,
   copies missing files into the selected canonical worker, and writes a
   migration report. The copied set is not limited to configuration outputs.
3. `ProjectMemory.cpp:560` calls `FrameworkKnowledgeService::seedProject()`.
4. `FrameworkKnowledge.cpp:899` seeds project knowledge and calls
   `approvedGlobalEntries()` -> `globalEntries()` -> `ensureGlobalLibrary()`.
5. `FrameworkKnowledge.cpp:757` may create/migrate the program-local knowledge
   library and its migration marker. Those are not project-local update targets.

These are conditional write paths proven by code inspection, not a claim that
they ran or caused data loss in this turn. The initial successful test fixture
does not exercise these conditions.

The current proposed snapshot list therefore cannot yet guarantee complete
rollback for all supported existing projects. Merely hashing emitted products
afterward would not repair the missing pre-mutation transaction coverage.
Snapshotting/migrating arbitrary legacy and global content during a project
configuration update would also broaden its scope unnecessarily.

`ProjectMemory::writeMemoryFiles()` is the canonical model-to-memory
configuration/contract producer, but is private (`ProjectMemory.h:131`). The
public `refreshDerivedState()` and `refreshMemoryContract()` do not accept the
proposed ProjectModel or persist its new memory configuration. Reimplementing
this serializer in Services.cpp would duplicate the canonical producer.

Requested additional authorization, NOT exercised:

- `src/core/ProjectMemory.h`
- `src/core/ProjectMemory.cpp`

Narrow purpose: expose an existing-worker configuration update route using the
existing canonical memory producers, without bootstrap/migration/global
knowledge seeding or new activation events, with an explicit project-local
mutation boundary. No rollback repair history, general migration behavior, or
FrameworkKnowledge implementation would be changed. Extend direct regression
coverage only in the already-authorized ConfigurationUpdateTests.cpp.

Implementation has stopped pending authorization. The partial changes remain
uncommitted and MUST NOT be treated as a verified configuration update release.

## Actual validation so far

- `cmake --build build --target aramf_configuration_update_tests --parallel 4`:
  PASS, exit 0 (incremental build only; not a clean/full build).
- `build/aramf_configuration_update_tests.exe`: PASS, exit 0. Output:
  `named_worker_updates: PASS (production generation, update, independent reload, isolation, missing/foreign worker, stale plan and removal guards)`
  and `configuration_update_tests: PASS (recursive ADD/MODIFY/REMOVE)`.
- No new propagation/transaction regression matrix has been completed yet.
- Full CTest, campaigns, current-state regeneration, final postflight and
  revalidation: NOT RUN for this incomplete implementation.
- `git diff --check`: PASS, exit 0; Git printed LF/CRLF checkout warnings.
- `git diff --cached --check`: PASS, exit 0; index empty.

## Read-only freshness

`build/rollback-closeout.exe freshness` (read-only mode, exit 0):

- F1–F4: FRESH.
- P1–P5: FRESH.
- S1: SOURCE_STALE.
- S2–S6: TRANSITIVE_DEPENDENCY_STALE.

The canonical Structure source manifest includes `src/core/Services.cpp`
(`CertificationFreshness.cpp:194`); the source changes legitimately invalidate
the Structure boundary. No fingerprint was manufactured and no revalidation
was attempted for the unfinished implementation. Historical certificates remain
unchanged. Phase 2B is not VERIFIED or complete.

## Integrity and change boundary

Compared raw SHA-256 for all 8,662 previously captured files against
`amended-ready-contract.json` / `protectedBefore`. Exactly five changed:

- src/core/ConfigurationUpdateService.cpp
- src/core/ConfigurationUpdateService.h
- src/core/Services.cpp
- src/core/Services.h
- src/ui/workflows/update/configuration/UpdateConfigurationPage.cpp

Every other captured file remains byte-identical. In particular the earlier
TemplateManager/test hunks, Phase 1 and Phase 2A evidence, live event history,
LICENSE, and both protected test run trees were not rewritten. The four-file
original baseline evidence remains unchanged; the configuration service has
additional authorized implementation hunks on top of that preserved baseline.

Separate protected-content check: 121 files checked, zero discrepancies.
The real HVD Components directory exists and contains zero entries (including
hidden/system entries). No product file outside the eight authorized paths was
modified. Index remains empty; no commit or push was created. Existing dirty
control-plane state was not absorbed or reset. FPSC, P6, C1, real HVD recreation
and configuration were not started.
