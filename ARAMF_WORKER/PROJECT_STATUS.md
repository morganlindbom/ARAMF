# ARAMF Self-Hosted Project Status

This is the live development status for the ARAMF repository. The repository
uses `ARAMF_WORKER/` as its active self-hosted control plane. Product and
bootstrap source remains under `aramf_setup/`; that directory is not a second
live status authority.

## Self-Hosting Snapshot

- Project: ARAMF — AI Rules And Memory Framework — self-hosted development project.
- Technology: C++17, Qt 6, CMake, CTest, Windows Desktop, Git, GitHub, Visual Studio Code.
- Primary AI: Codex.
- Historical release baseline: 550/550 validation completed and RELEASE-READY.
- Current architecture migration: generated managed-project control planes use `ARAMF_WORKER/`; legacy `ARAMF/` is detected and preserved during migration.
- Recursion protection: generated worker content is excluded from legacy-tree copying and is never treated as a nested worker.

This is the live development status for the ARAMF repository itself.
`ARAMF_WORKER/` is the active self-hosted project control plane. The separate
`aramf_setup/` directory is product/bootstrap source material and is not a
second live project-status authority.

## Current Architecture

ARAMF is a C++17 / Qt 6 desktop application. The repository's live project
status and agent-facing control plane live under `ARAMF_WORKER/`; product and
bootstrap source remains under `aramf_setup/`. The canonical control directory
generated inside every managed target project is `ARAMF_WORKER/`. Application code is under `src/`, tests are under
`tests/`, and target-project support is data driven rather than a second runtime
backend.

Template selection is composable: Project Setup uses a framed two-column
checkbox grid and stores selected module IDs in ProjectModel. MainWindow owns
the page-level vertical scroll area while workflow navigation scrolls
independently; page content keeps its natural height and horizontal scrolling
is disabled. The module grid is installed directly in the framed selector
layout, so its QGroupBox size hint includes every checkbox and cannot collapse.
Project Setup now separates atomic project modules from composite templates;
composite selections resolve their required module IDs into the module frame.
The visible catalog now treats Frame 1 as the built-in functionality catalog;
Frame 2 is reserved for user-saved templates, with an empty-state message when
no saved templates exist. Legacy composite definitions remain available only
for migration and compatibility.
All visible Frame 1 entries are now registered as authoritative module
definitions, and module selections are applied as partial contributions before
the merged project is validated.
Reusable template saving now performs structural/name validation only, strips
project-instance identity fields, and preserves selected module IDs for
save/restart/restore. Partial configurations such as Qt+CMake are supported.
The module grid is now four-column responsive. Two protected official templates
(Pico Visual Designer and ARAMF Development) are exposed alongside user-saved
templates; historical composites remain migration-only.
The Frame 1 catalog audit added catalog-backed C, TypeScript, web/application
role, Pico SDK and PIO Assembly modules, plus focused backend/frontend/library
and mobile roles.
Final official-template consistency audit confirms Pico Visual Designer now
activates Pico SDK, C and PIO Assembly explicitly; ARAMF Development activates
all of its visible Qt/CMake desktop modules.
The responsive layout regression pass retains the four-column module grid,
disables workflow horizontal scrolling, names the page scroll host for tests,
and verifies frame width across the supported window sizes.
The page host now ignores child minimum widths horizontally, while
TemplateSelector derives its module/template column count from actual content
width and reflows existing checkbox widgets without changing model state.
All workflow pages now receive a shared shell-level responsive normalization:
page and control minimum widths are cleared, form layouts wrap and grow fields,
and labels, editors, groups and selectors follow the scroll viewport width.
Dense checkbox groups use a reusable content-width-based reflow helper that
preserves their existing widgets and model state.
Windows builds now deploy the MinGW runtime DLLs beside ARAMF and its test
executables in addition to the Qt runtime, so launching `build/aramf.exe` does
not depend on the developer's MSYS2 `PATH`.

Project navigation layout V1 now gives PROJECT a reusable soft blue section
header and an always-expanded hierarchy: page 1 is a small overview parent,
1.1 owns project file/path/Worker identity controls, and 1.2 owns the existing
modules/templates selector. Stable page IDs keep direct navigation and
Back/Forward independent of visible row positions; older page-1 Setup routes
to the parent overview.

## Checkpoint Status

Completed: composable project modules, the 27-module catalog audit, official
Pico Visual Designer and ARAMF Development templates, user-saved template
persistence/provenance, partial validation, and the responsive Project page
including dynamic module reflow and Project path/Browse behavior.

The shared responsive normalization has been implemented for the remaining
workflow pages. Manual visual review of unusual font metrics or future
page-specific controls remains an ongoing verification item.
The Windows runtime deployment checkpoint is complete: the build system places
the required MinGW DLLs beside `build/aramf.exe`, and full CTest passes 4/4.
The previous runtime launch error is resolved; no broader manual all-page visual
certification is claimed here.
The Project navigation layout V1 checkpoint is implemented and verified by
focused navigation/template tests, full CTest, and a real application review
using the current ARAMF project file. Further visual approval remains with the
user.

`MainWindow` owns the application shell, shared workflow page host, global
scrolling, global UI zoom, and developer-controlled startup placement. The
startup screen index and requested width/height are supplied from `src/main.cpp`.
`WorkflowWidget` owns grouped navigation and Back/Forward sequencing. Every
visible workflow page has a dedicated directory under `src/ui/workflows/`.

