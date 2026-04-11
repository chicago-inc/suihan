# Suihan Self-Hosting Roadmap

**suhc v1.0.0 — M7 Full Bootstrap (fixed point reached)**

---

## Current State

M1–M7 complete. Fixed point reached. The compiler builds on 18 generated headers from its own ordbok, regenerates those headers, rebuilds itself, and produces byte-identical output.

- **Test suite:** 125 passed, 0 failed
- **Bootstrap:** `make bootstrap` → 63/63 outputs identical, binary byte-identical
- **Emission targets:** TypeScript, PostgreSQL, C11
- **Ordbok:** 15 domain .szh + 18 compiler .szh → all three targets
- **Generated headers:** 18 in `include/gen/` (M1: 4, M2: 3, M3: 1, M4: 3, M5: 5, M6: 2)
- **Aggregate S:** 0.00 (fully situated)
- **Hand-written C:** 12,525 lines src/ + 1,197 lines include/ = 13,722 total
- **Generated C:** 1,284 lines in include/gen/ (from 484 lines of compiler ordbok)

---

## Era 5 — Self-Hosting Through Recursively Adaptive Conditional Entropy

Eras 1–4 built a compiler that governs external code (the Spoxis app's TypeScript and PostgreSQL). Era 5 turns the compiler inward: the ordbok describes the compiler's own structures, the C emitter produces headers the compiler itself includes, and successive iterations reduce the compiler's dependence on hand-written C.

### The Mechanism

Each iteration of the self-hosting pipeline:

1. **Encodes** compiler-internal structure as .szh declarations
2. **Emits** that structure as C headers via `--target c`
3. **Replaces** the hand-written C equivalent with `#include` of the generated header
4. **Measures** the entropy cast — new possibility spaces the replacement activates
5. **Adapts** — the next iteration operates on the cast of the previous one

The conditional entropy H(X|Y) of the hand-written C, given the ordbok's declarations, decreases monotonically as the ordbok absorbs more of the compiler's structure. When H(X|Y) = 0, the hand-written C is fully predictable from the ordbok — it can be deleted. The process is recursive because each absorbed layer changes what the compiler can perceive about itself (D13: S decreases), which exposes the next layer for absorption.

---

## Production Milestones

| Milestone | Description | Measurement | Replaces | Status |
|-----------|-------------|-------------|----------|--------|
| **M1 — Enum Self-Description** | Encode `DeclType`, `ExprType`, `Kind`, `TokenType` as .szh dimensions. Emit as C enums. Replace hand-written enums in `ast.h` and `token.h` with `#include` of generated headers. | H₁ = 0 | `ast.h` enum blocks, `token.h` token enum | **COMPLETE** |
| **M2 — Kind Name Tables** | Encode kind/decl/tok → string mappings as .szh projections. Emit as C switch functions. Replace `kind_name()`, `decl_type_name()`, `token_type_name()`. | H₂ = 0 | `ast.c:kind_name()`, `token.c:token_type_name()` | **COMPLETE** |
| **M3 — Kind Inference Projection** | Encode the DeclType → Kind mapping as a cross-dimension .szh projection. Emit as C enum→enum switch. Replace `infer_decl_kind()` in `kindcheck.c`. | H₃ = 0 | `kindcheck.c:infer_decl_kind()` | **COMPLETE** |
| **M4 — Emitter Dispatch** | Encode the `emit_c`, `emit_ts`, `emit_sql` top-level switch (DeclType → handler) as .szh dispatch projections. The C emitter emits its own dispatch table. | H₄ = 0 | `emit_c.c:emit_c()` main switch, `emit_ts.c` main switch, `emit_sql.c` main switch | **COMPLETE** |
| **M5 — Expression Algebra** | Encode precedence levels, token→precedence mapping, and math function→target-name tables as .szh declarations. Emit reverse lookup (`*_from_name`) on all dimensions. | H₅ ≈ 0 (TOK_PIPE residual) | `parser.c` prec enum + `infix_prec()`, math strcmp chains in emitters | **COMPLETE** |
| **M6 — Parser Self-Description** | Encode the keyword → parse function and kind sigil → kind_t mappings as .szh dispatch projections. Two new emitter modes (parser_dispatch, kind_sigil). Parser includes generated dispatch; only Pratt climber and AST construction remain hand-written. | H₆ ≈ 0 (keyword dispatch + kind sigil dispatch fully determined) | `parser.c` keyword-to-declaration dispatch, 6 kind sigil if-statements | **COMPLETE** |
| **M7 — Full Bootstrap** | `make bootstrap` runs the two-stage fixed-point verification. Stage-1 builds from committed headers. Stage-2 regenerates and rebuilds. 63/63 outputs identical. Binary byte-identical. | H₇ ≈ 0 for all declarative structure. Residual: Pratt climber, lexer FSM, AST memory, file I/O. | Formalizes verification of M1–M6. | **COMPLETE** |

---

## Convergence Properties

Each milestone has a well-defined convergence test:

- **M1–M5:** ✅ Compile the self-describing ordbok, include the generated `.h`, rebuild suhc, run `make test`. 125/125 pass with all five milestones' generated headers.
- **M6:** ✅ Parser keyword dispatch and kind sigil dispatch generated from ordbok. 18 if-statements + 6 kind sigil checks replaced by 2 generated dispatch calls.
- **M7:** ✅ `make bootstrap` runs the full two-stage verification: stage-1 builds from committed headers, stage-2 regenerates and rebuilds. 63/63 ordbok outputs identical. Binary byte-identical. Fixed point reached.

The ratio |ΔR.k_{n+1}| / |ΔR.kₙ| across milestones should decrease: M1 has the largest cast (new ordbok files, new build step, new include pattern), M7 has the smallest (the remaining hand-written C is stable infrastructure). If any milestone's cast exceeds the previous milestone's, the self-hosting strategy is generating more problems than it solves — pause and reassess.

---

## What Gets Replaced, What Doesn't

The self-hosting thesis is not "rewrite the compiler in .szh." It is: every structural decision that is currently expressed as a C switch statement, enum definition, string table, or dispatch pattern is a declarative fact masquerading as imperative code. These facts belong in the ordbok. The imperative infrastructure — memory allocation, file I/O, the Pratt parsing algorithm, the diagnostic accumulator — remains hand-written C because it is genuinely procedural.

The boundary between "declarative fact" and "procedural mechanism" is the Lakatos barrier for self-hosting. The milestones are ordered by increasing proximity to that barrier:

- **M1** (enums) — far from it. Enums are purely declarative.
- **M6** (parser dispatch) — close. The dispatch table is declarative but the parsing algorithm that consumes it is not.
- **M7** — sits on the barrier and accepts it. The Pratt climber stays.

---

## Dependency Chain

```
M1 (enums) ✅ ──► M2 (string tables) ✅ ──► M3 (kind inference) ✅
                                                    │
                                                    ▼
                                              M4 (emitter dispatch) ✅
                                                    │
                                                    ▼
                                              M5 (expression algebra) ✅
                                                    │
                                                    ▼
                                              M6 (parser dispatch) ✅
                                                    │
                                                    ▼
                                              M7 (full bootstrap) ✅ ── FIXED POINT
```

Each milestone depends on its predecessors because the generated headers from earlier milestones must be stable before later milestones can build on them. M1 is the foundation — if the enum generation is wrong, everything downstream breaks.
