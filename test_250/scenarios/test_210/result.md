# TEST-210 — Scenario 210

## Purpose

Exercise GENERATE with an isolated target and varied configuration.

## Initial State

New isolated target: `D:/ON DESCTOP/HKR KURSER/C++/ARAMF2/ARAMF/test_250/projects/test_210/target`
Configuration: `D:/ON DESCTOP/HKR KURSER/C++/ARAMF2/ARAMF/test_250/projects/test_210/test_210.aramf.json`

## User Configuration

Area: GENERATE
Template: varied by scenario
Project / AI / Resources / Rules / Memory / Generation: scenario-specific model configuration.

## User Actions

1. Create/open the isolated project.
2. Configure the model and save the ARAMF project file.
3. Reload the saved model.
4. Review, Save & Generate, Verify, Finalize, and create agent entry points where applicable.
5. Inspect generated files on disk.

## Expected Result

The supported core workflow persists state and produces a valid, bounded, deterministic ARAMF control plane.

## Actual Result

Generate: PASS (2 files); Verify: PASS (16 checks). Finalize: PASS (second call alreadyFinalized=true); entry points: PASS.

## Result

PASS-AFTER-FIX

## Problems Found

Initial FAIL: Finalize unconditionally required memory consistency when selective generation excluded Memory.

## Root Cause

FinalizationServices::finalize() validated memory and appended PROJECT_FINALIZED regardless of GenerationOptions::generateMemory.

## Correction

Finalization now validates and records memory lifecycle state only when the Memory product is selected.

## Retest

Reran the original scenario after the production fix: Generate PASS, Verify PASS, Finalize PASS, repeated Finalize PASS.

## Framework Knowledge Candidate

YES — candidate only; not auto-approved.

Candidate lesson: None generated automatically by this campaign.

Evidence: `test_210/result.md`

Generalizable because: N/A