Repository-local setup material is stored under `aramf_setup/`. This is not a
runtime generation destination. When a user selects `ProjectModel::projectPath()`,
all generated bootstrap, memory, rules, routing, resources, verification and
other control-plane files are written below the selected project's uppercase
`ARAMF_WORKER/` directory.

Root-level generated bootstrap source is kept separately in
`aramf_setup/bootstrap/`. Its `AGENTS.md` is a thin template/reference for the
generated target-root `AGENTS.md`; it is not the repository-development
`aramf_setup/AGENTS.md`. The bootstrap directory itself is never generated in
managed projects.

## Current Workflow

The application has 28 clickable pages in seven unnumbered groups. Numbers are
user-facing references only; internal navigation uses stable `WorkflowPageId`
values and explicit page registration.

### PROJECT

1. What is the project?
1.1. Project file, path & Worker
1.2. Project modules & templates
2. Academic
3. Which languages are used?
4. Which frameworks / SDKs are used?
5. Which development tools are used?
6. Where does the project run?
7. Which hardware / architecture is used?
8. How is it built, tested and delivered?

### AI

9. Which AI agents are used?
10. What may AI work on?
11. How autonomous may AI be?
12. Which ARAMF systems should AI use?

### RESOURCES

13. Which resources belong to the project?
14. Which sources are authoritative?
15. How should AI use the resources?

### RULES

16. Which rules should apply?
17. How should rules be routed?

### MEMORY

18. What should ARAMF remember?
19. How should project memory be maintained?

### GENERATE

20. Review
21. Generate
22. Verify
23. Finalize

Each page is question-driven and owns one clear responsibility. Template
selection is part of Setup and may be `Disable`. Project Type is derived from
selected capabilities rather than manually entered. Academic configuration is
an independent project dimension. Project path (the managed target location)
and `projectFilePath` (the ARAMF configuration file) remain separate; Browse
changes only the project path and does not save.

## Implemented Features

- Added the Android Studio/Kotlin/Gemini project template. It uses the
  existing template/model/generation architecture with Kotlin, Gradle,
  Android Studio, Android SDK, Compose/Room preferences, Git, and Gemini
  defaults. Other agents remain selectable and course specifications can be
  represented as primary Source of Truth resources.
- Added Android-aware, scope-sensitive validation guidance for Gradle
  configuration/compile, unit tests, lint, instrumentation, emulator, and
  device checks, including Windows Gradle wrapper commands. Runtime evidence
  states remain distinct and are never fabricated.

- Project Memory Compaction is implemented as a governed C++ subsystem. It
  detects recurring semantic event patterns at a configurable threshold
  (default 500), creates project-local knowledge with source provenance,
  protects decisions/checkpoints/incidents/active evidence, records permanent
  compaction metadata, and rewrites history only through an explicit verified
  compaction operation. Normal recording never prunes history.

- Template-derived defaults with compatible user overrides and disabled-template
  manual configuration.
- Multi-selection capability models and stable catalog IDs for project
  languages, frameworks, tools, platforms, hardware, and build delivery.
- Four separate AI pages for agent identity, responsibilities, autonomy, and
  ARAMF integration. Primary and additional agents are distinct roles, and
  high-risk permissions require explicit selection.
- Select All / Clear All on AI pages 10–12. Page 11 excludes high-risk actions
  from Select All while Clear All clears all permissions.
- Finalize provides explicit AI Agent Entry Point creation. Generic and
  supported provider bootstraps are managed idempotently, preserve user-owned
  content, and converge on generated `ARAMF_WORKER/AGENTS.md`.
- Three resource pages for inventory, authority/scopes, and AI resource policy.
  `ProjectResource` stores stable ID, type, location, description, enabled
  state, location mode, authority, scopes, status, and loading override.
- Resource inventory and authority lists use compact native internal scrolling.
  The inventory detail editor is selection-driven and remains compact; an empty
  selection shows a short empty-state message.
- Project persistence saves the structured project, AI, academic, capability,
  and resource state. Older simple resource lists and older AI fields migrate
  safely.
- One MainWindow-level vertical `QScrollArea` hosts active pages. Ctrl+Plus,
  Ctrl+Minus, Ctrl+0, and Ctrl+mouse-wheel provide 30%–150% global UI zoom
  without resizing MainWindow.
- Rules are split into checkbox-driven selection and routing pages. Memory is
  split into capture and maintenance pages; both use compact grouped controls
  rather than rule tables or status-heavy panels.
- Project memory has a finite 10 GiB default and exposes only the maximum size
  as the user-facing capacity setting. `ProjectMemory` calculates managed
  memory usage before writes and automatically removes the oldest eligible
  event history until usage is near 90% of the limit. Protected current state,
  decisions, source-of-truth references, status and configuration data are not
  auto-deleted; if protected data alone exceeds the limit, the write is safely
  rejected.
- Generate page 21 maps every output-product checkbox to explicit
  `GenerationOptions`. Generation selectively writes agent/rule files, routing,
  platform metadata, resources, memory, and provenance; unselected products
  are preserved rather than deleted. Generate remains separate from Save,
  Build, Verify, and Finalize.

- Generate, Verify and Finalize now form a connected lifecycle. Review is a
  read-only summary of ProjectModel and the selected GenerationOptions. Verify
  performs non-destructive filesystem/content checks, parses selected JSON
  products, runs the existing memory consistency validation when memory output
  is selected, records a structured verification result, and detects stale
  generated output with a deterministic configuration fingerprint. Finalize
  requires a current PASS verification, validates memory consistency, records
  an idempotent PROJECT_FINALIZED event, and updates generated project status.
  It does not build, test, deploy, commit, or push.

