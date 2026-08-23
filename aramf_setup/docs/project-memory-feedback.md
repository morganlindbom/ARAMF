# Project Memory Feedback Bridge

Generated managed projects expose a safe headless recorder through the ARAMF
executable:

```text
aramf memory record --project <project-root> --operation <operation> ...
```

The supported operations are `task-start`, `task-complete`, `build-result`,
`test-result`, and `validation-result`. The generated
`ARAMF_WORKER/memory/memory-contract.json` is the machine-readable contract
for the active project and lists the configured maintenance options.

ProjectMemory remains the sole owner of event IDs, timestamps, sequence
numbers, metrics, current-state snapshots, consistency validation, bounded
memory behavior, and policy-controlled status updates. Agents must not edit
the owned memory files directly; they must use the recorder and report the
result. Checkpoints and durable decisions remain deliberate, separate
operations and are not inferred from ordinary build or test feedback.

Projects with an older minimal `memory-config.json` remain readable. Recording
is safely unavailable until the project is regenerated with an enabled
maintenance configuration.
