<!-- decisions.md -->

# Durable Decisions

## 2026-08-21 — ARAMF implementation language

ARAMF itself is a C++17 / Qt 6 desktop application. Runtime framework behavior, project memory, generation, validation, and control-plane management must be implemented in C++ unless an explicit later decision changes this architecture.

Target projects managed by ARAMF may use other languages and frameworks. Supporting those project types does not introduce those runtimes as ARAMF dependencies.

## 2026-08-21 — Canonical control directory

All project-local files intended to provide rules, memory, status, routing, resources, templates, verification context, or related AI-facing state belong under an uppercase `ARAMF_WORKER/` directory.

A repository-root `AGENTS.md` may exist as a minimal discovery/bootstrap file pointing to `ARAMF_WORKER/AGENTS.md`. It must not duplicate the canonical rule set.

## 2026-08-21 — Project status ownership

`ARAMF_WORKER/PROJECT_STATUS.md` is the canonical current program/project snapshot and must be updated by the active coding agent after meaningful implementation work. It records current capabilities, implemented changes, verified state, known issues, and next work; historical events remain separate.

## 2026-08-21 — User-owned custom content

Files under `ARAMF_WORKER/custom/` are user-owned and must not be modified automatically by generation, migration, validation, or agent workflows.

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
`ARAMF_WORKER/resources/` and must remain distinct from referenced resources.

## 2026-08-22 — Developer-controlled startup placement

MainWindow physical screen selection and startup width/height are developer
controlled from `src/main.cpp`, not user or project configuration. Normal Qt
window movement remains sufficient at runtime.

## 2026-08-22 — Rules and memory policy boundaries

Rules selection and rules routing are separate concerns, and rules are modeled
as grouped capabilities rather than table matrices. Memory capture and memory
maintenance are separate concerns, and memory configuration uses selectable
policies plus one maximum-size setting rather than status-heavy UI. ARAMF
project memory defaults to a finite 10 GiB limit. Before writes,
`ProjectMemory` automatically prunes the oldest eligible history toward 90% of
the limit. Protected current state, durable decisions, source-of-truth
references, project status, consistency metadata and active configuration are
never automatically deleted; archived memory remains subject to the total
storage ceiling.

## 2026-08-22 — Generation boundaries and explicit products

Generate is responsible only for producing the ARAMF control plane under the
managed `ProjectModel::projectPath()`. The UI maps each output checkbox to a
core `GenerationOptions` value, and `GenerationServices` is the sole
orchestrator of selected product writers. ProjectModel remains the authoritative
generation input. Unselected existing output is preserved rather than deleted.
Save, build, test, deploy, Verify, and Finalize remain separate operations.

## 2026-08-22 — Save before Generate

The Generate user action is an atomic-feeling sequential workflow: preflight,
save the current ProjectModel through the existing ProjectPersistence/Setup
Save As path, then generate from that same in-memory state. Save failure or
Save As cancellation prevents generation; a generation failure does not roll
back a successful save. ProjectPersistence and GenerationServices remain
separate responsibilities, and `projectFilePath` remains separate from the
managed `projectPath`.

## 2026-08-22 — Generate lifecycle boundaries

The output workflow is a four-stage lifecycle: Review is read-only, Generate
writes only selected ARAMF control-plane products, Verify performs
non-destructive content and JSON validation without auto-regeneration, and
Finalize records completion only after a current successful verification and
memory validation. Generation and verification share a deterministic
configuration fingerprint so changes to relevant ProjectModel state invalidate
previous results. Finalize is idempotent for an unchanged fingerprint and does
not build, deploy, publish, commit, or push. `ARAMF_WORKER/custom/` remains user-owned.

## Repository and generated-control path distinction

The repository-local ARAMF development control material is stored under
`aramf_setup/`. The generated control plane for a managed target project is
stored under that target's uppercase `ARAMF_WORKER/` directory. Repository bootstrap
files may point to `aramf_setup/AGENTS.md`; generated target-project bootstrap
files must point to `ARAMF_WORKER/AGENTS.md`. These paths must never be conflated.

Generation, ProjectMemory, VerificationServices and FinalizationServices must
resolve runtime output from `ProjectModel::projectPath()` and the uppercase
`ARAMF_WORKER/` control directory only. `aramf_setup/` is repository-internal and is
never a managed-project output path.

Root-level generated AI entry files are sourced separately under
`aramf_setup/bootstrap/`. These are thin templates/references, not an
independent rule store. The generated target bootstrap is written at the
target root and points into that target's uppercase `ARAMF_WORKER/` control plane;
`bootstrap/` itself is never generated.

Every supported AI agent enters a managed project through a thin bootstrap
adapter. Root and provider-specific entry files are sourced from
`aramf_setup/bootstrap/` and converge on generated target `ARAMF_WORKER/AGENTS.md`,
which remains the canonical instruction authority. `AgentEntryPointService`
owns safe, idempotent creation and preserves user-owned content. Unsupported
agents use the generic root entry point, and provider files are not
automatically deleted when selection changes.

## 2026-08-22 — Live Framework Knowledge

