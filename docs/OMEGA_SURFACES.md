# Suihan Omega Surfaces

Canonical output targets for compiler traversals. `omega` is the transformed condition that must exist after a milestone closes.

Use this document to stop declaring a milestone "done" because a single target emits correctly. In Suihan, an equation closes only when all three emission targets agree *and* `make bootstrap` reaches byte-identity.

---

## Surface Rules

1. `omega` includes both emitted code (visible) and bootstrap byte-identity (hidden required side effect).
2. A degraded-valid state is allowed only if it is explicit and bounded — e.g., M5's `TOK_PIPE` residual is bounded Lakatos.
3. A forbidden partial state counts as incomplete even if the compiler builds.
4. `OMEGA-SUHC-*` shadows `EQ-SUHC-*`. Every equation has a matching omega.

---

## Canonical Omega Definitions

### OMEGA-SUHC-001 Enum Definition (M1)

- Required visible outputs: `kind.h`, `decl_type.h`, `expr_type.h`, `token_type.h` + TS unions + SQL types
- Required hidden side effects: enum values stable across targets (same ordering, same zero)
- Degraded-valid states: none (H=0 required)
- Forbidden partial states: C target emits new member but TS target still has old union; stage-2 `diff` non-empty

### OMEGA-SUHC-002 String Table (M2)

- Required visible outputs: `kind_name()`, `decl_type_name()`, etc. + `*_from_name()` reverse lookups
- Required hidden side effects: string table and enum agree on member ordering
- Degraded-valid states: none
- Forbidden partial states: forward lookup exists but reverse lookup missing; target drift across C/TS/SQL

### OMEGA-SUHC-003 Kind Inference (M3)

- Required visible outputs: `infer_kind(decl_t)` across all three targets; kindcheck rejects unkinded AST nodes
- Required hidden side effects: every DeclType has exactly one Kind
- Degraded-valid states: none
- Forbidden partial states: new DeclType added to ordbok but no Kind projection; default-fallthrough silently assigned

### OMEGA-SUHC-004 Emitter Dispatch (M4)

- Required visible outputs: `emit_decl_c()`, `emit_decl_ts()`, `emit_decl_sql()` with full DeclType coverage
- Required hidden side effects: every handler produces an equivalent shape across targets
- Degraded-valid states: none
- Forbidden partial states: new DeclType routes to C emitter only; target-specific handler throws at runtime

### OMEGA-SUHC-005 Expression Algebra (M5)

- Required visible outputs: precedence table emitted; math-function projections applied per target
- Required hidden side effects: Pratt dispatch consumes the emitted table at runtime
- Degraded-valid states: `TOK_PIPE` retains hand-C parse path (bounded Lakatos)
- Forbidden partial states: new operator added without precedence row; SQL target uses TS operator spelling

### OMEGA-SUHC-006 Parser Dispatch (M6)

- Required visible outputs: keyword→handler table; kind-sigil→kind table
- Required hidden side effects: lookahead disambiguator agrees with table
- Degraded-valid states: lookahead remains hand-C (bounded Lakatos)
- Forbidden partial states: new keyword accepted by lexer but not parser; kind sigil parsed but kind unresolved

### OMEGA-SUHC-007 Bootstrap Verification (M7)

- Required visible outputs: `make bootstrap` reports all 63 comparisons == 0 diff; full test suite passes on stage-2
- Required hidden side effects: committed `include/gen/` headers match regenerated stage-2 outputs byte-for-byte
- Degraded-valid states: none — bootstrap is the terminal test
- Forbidden partial states: "bootstrap looks fine" without running it; any diff suppressed by `-q` flag

### OMEGA-SUHC-008 Audit Convergence (auxiliary)

- Required visible outputs: audit report with S-scores, exhaustiveness gaps, Yoneda gaps; convergence ratio
- Required hidden side effects: gap list persisted for trend analysis
- Degraded-valid states: r approaching 1 while new ordbok areas open (explicit note required)
- Forbidden partial states: audit reports "clean" while known-open traversals (`EQ-SUHC-009`) remain in ledger

### OMEGA-SUHC-009 App Integration (open)

- Required visible outputs: Spoxis consumes ordbok-emitted types directly with no shim; `make integrate-app` exists
- Required hidden side effects: Spoxis CI verifies the integration (type-check + drift check)
- Degraded-valid states: none yet defined — target state is full automation
- Forbidden partial states: ordbok emits types into `ordbok/out/` and Spoxis hand-authors mirrors; the "it works" illusion of a manual copy step

---

## Reading Omega in Reverse

If a milestone's omega is achieved but the corresponding equation's `DeltaR.k` grows, closure is fake — the gains were offset by new unresolved casts. A real close shrinks or bounds the delta and produces the omega.
