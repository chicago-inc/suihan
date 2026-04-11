# 歲寒 Suihan — Project Journal

**Compiler:** suhc v1.0.0 (M7 — Full Bootstrap)
**Language:** C11, zero dependencies beyond libc
**Last sprint:** M7 — Full Bootstrap (fixed point verified)
**Last audited:** 2026-03-27

---

## What Suihan Is

Suihan (歲寒, "the cold of the year") is a compiler. It reads `.szh` files — a domain-specific language encoding the Chicago constitution's structural principles — and emits TypeScript and PostgreSQL. The constitution is not documentation; it is source code. The compiler enforces constitutional discipline at compile time: kind violations, exhaustiveness gaps, perpendicularity breaches, and semantic smegmacra are caught before any code reaches the app.

The name comes from the Three Friends of Winter (松竹梅): pine (songqiao), bamboo (zhulin), and plum blossom (meihua). These are the three execution layers the language provides. The environment they persist in — the cold of the year — is the language itself.

---

## How the Compiler Works

### The Pipeline

A `.szh` file enters as text and exits as TypeScript, PostgreSQL, and/or C11 headers. The pipeline is single-pass, forward-only, and produces diagnostics without halting:

```
.szh source
  → lexer.c         tokenize into a stream
  → parser.c        build an AST (a stack of Decl nodes)
  → resolve.c       resolve imports, bind cross-file references
  → kindcheck.c     validate the kind (ξ/ζ/x/R.k/ω/ΔR.k) of every node
  → typecheck.c     validate type annotations, arity, incommensurability (Sprint 6A)
  → exhaustcheck.c  verify projection arms cover the full dimension
  → perpcheck.c     verify cross-product coverage for multi-dimensional projections
  → semcheck.c      semantic checks (smegmacra detection, unused decls)
  → emit_ts.c       generate TypeScript
  → emit_sql.c      generate PostgreSQL
  → emit_c.c        generate C11 headers (self-hosting — M1+)
  → convergence.c   measure D14 convergence ratio (optional)
```

Key design choice: no AST rewriting. Everything emits in one forward pass over the declaration stack. This keeps the codebase simple (~11K lines total) and makes the compiler predictable — the output order matches the source order.

### The Kind System

Every AST node carries exactly one kind from the unified equation `{ξ; (*ζ, *x)R.k} : {ω, ΔR.k}`:

| Kind | Meaning | Emission Rule | Mutability |
|------|---------|---------------|------------|
| ξ | Identity | `const` + type union | Immutable after init |
| ζ | Shape | Pure function | Computed, never stored |
| x | Variable | Interface field | Mutable runtime data |
| R.k | Operator | Function (traversal) | Pipeline logic |
| ω | Output | Return type | Terminal, consumed by viewer |
| ΔR.k | Entropy cast | JSDoc comment | Documentation only |

The kind checker (`kindcheck.c`) validates that declarations carry the correct kind. A ζ used in a permission gate is a compile error. A ξ mutated at runtime is a compile error. These rules encode constitutional invariants as grammar constraints.

### Declaration Types

The parser recognizes 18 declaration types (see `ast.h:DeclType`):

| Declaration | Purpose | Ordbok Origin |
|-------------|---------|---------------|
| `unit` | Base type, has identity | §3 |
| `zero` | Null/bottom for a unit | §3 |
| `magnitude` | Numeric quantity | §3 |
| `vector` | Compound quantity | §3 |
| `dimension` | Enumerated variant set | §4 |
| `dependency` | Typed relationship (user → resource) | §5 |
| `containment` | Structural inclusion | §5 |
| `morphism` | Structure-preserving map | §6 |
| `projection` | ζ-computation (invariant × context → appearance) | §7 |
| `traversal` | Full equation: ξ, ζ, x, R.k, ω, ΔR.k sections | §8 |
| `incommensurable` | Units with no lossless conversion | §3 |
| `commensurable` | Units with lossless conversion | §3 |
| `perpendicular` | Orthogonal dimensions (cross-product) | §4 |
| Kinded values | `ξ name : expr`, `ζ name : expr`, etc. | §2 |
| `meihua` | Pure computation (math, formulas) | §10 |
| `zhulin` | Control flow / branching logic | §10 |
| `songqiao` | Runtime landscape configuration | §10 |
| `import` | Cross-file reference | §11 |

### The Three Execution Layers

The Three Friends of Winter are the executable declarations:

**Meihua (梅花, plum blossom)** — Pure computation. Takes parameters, returns a value. No side effects. Emits to a SQL function body and a TS function.

```suihan
meihua haversine_distance(lat1, lon1, lat2, lon2) :
  6371 * 2 * asin(sqrt(sin((lat2 - lat1) / 2) ^ 2 + cos(lat1) * cos(lat2) * sin((lon2 - lon1) / 2) ^ 2))
```

**Zhulin (竹林, bamboo grove)** — Control flow. Pattern matching that selects a path. Emits to SQL CASE/TS ternary chains.

```suihan
zhulin select_traversal_mode(headedness) :
  match headedness { unheaded -> "conjunction", headed -> "relational_filter", _ -> "polynomial_expansion" }
```

**Songqiao (松喬, pine bridge)** — Runtime configuration. Declares what exists in the operational landscape. Emits to config objects and SQL views.

```suihan
songqiao ai_tools :
  name "Secretary"
  model "claude-sonnet-4-6"
  temperature 0.3
```

### The Ordbok

The ordbok (`ordbok/`) is the machine-readable constitutional glossary. Each `.szh` file encodes one domain of the Chicago constitution. Currently 15 files, 795 lines, covering:

| File | Domain | Key Declarations |
|------|--------|-----------------|
| `foundational.szh` | Core units, dimensions | user, resource, structural_roles |
| `structural.szh` | Roles, labels, shapes | 8 projections (role_label, resource_shape, etc.) |
| `pipeline.szh` | Traversals, core formulas | resolve_member_view, haversine_distance |
| `permissions.szh` | Access control projections | canAccess, RLS policy patterns |
| `spatial.szh` | Location, geocoding layers | privacy zones, proximity queries |
| `venue.szh` | Venue model, sub-venues | venue containment, space hierarchy |
| `time.szh` | Temporal scoping, seasons | time-bounded communities |
| `auth.szh` | Authentication patterns | session, token, email verification |
| `equipment.szh` | Cognitive mechanisms | D10 oracle ceiling classes |
| `solipsism.szh` | Belief space, bridging | D8, D13 sympathetic solipsism |
| `notifications.szh` | Routing, preferences | notification channels, muting |
| `query_patterns.szh` | N+1 prevention, rendering | progressive loading patterns |
| `computations.szh` | Pure math formulas | 8 meihua (haversine, hot_cost, etc.) |
| `adversarial.szh` | Threat response, rate limits | graduated response projections |
| `oracle_ceiling.szh` | Decidability classes | computational class hierarchy |

Every ordbok file produces three outputs: a `.ts` file, a `.sql` file, and a `.h` file. These live next to their source in `ordbok/`. The sixteen `ordbok/compiler/*.szh` files additionally serve as self-hosting inputs: their generated `.h` files live in `include/gen/` and are `#include`d by the compiler itself. The outputs are currently **not** automatically integrated into the Spoxis app — this is manual and is a known gap.

---

## How to Build

```bash
cd suihan
make clean && make          # build suhc binary (uses pre-committed include/gen/*.h)
make debug                  # build with -g -O0 for debugging
make regenerate             # re-generate include/gen/*.h from ordbok/compiler/*.szh
```

