# Cross-Project Lessons Useful to AR&MF

## 1. Deterministic work should be offloaded

Pico certification demonstrated that repetitive, deterministic, verifiable work should run in local automation rather than require continuous AI orchestration.

Architecture principle:

```text
Reasoning needed → AI agent
Deterministic/verifiable repetitive work → local automation
Both → durable evidence in AR&MF memory
```

PowerShell is one backend, not the architecture itself.

## 2. Infrastructure failure is not product failure

Examples from Pico certification:

- process-launch quoting failure on paths containing spaces;
- PowerShell native stderr promoted to terminating error;
- Windows path-length failure in a profiling workspace;
- stale CMake cache in copied validation fixtures;
- performance-profiling events misclassified as production drift.

These require explicit failure-domain classification.

## 3. Durable vs production sequence

Pico provided real-world evidence that control-plane/infrastructure events can advance durable history without changing production state. This directly supports AR&MF's separate durable/production sequences.

## 4. Certification requires compatibility evidence

A project-specific PASS is meaningful only with the exact baseline/toolchain/generated-state contract. Reuse should require compatibility matching rather than assuming transferability.

## 5. Measure the bottleneck, not assumptions

Pico two-row profiling showed generated build time dominated wall-clock runtime. UI/config/generation overhead was comparatively small. AR&MF should preserve measured categories instead of describing all elapsed time as AI time or framework overhead.

## 6. Safe cleanup is a framework-level lesson

The catastrophic deletion incident establishes a high-priority future safety lesson for any cleanup automation:

```text
dry-run
→ explicit candidate manifest
→ validate allowed root
→ detect/protect symlinks/junctions
→ delete only manifest-listed paths
→ verify post-delete scope/state
```

Never run a broad shell-delete command whose quoting/path expansion can exceed the intended root.

## 7. Generated/derived state should be regenerable

Do not manually edit derived AR&MF artifacts when `ProjectModel` or authoritative memory inputs should regenerate them. Validation and repair should remain conceptually separate.
