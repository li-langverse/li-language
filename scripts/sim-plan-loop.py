#!/usr/bin/env python3
"""Autonomous sim/algo plan loop until registry smokes are implemented."""

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
BACKLOG = ROOT / "docs/ecosystem/sim-algorithm-backlog.md"
REGISTRY = ROOT / "benchmarks" / "competitive" / "algo_registry.json"
GATES = ROOT / "scripts/sim-plan-gates.sh"
COMMIT = ROOT / "scripts/sim-plan-commit-push.sh"
HANDOFF = ROOT / "docs/ecosystem/sim-agent-handoff.md"
BRANCH = os.environ.get("SIM_PLAN_PR_BRANCH", "cursor/sim-algo-plan-loop")
STATE_DIR = ROOT / "data" / "sim-plan-loop"
STATE_FILE = STATE_DIR / "state.json"

TODO_RE = re.compile(
    r"- id: (\S+)\n\s+content: \"([^\"]+)\"\n\s+status: (\w+)",
    re.MULTILINE,
)

FAMILY_PRIORITY = (
    "md",
    "pde",
    "num",
    "rigid",
    "qm",
    "drug",
    "bio",
    "ml",
    "am",
    "viz",
    "robo",
    "auto",
)


def load_todos() -> list[dict]:
    if not BACKLOG.is_file():
        return []
    text = BACKLOG.read_text(encoding="utf-8")
    return [
        {"id": m.group(1), "content": m.group(2), "status": m.group(3), "kind": "backlog"}
        for m in TODO_RE.finditer(text)
    ]


def load_registry() -> list[dict]:
    doc = json.loads(REGISTRY.read_text(encoding="utf-8"))
    return list(doc.get("algorithms", []))


def next_registry_algo() -> dict | None:
    algos = load_registry()
    by_family: dict[str, list[dict]] = {}
    for a in algos:
        if a.get("implemented_smoke"):
            continue
        by_family.setdefault(a.get("family", ""), []).append(a)
    for fam in FAMILY_PRIORITY:
        rows = sorted(by_family.get(fam, []), key=lambda x: int(x["id"]))
        if rows:
            return {**rows[0], "kind": "registry"}
    return None


def pick_work(todos: list[dict], state: dict) -> dict | None:
    done = set(state.get("completed_ids", []))
    for t in todos:
        if t["status"] in ("pending", "in_progress") and t["id"] not in done:
            return t
    reg = next_registry_algo()
    if reg:
        return {
            "id": f"algo-{reg['id']}",
            "content": f"Implement {reg['name']} (algo_id={reg['id']}, family={reg['family']})",
            "status": "pending",
            "kind": "registry",
            "algo": reg,
        }
    return None


def registry_progress() -> tuple[int, int]:
    algos = load_registry()
    impl = sum(1 for a in algos if a.get("implemented_smoke"))
    return impl, len(algos)


def agents_root() -> Path | None:
    raw = os.environ.get("LI_CURSOR_AGENTS_ROOT", "")
    if raw:
        p = Path(raw)
        if p.is_dir():
            return p
    for c in [ROOT.parent / "li-cursor-agents", ROOT / ".." / "li-cursor-agents"]:
        if (c / "dist/cli/run-agent.js").is_file():
            return c.resolve()
    return None


def _git(cwd: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, check=False)


def recover_unpushed_work(lic_root: Path, branch: str) -> None:
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not token:
        return
    dirty = _git(lic_root, "status", "--porcelain").stdout.strip()
    if dirty:
        _git(lic_root, "add", "-A")
        _git(lic_root, "commit", "-m", "chore(sim): plan loop recovery")
    ahead = _git(lic_root, "rev-list", "--count", f"origin/{branch}..HEAD")
    if ahead.returncode == 0 and ahead.stdout.strip() not in ("", "0"):
        _git(
            lic_root,
            "push",
            f"https://x-access-token:{token}@github.com/li-langverse/lic.git",
            f"HEAD:{branch}",
        )


def build_instruction(work: dict) -> str:
    impl, total = registry_progress()
    algo_block = ""
    if work.get("algo"):
        a = work["algo"]
        algo_block = f"""
## Target algorithm
- **algo_id:** {a['id']}
- **name:** `{a['name']}`
- **family:** `{a['family']}`
- Set `implemented_smoke: true` in `benchmarks/competitive/algo_registry.json` when a real smoke exists.
- Document in `docs/reports/sim-plan/algos/{a['name']}.md` (validity, perf, memory).
"""
    return f"""# Sim algorithm plan iteration — `{work['id']}`

**Branch:** `{BRANCH}` (push every iteration)
**Registry progress:** {impl}/{total} smokes implemented
**Handoff:** `{HANDOFF.relative_to(ROOT)}`

## Work item
- **id:** {work['id']}
- **content:** {work['content']}
{algo_block}

## Mandatory deliverables
1. Implement the algorithm (replace registry stub in `sim.scientific` / `li-physics-*` as appropriate).
2. **Validity:** `./scripts/sim-plan-gates.sh` (composable, summaries, scoped verify).
3. **Performance:** scoped `bench-package.sh --timing` (included in gates).
4. **Memory:** `sim-bench-memory.sh` peak RSS (included in gates).
5. **Document:** iteration notes + `docs/reports/sim-plan/algos/<name>.md`.
6. **Commit + push:** `git push -u origin {BRANCH}` before stopping (or run `./scripts/sim-plan-commit-push.sh`).

Do **not** run full `bench.py --tier 12` — use package-scoped benches only.

## If blocked
- Record blocker in `docs/reports/sim-plan/algos/<name>.md` and leave `implemented_smoke: false`.
"""


