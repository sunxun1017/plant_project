---
name: plant-git
description: Manage Plant repository branches, staging, commits, rebases, and release history. Use before the first edit of an implementation task and whenever creating branches, commits, PR-ready history, tags, or architecture records.
---

# Plant Git Workflow

Follow the complete [Git workflow](../../../00_docs/development/git_workflow.md). Use trunk-based development, short-lived branches, Conventional Commits, atomic changes, and reviewable history.

## Decide the branch before editing

After read-only discovery and before the first task edit:

- Reuse the current feature branch when its name and existing commits match the requested outcome.
- Create a new branch when on `main`/`master`, when the current branch has a different completed purpose, or when the work is independently reviewable or revertible.
- Do not create a branch for read-only analysis or status reporting.
- Do not stack unrelated work on an unmerged feature branch merely for convenience. If the correct base is ambiguous or switching would disturb uncommitted work, stop and resolve the base first.

Name branches `<type>/<kebab-case-purpose>`, for example `feat/ble-ota`, `fix/light-sleep-wake`, or `docs/project-skills`.

## Stage and commit

- Inspect `git status` and `git diff` before staging.
- Stage explicit task files; do not use blind `git add .`.
- Inspect `git diff --cached` and `git diff --cached --check` before committing.
- Use `<type>(<scope>): <imperative description>` and keep one logical change per commit.
- Separate lasting architecture changes and ADRs from feature details when they are independently reviewable.
- Never include build products, IDE caches, logs, credentials, signing private keys, or unrelated user files.

Local branch creation and commits do not authorize fetch, pull, push, force-push, remote PR creation, rebasing shared history, tagging, or publishing a release. Perform those only when the user requests the corresponding external/history-changing action.
