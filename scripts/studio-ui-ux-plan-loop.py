#!/usr/bin/env python3
"""Autonomous Studio UI/UX loop — implement + capture + bench every iteration.

Usage:
  export CURSOR_API_KEY=cursor_...
  export LI_CURSOR_AGENTS_ROOT=/path/to/li-cursor-agents
  ./scripts/studio-ui-ux-plan-loop.py --once
  ./scripts/studio-ui-ux-plan-continuous.sh

State: data/studio-ui-ux-plan-loop/state.json
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
LOOP_BRANCH = os.environ.get("STUDIO_UI_UX_PR_BRANCH", "cursor/studio-ui-ux-plan-loop")
PLAN = ROOT / "docs/superpowers/plans/2026-05-24-studio-ui-ux-plan-loop.md"
UX_DOC = ROOT / "docs/game-dev/competitive-intel/ui-ux-by-dimension.md"
STATE_DIR = ROOT / "data/studio-ui-ux-plan-loop"
STATE_FILE = STATE_DIR / "state.json"
GATES = ROOT / "scripts/studio-ui-ux-plan-gates.sh"
CAPTURE = ROOT / "scripts/studio-ui-ux-capture-progress.sh"
COMMIT = ROOT / "scripts/studio-ui-ux-commit-push.sh"
UX_FILE = STATE_DIR / "latest-ux-assessment.json"


def _refresh_agent_canvases() -> None:
    langverse = Path(os.environ.get("LI_LANGVERSE_ROOT", ROOT.parent))
    lic = Path(os.environ.get("LIC_ROOT", langverse / "lic"))
    script = lic / "scripts/refresh-all-agent-canvases.sh"
    if script.is_file():
        subprocess.run(["bash", str(script)], check=False)
        return
    subprocess.run([sys.executable, str(ROOT / "scripts/studio-ui-ux-write-snapshot.py")], check=False)
    subprocess.run([sys.executable, str(ROOT / "scripts/studio-ui-ux-refresh-canvas.py")], check=False)


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


def pick_next(todos: list[dict], state: dict) -> dict | None:
    completed = set(state.get("completed_ids", []))

    def order_key(t: dict) -> tuple[int, str]:
        status_rank = 0 if t["status"] == "in_progress" else 1
        return (status_rank, t["id"])

    open_todos = [
        t
        for t in todos
        if t["id"].startswith("studio-ux-")
        and t["status"] in ("in_progress", "pending")
        and t["id"] not in completed
    ]
    if not open_todos:
        return None
    open_todos.sort(key=order_key)
    return open_todos[0]


def run_gates() -> tuple[bool, str]:
    if not GATES.is_file():
        return False, f"missing gates: {GATES}"
    proc = subprocess.run(
        ["bash", str(GATES)],
        cwd=ROOT,
        env={**os.environ, "LI_REPO_ROOT": str(ROOT)},
        capture_output=True,
        text=True,
    )
    out = (proc.stdout or "") + (proc.stderr or "")
    return proc.returncode == 0, out[-8000:]


def read_ux_pass() -> bool:
    if not UX_FILE.is_file():
        return False
    try:
        data = json.loads(UX_FILE.read_text(encoding="utf-8"))
        return bool(data.get("pass"))
    except json.JSONDecodeError:
        return False


def commit_push(todo_id: str) -> None:
    if not COMMIT.is_file():
        return
    subprocess.run(
        ["bash", str(COMMIT), todo_id, f"feat(studio-ui): {todo_id} — iteration"],
        cwd=ROOT,
        check=False,
    )


def post_capture(todo_id: str) -> int:
    if not CAPTURE.is_file():
        print("capture: missing script", flush=True)
        return 1
    env = {
        **os.environ,
        "STUDIO_UI_UX_ITERATION": todo_id,
        "LI_CURSOR_AGENTS_ROOT": str(agents_root() or ROOT.parent / "li-cursor-agents"),
    }
    proc = subprocess.run(["bash", str(CAPTURE)], cwd=ROOT, env=env, check=False)
    return proc.returncode


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
    for line in iter(pipe.readline, ""):
        out.write(line)
        out.flush()
        logf.write(line)
        logf.flush()


def loop_workflow_env() -> dict[str, str]:
    return {
        "LI_STUDIO_UI_UX_PLAN_LOOP": "1",
        "LI_REPO_WORKFLOW_REPO": "lic",
        "LI_REPO_WORKFLOW_BRANCH": LOOP_BRANCH,
        "LI_REPO_WORKFLOW_TRACK_REMOTE": "1",
        "LI_REPO_WORKFLOW_OPEN_PR": "1",
        "STUDIO_UI_UX_PR_BRANCH": LOOP_BRANCH,
    }


def _git(cwd: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, check=False)


def recover_unpushed_work(lic_root: Path, agents_root_path: Path | None, branch: str) -> None:
    if not (os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")):
        return

    def push_repo(repo_dir: Path, label: str) -> None:
        if not (repo_dir / ".git").is_dir():
            return
        cur = _git(repo_dir, "branch", "--show-current").stdout.strip()
        if cur and cur != branch:
            return
        if cur != branch:
            _git(repo_dir, "checkout", "-B", branch, f"origin/{branch}")
        if _git(repo_dir, "status", "--porcelain").stdout.strip():
            _git(repo_dir, "add", "-A")
            _git(repo_dir, "commit", "-m", f"chore(studio-ui-ux): plan loop recovery ({label})")
        ahead = _git(repo_dir, "rev-list", "--count", f"origin/{branch}..HEAD")
        if ahead.returncode == 0 and ahead.stdout.strip() not in ("", "0"):
            _git(repo_dir, "push", "-u", "origin", branch)

    push_repo(lic_root, "lic")
    if not agents_root_path:
        return
    ws = agents_root_path / "data" / "workspaces"
    if not ws.is_dir():
        return
    for org_dir in ws.iterdir():
        lic_dir = org_dir / "lic"
        if not lic_dir.is_dir():
            continue
        for run_dir in sorted(lic_dir.iterdir(), key=lambda p: p.stat().st_mtime, reverse=True)[:3]:
            repo = run_dir / "repo"
            if (repo / ".git").is_dir():
                push_repo(repo, run_dir.name)


def agent_timeout_sec() -> int | None:
    raw = os.environ.get("STUDIO_UI_UX_AGENT_TIMEOUT_SEC", "3600").strip()
    if raw in ("0", "none", "off"):
        return None
    try:
        return max(60, int(raw))
    except ValueError:
        return 3600


def run_subprocess_streaming(cmd: list[str], cwd: Path, env: dict[str, str], log_path: Path) -> int:
    timeout = agent_timeout_sec()
    with log_path.open("w", encoding="utf-8") as logf:
        logf.write(f"# cmd: {' '.join(cmd)}\n\n")
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
            threading.Thread(target=_tee_stream, args=(proc.stdout, logf, sys.stdout), daemon=True),
            threading.Thread(target=_tee_stream, args=(proc.stderr, logf, sys.stderr), daemon=True),
        ]
        for t in threads:
            t.start()
        try:
            rc = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            rc = proc.wait()
        finally:
            for t in threads:
                t.join(timeout=2)
        return rc


def run_cursor_agent(todo: dict, dry_run: bool) -> tuple[int, str]:
    root = agents_root()
    if not root:
        return 2, "li-cursor-agents not found"

    instruction = build_instruction(todo)
    if dry_run:
        return 0, instruction

    if not os.environ.get("CURSOR_API_KEY") and not os.environ.get("CURSOR_SDK_KEY"):
        return 2, "CURSOR_API_KEY not set"

    dist = root / "dist/cli/run-agent.js"
    if not dist.is_file():
        subprocess.run(["npm", "run", "build"], cwd=root, check=True)

    agent = os.environ.get("STUDIO_UI_UX_PLAN_AGENT", "studio_ui_ux_builder")
    goal_path = STATE_DIR / f"goal-{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}.md"
    goal_path.write_text(instruction, encoding="utf-8")

    env = {
        **os.environ,
        **loop_workflow_env(),
        "PYTHONUNBUFFERED": "1",
        "LI_SDK_TERMINAL_STREAM": os.environ.get("LI_SDK_TERMINAL_STREAM", "1"),
        "LI_AGENT_MINIMAL_PROMPT": "1",
        "LI_CONTROL_PLANE_STORE": os.environ.get("LI_CONTROL_PLANE_STORE", "disk"),
        "LI_STACK_SKIP_SUPABASE": os.environ.get("LI_STACK_SKIP_SUPABASE", "1"),
        "LI_EXPORT_DISK_CACHE": os.environ.get("LI_EXPORT_DISK_CACHE", "1"),
        "LIC_ROOT": str(ROOT),
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

    STATE_DIR.mkdir(parents=True, exist_ok=True)
    log_path = STATE_DIR / f"iter-{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}.log"
    rc = run_subprocess_streaming(cmd, root, env, log_path)
    recover_unpushed_work(ROOT, root, LOOP_BRANCH)
    return rc, f"log={log_path}"


def build_instruction(todo: dict) -> str:
    ux = ""
    if UX_DOC.is_file():
        ux = UX_DOC.read_text(encoding="utf-8")[:5000]
    branch = LOOP_BRANCH
    return f"""# Studio UI/UX iteration — todo `{todo['id']}`

