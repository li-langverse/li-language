# Release notes: 2026-05-25 — g-math-syn-for-range

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PH / REQ:** PH-2h, **G-math-syn**

## Summary (one sentence)

Documents and tests **`for i in start..<end`** as a **G-math-syn** closed slice; Python `range()` remains open.

## Agent continuation (required)

1. Read: `docs/verification/provability-gaps.md` and `li-tests/math_syntax/for_range_sum.li`.
2. Run: `LI_REPO_ROOT=$PWD ./li-tests/run_all.sh math_syntax`.
3. Then: `range(n)` helper or dynamic bounds if needed.
4. Blocked on: none for this slice.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| Corpus | `math_syntax/for_range_sum.li` | `verify_open_ok` |
| Register | `provability-gaps.md` **G-math-syn** | Partial |

## Not changed (scope fence)

- Python `range()` — not in this PR
- **G-par**, **G-dec**, **G-vc** — other open PRs (#193, #196)

## Breaking changes

None.

## Security

N/A.

## Performance

N/A.

## Downstream

N/A.
