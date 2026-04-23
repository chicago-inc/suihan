# Suihan Equation Files

Per-traversal equation contracts live here.

Each file represents one major transform in:

```text
{xi; (*zeta, *x)R.k} : {omega, DeltaR.k}
```

These files are the execution-state layer that sits between:

- timeless law: `CONSTITUTION.md`
- current state: `PROJECT_JOURNAL.md`
- terrain maps: (compiler has no separate terrain maps; ordbok itself is the terrain)

---

## Required Sections

- `EQ-ID`
- `xi`
- `*zeta`
- `*x`
- `bounds`
- `R.k`
- `omega`
- `DeltaR.k`
- `authority`
- `verification`

`EQ-ID` must appear as an explicit section, not only in the title line.

If a substantial change touches a traversal here, update the corresponding file in the same change set per `docs/EQUATION_CHANGE_PROTOCOL.md`.

---

## Files

- `m1-enums.md` — EQ-SUHC-001 Enum Definition
- `m2-string-tables.md` — EQ-SUHC-002 String Table
- `m3-kind-inference.md` — EQ-SUHC-003 Kind Inference
- `m4-emitter-dispatch.md` — EQ-SUHC-004 Emitter Dispatch
- `m5-expression-algebra.md` — EQ-SUHC-005 Expression Algebra
- `m6-parser-dispatch.md` — EQ-SUHC-006 Parser Dispatch
- `m7-bootstrap.md` — EQ-SUHC-007 Bootstrap Verification

Auxiliary traversals (`EQ-SUHC-008` Audit, `EQ-SUHC-009` App Integration) are documented only in the registry until they warrant a per-file contract.