The Makefile auto-discovers all `.c` files in `src/`. No configuration needed. The only dependency is a C11 compiler (gcc or clang) and libc.

**Two-stage build (M1+):** The compiler's own enums (M1), string tables (M2), kind inference (M3), emitter dispatch (M4), and expression algebra (M5) are generated from `ordbok/compiler/*.szh` → `include/gen/*.h` (16 headers). If the generated headers are committed (they are), stage 1 is skipped and the build is a normal `make`. Run `make regenerate` to re-run stage 1 with the current binary.

---

## How to Test

```bash
cd suihan
bash tests/run_tests.sh              # expect 125/125 pass
bash tests/run_tests.sh --verbose    # show skips and diffs
```

The test runner is a 573-line shell script that exercises:

1. **Diff tests** — compile a `.szh` to `.ts` and `.sql`, diff against `.expected` files
2. **Audit tests** — run `--audit` on ordbok files, verify S scores
3. **Emission validation** — compile and check that output files contain expected patterns
4. **SQL syntax validation** — `--validate-sql` on generated `.sql` files
5. **Convergence tests** — verify `--convergence` produces a report and ratio
6. **Ordbok tests** — compile each ordbok file to both targets, verify output

Test files live in `tests/`. To add a new test:

1. Create `tests/my_test.szh`
2. Generate expected output: `./suhc tests/my_test.szh --target ts` and `./suhc tests/my_test.szh --target sql`
3. Verify the output is correct
4. Copy to `tests/my_test.ts.expected` and `tests/my_test.sql.expected`
5. Run `bash tests/run_tests.sh` to confirm

---

## How to Add an Ordbok File

```bash
# 1. Create the file with an import
echo 'import foundational' > ordbok/newdomain.szh

# 2. Add declarations (dimensions, projections, meihua, etc.)
# 3. Validate
./suhc --validate ordbok/newdomain.szh

# 4. Compile to both targets
./suhc ordbok/newdomain.szh --target ts
./suhc ordbok/newdomain.szh --target sql

# 5. Audit
./suhc --audit ordbok/newdomain.szh
# Target: S = 0.00, all projections exhaustive

# 6. Test runner auto-discovers new ordbok files — run full suite
bash tests/run_tests.sh
```

---

## How to Modify the Compiler

### Adding a new expression type

1. Add the member to `ordbok/compiler/compiler_expr_types.szh`, run `make regenerate` to update `include/gen/expr_types.h`
2. Add the union member in `struct Expr`
3. Parse it in `parser.c` (Pratt climber for infix, `parse_primary` for prefix/atoms)
4. Emit in `emit_ts.c` — add a case to `expr_to_ts()` and `meihua_expr_to_ts()`
5. Emit in `emit_sql.c` — add a case to `meihua_expr_to_sql()`
6. Add a test `.szh` with `.expected` files
7. Run `bash tests/run_tests.sh`

### Adding a new declaration type

1. Add the member to `ordbok/compiler/compiler_decl_types.szh`, run `make regenerate` to update `include/gen/decl_types.h`
2. Add the union member in `struct Decl`
3. Add a `kind_name` case to `ast.c`
4. Parse it in `parser.c` — add a keyword check in the main parse loop
5. Kind-check in `kindcheck.c` — validate the kind is appropriate
6. Emit in both `emit_ts.c` and `emit_sql.c`
7. Add audit handling in `bloatlint.c` if structural analysis is needed
8. Add a test and an ordbok example

### Adding a new CLI mode

1. Add flag parsing to `main.c` (the flag table is in `main()`)
2. Implement the mode — either inline in `main.c` or in a new `.c` file
3. Add a header if needed (`include/newmode.h`)
4. The Makefile auto-discovers new `.c` files — just `make clean && make`

---

## Audit Tools

```bash
# Per-file structural audit (shows S, exhaustiveness, meihua validity)
./suhc --audit ordbok/structural.szh

# Directory-wide audit
./suhc --audit-dir ordbok/

# D14 convergence measurement (first run creates baseline)
./suhc --convergence ordbok/

# Drift detection (compares current output against .szh-lock snapshot)
./suhc --drift ordbok/foundational.szh

# File watcher (recompile on save)
./suhc --watch ordbok/foundational.szh
```

---

## Known Debt

| Item | Severity | Location | Notes |
|------|----------|----------|-------|
| 5 ungoverned entropy casts | Low | ordbok/ | Tracked, not yet governed by principles. |
| 2 Yoneda gaps | Low | ordbok/structural.szh | promote + transfer_ownership declared but unobserved. |
| No ordbok → app integration | Medium | Build pipeline | Emitted files stay in ordbok/, no deploy step. |
| Reserved keywords block meihua params | Low | lexer.c | `context`, `data`, etc. cannot be used as parameter names. |

---

## File Map

