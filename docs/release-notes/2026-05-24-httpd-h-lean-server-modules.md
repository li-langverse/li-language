# HTTPd — Lean on li-net-httpd (h-lean-server-modules)

## Summary

Completes plan todo **h-lean-server-modules**: `packages/li-net-httpd` (`lib.li` + `main.li`) builds with default Lean typecheck (`lake build AutoVC`) when `lake` is installed; ship binary build drops `--no-lean-verify` in that case.

## Agent deliverable

- [x] VC emitter: call-site requires include caller locals referenced in substituted requires (e.g. loop index `i`, `n - k`).
- [x] `scripts/check-httpd-server-lean-gate.sh` + `HTTPD_SERVER_LEAN_MAX_OPEN` (default 8).
- [x] `scripts/build-li-httpd.sh` — Lean on when `lake` present.
- [x] `scripts/httpd-plan-gates.sh` — runs server lean gate.
- [x] Plan `h-lean-server-modules: completed`.

## Tests

```bash
./scripts/build.sh
./scripts/check-httpd-server-lean-gate.sh
./scripts/check-httpd-lean-gate.sh
./scripts/build-li-httpd.sh
./scripts/httpd-plan-gates.sh   # or HTTPD_GATES_SKIP_LIC_BUILD=1 for Python-only slice
```

## Not changed

- Full discharge of every open VC on the ship binary (≤8 open goals with `--allow-open-vc`).
- Tier-5 bench matrix / live Pages refresh.

## Breaking

N/A