- Consumer-project generation now maintains an idempotent, ARAMF-scoped
  `.gitignore` block. ARAMF self-hosting is excluded using the canonical
  program-root identity, and generated root `AGENTS.md` is ignored only when
  its ARAMF ownership marker is present.

- The page 21 action is now `Save & Generate`. It validates Project Path and
  selected products, reuses the existing Setup Save/Save As mechanism, blocks
  generation on save failure or cancellation, and generates from the exact
  saved in-memory ProjectModel. `projectFilePath` remains the persistence
  location and `projectPath` remains the generated target location. Manual
  Save and Save As remain available.

- Finalize now exposes a separate `Create AI Agent Entry Points` action. It
  maintains target-root `AGENTS.md` and creates supported provider entry files
  for selected stable AI agent IDs. Managed sections are idempotent, existing
  user content is preserved, unsupported agents use generic `AGENTS.md`, and
  removed agents are never deleted automatically. All entry points route to
  target `ARAMF_WORKER/AGENTS.md`; repository-only `aramf_setup/bootstrap/` is never
  generated.

## Verified Functionality

- ANDROID-001 through ANDROID-020 pass in the native core regression suite.
- Full CTest suite passes: `aramf_core_tests`, `aramf_workflow_tests`,
  `aramf_template_tests`, `aramf_update_campaign`, and
  `aramf_configuration_update` (5/5 PASS).

- Current control-plane repair: memory manifest, current state, consistency
  validation, cold-start validation, recording configuration, legacy event-ID
  exception policy, and completed UPDATE lifecycle agree. Real PVD history was
  not deleted. The PVD compaction dry run projects 188 active events to 184.

- 250-scenario campaign completed under `test_250/`. The native core workflow
  exercised isolated project creation, Save/Load, selective Generate, Verify,
  Finalize, idempotent Finalize, AI entry points, and generated-file inspection
  across the required distribution bands. Initial campaign history recorded
  three failures in selective-generation Finalize cases; all three passed after
  the lifecycle correction.
- Finalization now validates and records Project Memory only when the Memory
  generation product is selected. This preserves selective-generation
  semantics while retaining the memory consistency precondition for full
  memory generation. A regression test covers selective finalization and its
  idempotence.

- Build: PASS — `cmake --build build --config Debug --parallel 4`.
- CTest: PASS — `aramf_core_tests` and `aramf_workflow_tests` both passed.
- Application startup: PASS — normal Windows platform startup smoke test
  completed and the process was stopped cleanly.
- Workflow navigation tests cover all 28 clickable IDs and non-clickable
  headings.
- Project layout tests cover the always-expanded 1/1.1/1.2 hierarchy, shared
  section-header presentation, active child selection, page ownership split,
  current-project loading, and responsive viewport boundaries.
- Core persistence tests cover structured resource authority, scopes, policy,
  old resource-name migration, AI state, Academic state, and capability state.
- Stale active `ProjectResourcesPage` references: none.
- Repository setup root: PASS — Git now tracks `aramf_setup/`; the former
  repository-local `ARAMF/` directory is gone. Generated target control paths
  use `ARAMF_WORKER/`; legacy `ARAMF/` is retained only for migration.
- Rules and memory persistence defaults: PASS — new rule enforcement/routing,
  memory capture/maintenance settings and the maximum byte limit round-trip
  through project persistence; legacy threshold/action fields are ignored
  safely when loading.
- Memory limit enforcement: PASS — pre-write checks, finite defaults, usage
  calculation, automatic oldest-eligible pruning toward a 90% target, and safe
  protected-memory rejection are implemented in the memory service.
- Memory consistency and cold-start validation: PASS. Initialization and core
  persistence tests exercise both generated validation artifacts.
- Final Rules/MEMORY refactor build: PASS — 23-page workflow navigation,
  grouped checkbox pages, memory configuration persistence, and oversized-write
  rejection are covered by the current build/tests; normal Windows startup also
  passed.

Selective generation: PASS — core tests cover product selection, disabled
memory side-effect isolation, catalog-based rule rendering, preserved
unselected routing files, and idempotent memory activation.

Physical multi-monitor, native file-dialog, and full interactive visual
click-through verification were not available in automated testing. The
application was started normally, but those interactions remain manual checks.

## Known Limitations

- Verify and Finalize intentionally do not build, test, deploy, or repair the
  target project; those operations remain outside the current ARAMF lifecycle.
- Resource location mode records referenced versus intended project-local copy;
  selecting that mode does not itself copy files.
- Full interactive GUI review at every zoom level remains a manual task.
- Historical reconstruction documents may contain obsolete terminology; they
  are evidence, not active architecture instructions.

## Remaining Work / Next Areas

1. Continue focused refinement of Rules, Memory, Review, Verify, and Finalize
   without weakening stable page ownership.
2. Add deeper widget-level tests for resource dialogs and zoom/scroll layout.
3. Implement explicit managed-resource copying when its ownership semantics are
   defined.
4. Add deeper widget-level tests for Review, Verify and Finalize status views.

## Live Framework Knowledge

