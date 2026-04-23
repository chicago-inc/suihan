# Suihan Xi Bindings

Canonical bindings for `xi` in the unified equation:

```text
{xi; (*zeta, *x)R.k} : {omega, DeltaR.k}
```

`xi` is the initial condition. In Suihan it is **never a user** — it is always a source artifact (ordbok file), a structural decision (dimension declaration), or a build-time invariant (committed generated header). Recording `xi` wrongly is the most common source of incomplete traversal closure.

---

## Binding Families

### 1. Source-Centered Xi

Use when the traversal is primarily a projection over a declared source file.

Examples:
- Enum definition (EQ-SUHC-001)
- String table emission (EQ-SUHC-002)
- Parser dispatch (EQ-SUHC-006)

Canonical shape:

```text
.szh source file + parsed AST + containing ordbok directory
```

### 2. Kind-Centered Xi

Use when the traversal starts from a kind-system projection rather than a specific file.

Examples:
- Kind inference (EQ-SUHC-003) — DeclType → Kind
- Perpendicularity check — dimensional product where one axis is Kind

Canonical shape:

```text
kind dimension + cross-product partner dimension + inference rule
```

### 3. Emission-Centered Xi

Use when the traversal starts from an emission target rather than a source.

Examples:
- Emitter dispatch (EQ-SUHC-004) — per-target handler resolution
- Target-specific type mapping

Canonical shape:

```text
emission target (C | TS | SQL) + handler table + source projection
```

### 4. Bootstrap-Centered Xi

Use when the initial condition is committed generated state, not live source.

Examples:
- Bootstrap verification (EQ-SUHC-007)
- Generated-header change audit

Canonical shape:

```text
committed include/gen/*.h + current ordbok + build result
```

### 5. Audit-Centered Xi

Use when the traversal is a measurement over the whole ordbok rather than one file.

Examples:
- Audit convergence (EQ-SUHC-008)
- Yoneda gap enumeration

Canonical shape:

```text
ordbok/ directory + compiler binary + accumulated S-score state
```

### 6. Consumer-Centered Xi

Use when the traversal starts from a downstream consumer checking upstream shape.

Examples:
- App integration (EQ-SUHC-009)
- `--diff` drift detection

Canonical shape:

```text
consumer source + observed shape + declared ordbok shape
```

---

## Anti-Pattern: User-As-Xi

The parent constitution's XI_BINDINGS.md warns that `user` is not a universal `xi` for Spoxis. For Suihan the analogous anti-pattern is **developer-as-xi**:

- "We changed the compiler because the developer wanted feature X."
- "The initial condition is my intent to add a new emission target."

A developer's intent is not an `xi`. The `xi` is the source artifact or structural fact that would *still need* the traversal even if a different developer made the request. Anchoring on the developer hides which ordbok files must change.

---

## Applying Bindings

When opening an equation-touching change:

1. Identify the family above.
2. Record the canonical shape literally — do not paraphrase.
3. Verify the `xi` survives the "another developer makes the same request tomorrow" test. If it does not, the binding is wrong.
