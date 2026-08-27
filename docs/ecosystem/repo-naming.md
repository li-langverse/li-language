# GitHub repo naming (aligned with imports)

## Rule

**Org mirror repo name = `li-` + import path with dots → hyphens.**

| Import | GitHub repo | Monorepo folder |
|--------|-------------|-----------------|
| `physics.relativity` | `li-physics-relativity` | `packages/li-physics-relativity` |
| `math` | `li-math` | `packages/li-math` |
| `math.numerics` | `li-math-numerics` | `packages/li-math-numerics` |
| `net.httpd` | `li-net-httpd` | `packages/li-net-httpd` |
| `net` | `li-net` | `packages/li-net` |

Metadata in each `li.toml`:

```toml
[package.metadata.li]
import_name = "physics.relativity"
github_repo = "li-physics-relativity"
```

## Legacy names

Older mirrors used `li-std-*` (e.g. `li-std-physics-relativity`). **Do not create new repos with `li-std-` prefix.**

Rename on GitHub (maintainer):

```bash
# Example — GitHub redirects old URLs after rename
gh repo rename li-physics-relativity --repo li-langverse/li-std-physics-relativity
```

Monorepo alignment:

```bash
python3 scripts/align-package-repo-names.py --dry-run
python3 scripts/align-package-repo-names.py --apply
./scripts/push-official-package-repo.sh li-physics-relativity --create
```

## Push mirrors

`push-official-package-repo.sh` uses **folder name** under `packages/` and reads `github_repo` from `li.toml` when set:

```bash
./scripts/push-official-package-repo.sh li-physics-relativity
```
