<!-- INSTALLATION.md -->

# Installing ARAMF for Windows Development

This guide is for a new ARAMF developer, including Yousef, working from a
clean Windows computer on the private GitHub repository.

## 1. Required access and tools

Before installing the repository, ask Morgan to grant your GitHub account
access to the private repository:

```text
https://github.com/morganlindbom/ARAMF
```

The repository currently defines and uses:

- Windows as the primary development platform;
- Git and GitHub;
- CMake, with `cmake_minimum_required(VERSION 3.21)`;
- Ninja;
- MSYS2 UCRT64;
- the MSYS2 UCRT64 GCC/G++ compiler;
- Qt 6, with the Core and Widgets components;
- CTest for the native C++ test suite; and
- C++17.

The repository does not define a locked compiler, Qt, Ninja or Git version.
Install current compatible versions. The CMake preset currently expects the
MSYS2 UCRT64 installation at:

```text
C:\msys64\ucrt64
```

This path is encoded in `CMakePresets.json` as the compiler, Ninja and Qt
prefix path. Do not use an arbitrary MSYS2 environment such as MSYS or
MINGW64 for the ARAMF preset.

Python and Node.js are not ARAMF runtime dependencies. Visual Studio Code is
the repository's configured editor, but an editor is not required for the
command-line build and test workflow.

## 2. GitHub authentication

Install Git for Windows, including its normal Git Credential Manager support.
Verify Git from PowerShell:

```powershell
git --version
```

Use the normal GitHub browser/device sign-in flow when Git Credential Manager
asks for authentication. Do not put a GitHub Personal Access Token, password,
API key or private key in this repository, in a script, or in a committed
configuration file.

If Morgan has not granted repository access, cloning will fail with a
permission or repository-not-found error. Ask Morgan to grant access; do not
work around the error by copying Morgan's credentials.

## 3. Install the Windows build environment

Install the current MSYS2 distribution and initialize its UCRT64 environment
according to the MSYS2 installation instructions. Install the UCRT64 toolchain
and the UCRT64 packages that provide the tools required by this repository:

- GCC/G++;
- Ninja;
- Qt 6 Base, providing Qt 6 Core and Widgets; and
- GDB if you want to use the checked-in VS Code debugging configuration.

The exact package versions are intentionally not pinned by this repository.
The important result is that these files are available under the preset prefix:

```text
C:\msys64\ucrt64\bin\g++.exe
C:\msys64\ucrt64\bin\ninja.exe
C:\msys64\ucrt64\include\qt6
```

Install CMake separately if it is not already available from PowerShell. The
repository's VS Code tasks refer to `C:\Program Files\CMake\bin\cmake.exe`,
while the command-line instructions below require `cmake` to be available on
your Windows `PATH`.

Verify the tools before cloning or configuring:

```powershell
cmake --version
Test-Path C:\msys64\ucrt64\bin\g++.exe
Test-Path C:\msys64\ucrt64\bin\ninja.exe
Test-Path C:\msys64\ucrt64\include\qt6
```

The three `Test-Path` commands must return `True`. CMake must report a version
that supports the repository's preset format and the declared CMake minimum.

## 4. Clone ARAMF

Clone the repository after access has been granted:

```powershell
git clone https://github.com/morganlindbom/ARAMF.git
cd ARAMF
```

Confirm that the clone is on the integrated branch and has no initial changes:

```powershell
git switch main
git pull --ff-only origin main
git status --short --branch
```

The status should show `main` tracking `origin/main` with no working-tree
changes.

## 5. Read the ARAMF governance entry point

Read these files before changing source or project-control files:

```powershell
Get-Content -Raw AGENTS.md
Get-Content -Raw ARAMF_WORKER/AGENTS.md
Get-Content -Raw ARAMF_WORKER/PROJECT_STATUS.md
```

The root `AGENTS.md` is only a bootstrap. The canonical repository-development
rules, current status, routing, memory, resources, validation state and
coordination data are under `ARAMF_WORKER/`.

The governed task flow is:

```text
ANALYZE -> PREPARE -> EXECUTE -> VALIDATE
```

Do not treat `aramf_setup/` as the live repository control plane. It contains
product-owned setup, bootstrap, templates and documentation source.

## 6. Repository orientation

The main current directories are:

- `src/` - ARAMF C++ application source and Qt UI;
- `tests/` - native C++/Qt unit, integration, Foundation, Structure and
  Process-related tests;
