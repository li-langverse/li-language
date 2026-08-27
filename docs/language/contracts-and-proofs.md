# Contracts and proofs

Li is **provable-only** by design: if proof obligations are not discharged, there should be **no binary**.

!!! note "Implementation status"
    **Today:** every `lic build` emits `build/generated/AutoVC.lean` and runs **Lean typecheck** when `lake` is installed; **open** obligations fail the build unless `--allow-open-vc`. Kernel discharge of all ensures is still **partial** — see **[Provability gaps](../verification/provability-gaps.md)**.

## On every procedure

```nim
def sqrt_pos(x: float) -> float
  requires x >= 0.0
  ensures result >= 0.0
  decreases 0
=
  ...
```

| Clause | Role |
|--------|------|
| `requires` | Precondition — caller must establish this |
| `ensures` | Postcondition — true on return (`result` names the return value) |
| `decreases` | Termination measure for the procedure body |

## On every loop

```nim
while n < limit
  invariant 0 <= n and n <= limit
  decreases limit - n
=
  n = n + 1
```

| Clause | Role |
|--------|------|
| `invariant` | True at the start of each iteration |
| `decreases` | Strictly decreases each iteration — proves the loop ends |

`parallel for` also carries `requires` (disjointness), `invariant`, and `decreases`.

## What gets proved

| Property | How |
|----------|-----|
| Type safety | Static checker |
| Index bounds | Refinements + checks |
| Value domains (`{x: int \| …}`) | Refinement types — **E0305** when provably violated; VC otherwise ([refinement-types](refinement-types.md)) |
| Memory / borrow | Borrow checker |
| Contract obligations | Lean 4 VC generation (**partial** — proc + call-site `requires` + refinement VCs; see [gaps](../verification/provability-gaps.md)) |
| Parallel races | Disjointness + `Send`/`Sync` (**partial** — policy heuristics today) |
| No `Any` / `sorry` | Hard reject |

## `lic check` vs `lic build`

| Command | Proof certificate? |
|---------|-------------------|
| `lic check` | **No** — IDE-speed feedback |
| `lic build` | **Target:** Lean must accept remaining goals · **Today:** static gate only ([gaps](../verification/provability-gaps.md)) |

When Phase **2f** lands, treat `lic build` like signing a theorem: the executable is the certificate artifact.

## Trusted base (tiny)

Only `docs/semantics/trusted.lean` may contain unproved axioms — minimal `IO` and audited `extern`. User application code never goes there.

## Why this is “mathematical”

Proofs are checked by the **Lean 4 kernel**, not by “we ran tests and it looked fine.” See [Why Li is provable](../compiler/why-provable.md).

## Common mistakes

| Mistake | What Li does |
|---------|----------------|
| Missing `decreases` | Compile error |
| `ensures true` on `-> float` / `-> int` / struct | **Compile error E0303** — postcondition must mention `result` |
| `ensures` too weak | May still prove, but you lied — review specs |
| `ensures` too strong | Proof fails — strengthen code or weaken spec honestly |
| Using `sorry` | Rejected |

More: [Verification overview](../verification/overview.md) · [Provability gaps](../verification/provability-gaps.md).
