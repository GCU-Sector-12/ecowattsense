# Documentation

Project documentation for EcoWattSense. Written in Markdown, diagrams as draw.io source plus PNG.

Final deliverables (D1 to D6) are submitted on Teams. This folder keeps the working versions, the sources and the decisions behind them, so the team can edit and review them with pull requests.

| Folder | What goes here | Module deliverable |
|---|---|---|
| `requirements/` | Use case diagrams and descriptions, user types, functional and non-functional requirements, user stories | D2 (week 4) |
| `design/` | Architecture, API description, database schema, UI wireframes, report format | D2 (week 4) |
| `decisions/` | Architecture decision records, one file per decision: `ADR-NNN-short-title.md` | all |
| `diagrams/` | draw.io sources (`.drawio`) and exported pictures (`.png`). One diagram per file, the same name for source and picture | all |
| `spikes/` | Notes from technical experiments: what we tried, what worked, what did not | D3, D4 |
| `research/` | Existing solutions, competitors, articles, standards. Facts with sources | D1, D2 |
| `testing/` | Test plan, test cases, test results, evaluation | D5 (week 10) |

## Rules

- File names: lower case, words separated by `-`, for example `use-case-admin.drawio`. No spaces, no Polish or other special letters.
- Every document starts with a title, the date and the author.
- Simple English. Short sentences. The reader is a team member or the tutor.
- No personal data, no passwords, no student numbers in any document (see the main README).
- Meeting agendas and minutes live in YouTrack, knowledge base of project EWS, not here.
- When a diagram changes, export the PNG again and commit both files together.
