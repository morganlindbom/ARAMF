<!-- AGENTS.md -->

# Canonical ARAMF Agent Instructions

## Required startup order

1. Read `PROJECT_STATUS.md`.
2. Read `memory/decisions.md`.
3. Read `memory/framework-knowledge.json` and apply only entries whose status is `approved`.
4. Read `rules/generated-rules.md`.
5. Load only task-relevant files from `routing/`, `resources/`, `platforms/`, `verification/`, or `docs/`.
6. Treat `custom/` as user-owned content. Never modify it automatically.

## Architecture contract

- ARAMF itself is a C++17 / Qt 6 desktop application.
- Runtime framework logic must be implemented in C++. Do not add Python, Node.js, shell, PowerShell, or another runtime as an application dependency when C++ can own the behavior.
- ARAMF may generate rules and configuration for projects written in other languages. Target-project language support must not be confused with ARAMF's implementation language.
- Keep application source under `src/`, tests under `tests/`, and product-owned
  setup/bootstrap source under `aramf_setup/`.
- The canonical project-local control directory generated for every managed
  project is `ARAMF_WORKER/`. It is distinct from this repository's
  `aramf_setup/` product/bootstrap source.
- The repository root `AGENTS.md` is a minimal bootstrap into this file.
  Generated target-project bootstrap files must point
  to their own `ARAMF_WORKER/AGENTS.md` instead.

## Project status contract

Update `PROJECT_STATUS.md` after every meaningful implementation task. Keep these sections current:

- what the program currently contains;
- what was implemented or changed;
- what has actually been verified;
- known issues or limitations;
- next concrete work.

`PROJECT_STATUS.md` is a current snapshot, not an append-only history.

## Memory contract

- Put durable architectural decisions in `memory/decisions.md`.
- Keep observations, TODOs, decisions, implementation, validation, and current state separate.
- Keep the event log append-only.
- Derived validation or current-state files may be regenerated.
- Never claim a build, test, launch, hardware, or certification result without evidence.

## Live Framework Knowledge contract

- `memory/framework-knowledge.json` is live knowledge for this repository. Approved entries apply immediately to later agent work; opening the ARAMF GUI or regenerating the control plane is not required.
- Authority order is: explicit current user instruction → current Source of Truth → current durable project decisions → approved Framework Knowledge → templates/defaults → AI inference.
- When a user correction or failed approach leads to a verified better result and the lesson is reusable, create or enrich a `candidate` entry with concise evidence. Do not silently promote observations.
- An AI agent must never self-approve a Framework Knowledge candidate. Only explicit user approval may change `status` to `approved`.
- Once approved, apply the lesson immediately when its scope is relevant. Keep `superseded` entries for auditability but do not apply them.
- Prefer updating an existing matching candidate over creating duplicates.

## Ownership and safety

- Do not automatically overwrite files under `custom/`.
- Preserve foreign root `AGENTS.md` files instead of silently replacing them.
- Prefer atomic writes for managed state.
- Keep generated state deterministic and portable between AI agents.

## TOP PRIORITY: Destructive cleanup prohibition

- Never use recursive shell deletion for cleanup or fixture removal. This
  prohibition specifically includes `cmd.exe /c rmdir /s /q`, `rd /s /q`,
  PowerShell `Remove-Item -Recurse`, Unix `rm -rf`, and equivalent commands.
- Never delete a repository, project root, `ARAMF_WORKER/`, build tree, or
  generated state to clean up temporary work. The 2026-08-23 PVD recovery
  incident proved that malformed Windows quoting can widen a deletion target
  and destroy unrelated repositories.
- Do not translate paths between shells or compose quoted destructive commands.
  Any exceptional file operation must use a narrow target with boundary
  validation and an exact resolved file list.
  If removal is genuinely required and explicitly authorized, stop and request
  confirmation of the exact resolved file list first; otherwise leave temporary
  fixtures in a uniquely named directory and report them.
- Prefer additive, recoverable, non-destructive alternatives. A cleanup task
  must never outrank preservation of source, memory, history, or control-plane
  state.

## Scope

This file is the live self-hosted project control plane for the repository.
Product implementation and bootstrap source remains under `../aramf_setup/`;
do not treat it as a second live project status or memory authority. Generated
project paths remain relative to their `ARAMF_WORKER/` control directory.

<!-- ARAMF-MEMORY-BEGIN -->

## Project Memory Feedback

