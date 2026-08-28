# Option types and absence

`None` means a value is absent; it is not `unit` and is not an empty collection.

| Concept | Example | Meaning |
|---------|---------|---------|
| No return value | `return` | `unit` |
| Absent value | `None` | `Option[T]` (not yet implemented) |
| Empty collection | `list()` / `dict()` | Collection API, not absence |

The compiler rejects `None` and `Some` with an `Option[T]` diagnostic until
option lowering is implemented. Use `unit` for procedures with no result, and
collection constructors for empty containers.
