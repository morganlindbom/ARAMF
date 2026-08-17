
<!-- README.md -->

# AR&MF — AI Rules & Memory Framework

AR&MF is a reusable framework for organizing, controlling, and preserving AI-assisted software development.

The framework is designed to give AI development agents a structured project environment containing explicit rules, project configuration, persistent memory, architectural decisions, validation evidence, and current project state.

The primary goal is to make AI-assisted development more **reliable, reproducible, measurable, and portable between AI sessions and development agents**.

---

## Purpose

Modern AI coding agents are highly capable, but they often operate with incomplete or temporary context.

Important information can be lost between sessions, including:

* Architectural decisions
* Project-specific rules
* Development conventions
* Current implementation state
* Previous failures and corrections
* Validation results
* Known working solutions
* User preferences
* Environment configuration

AR&MF provides a structured mechanism for keeping this information inside the project itself.

Instead of relying entirely on conversation history, an AI agent can reconstruct the project state directly from the repository.

---

## Core Principles

AR&MF is built around several fundamental principles.

### Explicit Rules

Project behavior should be controlled by explicit rules rather than assumptions made by the AI.

Rules may describe:

* Coding conventions
* Architecture constraints
* Build systems
* Languages
* Frameworks
* Databases
* Testing requirements
* Security requirements
* Development workflow

---

### Project-Local Memory

Important project knowledge should remain with the project.

Project memory may include:

* Durable decisions
* Current project state
* Development history
* Checkpoints
* Validation results
* Certification evidence
* Metrics
* Known failures
* Corrective actions

This allows a new AI session to understand the project without requiring access to previous conversations.

---

### Source of Truth

AR&MF distinguishes between authoritative project information and AI inference.

A conceptual priority model is:

1. Explicit current user instructions
2. Current project Source of Truth
3. Durable project decisions
4. Validated project memory
5. Approved reusable framework knowledge
6. Templates and defaults
7. AI inference

AI inference should never silently replace authoritative project information.

---

### Separation of Concerns

AR&MF intentionally separates different kinds of project information.

For example:

* **Observation** — something that has been noticed
* **TODO** — something that may need action
* **Decision** — an approved durable choice
* **Implementation** — a change made to the project
* **Validation** — evidence that something works
* **Current State** — the project's present status
* **History** — what happened over time

These concepts should not be treated as interchangeable.

---

## Project Memory

A typical AR&MF project may contain a memory structure similar to:

```text
aramf/
└── memory/
    ├── decisions.md
    ├── current-state.md
    ├── history/
    ├── checkpoints/
    ├── cold-start-validation.json
    ├── memory-consistency-validation.json
    └── certification-knowledge.jsonl
```

### Durable Decisions

Long-lived architectural and technical decisions can be stored in:

```text
aramf/memory/decisions.md
```

These decisions are intended to survive individual tasks and AI sessions.

---

### Current State

The current project status can be represented in:

```text
aramf/memory/current-state.md
```

This should describe the present state of the project rather than its full historical development.

---

### History

Development events may be recorded separately from durable decisions.

This allows AR&MF to preserve historical evidence without turning every development event into permanent project policy.

---

## Cold-Start Capability

One of the main goals of AR&MF is **cold-start reconstruction**.

A fresh AI session should be able to enter a repository and determine:

* What the project is
* What has already been implemented
* Which architectural decisions are authoritative
* Which rules apply
* What has already been validated
* What remains unfinished
* Which known problems exist
* How the project should be built and tested

The AI should not need previous chat history to reconstruct this information.

---

## AI Agent Instructions

AR&MF can generate or maintain agent-facing instruction files such as:

```text
AGENTS.md
```

These files can provide AI development agents with the minimum relevant context required for a task.

The framework is designed to avoid loading unnecessary project information whenever possible.

This reduces context size and helps keep AI behavior focused and deterministic.

---

## Supported Development Environments

AR&MF is intended to remain independent of any single programming language or development environment.

