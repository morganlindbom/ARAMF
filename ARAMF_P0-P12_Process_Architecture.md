<!-- ARAMF_P0-P12_Process_Architecture.md -->

# ARAMF Process Architecture — P0 to P12

**Document purpose:** Define the role, responsibility, boundaries, inputs, outputs, and intended value of every ARAMF process layer from **P0 through P12**.

**Current certified foundation:** P0–P4  
**Next process:** P5 — Canonical Code Bank  
**Roadmap processes:** P5–P12

---

## Process Model

ARAMF is organized as a layered governance and intelligence architecture.

Each process has a distinct responsibility:

```text
P0   Governance / Authority
P1   Context Coordination
P2   Execution Orchestration
P3   Predictive Task Optimization
P4   Self-Adjusting Routing
P5   Canonical Code Bank
P6   Agent Quality Scoring
P7   Multi-Agent Orchestration
P8   Semantic Conflict Reasoning
P9   Predictive Validation
P10  Controlled Autonomous Improvement
P11  Evaluation & Continuous Improvement Framework
P12  Knowledge Harvest & Core Promotion
```

The intended dependency direction is:

```text
P0
 ↓
P1
 ↓
P2
 ↓
P3
 ↓
P4
 ↓
P5
 ↓
P6
 ↓
P7
 ↓
P8
 ↓
P9
 ↓
P10
 ↓
P11
 ↓
P12
 ↓
New governed lifecycle cycle
```

Later processes may consume evidence produced by earlier processes, but they must not bypass the authority boundaries established below.

---

# P0 — Task Execution Governance

**Primary purpose:** Control what work is allowed to happen.

P0 is the governance and execution-boundary foundation of ARAMF. It defines and enforces the rules that every task must obey before, during, and after execution.

### Responsibilities

- Create and validate governed task contracts.
- Define permitted files, resources, scopes, and operations.
- Enforce ownership and resource claims.
- Block unmapped or unauthorized file modifications.
- Enforce preflight and postflight validation.
- Prevent destructive or prohibited operations.
- Track administrative overrides.
- Require valid execution evidence before a task can be considered complete.
- Protect project boundaries.
- Ensure execution does not exceed authorized scope.

### Inputs

- Task request.
- Project governance configuration.
- Resource ownership rules.
- File/scope permissions.
- Administrative override state.
- Required validation policy.

### Outputs

- Authorized `TaskContract`.
- Ownership/resource claim state.
- Governance decisions.
- Preflight/postflight results.
- Execution approval or rejection.
- Audit/recorder evidence.

### Core rule

> **P0 decides what is authorized.**

No later process may expand permissions beyond P0.

### Current role

**Certified foundation.**

---

# P1 — Context Coordination

**Primary purpose:** Give a task the correct context without exposing unnecessary or unauthorized information.

P1 coordinates what project knowledge, files, decisions, history, and dependencies an agent should receive for a task.

### Responsibilities

- Build and maintain project context indexes.
- Route context according to task scope.
- Preserve context provenance.
- Compact large context into efficient task-specific context.
- Enforce project and scope isolation.
- Track context freshness and fingerprints.
- Build task dependency DAGs.
- Produce governed task handoffs.
- Prevent context-scope escalation.
- Remain neutral between supported agent adapters.

### Inputs

- P0-authorized task.
- Project context index.
- Memory and historical decisions.
- Scope metadata.
- Dependency graph.
- Freshness/fingerprint state.

### Outputs

- Scoped task context.
- Context package.
- Dependency DAG.
- Handoff information.
- Context freshness evidence.
- Context provenance.

### Core rule

> **P1 decides what context a task may receive.**

P1 does not grant execution authority.

### Current role

**Certified foundation.**

---

# P2 — Execution Orchestration

**Primary purpose:** Execute governed tasks and measure what actually happens.

P2 coordinates execution while respecting P0 authorization and P1 context boundaries.

### Responsibilities

- Execute task DAGs and task contracts.
- Coordinate worker execution.
- Acquire and release P0-governed runtime ownership.
- Manage concurrency.
- Detect ownership/resource collisions.
- Enforce P0 postflight validation.
- Detect worker loss.
- Handle governed retry and recovery.
- Support checkpoint/resume behavior.
- Preserve execution history.
- Produce actual execution and validation evidence.
- Feed real outcome data into later analytical processes.