```
suihan/
├── Makefile                    # Build system (auto-discovers .c files)
├── suhc                        # Compiled binary
├── PROJECT_JOURNAL.md          # This file
├── build/                      # Object files (.o)
├── include/                    # Header files + 16 generated in gen/
│   ├── ast.h                   # AST node types — includes gen/kinds.h, gen/decl_types.h, gen/expr_types.h
│   ├── token.h                 # Token types — includes gen/token_types.h
│   ├── gen/                    # Generated headers (M1+M2+M3+M4+M5 — 16 headers from ordbok/compiler/*.szh)
│   │   ├── kinds.h             # M1: kind_t enum
│   │   ├── decl_types.h        # M1: decl_t enum
│   │   ├── expr_types.h        # M1: expr_t enum
│   │   ├── token_types.h       # M1: tok_t enum (85+ members)
│   │   ├── kind_names.h        # M2: kind_name() switch function
│   │   ├── decl_type_names.h   # M2: decl_type_name() switch function
│   │   ├── token_type_names.h  # M2: token_type_name() switch function
│   │   ├── kind_inference.h    # M3: infer_decl_kind() cross-dimension projection
│   │   ├── emit_c_dispatch.h   # M4: emit_c_dispatch() DeclType → C handler
│   │   ├── emit_ts_dispatch.h  # M4: emit_ts_dispatch() DeclType → TS handler
│   │   └── emit_sql_dispatch.h # M4: emit_sql_dispatch() DeclType → SQL handler
│   ├── parser.h                # Parser interface
│   ├── emitter.h               # Shared emitter interface
│   ├── convergence.h           # D14 measurement structures
│   ├── drift.h                 # Drift detection structures
│   └── ...                     # (12 more)
├── src/                        # 25 C source files (~12,350 lines)
│   ├── main.c (1,061)          # CLI dispatch, flag parsing
│   ├── lexer.c (479)           # Tokenizer
│   ├── parser.c (1,358)        # Pratt climber + declaration parser
│   ├── resolve.c (301)         # Import resolution
│   ├── kindcheck.c (283)       # Kind validation
│   ├── exhaustcheck.c (596)    # Projection exhaustiveness
│   ├── perpcheck.c (242)       # Perpendicularity / cross-product
│   ├── semcheck.c (211)        # Semantic checks, smegmacra
│   ├── emit_ts.c (1,702)       # TypeScript code generation
│   ├── emit_sql.c (908)        # PostgreSQL code generation
│   ├── convergence.c (477)     # D14 convergence measurement
│   ├── bloatlint.c (378)       # Structural audit engine
│   ├── drift.c (378)           # Drift detection
│   ├── watcher.c (288)         # File watcher mode
│   ├── ts_scanner.c (696)      # TS output validation scanner
│   ├── sql_validate.c (280)    # SQL syntax validation
│   ├── decidability.c (254)    # Decidability classification
│   ├── graph.c (204)           # Graph algorithms for containment
│   ├── dim_registry.c (128)    # Dimension registry for exhaustcheck
│   ├── diagnostic.c (107)      # Error/warning accumulator
│   ├── token.c (102)           # Token utilities
│   └── ast.c (131)             # AST constructors and utilities
├── ordbok/                     # 15 .szh files + 15 .ts + 15 .sql + 15 .h (795 lines source)
│   ├── foundational.szh        # Core units, dimensions
│   ├── structural.szh          # Roles, labels, shapes (S=0.00)
│   ├── pipeline.szh            # Traversals, formulas
│   ├── compiler/               # Self-hosting ordbok (M1–M6 — 18 files)
│   │   ├── compiler_kinds.szh              # M1: Kind enum
│   │   ├── compiler_decl_types.szh         # M1: DeclType enum
│   │   ├── compiler_expr_types.szh         # M1: ExprType enum
│   │   ├── compiler_token_types.szh        # M1: TokenType enum (85+ members)
│   │   ├── compiler_kind_names.szh         # M2: Kind → display string
│   │   ├── compiler_decl_type_names.szh    # M2: DeclType → label string
│   │   ├── compiler_token_type_names.szh   # M2: TokenType → display string
│   │   ├── compiler_kind_inference.szh     # M3: DeclType → Kind
│   │   ├── compiler_emit_c_dispatch.szh    # M4: DeclType → C handler
│   │   ├── compiler_emit_ts_dispatch.szh   # M4: DeclType → TS handler
│   │   ├── compiler_emit_sql_dispatch.szh  # M4: DeclType → SQL handler
│   │   ├── compiler_prec_levels.szh        # M5: precedence levels
│   │   ├── compiler_token_prec.szh         # M5: token → precedence
│   │   ├── compiler_math_fns.szh           # M5: math function names
│   │   ├── compiler_math_fn_c.szh          # M5: math → C stdlib
│   │   ├── compiler_math_fn_sql.szh        # M5: math → PostgreSQL
│   │   ├── compiler_keyword_dispatch.szh   # M6: keyword → parse function
│   │   └── compiler_kind_sigil_dispatch.szh # M6: sigil → kind_t
│   └── ...                     # (12 more domain ordbok files)
├── tests/                      # 31 .szh test files + expected baselines + stress/
│   ├── run_tests.sh            # Main test harness (125 tests)
│   ├── bootstrap_verify.sh     # M7 fixed-point verifier
│   ├── stress/                 # 28 paradigm stress tests
│   ├── check_equivalence.sh    # Ordbok-to-Spoxis equivalence
│   └── ...                     # (test .szh and .expected files)
├── CONSTITUTION.md             # Suihan-specific governing document (ξ)
└── docs/
    └── suihan/                 # Sprint documents
        ├── SUIHAN.md           # Language design document (§1-§11)
        └── SPRINT_*.md         # 20 sprint docs (0B through M7)
```

---

## Version History

| Version | Sprint | Key Deliverable |
|---------|--------|----------------|
| v0.1.0 | 0B | TypeScript emitter |
| v0.2.0 | 0C | Projection bodies, decidability checking |
| v0.3.0 | 1A | Import system, ordbok foundations |
| v0.4.0 | 1B | Expression language (21 types), section bodies |
| v0.5.0 | 2A | Match expressions, traversal compilation |
| v0.5.5 | 2B | Ordbok coverage (8→15 files), equivalence validation |
| v0.6.0 | 3A | Exhaustiveness checking, cross-file resolution |
| v0.6.5 | 3B | Perpcheck refactor, cross-product exhaustiveness |
| v0.7.0 | 4A | Drift detection, file watcher |
| v0.7.5 | 4B | SQL syntax validation, ordbok expansion |
| v0.7.8 | 5A | Meihua conditionals, typed parameter emission |
| v0.8.0 | 5B | Expression completeness, songqiao/zhulin, convergence |
| v0.9.1 | 6B | Error recovery, cascade suppression, Makefile hardening |
| v0.9.2 | 7A-pre | C emission test parity, ordbok .h generation, 125 tests |
| v0.9.3 | 7A | M1 enum self-description — compiler builds on its own generated enums |
| v0.9.4 | 7A | M2 string table projections — kind_name, decl_type_name, token_type_name generated |
| v0.9.5 | 7A | M3 kind inference projection — first behavioral self-hosting (infer_decl_kind generated) |
| v0.9.6 | 7A | M4 emitter dispatch — all three emitters self-describing (dispatch tables generated) |
| v0.9.7 | 7A | M5 expression algebra — precedence table + math function tables generated from ordbok |
| v0.9.8 | M6 | Parser self-description — keyword + kind sigil dispatch generated, 2 new emitter modes |
| v1.0.0 | M7 | Full bootstrap — fixed point reached, 63/63 outputs identical, binary byte-identical |

---

## Project Trajectory

### The Arc So Far

The Suihan compiler has moved through three distinct eras across 12 sprints:

**Era 1 — Bootstrap (0B–1B):** Get something that compiles. The lexer, parser, and TypeScript emitter came first. The language started as a way to encode ordbok terms and emit typed constants. By 1B, the expression language had 21 types and the import system connected files. The compiler was a transpiler with aspirations.

**Era 2 — Constitutional enforcement (2A–3B):** Make the compiler care about structure. Match expressions, traversal compilation, exhaustiveness checking, perpendicularity validation. This is where the compiler stopped being a code generator and started being an auditor. The question shifted from "does it emit code?" to "does it reject bad structure?" The Yoneda principle entered: a declaration without observation is a gap. The exhaustiveness checker is the first tool that catches constitutional violations at compile time.

**Era 3 — Completeness and measurement (4A–5B):** Fill the holes, measure the system. Drift detection, SQL validation, expression completeness, convergence measurement. Every sprint in this era closed gaps identified by the audit tooling built in Era 2. The convergence tool (5B) is the capstone: the project can now measure whether it is absorbing or generating entropy.

### Where 6A Fits

Sprint 6A opens **Era 4 — Semantic depth**. The compiler has syntax (Era 1), structural validation (Era 2), and self-measurement (Era 3). What it lacks is semantic understanding. It knows that `haversine_distance` takes four parameters but doesn't know they're magnitudes. It knows `user` and `resource` are declared incommensurable but can't enforce that at call sites.

The constitution makes this gap precise. The Lakatos barrier (CLAUDE.md ordbok) is "the locus where a component's S prevents it from perceiving the structure needed for self-revision." The compiler's meihua handling is behind its Lakatos barrier: without types, the compiler cannot perceive that `lat1` and `party_size` are structurally different values. It cannot self-correct because it cannot see the problem. Sprint 6A breaks through this barrier — once the type checker exists, previously invisible violations become compile errors.

The type system is the bridge between the ordbok's declarative knowledge ("these things are structurally distinct") and the compiler's operational enforcement ("therefore you cannot pass one where the other is expected"). The constitution's ordbok authority principle states the ordbok is "prescriptive for structure and descriptive for coverage." The type system deepens this: the ordbok becomes prescriptive not just for exhaustiveness and perpendicularity, but for type correctness at call sites.

