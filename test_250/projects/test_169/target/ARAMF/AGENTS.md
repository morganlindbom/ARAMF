<!-- AGENTS.md -->

# Canonical ARAMF Agent Instructions

Read `PROJECT_STATUS.md` and `memory/decisions.md` before project work.
Read `memory/framework-knowledge.json` and apply only entries whose status is `approved`.
Approved Framework Knowledge is live: it applies immediately in this project without regeneration.
Read `rules/generated-rules.md` when rule output is present.

Respect Sources of Truth, durable decisions, and the user-owned `custom/` directory.
Authority order: explicit current user instruction, current Source of Truth, current durable project decisions, approved Framework Knowledge, templates/defaults, then AI inference.
When a corrected approach is verified and reusable, record a Framework Knowledge candidate with evidence. Never self-approve it; explicit user approval is required before changing its status to `approved`. Superseded entries remain auditable but are not active.
Keep project status current and use project memory when configured.
The generated control directory is `ARAMF/`.
