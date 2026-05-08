# CONSTITUTION.md — Suihan Compiler (ξ)

## Purpose

Push suhc monotonically toward **fixed-point convergence between
compiler and ordbok under monotonic ordbok-coverage growth** — the
Suihan projection of the parent constitutions' rigorous-elegance-
under-recursive-self-application goal. Three perpendicular axes of
α:α:

- **Elegance** — α:α at the kind-grammar layer. Three scopes hold:
  local (one kind per AST node), cross-locus (perpendicularity —
  meihua/zhulin/songqiao do not predict one another), cross-instance
  (exhaustiveness — every projection covers its dimension). Bloat-
  six partitions by scope. Inherits parent ordbok §elegance (R2).
- **Rigor** — α:α at the derivation layer. `make bootstrap`
  stages 1–5 close the loop ordbok → headers → compiler → ordbok,
  byte-identical. Bootstrap divergence is rigor failure.
- **Recursive adaptation** — α:α at the temporal layer. M1→M7
  moved structural decisions from C into ordbok; each milestone
  passed bootstrap before the next; the Lakatos barrier names
  where the gradient stops. Every named principle subject to itself.

The goal is gradient: every milestone, ordbok extension, and
emitter change must converge the bootstrap, never diverge it. The
suhc binary is the witness; bootstrap is what verifies it.

Self-contained. Inherits axiom and ordbok from the immediate
parent (`SPOXIS_CONSTITUTION.md`); `RAE_CONSTITUTION.md` upstream.

---

## Identity

- **Name:** suhc (the suihan compiler)
- **Full name:** 歲寒 Suihan — "the cold of the year"
- **Language:** C11, zero dependencies beyond libc
- **Version:** 1.0.0 (M7 — full bootstrap, fixed point reached)
- **Source of truth:** `.szh` ordbok files
- **Emission targets:** TypeScript, PostgreSQL, C11

---

## The Axiom (inherited)

Everything is itself. α is α. α is not β.

A declaration is what its structural properties say it is,
not what a label says it is. A membership is a membership
because of its structural_role, not because someone stored
"Commissioner" in a display_title column.

**Perception corollary (inherited from parent constitutions'
Perception Axiom — SPOXIS_CONSTITUTION.md D35, RAE CLAUDE.md D50,
CHICAGO_THEORY.md §Perception).** The axiom presupposes
distinguishability, and distinguishability at any rendering or
input boundary presupposes a perceiver with a channel structure.
Color conflation, homograph conflation, and any foreground-surface
collapse are axiomatic violations at the boundary where the
compiler's output reaches a user. suhc is the parent-constitutions'
machine-enforcement layer for this corollary at the rendering
boundary: the `axiom_contrast_aa` predicate in
`suihan/ordbok/visual.szh` and the `suhc --literal-check` target
are the current instantiations. Extending the predicate from WCAG
luminance ratio to the two-channel rule (ΔL ≥ 0.4 on a perceptually
uniform lightness axis, or ΔL ≥ 0.3 ∧ ΔChroma ≥ 0.1 ∧ ΔHue ≥ 30°,
or a declared non-chromatic cue) is scheduled as Spoxis `UW-026 /
D35-BUILD-01` and will require corresponding suhc changes in
`src/color_math.c` and `src/literal_lint.c`.

---

## The Kind System

Every AST node carries exactly one kind from the unified equation:

```
{ξ; (*ζ, *x)R.k} : {ω, ΔR.k}
```

| Kind | Symbol | Meaning | Mutability | Emission |
|------|--------|---------|------------|----------|
| Identity | ξ | Retained identity-shape for the traversal | Immutable | `const` + type union |
| Shape | ζ | Conditional transform-shape in context | Computed, never stored | Pure function |
| Variable | x | Runtime data | Mutable | Interface field |
| Operator | R.k | Traversal paths | Pipeline logic | Function body |
| Output | ω | What the user sees | Terminal | Return type |
| Entropy cast | ΔR.k | Consequences of computation | Documentation | JSDoc comment |

Putting information in the wrong kind is a compile error.
A ζ used in a permission gate is rejected. A ξ mutated at
runtime is rejected. These are grammar constraints, not
conventions.

---

## The Six Bloat Categories

Every diagnostic the compiler produces is an instance of one
of six causes. There are no other error categories.