Every prior era built infrastructure that the type system depends on:

- Era 1 gave the expression language and call syntax that types annotate
- Era 2 gave the exhaustiveness framework that type coverage extends
- Era 3 gave the audit tooling that measures type adoption

6A is not a pivot — it's the next layer of the same strategy. The compiler has been moving from surface (does it parse?) through structure (does it validate?) toward semantics (does it understand?). Types are the first semantic feature. In D11 terms, the type system is a novel morphism: the first time the type-checking principle connects to the meihua domain. No prior sprint has attempted this.

### The Forward Path

After 6A, the trajectory continues through increasing semantic depth:

**6B — Error Recovery and Hardening (COMPLETE).** Sprint 6B was scoped as error recovery, cascade suppression, SQL emitter hardening, DECL_MAGNITUDE/DECL_VECTOR audit, Makefile hardening, and new test files. The audit revealed that all checker passes already collected multiple errors (no early returns after diag_error — multi-error recovery was already in place). The DECL_MAGNITUDE/DECL_VECTOR audit found all 7 DECL_UNIT references correctly handled. The 18 SQL test failures turned out to be stale `.o` files compiled against pre-6A `ast.h` — a full rebuild fixed all 18 without any code changes to the SQL emitter. Deliverables completed: cascade error suppression (10 per category, 50 total, with summary messages), Makefile with `-MMD -MP` header dependency tracking and `make test`/`make check` targets, three new test files (multi_error.szh, type_cascade.szh, kind_keyword_types.szh). Test suite: 101 passed, 0 failed. Version: 0.9.1.

**7A — Type-Aware Emission + D16 Encoding.** Sprint 6A checks types but doesn't emit them. 7A would extend `emit_ts.c` to produce TypeScript type annotations from Suihan type declarations, and `emit_sql.c` to produce typed SQL function signatures. The emitted code becomes self-documenting: the generated TypeScript carries the same type constraints the compiler enforced. 7A is also the natural place to encode D16's four dependency types (membership, attendance, time_binding, follow) as ordbok declarations — the type infrastructure from 6A makes incommensurability enforcement immediately available for them. This connects the ordbok to the constitution's dependency classification for the first time.

**7B — Commensurable Coercion + ζ-Projection Typing.** Sprint 6A allows commensurable substitution (passing `minutes` where `hours` is expected) but doesn't insert conversion logic. 7B would extend the emitters to insert coercion functions at commensurable boundaries. The ordbok would declare conversion factors; the compiler would emit them. 7B could also begin typing projection invariants: the constitution's D12 amendment (Sprint 48) defines ζ-projection as `invariant × context → appearance` — the compiler could validate that projection invariants reference declared types, connecting the type system to the four known ζ-projection instances (resolveResourceShape, getProjectLabels, resolveAttendeeVisibility, getWorkUnitSchema).

**8A — Scope Checking.** The constitution defines scope as mutual satisfaction between two typed units across intervening material — not containment. By Sprint 7B, the compiler has all three legs of the scope triad independently: the type registry (Sprint 6A) for typed-property lookup, dim_registry (existing) for dimensional membership, and perpcheck (existing) for perpendicularity constraints. What it lacks is their composition. 8A would add a `scopecheck.c` pass that validates: (1) dependencies carry compatible types (mutual satisfaction — type_registry_are_incommensurable returns false for the two units in a dependency relationship), (2) scope does not cross perpendicular dimensions (perpcheck already enforces this for expressions; scopecheck extends it to dependency relationships), (3) the small number constraint (no more than 3–4 simultaneously open scope relationships). This is also the sprint for encoding scope inheritance in the ordbok: the zero-walk pattern (zero on a dimension triggers containment-walk until nonzero or default) is a single operation with multiple constitutional instances (attendee visibility, ghost mode, location, discovery scope). Making it a first-class ordbok construct lets the compiler validate all instances against the same rule.

**8B — Constitutional Domain Expansion.** Several constitutional domains have no ordbok encoding: context-sensitive identity presentation (D4 — profile as containment {z,k}, four derivation types), spatial resolution (D5 — three spatial layers, five location creation paths), the evaluation rule's smegmacrum/readiness-to-hand checks, and the equipment terms (Genevieve/Eleanor/Emily/Lilith computational classes). These are constitutional structure that the ordbok could make machine-readable and the compiler could validate. This is where the ordbok's descriptive coverage grows to match the constitution's prescriptive breadth.

**9+ — Ordbok-to-App Integration.** The largest remaining gap in the entire Suihan project: emitted `.ts` and `.sql` files sit in `ordbok/` and are never automatically integrated into the Spoxis app. This is the pipeline completion sprint — a build step that takes compiler output and deploys it into the application's type system and database. The HANDOFF.md documents the app's current state: 86 routes, known ζ-projection instances in `src/lib/`, the universal membership table. The integration sprint connects compiler output to these. This is where the constitution stops being a parallel document and becomes load-bearing infrastructure.

### The Convergence Question

Each era has reduced the project's entropy cast:

| Era | ΔR.k Character | Effect |
|-----|----------------|--------|
| 1 (Bootstrap) | Rapid expansion | New syntax = new possibility spaces everywhere |
| 2 (Enforcement) | Contraction | Checkers govern previously ungoverned spaces |
| 3 (Completeness) | Stabilization | Gaps filled, measurement begins, ratio approaches 1 |
| 4 (Semantics) | Targeted expansion + governance | Types add spaces but immediately govern them |

The healthy pattern is: each era's expansion is smaller than the previous era's, and each era's governance is more complete. Era 1 added 21 expression types with no validation. Era 2 added checkers that governed most of them. Era 3 filled the remaining gaps. Era 4 adds types — new spaces — but adds the checker in the same sprint. The cast is born governed.

The constitution (D14) states: "the denser the meta-category (D11), the lower the convergence ratio, because more of the cast lands in territory where a principle already applies." The Suihan project has been increasing meta-category density with each era: Era 1 connected the parsing principle to the ordbok domain, Era 2 connected exhaustiveness and perpendicularity to ordbok structure, Era 3 connected convergence measurement to the project itself. Era 4 connects type-checking to meihua and declarations — a novel morphism that increases density further.

The constitution also defines the gap between the ordbok's current coverage and the full constitution. Scope (mutual satisfaction between typed units, constrained by perpendicularity, bounded by the small number constraint), D4 (context-sensitive identity), D5 (spatial resolution), D16 (dependency classification), scope inheritance (zero-walk), the evaluation rule, and the equipment terms (readiness-to-hand, Genevieve/Eleanor/Emily/Lilith) are all constitutional domains not yet encoded. Each is an unmeasured territory (S > 0 for the ordbok's model of the constitution). The forward path from 7A through 8B would systematically reduce this S by encoding each domain as ordbok declarations that the compiler can validate. Scope checking (8A) is particularly significant because it is the first pass that composes multiple existing compiler subsystems (types + dimensions + perpendicularity) rather than adding a new one — a structural integration rather than an extension.

If this pattern holds, the project converges. If Era 4 activates more spaces than it governs — if the type system creates more problems than it solves — the convergence ratio exceeds 1 and the strategy needs revision. The convergence tool built in 5B will measure this. That's the tool doing its job.

### Sprint 7A-pre: C Emission Test Parity

Sprint 7A-pre established C as a first-class emission target. The C emitter (`emit_c.c`, 875 lines) existed since Sprint 5B but had no regression coverage — `make check` proved it compiled, nothing verified *what* it emitted. This sprint closes the Yoneda gap: every morphism the TS and SQL test paths observe, the C test path now observes too.