### Inputs

- P0 task contracts.
- P1 task DAG and context.
- Runtime ownership state.
- Worker availability.
- Validation requirements.

### Outputs

- Execution result.
- Build/test/validation evidence.
- Retry/recovery evidence.
- Resource collision history.
- Checkpoint/recovery state.
- Task completion status.
- Measured operational evidence.

### Core rule

> **P2 executes and measures reality.**

P2 does not redefine permissions or predictions.

### Current role

**Certified foundation.**

---

# P3 — Predictive Task Optimization

**Primary purpose:** Predict what a task is likely to require before execution.

P3 uses governed historical evidence to make deterministic, explainable predictions about future tasks.

### Responsibilities

- Generate normalized task signatures.
- Fingerprint task characteristics.
- Retrieve relevant historical evidence.
- Rank evidence deterministically.
- Predict likely scopes.
- Predict likely files/components.
- Predict likely validation suites.
- Predict likely risk categories.
- Estimate change breadth.
- Calculate explainable confidence.
- Compare predictions with actual outcomes.
- Detect prediction drift.
- Use approved cross-project/global knowledge without leaking foreign project authority or paths.
- Persist versioned prediction history.

### Inputs

- P2 execution/evaluation history.
- P1 scope/context metadata.
- P0 governance classifications.
- Approved Framework Knowledge.
- Historical task signatures.
- Validation history.

### Outputs

- `PredictionContract`.
- Predicted scopes.
- Predicted files/components.
- Predicted validation.
- Predicted risks.
- Change-breadth classification.
- Confidence score/rating.
- Evidence references.
- Prediction-vs-actual metrics.
- Drift reports.

### Core rule

> **P3 predicts. It does not authorize, route, or execute.**

Prediction is advisory evidence only.

### Current role

**Certified foundation.**

---

# P4 — Self-Adjusting Routing

**Primary purpose:** Select the most appropriate permitted route for a task and adapt future routing from governed outcome evidence.

P4 converts P3 predictions and historical execution results into deterministic route choices while preserving P0 authority.

### Responsibilities

- Maintain a versioned canonical route registry.
- Filter routes through hard governance eligibility.
- Rank eligible routes deterministically.
- Select the preferred permitted route.
- Select governed fallback routes.
- Apply conservative routing when evidence is weak.
- Track route outcomes.
- Maintain route health.
- Apply bounded self-adjustment to future route rankings.
- Prevent route oscillation through hysteresis.
- Detect route degradation.
- Apply governed circuit-breaker behavior.
- Choose legal P1 context strategies.
- Escalate validation when justified.
- Route among authorized adapters/tools where supported.
- Explain why a route was selected and why alternatives were rejected.

### Inputs

- P0 authorization constraints.
- P1 legal context routes.
- P2 execution outcomes.
- P3 predictions and confidence.
- Route health history.
- Adapter/tool availability.

### Outputs

- `RoutingDecision`.
- Selected route.
- Fallback route.
- Candidate route ranking.
- Rejection reasons.
- Routing confidence.
- Route health state.
- Circuit-breaker state.
- Future route-performance evidence.

### Core rule

> **P4 may adapt which authorized route is preferred. It may never adapt what is authorized.**

### Current role

**Certified foundation.**

---

# P5 — Canonical Code Bank

**Primary purpose:** Build a governed repository of reusable technical solutions that have sufficient evidence to be treated as canonical.

P5 moves ARAMF from remembering what happened to preserving validated implementation patterns that can be reused safely.

### Responsibilities

- Identify reusable implementation candidates.
- Store code patterns, components, algorithms, integration patterns, test patterns, and build patterns.
- Attach provenance to every candidate.
- Attach validation and regression evidence.
- Record applicable languages, frameworks, platforms, versions, and constraints.
- Compare candidate implementations.
- Prevent one successful execution from becoming canonical automatically.
- Promote only sufficiently verified implementations.
- Version canonical assets.
- Deprecate or supersede obsolete canonical assets.
- Prevent stale or incompatible solutions from being reused as current truth.
- Allow later tasks to discover relevant canonical solutions.

### Intended lifecycle