def agent_timeout_sec() -> int | None:
    raw = os.environ.get("LI_SIM_PLAN_AGENT_TIMEOUT_SEC", "3600").strip()
    if raw in ("0", "none", "off"):
        return None
    try:
        return max(60, int(raw))
    except ValueError:
        return 3600


def _tee_stream(stream, logf, out):
    for line in stream:
        if line:
            out.write(line)
            out.flush()
            logf.write(line)
            logf.flush()


def run_agent_streaming(work: dict, *, dry_run: bool) -> tuple[int, str]:
    root = agents_root()
    if not root:
        return 2, "LI_CURSOR_AGENTS_ROOT / li-cursor-agents not found"

    instruction = build_instruction(work)
    if dry_run:
        print(instruction)
        return 0, "dry-run"

    if not os.environ.get("CURSOR_API_KEY") and not os.environ.get("CURSOR_SDK_KEY"):
        return 2, "CURSOR_API_KEY not set"

    dist = root / "dist/cli/run-agent.js"
    if not dist.is_file():
        subprocess.run(["npm", "ci"], cwd=root, check=False)
        subprocess.run(["npm", "run", "build"], cwd=root, check=True)

    STATE_DIR.mkdir(parents=True, exist_ok=True)
    goal_path = STATE_DIR / f"goal-{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}.md"
    goal_path.write_text(instruction, encoding="utf-8")
    log_path = STATE_DIR / f"iter-{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}.log"

    env = {
        **os.environ,
        "PYTHONUNBUFFERED": "1",
        "LI_SDK_TERMINAL_STREAM": os.environ.get("LI_SDK_TERMINAL_STREAM", "1"),
        "LI_AGENT_MINIMAL_PROMPT": "1",
        "LIC_ROOT": str(ROOT),
        "LI_AGENT_EXTRA_INSTRUCTION": instruction,
        "LI_AGENT_GOAL": instruction,
        "SIM_PLAN_PR_BRANCH": BRANCH,
    }
    cmd = [
        "node",
        str(dist),
        "--agent",
        os.environ.get("LI_SIM_PLAN_AGENT", "code_implementer"),
        "--cwd",
        str(ROOT),
        "--workflow-repo",
        "lic",
        "--goal-file",
        str(goal_path),
    ]
    timeout = agent_timeout_sec()
    with log_path.open("w", encoding="utf-8") as logf:
        logf.write(f"# cmd: {' '.join(cmd)}\n\n")
        proc = subprocess.Popen(
            cmd,
            cwd=str(root),
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
            logf.write(f"\n# timeout after {timeout}s\n")
            rc = 124
        for t in threads:
            t.join(timeout=2)

    recover_unpushed_work(ROOT, BRANCH)
    return rc, f"log={log_path}"


def run_gates() -> tuple[bool, str]:
    if not GATES.is_file():
        return False, "missing sim-plan-gates.sh"
    proc = subprocess.run(["bash", str(GATES)], cwd=ROOT, capture_output=True, text=True)
    out = (proc.stdout or "") + (proc.stderr or "")
    return proc.returncode == 0, out


def load_state() -> dict:
    if STATE_FILE.is_file():
        return json.loads(STATE_FILE.read_text(encoding="utf-8"))
    return {"completed_ids": [], "iterations": 0, "history": []}


def save_state(state: dict) -> None:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    impl, total = registry_progress()
    state["registry"] = {"implemented": impl, "total": total}
    STATE_FILE.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")


def commit_push_iteration(work_id: str) -> None:
    if COMMIT.is_file():
        subprocess.run(
            ["bash", str(COMMIT), work_id, f"feat(sim): {work_id} — validity perf memory"],
            cwd=ROOT,
            check=False,
        )


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--once", action="store_true")
    p.add_argument("--max", type=int, default=0, help="0 = until registry done")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--skip-agent", action="store_true")
    args = p.parse_args()

    impl, total = registry_progress()
    print(f"registry: {impl}/{total} implemented_smoke")

    if impl >= total and total > 0:
        print("sim-plan-loop: all registry smokes marked implemented")
        return 0

    state = load_state()
    max_iter = 1 if args.once else (args.max or 999_999)
    iteration = 0

    while iteration < max_iter:
        work = pick_work(load_todos(), state)
        if not work:
            print("sim-plan-loop: no work items")
            return 0

        print(f"\n=== iteration {iteration + 1}: {work['id']} ===")
        print(f"    {work['content']}")

        if args.dry_run:
            _, msg = run_agent_streaming(work, dry_run=True)
            print(msg)
            return 0

        if args.skip_agent:
            ok, out = run_gates()
            print(out[-1500:])
            return 0 if ok else 1

        code, msg = run_agent_streaming(work, dry_run=False)
        print(msg, flush=True)

        ok, gate_out = run_gates()
        if not ok:
            print("gates: FAIL", file=sys.stderr)
            print(gate_out[-2000:], file=sys.stderr)
        else:
            print("gates: OK")
            commit_push_iteration(work["id"])

        state["iterations"] = state.get("iterations", 0) + 1
        state.setdefault("history", []).append(
            {
                "at": datetime.now(timezone.utc).isoformat(),
                "todo_id": work["id"],
                "agent_exit": code,
                "gates_ok": ok,
            }
        )
        save_state(state)

        if code != 0:
            print(f"agent exit {code} — stopping", file=sys.stderr)
            return code
        if not ok:
            print("gates failed — stopping", file=sys.stderr)
            return 1

        wid = work["id"]
        if wid not in state.setdefault("completed_ids", []):
            state["completed_ids"].append(wid)
        save_state(state)

        impl, total = registry_progress()
        if impl >= total:
            print("sim-plan-loop: registry complete")
            subprocess.run(["bash", str(ROOT / "scripts/sim-plan-daily-report.sh")], check=False)
            return 0

        iteration += 1
        if args.once:
            break

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
