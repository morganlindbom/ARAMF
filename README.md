<!-- README.md -->

# AR&MF — AI Rules & Memory Framework

AR&MF is a **C++17 / Qt 6 desktop application** for creating and maintaining a project-local control plane for AI-assisted software development.

Its purpose is to make project rules, current status, durable decisions, memory, routing, resources, and verification evidence portable between ChatGPT, Codex, and other repository-aware coding agents.

## Implementation

ARAMF itself is fully C++ based:

- **Language:** C++17
- **GUI:** Qt 6 Widgets
- **Build:** CMake + Ninja/MSYS2 UCRT64 on the primary Windows environment
- **Testing:** CTest with native C++ tests
- **Project memory:** native C++/Qt JSON and file handling
- **Python runtime dependency:** none
- **Node.js runtime dependency:** none

ARAMF can still configure projects that use Python, C#, JavaScript/TypeScript, C, embedded SDKs, databases, and other technologies. Those are target-project choices, not ARAMF implementation dependencies.

## Repository Structure

The repository-local ARAMF development control material is under
`aramf_setup/`. The uppercase `ARAMF/` name is reserved for the control plane
generated inside managed target projects.

Root-level generated entry-file templates are kept separately under
`aramf_setup/bootstrap/`. In particular, `aramf_setup/bootstrap/AGENTS.md` is
the source reference for a generated `<ProjectPath>/AGENTS.md`; it is not the
repository-development `aramf_setup/AGENTS.md`. The generated bootstrap points
to `<ProjectPath>/ARAMF/AGENTS.md`, and the bootstrap source directory is never
copied into a target project.

```text
aramf_setup/
├── AGENTS.md                  # canonical agent instructions
├── PROJECT_STATUS.md          # current project/program state
├── aramf-profile.json
├── rules/
│   └── generated-rules.md
├── memory/
│   ├── decisions.md
│   ├── checkpoints.json
│   ├── metrics.json
│   ├── event-log.jsonl
│   ├── memory-manifest.json
│   ├── current-state.md
│   ├── cold-start-validation.json
│   └── memory-consistency-validation.json
├── routing/
├── resources/
├── templates/
├── platforms/
├── verification/
├── custom/                    # user-owned; never modified automatically
├── docs/
└── evidence/

src/
├── core/
│   ├── AramfPaths.h
│   ├── ProjectMemory.h/.cpp
│   ├── ProjectModel.h/.cpp
│   └── Services.h/.cpp
└── ui/

tests/
└── ProjectMemoryTests.cpp

AGENTS.md                      # minimal bootstrap only
CMakeLists.txt
CMakePresets.json
README.md
LICENSE
```

## Agent Model

The repository root contains only a small `AGENTS.md` discovery file. The
canonical repository-development instructions and every file those
instructions depend on are kept under `aramf_setup/`. This is intentionally
different from the uppercase `ARAMF/` control directory generated inside a
managed target project.

The intended cold-start order is:

1. `aramf_setup/PROJECT_STATUS.md`
2. `aramf_setup/memory/decisions.md`
3. `aramf_setup/rules/generated-rules.md`
4. task-relevant routing/resources/platform/verification files only

This keeps the project root clean and avoids duplicated rule stores.

## Project Memory

`ProjectMemory` is implemented in C++ and owns:

- initialization of the canonical generated-project `ARAMF/` hierarchy;
- append-only JSONL events;
- durable sequence tracking;
- generated `current-state.md`;
- cold-start validation;
- memory consistency validation;
- safe creation of missing managed files.

`aramf_setup/PROJECT_STATUS.md` is intentionally separate from derived memory
state. It is the current human/agent-facing snapshot of what the program
contains, what is done, what has been verified, known issues, and what should
happen next.

## Build on the Primary Windows Environment

```powershell
cmake --preset windows-ucrt64
cmake --build --preset windows-ucrt64-debug
ctest --test-dir build --output-on-failure
```

The preset expects the established MSYS2 UCRT64 GCC/Ninja environment. Qt 6 must be discoverable by CMake in the development environment.

## Generation Contract

When ARAMF generates a managed project, it creates an uppercase `ARAMF/` control directory. A root `AGENTS.md` is created only when one does not already exist; foreign root agent instructions are not silently overwritten.

Files under `ARAMF/custom/` are user-owned and protected from automatic modification.

## Historical Material

Recovered architecture notes and reconstruction evidence are retained under
`aramf_setup/docs/reconstruction/`. They are archival evidence and may describe
superseded lowercase paths or the temporary Python reconstruction. They are not
current authority.

## Live Framework Knowledge

ARAMF managed projects keep reusable, evidence-backed lessons in
`ARAMF/memory/framework-knowledge.json`. Agents may propose candidates after a
verified correction, but only explicit user approval can promote a candidate to
`approved`. Approved knowledge is immediately active through `ARAMF/AGENTS.md`;
users do not need to reopen ARAMF just to benefit from an already approved
lesson. Current user instructions, Sources of Truth, and durable project
decisions always outrank Framework Knowledge.
