#!/usr/bin/env python3
"""Autonomous loop: compiler Wave A (2e/2f/2i/7d/7e) then Studio/sim verticals.

Not li-httpd — use httpd-plan-loop.py in a separate process.

Usage:
  export CURSOR_API_KEY=cursor_...
  export LI_CURSOR_AGENTS_ROOT=/path/to/li-cursor-agents
  ./scripts/compiler-studio-plan-loop.py --once
  ./scripts/compiler-studio-plan-loop.py --max 30
  ./scripts/compiler-studio-plan-overnight.sh   # until 08:00 HTTPD_PLAN_TZ (default Europe/Berlin)

State: data/compiler-studio-plan-loop/state.json
Logs:  data/compiler-studio-plan-loop/iter-*.log
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import threading
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOOP_BRANCH = os.environ.get("COMPILER_STUDIO_PR_BRANCH", "cursor/compiler-studio-plan-loop")
PLAN = ROOT / "docs/superpowers/plans/2026-05-22-compiler-studio-plan-loop.md"
BASELINE = ROOT / "docs/ecosystem/algorithms-and-libraries-plan.md"
STATE_DIR = ROOT / "data/compiler-studio-plan-loop"
STATE_FILE = STATE_DIR / "state.json"
GATES = ROOT / "scripts/compiler-studio-plan-gates.sh"

TODO_RE = re.compile(
    r"^\s+-\s+id:\s+(\S+)\s*\n"
    r"(?:\s+content:\s+\"([^\"]+)\"|\s+content:\s+(.+?))\s*\n"
    r"\s+status:\s+(\w+)\s*$",
    re.MULTILINE,
)


def load_plan_todos() -> list[dict]:
    text = PLAN.read_text(encoding="utf-8")
    m = re.search(r"^todos:\s*\n(.*)^---\s*$", text, re.MULTILINE | re.DOTALL)
    block = m.group(1) if m else ""
    todos: list[dict] = []
    for match in re.finditer(
        r"- id: (\S+)\n\s+content: \"?([^\"\n]+)\"?\n\s+status: (\w+)",
        block,
    ):
        todos.append(
            {"id": match.group(1), "content": match.group(2).strip(), "status": match.group(3)}
        )
    return todos


def compiler_wave_tier(todo_id: str) -> tuple[int, int]:
    if todo_id.startswith("wave-a-"):
        sub = 0 if "7e" in todo_id or "2f" in todo_id else 1
        return (0, sub)
    if todo_id.startswith("wave-b-"):
        return (1, 0)
    if todo_id.startswith("wave-c-"):
        return (2, 0)
    if todo_id.startswith("studio-"):
        return (3, 0)
    return (4, 0)


def pick_next(todos: list[dict], state: dict) -> dict | None:
    completed = set(state.get("completed_ids", []))

    def order_key(t: dict) -> tuple[int, int, int, str]:
        status_rank = 0 if t["status"] == "in_progress" else 1
        tier, sub = compiler_wave_tier(t["id"])
        return (status_rank, tier, sub, t["id"])

    open_todos = [
        t
        for t in todos
        if t["status"] in ("in_progress", "pending") and t["id"] not in completed
    ]
    if not open_todos:
        return None
    open_todos.sort(key=order_key)
    return open_todos[0]


def run_gates() -> tuple[bool, str]:
    if not GATES.is_file():
        return False, f"missing gates script: {GATES}"
    env = {**os.environ, "LI_REPO_ROOT": str(ROOT)}
    proc = subprocess.run(
        ["bash", str(GATES)],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    out = (proc.stdout or "") + (proc.stderr or "")
    return proc.returncode == 0, out[-8000:]


def agents_root() -> Path | None:
    for candidate in [
        os.environ.get("LI_CURSOR_AGENTS_ROOT"),
        ROOT.parent / "li-cursor-agents",
        Path("/workspace/li-cursor-agents"),
    ]:
        if not candidate:
            continue
        p = Path(candidate)
        if (p / "package.json").is_file():
            return p
    return None


def _tee_stream(pipe, logf, out) -> None:
    """Copy subprocess line-by-line to terminal and log (live, unbuffered)."""
    for line in iter(pipe.readline, ""):
        out.write(line)
        out.flush()
        logf.write(line)
        logf.flush()


def loop_workflow_env() -> dict[str, str]:
    branch = LOOP_BRANCH
    return {
        "LI_COMPILER_STUDIO_PLAN_LOOP": "1",
        "LI_REPO_WORKFLOW_REPO": "lic",
        "LI_REPO_WORKFLOW_BRANCH": branch,
        "LI_REPO_WORKFLOW_TRACK_REMOTE": "1",
        "LI_REPO_WORKFLOW_OPEN_PR": "1",
        "COMPILER_STUDIO_PR_BRANCH": branch,
    }


def _git(cwd: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
    )


def recover_unpushed_work(lic_root: Path, agents_root: Path | None, branch: str) -> None:
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not token:
        print("recover: skip (no GH_TOKEN)", flush=True)
        return

    def push_repo(repo_dir: Path, label: str) -> None:
        if not (repo_dir / ".git").is_dir():
            return
        cur = _git(repo_dir, "branch", "--show-current").stdout.strip()
        if cur and cur != branch:
            print(f"recover: skip {label} (branch {cur!r} != {branch!r})", flush=True)
            return
        if cur != branch:
            _git(repo_dir, "checkout", "-B", branch, f"origin/{branch}")
        dirty = _git(repo_dir, "status", "--porcelain").stdout.strip()
        if dirty:
            _git(repo_dir, "add", "-A")
            _git(repo_dir, "commit", "-m", f"chore(compiler-studio): plan loop recovery ({label})")
        ahead = _git(repo_dir, "rev-list", "--count", f"origin/{branch}..HEAD")
        if ahead.returncode == 0 and ahead.stdout.strip() not in ("", "0"):
            push = _git(repo_dir, "push", "-u", "origin", branch)
            if push.returncode == 0:
                print(f"recover: pushed {label}", flush=True)

    push_repo(lic_root, "lic checkout")
    if not agents_root:
        return
    ws = agents_root / "data" / "workspaces"
    if not ws.is_dir():
        return
    clones: list[Path] = []
    for org_dir in ws.iterdir():
        if not org_dir.is_dir():
            continue
        lic_dir = org_dir / "lic"
        if not lic_dir.is_dir():
            continue
        for run_dir in lic_dir.iterdir():
            repo = run_dir / "repo"
            if (repo / ".git").is_dir():
                clones.append(repo)
    clones.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    for repo in clones[:3]:
        push_repo(repo, repo.parent.name)


def refresh_live_pages() -> None:
    """Optional benchmarks dashboard refresh after perf-changing work."""
    if os.environ.get("COMPILER_STUDIO_REFRESH_PAGES", "").strip() in ("0", "false", "no"):
        return
    bench_root = Path(os.environ.get("BENCHMARKS_ROOT", str(ROOT.parent / "benchmarks")))
    script = bench_root / "scripts/refresh-live-sites.sh"
    if not script.is_file():
        print("pages: skip (no benchmarks/scripts/refresh-live-sites.sh)", flush=True)
        return
    env = {
        **os.environ,
        "LIC_ROOT": str(ROOT),
        "SKIP_BENCH": os.environ.get("COMPILER_STUDIO_PAGES_SKIP_BENCH", "0"),
        "SKIP_ROADMAP": os.environ.get("COMPILER_STUDIO_PAGES_SKIP_ROADMAP", "0"),
    }
    print("pages: refresh-live-sites.sh", flush=True)
    proc = subprocess.run(["bash", str(script)], cwd=bench_root, env=env, check=False)
    if proc.returncode != 0:
        print(f"pages: refresh exit {proc.returncode}", flush=True)


def agent_timeout_sec() -> int | None:
    raw = os.environ.get("LI_COMPILER_STUDIO_PLAN_AGENT_TIMEOUT_SEC", "2700").strip()
    if raw in ("0", "none", "off"):
        return None
    try:
        return max(60, int(raw))
    except ValueError:
        return 2700


def run_subprocess_streaming(cmd: list[str], cwd: Path, env: dict[str, str], log_path: Path) -> int:
    """Run child with stdout/stderr streamed to this terminal and log_path."""
    timeout = agent_timeout_sec()
    with log_path.open("w", encoding="utf-8") as logf:
        logf.write(f"# cmd: {' '.join(cmd)}\n# cwd: {cwd}\n\n")
        logf.flush()
        hint = f", timeout={timeout}s" if timeout else ""
        print(f"==> agent subprocess (live; also {log_path}{hint})", flush=True)
        proc = subprocess.Popen(
            cmd,
            cwd=str(cwd),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        threads = [
            threading.Thread(
                target=_tee_stream,
                args=(proc.stdout, logf, sys.stdout),
                daemon=True,
            ),
            threading.Thread(
                target=_tee_stream,
                args=(proc.stderr, logf, sys.stderr),
                daemon=True,
            ),
        ]
        for t in threads:
            t.start()
        try:
            rc = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            rc = proc.wait()
            msg = f"agent timed out after {timeout}s — killed\n"
            sys.stderr.write(msg)
            logf.write(msg)
            return 124
        finally:
            for t in threads:
                t.join(timeout=2)
        return rc


def run_cursor_agent(todo: dict, dry_run: bool) -> tuple[int, str]:
    root = agents_root()
    if not root:
        return 2, "li-cursor-agents not found (clone next to lic or set LI_CURSOR_AGENTS_ROOT)"

    instruction = build_instruction(todo)
    if dry_run:
        return 0, instruction

    if not os.environ.get("CURSOR_API_KEY") and not os.environ.get("CURSOR_SDK_KEY"):
        return 2, "CURSOR_API_KEY not set — export key or use Cloud Agent with pasted instruction"

    dist = root / "dist/cli/run-agent.js"
    if not dist.is_file():
        subprocess.run(["npm", "ci"], cwd=root, check=False)
        subprocess.run(["npm", "run", "build"], cwd=root, check=True)

    benchmarks = os.environ.get("BENCHMARKS_ROOT")
    if benchmarks and not Path(benchmarks, "scripts/agent-briefing.py").is_file():
        benchmarks = None
    if not benchmarks:
        for c in [ROOT.parent / "benchmarks", Path("/workspace")]:
            if (c / "scripts/agent-briefing.py").is_file():
                benchmarks = str(c)
                break

    agent = os.environ.get("LI_COMPILER_STUDIO_PLAN_AGENT", "code_implementer")
    goal_path = STATE_DIR / f"goal-{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}.md"
    goal_path.write_text(instruction, encoding="utf-8")

    env = {
        **os.environ,
        **loop_workflow_env(),
        "PYTHONUNBUFFERED": "1",
        "LI_SDK_TERMINAL_STREAM": os.environ.get("LI_SDK_TERMINAL_STREAM", "1"),
        "LI_AGENT_MINIMAL_PROMPT": "1",
        "LIC_ROOT": str(ROOT),
        "BENCHMARKS_ROOT": benchmarks or os.environ.get("BENCHMARKS_ROOT", str(ROOT.parent / "benchmarks")),
        "LI_AGENT_EXTRA_INSTRUCTION": instruction,
        "LI_AGENT_GOAL": instruction,
    }
    cmd = [
        "node",
        str(dist),
        "--agent",
        agent,
        "--cwd",
        str(ROOT),
        "--workflow-repo",
        "lic",
        "--goal-file",
        str(goal_path),
    ]
    if benchmarks:
        cmd.extend(["--benchmarks", benchmarks])

    STATE_DIR.mkdir(parents=True, exist_ok=True)
    log_path = STATE_DIR / f"iter-{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}.log"
    rc = run_subprocess_streaming(cmd, root, env, log_path)
    recover_unpushed_work(ROOT, root, LOOP_BRANCH)
    return rc, f"log={log_path} (streamed above)"


def build_instruction(todo: dict) -> str:
    baseline = ""
    if BASELINE.is_file():
        baseline = BASELINE.read_text(encoding="utf-8")[:6000]
    branch = LOOP_BRANCH
    return f"""# Compiler + Studio plan iteration — todo `{todo['id']}`

