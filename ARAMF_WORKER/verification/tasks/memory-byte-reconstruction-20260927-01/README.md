# Documented memory byte reconstruction

RECONSTRUCTED — exact complete pre-rollback byte state was not available for all files.

## Authority and evidence gap

This is the explicitly approved revised Phase 1 reconstruction, not a claim
of exact complete pre-rollback restoration. Complete immediate pre-rollback
full-file snapshots/hashes were not captured for all five affected files.
The earlier PREPARE baseline predates additional valid validation events.
No certificate, existing evidence artifact, event, decision or timestamp was
rewritten to bridge this gap. No event was added for this reconstruction.

The decisions.md candidate matches its complete earlier PREPARE baseline
SHA-256. The first 213608 bytes of the reconstructed event log match the
earlier PREPARE historical prefix SHA-256. The remaining event-log suffix
and the three mutable state/report files rely on preserved current logical
content and the independently replayed physical transformation, not missing
historical hashes. All five files are classified as reconstruction targets.

## Before-write evidence

- `before/`: complete original physical bytes, captured before reconstruction.
- `current-state-evidence.json`: paths, sizes, SHA-256, newline distributions,
  UTF-8/BOM validation, event invariants, logical hashes and parsed JSON.
- `logical-before/`: complete logical representations, not substitutes for
  the preserved original bytes.
- `events-before.json`: parsed event objects in original order.
- `repository-before.json`: tracked and existing nonignored untracked file
  hashes, plus exhaustive protected-directory file hashes.
- `prepare-contract.bin` and `protected-baseline.bin`: unchanged copies of
  earlier evidence, explicitly not immediate pre-rollback snapshots.
- `ProjectMemory.cpp.bin` and `failed-closeout-helper.cpp.bin`: source traces
  for the failed operation and its raw-snapshot/text-restore behavior.

## Proven transformation and candidates

ProjectMemory::snapshotFiles reads raw bytes into in-process snapshots.
recordDecision snapshots exactly these five memory files. A failed append
or validation calls restoreSnapshots, which passes those raw bytes to
writeTextFile using QSaveFile WriteOnly|Text. On this Windows/Qt runtime,
that write inserts CR before each LF, turning CRLF into CRCRLF.

All five current files contained only CRCRLF line endings: respectively
332 (decisions), 482 (events), 9 (manifest), 15 (current-state), and 89
(consistency report). All were valid UTF-8 without BOM; no bare CR, bare LF
or ordinary CRLF was present. Candidates remove exactly one CR immediately
before each LF; all other bytes remain untouched. No text reserialization,
timestamp generation or historical record reconstruction is applied to them.

`candidates/` contains the prospective binary files. `qt-text-replay/`
contains output from the actual QSaveFile WriteOnly|Text API applied to each
candidate in this evidence directory. Each replay equals the corresponding
damaged original byte for byte. This independently checks the physical
transformation, but does not retroactively create missing historical hashes.

`reconstruction-manifest.json` records every before/candidate hash,
logical-equivalence result, exact historical evidence and evidence gap.
The complete logical line content is identical for each pair; parsed JSON
objects and the ordered list of event objects are equal. Event count is 482,
unique IDs 482, sequences 1–482 continuous, duplicate IDs zero, before and
after. Decisions retain every historical entry/status/order and match their
known earlier full-file hash after reconstruction.

## Execution and results

One-time inspection/reconstruction utility: `build/memory-reconstruction.cpp`;
an immutable copy is included as `reconstruction-helper.cpp.bin`. It is not
an ARAMF implementation change or product dependency. The helper is linked
against existing ARAMF core objects and uses ProjectMemory::validate with
persistReport=false; validation reports are written only in this new evidence
directory, never over the historical live reports.

Commands:

```
build/memory-reconstruction.exe audit ARAMF_WORKER/verification/tasks/memory-byte-reconstruction-20260927-01
build/memory-reconstruction.exe apply ARAMF_WORKER/verification/tasks/memory-byte-reconstruction-20260927-01
git diff --check
git diff --cached --check
```

- Audit: PASS; all five candidates verified before tracked replacements.
- Replacement: completed using QSaveFile binary mode for only the five
  explicitly authorized targets, with exact candidate readback checks.
- Repository/protected before-versus-after hashes: only these five original
  files changed. The new evidence package is additive; no existing artifact
  was overwritten. Prior unrelated implementation/control-plane changes remain.
- Protected historical baseline: all 194 recorded file hashes still match.
  HVD Components, LICENSE, test_250/runs and test_550/runs are unchanged.
- Event and historical-prefix checks: PASS, before and after.
- Canonical read-only memory consistency: FAIL before and after; 17 of 18
  checks PASS, only `cold-start-fresh` FAIL. The returned error is:
  `Persisted cold-start validation is missing, failed, or stale.`
  The cold-start file exists and its stored status is PASS, but its current
  binding does not pass canonical freshness validation. It was not regenerated.
- Apply command: nonzero validation result; not claimed as complete PASS.
- `git diff --check`: exit 0 (existing Git LF/CRLF checkout warnings only).
- `git diff --cached --check`: exit 0; staging remains empty.
- No commit; HEAD unchanged at cb4ba92048c30a3fd8a5c41014dbfab0d54d3a12.

`memory-consistency-before.json`, `memory-consistency-after.json`, `after.json`,
and the captured Git outputs preserve actual results. The reconstructed live
memory-consistency-validation.json retains its historical timestamp and status;
it is not presented as a new successful validation result.

## Stop boundary

Reconstruction is complete, but full memory validation is not PASS. Stop
without touching cold-start metadata, repairing implementation, continuing
named-worker work, configuring HVD Components, staging or committing. No
Phase 2, F/P/S/C, P6, C1 or component development was started by this operation.
