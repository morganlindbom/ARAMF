<!-- generated-rules.md -->

# Generated ARAMF Rules

## Active implementation baseline

- Implement ARAMF runtime behavior in C++17 using Qt 6.
- Use CMake as the application build system.
- Keep runtime dependencies native to the C++ application; do not require Python or Node.js for ARAMF operation.
- Store all AI-facing control-plane material under `ARAMF/`.
- Keep repository root `AGENTS.md` minimal and canonical repository instructions
  in `aramf_setup/AGENTS.md`.
- Maintain `aramf_setup/PROJECT_STATUS.md` after meaningful implementation work.
- Protect user-owned `aramf_setup/custom/` from automatic modification.
- Separate implementation claims from verified evidence.
