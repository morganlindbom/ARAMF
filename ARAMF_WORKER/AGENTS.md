<!-- AGENTS.md -->

# Canonical ARAMF Agent Instructions

## Mandatory startup router

1. Read `project.json` to identify the project and stable capabilities.
2. Read `worker-manifest.json` to resolve canonical ownership and the cold-start read set.
3. Read `memory/current-state.md` and the latest validation summary.
4. Determine the task scope, then follow `routing/task-routes.json` and `routing/scope-routes.json`.
5. Read only the selected scope's instructions/resources; defer `memory/event-log.jsonl` unless history is explicitly required.

6. For contextual retrieval, read `context/context-index.json` first; `compressed-context.json` is a derived, provenance-preserving shortcut. Check `context/freshness.json` before using compressed context or decisions. Task dependencies use `context/task-dag.json`; handoffs preserve the originating TaskContract and cannot expand scope. Agent adapters change presentation only and never governance.

Before scoped AI edits, derive a task contract with `aramf task prepare --config <saved-project> --request <task.json>`. Continue only from READY or READY_WITH_WARNINGS; unmapped source files require canonical scopeMetadata. After edits, use `aramf task postflight --config <saved-project> --contract <prepared.json> --evidence <evidence.json>` and satisfy its evidence requirements before claiming VERIFIED. Task contracts are derived views, not independent authority.

Read `PROJECT_STATUS.md` and `memory/decisions.md` when the task requires current project detail or durable architectural context.
<!-- ARAMF-TASK-GOVERNANCE-BEGIN -->

## Governed Task Execution Contract

Every governed task follows ANALYZE -> PREPARE -> EXECUTE -> VALIDATE. ANALYZE resolves the applicable scope and dependencies without mutating the project. PREPARE produces a READY TaskContract with exact permitted files/resources, ownership, canonical producers, ChangeImpact, ValidationRouting, and required evidence; PREPARE must not perform Execute. EXECUTE is the only implementation mutation boundary. VALIDATE performs postflight and evidence checks before completion.
Modify only files and resources explicitly permitted by the TaskContract. Unmapped files, path traversal, protected files, user-owned Sources of Truth, and ownership conflicts are blocked. Permission to write generated output never overrides its canonical producer or resource ownership. Generated/service-owned files must be produced or repaired by their authoritative ARAMF service, not recreated manually.
Project isolation is mandatory: preserve pre-existing dirty and unrelated files, exclude them from current-task attribution, and fail on new out-of-scope edits. Never broaden a task to the whole project or full ARAMF_WORKER because precise scope resolution is inconvenient. Use declared ChangeImpact and dependency scope to determine affected validation and evidence. Evidence is fresh only for the dependencies it covers; later relevant changes stale that evidence.
Follow the authoritative route in `routing/validation-policy.json`. VERIFIED requires all applicable valid software evidence and fresh fingerprints. CERTIFIED is a separate claim requiring its applicable certification evidence; software verification must not imply physical certification. HARDWARE_CERTIFIED or other physical claims require valid physical/on-target evidence and must never be fabricated.
Persist governed state through the canonical ARAMF services, save/reload it, and verify readback and cross-file consistency. Governance events use the append-only recorder and its current-state, manifest, metrics, PROJECT_STATUS, memory-consistency, and cold-start mechanisms; do not invent recorder files or rewrite history. Keep the generated Worker topology coherent and treat `ARAMF_WORKER/` as orchestration while the managed project root remains the implementation target.
<!-- ARAMF-TASK-GOVERNANCE-END -->
Read `memory/framework-knowledge.json` and apply only entries whose status is `approved`.
Approved Framework Knowledge is live: it applies immediately in this project without regeneration.
Read `rules/generated-rules.md` when rule output is present.

