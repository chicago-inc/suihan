# Suihan DeltaR.k Ledger

First-class ledger of unresolved casts produced by compiler traversals.

`DeltaR.k` is not an embarrassment to hide. It is the part of the transform that remains unresolved, deferred, or bounded by the Lakatos barrier. If it is not logged, the bootstrap appears more complete than it actually is.

---

## Status Meanings

| Status | Meaning |
|---|---|
| `resolved` | no known unresolved cast remains for the current bounded domain |
| `bounded` | unresolved cast exists but is intentionally deferred — typically a Lakatos residual |
| `open` | unresolved cast exists and is currently a correctness or completeness risk |

---

## Ledger Entries

### EQ-SUHC-001 Enum Definition (M1)

- Status: resolved
- `DeltaR.k`: empty. All enum dimensions (Kind, DeclType, ExprType, TokenType) emit to all three targets with H=0.

### EQ-SUHC-002 String Table (M2)

- Status: resolved
- `DeltaR.k`: empty. Forward + reverse lookups generated for every declared dimension.

### EQ-SUHC-003 Kind Inference (M3)

- Status: resolved
- `DeltaR.k`: empty. Every DeclType resolves to exactly one Kind; kindcheck enforces no fallthrough.

### EQ-SUHC-004 Emitter Dispatch (M4)

- Status: resolved
- `DeltaR.k`: empty. Three-target dispatch table fully declarative.

### EQ-SUHC-005 Expression Algebra (M5)

- Status: bounded
- Current `DeltaR.k`:
  - `TOK_PIPE` parsing retains a hand-C path (explicit Lakatos residual; pipe operator semantics depend on in-flight compiler state not accessible to the declarative layer)
- Resolution owner: `src/parse_expr.c` + future `compiler/pipe_semantics.szh` if the Lakatos barrier shifts

### EQ-SUHC-006 Parser Dispatch (M6)

- Status: bounded
- Current `DeltaR.k`:
  - Lookahead disambiguator remains hand-C (reserved keywords appearing as dimension member names require multi-token lookahead)
- Resolution owner: `src/parse_decl.c`; bounded by CONSTITUTION.md §Reserved Keywords

### EQ-SUHC-007 Bootstrap Verification (M7)

- Status: bounded
- Current `DeltaR.k`:
  - Pratt algorithm, AST memory management, lexer FSM, file I/O remain procedural (CONSTITUTION.md §Self-Hosting declares these the Lakatos barrier)
- Resolution owner: these are *not* resolution targets — they are the declared boundary. Movement requires constitutional amendment.

### EQ-SUHC-008 Audit Convergence (auxiliary)

- Status: open
- Current `DeltaR.k`:
  - Audit does not yet measure app-integration coverage (the ordbok→Spoxis consumer layer)
  - Convergence ratio does not incorporate downstream consumer drift
- Resolution owner: `src/audit.c` + dependency on EQ-SUHC-009 closure

### EQ-SUHC-009 App Integration (open)

- Status: open
- Current `DeltaR.k`:
  - No `make integrate-app` target exists; ordbok outputs sit unused in `ordbok/out/`
  - Spoxis hand-authors mirrors of ordbok-declared shapes
  - No CI check verifies consumer drift
- Resolution owner: future `Makefile` target + Spoxis repo tooling

### EQ-SUHC-010 Mongwu Integration (future)

- Status: open (not yet registered in `EQUATION_REGISTRY.md`)
- Current `DeltaR.k`:
  - Kernel assembly emission target not yet implemented
  - `mongwu/ROADMAP.md` describes the consumer; no compiler support
- Resolution owner: future emission target + assembly-emit milestone

---

## Cross-Equation Casts

Suihan does not currently produce cross-equation casts of the kind Spoxis' ledger tracks (the "RPC-Level Casts" section). Compiler passes are strictly ordered — a value does not cross perpendicular dimensions mid-traversal. If a future change introduces cross-pass value movement, add a `Cross-Pass Casts` section here following Spoxis' RPC-Level Casts template.

---

## Promotion Rules

- `open` → `bounded`: requires explicit Lakatos justification in `CONSTITUTION.md §Self-Hosting` with H-reduction claim.
- `open` → `resolved`: requires `make bootstrap` green and audit reporting no Yoneda gap for the traversal.
- `bounded` → `open`: if the Lakatos barrier is later shown to be penetrable, demote to open and schedule a milestone.
- `bounded` → `resolved`: rare; happens when a Lakatos residual becomes declarable (see amendment protocol).
