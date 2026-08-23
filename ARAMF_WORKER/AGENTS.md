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

## Scope

This file is the live self-hosted project control plane for the repository.
Product implementation and bootstrap source remains under `../aramf_setup/`;
do not treat it as a second live project status or memory authority. Generated
project paths remain relative to their `ARAMF_WORKER/` control directory.

<!-- ARAMF-MEMORY-BEGIN -->

## Project Memory Feedback

Read `memory/memory-contract.json` before recording development results. Do not edit ProjectMemory-owned bookkeeping files directly. Use `aramf memory record --project <project-root> --operation <operation> ...`.
- Record task starts/completions, build results, test results, and validation outcomes when configured.
- Record durable decisions only for genuine architecture or policy choices through the decision workflow.
- Record a checkpoint only for a genuine stable recovery point with `aramf memory checkpoint --project <project-root> --title <title> --summary <summary>`; routine feedback does not create one.
- Framework Knowledge has separate built-in, global, and project-local layers. The global user library is under `ARAMF_DATA/` at the resolved ARAMF program root; build directories are disposable. Promote an explicitly approved portable entry only through `aramf memory knowledge promote --project <project-root> --id <knowledge-id>`. Never edit the library or project knowledge files directly.
- UPDATE is a deliberate human-controlled workflow: review approved knowledge, analyze the whole project, prepare a plan, and explicitly execute it through the configured agent. The managed project root is the implementation target; `ARAMF_WORKER/` is orchestration only. `READY_FOR_EXTERNAL_AGENT` is an incomplete handoff, not completion; actual project changes and validation are required. Read `update/update-plan.json` and `update/update-contract.json` when present. Candidates are never active.
- When work reveals a deficiency in ARAMF's own canonical workflow, rules, representations, validation, resource model, or control plane, report it with `aramf improvement report --project <project-root> --title <title> --observation <observation> ...` and continue the managed project when safe. This creates an observation only; it does not create a TODO, durable decision, or Framework Knowledge. Do not report ordinary project bugs as ARAMF gaps. Use `aramf improvement list` to inspect the global backlog.
- Run the minimum validation required by `routing/validation-policy.json`; do not run full regression campaigns for ordinary isolated changes. Escalate when scope, risk, failure, or explicit milestone policy requires it.
- Follow current durable decisions; explicitly superseded decisions remain historical and inactive.

The recorder owns event IDs, timestamps, sequences, metrics, pruning, validation, and current-state pointers.

<!-- ARAMF-MEMORY-END -->