```text
OBSERVED
  ↓
CANDIDATE
  ↓
VALIDATED
  ↓
CERTIFIED
  ↓
CANONICAL
  ↓
SUPERSEDED / DEPRECATED
```

### Inputs

- P2 execution evidence.
- P3 prediction/evaluation history.
- P4 route/outcome history.
- Test evidence.
- Validation evidence.
- Provenance.
- Compatibility metadata.

### Outputs

- Canonical code entries.
- Reusable implementation patterns.
- Versioned technical assets.
- Applicability constraints.
- Validation evidence.
- Supersession/deprecation history.

### Core rule

> **P5 answers: “What implementation do we have strong evidence that works?”**

P5 must not confuse reusable code with globally authoritative knowledge.

### Roadmap status

**Next process to implement.**

---

# P6 — Agent Quality Scoring

**Primary purpose:** Measure how reliably different agents, models, adapters, or worker configurations perform specific categories of work.

P6 provides evidence-based performance assessment without turning a single global score into unrestricted authority.

### Responsibilities

- Track task outcomes by agent/adapter/model identity.
- Measure success rate.
- Measure validation quality.
- Measure retry/recovery frequency.
- Measure scope discipline.
- Measure unnecessary change breadth.
- Measure prediction/routing agreement where relevant.
- Separate performance by task category and capability.
- Preserve sample-size awareness.
- Penalize insufficient or contradictory evidence.
- Detect performance drift.
- Avoid unfairly comparing agents across unrelated task categories.
- Keep historical scoring versioned and explainable.

### Inputs

- P2 execution results.
- P3 task classification.
- P4 routing decisions.
- Validation evidence.
- Retry/recovery history.
- Provenance identifying the actual agent/tool.

### Outputs

- Task-category-specific quality evidence.
- Agent/adapter performance profiles.
- Confidence/sample-size metadata.
- Quality trends.
- Drift indicators.

### Core rule

> **P6 measures agent quality; it does not grant authority by itself.**

A high score must never override P0 governance.

### Roadmap status

**Planned.**

---

# P7 — Multi-Agent Orchestration

**Primary purpose:** Coordinate multiple specialized agents as a governed team.

P7 uses earlier evidence to divide work, assign responsibilities, coordinate handoffs, and combine results without allowing agents to escape project governance.

### Responsibilities

- Decompose complex work into coordinated sub-tasks.
- Assign tasks to suitable authorized agents/routes.
- Build multi-agent dependency graphs.
- Coordinate parallel execution where safe.
- Prevent conflicting ownership.
- Define handoff contracts.
- Merge compatible outputs.
- Detect incomplete or contradictory agent work.
- Recover from worker/agent failure.
- Preserve full provenance for every contribution.
- Prevent one agent from silently expanding another agent's scope.
- Coordinate validation of combined results.

### Inputs

- P0 task authority.
- P1 context/handoff infrastructure.
- P2 execution orchestration.
- P3 predictions.
- P4 routing.
- P6 agent-quality evidence.

### Outputs

- Multi-agent execution plan.
- Agent/task assignments.
- Handoff chain.
- Combined result.
- Conflict/recovery evidence.
- Multi-agent provenance.

### Core rule

> **P7 coordinates multiple agents but every agent remains individually governed.**

### Roadmap status

**Planned.**

---

# P8 — Semantic Conflict Reasoning

**Primary purpose:** Detect and reason about semantic conflicts between requirements, decisions, code changes, agent outputs, rules, and project knowledge.

P8 goes beyond file-level conflicts. It attempts to identify when two individually valid actions are logically incompatible.

### Responsibilities

- Detect contradictory requirements.
- Detect conflicting agent conclusions.
- Detect implementation-vs-documentation conflicts.
- Detect policy-vs-task conflicts.
- Detect incompatible assumptions.
- Detect mutually inconsistent project decisions.
- Distinguish syntax conflicts from semantic conflicts.
- Trace each side of a conflict back to provenance.
- Identify authoritative sources.
- Produce explainable conflict cases.
- Escalate unresolved ambiguity instead of inventing a resolution.
- Preserve minority/alternative evidence where appropriate.
- Record conflict resolutions and supersession.

### Inputs

- P0 governance.
- P1 context.
- P2 execution evidence.
- P3 predictions.
- P5 canonical code knowledge.
- P7 multi-agent outputs.
- Project decisions and memory.

