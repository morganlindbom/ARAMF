# ARAMF UPDATE validation campaign

This campaign validates the deliberate Framework Knowledge UPDATE workflow.
It is a new subsystem campaign and is independent of the frozen historical
`test_250/` and `test_550/` evidence.

The campaign covers candidate visibility, explicit approval, scope filtering,
whole-project analysis, stable plan fingerprints, higher-authority conflicts,
stale-plan rejection, explicit external-agent handoff, asynchronous Codex
execution boundaries, validation-gated completion, and separation from
Project Memory bookkeeping.

Scenario namespace: `UPDATE-001` through `UPDATE-260`.

The extension also validates the three Framework Knowledge layers: built-in
product knowledge, the persistent user-owned global ARAMF library, and
project-local knowledge. It covers explicit promotion, stable identity and
provenance, deduplication, supersession, and seeding into a fresh managed
project. The global library is stored beside the running ARAMF executable under
`ARAMF_DATA/` and is not part of the project worker or repository source tree.
Legacy AppData migration is tested as a one-time compatibility path only; it
never remains an active fallback authority.
