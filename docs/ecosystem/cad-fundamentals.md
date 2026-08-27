# CAD fundamentals — Li v1 (proof-first)

**Status:** v1 gap analysis + thin API surface proposal — no kernel FFI in v1.  
**Research goal:** `cad_fundamentals` (li-cursor-agents `config/goal-scaffolds/cad_fundamentals.md`).

## North star

Geometry/CAD support for the scientific Li ecosystem: favor small **std** slices and composable **packages/** over a monolithic CAD kernel.

## Gap table (v1)

| Area | Mature references | Li v1 stance |
|------|-------------------|--------------|
| B-rep / solid modeling | OpenCascade, CGAL | Document gaps; no FFI unless already in tree |
| Mesh / manifold ops | Manifold, libigl | Prefer proved mesh types in a future `li-std-geometry` slice |
| Constraints / sketches | SolveSpace, FreeCAD | Defer; types-only sketch in v1 API proposal |
| IO (STEP/IGES) | OCCT, Open CASCADE | **Out of scope** v1 |

## Learned from

| System | Takeaway for Li |
|--------|-----------------|
| **OpenCascade** | Heavy kernel; Li should expose thin, contract-backed wrappers only behind explicit `PKG-*` |
| **CGAL** | Exact predicates + Epsilon policies → tier-0 numerics discipline (see below) |
| **Manifold** | Fast mesh booleans; good reference for future SIMD-friendly batch APIs |

## Proposed thin API surface (types + contracts only)

v1 does **not** implement a kernel. Proposed names for a future package (illustrative):

```nim
type Vec3 = object
  x, y, z: float

def vec3_add(a, b: Vec3) -> Vec3
  requires true
  ensures true   # component-wise; refine with finiteness when float proofs land
  decreases 0
```

- Keep geometry types in `packages/` until std promotion review.
- Every export: mandatory contracts; no `trusted.lean` changes in agent PRs.

## Tier-0 numerics

- Document epsilon policy for degenerate triangles and coplanar facets before any float geometry ships.
- Reuse `li-std-math` / numerics patterns; do not weaken `stdlib_seal` or coverage tiers.

## Out of scope (v1)

CAD editor UI, STEP/IGES importers, GPU tessellation, lowering `std/**` coverage below 100%.

## Evidence for implement PRs

- Research handoff / session id `cad_fundamentals` in PR body.
- Tests under `li-tests/` or package `lit` with ≥80% before `lip publish` promotion.
