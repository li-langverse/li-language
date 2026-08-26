# Refinement types

A **refinement type** declares which values are allowed for a name by attaching a boolean **predicate** to a base type.

```li
type NonNeg = {x: int | x >= 0}
```

Read this as: “an `int` named `x` such that `x >= 0`.” The binder (`x`) is used only inside the predicate; the parameter name at a call site can differ.

## Where refinements apply

| Site | Checked today |
|------|----------------|
| Function / method parameter at **call** | Yes — **E0305** when provably outside the bound |
| **`var` initializer** | Yes — **E0305** when provably outside the bound |
| Array index (`Index10`, etc.) | Partial — index refinements + existing bounds paths |
| Return type refinements | Not yet |

Refinements compose with callee **`requires`** clauses: both must hold. Use refinements to document the **type domain**; use `requires` for **procedure-specific** preconditions.

## Enforcement (open + strict)

The compiler uses the same three-way rule as call-site `requires`:

1. **Provably inside** the refinement (literal, or `var y = 5` folded into `callee(y)`) → OK.
2. **Provably outside** (e.g. `callee(-1)` with `NonNeg`) → **compile error E0305** with a plain-language message.
3. **Not provable yet** (parameter, computed value) → no false error; a **Lean VC** is emitted at the call / init site and **`lic build` fails** if the goal stays open (unless `--allow-open-vc`).

You declare possible values once on the type; callers stay free to pass any value **inside** that declaration, and the toolchain blocks values **outside** it when it can prove the violation.

## Examples

**Alias (recommended for reuse):**

```li
type NonNeg = {x: int | x >= 0}

def sqrt_int(x: NonNeg) -> int
  requires true
  ensures result >= 0
  decreases 0
=
  return x
```

**Inline on a parameter:**

```li
def clamp_pos(v: {n: int | n >= 0}) -> int
  ...
=
  return v
```

**Negative test (compile fail):**

```li
return sqrt_int(-1)   # E0305: value outside NonNeg
```

## Error code

| Code | Meaning |
|------|---------|
| **E0305** | `type.refinement_not_met` — value provably violates a refinement type |

See also [Contracts and proofs](contracts-and-proofs.md) and [Types and data](types-and-data.md).
