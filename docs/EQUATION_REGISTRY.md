# Suihan Equation Registry

Canonical traversal index for the unified equation:

```text
{xi; (*zeta, *x)R.k} : {omega, DeltaR.k}
```

In Suihan, a traversal is an **ordbok-to-emission** path: `.szh` source → parse → kindcheck → perpcheck → exhaustcheck → emit(TS|SQL|C) → bootstrap verification. Each compiler milestone (M1–M7) is a distinct traversal with its own closure condition.

This registry is broader than the M-table in `CONSTITUTION.md §Self-Hosting`. It adds the auxiliary traversals (audit, convergence measurement, diff) when their closure materially affects whether a milestone traversal closes.

---

## Fields

| Field | Meaning |
|---|---|
| `xi` | Initial condition being transformed |
| `*zeta` | Containment and situated shape list (which ordbok files + which AST positions) |
| `*x` | Relational/dependency dimensions used in the transform (kinds, enums, dispatch modes) |
| `bounds` | Compiler-pass gates, Lakatos barrier, emission-target rules |
| `R.k` | Ordered traversal: lex → parse → kindcheck → perpcheck → exhaustcheck → emit → bootstrap |
| `omega` | Required transformed condition (emitted code + passing bootstrap) |
| `DeltaR.k` | Unresolved/deferred casts (residual hand-C, perceptual Lakatos barriers) |
| `authority` | Canonical `.szh` sources, compiler pass, emitter, generated header |
| `completion_test` | `make bootstrap` stage + specific regression test |

---

## Registered Equations

### EQ-SUHC-001 — Enum Definition Traversal (M1)

| Field | Value |
|---|---|
| `xi` | dimension declarations in `compiler/kinds.szh`, `compiler/decl_types.szh`, `compiler/expr_types.szh`, `compiler/token_types.szh` |
| `*zeta` | enum position (TS union type, C11 enum, ordbok declaration) |
| `*x` | dimension members, zero values, canonical ordering |
| `bounds` | parse → perpcheck (members perpendicular to dimension axis); no cross-dimension leakage |
| `R.k` | `.szh` source → parse → dimension resolution → string-table + enum emission |
| `omega` | `include/gen/kind.h`, `decl_type.h`, `expr_type.h`, `token_type.h` generated; stage-1 == stage-2 byte-identical |
| `DeltaR.k` | none (H=0; closed) |
| `authority` | `ordbok/compiler/*.szh`, `src/emit_c.c`, `include/gen/*.h` |
| `completion_test` | `make bootstrap` stage 4 byte-compare on enum headers + regression test `tests/m1_enums.sh` |

### EQ-SUHC-002 — String Table Traversal (M2)

| Field | Value |
|---|---|
| `xi` | enum-dimension sources (output of EQ-SUHC-001) |
| `*zeta` | string-table function position in TS/SQL/C |
| `*x` | member → string projection + reverse (`*_from_name`) |
| `bounds` | exhaustcheck must cover every dimension member; wildcard permitted with warning |
| `R.k` | parse → dispatch-mode inference (1D string-table) → 3-target emission |
| `omega` | `kind_name()`, `decl_type_name()`, etc. in C + TS + SQL; reverse lookups generated |
| `DeltaR.k` | none (H=0; closed) |
| `authority` | `ordbok/compiler/*.szh`, `src/emit_ts.c`, `src/emit_sql.c`, `src/emit_c.c` |
| `completion_test` | bootstrap stage 4 on `_name.c/.ts/.sql` + `tests/m2_string_tables.sh` |

### EQ-SUHC-003 — Kind Inference Traversal (M3)

| Field | Value |
|---|---|
| `xi` | `compiler/kind_inference.szh` (DeclType → Kind projection) |
| `*zeta` | kind-inference function body emitted per target |
| `*x` | DeclType × Kind cross product with auto-detected cross-dimension mode |
| `bounds` | perpcheck (DeclType axis perpendicular to Kind axis); exhaustcheck over DeclType |
| `R.k` | parse → cross-dimension detection → emission of `infer_kind(decl_t)` |
| `omega` | all DeclType values produce a Kind at parse time; kind errors are compile errors |
| `DeltaR.k` | none (H=0; closed) |
| `authority` | `ordbok/compiler/kind_inference.szh`, `src/kindcheck.c` |
| `completion_test` | bootstrap stage 4 + `tests/m3_kind_inference.sh` |

