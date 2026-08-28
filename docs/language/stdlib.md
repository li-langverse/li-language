# Standard library and prelude

The Li **prelude** is the set of builtin types and functions the compiler knows without an import (`int`, `list`, `dict`, `print`, …).

## Sealed names (security)

You **cannot** define your own `def` or `type` with a name reserved for the prelude or for symbols exported from `std/`. Attempting to do so fails at compile time with `stdlib_symbol_shadow`.

Execution decorator names (`@parallel`, `@cpu`, …) are also reserved; see [Decorators](decorators.md).

## Shipped std/

Today the repository ships minimal std under `std/` (e.g. `std/execution/decorators.li`). As the std tree grows, names exported from those modules are added to the compiler seal list.

## Imports (preview)

`import std_math as m` will bind only the module’s public API; you cannot replace `m` or redefine sealed symbols. Full module resolution is Phase **8a**.
