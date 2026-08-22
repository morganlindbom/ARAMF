# ARAMF UI Architecture

Repository path note: this documentation is part of the repository-local
`aramf_setup/` development control material. It must not be confused with the
uppercase `ARAMF/` control plane generated inside a managed target project.

Generation resolves its destination from `ProjectModel::projectPath()` and
appends `ARAMF`. `aramf_setup` is never created in a managed target project.
The generated root `AGENTS.md` always routes to `ARAMF/AGENTS.md`; Verify and
Finalize inspect that same generated control plane.

Repository templates for root-level generated entry files live under
`aramf_setup/bootstrap/`. This source area is distinct from the repository
development instructions in `aramf_setup/AGENTS.md` and is never mapped to a
runtime `<ProjectPath>/bootstrap/` directory.

## Source ownership

The UI filesystem mirrors the visible application hierarchy:

```text
src/ui/
├── mainwindow/
│   ├── MainWindow.cpp
│   └── MainWindow.h
├── workflow/
│   ├── WorkflowWidget.cpp
│   ├── WorkflowWidget.h
│   └── WorkflowPageId.h
├── shared/
└── workflows/
    ├── project/
    │   ├── setup/
    │   ├── academic/
    │   ├── languages/
    │   ├── frameworks/
    │   ├── developmenttools/
    │   ├── platforms/
    │   ├── hardwarearchitecture/
    │   ├── builddelivery/
    │   └── template/          # Setup-owned TemplateSelector
    ├── ai/
    │   ├── agents/
    │   ├── responsibilities/
    │   ├── autonomy/
    │   └── integration/
    ├── resources/
    │   ├── inventory/
    │   ├── authority/
    │   └── policy/
    ├── rules/
    │   ├── selection/
    │   └── routing/
    ├── memory/
    │   ├── capture/
    │   └── maintenance/
    └── output/
        ├── review/
        ├── generate/
        ├── verify/
        └── finalize/
```

Every independently displayed workflow page owns a dedicated source directory
that mirrors its position in the visible workflow. Page-specific helpers stay
with that page. `ui/shared/` is reserved for code genuinely used by multiple
independent pages. This supports maintainability, isolated development,
AI-assisted page review, and focused source exchange.

## Shell and navigation

`ui/mainwindow/` contains only the MainWindow shell. MainWindow registers page
widgets explicitly and maps stable `WorkflowPageId` values to those widgets;
it never relies on visual row numbers or accidental stacked-widget order.

`ui/workflow/` contains only grouped navigation, selected-page state, and
Back/Forward sequencing. Group headings are non-clickable. Leaf pages are
numbered for user reference, but those numbers are not internal identifiers.

MainWindow provides one shared vertical page-host `QScrollArea`. Workflow pages
do not create ordinary whole-page scroll areas. Native item views such as
resource lists may scroll internally because they represent their own data
collections. Page changes reset the shared page scroll position to the top.

Global UI zoom is an application-shell capability. The application font is
scaled from the original base font, and checkbox indicator geometry follows the
same factor. Zoom ranges from 30% to 150% without changing MainWindow size.
Startup screen and requested window dimensions are developer-controlled values
passed from `src/main.cpp`; they are not project settings.

## Workflow page responsibilities

PROJECT is an eight-page question-driven interview: identity, Academic,
languages, frameworks/SDKs, development tools, platforms,
hardware/architecture, and build/test/delivery. Template selection is owned by
Setup and may be disabled. Project Type is derived from selected capabilities.

AI is four pages: agent identity, project responsibilities, autonomy
permissions, and ARAMF integration. These are separate concepts in both UI and
model. Stable agent IDs are supplied by the central catalog; the primary agent
is single-select and additional agents are multi-select.

RESOURCES is three pages: inventory, authority, and policy. Inventory manages
structured `ProjectResource` records and their enabled state. Authority assigns
levels and scopes, allowing multiple sources of truth for different scopes.
Policy controls how AI loads and uses those resources. Resource selection alone
does not write project files or copy data.

