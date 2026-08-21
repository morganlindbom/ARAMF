# AR&MF Project Memory

## Core principle

**Append-only history is authoritative; current state is derived/regenerated.**

Files:

- `work-log.jsonl` — WHAT happened / work entries.
- `event-log.jsonl` — durable development/event history.
- `decisions.md` — durable architectural/product decisions.
- `checkpoints.json` — explicit known/stable states.
- `current-state.md` — compact derived snapshot.
- `metrics.json` — derived metrics.
- `memory-manifest.json` — counters/index/current pointers.
- `cold-start-validation.json` — deterministic cold-start validation artifact.
- `memory-consistency-validation.json` — deterministic memory consistency report.

Recovered Pico memory confirms `memoryVersion = "2"` in production usage.

## Auto activation

Later implementation automatically activates memory for a valid project folder:

- ensure `aramf/memory/`;
- create/load manifest;
- append activation event;
- regenerate current state;
- generate cold-start validation;
- run memory consistency validation;
- avoid creating invalid paths while a user is still typing.

Recovered `MainWindow.cpp` lifecycle includes:

```cpp
memory.generateColdStartValidation(&errorMessage);
memory.validateMemoryConsistency(&errorMessage);
activeMemoryProjectFolder_ = projectFolder;
```

## Durable sequence vs production sequence

This distinction is central.

Recovered `generateCurrentState()` logic treats these event types as control-plane/non-production-relevant:

- `PROJECT_MEMORY_ACTIVATED`
- `PROJECT_CONTEXT_CHANGED`
- `DECISION_RECORDED`
- `CHECKPOINT_CREATED`

Current state includes:

```text
## Latest Durable Sequence
<sequence>

## Latest Production Development Event
<event id>

## Latest Production Sequence
<sequence>
```

Therefore `durableSequence` may legitimately be greater than `productionSequence`.

A required regression test proves:

```text
durableSequence = 2
productionSequence = 1
```

is valid when sequence 2 is a control-plane event.

## Memory consistency validator

Recovered public API:

```cpp
bool generateColdStartValidation(QString* errorMessage = nullptr);
bool validateMemoryConsistency(QString* errorMessage = nullptr);
bool recordAutomationDecision(const QJsonObject& decision,
                              QString* errorMessage = nullptr);
```

Recovered path constants:

```cpp
inline const QString ColdStartValidationFile =
    QStringLiteral("aramf/memory/cold-start-validation.json");
inline const QString MemoryConsistencyValidationFile =
    QStringLiteral("aramf/memory/memory-consistency-validation.json");
inline const QString CertificationKnowledgeFile =
    QStringLiteral("aramf/memory/certification-knowledge.jsonl");
```

### Report shape

```json
{
  "status": "PASS|FAIL",
  "checkedAt": "...",
  "durableSequence": 0,
  "productionSequence": 0,
  "checks": [],
  "errors": [],
  "warnings": []
}
```

Each check records name/status and may record expected/actual/message.

### Structural checks

Recovered checks include:

- manifest JSON parsing;
- JSONL parsing line-by-line;
- required identifiers;
- unique IDs;
- `sequenceNumber > 0`;
- duplicate sequence detection;
- monotonic sequence order;
- manifest `nextSequenceNumber == maxSequence + 1`;
- manifest work/event counts match parsed append-only history;
- manifest latest event/entry resolve;
- work-entry duration validity;
- timestamp order (`startedAt <= completedAt`).

Duration convention in recovered validator:

- `-1` = unknown;
- `>= 0` = valid;
- `< -1` = invalid.

Fields checked include:

- `durationSeconds`
- `humanActiveSeconds`
- `aiActiveSeconds`
- `autonomousExecutionSeconds`
- `waitingSeconds`
- `diagnosisSeconds`

### Cross references

Recovered validation includes:

- Decision `relatedEvent` must resolve when non-empty/non-`None`.
- `DECISION_RECORDED.decisionId` must resolve to a durable decision.
- Decision IDs must be present/unique.
- Manifest decision count must agree when used.
- Checkpoint `latestDevelopmentEventId` must resolve when present.
- Checkpoint sequence must be valid.
- Current-state durable/production sequence and latest production event must match authoritative history.
- Metrics artifact must parse.
- Optional context-reuse, automation-decision and certification-knowledge JSONL artifacts are parsed when present.
- Cold-start validation, when it records `durableSequence`, must describe the current durable sequence.

### Required deterministic tests

Recovered `MemoryIntegrationTests::testMemoryConsistencyValidationScenarios()` covered:

1. valid memory → PASS;
2. stale `current-state.md` → FAIL;
3. regenerate current state → PASS;
4. incorrect manifest durable sequence → FAIL;
5. legitimate control-plane divergence → PASS;
6. broken decision reference → FAIL;
7. broken checkpoint reference → FAIL;
8. malformed JSONL → FAIL.

Final validation evidence:

- Release build PASS;
- `MemoryIntegrationTests`: 18/18 PASS;
- full integration suite: 4/4 PASS;
- 100% tests passed.

## Metrics and efficiency

Implemented metrics distinguish:

- human-active time;
- AI-agent-active time;
- autonomous local execution time;
- tool/build execution;
- waiting time;
- diagnosis/corrective-action time;
- iterations;
- build attempts;
- test attempts;
- failures;
- corrective actions.

Critical rule: **total elapsed time must not be interpreted as AI-active time**.

Context-reuse measurements are separate. No efficiency gain is inferred without comparable measurements.

## Failure domains / work categories

Recovered vocabulary includes:

- `PRODUCT`
- `VALIDATION_INFRASTRUCTURE`
- `TEST_INFRASTRUCTURE`
- `CONTROL_PLANE`
- `MEMORY_SYSTEM`
- `TOOLCHAIN_ENVIRONMENT`
- `DATA_OR_CLASSIFICATION`
- `RESOURCE_OR_CONFIGURATION`
- `EXTERNAL_DEPENDENCY`
- `OTHER`

## Certification knowledge

Reusable certification records include a compatibility contract and certification level. Candidate contract fields include:

- implementation hash;
- interface hash;
- generator hash;
- target architecture/platform;
- SDK/toolchain version;
- dependencies;
- build assumptions;
- validation paths;
- certification level;
- evidence hash;
- timestamp;
- superseded state.

Three conceptual levels:

1. project-specific certification;
2. certified reusable component/module;
3. generalized Framework Knowledge.

Project-specific PASS must not automatically transfer to another project.

## Framework Knowledge proposal

Future Finalize design idea (not proven implemented): create portable `framework-knowledge.json` containing generalized, user-approved lessons.

Promotion path:

```text
Project Evidence
→ Supported Lesson
→ Generalization
→ Review
→ User Approval
→ Framework Knowledge
```

This knowledge must never override explicit current user instruction or current project Source of Truth/durable decisions.
