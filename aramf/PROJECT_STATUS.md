<!-- PROJECT_STATUS.md -->

# ARAMF Project Status

## Current Program

AR&MF is a C++17 / Qt 6 desktop application for defining project context, templates, development environment, AI strategy, rules and routing, project memory, resources, review, generation, verification, and finalization.

The application can describe and generate AI-facing configuration for target projects using C, C++, C#, Python, JavaScript/TypeScript, embedded platforms, web stacks, databases, and other technologies. Those are target-project capabilities; ARAMF itself is implemented in C++.

## Implemented

- Qt 6 desktop GUI with the left-side workflow and page-based configuration flow.
- C++ `ProjectModel` for active project configuration.
- C++ template and generation services.
- Native C++ `ProjectMemory` service replacing the reconstructed Python backend.
- Canonical uppercase `ARAMF/` project-local control directory.
- Minimal root `AGENTS.md` bootstrap with canonical instructions in `ARAMF/AGENTS.md`.
- Agent-facing rules, project status, memory, routing, resources, templates, verification data, documentation, and custom content isolated under `ARAMF/`.
- `ARAMF/PROJECT_STATUS.md` contract requiring agents to maintain the current program state after meaningful work.
- Durable decisions at `ARAMF/memory/decisions.md`.
- Append-only event log, manifest sequence tracking, derived current-state generation, cold-start validation, and memory-consistency validation implemented in C++.
- CMake/CTest native C++ core test executable added.
- Refactored the GUI into permanent hierarchical PROJECT, RULES, and OUTPUT navigation groups with 11 selectable leaf pages; group headings are not pages.
- Removed Project Profile and Context from the workflow and merged AI Strategy/AI Tools into the AI page.
- Added a central `TemplateDefinition` catalog. A single primary template now derives project type and environment defaults, while explicit environment overrides are retained across template changes.
- Added a simple Setup page, template-separated optional capabilities, a template-derived Environment page with collapsed advanced options, and a unified AI configuration page.
- Refined the navigation presentation so PROJECT, TEMPLATE, AI, RESOURCES, RULES, and GENERATE are distinct non-clickable headings with indented leaf pages beneath them; page IDs remain correctly mapped despite the visual grouping.
- Reformatted the active workflow, model, setup, template, resource, and main-window implementation files for readable C++ layout without changing behavior.
- Historical reconstruction notes and Pico evidence retained under `ARAMF/docs/reconstruction/` and `ARAMF/evidence/` instead of cluttering the repository root.

## Removed

- `aramf.py` runtime/backend.
- `test_aramf.py` Python tests.
- Lowercase `aramf/` as the active control-plane root.
- Generated build output from the source handoff package.

## Verified

- Source tree checked for active Python backend/test files: removed.
- Active C++ generation paths use uppercase `ARAMF/`.
- CMake now defines the application and C++ core tests only.
- Repository structure has been normalized and stale build artifacts removed.
- `cmake --build build --config Debug` completed successfully on 2026-08-21 using the configured Qt/MSYS2 toolchain.
- `ctest --test-dir build -C Debug --output-on-failure` passed: `aramf_core_tests`.
- Core tests verified the primary Pico template ordering, template-derived Qt/Pico defaults, and preservation of an explicit language override.
- `build/aramf.exe -platform offscreen` started successfully for a short smoke run on 2026-08-21 and was then stopped cleanly.
- After formatting, `cmake --build build --config Debug` completed and CTest passed again.

The current sandbox does not contain Qt 6 development packages, so a fresh native compile and CTest run could not be executed here. The Windows CMake preset remains configured for the project's MSYS2 UCRT64 toolchain.

## Known Issues

- Several reconstructed GUI source files still use dense one-line formatting from the recovery baseline. This does not change architecture but should be cleaned progressively when those files are touched.
- Some reconstruction-history documents intentionally contain obsolete lowercase `aramf/` and Python references because they are historical evidence, not active instructions.
- Finalize and verification pages are still primarily UX scaffolding and do not yet execute every displayed action.
- Full interactive GUI click-through verification is still pending; the executable was compiled and linked, but automated widget-level coverage has not been added.

## Next Work

1. Add automated Qt widget tests for group-heading non-selection, leaf activation, and Back/Forward ordering.
2. Connect the Project Memory page directly to live `ProjectMemory` state instead of preview-only text.
3. Convert generation checkboxes into explicit output selection rather than generating the complete control plane unconditionally.
4. Implement safe managed/foreign root `AGENTS.md` deployment status in the Finalize workflow.
5. Continue restructuring dense reconstructed UI files into smaller maintainable components without changing the established workflow UX.
