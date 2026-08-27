# httpd plan loop (local SDK)

Runs the li-httpd master plan autonomously via `scripts/httpd-plan-loop.py` and agent **`code_implementer`** (or `LI_HTTPD_PLAN_AGENT`) with each plan todo as the SDK **goal** (`--goal-file`).

## Prerequisites

- `CURSOR_API_KEY` in environment  
- Clone [li-cursor-agents](https://github.com/li-langverse/li-cursor-agents) → set `LI_CURSOR_AGENTS_ROOT`  
- Optional: sibling `benchmarks` for preflight JSON  

## Commands

```bash
cd lic
chmod +x scripts/httpd-plan-gates.sh scripts/httpd-plan-loop.py
./scripts/httpd-plan-loop.py --dry-run   # inspect next todo + prompt
./scripts/httpd-plan-loop.py --once      # one agent run
./scripts/httpd-plan-loop.py --max 30    # loop until done or cap
```

## Cloud Agent VM

Use `li-cursor-agents/scripts/wait-and-overnight.sh` pattern with:

```bash
export LI_REPO_WORKFLOW_REPO=lic
export LIC_ROOT=/path/to/lic
while ! ./lic/scripts/httpd-plan-loop.py --once; do sleep 120; done
```

## Human merge

Agent opens PRs only — add `merge-approved` after review; do not self-merge.
