# SELFHOST-ISSUE-001 — GUI resource duplicate observation and correction

Status: FIXED

The Add File/Add Folder workflow originally appended repeated entries for the
same canonical local path. The root cause was unconditional append in
`ResourceInventoryPage::addResource`, with no shared identity rule.

The correction normalizes local paths and URLs into canonical identities,
blocks duplicate insertion in the GUI, deduplicates identical legacy entries
deterministically during generation, rejects conflicting duplicate metadata,
and makes Verify report duplicate identities as invalid.

Real GUI retest: the same `AGENTS.md` file was added twice. The saved project
and generated manifest contain one logical entry; Verify and Finalize passed.
