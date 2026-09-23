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

This repository is ARAMF's self-hosted development project. Its active
repository control plane is `ARAMF_WORKER/`. Product-owned setup and bootstrap
source remains under `aramf_setup/`; it is not a second live status or memory
authority.

Root-level generated entry-file templates are kept separately under
`aramf_setup/bootstrap/`. In particular, `aramf_setup/bootstrap/AGENTS.md` is
the source reference for a generated `<ProjectPath>/AGENTS.md`; it is not the
repository-development `aramf_setup/AGENTS.md`. The generated bootstrap points
to `<ProjectPath>/ARAMF_WORKER/AGENTS.md`, and the bootstrap source directory is never
copied into a target project.

```text
# Product-owned setup/bootstrap source (abbreviated)
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

The setup listing above is intentionally abbreviated and is not the live
repository control plane. For current status, rules, memory, routing,
certification and project coordination, use the tracked `ARAMF_WORKER/`
contents described above.

`ARAMF_WORKER/` is tracked in Git and must be present in every developer clone.
`ARAMF_DATA/`, build output, caches, credentials and machine-specific runtime
data remain local and ignored.

## Agent Model

The repository root contains only a small `AGENTS.md` discovery file. The
canonical repository-development instructions and every file those
instructions depend on are kept under `ARAMF_WORKER/`. This is intentionally
different from `aramf_setup/`, which contains product-owned setup and bootstrap
source.

The intended cold-start order is:

1. `AGENTS.md`
2. `ARAMF_WORKER/AGENTS.md`
3. `ARAMF_WORKER/PROJECT_STATUS.md`
4. `ARAMF_WORKER/memory/decisions.md`
5. approved entries in `ARAMF_WORKER/memory/framework-knowledge.json`
6. `ARAMF_WORKER/rules/generated-rules.md`
7. task-relevant routing/resources/platform/verification files only

This keeps the project root clean while making the self-hosted control plane
portable for every developer and coding agent.

## Current ARAMF Architecture

The current architecture has four independent Foundations, fourteen canonical
Processes and six Structure modules. These families have different ownership
and lifecycle responsibilities.

### Foundations: F1-F4

Foundations provide shared integrity and lifecycle services underneath the
Processes. They are not ordinary sequential Processes, and there is no current
P0 Foundation.

| ID | Foundation | Responsibility | Dependency order |
| --- | --- | --- | --- |
| F1 | Memory & Evidence Foundation | Append-only evidence, memory integrity, cold-start reconstruction and certification evidence | first; no Foundation dependency |
| F2 | Identity, Provenance & Trust Foundation | Actor identity, provenance validation and trust boundaries | after F1 |
| F3 | Scope, State & Integrity Foundation | Canonical scopes, project isolation, state relationships and integrity | after F1 and F2 |
| F4 | Lifecycle & Certification Foundation | Lifecycle meaning, certification semantics and Foundation/Process gating | after F1, F2 and F3 |

The implementation is in `src/core/FoundationServices.*` and
`src/core/MemoryEvidenceFoundation.*`. Coverage is in
`tests/FoundationTests.cpp` and related tests. Exact certification and
freshness claims come from `ARAMF_WORKER/certification/` and
`ARAMF_WORKER/PROJECT_STATUS.md`.

### Processes: P1-P14

Processes are the canonical governed development lifecycle. P1-P5 have current
implementation and certification evidence. P6 is the next future process and
is Foundation-gated; P7-P14 remain roadmap processes.

| ID | Canonical process | Current role/status |
| --- | --- | --- |
| P1 | Task Execution Governance | implemented and certified |
| P2 | Context Coordination | implemented and certified |
| P3 | Execution Orchestration | implemented and certified |
| P4 | Predictive Task Optimization | implemented and certified |
| P5 | Self-Adjusting Routing | implemented and certified |
| P6 | Canonical Code Bank | future; Foundation-gated |
| P7 | Agent Quality Scoring | roadmap |
| P8 | Multi-Agent Orchestration | roadmap |
| P9 | Semantic Conflict Reasoning | roadmap |
| P10 | Predictive Validation | roadmap |
| P11 | Controlled Autonomous Improvement | roadmap |
| P12 | Evaluation & Continuous Improvement | roadmap |
| P13 | Knowledge Harvest & Core Promotion | roadmap |
| P14 | Governed Lifecycle Renewal | roadmap; canonical-only process |

There is no current P0. P0 labels may appear in immutable historical evidence
and legacy namespace mappings only. They must not be treated as a current
Process or Foundation. `ProcessNamespaceService` preserves historical labels
while resolving the canonical V2 namespace.

### Structures: S1-S6

Structures define responsibility, physical organization, boundaries,
composition, modularity and evolution rules. The current six Structure modules
are certified and complete in the repository status baseline.

| ID | Structure | Responsibility |
| --- | --- | --- |
| S1 | Responsibility & Ownership | ownership and responsibility boundaries |
| S2 | Physical Structure & Artifact Placement | physical tree, placement and containment |
| S3 | Dependency & Interface Boundaries | dependencies, interfaces and contracts |
| S4 | Composition & Encapsulation | composition contracts and encapsulation policies |
| S5 | Decomposition & Modularity | modularity assessment and decomposition policy |
| S6 | Structural Evolution & Enforcement | audit/warn/enforce policy, waivers and migration planning |

Structure source is under `src/structure/S1/` through `src/structure/S6/`.
Dedicated tests and certification evidence are tracked under `tests/` and
`ARAMF_WORKER/certification/`.

The canonical namespace is V2. The exact current lifecycle, versions,
certificates, freshness and next-work decisions are authoritative in
`ARAMF_WORKER/project.json` and `ARAMF_WORKER/PROJECT_STATUS.md`.

## Project Memory

`ProjectMemory` is implemented in C++ and owns:

- initialization of the canonical generated-project `ARAMF_WORKER/` hierarchy;
- append-only JSONL events;
- durable sequence tracking;
- generated `current-state.md`;
- cold-start validation;
- memory consistency validation;
- safe creation of missing managed files.

`ARAMF_WORKER/PROJECT_STATUS.md` is intentionally separate from derived memory
state. It is the current human/agent-facing snapshot of what the program
contains, what is done, what has been verified, known issues, and what should
happen next. Historical events remain append-only in
`ARAMF_WORKER/memory/event-log.jsonl`, and durable architecture decisions belong
in `ARAMF_WORKER/memory/decisions.md`.

## Build on the Primary Windows Environment

```powershell
cmake --preset windows-ucrt64
cmake --build --preset windows-ucrt64-debug
ctest --test-dir build --output-on-failure
```

The preset expects the established MSYS2 UCRT64 GCC/Ninja environment. Qt 6 must be discoverable by CMake in the development environment.

## Generation Contract

When ARAMF generates a managed project, it creates an uppercase `ARAMF_WORKER/` control directory. A root `AGENTS.md` is created only when one does not already exist; foreign root agent instructions are not silently overwritten.

Files under `ARAMF_WORKER/custom/` are user-owned and protected from automatic modification.

## Parallel Development

`main` is the integrated branch. Use separate task branches, for example:

```text
morgan/<task-name>
yousef/<task-name>
```

Keep work scoped to the relevant Foundation, Process or Structure files, avoid
parallel edits to the same governed source-of-truth files, and merge reviewed
work back into `main`.

## Historical Material

Recovered architecture notes and reconstruction evidence are retained under
`aramf_setup/docs/reconstruction/`. They are archival evidence and may describe
superseded lowercase paths or the temporary Python reconstruction. They are not
current authority.

## Live Framework Knowledge

ARAMF managed projects keep reusable, evidence-backed lessons in
`ARAMF_WORKER/memory/framework-knowledge.json`. Agents may propose candidates after a
verified correction, but only explicit user approval can promote a candidate to
`approved`. Approved knowledge is immediately active through `ARAMF_WORKER/AGENTS.md`;
users do not need to reopen ARAMF just to benefit from an already approved
lesson. Current user instructions, Sources of Truth, and durable project
decisions always outrank Framework Knowledge.