- `FrameworkKnowledgeService` is implemented in C++ core.
- Managed projects receive `ARAMF_WORKER/memory/framework-knowledge.json` as part of Project Memory initialization.
- Approved lessons are read directly by AI agents through the canonical `ARAMF_WORKER/AGENTS.md` startup contract and therefore become useful immediately without reopening ARAMF.
- The lifecycle supports evidence-backed candidates, explicit user approval, deduplication, scope filtering and non-destructive superseding.
- AI agents are explicitly forbidden from self-approving candidates.
- For ARAMF itself, live Framework Knowledge is `ARAMF_WORKER/memory/framework-knowledge.json`; `aramf_setup/` remains product/bootstrap source.
- Two approved development lessons currently preserve verified corrections and correct repository-root path interpretation.
- The 250-scenario campaign produced candidate `fk-7a246faa4bc6ad74` with
  `reviewStatus: more-evidence`. It generalizes ISSUE-001 across lifecycle,
  selective-generation, optional-components, verification and finalization;
  evidence is attached from TEST-198, TEST-204 and TEST-210. It remains
  inactive until explicit human approval and is not part of the approved
  knowledge precedence layer.
- Release validation TEST-251 through TEST-550 completed 300 new scenarios.
  The manual phase has user-confirmed 40 GUI passes and 10 PASS-AFTER-FIX
  cases. ISSUE-002 (viewport overflow with horizontal scrolling disabled) was
  fixed and retested. The apparent Save As failure was reclassified as
  TEST-HARNESS-002 after the normal production executable passed Cancel, Escape
  and window-close verification. All manual scenarios are complete and the
  classification is RELEASE-READY.
- Windows/Qt source-level verification is available through the existing CMake build and CTest configuration.

## ARAMF_WORKER Migration Verification

- The universal generated control-plane directory is `ARAMF_WORKER/`.
- Legacy `ARAMF/` projects are detected, copied non-destructively into the
  worker where files are missing, and retained as legacy evidence. If both
  directories exist, `ARAMF_WORKER/` is authoritative and conflicts are not
  overwritten.
- Root, Codex, Claude, Gemini, and Copilot bootstrap routing points to the
  worker. `aramf_setup/` remains product/bootstrap source.
- Migration regression coverage includes new generation, path safety, spaces
  in project paths, provider entry points, memory/Framework Knowledge,
  Verify/Finalize, idempotence, legacy preservation, both-directory handling,
  and nested-worker recursion protection.
- Verification completed on Windows/Qt: production build PASS, CTest 2/2 PASS,
  test_250 runner 250/250 PASS, and application startup PASS.
- Initial execution of the legacy test_550 runner exposed 140 obsolete GUI
  harness assertions and 50 manual cases; historical test_550 evidence was
  restored unchanged while the executable runner was corrected.
- The runner was corrected to use current `ARAMF_WORKER` assertions, the
  verified `ScrollBarAsNeeded` behavior, and deterministic reverse page IDs.
  It now executes only the 250 automatically executable scenarios and passes
  250/250. The 50 historical manual scenarios remain separate and unchanged.

## ARAMF Lifecycle

Current state: Finalized

SELFHOST-ISSUE-002 (resource authority persistence) is fixed. Final production
GUI validation preserved the complete ten-resource authority matrix through
navigation, Save, Close/Open, Save without edits, second Close/Open, Save &
Generate, Verify, Finalize, and final Close/Open. The generated resource
manifest matched the persisted project state for all 10 resources; IDs,
descriptions, scopes, and canonical identities were preserved.

















































### Governed State-Consistency Audit And Baseline Synchronization

### Current implementation state

- Current Git HEAD commit: `a9a9c5c94483c334f5d9eee7a304c25cc0dbeab4` (Complete universal structured provenance coverage and semantic scope isolation in Project Memory).
- Previous synchronized merge/code baseline: `af22b35ee6734f66e875b652602802b0758fa19c` (Merge remote-tracking branch 'origin/main' into main).
- Preceding feature implementation: `2575dbe79c5b98835da1837bd04ff307cd59ad32` (Add Update Configuration workflow page and update service).
- Preceding P2 orchestration integration: `cf6c556eb940d4979a50ca8f5118dd2d367da3f8` (Integrate P2 orchestration and record recertification requirement).
- Historical earlier certified baseline: `bcb26854e13f3714ac294e4e7b6f48573860151e` (Add canonical P0 runtime ownership authority, P0.1.2.1.1).

The lifecycle record is preserved as historical/current persisted state:

- P0: `P0.1.2.1.1` (freshly recertified, cert=1, done=1)
- P1: `P1.1.1.1.1` (freshly recertified, cert=1, done=1)
- P2: `P2.1.4.1.1` (freshly recertified, cert=1, done=1, dependency-stale condition cleared)
- P3: `P3.1.1.1.1` (certified, loop=1, iteration=1, cert=1, done=1)
- Loop: `1`
- Product version: `0.0.0`

### P2 implementation and Update Configuration additions

P2 iteration 4 implemented and exercised the canonical chain:

`P1 DAG/context -> P2 orchestration -> P0 TaskContract -> P0 RuntimeOwnershipService -> execution -> P0 validation/postflight -> P1 ContextCoordinationService::createHandoff()`

The implementation includes `ExecutionOrchestrator`, canonical P1 DAG/context consumption, P0 ownership and postflight validation integration, P1 handoff integration, scheduling, parallel execution, ownership conflict handling, failure classification, retry, checkpoint/resume, worker-loss handling, stale-context handling, deadlock diagnostics, and persistence/reload.

The Update Configuration workflow page (`UpdateConfigurationPage`) and update service (`ConfigurationUpdateService`) add project update validation, recursive ADD/MODIFY/REMOVE change planning, removal-blocking, and dedicated unit/regression coverage (`aramf_configuration_update`).