Read `memory/memory-contract.json` before recording development results. In `agent-direct` mode the active coding agent is the project-local Project Memory writer; no aramf.exe, global `aramf` command, recorder daemon, or external service is required. Do not edit `memory/event-log.jsonl`, `memory/metrics.json`, `memory/current-state.md`, `memory/memory-manifest.json`, validation state, or `PROJECT_STATUS.md` bookkeeping fields outside the governed protocol. Identify canonical targets, read current files and schemas, preserve unrelated state, perform the narrowest valid mutation, write the existing schema, reload from disk, parse/validate, and verify uniqueness, ordering, and cross-file consistency.
`memory/event-log.jsonl` is append-only historical evidence: preserve failed attempts, successful corrections, and their original IDs, sequences, timestamps, ordering, and PASS/FAIL results. Never rewrite prior events or regenerate history from current state.
- Record meaningful task starts and completions.
- Record completed build attempts and their PASS/FAIL result.
- Record completed test attempts and their PASS/FAIL result.
- Record meaningful validation outcomes.
- Let ProjectMemory refresh current-state from accepted events.
- Allow meaningful completed tasks to update PROJECT_STATUS through the recorder policy.
- Record a checkpoint only when an actual stable checkpoint is warranted.

- Record durable decisions only for genuine architecture or policy choices through the decision workflow.
- Follow current durable decisions; explicitly superseded decisions remain historical and inactive.

The active agent owns governed writes in `agent-direct` mode. `PROJECT_STATUS.md` and current-state files describe current truth; `memory/event-log.jsonl` preserves historical truth. Corrections and durable decision changes are represented as new evidence with explicit supersession.

<!-- ARAMF-MEMORY-END -->
<!-- ARAMF-P1-CONTEXT-BEGIN -->

## Governed Context and Task Coordination

Read `ARAMF_WORKER/context/context-index.json` first for relevant governed context. `ARAMF_WORKER/context/compressed-context.json` is derived and must retain provenance; check `ARAMF_WORKER/context/freshness.json` before relying on it. Use `ARAMF_WORKER/context/task-dag.json` for dependency gating and preserve the originating TaskContract in handoffs. Agent adapters change presentation only and cannot change ARAMF scope, ownership, permissions, routing or validation.
<!-- ARAMF-P1-CONTEXT-END -->






























































































































































<!-- ARAMF-TASK-GOVERNANCE-BEGIN -->

## Governed Task Execution Contract

Every governed task follows ANALYZE -> PREPARE -> EXECUTE -> VALIDATE. ANALYZE resolves the applicable scope and dependencies without mutating the project. PREPARE produces a READY TaskContract with exact permitted files/resources, ownership, canonical producers, ChangeImpact, ValidationRouting, and required evidence; PREPARE must not perform Execute. EXECUTE is the only implementation mutation boundary. VALIDATE performs postflight and evidence checks before completion.
Modify only files and resources explicitly permitted by the TaskContract. Unmapped files, path traversal, protected files, user-owned Sources of Truth, and ownership conflicts are blocked. Permission to write generated output never overrides its canonical producer or resource ownership. Generated/service-owned files must be produced or repaired by their authoritative ARAMF service, not recreated manually.
Project isolation is mandatory: preserve pre-existing dirty and unrelated files, exclude them from current-task attribution, and fail on new out-of-scope edits. Never broaden a task to the whole project or full ARAMF_WORKER because precise scope resolution is inconvenient. Use declared ChangeImpact and dependency scope to determine affected validation and evidence. Evidence is fresh only for the dependencies it covers; later relevant changes stale that evidence.
Follow the authoritative route in `routing/validation-policy.json`. VERIFIED requires all applicable valid software evidence and fresh fingerprints. CERTIFIED is a separate claim requiring its applicable certification evidence; software verification must not imply physical certification. HARDWARE_CERTIFIED or other physical claims require valid physical/on-target evidence and must never be fabricated.
Persist governed state through the canonical ARAMF services, save/reload it, and verify readback and cross-file consistency. Governance events use the append-only recorder and its current-state, manifest, metrics, PROJECT_STATUS, memory-consistency, and cold-start mechanisms; do not invent recorder files or rewrite history. Keep the generated Worker topology coherent and treat `ARAMF_WORKER/` as orchestration while the managed project root remains the implementation target.
<!-- ARAMF-TASK-GOVERNANCE-END -->
