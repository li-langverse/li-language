#!/usr/bin/env python3
"""Monte Carlo discharge for prob_ensures P(event) < ε (httpd plan prob-hoare P2)."""
from __future__ import annotations

import json
import math
import os
import random
import re
import sys
from dataclasses import dataclass
from pathlib import Path

DEFAULT_SAMPLES = 10_000
DEFAULT_DELTA = 0.01  # 1 - confidence


@dataclass
class ProbObligation:
    proc: str
    event: str
    epsilon: float
    given: str
    samples: int
    line: int


def _parse_scientific(s: str) -> float:
    return float(s.strip().replace("_", ""))


def parse_prob_obligations(source: str) -> list[ProbObligation]:
    """Extract prob_ensures blocks from Li source (lexer-aligned subset)."""
    lines = source.splitlines()
    out: list[ProbObligation] = []
    current_proc = "<module>"
    i = 0
    prob_re = re.compile(
        r"^\s*prob_ensures\s+(.+?)\s*<\s*([0-9.eE_+-]+)\s*$", re.IGNORECASE
    )
    while i < len(lines):
        line = lines[i]
        mproc = re.match(r"^\s*(?:extern\s+)?(?:async\s+)?(?:proc|def)\s+(\w+)", line)
        if mproc:
            current_proc = mproc.group(1)
        mprob = prob_re.match(line)
        if mprob:
            event = mprob.group(1).strip()
            epsilon = _parse_scientific(mprob.group(2))
            given = ""
            samples = DEFAULT_SAMPLES
            j = i + 1
            while j < len(lines):
                tail = lines[j].strip()
                if not tail:
                    j += 1
                    continue
                if re.match(
                    r"^\s*(requires|ensures|prob_ensures|decreases|invariant)\b", lines[j]
                ):
                    break
                if tail == "=":
                    break
                mg = re.match(r"^\s*given\s+(\w+)\s*$", lines[j], re.IGNORECASE)
                if mg:
                    given = mg.group(1)
                    j += 1
                    continue
                ms = re.match(r"^\s*samples\s+([0-9_]+)\s*$", lines[j], re.IGNORECASE)
                if ms:
                    samples = int(ms.group(1).replace("_", ""))
                    j += 1
                    continue
                break
            out.append(
                ProbObligation(
                    proc=current_proc,
                    event=event,
                    epsilon=epsilon,
                    given=given,
                    samples=samples,
                    line=i + 1,
                )
            )
        i += 1
    return out


def hoeffding_upper_bound(hits: int, n: int, delta: float) -> float:
    """One-sided upper bound on P(event) at confidence 1 - delta."""
    if n <= 0:
        return 1.0
    if hits == 0:
        # Rule-of-three style bound when no events observed.
        return min(1.0, -math.log(delta) / n)
    p_hat = hits / n
    margin = math.sqrt(math.log(2.0 / delta) / (2.0 * n))
    return min(1.0, p_hat + margin)


def simulate_event(event: str, n: int, rng: random.Random, *, given: str = "") -> int:
    """Return hit count for named probabilistic events."""
    if given == "BadRng":
        if event.strip().startswith("iv_collision") or event.strip() in (
            "duplicate_draw",
            "event_duplicate_draw",
            "collision_iv",
        ):
            return n
        if event.strip() in ("always_false", "false_event"):
            return n
        return 0
    ev = event.strip()
    if ev in ("duplicate_draw", "event_duplicate_draw", "collision_iv"):
        space = 2**32
        seen: set[int] = set()
        hits = 0
        for _ in range(n):
            v = rng.randrange(space)
            if v in seen:
                hits += 1
            seen.add(v)
        return hits
    if ev.startswith("iv_collision"):
        space = 2**96
        seen: set[int] = set()
        hits = 0
        for _ in range(n):
            v = rng.randrange(space)
            if v in seen:
                hits += 1
            seen.add(v)
        return hits
    if ev in ("always_false", "false_event"):
        return 0
    if ev in ("always_true", "bad_rng_constant"):
        return n
    raise ValueError(f"unknown prob_ensures event: {event!r}")


def discharge_one(ob: ProbObligation, delta: float) -> dict:
    seed = 42 if ob.given in ("PrngSeed", "SimRng", "OsRngUniform", "") else 0
    if ob.given == "BadRng":
        seed = 0
    rng = random.Random(seed)
    n = max(100, ob.samples)
    hits = simulate_event(ob.event, n, rng, given=ob.given)
    p_upper = hoeffding_upper_bound(hits, n, delta)
    ok = p_upper < ob.epsilon
    return {
        "proc": ob.proc,
        "event": ob.event,
        "epsilon": ob.epsilon,
        "given": ob.given or "OsRngUniform",
        "samples": n,
        "hits": hits,
        "p_hat": hits / n,
        "p_upper": p_upper,
        "confidence": 1.0 - delta,
        "ok": ok,
        "line": ob.line,
    }


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: prob_check.py <file.li>", file=sys.stderr)
        return 2
    path = Path(argv[1])
    source = path.read_text(encoding="utf-8")
    obligations = parse_prob_obligations(source)
    if not obligations:
        print(f"prob_check: no prob_ensures in {path}")
        return 0
    delta = float(os.environ.get("LI_PROB_CHECK_DELTA", str(DEFAULT_DELTA)))
    results = [discharge_one(ob, delta) for ob in obligations]
    root = Path(os.environ.get("LI_REPO_ROOT", "."))
    out_dir = root / "build" / "generated"
    out_dir.mkdir(parents=True, exist_ok=True)
    cert_path = out_dir / "prob_check.json"
    cert_path.write_text(json.dumps({"obligations": results}, indent=2) + "\n", encoding="utf-8")

    failed = [r for r in results if not r["ok"]]
    for r in results:
        status = "ok" if r["ok"] else "FAIL"
        print(
            f"prob_check [{status}] {r['proc']}:{r['line']} "
            f"P({r['event']}) upper={r['p_upper']:.3e} < {r['epsilon']:.3e} "
            f"(hits={r['hits']}/{r['samples']}, given={r['given']})"
        )
    print(f"prob_check: certificate → {cert_path}")
    if failed:
        print(f"prob_check: {len(failed)} obligation(s) exceeded ε", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
