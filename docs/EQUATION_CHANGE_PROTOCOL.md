# Suihan Equation Change Protocol

Mandatory workflow for any substantial change that touches a compiler traversal.

This protocol exists to stop partial completion: ordbok change without emitter update, new dispatch mode without test, new keyword without parser-dispatch wire-in, or "fixed" changes that never close the bootstrap.

The protocol extends `CONSTITUTION.md §Amendment Protocol` with a validation step: bootstrap identity.

---

## Trigger

Use this protocol when a change affects any of the following:

- an ordbok dimension declaration (`compiler/*.szh` or domain `*.szh`)
- a dispatch mode annotation (`yields ω ...`)
- a checker pass (kindcheck, perpcheck, exhaustcheck)
- an emission target (TS, SQL, C, future asm)
- the Lakatos barrier (moving procedural code into the ordbok or vice versa)
- a reserved keyword
- an execution layer (meihua, zhulin, songqiao)

---

## Required Steps

### 1. Identify Touched Equations

- Locate the affected `EQ-SUHC-*` entries in `docs/EQUATION_REGISTRY.md`.
- If no equation exists for the traversal, add one before continuing.

### 2. Declare Xi

- Record the correct `xi` binding using `docs/XI_BINDINGS.md`.
- Reject developer-intent as `xi`. The `xi` is the source file, dimension, or committed header being transformed.

### 3. Trace Boundaries

- Record affected lex, parse, kind, perpendicularity, exhaustiveness, emission, Lakatos, and bootstrap bounds.
- Update `docs/TRANSFORM_BOUNDARIES.md` if the bounded domain changes.

### 4. Update Target Omega

- Update `docs/OMEGA_SURFACES.md` and the per-milestone file if the transformed condition changes.
- State forbidden partial states explicitly; do not leave them inferred.

### 5. Update DeltaR.k

- If unresolved cast remains, update `docs/DELTA_RK_LEDGER.md`.
- If no unresolved cast remains, mark the entry `resolved`.
- Status changes (`open` → `bounded`, etc.) follow the promotion rules in the ledger.

### 6. Update Consumers

- Update `docs/CONSUMER_INVENTORY.md` when a shared interface changes.
- Sweep all consumer classes listed there in the same change set.

### 7. Run the Bootstrap

- `make bootstrap` must pass with all 63 comparisons == 0 diff.
- `make test` must pass on the stage-2 binary.
- `make check` must produce clean output for every target.

### 8. Verify Audit Convergence

- `./suhc --audit ordbok/` — convergence ratio must not regress.
- `./suhc --convergence ordbok/` — S-scores must not rise except where ΔR.k grew intentionally.
- If either regresses and the change is not explicitly widening the ordbok, the change is incomplete.

### 9. Verify Closure

The change is complete only if all are true:

- the intended `omega` is achieved
- no forbidden partial state remains
- consumers were updated
- remaining `DeltaR.k` is explicit in the ledger
- `make bootstrap` passes
- audit convergence did not regress
- verification reached the actual traversal, not only one file

---

## Equation-Close Questions

Before closing a task, answer:

1. Which `EQ-SUHC-*` entries changed?
2. What is `xi` for this traversal?
3. What changed in `*zeta`?
4. What changed in `*x`?
5. Did `R.k` gain, lose, or reroute any pass boundary?
6. What is the required `omega` now?
7. What `DeltaR.k` remains, and is it `open`, `bounded`, or `resolved`?
8. Which consumers were updated (suhc self, Spoxis TS, Spoxis SQL, Mongwu)?
9. Did `make bootstrap` pass?
10. Did audit convergence hold or improve?

---

## Relation to CONSTITUTION.md §Amendment Protocol

The amendment protocol in the constitution triggers when a *constitutional* item changes (a new ordbok term, dispatch mode, checker pass, emission target, Lakatos shift, execution layer). This equation-change protocol triggers on any *traversal-touching* change, which is a superset. An amendment is always also an equation change; an equation change is not always an amendment.