- `docs/` - repository documentation;
- `aramf_setup/` - product-owned setup/bootstrap source;
- `ARAMF_WORKER/` - the tracked self-hosted governance and project-control
  plane;
- `CMakeLists.txt` - application and test targets; and
- `CMakePresets.json` - the Windows MSYS2 UCRT64 configure/build presets.

The current architecture is organized into three families:

### Foundations: F1-F4

| ID | Name |
| --- | --- |
| F1 | Memory & Evidence Foundation |
| F2 | Identity, Provenance & Trust Foundation |
| F3 | Scope, State & Integrity Foundation |
| F4 | Lifecycle & Certification Foundation |

The Foundation dependency order is F1, then F2, then F3, then F4. The
implementations are in `src/core/FoundationServices.*` and
`src/core/MemoryEvidenceFoundation.*`.

### Processes: P1-P14

Processes use the canonical P1-P14 architecture. P1-P5 have current
implementation and certification evidence. P6 is future and Foundation-gated;
P7-P14 are architecture/roadmap material, not claimed as implemented here.

There is no current canonical P0. P0 references that remain in historical
records or legacy namespace mappings are preserved for auditability only.

### Structures: S1-S6

`src/structure/S1/` through `src/structure/S6/` contain the current Structure
modules: Responsibility & Ownership, Physical Structure & Artifact Placement,
Dependency & Interface Boundaries, Composition & Encapsulation, Decomposition &
Modularity, and Structural Evolution & Enforcement.

Exact lifecycle versions, certification state, freshness and next work are
authoritative in `ARAMF_WORKER/PROJECT_STATUS.md`,
`ARAMF_WORKER/project.json` and the relevant certification/verification files.

## 7. Configure ARAMF

Run the canonical configure preset from the repository root in PowerShell:

```powershell
cmake --preset windows-ucrt64
```

This uses Ninja, Debug configuration, C++17, the UCRT64 `g++.exe`, and Qt 6
from `C:\msys64\ucrt64`. It writes the out-of-source build tree to `build/`.

