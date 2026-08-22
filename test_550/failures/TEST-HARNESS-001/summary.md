# TEST-HARNESS-001 — Off-viewport workflow-row interaction

Initial observation: 140 automated GUI scenarios failed because native mouse delivery did not reliably activate scrolled QListWidget rows.

Root cause: the test harness did not establish a visible/selected row consistently for reverse and off-viewport navigation.

Correction: scroll each row into view, perform QTest mouse interaction, and use the production QListWidget selection path as a widget-level fallback. Reverse navigation assertions were made direction-aware.

Retest: all 140 original cases passed after the harness correction. This was not a production ARAMF defect.
