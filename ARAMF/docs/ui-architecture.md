# ARAMF UI Architecture

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
    │   ├── routing/
    │   └── memory/
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

## Durable UI rules

- Every workflow page answers one clear user question.
- A property that can truthfully have several simultaneous values uses a
  multi-selection model and checkbox UI.
- Stable IDs are persisted; display labels remain catalog data.
- Model and persistence logic stay in `src/core/`, separate from page widgets.
- New visible workflow pages receive their own directory under the matching
  workflow group.