Deliverables:

- 6 `.h.expected` baseline files for diff tests (minimal, membership, match_test, import_test, meihua_conditional, expr_complete)
- `run_tests.sh` updated: C diff tests in all unit test loops, ordbok integration loop expanded from `ts sql` to `ts sql c`, songqiao/zhulin C emission checks, conditional meihua C ternary validation
- 15 ordbok `.h` files generated and committed alongside existing `.ts` and `.sql` outputs
- Test suite: 125 passed, 0 failed (up from 101)

### Sprint 7A: M1 — Enum Self-Description (COMPLETE)

Sprint 7A completes Milestone 1 of the self-hosting roadmap. The compiler now builds on its own generated enums — the four structural enum types (`Kind`, `DeclType`, `ExprType`, `TokenType`) are encoded as `.szh` dimension declarations in `ordbok/compiler/`, emitted as C11 headers via `--target c`, and `#include`d by the compiler's own `ast.h` and `token.h`.

**What was done:**

- Created `ordbok/compiler/` with 4 `.szh` files encoding the compiler's own enums as dimensions
- Generated `include/gen/*.h` (4 headers) containing `typedef enum { ... } kind_t;` etc. plus string conversion functions
- Replaced hand-written enums in `ast.h` and `token.h` with `#include "gen/*.h"` + typedef bridge (`typedef kind_t Kind;`)
- Added `make regenerate` target for the two-stage build
- Extended parser to handle context-sensitive keywords as dimension member names (all keyword tokens accepted as identifiers in expression context, with `peek_is_enum_sep()` lookahead to disambiguate `decidable`/`undecidable`/`if` as bare names vs. prefix operators)
- Increased `emit_c.c` dimension member limit from 64 to 256 (TokenType has 85+ members)
- Regenerated all 6 `.h.expected` baselines

**Bootstrap resolution:** The compiler that generates its own enum headers must already have those enums defined to compile. Solution: commit the generated headers. The initial build uses the committed headers; `make regenerate` uses the current binary to re-derive them. The two-stage build is the bootstrap.

**Metrics:** 125/125 tests passing. All 4 generated headers compile cleanly. The two-stage build produces a binary that passes all tests identically. H₁ (conditional entropy of ast.h/token.h enum definitions given ordbok) = 0 for the enum blocks — they are fully determined by the ordbok.