### Outputs

- Semantic conflict reports.
- Conflict provenance.
- Authority comparison.
- Candidate resolutions.
- Escalation requirements.
- Resolved/superseded decision evidence.

### Core rule

> **P8 detects and reasons about conflicts; it must not fabricate agreement where evidence conflicts.**

### Roadmap status

**Planned.**

---

# P9 — Predictive Validation

**Primary purpose:** Predict what validation will be required and where failures are most likely before expensive validation is executed.

P9 extends the predictive architecture from task planning into validation strategy.

### Responsibilities

- Predict likely failing tests.
- Predict required validation layers.
- Identify validation dependencies.
- Detect likely stale validation.
- Predict cross-layer regression risk.
- Predict areas requiring full regression.
- Prioritize validation order.
- Estimate validation breadth.
- Compare predicted validation needs with actual failures/results.
- Improve future validation predictions from measured evidence.
- Preserve mandatory governance validation as non-reducible.

### Inputs

- P2 test/validation history.
- P3 task predictions.
- P4 routing decisions.
- P5 canonical code evidence.
- Dependency information.
- Historical regression patterns.

### Outputs

- Predictive validation plan.
- Likely failure locations.
- Validation-risk map.
- Validation priority.
- Predicted regression breadth.
- Prediction-vs-actual validation metrics.

### Core rule

> **P9 may predict and prioritize validation, but it may never remove mandatory validation.**

### Roadmap status

**Planned.**

---

# P10 — Controlled Autonomous Improvement

**Primary purpose:** Allow ARAMF to propose and perform tightly governed improvements to its own operational behavior without uncontrolled self-modification.

P10 introduces bounded autonomy.

### Responsibilities

- Detect recurring inefficiencies or failure patterns.
- Generate improvement candidates.
- Estimate expected benefit and risk.
- Define exact proposed changes.
- Require governance approval where policy demands it.
- Apply changes only within explicitly authorized improvement scope.
- Validate improvements before adoption.
- Compare before/after evidence.
- Roll back or reject unsuccessful improvement candidates.
- Preserve immutable evidence of previous behavior.
- Prevent autonomous expansion of ARAMF authority.
- Prevent self-modification of protected governance rules without explicit authorization.

### Inputs

- P2 execution metrics.
- P3 prediction quality.
- P4 routing performance.
- P6 agent-quality trends.
- P9 validation predictions.
- Drift/failure evidence.
- Governance constraints.

### Outputs

- Improvement proposals.
- Controlled experiments.
- Before/after evaluation.
- Approved operational improvements.
- Rejected/rolled-back improvement history.

### Core rule

> **P10 may improve behavior only inside governance boundaries. It must never improve itself by removing its own controls.**

### Roadmap status

**Planned.**

---

# P11 — Evaluation & Continuous Improvement Framework

**Primary purpose:** Provide the system-wide evaluation framework that measures whether ARAMF itself is becoming more reliable, efficient, safe, and useful over time.

P11 evaluates the performance of the full ARAMF architecture rather than only individual tasks or agents.

### Responsibilities

- Define system-level quality metrics.
- Track long-term execution quality.
- Track governance violations and prevented violations.
- Track prediction quality.
- Track routing quality.
- Track validation effectiveness.
- Track canonical-code reuse effectiveness.
- Track agent/team performance.
- Track recovery and failure rates.
- Compare process versions.
- Detect regressions introduced by new ARAMF versions.
- Evaluate whether autonomous improvements actually improved the system.
- Maintain comparable historical evaluation baselines.
- Produce auditable improvement reports.

### Inputs

- Evidence from P0 through P10.
- Certification history.
- Regression results.
- Operational metrics.
- Prediction/routing/validation metrics.
- Improvement experiments.

### Outputs

- System evaluation reports.
- Longitudinal quality metrics.
- Regression trends.
- Improvement effectiveness evidence.
- Certification-quality evidence.
- Recommendations for future governed changes.

### Core rule

> **P11 evaluates whether ARAMF is actually improving, not merely becoming more complex.**

### Roadmap status

**Planned.**

---

# P12 — Knowledge Harvest & Core Promotion

**Primary purpose:** Extract durable, verified knowledge from ARAMF's accumulated operational evidence and promote only trustworthy knowledge into the long-term core.