1. **Reduplication** — same ξ at two addresses
2. **Niche pipe** — ζ in ξ position (conditional shape treated as retained shape)
3. **Temporal sediment** — unreachable R.k path (dead code)
4. **Failure to derive** — ω without R.k derivation
5. **Scope confusion** — information in wrong kind slot
6. **Obtruding documentation** — R.k mechanism leaking into ω

---

## Perpendicularity

Dimensions of incommensurable units are perpendicular. You
cannot substitute a value from one where another is expected.
This is not a type error — it is a dimensional error, a stronger
guarantee. The perpendicularity checker enforces this across
all expressions, including cross-product projections.

---

## Exhaustiveness

Every projection must cover its full dimension. A projection
over `structural_roles` that handles `owner`, `admin`, `member`
but not `pending` has a gap. Wildcard defaults are permitted
but each uncovered member produces a warning. The exhaustiveness
checker verifies this for all declared projections.

---

## Ordbok Authority

The ordbok (`ordbok/`) is prescriptive for structure and
descriptive for coverage. A projection declared in the ordbok
defines the canonical case matrix. If the compiler diverges
from the ordbok, the compiler is wrong. But the ordbok does
not claim to cover all possible declarations — uncovered
territory is unmeasured, not invalid.

The ordbok's coverage grows monotonically. It never removes
a declaration that passes its own tests.

---

## Self-Hosting

The compiler's own structural decisions (enums, dispatch tables,
kind inference rules, precedence tables, parser dispatch) are
declarative facts that belong in the ordbok, not in hand-written C.

Seven milestones moved these facts from C to ordbok:

| M | What Moved | H Remaining |
|---|-----------|-------------|
| M1 | Enum definitions (Kind, DeclType, ExprType, TokenType) | 0 |
| M2 | String table functions (kind_name, decl_type_name, token_type_name) | 0 |
| M3 | Kind inference rule (DeclType → Kind) | 0 |
| M4 | Emitter dispatch (DeclType → handler, all 3 targets) | 0 |
| M5 | Expression algebra (precedence table, math function mappings) | ≈0 (TOK_PIPE residual) |
| M6 | Parser dispatch (keyword → handler, kind sigil → kind) | ≈0 |
| M7 | Bootstrap verification (formalized fixed-point test) | ≈0 |

**The Lakatos barrier** is the boundary where declarative
encoding stops being useful: the Pratt parsing algorithm,
AST memory management, the lexer FSM, and file I/O are
genuinely procedural. They stay in C.

---

## The Bootstrap

The bootstrap is the mechanism by which the compiler verifies
self-consistency.

```
make bootstrap
```

This runs a five-stage pipeline:

1. **Stage 1:** Build suhc from committed `include/gen/` headers
2. **Stage 2:** Use stage-1 suhc to regenerate headers from ordbok
3. **Stage 3:** Rebuild suhc from regenerated headers
4. **Stage 4:** Compare all ordbok outputs from stage-1 and stage-2
5. **Stage 5:** Run full test suite on stage-2 binary

If all outputs are identical, the fixed point is reached.
The compiler and its ordbok agree about what the compiler is.

**Generated headers are committed artifacts.** They serve as
the stage-1 input. After `make regenerate`, commit the updated
headers alongside the ordbok changes that produced them.

---

## Dispatch Modes

The C emitter supports five projection modes:

| Mode | Annotation | Generated Signature | Milestone |
|------|-----------|---------------------|-----------|
| String table | (default 1D) | `const char* name(enum_t val)` | M2 |
| Cross-dimension | (auto-detected) | `enum2_t name(enum1_t val)` | M3 |
| Emitter dispatch | `yields ω dispatch` | `void name(FILE*, Decl*)` | M4 |
| Parser dispatch | `yields ω parser_dispatch` | `Decl* name(Parser*, tok_t)` | M6 |
| Kind sigil | `yields ω kind_sigil` | `kind_t name(tok_t)` | M6 |

Every dimension also generates a reverse lookup `*_from_name()`
(string → enum) alongside `*_to_str()` (enum → string).

---

## Build Discipline

-1. **Empirical gating.** Measurement c-commands description.
    Before reading documentation, verify: `make check` output,
    `./suhc --convergence ordbok/` metrics, `ls ordbok/*.szh`.
    If measurements contradict documentation, documentation is
    temporal sediment — ignore it.
