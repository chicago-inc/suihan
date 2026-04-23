# Suihan Partial Completion Anti-Patterns

Catalogue of common sloppy or incomplete change modes in the compiler and ordbok.

Use this document when a change "looks fixed" locally but the traversal has not closed.

Each anti-pattern references the six bloat categories from `CONSTITUTION.md §The Six Bloat Categories` (reduplication, niche pipe, temporal sediment, failure to derive, scope confusion, obtruding documentation).

---

## Anti-Patterns

### 1. Single-Target Closure

- Symptom: TS emission updated and looks correct
- Hidden failure: SQL or C target still emits old shape; `make bootstrap` fails at stage 4
- Violated equation part: `R.k` (emission pass), `omega`
- Bloat category: #1 (reduplication across targets)

### 2. Ordbok-Only Closure

- Symptom: `.szh` source updated and parses
- Hidden failure: no handler in emitter; new dimension member produces default fallthrough
- Violated equation part: `*zeta`, consumers
- Bloat category: #4 (failure to derive)

### 3. Dispatch Without Table

- Symptom: a new `yields ω dispatch` annotation is added
- Hidden failure: the emitter's dispatch table still has the old DeclType set; cross-dimension mode auto-detects wrongly
- Violated equation part: `*x`
- Bloat category: #2 (niche pipe — dispatch logic hand-wired when table-declared)

### 4. Keyword Added to Ordbok, Not to Parser

- Symptom: reserved-keyword list in ordbok grows
- Hidden failure: lexer accepts the new keyword, parser rejects, producing "unexpected token" error
- Violated equation part: `bounds` (parse + Lakatos boundary), `R.k`
- Bloat category: #5 (scope confusion — rule in one layer, enforcement in another)

### 5. Hidden Bootstrap Skip

- Symptom: a commit message says "bootstrap verified"
- Hidden failure: `make bootstrap` was not run; `-q` flag suppressed diff output; stage-2 byte comparison never happened
- Violated equation part: `bounds` (bootstrap boundary), `omega`
- Bloat category: #6 (obtruding documentation — claim replaces measurement)

### 6. Lakatos Retreat Without Justification

- Symptom: a previously declarable pattern moves back to hand-C
- Hidden failure: `CONSTITUTION.md §Self-Hosting` still says the pattern is in the ordbok; documentation lags code
- Violated equation part: `bounds` (Lakatos boundary)
- Bloat category: #3 (temporal sediment — old doc persisting past structural change)

### 7. Consumer Mirror Drift

- Symptom: Spoxis adds a hand-authored type matching a recent ordbok change
- Hidden failure: future ordbok change will drift silently; the mirror became a second source of truth
- Violated equation part: consumers
- Bloat category: #1 + #2 (reduplication that is also a niche pipe)

### 8. Convergence Metric Substitution

- Symptom: audit reports "convergence OK" after a change
- Hidden failure: the convergence calculation did not include the newly opened ordbok area; r < 1 is reported over a narrower domain than the change touched
- Violated equation part: `DeltaR.k` (hidden delta)
- Bloat category: #5 (scope confusion — metric applied to wrong scope)

### 9. Stage-1/Stage-2 Identity Illusion

- Symptom: `make bootstrap` shows no diff
- Hidden failure: the generated headers were regenerated *before* the bootstrap ran; stage-1 and stage-2 both use the new output, so byte-identity is trivial
- Violated equation part: `omega` (bootstrap verification)
- Bloat category: #3 (temporal sediment — prior-generation headers never actually tested)

### 10. Test Coverage Without Traversal Coverage

- Symptom: new regression test passes; commit looks clean
- Hidden failure: the test exercises one target or one dispatch mode; other modes or targets remain untested
- Violated equation part: `R.k`
- Bloat category: #4 (failure to derive — tests hand-written per case instead of generated per dimension member)

---

## Detection Guidance

Partial completion is detected by *measurement-before-documentation* (CONSTITUTION.md §Build Discipline rule -1). If a doc claims closure but the measurement disagrees, the measurement wins. Common measurements:

- `make bootstrap` → if diff, closure is false
- `./suhc --audit` → if new gaps, closure is false
- `./suhc --convergence` → if r ≥ 1, closure is contested
- `./suhc --diff file.szh app.ts` → if drift, consumer sweep was incomplete
