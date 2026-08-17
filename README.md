# AR&MF Reconstruction Knowledge Pack

Purpose: preserve all currently recovered information useful for reconstructing **AI Rules & Memory Framework (AR&MF / ARAMF)** after the 2026-08-15 data-loss incident.

## Evidence policy

- **Verified implementation**: backed by surviving source, recovered diffs, Codex completion output, or exact memory/event artifacts.
- **Verified historical state**: backed by exact `aramf/memory/*` data or exact logs.
- **Reconstruction inference**: a best-fit conclusion from multiple diffs where intermediate and final lines are mixed. These are explicitly labelled.
- **Open gap**: information known to have existed but not yet recovered exactly.

Do not silently convert an instruction/proposal into implemented fact. Do not fabricate missing history.

## Files

- `01-architecture-and-product-state.md` — product identity, core architecture, workflow, templates, resources and major decisions.
- `02-project-memory.md` — Project Memory architecture, schema, consistency validation, cold-start, metrics and durable/production semantics.
- `03-improvement-backlog.md` — canonical TODO-001..008 implementation history and final state.
- `04-reconstruction-ledger.md` — chronological post-Aug-9 reconstruction ledger and confidence.
- `05-windows-developer-environment.md` — Windows/Qt/MinGW runtime deployment, VS Code and developer conveniences.
- `06-external-project-validation.md` — fullstack Node/Express/SQLite AR&MF validation and security/project configuration facts.
- `07-cross-project-lessons.md` — reusable lessons from Pico certification, automation and the cleanup/data-loss incident.
- `08-recovered-source-snippets.md` — reconstruction-grade source snippets recovered from Codex diffs.

## Surviving source baseline

Authoritative reconstruction baseline currently identified:

- `ARAMF_review_source_2026-08-09(1).zip` — later Aug 9 source snapshot; use as **R0**.
- `AI Project Rule Framework.zip` — older Aug 7–8 snapshot.

R0 contained real Qt/C++ source including `ProjectModel`, `RuleCatalog`, `RuleResolver`, `ContextRouter`, `RuleGenerator`, `MemoryManager`, resource management, templates and four integration-test executables.

## Raw cross-project evidence

- `evidence/pico/recovered-event-log-fragments.jsonl` — exact recovered Pico events useful for AR&MF validation semantics.
