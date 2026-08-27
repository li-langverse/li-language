# P-float: sqrt_open_bound uses li_rt_sqrt

## Summary

`sqrt_open_bound.li` now calls `li_rt_sqrt` instead of identity `return x`; the `abs(result² - x) < ε` ensures goal remains intentionally open in the discharge corpus.

## Agent continuation

1. **Read:** `li-tests/contracts_verify/sqrt_open_bound.li`, `contracts_discharge_corpus.sh`.
2. **Run:** `lic build --allow-open-vc li-tests/contracts_verify/sqrt_open_bound.li` (pass); without flag (fail on open goals).
3. **Next:** Lean lemmas for `Float.abs` + libm error model, or tier-1 bench refresh.
4. **Blocked on:** **G-hw** / libm ULP proof — do not close with `sorry` without policy sign-off.

## Changed

| Path | Evidence |
|------|----------|
| `li-tests/contracts_verify/sqrt_open_bound.li` | `return li_rt_sqrt(x)` |

## Not changed

- Corpus policy: goal must stay open (`check-autovc-open-goals.sh` in `contracts_discharge_corpus.sh`).
- Full NumPy broadcast.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A |
| **Security** | N/A |
| **Performance** | N/A |
| **Downstream** | N/A |