### EQ-SUHC-004 — Emitter Dispatch Traversal (M4)

| Field | Value |
|---|---|
| `xi` | `compiler/emit_dispatch.szh` with `yields ω dispatch` annotation, per target |
| `*zeta` | dispatch table in TS/SQL/C emitter |
| `*x` | DeclType × handler function × emission-target triple product |
| `bounds` | every DeclType must have a handler in every target; no silent fallthrough |
| `R.k` | parse → dispatch-mode detection → per-target emission → linker verification |
| `omega` | `emit_decl(FILE*, Decl*)` dispatches correctly in C, TS, SQL targets |
| `DeltaR.k` | none (H=0; closed) |
| `authority` | `ordbok/compiler/emit_dispatch.szh`, `src/emit_*.c` |
| `completion_test` | bootstrap stage 4 + `tests/m4_emit_dispatch.sh` |

### EQ-SUHC-005 — Expression Algebra Traversal (M5)

| Field | Value |
|---|---|
| `xi` | `compiler/expression_algebra.szh` (precedence, math-function mappings) |
| `*zeta` | Pratt-parser precedence table, operator-to-function projection |
| `*x` | TokenType × precedence × associativity × target-specific operator spelling |
| `bounds` | Lakatos barrier: Pratt algorithm stays in C; only the table is declarable |
| `R.k` | parse → precedence-table emission → Pratt dispatch at runtime |
| `omega` | expressions parse with correct precedence across targets |
| `DeltaR.k` | `TOK_PIPE` residual — pipe operator parsing retains hand-C path; bounded |
| `authority` | `ordbok/compiler/expression_algebra.szh`, `src/parse_expr.c` |
| `completion_test` | bootstrap stage 4 + `tests/m5_expression_algebra.sh` + Pratt regression |

### EQ-SUHC-006 — Parser Dispatch Traversal (M6)

| Field | Value |
|---|---|
| `xi` | `compiler/parser_dispatch.szh` (keyword → handler, kind sigil → kind) |
| `*zeta` | parser dispatch table, kind-sigil disambiguation |
| `*x` | TokenType × parser-handler projection + kind-sigil projection |
| `bounds` | exhaustcheck over reserved keywords; disambiguator lookahead is Lakatos |
| `R.k` | parse → dispatch-mode detection → `parse_decl(Parser*, tok_t)` emission |
| `omega` | every declaration keyword routes to correct parser; every kind sigil resolves |
| `DeltaR.k` | lookahead disambiguator remains hand-C (bounded Lakatos residual) |
| `authority` | `ordbok/compiler/parser_dispatch.szh`, `src/parse_decl.c` |
| `completion_test` | bootstrap stage 4 + `tests/m6_parser_dispatch.sh` |

### EQ-SUHC-007 — Bootstrap Verification Traversal (M7)

| Field | Value |
|---|---|
| `xi` | committed `include/gen/` headers + current ordbok |
| `*zeta` | stage-1 suhc, stage-2 regenerated headers, stage-3 rebuilt suhc |
| `*x` | byte-identity relation across stage-1 and stage-2 outputs |
| `bounds` | all five bootstrap stages must pass; any divergence fails the equation |
| `R.k` | build stage-1 → regenerate → rebuild → byte-compare → run regression |
| `omega` | fixed point reached: compiler and ordbok agree about the compiler |
| `DeltaR.k` | Pratt + FSM + AST + I/O remain procedural (Lakatos barrier; bounded) |
| `authority` | `Makefile` bootstrap target, `make check`, `make test` |
| `completion_test` | `make bootstrap` all 63 comparisons == 0 diff + full test suite pass |

### EQ-SUHC-008 — Audit Convergence Traversal (auxiliary)