### Validation completed

The latest known validation evidence is verified across the complete suite:

- Full build: PASS
- Full CTest: 5/5 PASS (`aramf_core_tests`, `aramf_workflow_tests`, `aramf_template_tests`, `aramf_update_campaign`, `aramf_configuration_update`)
- P0/Worker tests: 275/275 PASS
- P1 context: 33/33 PASS
- Workflow tests: PASS
- Template tests: 4948 checks / 0 failures
- Configuration update tests: PASS
- UPDATE campaign: 310/310 PASS
- Cold-start validation: PASS (`ARAMF_WORKER/memory/cold-start-validation.json` refreshed and verified)
- Memory consistency validation: PASS (`ARAMF_WORKER/memory/memory-consistency-validation.json` verified)
- Governance/routing: PASS
- Persistence/reload: PASS
- P2 runtime integration: PASS
- Parallel execution: PASS
- Ownership conflict: PASS
- Retry: PASS
- Checkpoint/resume: PASS
- Worker loss: PASS
- Stale context: PASS
- Deadlock diagnostics: PASS
- `git diff --check`: PASS
- Working tree: clean and synchronized with `origin/main`

### Post-certification cross-layer audit

The historical cross-layer audit found a material P0 semantic change in `src/core/WorkerTaskServices.cpp`, affecting P0 snapshot/postflight behavior, TaskContract binding fingerprints, authorization binding, generated `project.json` acceptance, and verification-evidence acceptance during orchestration.

- P0 semantic change detected: RESOLVED (P0 full recertification completed)
- Affected authority: WorkerTaskServices / P0 postflight and binding semantics
- Classification: `P0_RECERTIFIED`
- P1 classification: `P1_RECERTIFIED`
- P2 dependency state: `DEPENDENCY-FRESH` (P2 full recertification completed against freshly certified P0/P1)

### Certification trust state

The complete dependency chain is freshly validated and trusted for production:

- P0: `P0.1.2.1.1` CERTIFIED (freshly verified by dedicated P0 suite, 275/275 checks PASS)
- P1: `P1.1.1.1.1` CERTIFIED (freshly verified by dedicated P1 suite, 33/33 checks PASS)
- P2: `P2.1.4.1.1` CERTIFIED (freshly verified by P2 integration suite, dependency-stale condition cleared)
- P3: `P3.1.1.1.1` CERTIFIED (freshly verified by dedicated P3 suite, 18/18 hermetic PASS, dogfooding PASS, boundary governance PASS)

### Governed ARAMF Memory System Completion (Universal Provenance & Semantic Scope Isolation)

Defects resolved:
1. Universal Truthful Structured Provenance Coverage:
   - Extended non-operational human- and agent-initiated governance/knowledge event paths (`DECISION_RECORDED`, `DECISION_SUPERSEDED`, `CHECKPOINT_CREATED`, `ADMIN_OVERRIDE`, `ADMIN_OVERRIDE_VALIDATION`, `FRAMEWORK_KNOWLEDGE_CANDIDATE`, `FRAMEWORK_KNOWLEDGE_APPROVED`) with mandatory structured provenance (`actor`, `agentId`, `tool`).
   - Extended `ProjectMemory::recordDecision`, `ProjectMemory::supersedeDecision`, `ProjectMemory::recordCheckpoint`, `ProjectMemory::recordAdministrativeOverride`, `FrameworkKnowledgeService::propose`, `FrameworkKnowledgeService::proposeApprovedByAdministrator`, `FrameworkKnowledgeService::approve`, and `FrameworkKnowledgeService::supersede`.
   - Wired CLI parameters (`--actor`, `--agent-id`, `--tool`, `--scope`) into `MemoryCommand.cpp` for `memory decision record`, `memory decision supersede`, `memory checkpoint`, `memory knowledge propose`, and `memory knowledge approve`.
   - Guaranteed that human actions preserve administrator provenance while agent actions record explicit agent identity and tool, with canonical system provenance reserved exclusively for automated/system-internal events.
2. Semantic Scope Isolation (Levels C, D, E):
   - Level C (Type Affinity): Enforced category-specific scope validity via `ProjectMemory::isScopeValidForCategory`. Checkpoint recovery scopes must be repository recovery scopes (`project`, `entire-project`, `all`, `history`, `project+global`), rejecting partition scopes; decision scopes reject archive partition `history`; operational event scopes reject pure `global`; Knowledge entries enforce slug validity and unique topic tagging.
   - Level D (Combination Legality): Enforced `ProjectMemory::validateScopeCombinations` rejecting contradictory combinations (`project` and `global` without `project+global`, universal `all`/`entire-project` with specific partitions, archive `history` with active partitions).
   - Level E (Cross-Scope File Matching): Enforced `ProjectMemory::validateCrossScopeFiles` rejecting records that claim one partition scope while referencing files/resources outside that permitted scope.
3. Verification results:
   - Hermetic regression suite: 31/31 PASS (`PROV-001` through `PROV-016`, `SCOPE-001` through `SCOPE-014`).
   - Full CTest suite: 5/5 PASS (`aramf_core_tests`, `aramf_workflow_tests`, `aramf_template_tests`, `aramf_update_campaign`, `aramf_configuration_update`).
   - Memory consistency validation: PASS (`ARAMF_WORKER/memory/memory-consistency-validation.json` 20/20 checks PASS, status PASS).
   - Cold-start validation: PASS (`ARAMF_WORKER/memory/cold-start-validation.json` status PASS).
   - Final strict memory certification: Provenance PASS, Scope Isolation PASS, Memory System Overall Status PASS.