Framework Knowledge is a live, evidence-backed memory layer stored at
`ARAMF_WORKER/memory/framework-knowledge.json` in managed projects and at
`aramf_setup/memory/framework-knowledge.json` for development of ARAMF itself.
Knowledge moves through `candidate` → explicit user approval → `approved`;
agents may propose candidates but must never self-approve them. Approved entries
apply immediately without rerunning ARAMF or regenerating the control plane.
Superseded entries remain auditable but inactive. Authority is explicit current
user instruction → current Source of Truth → current durable project decisions →
approved Framework Knowledge → templates/defaults → AI inference. Repeated
matching proposals are deduplicated and evidence is accumulated rather than
creating parallel lessons.

## Decision Record: legacy-framework-knowledge-location

<!-- ARAMF-DECISION -->
- Decision-ID: legacy-framework-knowledge-location
- Topic: framework-knowledge-location
- Status: superseded
- Superseded-By: self-host-framework-knowledge-location
- Summary: Historical repository setup location is retained only as historical context.
<!-- /ARAMF-DECISION -->

## Decision Record: self-host-framework-knowledge-location

<!-- ARAMF-DECISION -->
- Decision-ID: self-host-framework-knowledge-location
- Topic: framework-knowledge-location
- Status: current
- Superseded-By: none
- Summary: For ARAMF itself, live Framework Knowledge is ARAMF_WORKER/memory/framework-knowledge.json; aramf_setup remains product/bootstrap source.
<!-- /ARAMF-DECISION -->

## Decision Record: project-memory-feedback-ownership

<!-- ARAMF-DECISION -->
- Decision-ID: project-memory-feedback-ownership
- Topic: project-memory-feedback-ownership
- Status: current
- Superseded-By: none
- Summary: External agents use the ARAMF headless recorder; ProjectMemory solely owns IDs, timestamps, sequences, metrics, current-state, manifests, pruning, and consistency. Checkpoints and decisions remain deliberate separate workflows.
<!-- /ARAMF-DECISION -->

## Decision Record: proportional-validation-routing

<!-- ARAMF-DECISION -->
- Decision-ID: proportional-validation-routing
- Topic: validation-policy
- Status: current
- Superseded-By: none
- Summary: Validation is proportional to changed scope and risk: start at FOCUSED, escalate to SUBSYSTEM or FULL REGRESSION only for relevant impact, failure, uncertainty, milestone, release, migration, or explicit certification.
<!-- /ARAMF-DECISION -->

## Decision Record: framework-knowledge-update-workflow

<!-- ARAMF-DECISION -->
- Decision-ID: framework-knowledge-update-workflow
- Topic: framework-knowledge-update
- Status: current
- Superseded-By: none
- Summary: Framework Knowledge UPDATE is a deliberate human-controlled workflow: review candidates separately, analyze the whole project, apply only explicitly selected approved knowledge to affected areas, preserve higher authority, and hand off a traceable plan and contract for AI work.
<!-- /ARAMF-DECISION -->

## Decision Record: global-framework-knowledge-library

<!-- ARAMF-DECISION -->
- Decision-ID: global-framework-knowledge-library
- Topic: framework-knowledge-library
- Status: current
- Superseded-By: none
- Summary: ARAMF maintains a persistent global library of explicitly user-approved portable Framework Knowledge. New managed projects inherit approved global knowledge while project-specific and Source-of-Truth authority remains higher.
<!-- /ARAMF-DECISION -->

## Decision Record: portable-global-framework-knowledge-storage

<!-- ARAMF-DECISION -->
- Decision-ID: portable-global-framework-knowledge-storage
- Topic: framework-knowledge-library-storage
- Status: superseded
- Superseded-By: root-global-framework-knowledge-storage
- Summary: ARAMF global user-approved portable Framework Knowledge is stored in ARAMF_DATA beside the running ARAMF program so the program and its accumulated knowledge can move together. Legacy AppData data may be merged once but is never an active fallback.
<!-- /ARAMF-DECISION -->

## Decision Record: root-global-framework-knowledge-storage

<!-- ARAMF-DECISION -->
- Decision-ID: root-global-framework-knowledge-storage
- Topic: framework-knowledge-library-storage
- Status: current
- Superseded-By: none
- Summary: ARAMF global user-approved portable Framework Knowledge is stored under ARAMF_DATA at the resolved ARAMF program root. Build directories are disposable and must never own persistent ARAMF knowledge.
<!-- /ARAMF-DECISION -->

## Decision Record: decision-global-improvement-backlog

<!-- ARAMF-DECISION -->
- Decision-ID: decision-global-improvement-backlog
- Topic: Global improvement backlog boundaries
- Status: current
- Superseded-By: none
- Summary: Framework deficiencies discovered during managed-project work are recorded as global improvement backlog observations. Observation, TODO triage, durable decisions, implementation, validation, and Framework Knowledge remain separate lifecycle concepts; reporting must not automatically modify ARAMF source or create Framework Knowledge.
<!-- /ARAMF-DECISION -->

## Decision Record: top-priority-no-recursive-shell-delete

<!-- ARAMF-DECISION -->
- Decision-ID: top-priority-no-recursive-shell-delete
- Topic: destructive-cleanup-safety
- Status: current
- Superseded-By: none
- Summary: TOP PRIORITY: ARAMF agents and tools must never use recursive shell deletion for cleanup or fixture removal, including cmd.exe rmdir /s /q, rd /s /q, PowerShell Remove-Item -Recurse, Unix rm -rf, or equivalents. The 2026-08-23 incident demonstrated that malformed Windows quoting can widen scope and destroy unrelated repositories. Preserve state, verify exact targets, and use non-destructive alternatives.
<!-- /ARAMF-DECISION -->