| Field | Value |
|---|---|
| `xi` | current ordbok + compiler binary |
| `*zeta` | per-file S score, exhaustiveness gap count, Yoneda gap list |
| `*x` | observed vs. declared morphism coverage |
| `bounds` | convergence ratio r < 1 required; r >= 1 signals Lakatos or divergence |
| `R.k` | `./suhc --audit` → S-score computation → gap enumeration → convergence ratio |
| `omega` | audit report with actionable P0 gaps; convergence ratio below ceiling |
| `DeltaR.k` | open: app-integration coverage not yet in audit scope |
| `authority` | `src/audit.c`, `./suhc --audit`, `./suhc --convergence` |
| `completion_test` | audit report parses; convergence < 1 on stable ordbok |

### EQ-SUHC-009 — App Integration Traversal (open)

| Field | Value |
|---|---|
| `xi` | domain ordbok (`ordbok/*.szh` non-compiler) + Spoxis TS/SQL consumers |
| `*zeta` | emitted TS types, emitted SQL functions, consumer import sites |
| `*x` | ordbok-declared shape × consumer-observed shape |
| `bounds` | Spoxis repo layout; no circular dependency between compiler and app |
| `R.k` | compile ordbok → emit to `ordbok/out/` → copy into Spoxis source tree → consumer type-check |
| `omega` | Spoxis consumes ordbok-emitted types with zero hand-written shim |
| `DeltaR.k` | **open** — no mechanical sync step exists today; consumers import hand-authored mirrors |
| `authority` | future `make integrate-app`, Spoxis `src/lib/` |
| `completion_test` | Spoxis `tsc --noEmit` clean against ordbok-emitted types; consumer drift < 1 |

---

### EQ-SUHC-010 — Browser/Web Workspace Emission Target (placeholder, opened 2026-04-16)

| Field | Value |
|---|---|
| `xi` | stack-file ordbok + JS/TS runtime target + browser DOM/Canvas substrate |
| `*zeta` | TS emission with browser-runtime shims; PPM-bytestream consumer in Web Worker; OBJ parser bound to `<canvas>` or Three.js/Babylon substrate |
| `*x` | emitted types × browser DOM API × existing Spoxis TS consumer surface |
| `bounds` | browser target is additive to existing TS/SQL/C/ASM emitters; must not regress any M1–M7 bootstrap output; stack-file format remains Minnich-compliant (OBJ/PPM/voxmap) per `minnich/on_computing.md` §157 |
| `R.k` | `.szh` stack ordbok → parse → kindcheck → browser-runtime emit → bundle for target → load in `<canvas>`/WebGL context → render loop |
| `omega` | workspace stack files render in a browser byte-identical (at matching resolution) to the same stack rendered via Mongwu-native ASM emitter once Mongwu Sprint 20 lands |
| `DeltaR.k` | **open** — GPU substrate (WebGL vs WebGPU) unresolved; 3D-library substrate (Three.js vs Babylon.js vs hand-rolled) unresolved; no mechanical sync step with Spoxis's React Native client yet exists |
| `authority` | future `src/emit_browser.c`, future `ordbok/workspace.szh`, Spoxis parent eq `EQ-021 Workspace Stack File Runtime` |
| `completion_test` | round-trip: stack file → suhc browser emit → browser render → serialize → byte-compare original; cross-target: same stack file via TS-browser emitter and ASM-Mongwu emitter produce visually equivalent output at matching resolution |

**Status:** Placeholder. No implementation. Opened as a forward declaration of the next Suihan milestone, driven by Spoxis Games substrate-platform reframe (see Spoxis `docs/VERTICAL_AUDIT_HEALTH_AND_GAMES_2026-04-16.md` Addendum 2026-04-16 and `docs/EQUATION_REGISTRY.md` EQ-021).

---

## Registry Scope

Registry covers compiler milestones + audit + app-integration + browser workspace emission. It does **not** yet cover:
- Full Mongwu kernel integration (governed by `mongwu/ROADMAP.md`; future `EQ-SUHC-011+` once Sprints 5–20 land)
- Stress-test paradigms (`tests/stress/`); those are regression seeds, not traversals
