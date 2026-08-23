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

The application has 23 clickable pages in six unnumbered groups. Numbers are
user-facing references only; internal navigation uses stable `WorkflowPageId`
values and explicit page registration.

### PROJECT

1. What is the project?
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
- Workflow navigation tests cover all 23 clickable IDs and non-clickable
  headings.
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
- Repository development uses the same live model in `aramf_setup/memory/framework-knowledge.json`.
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


## Latest Agent Task

- Task: Improve Resource Authority help text for applicable scopes
- Status: PASS
