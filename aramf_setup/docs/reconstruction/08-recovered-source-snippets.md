# Recovered AR&MF Source Snippets

These are reconstruction-grade snippets recovered from Codex diffs. Combined diffs sometimes contain old/new duplicate lines; use verified final behavior.

## `AramfPaths.h`

```cpp
inline const QString ColdStartValidationFile = QStringLiteral("aramf/memory/cold-start-validation.json");
inline const QString MemoryConsistencyValidationFile = QStringLiteral("aramf/memory/memory-consistency-validation.json");
inline const QString CertificationKnowledgeFile = QStringLiteral("aramf/memory/certification-knowledge.jsonl");
```

## `MemoryManager.h`

```cpp
bool generateColdStartValidation(QString* errorMessage = nullptr);
bool validateMemoryConsistency(QString* errorMessage = nullptr);
bool recordAutomationDecision(const QJsonObject& decision,
                              QString* errorMessage = nullptr);
```

## Current-state durable/production classification

```cpp
qint64 latestDurableSequence = 0;
qint64 latestProductionSequence = 0;
QString latestProductionEventId;

for (const DevelopmentEvent& event : events)
{
    latestDurableSequence = qMax(latestDurableSequence, event.sequenceNumber);
    const bool productionRelevant = event.eventType != EventType::PROJECT_MEMORY_ACTIVATED
        && event.eventType != EventType::PROJECT_CONTEXT_CHANGED
        && event.eventType != EventType::DECISION_RECORDED
        && event.eventType != EventType::CHECKPOINT_CREATED;

    if (productionRelevant && event.sequenceNumber >= latestProductionSequence)
    {
        latestProductionSequence = event.sequenceNumber;
        latestProductionEventId = event.eventId;
    }
}
```

Generated sections:

```text
## Latest Durable Sequence
...

## Latest Production Development Event
...

## Latest Production Sequence
...
```

## Memory-consistency invariants

Recovered core invariants:

```cpp
manifest.nextSequenceNumber == maxSequence + 1
manifest.entryCount == workRecords.size()
manifest.eventCount == eventRecords.size()
```

`DECISION_RECORDED.decisionId` must resolve to a durable decision.

Checkpoint references must resolve to an existing event where supplied and a valid sequence.

Cold-start `durableSequence`, when present, must match current durable sequence.

## Required test proof

```text
Valid memory                  PASS
Stale current-state           FAIL
Regenerate current-state      PASS
Bad manifest sequence         FAIL
Durable=2 / Production=1      PASS
Broken decision reference     FAIL
Broken checkpoint reference   FAIL
Malformed JSONL               FAIL
```
