# EQ-SUHC-005 — Expression Algebra Traversal (M5)

## EQ-ID
EQ-SUHC-005

## xi
`ordbok/compiler/expression_algebra.szh` — precedence table, associativity declarations, math-function target mappings.

## *zeta
- Pratt precedence table per target
- operator-to-function projection (e.g., `^` → `pow()` in C, `**` in TS, `^` in SQL)
- associativity flags embedded in table

## *x
- TokenType × precedence × associativity
- target-specific operator spelling

## bounds
- **Lakatos boundary: the Pratt algorithm stays in C.** Only the precedence table is declarable.
- perpendicularity: precedence and associativity are perpendicular axes
- emission: target-specific spelling handled at emit time, not parse time

## R.k
1. parse `expression_algebra.szh`
2. emit per-target precedence table
3. at parse time, Pratt dispatch consumes the table
4. at emit time, operator spelling mapped per target
5. bootstrap compare

## omega
Expressions parse with correct precedence across targets. Adding a new operator requires one row in the precedence table plus target-specific spellings — not a parser change.

## DeltaR.k
- Status: `bounded`
- `TOK_PIPE` pipe operator retains a hand-C parse path — pipe semantics depend on in-flight compiler state (`last_decl` context) not accessible to the declarative layer. This is an explicit Lakatos residual.

## authority
- Source: `ordbok/compiler/expression_algebra.szh`
- Compiler: `src/parse_expr.c` (Pratt dispatch — Lakatos), `src/emit_*.c` (operator spelling)
- Generated: precedence table per target

## verification
```
make bootstrap
make test        # tests/m5_expression_algebra.sh + Pratt regression
```

Completion evidence: PROJECT_JOURNAL.md §M5 (H remaining: ~0, TOK_PIPE residual).