**Cast (ΔR.k):** M1 activates the following possibility spaces: (1) the `ordbok/compiler/` directory as a self-hosting surface, (2) the `include/gen/` pattern for generated-then-included headers, (3) the typedef bridge pattern as a migration strategy, (4) the two-stage build as a bootstrap mechanism. All four are governed by existing principles (D14 convergence, D7 niche pipe test, bloat #3 temporal sediment for bridges). The cast is born governed.

### Sprint 7A: M2 — String Table Projections (COMPLETE)

Sprint 7A continues with Milestone 2 of the self-hosting roadmap. The compiler's three string conversion functions (`kind_name()`, `decl_type_name()`, `token_type_name()`) are now generated from `.szh` projections over the M1 dimensions, emitted as C11 switch functions, and included by the compiler itself.

**What was done:**

- Created 3 new `.szh` projection files in `ordbok/compiler/`:
  - `compiler_kind_names.szh` — maps Kind enum → display strings (`"ξ"`, `"ζ"`, `"R.k"`, etc.)
  - `compiler_decl_type_names.szh` — maps DeclType enum → label strings
  - `compiler_token_type_names.szh` — maps TokenType enum → 85+ display strings (including Unicode `"ξ"`, `"×"` and operators `"->"`, `"=>"`, etc.)
- Extended `emit_c.c` with a dimension registry: the emitter now collects all dimension declarations (local + imported), and when a 1D projection's invariant matches a known dimension, emits an enum-based `switch` function instead of a strcmp if/else chain
- Generated `include/gen/kind_names.h`, `include/gen/decl_type_names.h`, `include/gen/token_type_names.h`
- Replaced hand-written `kind_name()` and `decl_type_name()` in `ast.c` with `#include` via `ast.h`
- Replaced hand-written `token_type_name()` in `token.c` with `#include` via `token.h`
- Extended `peek_is_enum_sep()` in `parser.c` to recognize `TOK_ARROW` — keywords like `decidable`, `undecidable`, `if` now parse as bare identifiers in projection arm patterns (`decidable -> "decidable"`)
- Updated Makefile `regenerate` target with 3 new projection headers

**New emitter capability:** The C emitter can now emit enum-switch projection functions from any 1D `.szh` projection whose invariant is a dimension declared in the same file or imported. This is a general mechanism — any future projection over a dimension (M3 checker dispatch, M4 emitter dispatch) will automatically emit as a switch.

**Metrics:** 125/125 tests passing. 7 generated headers in `include/gen/` (4 from M1, 3 from M2). H₂ = 0 for the string-table functions — they are fully determined by the ordbok projections.

**Cast (ΔR.k):** M2 activates: (1) the dimension registry pattern in the emitter (reusable for M3+), (2) the `TOK_ARROW` addition to `peek_is_enum_sep` (keywords-as-identifiers in projection arms), (3) the `ast.h`/`token.h` → `include/gen/*_names.h` include pattern. All governed by D14 (convergence), D7 (niche pipe test), and the existing `peek_is_enum_sep` mechanism. Cast is smaller than M1's — the patterns were established, M2 only instantiates them for a new use case. Convergence ratio decreasing as predicted.

### Sprint 7A: M3 — Kind Inference Projection (COMPLETE)

Sprint 7A completes Milestone 3: the compiler's kind inference logic is now generated from the ordbok. This is the first *behavioral* function subsumed — not a label or a name, but a rule the compiler uses to make decisions.

**What was done:**

- Created `ordbok/compiler/compiler_kind_inference.szh` — a cross-dimension projection mapping DeclType → Kind, encoding the constitutional assignment of kinds to declaration types (e.g., "a meihua is ξ because it's a pure expression; a traversal is R.k because it's a pipeline operator; a songqiao is ζ because it's runtime configuration")
- Extended `emit_c.c` with cross-dimension projection detection: when all result strings of a 1D projection are members of a known dimension, the emitter infers the codomain dimension and emits `enum → enum` switch function (returning `kind_t` values, not strings)
- Generated `include/gen/kind_inference.h` with signature `static inline kind_t infer_decl_kind(decl_t val)`
- Replaced hand-written `infer_decl_kind()` in `kindcheck.c` with `#include "gen/kind_inference.h"`

**New emitter capability:** Cross-dimension projections. The M2 mechanism detected `dimension → string` projections. M3 generalizes: the emitter now detects `dimension → dimension` projections automatically. Any future projection whose results all map to members of a known dimension will emit as an `enum → enum` function. This is the mechanism M4 (emitter dispatch) will use.

**Why this matters:** M1 replaced what the compiler's types ARE NAMED. M2 replaced how it DISPLAYS those names. M3 replaces what the compiler KNOWS about the relationship between its declaration types and the unified equation. The kind inference mapping is constitutional knowledge — it derives from CLAUDE.md's unified equation, not from engineering preference. The ordbok now governs the compiler's kind-checking behavior at the source level.

**Metrics:** 125/125 tests passing. 8 generated headers in `include/gen/` (4 M1, 3 M2, 1 M3). H₃ = 0 for `infer_decl_kind` — the function is fully determined by the ordbok projection.

**Cast (ΔR.k):** M3 activates: (1) the cross-dimension projection detection in the emitter (general mechanism, reusable), (2) the pattern of including behavioral logic from `gen/` headers (not just labels). Cast is minimal — both mechanisms are governed by existing patterns (dimension registry from M2, include pattern from M1). Convergence ratio continues to decrease.

### Sprint 7A: M4 — Emitter Dispatch (COMPLETE)

Sprint 7A M4: the compiler's three emitters now dispatch through generated switch functions. The C emitter emits its own dispatch table — the first instance of a compiler component generating the code that governs itself.

**What was done:**

- Created 3 new `.szh` dispatch projection files in `ordbok/compiler/`:
  - `compiler_emit_c_dispatch.szh` — DeclType → C handler function (18 cases)
  - `compiler_emit_ts_dispatch.szh` — DeclType → TS handler function (18 cases)
  - `compiler_emit_sql_dispatch.szh` — DeclType → SQL handler function (18 cases)
- Extended `emit_c.c` with **dispatch mode**: when a projection has `yields ω dispatch` and its invariant is a known dimension, the emitter generates a `static inline void dispatch(FILE*, Decl*)` function containing a switch that calls handler functions by name. Empty string results emit no-op `break;` cases.
- Generated `include/gen/emit_c_dispatch.h`, `emit_ts_dispatch.h`, `emit_sql_dispatch.h` — each a self-contained dispatch function
- Created uniform-signature wrapper functions for non-standard handlers:
  - emit_c.c: `emit_c_unit()`, `emit_c_magnitude()`, `emit_c_vector()`, `emit_c_import_comment()` (wrapping `emit_c_unit_type` with its label parameter)
  - emit_ts.c: `emit_kinded_value_ts()` (wrapping nested Kind switch), `emit_projection_ts()` (wrapping `emit_projection` with file-scope `prog`)
  - emit_sql.c: `emit_dependency_sql_d()` (wrapping `emit_dependency_sql` with file-scope `&er`), `emit_unit_sql_comment()`, `emit_import_sql_comment()`
- Replaced all three hand-written DeclType dispatch switches with single-line calls to generated dispatch functions

**New emitter capability:** Dispatch projections. M2 detected `dimension → string`. M3 detected `dimension → dimension`. M4 adds `dimension → function name` with dispatch mode (`yields ω dispatch`). The emitter generates a complete switch function that calls handler functions directly — no string lookup, no runtime overhead. The generated dispatch is included at file scope after all handler definitions, so `static` handlers are accessible.

**Why this matters:** The C emitter now generates the code that controls its own behavior. When the compiler processes `compiler_emit_c_dispatch.szh`, it produces `emit_c_dispatch.h`, which is `#include`d by `emit_c.c`, which is the same file that contains the dispatch mode emitter. The reflexive loop is closed. Adding a new DeclType to the ordbok automatically propagates to all three emitters through the generated dispatch — no hand-editing of switch statements.

**Metrics (at M4):** 125/125 tests passing. 11 generated headers in `include/gen/` (4 M1, 3 M2, 1 M3, 3 M4). 11 .szh files in `ordbok/compiler/`. H₄ = 0 for all three dispatch switches — fully determined by ordbok projections. Second-stage bootstrap verified (suhc regenerates → rebuilds → identical output).

**Cast (ΔR.k):** M4 activates: (1) the dispatch mode in the emitter (general mechanism — any `yields ω dispatch` projection generates a dispatch function), (2) the pattern of file-scope state for wrapper functions (g_ts_prog, g_sql_er). The file-scope state is architectural sediment — a forced duplication caused by the `static inline` calling convention. Acknowledged, minimal, and contained. Convergence ratio continues to decrease: M4's cast is smaller than M3's because the dispatch mechanism builds on the existing dimension registry and enum projection infrastructure.

### Sprint 7A: M5 — Expression Algebra (COMPLETE)

Sprint 7A M5: the parser's precedence table and both math function lookup chains are now generated from ordbok. Three hand-written tables replaced with generated headers; one cross-dimension regression caught and fixed.

**What was done:**

- Created 5 new `.szh` files in `ordbok/compiler/`:
  - `compiler_prec_levels.szh` — 13 precedence levels as a dimension (none through cross)
  - `compiler_token_prec.szh` — cross-dimension projection: tok_t → prec_t (with wildcard default for non-infix tokens)
  - `compiler_math_fns.szh` — 17 math function names as a dimension
  - `compiler_math_fn_c.szh` — projection: math_fn → C stdlib name (abs→fabs, min→fmin, max→fmax)
  - `compiler_math_fn_sql.szh` — projection: math_fn → PostgreSQL name (log→ln, log2→log, min→least, max→greatest)
- Extended `emit_c_dimension()` with `*_from_name()` reverse lookup (string → enum) alongside existing `*_to_str()` (enum → string). This enables the pipeline: `math_fn_from_name(callee)` → `math_fn_t` → `math_fn_c(val)` → C stdlib name.
- Generated 5 new headers in `include/gen/`: `prec_levels.h`, `token_prec.h`, `math_fns.h`, `math_fn_c.h`, `math_fn_sql.h`
- Wired `parser.c`: replaced the anonymous `enum { PREC_NONE = 0, ... PREC_CROSS = 12 }` and most of `infix_prec()` with generated `prec_from_name(infix_token_prec(tok))`. TOK_PIPE edge case (length-check conditional logic) remains hand-written — cannot be encoded as a pure projection.
- Wired `emit_c.c`: replaced 17-entry strcmp chain in `expr_to_c()` EXPR_CALL case with `math_fn_from_name(callee)` + `math_fn_c()`
- Wired `emit_sql.c`: replaced 17-entry strcmp chain in `meihua_expr_to_sql()` EXPR_CALL case with `math_fn_from_name(callee)` + `math_fn_sql()`. The `log2` special case (rewriting to `log(2, x)` for PostgreSQL) now uses `mfn == MATH_FN_LOG2` enum comparison instead of a second strcmp.
- Fixed a cross-dimension regression: `detect_codomain_dimension()` incorrectly fired when the codomain was the same dimension as the invariant (e.g., `decl_type_names` maps decl → strings that happen to match decl member names). Added guard: `if (codomain == dim) codomain = NULL;`
- Updated Makefile with 5 new generation commands and 5 new header entries

**Naming decisions:** The .szh dimension members can't use C reserved words. Precedence levels use `enum_prec`, `or_prec`, `and_prec` instead of `enum`, `or`, `and`. Generated constants are `PREC_ENUM_PREC`, `PREC_OR_PREC`, `PREC_AND_PREC`. These differ from the old hand-written `PREC_ENUM`, `PREC_OR`, `PREC_AND`, but since all references were rewritten to use the generated lookup, no compat aliases are needed.

**Metrics:** 125/125 tests passing. 16 generated headers in `include/gen/` (4 M1, 3 M2, 1 M3, 3 M4, 5 M5). 16 .szh files in `ordbok/compiler/`. H₅ ≈ 0 for the precedence table and both math function chains — fully determined by ordbok projections. The `infix_prec()` TOK_PIPE edge case is the residual H₅ > 0: a conditional that requires runtime state (token length), not expressible as a declarative projection. Second-stage bootstrap verified (suhc regenerates → rebuilds → byte-identical output).

**Cast (ΔR.k):** M5 activates: (1) the `*_from_name()` reverse lookup pattern on all dimensions (general mechanism — every dimension now emits string → enum alongside enum → string), (2) the two-step string lookup pattern for cross-dimension wiring (`tok → string → prec`), (3) the same-dimension codomain guard (a new constraint on the cross-dimension emitter). Cast is small: (1) is a mechanical extension of M1, (2) is the known cost of not having a direct tok_t → prec_t enum switch (the emitter generates string intermediaries), (3) is a one-line guard that prevents future regressions. Convergence ratio continues to decrease.

### Era 5 — Self-Hosting Through Recursively Adaptive Conditional Entropy

The C emitter changes the compiler's relationship to itself. Eras 1–4 built a compiler that governs external code (the Spoxis app's TypeScript and PostgreSQL). Era 5 turns the compiler inward: the ordbok describes the compiler's own structures, the C emitter produces headers the compiler itself includes, and successive iterations reduce the compiler's dependence on hand-written C.

