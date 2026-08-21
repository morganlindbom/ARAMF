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