### Full Governed P0-P2 Recertification Campaign

Campaign Identity: `P0-P2-RECERTIFICATION-2026-09-18`
Baseline: `a9a9c5c94483c334f5d9eee7a304c25cc0dbeab4` (main)

Lifecycle Transitions:
- P0: Reset to `P0.1.2.0.0` -> Recertified with fresh evidence -> `P0.1.2.1.1` (cert=1, done=1)
- P1: Reset to `P1.1.1.0.0` -> Recertified with fresh evidence -> `P1.1.1.1.1` (cert=1, done=1)
- P2: Reset to `P2.1.4.0.0` -> Recertified with fresh evidence -> `P2.1.4.1.1` (cert=1, done=1)

Campaign Evidence:
1. Pre-Campaign Consistency:
   - HEAD == origin/main == `a9a9c5c`
   - Working tree clean
   - Cold-start validation PASS
   - Memory consistency validation PASS (20/20 PASS)
   - Recorder integrity verified
2. Process Reset:
   - State reset reason: "Full governed P0-P2 recertification after memory-system completion and current repository integration."
   - Provenance recorded: actor=agent, agentId=antigravity, tool=aramf-cli, scope=project
   - Task start event recorded: sequence 312 (`event-334079b3-ef0d-44e6-bd33-a225b799aed9`)
3. P0 Foundation Recertification:
   - Validated: TaskContract preparation, RuntimeOwnershipService synchronized claiming/releasing, file permission boundaries, unmapped file blocking, preflight/postflight enforcement, administrative override, destructive command prohibition, and out-of-scope modification blocking.
   - Focused test suite: `aramf_core_tests --worker-tasks`: 275/275 checks PASS (0 failures across 11 scope matrix configurations).
   - Verdict: `P0.1.2.1.1` PASS.
4. P1 Context Coordination Recertification:
   - Validated: ContextCoordinationService, derived context index (v1, derived, project-bound), scoped routing, decision retrieval history exclusion, compressed context with provenance, byte-stable regeneration, freshness tracking, linear task DAG, DAG cycle rejection, contract handoffs without scope escalation, agent adapter neutrality (`openai-codex`, `gemini`), and project-to-project isolation.
   - Focused test suite: `ContextCoordinationTests`: 33/33 checks PASS (0 failures).
   - Verdict: `P1.1.1.1.1` PASS.
5. P2 Execution Orchestration Recertification:
   - Validated: `ExecutionOrchestrator`, consumption of P1 DAG and context, P0 ownership claim and release during execution, strict P0 postflight validation enforcement (cannot be bypassed by caller flags), P1 handoff generation on task completion, concurrency across independent resource claims, ownership collision blocking, worker-loss recovery, retry mechanics, checkpoint/resume, deadlock diagnostics, and state reloading without loss of semantic ownership history.
   - Focused test suite: `aramf_core_tests --p2-execution`: PASS.
   - Cross-layer integration: P0 -> P1 -> P2 full chain PASS.
   - Verdict: `P2.1.4.1.1` PASS. Dependency-stale debt CLEARED.
6. Full Repository Regression:
   - Full CTest suite: 5/5 PASS (100%)
     - 1/5 `aramf_core_tests`: PASS (66.69s)
     - 2/5 `aramf_workflow_tests`: PASS (2.08s)
     - 3/5 `aramf_template_tests`: PASS (79.32s)
     - 4/5 `aramf_update_campaign`: PASS (6.36s)
     - 5/5 `aramf_configuration_update`: PASS (0.10s)
   - Task completion event recorded: sequence 313 (`event-3917488c-f285-447f-8491-43344e18bdb2`)
7. Conclusion:
   - P0, P1, and P2 are all freshly certified.
   - All historical records and process evidence preserved.
   - P3 is unblocked and activated.

### P3.1.1 Implementation & Certification — Predictive Task Optimization

- Campaign Identity: `P3-PREDICTIVE-OPTIMIZATION-ITERATION-1`
- Baseline Commit: `6cd453467b839e2c064a0576a44a0df7b8152917` (main)
- Canonical Transition: `P3.1.0.0.0` -> `P3.1.1.0.0` -> `P3.1.1.1.1` (loop=1, iteration=1, cert=1, done=1)
- Architecture Implemented:
  - `TaskSignature` (`src/core/TaskSignature.h`, `src/core/TaskSignature.cpp`): Deterministic normalization across 10 task dimensions (category, operation, scopes, referenced files, target subsystem, language/framework, validation requirements, governance class, resource ownership class, version). Computes stable SHA-256 fingerprint from compact canonical JSON. Implements deterministic Jaccard-based similarity metric without embeddings or LLM inference.
  - `PredictionContract` (`src/core/PredictiveOptimizationService.h`): Versioned (1.0) contract encapsulating predictionId, taskId, taskSignature, taskClassification, predictedScopes, predictedFiles, predictedValidation, predictedRiskCategories, predictedChangeBreadth, breadthRationale, confidence, evidenceReferences, provenance, createdAt, sourceProject, and advisoryStatus (`ADVISORY`). Hardcoded `isExecutionAuthority() == false`.
  - `PredictionConfidence` (`src/core/PredictiveOptimizationService.h`): Measurable, explainable confidence model based on sample size, match precision, consistency score, freshness score, and conflicting evidence penalty. Produces ratings `HIGH`, `MEDIUM`, `LOW`, or `INSUFFICIENT_EVIDENCE`.
  - `PredictionEvaluation` (`src/core/PredictiveOptimizationService.h`): Deterministic comparison between PREDICTED and ACTUAL execution outcomes. Computes file precision, recall, and change breadth comparison; scope match/mismatch; validation matched vs missed; and risk anticipation.
  - `PredictiveOptimizationService` (`src/core/PredictiveOptimizationService.cpp`): Evidence-based prediction engine consuming historical events from `ARAMF_WORKER/memory/event-log.jsonl`, scope metadata, and validation policy. Supports prediction, evaluation, persistence to `ARAMF_WORKER/predictions/`, and registry listing.