**Plan:** `{PLAN.relative_to(ROOT)}`
**Agent:** `studio_ui_ux_builder`

## Current todo
- **id:** {todo['id']}
- **content:** {todo['content']}

## Skills (read first)
- `.cursor/skills/studio-design-review/SKILL.md`
- `.cursor/skills/studio-agentic-ux/SKILL.md`
- `.cursor/skills/studio-accessibility-web-quality/SKILL.md`
- `.cursor/skills/studio-ui-ux-rubric/SKILL.md`
- Sources: `docs/agent-skills/awesome-ui-ux-sources.md`

## Mandatory every iteration
1. Implement the todo in **lic** on `{branch}`; push; PR only.
2. Run `./scripts/studio-ui-ux-plan-gates.sh`.
3. Score **UX-01 … UX-14** (0–3) in PR body — see rubric below.
4. Run `./scripts/studio-ui-ux-capture-progress.sh` with `STUDIO_UI_UX_ITERATION={todo['id']}`.
5. Run `./scripts/bench-studio-viewport-perf.sh` — cite `latest-bench.json` (load, particles, memory).
6. Write **`data/studio-ui-ux-plan-loop/latest-ux-assessment.json`** (see schema below).
7. **Do not** commit PNG/MP4 to git.

### latest-ux-assessment.json
```json
{{
  "pass": true,
  "avg_score": 2.1,
  "min_score": 1.8,
  "dimensions": {{ "UX-01": {{ "score": 2, "note": "..." }}, "...": {{}} }},
  "honest_native_viewport": false,
  "notes": "What improved; what is still mock-only"
}}
```
**pass** = all UX-01…14 scored, avg ≥ 2.0, min ≥ 1.5, no P0 regressions vs last iteration.

