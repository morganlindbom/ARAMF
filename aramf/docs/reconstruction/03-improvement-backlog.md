# Canonical AR&MF Improvement Backlog — Recovered State

The backlog is append-only. Older `Observed` / `Confirmed` statuses remain historical evidence; final current status is separate.

## Final state

| TODO | Title / purpose | Final status |
|---|---|---|
| TODO-001 | Detailed efficiency/time metrics | Completed |
| TODO-002 | Selective durable decision capture | Completed |
| TODO-003 | Memory Consistency Validation | Completed |
| TODO-004 | Automatic cold-start validation | Completed |
| TODO-005 | Context reuse measurement | Completed |
| TODO-006 | Failure-domain/work-category classification | Completed |
| TODO-007 | Reusable certification knowledge | Completed |
| TODO-008 | Agent-to-automation offloading | Completed |

## Implementation wave — 2026-08-11

Recovered history records TODO-001, 002, 004, 005, 006, 007 and 008 as implemented + integration-tested first. TODO-003 initially remained `Observed` pending the complete deterministic contract.

### TODO-001

- human-active, AI-active, autonomous-execution, waiting and diagnosis/corrective-action time;
- iteration/build/test/failure/corrective-action counters;
- elapsed time is not AI-agent usage.

### TODO-002

- selective durable decision capture;
- `DECISION_RECORDED` event linked to durable decision.

### TODO-004

- automatic `aramf/memory/cold-start-validation.json`.

### TODO-005

- separate context-reuse measurement records;
- no inferred efficiency improvement without comparable measurements.

### TODO-006

- failure-domain and work-category fields in durable development events.

### TODO-007

- certification knowledge records with compatibility contract and certification level;
- project evidence remains distinct from generalized Framework Knowledge.

### TODO-008

- separate automation/offloading decision records;
- classification of reasoning requirement vs deterministic/verifiable work;
- explicit verification requirements.

Validation for this implementation wave:

- Release build PASS;
- integration tests 4/4 PASS;
- 100% tests passed.

Main implementation areas recovered from Codex history:

- `MemoryManager.cpp/.h`
- `MemoryMetrics.cpp`
- `AramfPaths.h`
- `MainWindow.cpp`
- `MemoryIntegrationTests.cpp`

## TODO-003 final completion

Completed only after deterministic positive/negative scenarios passed.

Implementation:

- `MemoryManager::validateMemoryConsistency()`;
- output `aramf/memory/memory-consistency-validation.json`;
- structural validation;
- cross-reference validation;
- semantic durable/production validation;
- stale derived state detection;
- corruption detection;
- legitimate control-plane divergence handling.

Final evidence:

- Release build PASS;
- `MemoryIntegrationTests` 18/18 PASS;
- full suite 4/4 PASS;
- stale current-state detected;
- regeneration restored PASS;
- invalid manifest sequence detected;
- broken decision/checkpoint references detected;
- corrupt JSONL detected;
- durable 2 / production 1 control-plane divergence PASS.

## Interpretation boundary

`Completed` means implemented and integration-tested. It does **not** automatically mean long-term validation across many real projects.

The Pico 2 W Visual Designer certification campaign is empirical evidence for this backlog and must not be treated as hypothetical design material.
