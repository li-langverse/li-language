# Publishing the physics module branch

The cloud agent (`cursor[bot]`) cannot push to `li-langverse/lic`. Use a maintainer PAT:

```bash
cd lic   # or your clone
git fetch origin
git checkout feat/physics-module-packages
# if branch exists only locally:
# git cherry-pick 8c95e10..HEAD

git push -u origin feat/physics-module-packages
gh pr create --base main --head feat/physics-module-packages \
  --title "feat(physics): li-std-physics* packages and Tier-2 benches" \
  --body-file docs/release-notes/2026-05-16-physics-module-packages.md
```

Pair with **benchmarks** PR: https://github.com/li-langverse/benchmarks/pull/6
