# Rules for AI tools in this repository

This file is for Codex, Claude Code, Copilot and any other AI tool that writes or reviews code here. Read it before you touch a file. The same rules bind people. The long version with examples is in the YouTrack knowledge base: EWS-A-25 (code) and EWS-A-26 (review).

## Code rules (KB EWS-A-25)

1. Flat code. No control statement inside another one: no if, for, while, try or switch inside an if, for, while, try or switch, and no callback that holds logic. Pull the inner part out into a small function with a name that says what it does. Return early instead of nesting.
2. Documented code. Every file starts with a comment that says what the file is for and who uses it. Every function has a documentation comment: what it does, what it takes, what it returns, what can go wrong. JSDoc in JavaScript, a docstring in Python (summary, Args, Returns, Raises), /// in C#, a comment block in PowerShell. A line that is not obvious gets a short comment that says why. No comments about work state (TODO, for now, not yet) and none that repeat the code.
3. Existing code: bring the functions you touch up to these rules. Do not rewrite a whole file only for them.
4. Simple English in comments and messages. Short sentences. Names as in the project (energy_uj, device_id).

## Repository rules (README, KB EWS-A-19, EWS-A-20, EWS-A-22)

- One task, one branch from dev, named with the YouTrack key: EWS-NN-short-name, EWS_WEB-NN-..., EWS_API-NN-.... One pull request into dev. Never a push to dev or main.
- Every commit message starts with the task key, for example "EWS_WEB-24: add the collapse button".
- The pull request does only what the task says. Its description says what changed and why, how it was tested, and links the YouTrack task. After the PR is open, the task goes to To Verify with the PR link in a comment.
- Nothing in the diff that is not source: no .env, database file, node_modules, dist, build output or IDE settings. No secret, ever. The repository is public.
- No new dependency and no change under .github unless the task asks for it.
- Keep the branch current with "git merge dev", never a rebase or a force push. On a conflict stop and ask the team on Discord.
- One review from another member. The author pushes fixes to the same branch. The reviewer or Dariusz merges with Rebase and merge (squash when the branch has many fix-up commits) and deletes the branch.

## Review checklist (KB EWS-A-26)

When you review a pull request, do not change any file and do not push. Read the whole diff and every file it touches. List every break of a rule with the file and the line, most important first:

1. Flat code: every control statement inside another one, with the small function to pull out.
2. Comments: every touched file without a header, every touched function without a documentation comment, every work-state comment.
3. Safety: a new dependency (package.json, package-lock.json, requirements.txt, a PackageReference in a .csproj), a change under .github, a network call, an install script, a secret or a .env file.
4. Scope: compare the code with the YouTrack task text. List what is extra and what is missing. A changed or removed test is a finding.
5. Tests: run the tests of the touched part and report the counts and the exit codes. Dashboard: npm ci, npx vitest run, npm run build in dashboard/. Server: pip install -r requirements.txt and pytest in server/. Client: dotnet build and the scripts tests/Test-*.ps1 (Windows only). Say which cases the new tests cover and which they miss.
6. Dashboard only: colours only through the CSS variables from dashboard/static/tokens.css; green, amber and red only for the states Active, Idle and Unused; every state shown with a word, not only a colour.
7. Repository rules: the branch name and every commit start with the task key, the PR goes into dev, the description links the task and says how it was tested, no file outside the task's area, no conflict with dev.

End with one line: approve, approve with small fixes, or request changes, and one sentence why. The result is a list of claims. A person checks each one in the code before it goes into the review.