0. **Path derivation.** Source files locate the project root via
   directory structure, not configuration. Moving a file between
   directories requires updating its root derivation.
1. **Read before write.** Unread code has S = 1.
2. **Verify the bootstrap.** After any ordbok/compiler/ change,
   run `make bootstrap`.
3. **Stage deliberately.** Only files you changed.
4. **Type errors are blocking.** Do not commit with errors.
5. **One commit per milestone.** Each milestone is atomic.
6. **Immutable committed headers.** Never hand-edit `include/gen/`.
   Always regenerate from ordbok.
7. **No empty ordbok files.** Every .szh file must contain at
   least one declaration. Empty files are bloat #3.
8. **Categorical realism check.** ξ or ζ? Treating a transient
   measurement as a permanent structural property is a violation.

### Cycle Prevention

P1. **Morphism lock.** Never search for a missing file. If a path
    fails to resolve, declare identity lost and stop.
P2. **Binary exclusion.** Never commit build artifacts (*.o, *.d)
    or generated output reproducible from ordbok source.
P3. **FTDF limit.** One turn to fix an environment/build error.
    If the second attempt fails, the issue is structural. Cease.
P4. **Hard reset protocol.** When git history entropy exceeds
    pushability, abandon surgical rewrites. Stash, reset, restore.
P5. **Token gating.** Cease generation and request coordinate
    reset if cogitation exceeds 1,000 tokens without a terminal
    write or measurement.
P6. **Telegraphic mode.** Measurement data only.

---

## Testing

```bash
make test          # 125 regression tests
make bootstrap     # two-stage fixed-point verification (63 comparisons)
make check         # smoke test: all ordbok files to all targets
```

28 stress tests in `tests/stress/` cover 20 language paradigms
and 8 edge cases. Not part of the regression suite.

---

## Audit Tools

```bash
./suhc --audit ordbok/          # S scores, exhaustiveness, Yoneda gaps
./suhc --convergence ordbok/    # D14 convergence ratio
./suhc --diff file.szh app.ts   # drift detection
./suhc --watch ordbok/          # recompile on save
./suhc --graph ordbok/          # Mermaid dependency graph
./suhc --validate-sql file.sql  # SQL syntax validation
./suhc --dump-tokens file.szh   # lexer output
./suhc --dump-ast file.szh      # parsed AST
./suhc --dump-types file.szh    # type registry
```

---

## Reserved Keywords

The following cannot be used as meihua/zhulin/songqiao parameter
names:

`unit`, `zero`, `magnitude`, `vector`, `dimension`, `dependency`,
`containment`, `morphism`, `projection`, `traversal`,
`incommensurable`, `commensurable`, `perpendicular`, `invariant`,
`context`, `data`, `operator`, `output`, `cast`, `identity`,
`yields`, `cases`, `from`, `through`, `yield`, `carries`,
`structure`, `opens`, `governed_by`, `preserves`, `changes`,
`meihua`, `zhulin`, `songqiao`, `decidable`, `undecidable`,
`if`, `then`, `else`, `import`

Keywords CAN appear as dimension member names (the parser
disambiguates via lookahead).

---

## Three Execution Layers

The Three Friends of Winter are perpendicular execution contexts:

**梅花 meihua (plum blossom)** — Pure computation. No side effects.
Emits to SQL functions and TS functions.

**竹林 zhulin (bamboo grove)** — Control flow. Pattern matching,
branching. Emits to CASE/ternary chains.

**松喬 songqiao (pine bridge)** — Runtime configuration. Cluster
topology, resource allocation. Emits to config objects.

```
perpendicular meihua, zhulin, songqiao
```

Changing an expression does not predict a change in flow control.
Changing flow control does not predict a change in runtime context.

---

## Amendment Protocol

This constitution shall be amended when:

1. A new ordbok term is needed that no existing term covers
2. A dispatch mode is added to the emitter
3. A checker pass is added or modified
4. A new emission target is added
5. The Lakatos barrier shifts (procedural code becomes declarable)
6. A new execution layer is added

Amendments require `make bootstrap` to pass after the change.

---

## Self-Application

This constitution is subject to itself. If a principle stated
here fails its own test, the constitution must be revised.

The bootstrap is the self-application mechanism: the compiler
enforces these principles on ordbok code, and the ordbok
describes the compiler. A contradiction between the two is
a bootstrap failure — `make bootstrap` will report divergence.
