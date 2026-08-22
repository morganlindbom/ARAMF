# ARAMF Project Status

## Current Architecture

ARAMF is a C++17 / Qt 6 desktop application. The canonical framework and
agent-facing control directory is `ARAMF/` (uppercase). Application code is
under `src/`, tests are under `tests/`, and target-project support is data
driven rather than a second runtime backend.

`MainWindow` owns the application shell, shared workflow page host, global
scrolling, global UI zoom, and developer-controlled startup placement. The
startup screen index and requested width/height are supplied from `src/main.cpp`.
`WorkflowWidget` owns grouped navigation and Back/Forward sequencing. Every
visible workflow page has a dedicated directory under `src/ui/workflows/`.

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

## Verified Functionality

- Build: PASS — `cmake --build build --config Debug --parallel 4`.
- CTest: PASS — `aramf_core_tests` and `aramf_workflow_tests` both passed.
- Application startup: PASS — normal Windows platform startup smoke test
  completed and the process was stopped cleanly.
- Workflow navigation tests cover all 23 clickable IDs and non-clickable
  headings.
- Core persistence tests cover structured resource authority, scopes, policy,
  old resource-name migration, AI state, Academic state, and capability state.
- Stale active `ProjectResourcesPage` references: none.
- Canonical framework root: PASS — Git now tracks `ARAMF/` only; the duplicate
  lowercase root was removed from the working tree.
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

Physical multi-monitor, native file-dialog, and full interactive visual
click-through verification were not available in automated testing. The
application was started normally, but those interactions remain manual checks.

## Known Limitations

- Finalize and Verify remain primarily workflow scaffolding and do not yet
  execute every displayed action.
- Resource location mode records referenced versus intended project-local copy;
  selecting that mode does not itself copy files.
- Full interactive GUI review at every zoom level remains a manual task.
- Historical reconstruction documents may contain obsolete terminology; they
  are evidence, not active architecture instructions.

## Remaining Work / Next Areas

1. Continue focused refinement of Rules, Memory, Review, Generate, Verify, and
   Finalize without weakening stable page ownership.
2. Add deeper widget-level tests for resource dialogs and zoom/scroll layout.
3. Implement explicit managed-resource copying when its ownership semantics are
   defined.
4. Connect Memory and generation workflows to all remaining live services.