- Governance Boundaries Verified:
  - P3 cannot claim runtime ownership (`RuntimeOwnershipService::claim` rejects prediction artifacts with `TASK_CONTRACT_INVALID`).
  - P3 cannot expand permitted files or bypass P0 `WorkerTaskServices::prepare`.
  - P3 cannot bypass P0 postflight validation (`WorkerTaskServices::postflight` rejects prediction artifacts as execution evidence).
  - P3 cannot expand P1 context scope (`ContextCoordinationService::route` enforces model-governed scopes).
  - P3 is explicitly non-authoritative: prediction != permission, prediction != execution, prediction != fact.
- Verification Evidence:
  - Dedicated P3 test suite (`tests/P3PredictiveTests.cpp`): 18/18 hermetic checks PASS (`P3-001` through `P3-018`).
  - Dogfooding against ARAMF repository self-model: PASS (predicted 3 files, 3 validation suites, confidence=0.665 MEDIUM, precision=1.0, recall=1.0, 0 missed validation).
  - P0 regression: 275/275 checks PASS (`aramf_core_tests --worker-tasks`).
  - P1 regression: 33/33 checks PASS (`ContextCoordinationTests`).
  - P2 regression: `P2-EXECUTION checks=PASS` (`aramf_core_tests --p2-execution`).
  - Full CTest suite: 5/5 tests PASS (100%, 0 failures):
    - 1/5 `aramf_core_tests`: PASS (55.14s)
    - 2/5 `aramf_workflow_tests`: PASS (1.93s)
    - 3/5 `aramf_template_tests`: PASS (79.53s)
    - 4/5 `aramf_update_campaign`: PASS (6.25s)
    - 5/5 `aramf_configuration_update`: PASS (0.10s)
  - Memory cold-start validation: PASS (`ARAMF_WORKER/memory/cold-start-validation.json`).
  - Memory consistency validation: PASS (`ARAMF_WORKER/memory/memory-consistency-validation.json`).
- Recorder Events:
  - Campaign Start: Sequence 314 (`event-fdd47333-d665-461b-a5f9-f315591eb8f5`), `TASK_STARTED`, actor=agent, agentId=antigravity, tool=aramf-cli, scope=project.
  - Campaign Completion: Sequence 315 (`event-d904d74c-549e-4bba-b225-cfc9630383b5`), `TASK_COMPLETED`, status=PASS, actor=agent, agentId=antigravity, tool=aramf-cli, scope=project.
- Known Limitations:
  - P3.1.1 is deterministic and evidence-backed; it does not utilize machine learning or neural networks.
  - Tasks with no historical precedent return `INSUFFICIENT_EVIDENCE` rather than speculating.
  - P3 does not autonomously self-modify prediction rules.
- Next Recommended Iteration: Completed — all 4 iterations (P3.1.1 through P3.1.4) certified.

### P3.1.2 Implementation & Certification — Multi-Project Evidence, Ranking & Explainability Decomposition

- Campaign Identity: `P3-PREDICTIVE-OPTIMIZATION-ITERATION-2`
- Canonical Transition: `P3.1.2.0.0` -> `P3.1.2.1.1` (loop=1, iteration=2, cert=1, done=1)
- Architecture Implemented:
  - `EvidenceSourceType` & `RankedEvidence` (`src/core/PredictiveOptimizationService.h`, `src/core/PredictiveOptimizationService.cpp`): 4-type evidence taxonomy (`LocalOperationalEvent`, `ApprovedGlobalKnowledge`, `ScopePolicy`, `ContractHistory`). Deterministic evidence ranking formula:
    TotalRank = SourceMultiplier * (0.50 * Relevance + 0.25 * Freshness + 0.25 * OutcomeWeight)
    with source multipliers: Local=1.0, ApprovedGlobal=0.85, ContractHistory=0.90, ScopePolicy=0.70. Deterministic tie-breaking: `(totalRank DESC, freshness DESC, evidenceId ASC)`.
  - Multi-Project Evidence Isolation: Consumes approved Framework Knowledge (`FrameworkKnowledgeService::approvedEntries` and `approvedGlobalEntries`) with strict approval checks (`status == "approved"` && `reviewStatus == "approved"`). Candidate and unapproved entries strictly excluded.
  - Foreign Path Injection Prevention: Foreign file paths from external projects are strictly prohibited from entering local `predictedFiles`.
  - Explainability Decomposition: Every `PredictedItem` explicitly links to its `primaryEvidenceId`, aggregated `contributionScore`, and contributing `sourceTypes`, with human-readable rationales explaining why the item was predicted.
- Verification Evidence:
  - Hermetic test suite: Tests `P3-019` through `P3-022` PASS.
