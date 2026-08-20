# Publishing li-nanoreactor

1. Update `CHANGELOG.md` and `li.toml` `version` together.
2. `lic check --workspace packages/li-nanoreactor/li.toml` (or from the monorepo workspace root).
3. Run the package test manifest (`li-tests/manifest.toml`).
4. Tag `vX.Y.Z` on GitHub; org mirror `li-langverse/li-nanoreactor`.
