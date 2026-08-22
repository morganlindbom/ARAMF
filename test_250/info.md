# ARAMF 250-Scenario End-to-End Campaign

Scenarios planned: 250  
Scenarios executed: 250  
Initial PASS: 247  
Initial FAIL: 3  
PASS after fix: 3  
Remaining FAIL: 0  
Blocked: 0  
Unique issues: 1  
Critical: 0 · High: 0 · Medium: 1 · Low: 0 · UX: 0  
Issues fixed: 1  
Regression tests added: 1  
Framework Knowledge candidates: 1  
Recommended for approval: 1 — MORE EVIDENCE

## Campaign Summary

Scenarios planned: 250
Scenarios executed: 250

Initial PASS: 247
Initial FAIL: 3
PASS after correction: 3
Remaining FAIL: 0
Blocked: 0
Defects found: 1
Defects fixed: 1
Framework Knowledge candidates: 1
Recommended for approval: 1 (MORE EVIDENCE)

## Environment

OS: Windows
Compiler / Qt / CMake: captured from existing build configuration; native build and CTest passed before campaign.
Workflow runner: native C++ core services; GUI click-through unavailable in this environment.

## Tested Workflow

Each scenario used isolated project creation, configuration, persistence Save/Load, Review-equivalent model inspection, Save & Generate-equivalent generation, Verify, Finalize, idempotent Finalize, AI entry-point creation, and on-disk inspection where selected. Variations covered the required distribution areas.

## Results by Area

All twelve required distribution bands were executed. See each `scenarios/test_NNN/result.md` and checkpoint logs for detail.

## Problems Discovered

### ISSUE-001 — Selective generation blocked Finalize

Discovered in: TEST-198, TEST-204, TEST-210

Severity: Medium

Observed behavior: Generate and Verify passed when Memory was unchecked, but Finalize failed on memory consistency.

Root cause: Finalize treated memory validation as unconditional instead of following selected GenerationOptions.

Correction: Memory validation and PROJECT_FINALIZED event recording are gated by `generateMemory`.

Validation: CTest passed; all three original scenarios passed on retest.

Framework Knowledge candidate: YES — MORE EVIDENCE

Lesson: Lifecycle preconditions should be derived from the explicitly selected product set, and selective output must remain finalizable when its selected products verify successfully.

## Repeated Problem Patterns

Selective-output lifecycle preconditions were initially too broad; the correction generalized across all three affected scenarios.

## Corrections That Prevented Repetition

ISSUE-001 was fixed before the full rerun. TEST-198, TEST-204, and TEST-210 all passed after the same correction with no recurrence.

## Framework Knowledge Candidates

One candidate is recorded in scenario evidence only and is not written to live repository Framework Knowledge. It should receive human review after broader evidence.

## Remaining Risks

Native GUI click-through, dialogs, visual layout, zoom, and physical multi-monitor behavior remain manual checks. The runner does not claim those as full user-E2E.

## GUI Checks Still Required

Open the Qt application and manually exercise project dialogs, every workflow page, Save As cancellation, scrolling/zoom, and provider-specific bootstrap selection.

## Final Assessment

The tested core lifecycle completed across 250 isolated configurations. Persistence, selective generation, Verify guards, Finalize idempotence, bounded memory configuration, and bootstrap convergence were exercised. GUI-level evidence is still required before claiming complete user-interface E2E coverage.
