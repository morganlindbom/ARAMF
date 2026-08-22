<!-- decisions.md -->

# Durable Decisions

## 2026-08-21 — ARAMF implementation language

ARAMF itself is a C++17 / Qt 6 desktop application. Runtime framework behavior, project memory, generation, validation, and control-plane management must be implemented in C++ unless an explicit later decision changes this architecture.

Target projects managed by ARAMF may use other languages and frameworks. Supporting those project types does not introduce those runtimes as ARAMF dependencies.

## 2026-08-21 — Canonical control directory

All project-local files intended to provide rules, memory, status, routing, resources, templates, verification context, or related AI-facing state belong under an uppercase `ARAMF/` directory.

A repository-root `AGENTS.md` may exist as a minimal discovery/bootstrap file pointing to `ARAMF/AGENTS.md`. It must not duplicate the canonical rule set.

## 2026-08-21 — Project status ownership

`ARAMF/PROJECT_STATUS.md` is the canonical current program/project snapshot and must be updated by the active coding agent after meaningful implementation work. It records current capabilities, implemented changes, verified state, known issues, and next work; historical events remain separate.

## 2026-08-21 — User-owned custom content

Files under `ARAMF/custom/` are user-owned and must not be modified automatically by generation, migration, validation, or agent workflows.

## 2026-08-21 — UI source ownership mirrors workflow hierarchy

Each independently displayed workflow page owns a dedicated source directory
under `src/ui/workflows/` that mirrors its visible workflow position. MainWindow
and workflow navigation have their own component directories, while only
genuinely shared UI helpers belong under `src/ui/shared/`. This supports
maintainability, isolated development, AI-assisted page review, and focused
source exchange.

## 2026-08-22 — Academic is an independent project dimension

Academic, research, and thesis context belongs to its own PROJECT workflow page
and persisted model section. It must not be represented as a runtime platform or
inferred solely from technical selections. Academic configuration may coexist
with any languages, frameworks, platforms, hardware, and build choices.

## 2026-08-22 — Resource inventory, authority, and policy are separate

Project resources are represented as structured records with stable IDs. The
Resources workflow separates inventory, source authority/scopes, and AI loading
policy into independent pages. Legacy simple resource-name arrays remain
loadable through migration, and resource selection alone must not silently
copy files or write project configuration.

## 2026-08-22 — Question-driven workflow and stable page identity

Every independently displayed workflow page answers one clear user question
and owns a dedicated source directory that mirrors its visible workflow
position. Visible page numbers are user-facing references only; internal
navigation uses stable `WorkflowPageId` values and explicit page registration.
The shared MainWindow page host owns workflow scrolling, and global UI zoom is
an application-shell capability rather than page-specific behavior.

## 2026-08-22 — Project configuration boundaries

Template selection belongs to Setup and may be disabled. Project Type is
derived from selected capabilities rather than manually configured. Academic
context is an independent project dimension. The managed project path and the
ARAMF project-file path remain separate concepts.

## 2026-08-22 — AI configuration boundaries

AI identity, responsibilities, autonomy permissions, and ARAMF integration are
separate concerns in both the UI and model. Primary and supporting agents use
stable catalog IDs, and high-risk autonomy actions remain explicit.

## 2026-08-22 — Scoped sources of truth

Resource inventory, resource authority, and AI resource policy are separate
concerns. Multiple sources of truth are valid when they govern different
scopes. Project-local copied resources, when implemented, belong under
`ARAMF/resources/` and must remain distinct from referenced resources.

## 2026-08-22 — Developer-controlled startup placement

MainWindow physical screen selection and startup width/height are developer
controlled from `src/main.cpp`, not user or project configuration. Normal Qt
window movement remains sufficient at runtime.
