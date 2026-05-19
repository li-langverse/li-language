# proof_gaps (research fixtures)

Tests that document **known soundness gaps** between Li’s verification story and `lic` today.

These are **not** listed in `manifest.toml` (would fail CI until the Lean gate is wired).
Run manually:

```bash
lic build li-tests/proof_gaps/false_ensures_still_builds.li -o /dev/null   # currently succeeds (gap)
```

See `docs/verification/provability-gaps.md` for the canonical register.
