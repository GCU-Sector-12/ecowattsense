# EcoWattSense

Computer energy monitoring system. Group 12 project for the module M3W226703 Group Project, Glasgow Caledonian University, September to December 2026.

> **Do not commit secrets.** No passwords, API keys, tokens, `.env` files, private keys or database files go into this repository. See [Secrets and passwords](#secrets-and-passwords) below. If you commit a secret by mistake, tell the team on Discord at once and change the secret. Removing the file in a later commit is not enough, it stays in the history.

## What the project does

Many computers in offices and labs stay on while nobody uses them. EcoWattSense shows which computers are **Active**, **Idle** or **Unused** and estimates how much energy they use in each state.

The system has three parts:

| Part | What it does | Folder |
|---|---|---|
| Monitoring client | Runs on each computer. Reads idle time, CPU load and screen lock, decides the state and sends a report to the server at a fixed interval. Windows first, then Linux, then macOS. | `client/` |
| Server | Receives the reports, stores them in a database, gives data to the dashboard. | `server/` |
| Web dashboard | Lists every computer with its state, history and estimated energy. | `dashboard/` |

Full description: Group 12 Planning Report (Teams channel Group 12).

## Status

Week 3 of 12. Analysis and design phase. The technical stack is proposed in [ADR-001](docs/decisions/ADR-001-tech-stack.md) and is waiting for the team decision. No application code yet, only spikes and documents.

## Repository layout

```
client/        monitoring client (per operating system)
  spike/       small test scripts, not production code
server/        API and database
dashboard/     web dashboard
data/          local database files, ignored by Git
docs/          project documentation, see docs/README.md
  requirements/  use cases, user types, requirements (D2)
  design/        architecture, API, database schema, wireframes (D2)
  decisions/     architecture decision records (ADR-001, ADR-002, ...)
  diagrams/      draw.io source files and exported PNGs
  spikes/        notes from technical experiments
  research/      existing solutions, competitors, sources
  testing/       test plan and results (D5)
```

Diagrams: keep the `.drawio` source file and an exported `.png` next to each other, so the picture shows on GitHub and the source can still be edited.

## Where things live

| What | Where |
|---|---|
| Code, diagrams source, ADRs | this repository |
| Documents, submissions, exported diagrams | Teams channel Group 12 |
| Tasks, sprints, Gantt, knowledge base | YouTrack, project EWS |
| Daily chat, help channel | Discord |

Please do not create new places for project files. See the Planning Report, Section 5.

## How we work with Git

Two long-lived branches:

| Branch | Purpose | Who pushes |
|---|---|---|
| `main` | Stable. Only what was reviewed, tested and shown to the tutor. Updated at milestones and deliverables (D2 to D6). | nobody directly, only merges from `dev` |
| `dev` | Integration branch. **During development all work goes through `dev`.** Feature branches start here and come back here. | nobody directly, only pull requests |

How a task goes through:

1. Start from `dev`: `git switch dev && git pull`, then `git switch -c EWS-28-use-case-diagram`. One branch per task, named after the YouTrack issue.
2. Commit small and often. Put the issue key in the message, for example `EWS-28: add use case diagram for the admin`. YouTrack links the commit to the task.
3. Open a pull request **into `dev`**. One review from another team member is required before merge. Delete the branch after merge.
4. Before a deliverable or a demo, the Technical Lead opens a pull request from `dev` into `main`, the team checks it, then it is merged. A tag marks the version, for example `D2-design`.

Never push directly to `main` or `dev`. Both branches are protected. Do not commit generated files, build output or IDE settings.

## Secrets and passwords

This repository is visible to the whole team and may become public. Never commit:

- passwords, API keys, access tokens, connection strings
- `.env` files or any config file with real credentials
- private keys, certificates, `.pem`, `.key`, `.pfx` files
- database files with real data (`*.db`, `*.sqlite`)
- personal data of users or team members (student numbers, e-mails, hostnames of real machines)

Instead:

- put configuration in environment variables and commit only an example file, for example `.env.example` with empty values
- keep real credentials in your own `.env` file, which is ignored by Git
- check `git status` and `git diff` before every commit

The `.gitignore` already blocks the common cases, but it cannot catch everything. If you are not sure, ask on Discord before you push.

## Licence

Not decided yet. Note: WattSeal, which we studied as an existing solution, is GPL-3.0. If we copy any code from it, EcoWattSense must also be GPL-3.0 and the source must be credited. Decide as a team before reusing any code.

## Team

Group 12, GitHub organisation GCU-Sector-12. Seven members. Roles and tasks are in YouTrack and in the Planning Report.
