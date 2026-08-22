# SELFHOST-ISSUE-002 — Resource authority persistence

Status: FIXED

## Initial symptom

Authority values selected in the Resource Authority page could return to
`supporting-reference` after Save, Close, and Open. Descriptions and scopes
persisted correctly, while direct ProjectPersistence round trips preserved
authority values.

## First correction

The first correction changed ResourceAuthorityPage and ResourceInventoryPage
from row-based resource lookup to stable `ProjectResource.id` lookup and
preserved the selected ID across refreshes. This improved behavior but did not
provide sufficient final real-GUI evidence for all ten resources.

## Deeper synchronization audit

The authority page now blocks authority and scope signals while loading model
state into controls. It also keeps the selected resource ID independently of a
transient list row, resolves the live ProjectResource by ID, and restores the
selection by ID after list rebuilds. Inventory editing follows the same ID
binding and does not reconstruct partial resources.

## Regression coverage

The workflow test now verifies:

- opening ResourceAuthorityPage does not mutate model authority values;
- GUI combo changes update only the selected resource;
- different authority levels remain attached to their IDs;
- model-driven list rebuilds preserve authority values;
- constructing a second authority page is read-only.

Core tests also cover authority, descriptions, scopes, persistence, resource
reordering, duplicate identity handling, and generated authority metadata.

## Current validation

Build: PASS
CTest: PASS — 2/2
test_250: PASS — 250/250
test_550 automated: PASS — 250/250

The production GUI must still be exercised with the complete ten-resource
matrix and verified after Save/Reopen before this issue is marked fixed.

## Final real-user validation

The final matrix was configured through the production ARAMF GUI and retained
the following authority values:

| Resource | Authority |
| --- | --- |
| `test_250/` | Supporting Reference |
| `test_550/` | Supporting Reference |
| `CMakeLists.txt` | Primary Source of Truth |
| `test_250/info.md` | Supporting Reference |
| `aramf_setup/` | Authoritative |
| `src/` | Primary Source of Truth |
| `test_550/info.md` | Supporting Reference |
| `ARAMF_WORKER/` | Primary Source of Truth |
| `tests/` | Authoritative |
| `AGENTS.md` | Authoritative |

Descriptions, scopes, stable IDs, and the ten unique resource identities were
preserved. The matrix remained 10/10 after resource navigation and navigation
through Resource Policy, Rules, and Memory before Save.

The real-user round trip passed at every required boundary:

- serialized project JSON: 10/10;
- first Close/Open: 10/10;
- Save without authority edits followed by Close/Open: 10/10;
- generated `ARAMF_WORKER/resources/resources.json`: 10/10 semantic match;
- final Close/Open after Save & Generate, Verify, and Finalize: 10/10;
- Verify: PASS;
- Finalize: PASS.

This completes the evidence chain from GUI selection through live model,
serialization, deserialization, GUI restoration, generated manifest, and
lifecycle finalization. No additional production correction was required after
the stable-ID and signal-blocking changes.