A project may define requirements for:

* C
* C++
* C#
* Python
* Java
* JavaScript
* TypeScript
* Rust
* Embedded systems
* Web applications
* Desktop applications
* Databases
* Build systems
* Testing frameworks

The active project configuration determines which rules are relevant.

---

## AI Agent Compatibility

AR&MF is designed around project-local, tool-independent information.

The framework can therefore be used with AI development systems such as:

* OpenAI Codex
* ChatGPT
* Other repository-aware AI coding agents
* Future AI development tools

The repository remains the authoritative project environment rather than any specific AI conversation.

---

## Templates

AR&MF may provide reusable templates for different project types.

Templates can preconfigure common requirements such as:

* Programming languages
* Project structure
* Build systems
* Testing strategies
* Database technologies
* Documentation requirements
* Development environments
* Security requirements

Templates provide starting points, not absolute authority.

Explicit project decisions always take precedence over template defaults.

---

## Custom Project Content

User-controlled content can be kept separately from automatically generated framework content.

For example:

```text
aramf/custom/
```

Files stored in user-owned areas should not be modified automatically unless explicitly requested.

This provides a clear boundary between framework-managed information and manually maintained project content.

---

## Framework Knowledge

AR&MF may support portable knowledge derived from completed projects.

A future project may receive generalized lessons through a file such as:

```text
framework-knowledge.json
```

Framework Knowledge should contain reusable lessons rather than project-specific implementation details.

It must not override:

* Explicit current user instructions
* Current project Source of Truth
* Durable project decisions

---

## Validation

AR&MF treats validation as separate from implementation.

A change being implemented does not automatically mean that it has been proven correct.

Possible validation levels may include:

* Implemented
* Build verified
* Test verified
* Host validated
* Target validated
* Partially validated
* Unverified

Validation claims should be supported by evidence.

---

## Memory Consistency

Because project memory can influence future AI behavior, consistency is important.

AR&MF may perform automated checks to verify that:

* Generated memory is internally consistent
* Durable decisions are represented correctly
* Current state matches authoritative project information
* Required memory files exist
* Historical and current information remain separated

Consistency reports can be stored as machine-readable artifacts.

---

## Development Metrics

AR&MF can collect development metrics to better understand AI-assisted development.

Examples include:

* Human development time
* AI development time
* Autonomous execution time
* Waiting time
* Diagnosis time
* Development iterations
* Build attempts
* Test attempts
* Failures
* Corrective actions
* Failure domains
* Work categories

Measured results should remain distinguishable from subjective estimates.

---

## Security

Project secrets must never be stored in repository memory or generated documentation.

Runtime configuration should use appropriate mechanisms such as:

```text
.env
```

Real secrets should not be committed to Git.

Example configuration files should contain placeholders only.

---

## Design Philosophy

AR&MF favors:

* Explicit configuration
* Deterministic behavior
* Small and focused context
* Persistent project knowledge
* Clear ownership of information
* Traceable decisions
* Verifiable results
* Reproducible AI workflows
* Minimal unnecessary AI inference

The framework should help the AI understand the project rather than force the AI to rediscover it repeatedly.

---

## Long-Term Vision

The long-term goal of AR&MF is to create a standardized project environment in which an AI development agent can:

1. Enter an unfamiliar repository.
2. Read the relevant project rules.
3. Reconstruct the current project state.
4. Understand previous durable decisions.
5. Identify applicable constraints.
6. Perform a development task.
7. Build and test the result.
8. Record meaningful changes.
9. Validate project memory.
10. Leave the repository in a state that another fresh AI session can understand.

The result should be a development workflow where project knowledge belongs to the project itself rather than being trapped inside temporary AI conversations.

---

## Status

AR&MF is under active development.

The architecture, memory model, validation system, project configuration, templates, and AI-agent integration will continue to evolve as the framework is tested against real software development projects.

---

## License

License information will be added when the project's distribution model has been finalized.
