# Import style — easy dotted modules

**Goal:** imports read like other modern languages — `physics.relativity`, not `li_std_physics_relativity`.

## Preferred (ergonomic)

```li
import physics.relativity
import physics.runtime
import math
import math.numerics
import ui
import scene
import io
import csv
```

## Also valid (stdlib tree)

```li
import std.physics.relativity
import std.math
import std.io
import std.csv
```

Maps to files under `std/` (e.g. `std/physics/relativity.li`).

## Legacy (avoid in new code)

```li
import li_std_physics_relativity   # old package snake; prefer physics.relativity
import li_httpd                     # old httpd alias; prefer net.httpd
```

Use **only** in generated code or third-party pins not yet updated.

## Package metadata

Official packages set in `li.toml`:

```toml
[package.metadata.li]
import_name = "physics.relativity"
github_repo = "li-physics-relativity"
```

GitHub org mirror name = `li-` + import with dots → hyphens. See [repo-naming.md](../ecosystem/repo-naming.md).

Run `python3 scripts/apply-import-names.py` after adding a package.

## Rules for new modules

| Domain | Pattern | Example |
|--------|---------|---------|
| Physics | `physics.<area>` | `physics.fluids` |
| Math | `math` or `math.<area>` | `math.numerics` |
| UI / scene | `ui`, `scene` | `import ui` |
| IO / data | `io`, `csv` | `import csv` |
| Network | `net`, `net.httpd` | `import net.httpd` |

**Avoid:** `li-std-*` in user-facing sample code and docs.

## Compiler

`import_resolve.cpp` resolves workspace `import_name` (when `packages/li.toml` is present) → ergonomic `std/*` facades → explicit `std.*` paths → legacy snake / folder names.
