ISSUE-001 — Selective generation could not finalize.

Initial failures: TEST-198, TEST-204, TEST-210.
Root cause: Finalize always validated memory although Memory was not selected.
Correction: gate memory validation and finalization event recording on generateMemory.
Retest: all three original scenarios passed after correction.