The mechanism is recursively adaptive conditional entropy. Each iteration of the self-hosting pipeline:

1. **Encodes** compiler-internal structure as .szh declarations
2. **Emits** that structure as C headers via `--target c`
3. **Replaces** the hand-written C equivalent with `#include` of the generated header
4. **Measures** the entropy cast — new possibility spaces the replacement activates
5. **Adapts** — the next iteration operates on the cast of the previous one

The conditional entropy H(X|Y) of the hand-written C, given the ordbok's declarations, decreases monotonically as the ordbok absorbs more of the compiler's structure. When H(X|Y) = 0, the hand-written C is fully predictable from the ordbok — it can be deleted. The process is recursive because each absorbed layer changes what the compiler can perceive about itself (D13: S decreases), which exposes the next layer for absorption.

#### Production Milestones

| Milestone | Description | Measurement | Replaces |
|-----------|-------------|-------------|----------|
| **M1 — Enum Self-Description** ✅ | Encode `DeclType`, `ExprType`, `Kind`, `TokenType` as .szh dimensions. Emit as C enums. Replace hand-written enums in `ast.h` and `token.h` with `#include` of generated headers. | H₁ = 0 (enum blocks fully determined by ordbok) | `ast.h` enum blocks, `token.h` token enum |
| **M2 — Kind Name Tables** ✅ | Encode kind/decl/tok → string mappings as .szh projections. Emit as C switch functions. Replace `kind_name()`, `decl_type_name()`, `token_type_name()`. | H₂ = 0 (string tables fully determined by ordbok projections) | `ast.c:kind_name()`, `ast.c:decl_type_name()`, `token.c:token_type_name()` |
| **M3 — Kind Inference Projection** ✅ | Encode the DeclType → Kind mapping as a cross-dimension .szh projection. Emit as C enum→enum switch. Replace `infer_decl_kind()` in `kindcheck.c`. | H₃ = 0 (kind inference fully determined by ordbok projection) | `kindcheck.c:infer_decl_kind()` |
| **M4 — Emitter Dispatch** ✅ | Encode the `emit_c`, `emit_ts`, `emit_sql` top-level switch (DeclType → handler) as .szh dispatch projections. The C emitter emits its own dispatch table. | H₄ = 0 (all three dispatch switches fully determined by ordbok projections) | `emit_c.c:emit_c()` main switch, `emit_ts.c:emit_typescript()` main switch, `emit_sql.c:emit_sql()` main switch |
| **M5 — Expression Algebra** ✅ | Encode precedence levels, token→precedence mapping, and math function→target-name tables as .szh declarations. Emit reverse lookup (`*_from_name`) on all dimensions. Replace anonymous prec enum + most of `infix_prec()` in `parser.c`, 17-entry strcmp chains in `emit_c.c` and `emit_sql.c`. | H₅ ≈ 0 (prec table and math chains fully determined; TOK_PIPE edge case is residual H > 0) | `parser.c` anonymous prec enum + infix_prec() core, `emit_c.c:expr_to_c()` math strcmp chain, `emit_sql.c:meihua_expr_to_sql()` math strcmp chain |
| **M6 — Parser Self-Description** | Encode the keyword → DeclType mapping and the declaration parse grammar as .szh. The parser includes a generated dispatch table; only the Pratt climber and AST construction remain hand-written. | H₆: conditional entropy of the parser's keyword dispatch given ordbok | `parser.c` keyword-to-declaration dispatch |
| **M7 — Full Bootstrap** | The compiler compiles its own ordbok to C, includes the result, and rebuilds itself. The hand-written C contains only: the Pratt climber, AST memory management, file I/O, and the main() flag parser. Everything else is generated. | H₇ ≈ 0 for all structural code. The remaining hand-written C is infrastructure (I/O, memory), not structure. | The structural majority of the compiler |

#### Convergence Properties

Each milestone has a well-defined convergence test:

- **M1–M5:** ✅ Compile the self-describing ordbok, include the generated `.h`, rebuild suhc, run `make test`. 125/125 pass with generated enums (M1), generated string tables (M2), generated kind inference (M3), generated emitter dispatch (M4), and generated expression algebra (M5). All five milestones converge. Second-stage bootstrap (suhc regenerates headers, rebuilds, produces byte-identical output) verified through M5.
- **M6:** The parser keyword dispatch uses generated tables. The Pratt climber's algorithm remains hand-written because it is procedural (not declarative) — this is the Lakatos barrier for pure self-hosting. M5 proved that the climber's *data* (precedence table, math tables) is declarative even though the *algorithm* is not.
- **M7:** `suhc` compiles `ordbok/compiler/*.szh` → `include/gen/*.h`, then `make` rebuilds `suhc` using those headers. The two-stage build is the bootstrap. The convergence test: does the second-stage `suhc` produce identical output to the first-stage on all ordbok files? If yes, the fixed point is reached.

The ratio |ΔR.k_{n+1}| / |ΔR.kₙ| across milestones should decrease: M1 has the largest cast (new ordbok files, new build step, new include pattern), M7 has the smallest (the remaining hand-written C is stable infrastructure). If any milestone's cast exceeds the previous milestone's, the self-hosting strategy is generating more problems than it solves — pause and reassess.

#### What Gets Replaced, What Doesn't

The self-hosting thesis is not "rewrite the compiler in .szh." It is: every structural decision that is currently expressed as a C switch statement, enum definition, string table, or dispatch pattern is a declarative fact masquerading as imperative code. These facts belong in the ordbok. The imperative infrastructure — memory allocation, file I/O, the Pratt parsing algorithm, the diagnostic accumulator — remains hand-written C because it is genuinely procedural.

The boundary between "declarative fact" and "procedural mechanism" is the Lakatos barrier for self-hosting. The milestones are ordered by increasing proximity to that barrier. M1 (enums) is far from it — enums are purely declarative. M6 (parser dispatch) is close — the dispatch table is declarative but the parsing algorithm that consumes it is not. M7 sits on the barrier and accepts it: the Pratt climber stays.

---

## Sprint Plan: M6 — Parser Self-Description

### The Entropy Landscape After M5

M1–M5 absorbed five classes of declarative fact: enums, string tables, kind inference, emitter dispatch, and expression algebra. The conditional entropy H(hand-written C | ordbok) has decreased monotonically across all five. What remains?

