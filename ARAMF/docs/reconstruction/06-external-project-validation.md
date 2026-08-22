# External Project Validation — Fullstack Test Project

This information is project-specific evidence of AR&MF genericity. Do not treat every local fullstack-project implementation detail as an AR&MF product feature.

## Verified project stack

- Node.js / Express web application + backend service.
- SQLite.
- ES modules (`"type": "module"`, `import` / `export`).
- package `imports` aliases: `#src/*` and `#test/*`.

## Security/project configuration — current verified status

- Authentication and authorization are optional/configurable features.
- Controlled through `AUTH_ENABLED`.
- When enabled: register, login and logout flows exist.
- Passwords use secure hashing and must never be stored in plaintext.
- Session cookie identifies logged-in users.
- Access control is based on resource ownership.
- When auth is disabled, the application can run without login requirements according to project configuration.
- Secrets/environment-dependent runtime configuration use `.env`.
- `.env` must not be committed.
- `.env.example` documents required variables without real secrets.

## Durable decisions for the fullstack project

1. Authentication/authorization are configurable, not the only hardcoded mode.
2. Passwords must use secure hashing/verification.
3. Secrets/runtime configuration come from `.env`; real secrets never enter version control.
4. ES modules and aliases `#src/*`, `#test/*` are documented selectable project requirements.

## Source-verification required before recording exact details

Do not add these without source inspection:

- exact register/login/logout routes;
- exact auth-protected routes/resources;
- user/session table names;
- hashing/session package names;
- exact required environment variable set;
- exact security tests;
- known limitations / remaining improvements.

## AR&MF evidence from this project

A real external project was bootstrapped through root `AGENTS.md`, canonical `aramf/AGENTS.md` and routing. It successfully exercised a non-C++ stack (Express + SQLite + HTML/JS) and VS Code project integration.

An early proposal in this project suggested `aramf/memory/history.md`. Treat that file proposal as superseded by the mature memory architecture (`work-log.jsonl`, `event-log.jsonl`, decisions, checkpoints and derived current-state). The enduring global lesson is:

> A real project may reveal a candidate global AR&MF improvement, but project-specific facts remain at project level. Generalizable improvements must be reviewed and recorded at framework/main-program level.
