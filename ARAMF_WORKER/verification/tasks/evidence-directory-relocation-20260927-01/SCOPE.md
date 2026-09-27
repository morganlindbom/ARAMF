# Evidence directory relocation only

Two explicitly authorized directory renames preserve every snapshot byte.
No Phase 1 edit, source change, recorder event, context/cold-start regeneration, staging or commit.
VerificationServices persisted current verification plus its mandatory memory-validation report.
Previous current reports are preserved as flat previous-*.bin snapshots.
relocation-amendment.json contains complete recursive before/after manifests and provenance.
Earlier blocked reports remain unchanged at their original locations except the two authorized backup-directory relocations.
Phase 2A PREPARE is recorded separately; no execution is authorized by this task.
