# EQ-SUHC-003 — Kind Inference Traversal (M3)

## EQ-ID
EQ-SUHC-003

## xi
`ordbok/compiler/kind_inference.szh` — the DeclType → Kind projection rule.

## *zeta
- kindcheck function body per target
- cross-dimension table emission
- runtime `infer_kind(decl_t)` callable

## *x
- DeclType × Kind cross product
- auto-detected cross-dimension mode (M3 is the first such mode)

## bounds
- perpendicularity: DeclType axis and Kind axis remain perpendicular; the projection maps between them without collapse
- exhaustiveness: every DeclType value maps to exactly one Kind
- kind slot: projection output is kind ξ (identity-assigning)

## R.k
1. parse `kind_inference.szh`
2. detect cross-dimension dispatch mode
3. build DeclType → Kind table
4. emit `infer_kind(decl_t)` function per target
5. kindcheck pass consumes the table at compile time

## omega
Every DeclType added to ordbok automatically gets a Kind at parse time. Kindcheck rejects any AST node whose declared kind does not match the inferred kind.

## DeltaR.k
None. Status: `resolved`.

## authority
- Source: `ordbok/compiler/kind_inference.szh`
- Compiler: `src/kindcheck.c`
- Generated: `infer_kind.c`, `infer_kind.ts`, `infer_kind.sql`

## verification
```
make bootstrap
make test        # tests/m3_kind_inference.sh
```

Completion evidence: PROJECT_JOURNAL.md §M3 (H remaining: 0).
