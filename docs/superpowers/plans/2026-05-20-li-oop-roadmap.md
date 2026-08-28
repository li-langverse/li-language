# Full OOP roadmap (Phase 2j)

> **Depends on:** **2g** (v1 — `def`, `object`, field `private`/`public`, PascalCase types)  
> **Does not block:** **7** (HPC), **2i** (linalg) — but **method VCs** and **trait proofs** extend **2e–2f**  
> **Out of scope here:** **Phase H** (li-httpd), `packages/li-http/**`, `packages/li-net-httpd/**`

**Goal:** Li ships **proved, readable OOP** — Nim-style `object` + methods + visibility + traits — not a Java runtime class system. User code stays **`def`-only** at the surface; the compiler elaborates methods and enforces encapsulation in **typecheck + MIR + Lean**.

**Naming:** [Naming conventions](../../language/naming-conventions.md) — **PascalCase** type names (`RigidBody`), **snake_case** methods and fields.

---

## Today (Phase 2g v1 — done)

| Feature | State | Evidence |
|---------|--------|----------|
| `type T = object` with fields | **Done** | `li-tests/objects/*`, physics packages |
| `public` / `private` **fields** | **Done** | `private_field_access.li` → compile_fail |
| Free `def` + `import` | **Done** | `encapsulation/`, `modules/` |
| Object MIR lowering (scalars, arrays) | **Done** | `object_*` tests, composable physics smoke |
| `var T` object params | **Done (2j-c)** | MIR `__li_o_wb*` copy-in/out for `var` object first param + `Ident` receiver |
| Methods on types | **Done (2j-a)** | `obj.method(args)` → `Type_method(self, …)`; `def_method_*.li` |
| Private **methods** | **Done (2j-b)** | `private def` not merged on import; `private_method_use.li` |
| Object mutation to caller | **Done (2j-c)** | `c.bump()`, `rigid_integrate(body, …)` without return-assign |
| Inheritance / `override` | **Done (2j-d)** | `object of Base`, static subtyping, `@override` signature check |
| Traits (`Hash`, …) | **Done (2j-e)** | `type Hash = trait`; `def f[T: Hash]`; `trait_hash_impl.li` / `trait_missing_impl.li` |
| Method VCs at call site | **Done (2j-f)** | `check_requires_at_method_call`; `method_call_requires_*.li` |
| Cross-module private **fields** | **Done** | `leak_imported_vault.li` compile_fail |

**Honest label:** **2g = records + field privacy**, not full OOP. This plan is **2j**.

---

## Phase 2j — milestones

### 2j-a — Methods and `self` (syntax + typecheck)

| Deliverable | Detail |
|-------------|--------|
| Parse | `def step(self: var Body, dt: float) -> unit` on types or module block adjacent to `type` |
| Desugar | `body.step(dt)` → `Body_step(body, dt)` (mangled module-qualified name) |
| `self` | First parameter; `var self` for mutating methods |
| CI | `li-tests/encapsulation/def_method_call.li` verify_ok; `def_method_parse.li` |

**Exit:** build + verify green on method call sugar; no new runtime dispatch.

### 2j-b — Visibility for methods and cross-module fields

| Deliverable | Detail |
|-------------|--------|
| `private def` / `public def` | Module- or type-scoped visibility (pick one; document in handbook) |
| Cross-module | `private` fields not accessible via import merge (E02xx) |
| CI | `private_field_cross_module.li` compile_fail; `private_method_use.li` compile_fail |

**Exit:** private **methods** + module visibility tests; **G-def** → **Partial+** in [provability-gaps](../../verification/provability-gaps.md). (Cross-module **field** privacy: `leak_imported_vault.li` already green.)

### 2j-c — Object semantics (mutation + call conventions)

| Deliverable | Detail |
|-------------|--------|
| `var RigidBody` write-back | After `integrate(body, dt)`, caller `body` slots updated in MIR |
| Whole-object assign | Already partial; align with method receiver |
| Borrow | `borrowck` knows method calls as moves on `var self` |
| CI | Extend `import_physics_runtime.li` to assert post-step `pz`; `object_method_mutate.li` |

**Exit:** composable physics smoke asserts integrated state, not only version int.

### 2j-d — Nominal subtyping (inheritance v1)

| Deliverable | Detail |
|-------------|--------|
| Syntax | `type Derived = object of Base` or Nim-style `object of Base` field block |
| Subtyping | `Derived` assignable where `Base` expected (fields must extend layout) |
| No virtual dispatch v1 | Static resolution only; `override` annotation stored, checked name match |
| CI | `inheritance_layout.li`, `override_mismatch.li` compile_fail |

**Exit:** handbook + spec updated; no vtables in LLVM yet.

### 2j-e — Traits and static dispatch

| Deliverable | Detail |
|-------------|--------|
| `type Hash = trait` with required `def hash(self: Self) -> u64` | Parse + check impl on `object` types |
| Bounds | `def sort[T: Hash](xs: list[T])` — reuse **2b** generics |
| Proof | Trait laws as optional `requires` templates (document trusted vs proved) |
| CI | `trait_hash_impl.li`, `trait_missing_impl.li` compile_fail |

**Exit:** `dict[K,V]` can require `K: Hash` in typechecker (may stay stub if collections phase lags).

### 2j-f — Lean VCs for methods (2e extension)

| Deliverable | Detail |
|-------------|--------|
| AutoVC | Callee `requires` on desugared `Type_method` names |
| `ensures` on methods | `result` + `old(self.field)` sugar (spec) |
| Invariant | Optional `invariant` on `var self` loops inside methods |

**Exit:** proof-corpus row for method call-site `requires`; tracked in [proof-corpus-roadmap](../../verification/proof-corpus-roadmap.md).

---

## Explicit non-goals (2j)

| Item | Reason |
|------|--------|
| Runtime `class` keyword | Li keeps `type = object` |
| Multiple inheritance | Deferred; traits cover sharing |
| Virtual dispatch / vtables | **2j-d** static only; dynamic → v2 research |
| Reflection / `getattr` | Breaks proof story |
| HTTPd / net types | **Phase H** separate plan |

---

## Compiler / gap mapping

| Gap ID | After 2j |
|--------|----------|
| **G-def** | **Partial** → **Done** when 2j-b + 2j-c exit gates pass |
| **G-oop** (new) | Tracks methods, traits, inheritance — see provability-gaps |
| **G-vc** | Extends for method contracts (**2j-f**) |

---

## Learned from

| System | Take |
|--------|------|
| **Nim** | `object` layout, `method` desugar, visibility |
| **Rust** | `&mut self` borrow story — align with existing `borrowck` |
| **Python** | `def` readability; no hidden `self` injection without source contract |
| **Lean** | Struct fields + lawful typeclasses — trait proof story |

---

## Suggested execution order

```text
2j-a (methods) → 2j-b (visibility) → 2j-c (mutation/write-back)
       ↘
        2j-d (inheritance) — parallel once 2j-c stable
        2j-e (traits) — after 2b generics stable
        2j-f (Lean) — with 2e corpus
```

**Parallel safe (non-httpd):** **7d-c** disjoint AST, **2i** matmul, proof corpus — no dependency on 2j until method VCs (**2j-f**).

---

## Handbook & agent links

- [Types and data](../../language/types-and-data.md)
- [Naming conventions](../../language/naming-conventions.md)
- [Philosophy](../../language/philosophy.md)
- Master plan: [2026-05-14-li-master-plan.md](2026-05-14-li-master-plan.md) § Phase 2j