**Plan:** `{PLAN.relative_to(ROOT)}`
**Master plan:** `docs/superpowers/plans/2026-05-14-li-master-plan.md`

## Loop mission
- **Wave A first** (`wave-a-*`): 2e/2f/2i/7d/7e, tier-1 verify, pure-Li correctness.
- **Then** `wave-b-*` / `studio-*` (physics, chem, Studio UI).
- **Not httpd** — separate process.

## Current todo
- **id:** {todo['id']}
- **content:** {todo['content']}

## Rules (mandatory)
1. Work only in **lic** on branch `{branch}`.
2. **Push before you stop:** commit, `git push -u origin {branch}`, open/update PR.
3. PR-only — do **not** merge to main yourself.
4. Run `./scripts/compiler-studio-plan-gates.sh` before finishing.
5. **DCE allowed** — must pass `reference.py` / `bench-verify-results.sh` / golden scripts.
6. **Math:** explicit linear algebra only — **no NumPy broadcasting**; matching shapes or compile fail.
7. Do **not** implement httpd/tier5 in this loop (separate httpd agent — do not kill their process).

## Verify (benches/math)
```bash
./scripts/verify-math-physics-goldens.sh
./scripts/bench-verify-results.sh 1
```

## Context
{baseline}

## Deliverable
- Implement the todo slice with tests.
- Update plan YAML todo status for `{todo['id']}` when done.
- PR URL + test commands.
"""


def load_state() -> dict:
    if STATE_FILE.is_file():
        return json.loads(STATE_FILE.read_text(encoding="utf-8"))
    return {"completed_ids": [], "iterations": 0, "history": []}


def save_state(state: dict) -> None:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    STATE_FILE.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Autonomous compiler+Studio plan loop")
    parser.add_argument("--max", type=int, default=0, help="Max agent iterations (0 = until done)")
    parser.add_argument("--once", action="store_true", help="Single iteration")
    parser.add_argument("--dry-run", action="store_true", help="No SDK call; print instruction only")
    parser.add_argument("--skip-agent", action="store_true", help="Only run gates")
    parser.add_argument("--mark-done", metavar="ID", help="Record todo id completed in state")
    args = parser.parse_args()

    if args.mark_done:
        state = load_state()
        if args.mark_done not in state.setdefault("completed_ids", []):
            state["completed_ids"].append(args.mark_done)
        save_state(state)
        print(f"marked done: {args.mark_done}")
        return 0

    if not PLAN.is_file():
        print(f"error: plan missing: {PLAN}", file=sys.stderr)
        return 1

    todos = load_plan_todos()
    state = load_state()
    pending = [t for t in todos if t["status"] in ("pending", "in_progress")]
    done_count = len([t for t in todos if t["status"] == "completed"])
    print(f"plan todos: {len(todos)} total, {done_count} completed in plan, {len(pending)} open")

    max_iter = 1 if args.once else (args.max or 999)
    iteration = 0
    while iteration < max_iter:
        todo = pick_next(todos, state)
        if not todo:
            print("All actionable todos complete (or none in pending/in_progress).")
            return 0

        print(f"\n=== iteration {iteration + 1}: {todo['id']} ===")
        print(f"    {todo['content']}")

        ok, gate_out = run_gates()
        if not ok:
            print("gates: FAIL (agent should fix first)")
            print(gate_out[-2000:])
        else:
            print("gates: OK")

        if args.skip_agent:
            return 0 if ok else 1

        code, msg = run_cursor_agent(todo, args.dry_run)
        print(msg, flush=True)
        if args.dry_run:
            return 0

        state["iterations"] = state.get("iterations", 0) + 1
        state.setdefault("history", []).append(
            {
                "at": datetime.now(timezone.utc).isoformat(),
                "todo_id": todo["id"],
                "agent_exit": code,
                "gates_ok": ok,
            }
        )
        save_state(state)

        if code != 0:
            print(f"agent exit {code} — stopping loop (fix and re-run)", file=sys.stderr)
            return code

        refresh_live_pages()

        tid = todo["id"]
        if tid not in state.setdefault("completed_ids", []):
            state["completed_ids"].append(tid)
            save_state(state)
            print(f"todo {tid}: agent exit 0 — advancing loop (marked done in state.json)", flush=True)
        else:
            print(f"todo {tid}: agent exit 0 (already marked done)", flush=True)

        iteration += 1
        todos = load_plan_todos()

    print("max iterations reached")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