## PH-UX targets
- Viewport ≥ 60 fps (when native path exists)
- Panel switch &lt; 100 ms
- MD particles: 10k@60fps, 100k@30fps tiers (honest reporting)

## UX rubric
{ux}

## Deliverable
- PR URL, tests run, UX dimension table, bench summary, GitHub issue comment link from capture script.
- Update plan YAML status for `{todo['id']}` when done.
"""


def load_state() -> dict:
    if STATE_FILE.is_file():
        return json.loads(STATE_FILE.read_text(encoding="utf-8"))
    return {"completed_ids": [], "iterations": 0, "history": []}


def save_state(state: dict) -> None:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    STATE_FILE.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Studio UI/UX plan loop")
    parser.add_argument("--max", type=int, default=0)
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-agent", action="store_true")
    parser.add_argument("--mark-done", metavar="ID")
    args = parser.parse_args()

    if args.mark_done:
        state = load_state()
        state.setdefault("completed_ids", []).append(args.mark_done)
        save_state(state)
        return 0

    if not PLAN.is_file():
        print(f"error: plan missing: {PLAN}", file=sys.stderr)
        return 1

    todos = load_plan_todos()
    state = load_state()
    max_iter = 1 if args.once else (args.max or 999)
    iteration = 0

    while iteration < max_iter:
        todo = pick_next(todos, state)
        if not todo:
            print("All studio-ux todos complete or none pending.")
            return 0

        print(f"\n=== studio-ui-ux iteration {iteration + 1}: {todo['id']} ===")

        if args.skip_agent:
            ok, gate_out = run_gates()
            return 0 if ok else 1

        code, msg = run_cursor_agent(todo, args.dry_run)
        print(msg, flush=True)
        if args.dry_run:
            return 0

        if code != 0:
            return code

        ok, gate_out = run_gates()
        print("gates: OK" if ok else "gates: FAIL")
        if not ok:
            print(gate_out[-1500:], file=sys.stderr)

        cap_rc = post_capture(todo["id"])
        print(f"capture: exit {cap_rc}", flush=True)

        subprocess.run(
            [sys.executable, str(ROOT / "scripts/studio-ui-ux-iteration-report.py"), todo["id"]],
            cwd=ROOT,
            check=False,
        )

        if ok:
            commit_push(todo["id"])

        ux_ok = read_ux_pass()
        state["iterations"] = state.get("iterations", 0) + 1
        state.setdefault("history", []).append(
            {
                "at": datetime.now(timezone.utc).isoformat(),
                "todo_id": todo["id"],
                "agent_exit": code,
                "capture_exit": cap_rc,
                "gates_ok": ok,
                "ux_pass": ux_ok,
            }
        )
        save_state(state)
        _refresh_agent_canvases()

        if not ok:
            return 1

        tid = todo["id"]
        if tid not in state.setdefault("completed_ids", []):
            state["completed_ids"].append(tid)
            save_state(state)

        iteration += 1
        todos = load_plan_todos()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