RULES is split into rule selection and rule routing. Rule selection owns the
active rule categories and enforcement level; routing owns loading strategy,
work/project scopes, context efficiency, and conflict handling. Both pages use
grouped checkbox capabilities and stable IDs, never table or matrix editors.

MEMORY is split into capture and maintenance. Capture owns durable-memory
categories and retention level. Maintenance owns update triggers, validation,
history policy and the single maximum memory size control. `ProjectMemory`
enforces the finite storage ceiling before writes, automatically pruning the
oldest eligible history toward a 90% target. Archived data remains part of the
total and protected current state/decisions are never automatically deleted.

GENERATE is a narrow output orchestration page. `GeneratePage` translates its
visible output-product checkboxes into `GenerationOptions`; `GenerationServices`
then writes only the selected ARAMF products from the authoritative
`ProjectModel`. The generated products are agent/rules, routing, platform and
environment metadata, resource manifest, Project Memory, and provenance/
selection effects. Unselected existing products are preserved, not deleted.
Generate uses `ProjectModel::projectPath()` and never saves `projectFilePath`,
builds the target, runs tests, deploys, commits, or performs Finalize work.

The four GENERATE pages form an explicit lifecycle:

```text
Review → Generate → Verify → Finalize
```

`ReviewPage` is read-only and summarizes the current model and the same
`GenerationOptions` used by Generate. `VerificationServices` performs
non-destructive filesystem and JSON checks, records a structured verification
result, and marks output stale when its configuration fingerprint differs from
the current model. It never regenerates or repairs output. `FinalizePage`
requires a current PASS result, validates memory consistency, records one
`PROJECT_FINALIZED` event, and updates the generated project status. Finalize
does not build, test, deploy, commit, or push. The generated `custom/` area is
never scanned as generated content or modified by these services.

The Generate page's user action is `Save & Generate`. It validates the target
project path and selected products, calls the Setup page's existing Save/Save
As mechanism through `ProjectPersistence`, and only then calls
`GenerationServices` with the same in-memory `ProjectModel` and options. A
save failure or Save As cancellation blocks generation. `projectFilePath`
continues to identify the ARAMF configuration file, while `projectPath`
continues to identify the managed target directory. Standalone Save and Save
As controls remain available on Setup.

## Durable UI rules

- Every workflow page answers one clear user question.
- A property that can truthfully have several simultaneous values uses a
  multi-selection model and checkbox UI.
- Stable IDs are persisted; display labels remain catalog data.
- Model and persistence logic stay in `src/core/`, separate from page widgets.
- New visible workflow pages receive their own directory under the matching
  workflow group.

Finalize also exposes an explicit AI Agent Entry Points action. The page
delegates file work to `AgentEntryPointService` in `src/core/Services.*`; it
does not write files directly. The service combines selected stable agent IDs,
maintains target-root `AGENTS.md`, and uses central metadata for supported
provider files. Managed sections point to generated target `ARAMF/AGENTS.md`,
preserve unrelated user content, are idempotent, and never delete provider
files when an agent is removed. Unsupported agents use the generic entry
point. `aramf_setup/bootstrap/` is repository-internal source/template
material and is never copied to `<ProjectPath>/bootstrap/`.

## Live Framework Knowledge

Framework Knowledge is intentionally not tied to a workflow page. The managed
project stores it at `ARAMF/memory/framework-knowledge.json`, and canonical
`ARAMF/AGENTS.md` tells every bootstrap-connected agent to read relevant
approved entries during startup. This makes approved lessons effective in the
current project immediately, without reopening the ARAMF desktop application.
Agents may add evidence-backed candidates during normal work after verified
corrections, but promotion to `approved` requires explicit user approval.
`FrameworkKnowledgeService` provides the C++ read/propose/approve/supersede API
for application and test use; direct agent maintenance follows the same schema.