Successful configuration ends with messages equivalent to:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: ...\ARAMF\build
```

If CMake cannot find Qt, first verify the UCRT64 Qt installation and the
`C:\msys64\ucrt64` paths above. If the preset itself is rejected, upgrade
CMake to a version that supports the checked-in preset format.

## 8. Build ARAMF

Build using the repository's build preset:

```powershell
cmake --build --preset windows-ucrt64-debug
```

The build succeeds when CMake/Ninja exits with code 0 and reports the targets
as built. The main application is produced as `build/aramf.exe`; this is local
build output and must not be committed.

## 9. Run the tests

Run all CTest targets from the repository root:

```powershell
ctest --test-dir build --output-on-failure
```

Successful output ends with a zero-failure summary. The test count is allowed
to change as the repository evolves; do not use a fixed count as the definition
of success. `--output-on-failure` prints useful diagnostics for failed tests.

The CMake test registration currently includes core, freshness, revalidation,
workflow, template, S1-S6, update-campaign and configuration-update targets.

## 10. Create Your Development Branch

Yousef must not perform ordinary development directly on `main`:

```powershell
git switch main
git pull --ff-only origin main
git switch -c yousef/<task-name>
git push -u origin yousef/<task-name>
```

Replace `<task-name>` with a short task identifier. This is an example branch
name, not a required task:

```text
yousef/f3-integrity-validation
```

Verify the branch and upstream:

```powershell
git status --short --branch
git branch -vv
```

## 11. Parallel development workflow

The normal collaboration shape is:

```text
main
├── morgan/<task-name>
└── yousef/<task-name>
```

Rules for parallel work:

- do not use `main` for ordinary development;
- synchronize with `origin/main` before starting a task;
- keep each branch focused on one task;
- avoid modifying another developer's active Foundation, Process, Structure
  or governed source-of-truth files unnecessarily;
- commit logically related work;
- push the task branch;
- integrate through the agreed review/merge process;
- do not rewrite shared history; and
- do not force-push shared branches unless explicitly authorized.

This document complements ARAMF governance; it does not replace
`ARAMF_WORKER/AGENTS.md` or the governed task workflow.

## 12. Keep Your Branch Current

To update local `main` safely:

```powershell
git fetch origin
git switch main
git pull --ff-only origin main
git switch yousef/<task-name>
```

This repository does not prescribe merge versus rebase for incorporating
`main` into an active task branch. Use the integration method agreed with
Morgan for that task. Do not rewrite shared history or force-push without
explicit authorization.

## 13. Local files, generated files and secrets

The `.gitignore` intentionally excludes local or generated material such as:

- `build/`, `Build/` and `build-*/`;
- CMake generated state such as `CMakeFiles/`, `CMakeCache.txt`, `build.ninja`
  and `compile_commands.json`;
- IDE and local tool state such as `.idea/`, `.gradle/`, `.cxx/` and
  `local.properties`;
- test output, caches and temporary files;
- `.env` files;
- private keys and certificate containers such as `*.key`, `*.pem`, `*.p12`,
  `*.pfx`, `*.jks` and `*.keystore`; and
- `ARAMF_DATA/`.

Never force-add an ignored file merely to make another computer work. If a
shared configuration is required, add a tracked non-secret template such as an
example file through the normal review process. Never commit passwords, tokens,
API keys, private keys or machine-specific absolute paths.

## 14. ARAMF_DATA

`ARAMF_DATA/` is intentionally user-owned and ignored. It contains local or
portable runtime knowledge, templates and improvement data rather than the
shared ARAMF source/control plane. Yousef does not need Morgan's
`ARAMF_DATA/` directory to clone, configure, build or test ARAMF and must not
copy personal data from it.

## 15. Visual Studio Code

Visual Studio Code is the configured development environment, but it is
optional. The checked-in `.vscode/` files configure:

- CMake Tools to use the repository presets;
- C/C++ IntelliSense for MSYS2 UCRT64 Qt 6; and
- an optional GDB launch configuration for `build/aramf.exe`.

The command-line workflow remains authoritative. Install VS Code extensions
only if you want the corresponding editor, CMake or debugging features; they
are not required to run the documented PowerShell commands.

## 16. Verify the installation

Use this checklist after the initial setup:

- [ ] GitHub access and repository clone succeed.
- [ ] `AGENTS.md` is accessible.
- [ ] `ARAMF_WORKER/AGENTS.md` and `ARAMF_WORKER/PROJECT_STATUS.md` are accessible.
- [ ] `cmake --preset windows-ucrt64` succeeds.
- [ ] `cmake --build --preset windows-ucrt64-debug` succeeds.
- [ ] `ctest --test-dir build --output-on-failure` passes.
- [ ] `git status` is clean apart from documented ignored build output.
- [ ] A `yousef/<task-name>` branch can be created.
- [ ] The branch can be pushed to `origin`.
- [ ] No secrets were added to Git.

## 17. Troubleshooting

### GitHub permission denied or clone fails

Confirm that Morgan granted the GitHub account access and that Git Credential
Manager is using the intended GitHub account. Then retry the clone. Do not put
credentials in the repository.

### `cmake` is not recognized

Install CMake and make sure its `bin` directory is on the Windows `PATH`.
Open a new PowerShell window and verify with:

```powershell
cmake --version
```

### CMake cannot find Ninja or the compiler

Verify the UCRT64 files and use the repository's preset:

```powershell
Test-Path C:\msys64\ucrt64\bin\g++.exe
Test-Path C:\msys64\ucrt64\bin\ninja.exe
cmake --preset windows-ucrt64
```

### Qt is not detected

The preset searches under `C:\msys64\ucrt64`. Verify that Qt 6 Core and
Widgets are installed in that UCRT64 prefix. Do not mix Qt libraries from a
different compiler or MSYS2 environment.

### A stale or incompatible CMake cache is reported

Do not copy a build directory from another computer or compiler. Inspect the
configuration and re-run the repository preset. If the existing local cache
cannot be repaired safely, stop and recreate only the local ignored build
directory according to your team's agreed procedure; never use destructive
repository cleanup commands as routine troubleshooting.

### Tests fail

Run the complete command with diagnostics and preserve the output:

```powershell
ctest --test-dir build --output-on-failure
```

Report the first failing test, its output, the branch, and the commit. Do not
rewrite project memory or certification evidence to hide a failure.

### Local `main` is stale

Use the safe update sequence in [Keep Your Branch Current](#12-keep-your-branch-current).
Do not use `git reset --hard` or `git clean -fdx` as routine troubleshooting.