- Recorder Events:
  - Start: Sequence 316 (`event-76db301a-fe1c-4c47-a540-514edd2c1d8a`), `TASK_STARTED`.
  - Completion: Sequence 317 (`event-13c792aa-a3a5-43c6-9392-3293db5243f5`), `TASK_COMPLETED`, status=PASS.

### P3.1.3 Implementation & Certification — Refined Change Breadth, 11-Category Risk Taxonomy & Validation Routing

- Campaign Identity: `P3-PREDICTIVE-OPTIMIZATION-ITERATION-3`
- Canonical Transition: `P3.1.3.0.0` -> `P3.1.3.1.1` (loop=1, iteration=3, cert=1, done=1)
- Architecture Implemented:
  - 6-Tier Change Breadth Taxonomy: `LOCAL`, `COMPONENT`, `MULTI_COMPONENT`, `CROSS_LAYER`, `PROJECT_WIDE`, `UNKNOWN`. Implemented deterministic boundary classification in `determineChangeBreadth` based on touched subsystems, layer boundaries, and project configuration files.
  - 11-Category Canonical Risk Taxonomy: `OWNERSHIP_CONFLICT`, `SCOPE_EXPANSION`, `STALE_CONTEXT`, `STALE_VALIDATION`, `RECORDER_CONSISTENCY`, `PROVENANCE_MISSING`, `PERSISTENCE_FAULT`, `CROSS_LAYER_REGRESSION`, `DEPENDENCY_REGRESSION`, `RECOVERY_RETRY`, `DESTRUCTIVE_OPERATION`. Deterministically detected from historical clusters, failure patterns, and approved knowledge.
  - Proportional Validation Routing: Implemented `determineValidationLevel` escalating from `FOCUSED` to `SUBSYSTEM` or `FULL_REGRESSION` based on breadth, critical risks, and confidence scores.
  - Boundary Governance: Strict enforcement that predicted breadth or risks never grant execution authority or override P0 TaskContract file claim restrictions.
- Verification Evidence:
  - Hermetic test suite: Tests `P3-023` through `P3-026` PASS.
- Recorder Events:
  - Start: Sequence 318 (`event-d840bf07-63b9-4567-af0d-569a93641fd3`), `TASK_STARTED`.
  - Completion: Sequence 319 (`event-ef0b72c7-e4c8-4493-80d8-5d174b877204`), `TASK_COMPLETED`, status=PASS.

### P3.1.4 Implementation & Certification — Strict Calibration, Rolling Drift Detection & Dogfood Campaign

- Campaign Identity: `P3-PREDICTIVE-OPTIMIZATION-ITERATION-4`
- Canonical Transition: `P3.1.4.0.0` -> `P3.1.4.1.1` (loop=1, iteration=4, cert=1, done=1)
- Architecture Implemented:
  - Strict Confidence Calibration: Implemented sample-size scaling:
    - N=0: score = 0.0, rating = `INSUFFICIENT_EVIDENCE`.
    - N=1: score strictly capped at <= 0.35, rating = `LOW`.
    - N=2: score strictly capped at <= 0.70, rating = `MEDIUM` or `LOW`.
    - N>=3: eligible for `HIGH` only if consistency >= 0.85, match precision >= 0.75, freshness >= 0.70, and zero conflicting evidence penalty.
  - Rolling Prediction Drift Detector: `PredictionDriftDetector` evaluates rolling window of evaluations for precision, recall, missed validations, and overconfidence, producing advisory `PredictionDriftReport` (`STABLE`, `DRIFT_SUSPECTED`, `DRIFT_CONFIRMED`). Saved to `ARAMF_WORKER/predictions/drift-report.json`.
  - Multi-Scenario Dogfood Campaign: Evaluated across 6 diverse historical scenarios on the ARAMF self-model (Memory, UI, Release Management, Governance/Recertification, Configuration Update, and Novel Task), verifying 0 missed validations for known subsystems and zero hallucinations for novel tasks.
- Verification Evidence:
  - Dedicated P3 test suite: 31/31 hermetic checks PASS (`P3-001` through `P3-031`).
  - Full CTest suite: 5/5 PASS (100%, 0 failures)
    - 1/5 `aramf_core_tests`: PASS (60.06s)
    - 2/5 `aramf_workflow_tests`: PASS (2.16s)
    - 3/5 `aramf_template_tests`: PASS (79.26s)
    - 4/5 `aramf_update_campaign`: PASS (6.28s)
    - 5/5 `aramf_configuration_update`: PASS (0.16s)
  - P0 regression: 275/275 checks PASS (`aramf_core_tests --worker-tasks`).
  - P1 regression: 33/33 checks PASS (`ContextCoordinationTests`).
  - P2 regression: PASS (`aramf_core_tests --p2-execution`).
  - Memory cold-start validation: PASS (`ARAMF_WORKER/memory/cold-start-validation.json`).
  - Memory consistency validation: PASS (`ARAMF_WORKER/memory/memory-consistency-validation.json`).
- Recorder Events:
  - Start: Sequence 320 (`event-790c5643-488b-4938-a121-026954dec49f`), `TASK_STARTED`.
  - Completion: Sequence 321 (`event-f0b6e976-43b2-467d-a929-7c32ef735dd2`), `TASK_COMPLETED`, status=PASS.
- Final P3 Verdict: **CERTIFIED PASS** (`P3.1.4.1.1`, cert=1, done=1).

## Latest Agent Task

- Task: ARAMF P3.1.4 - Strict Confidence Calibration, Rolling Drift Detection, and Multi-Scenario Dogfood Campaign
- Status: PASS
