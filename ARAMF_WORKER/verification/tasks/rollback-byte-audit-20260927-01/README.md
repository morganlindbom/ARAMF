# Phase 2A rollback audit: STOP before implementation

User-authorized Phase 2A only. HEAD: cb4ba92048c30a3fd8a5c41014dbfab0d54d3a12.

Stop condition reached: rollback snapshots do not contain sufficient original
bytes for all files actually changed by the governed decision operation.

## Read-only product-source findings

- ProjectMemory.cpp snapshotFiles (line 202) captures QByteArray via raw QFile
  ReadOnly/readAll. Snapshot storage is an in-process QList<FileSnapshot>.
- restoreSnapshots (line 218) delegates snapshot.contents to writeTextFile,
  whose QSaveFile WriteOnly|Text mode at line 53 expands CRLF to CRCRLF on
  Windows. No JSON serialization or QString conversion occurs in this restore
  path. The separate restoreSnapshot helper already uses binary WriteOnly.
- recordDecision snapshots decisions, event-log, manifest, current-state and
  consistency-validation (line 963). supersedeDecision uses the same five-path
  set (line 1048). Neither captures metrics or cold-start-validation.
- EvidenceStorage::appendEvidence (line 302) appends the event and updates
  manifest, metrics (activeEvents and totalEventsCreated) and current-state.
  recordDecision then generates cold-start validation before final validation.
  Rejection restores only the five captured paths, leaving metrics and the
  cold-start binding changed. A binary writer fix alone cannot correct this.
- recordCheckpoint captures cold-start but not metrics (line 1153), although
  its append also updates metrics. recordOperation captures event-log,
  manifest, current-state, metrics, consistency, cold-start and project status
  (line 1305); its failure branches use the same text-mode batch restore.
- Snapshot capture also silently leaves an empty QByteArray if opening an
  existing file fails (line 211). This is a separate fail-closed concern found
  by inspection, not a reproduced unreadable-file failure in this audit.

## Isolated reproduction

The temporary audit helper uses the actual existing ProjectMemory production
API linked to existing core objects. It does not modify product source.
Fixture: build/rollback-coverage-audit-DRGamO (retained, no automatic cleanup).

1. Initialize isolated project memory and record one current decision.
2. Observe two events; canonical read-only memory validation is PASS.
3. Capture raw bytes externally for all seven affected files.
4. Attempt another current decision on the same topic; governance rejects it.
5. Compare physical bytes and parsed state without repair or regeneration.

Observed results in isolated-observation.json:

- Operation rejected, original ordered event objects unchanged (2 -> 2).
- All five captured files lose byte identity through text-mode rollback.
- Unsnapshotted metrics change activeEvents 2 -> 3 and totalEventsCreated
  2 -> 3, even though the rejected event no longer exists.
- Unsnapshotted cold-start-validation retains a changed fingerprint.
- Canonical memory validation becomes FAIL: cold-start-fresh.

The helper compiled successfully and the audit command exited 0, meaning the
observation was recorded successfully, NOT that rollback passed. Exact raw
before/after files and audit.json remain in the retained fixture. The helper
source and response file remain under ignored build/rollback-coverage-audit.*.

## Required next authorization

Authorize a narrow extension of Phase 2A to include complete snapshot coverage
for every file mutated by the decision/supersession/checkpoint paths, and
fail-closed capture handling, in addition to binary restoration. This does not
require configuration propagation, named-worker changes, HVD configuration or
Foundation coupling. No such implementation has been performed here.

## Preserved boundaries

Live event history remains 482 events, 482 unique IDs, sequence 1-482. No live
recorder or cold-start regeneration was invoked. No tracked product source was
edited, staged or committed. Existing named-worker work is untouched. Phase 1
evidence remains unchanged and still states RECONSTRUCTED, not exact complete
pre-rollback restoration. No Phase 2B, HVD, F/P/S/C, P6 or C1 work started.
