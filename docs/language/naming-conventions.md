# Naming conventions

Canonical style for Li source, packages, and docs. Li favors **Python readability** for functions and variables; **class-like types** use **PascalCase** (`ClassName`).

## Quick reference

| Kind | Casing | Example |
|------|--------|---------|
| **Type / object** (class name) | **PascalCase** | `RigidBody`, `PhysicsWorld`, `Vec3` |
| **`def` name** | snake_case | `rigid_integrate_semi_implicit`, `step_world` |
| **Variable / parameter** | snake_case | `time_step`, `inv_mass` |
| **Object field** | snake_case | `px`, `velocity`, `inv_mass` |
| **Module import** | lowercase dotted | `physics.rigid`, `math.numerics` |
| **Generic parameter** | single uppercase letter or short PascalCase | `T`, `S` |

## Class names → PascalCase (`ClassName`)

Li does not use a `class` keyword today. User-defined **class-like** types are written as:

```li
type RigidBody = object
  public px: float
  public py: float
  ...
```

Treat **`type` / `object` names** like class names in Java, C#, or TypeScript:

| Do | Don't |
|----|--------|
| `RigidBody`, `PhysicsWorld`, `CollisionPair` | `rigid_body`, `physicsWorld`, `RIGID_BODY` |
| `Vec3`, `NumericalTargets` | `vec3` (as a type name), `numerical_targets` (as a type name) |

**Rule:** every word in a type name starts with an uppercase letter; no underscores in the type identifier.

### Terminology

| Name | Shape | Used in Li for |
|------|--------|----------------|
| **PascalCase** (UpperCamelCase) | `ClassName` | **Types / objects** |
| **camelCase** (lowerCamelCase) | `className` | **Not** used for types; avoid for new APIs |
| **snake_case** | `class_name` | **Functions, variables, fields, modules** |

When docs say “class name,” they mean the **PascalCase type name** (`RigidBody`), not a separate `class` declaration.

## Functions (`def`)

- **snake_case** verb phrases: `apply_gravity`, `load_routes_from_toml`
- Not PascalCase, not camelCase: `ApplyGravity` and `applyGravity` are wrong for `def` names

## Variables and fields

- **snake_case** nouns: `time_step`, `open_rate`
- Short names only in tiny scopes: loop indices `i`, `j`
- Booleans: `is_`, `has_`, `can_` prefixes — `is_visible`, `has_collision`

## Packages and imports

See [import-style.md](import-style.md). Summary:

- Import path: `physics.runtime` (domain words, lowercase, dots)
- GitHub repo: `li-physics-runtime` (`li-` + dots → hyphens)

## Examples (consistent module)

```li
import physics.rigid

type RigidBody = object
  public px: float
  public inv_mass: float

def rigid_integrate_semi_implicit(b: var RigidBody, dt: float) -> unit
  requires dt > 0.0
  ensures true
  decreases 0
=
  ...
```

| Identifier | Role | Casing |
|------------|------|--------|
| `physics.rigid` | import | dotted lowercase |
| `RigidBody` | type / object | PascalCase |
| `px`, `inv_mass` | fields | snake_case |
| `rigid_integrate_semi_implicit` | `def` | snake_case |
| `b`, `dt` | locals / params | short or snake_case |

## Agents and reviewers

1. New `type … = object` names must be **PascalCase** before merge.
2. Do not rename existing public types without an ADR / breaking note in release notes.
3. Stdlib and `packages/*` should follow this table in new code; legacy snake type names are migrated opportunistically.

## Related

- [Philosophy](philosophy.md) — readability and prose
- [Types and data](types-and-data.md) — `object`, fields, visibility
- [Import style](import-style.md)
- [Repo naming](../ecosystem/repo-naming.md)
