<!-- AGENTS.md -->

# Canonical ARAMF Agent Instructions

## Required startup order

1. Read `PROJECT_STATUS.md`.
2. Read `memory/decisions.md`.
3. Read `rules/generated-rules.md`.
4. Load only task-relevant files from `routing/`, `resources/`, `platforms/`, `verification/`, or `docs/`.
5. Treat `custom/` as user-owned content. Never modify it automatically.

## Architecture contract

- ARAMF itself is a C++17 / Qt 6 desktop application.
- Runtime framework logic must be implemented in C++. Do not add Python, Node.js, shell, PowerShell, or another runtime as an application dependency when C++ can own the behavior.
- ARAMF may generate rules and configuration for projects written in other languages. Target-project language support must not be confused with ARAMF's implementation language.
- Keep application source under `src/`, tests under `tests/`, and AI-facing control-plane files under `ARAMF/`.
- The canonical project-local control directory generated for managed projects is `ARAMF/`, uppercase.
- A root `AGENTS.md` may exist only as a minimal bootstrap into `ARAMF/AGENTS.md`. It must not become a second rule store.

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

## Ownership and safety

- Do not automatically overwrite files under `custom/`.
- Preserve foreign root `AGENTS.md` files instead of silently replacing them.
- Prefer atomic writes for managed state.
- Keep generated state deterministic and portable between AI agents.

## Scope

All paths referenced by this file are relative to the `ARAMF/` directory unless explicitly stated otherwise. Do not depend on AI rule, memory, status, or routing files outside this directory.