P12 is the final knowledge-consolidation layer in the P0–P12 cycle.

### Canonical knowledge maturity chain

```text
OBSERVED
  ↓
CANDIDATE
  ↓
SUPPORTED
  ↓
VERIFIED
  ↓
PROMOTED
```

### Responsibilities

- Harvest recurring lessons from project history.
- Separate local observations from portable knowledge.
- Create knowledge candidates.
- Gather supporting evidence.
- Reject weak, contradictory, stale, or project-specific conclusions where inappropriate.
- Validate knowledge across sufficient evidence.
- Preserve origin provenance.
- Record applicability constraints.
- Promote verified knowledge into the governed ARAMF core.
- Supersede obsolete knowledge.
- Preserve historical versions.
- Prevent accidental promotion of speculation into canonical truth.
- Feed promoted knowledge into future ARAMF lifecycle cycles.

### Inputs

- P0–P11 evidence.
- Framework Knowledge.
- Canonical Code Bank evidence.
- Project outcomes.
- Validation history.
- System evaluation results.
- Provenance and applicability metadata.

### Outputs

- Verified framework knowledge.
- Promoted core knowledge.
- Supersession history.
- Applicability constraints.
- Provenance and verification evidence.
- New trusted baseline knowledge for future processes.

### Core rule

> **P12 answers: “What have we learned strongly enough that ARAMF may treat it as durable core knowledge?”**

P12 is broader than P5:

- **P5** promotes reusable technical/code solutions.
- **P12** promotes verified knowledge about engineering, governance, workflows, validation, routing, architecture, and other durable lessons.

### Roadmap status

**Planned final process of the current P0–P12 lifecycle.**

---

# Complete Responsibility Chain

```text
P0  AUTHORIZES
P1  CONTEXTUALIZES
P2  EXECUTES AND MEASURES
P3  PREDICTS
P4  ROUTES
P5  PRESERVES VERIFIED CODE SOLUTIONS
P6  MEASURES AGENT QUALITY
P7  ORCHESTRATES MULTIPLE AGENTS
P8  RESOLVES SEMANTIC CONFLICTS
P9  PREDICTS VALIDATION NEEDS
P10 IMPROVES BEHAVIOR UNDER CONTROL
P11 EVALUATES THE WHOLE SYSTEM
P12 HARVESTS AND PROMOTES VERIFIED KNOWLEDGE
```

---

# Authority Model

The process number does **not** mean that later processes gain authority over earlier governance.

The intended authority relationship is:

```text
                          P12 Knowledge
                               │
                          P11 Evaluation
                               │
                    P10 Controlled Improvement
                               │
                         P9 Validation
                               │
                     P8 Conflict Reasoning
                               │
                    P7 Multi-Agent Coordination
                               │
                      P6 Agent Evaluation
                               │
                    P5 Canonical Code Bank
                               │
                         P4 Routing
                               │
                        P3 Prediction
                               │
                        P2 Execution
                               │
                         P1 Context
                               │
                         P0 Authority
```

**P0 remains the governance root.**

A later process may provide better information, prediction, routing, knowledge, or optimization, but it may not silently remove P0 restrictions.

---

# Current ARAMF Lifecycle Position

At the time of this document:

```text
P0  CERTIFIED / DONE
P1  CERTIFIED / DONE
P2  CERTIFIED / DONE
P3  CERTIFIED / DONE
P4  CERTIFIED / DONE

P5  NEXT
P6  PLANNED
P7  PLANNED
P8  PLANNED
P9  PLANNED
P10 PLANNED
P11 PLANNED
P12 PLANNED
```

Current next lifecycle state:

```text
P5.1.0.0.0
```

---

# Summary

The P0–P12 architecture transforms ARAMF progressively from a governance system into a governed engineering intelligence system:

```text
Govern
  ↓
Understand context
  ↓
Execute
  ↓
Measure
  ↓
Predict
  ↓
Route
  ↓
Reuse verified solutions
  ↓
Evaluate agents
  ↓
Coordinate teams of agents
  ↓
Reason about conflicts
  ↓
Predict validation
  ↓
Improve under control
  ↓
Evaluate the whole framework
  ↓
Promote verified knowledge
```

The architectural principle across the entire system is:

> **More intelligence must never mean less governance.**