Respect Sources of Truth, durable decisions, and the user-owned `custom/` directory.
Project resources have explicit governance roles in resources/resources.json: source-of-truth is authoritative project fact/requirement, instruction is a directive within its authority and scope, reference is informational and does not override governing sources, and supporting-material is contextual with lower governance authority. Ignore disabled resources. Do not infer roles from filenames or file types. If active instructions conflict at equal effective authority, surface the conflict and require governance resolution; never silently choose or merge them.
Thesis Template and Report Template are distinct canonical resource roles. Thesis and Report may both be enabled. Use the ARAMF Default Thesis Template or ARAMF Default Report Template automatically when their document is enabled without a custom source; custom sources must resolve by resource ID and matching role from resources/resources.json.
Authority order: explicit current user instruction, current Source of Truth, current durable project decisions, approved Framework Knowledge, templates/defaults, then AI inference.
When a corrected approach is verified and reusable, record a Framework Knowledge candidate with evidence. Never self-approve it; explicit user approval is required before changing its status to `approved`. Superseded entries remain auditable but are not active.
Keep PROJECT_STATUS.md current as human-readable present state; it is distinct from append-only historical evidence. Project Memory ownership is explicit in memory/memory-contract.json.
The generated control directory is `ARAMF_WORKER/`.
When communication-contract.json is present, it is the canonical communication Source of Truth. Do not invent message IDs, rename contract fields, change logical field types or protocol versions independently, or change wire encoding for only one endpoint; update the shared contract first and run compatibility validation for every affected endpoint.
Communication commands use symbolic hardware resource IDs. Resolve physical pins only through `hardware/hardware-resources.json`; validate endpoint ownership and capabilities, and never invent or access arbitrary numeric GPIOs in communication code.
Framework Knowledge has distinct built-in, global, and project-local layers. The global user library is stored under `ARAMF_DATA/` at the resolved ARAMF program root; build directories are disposable. Only explicitly approved portable knowledge may be promoted there; use the memory knowledge promotion command and never edit knowledge stores directly. New projects seed approved global knowledge without replacing project-local authority.
UPDATE is a separate human-controlled workflow: review approved Framework Knowledge, analyze the whole project, prepare a plan, then explicitly execute it through the configured agent. Read `update/update-plan.json` and `update/update-contract.json` when present; the managed project root is the implementation target and `ARAMF_WORKER/` is orchestration only. `READY_FOR_EXTERNAL_AGENT` is an incomplete handoff, not completion; actual project changes are required. Preserve higher-authority instructions and use the scope-aware validation policy.

## Governed Project Resources

- `test_250` — role `supporting-material`, authority `supporting-reference`, scope `architecture, software, build-system, testing`, enabled `yes`; resolve the source from `resources/resources.json`.
- `test_550` — role `supporting-material`, authority `supporting-reference`, scope `testing`, enabled `yes`; resolve the source from `resources/resources.json`.
- `CMakeLists.txt` — role `source-of-truth`, authority `primary-source-of-truth`, scope `software, build-system`, enabled `yes`; resolve the source from `resources/resources.json`.
- `info.md` — role `supporting-material`, authority `supporting-reference`, scope `all`, enabled `yes`; resolve the source from `resources/resources.json`.
- `aramf_setup` — role `supporting-material`, authority `authoritative`, scope `software`, enabled `yes`; resolve the source from `resources/resources.json`.
- `src` — role `source-of-truth`, authority `primary-source-of-truth`, scope `architecture, software`, enabled `yes`; resolve the source from `resources/resources.json`.
- `info.md` — role `supporting-material`, authority `supporting-reference`, scope `all`, enabled `yes`; resolve the source from `resources/resources.json`.
- `ARAMF_WORKER` — role `source-of-truth`, authority `primary-source-of-truth`, scope `project-requirements, current-project-state`, enabled `yes`; resolve the source from `resources/resources.json`.
- `tests` — role `supporting-material`, authority `authoritative`, scope `testing`, enabled `yes`; resolve the source from `resources/resources.json`.
- `AGENTS.md` — role `source-of-truth`, authority `primary-source-of-truth`, scope `project-requirements, architecture, software, testing, current-project-state`, enabled `yes`; resolve the source from `resources/resources.json`.

## Documentation Template Routing

Academic project types are independently selected: none. Do not treat them as mutually exclusive.
Selected document languages: sv. Produce a separate version of each enabled document in each selected language; do not combine languages into one bilingual document. Follow the variantId and language of each entry in the documentation manifest.
- Thesis: enabled=no, templateMode=aramf-default, templateSourceId=ARAMF default, instruction=aramf-thesis-instruction/v1; ARAMF built-in structure is active. Preserve the original source, apply only this document-type instruction, and treat guidance as authoring assistance rather than final prose.
- Report: enabled=no, templateMode=aramf-default, templateSourceId=ARAMF default, instruction=aramf-report-instruction/v1; ARAMF built-in structure is active. Preserve the original source, apply only this document-type instruction, and treat guidance as authoring assistance rather than final prose.
Thesis never uses the Report instruction; Report never uses the Thesis instruction. Custom template structure remains custom and external instructions retain their governed authority.
The canonical built-in section hierarchy and bilingual authoring guidance are in `documentation/documentation-manifest.json` when documentation is enabled. Guidance is authoring assistance, not final document prose.
Run the minimum validation required by `routing/validation-policy.json`; do not run full regression campaigns for ordinary isolated changes. Escalate when scope, risk, failure, or explicit milestone policy requires it.
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