The parser's `parse_declaration()` function (parser.c:1317–1386) is the largest surviving hand-written dispatch. It is an if/match chain mapping 18 keyword tokens to parse functions — the same structural pattern as the emitter dispatch M4 replaced. The difference: M4's dispatch was `DeclType → handler function`. M6's dispatch is `TokenType → parse function`. Same shape. Same mechanism. Different dimension.

But `parse_declaration()` is not a pure dispatch. Three structural complications make it higher-entropy than M4:

1. **Kind sigils** (lines 1322–1327): Six TOK_{XI,ZETA,X,RK,OMEGA,DELTA_RK} entries don't map to unique parse functions — they all call `parse_kinded_value(p, KIND_*)`. This is a cross-dimension dispatch: tok → kind → parse function. M5's cross-dimension infrastructure (tok → prec) is the precedent.

2. **Import** (lines 1330–1343): Has inline error handling and AST construction — not a one-liner dispatch. This is procedural logic masquerading as a dispatch arm. It resists declarative encoding.

3. **Zero** (lines 1364–1378): Same as import — inline logic, not a clean function call.

The recursively adaptive approach: R.k₁ absorbs what's purely declarative (the 15-entry keyword → parse function mapping). R.k₂ operates on the cast of R.k₁ — the residual entropy in the kind sigil cross-dispatch, the import inline logic, and the zero inline logic. R.k₃ evaluates whether those residuals are genuinely procedural (Lakatos barrier) or just unseen declarative structure (S > 0).

### What To Build

**Phase 1 — The keyword dispatch projection:**

Create `ordbok/compiler/compiler_keyword_dispatch.szh`:
```
import compiler_token_types
projection keyword_dispatch :
  invariant ξ tok
  yields ω dispatch
  cases:
    unit -> "parse_unit_like_unit"
    magnitude -> "parse_unit_like_magnitude"
    vector -> "parse_unit_like_vector"
    dimension -> "parse_dimension"
    dependency -> "parse_dependency"
    containment -> "parse_containment"
    morphism -> "parse_morphism"
    projection -> "parse_projection"
    traversal -> "parse_traversal"
    incommensurable -> "parse_relation_incommensurable"
    commensurable -> "parse_relation_commensurable"
    perpendicular -> "parse_relation_perpendicular"
    meihua -> "parse_exec_layer_meihua"
    zhulin -> "parse_exec_layer_zhulin"
    songqiao -> "parse_exec_layer_songqiao"
    _ -> ""
```

This is a dispatch projection (M4 pattern). The emitter generates `keyword_dispatch(FILE*, Decl*)` — but wait, the parser dispatch signature is `Decl* handler(Parser*)`, not `void handler(FILE*, Decl*)`. M4's dispatch mode assumes emitter signature. M6 needs a **new dispatch signature**: `Decl* (*)(Parser*)`.

**Decision point:** extend the dispatch emitter to support configurable signatures, or create thin wrappers with the emitter signature and trampoline into the real parse functions. The wrapper approach (M4 precedent) is lower-cast — it doesn't modify the emitter's dispatch mode. The configurable approach is more general but activates a larger ΔR.k. Measure cast before deciding.

**Phase 2 — The kind sigil cross-dispatch:**

Create `ordbok/compiler/compiler_kind_sigil_dispatch.szh`:
```
import compiler_token_types
import compiler_kinds
projection kind_sigil_dispatch :
  invariant ξ tok
  yields ω kind
  cases:
    xi -> "xi"
    zeta -> "zeta"
    x -> "x"
    rk -> "rk"
    omega -> "omega"
    delta_rk -> "delta_rk"
    _ -> ""
```

This is the M5 cross-dimension pattern (tok → kind). The generated function returns `kind_t`. In parser.c, the sigil block becomes:
```c
kind_t k = kind_sigil_dispatch(p->current.type);
if (k != (kind_t)-1) {
    parser_advance(p);
    return parse_kinded_value(p, k);
}
```

Six if-statements collapse to three lines.

**Phase 3 — Wiring and wrappers:**

The parse functions have non-uniform signatures:
- `parse_dimension(Parser*)` — no extra arg
- `parse_unit_like(Parser*, DeclType)` — needs DeclType
- `parse_exec_layer(Parser*, DeclType)` — needs DeclType
- `parse_relation(Parser*, DeclType)` — needs DeclType

Same problem M4 solved with file-scope state and thin wrappers. Create uniform-signature wrappers:
```c
static Decl *parse_unit_like_unit(Parser *p) { return parse_unit_like(p, DECL_UNIT); }
static Decl *parse_unit_like_magnitude(Parser *p) { return parse_unit_like(p, DECL_MAGNITUDE); }
// ... etc for all parameterized parse functions
```

**Phase 4 — Import and Zero:**

Extract import and zero parsing into standalone `parse_import(Parser*)` and `parse_zero_decl(Parser*)` functions. This makes them callable from the dispatch table. The inline error handling moves into the extracted functions — no behavioral change, just refactoring for dispatch compatibility.

Add `import -> "parse_import"` and `zero -> "parse_zero_decl"` to the dispatch projection.

### Measurement

**H₆ target:** The keyword dispatch chain (18 if-statements + 6 kind sigils) is fully determined by two ordbok projections. Residual H₆ > 0 is the `parse_declaration()` frame itself (skip_newlines, EOF check, error fallthrough) — genuine procedure, not encoded.

**Lines replaced:** ~50 lines of if/match chain → 1 dispatch call + 1 kind-sigil check.

**Cast (ΔR.k) prediction:**
1. New dispatch signature support (if extending emitter) — moderate cast into emitter architecture
2. Wrapper functions (if using M4 pattern) — minimal cast, contained in parser.c
3. The pattern of extracting inline logic into named functions for dispatch compatibility — this is a general refactoring principle the constitution doesn't yet name. Missing object? Possibly. Evaluate at R.k₂.

**Convergence test:** Same as M1–M5. Compile ordbok, include headers, rebuild suhc, 125/125 tests pass. Bootstrap: suhc regenerates → rebuilds → byte-identical. The keyword dispatch projection additionally needs a parser-level integration test: parse a .szh file that exercises all 18 declaration types and verify identical AST output before/after wiring.

### Risk: The Dispatch Signature Fork

M4 dispatch generates `void handler(FILE*, Decl*)`. M6 dispatch needs `Decl* handler(Parser*)`. Two options:

**Option A — Signature-parameterized dispatch mode.** Extend the `yields ω dispatch` annotation to accept a signature hint: `yields ω dispatch(Decl*, Parser*)`. The emitter generates the dispatch function with the specified return type and parameter list. Higher generality, higher cast.

**Option B — Parser-specific dispatch mode.** Add a new yield annotation: `yields ω parser_dispatch`. The emitter generates `Decl* keyword_dispatch(Parser* p, tok_t tok)` with a switch that calls named functions. Lower generality, lower cast, but adds a new ad-hoc annotation — a potential niche pipe (D7).

**Option C — No new emitter mode.** Use the string-table projection (M2 pattern) to emit `tok → function name string`, then hand-write a single function-pointer dispatch in parser.c that uses the string to index a table. Lowest cast, but introduces a runtime string comparison in the parser hot path.

**Recommendation:** Option A if the emitter already has the infrastructure to parameterize. Option B if not. Option C is a fallback that avoids emitter changes entirely. The decision depends on measuring S for the emitter's dispatch mode — how much structure does it already perceive?

---

*This journal is the entry point for any agent or developer picking up the Suihan compiler. Read this first, then SUIHAN.md for the language specification, then ast.h for the data structures.*
