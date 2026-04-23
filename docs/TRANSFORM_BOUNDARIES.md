# Suihan Transform Boundaries

Canonical registry for the equation bounds: where a compiler pass is allowed to operate, where it must stop, and where it must hand off to the next pass.

The ordbok describes declarations. This document describes the *rules the compiler enforces on the declarations as they cross pass boundaries*.

---

## Boundary Types

| Boundary | Enforced In | Purpose | Failure Signature |
|---|---|---|---|
| Lex boundary | `src/lex.c` | convert characters to tokens | malformed token, unterminated string |
| Parse boundary | `src/parse_*.c` | convert tokens to AST | expected-keyword error, unclosed dimension |
| Kind boundary | `src/kindcheck.c` | enforce ξ/ζ/x/R.k/ω/ΔR.k kind slots | kind mismatch, ξ mutated, ζ used as gate |
| Perpendicularity boundary | `src/perpcheck.c` | reject cross-dimension substitution | dimensional error across incommensurable axes |
| Exhaustiveness boundary | `src/exhaustcheck.c` | every projection covers full dimension | uncovered enum member (warning or error) |
| Emission boundary | `src/emit_{ts,sql,c}.c` | per-target code generation | emission mismatch across targets |
| Lakatos boundary | compiler source + `CONSTITUTION.md §Self-Hosting` | where declarative encoding stops being useful | attempt to declare Pratt parser / FSM / AST / I/O |
| Bootstrap boundary | `Makefile` bootstrap target | byte-identity invariant across stage-1 and stage-2 | any `diff` output after regenerate+rebuild |

---

## Boundary Contracts By Equation

### EQ-SUHC-001 Enum Definition

- Parse boundary: dimension syntax `ξ dimension X { a, b, c }`
- Kind boundary: dimension members inherit the dimension's kind
- Emission boundary: three targets (C enum, TS union, SQL type) must agree
- Failure signature: member count differs across targets

### EQ-SUHC-002 String Table

- Exhaustiveness boundary: every enum member must map to a string
- Emission boundary: reverse lookup `*_from_name` generated alongside forward
- Failure signature: unknown member falls through without warning

### EQ-SUHC-003 Kind Inference

- Perpendicularity boundary: DeclType axis and Kind axis are perpendicular
- Exhaustiveness boundary: every DeclType produces exactly one Kind
- Failure signature: DeclType added to ordbok but `infer_kind` returns default

### EQ-SUHC-004 Emitter Dispatch

- Exhaustiveness boundary: every DeclType has a handler in every emission target
- Kind boundary: the handler's output kind is ω (terminal)
- Failure signature: TS target emits new shape but SQL target silently skips

### EQ-SUHC-005 Expression Algebra

- Lakatos boundary: **Pratt algorithm is not declarable.** Only the precedence table is.
- Emission boundary: target-specific operator spellings (`&&` in TS/C vs `AND` in SQL)
- Failure signature: declaring a new operator without updating the Pratt dispatch table

### EQ-SUHC-006 Parser Dispatch

- Parse boundary: reserved-keyword set must match the ordbok declaration
- Lakatos boundary: lookahead disambiguator stays in hand-C
- Failure signature: new keyword added to ordbok but parser still rejects

### EQ-SUHC-007 Bootstrap Verification

- Bootstrap boundary: stage-1 and stage-2 outputs must be byte-identical for 63 comparison files
- Emission boundary: all three targets contribute to bootstrap comparison
- Failure signature: regenerate+rebuild produces any `diff` output

### EQ-SUHC-008 Audit Convergence

- Lakatos boundary: convergence metric is a ratio, not a count; interpretation requires human review when r approaches 1
- Failure signature: audit reports zero gaps while Yoneda holes are known to exist (anti-ontology)

### EQ-SUHC-009 App Integration

- Lakatos boundary: consumer code is not ordbok-declared; only the shape contract is
- Emission boundary: ordbok-emitted types must be importable without wrapping
- Failure signature: Spoxis hand-authors a mirror of an ordbok-declared shape

---

## Cross-Cutting Rules

- **No pass skipping.** A change that reaches emission without crossing kindcheck is a boundary violation, even if it looks correct.
- **Bootstrap dominates.** If any pass says "OK" but `make bootstrap` fails, the pass is wrong.
- **Lakatos is not a loophole.** Moving something back to hand-C requires updating `CONSTITUTION.md §Self-Hosting` with explicit justification; silent retreat is bloat #4 (failure to derive).
