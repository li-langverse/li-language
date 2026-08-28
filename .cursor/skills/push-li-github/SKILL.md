---
name: push-li-github
description: >-
  Commit and push the Li repo to GitHub using ../.env.github credentials.
  Use after every verified master-plan slice, CI fix, or user request to sync.
  Never ask the user to push manually.
---

# Push Li to GitHub

## When to run

- End of a master-plan phase gate (after tests pass)
- User says continue master plan / ship / sync
- Before ending a long agent session with uncommitted work

## Steps

1. Verify: `./li-tests/run_all.sh` or `./scripts/check-master-plan-gates.sh` (as appropriate).
2. Push:
   ```bash
   ./scripts/agent-push-github.sh "feat(phase-N): short description"
   ```
3. If push fails: read stderr; confirm `../.env.github` has `GH_TOKEN` (fine-grained PAT, `repo` on `li-langverse/*` and your fork).

## Split org repos

```bash
./scripts/push-li-langverse-repos.sh
```

## Do not

- Ask the user to run `git push`
- Commit `.env.github` or paste `GH_TOKEN` in chat
- Force-push (`main`/`master`) without explicit user request
