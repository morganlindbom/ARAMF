# AR&MF Architecture and Product State

## Identity

- Official user-facing name: **AI Rules & Memory Framework**.
- Display abbreviation: **AR&MF**.
- Safe technical identifiers: **ARMF** / historically **ARAMF** where already used by files/code.
- Purpose: reusable framework for AI development rules + memory across software projects.
- Goals include separating universal rules from project-specific rules, generating `AGENTS.md`, routing only relevant context/rules, reducing unnecessary AI context/token use, and preserving auditable project memory.

## Canonical output root

Mature architecture uses `aramf/` as active root. `docs/ai/` is legacy migration input, not the active generated root.

Expected mature layout:

```text
aramf/
├── AGENTS.md
├── aramf-profile.json
├── project-description.md
├── provenance.json
├── selection-effects.json
├── generated/
│   └── rules.md
├── routing/
│   ├── README.md
│   ├── task-routes.json
│   └── scope-routes.json
├── resources/
│   ├── resources.json
│   ├── supporting-references.json
│   └── <resource>/
├── platforms/
│   ├── openai-codex.md
│   └── chatgpt.md
├── templates/
│   └── custom-templates.json
├── verification/
│   └── external-verification.json
├── memory/
│   ├── memory-manifest.json
│   ├── current-state.md
│   ├── decisions.md
│   ├── checkpoints.json
│   ├── work-log.jsonl
│   ├── event-log.jsonl
│   ├── metrics.json
│   ├── cold-start-validation.json
│   └── memory-consistency-validation.json
└── custom/
```

Additional optional memory artifacts existed for context reuse, automation/offloading decisions and certification knowledge.

## Core architecture

Surviving R0 source supports this conceptual flow:

```text
ProjectModel
   ↓
RuleCatalog
   ↓
RuleResolver
   ↓
ContextRouter
   ↓
GenerationPlan
   ↓
RuleGenerator
   ↓
canonical ARAMF context
   ↓
ChatGPT / OpenAI Codex / Copilot / future agents
```

Resources and Project Memory surround `ProjectModel`. `ProjectModel` is the canonical project configuration source.

## Source-of-truth / authority rules

Recovered durable decisions:

1. Explicit current user instruction has highest authority.
2. Current project Source of Truth/configuration follows.
3. Current durable decisions follow.
4. Compatible certified knowledge may be used next.
5. Approved Framework Knowledge may guide defaults.
6. Templates/defaults are below explicit project knowledge.
7. AI inference is lowest authority.

Supporting References cannot override Source of Truth.
Static Data may be Source of Truth when explicitly designated.
Files under `aramf/custom/` are user-owned and must not be modified automatically.

## AGENTS deployment

- `aramf/AGENTS.md` is canonical.
- Root `AGENTS.md` is a deployment/discovery copy.
- If root `AGENTS.md` is absent: create managed copy.
- If managed by ARAMF: update.
- If foreign/user-owned: refuse overwrite.

A later task **AR&MF Root Agent Configuration** passed and established root bootstrap discovery without manual setup.

Recovered AGENTS principles include:

- explicit user instruction first;
- read routing before project work;
- load only relevant generated rules;
- protect `aramf/custom/`;
- never fabricate sources/results/quotations/bibliography;
- keep **Project Context**, **Work Product**, and **Development Environment** separate.

## Project Context reset rule

Changing Project Context triggers a downstream reset to Template while preserving:

- Project ID;
- Project Memory;
- user-owned content.

Recovered event effect text:

```text
Downstream configuration reset; restart from Template
```

## Templates

Likely mature built-ins (source + later durable decision):

1. Pico 2 W Visual Designer — must remain template #1.
2. Qt Desktop Application.
3. C++ Command Line Application.
4. CMake Library.
5. Raspberry Pi Pico Firmware.
6. React Frontend.
7. Python Backend.
8. C# .NET Backend.
9. Mobile Application.
10. Full-Stack Web Application.
11. Bachelor Thesis.

Older material sometimes reports 10 built-ins. Preserve chronology; Bachelor Thesis was later established as template #11.

User-created templates are reusable and removable; built-ins remain protected from deletion.

## Stable baseline milestones

Recovered later milestones:

### Full Application Stability and Regression Test

- template-application freeze root cause: each intermediate `ProjectModel` mutation emitted `modelChanged`, causing synchronous refresh cascades;
- fix: ProjectModel update batching; template apply emits one notification;
- regression test: `applyingTemplateCoalescesModelRefreshes`;
- 17 GUI workflow pages tested;
- 96 keyboard navigation transitions;
- 33 checkbox cycles;
- Production + Debug builds passed;
- all four integration test executables passed after final correction.

### Final Stable Baseline Closure

- freeze count 0;
- Production 4/4 tests PASS;
- Debug 4/4 tests PASS;
- single-setting regeneration PASS;
- template regeneration PASS;
- project reload PASS;
- Project ID preserved: `project-396007c5-6002-4e94-88b3-48ea60188d56`;
- custom template lifecycle PASS: save → restart → apply → delete → restart-after-delete;
- external verification Build/Tests/Launch independently persisted;
- AGENTS regression PASS;
- Project Memory PASS;
- durable checkpoint: **AR&MF Stable Application Baseline**.

### Root Agent Configuration

- canonical/root AGENTS deployment PASS;
- profile configured for C++, Qt, CMake, Windows Desktop, CTest/Qt Test/Regression Testing;
- Project Memory enabled;
- AI platforms included ChatGPT and OpenAI Codex;
- `docs/ai` not recreated;
- durable checkpoint: **AR&MF Agent Bootstrap Configured**.

### Development Environment profile correction

- IDE was initially omitted from canonical profile;
- corrected to `Visual Studio Code`;
- UI persisted/reloaded it;
- generation metadata/selection effects included it;
- regression test `developmentEnvironmentIdeSurvivesRoundTrip`;
- durable checkpoint: **AR&MF Development Environment Profile Corrected**.

## Known open reconstruction gaps

- exact final Design & Resources UX/project-local resource-management implementation;
- exact Thesis subsystem implementation;
- exact final `aramf/AGENTS.md` text;
- exact final VS Code/preset/batch helper contents, though behavior is well recovered;
- any post-Aug-9 UI refinements not represented in recovered diffs.
