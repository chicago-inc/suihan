# EQ-SUHC-001 — Enum Definition Traversal (M1)

## EQ-ID
EQ-SUHC-001

## xi
Dimension declarations in `ordbok/compiler/kinds.szh`, `decl_types.szh`, `expr_types.szh`, `token_types.szh`. Each file declares one dimension of the compiler's kind system.

## *zeta
- position in TS: union type alias
- position in SQL: `CREATE TYPE ... AS ENUM`
- position in C: `enum` with explicit integer mapping
- position in ordbok: canonical declaration with member ordering

## *x
- dimension members (e.g., `owner | admin | member | pending`)
- zero value (first member unless overridden)
- canonical member ordering (preserved across targets)

## bounds
- parse boundary: dimension syntax must parse without fallback
- perpendicularity boundary: members of one dimension cannot substitute for members of another
- emission boundary: all three targets must emit the same member set

## R.k
1. lex `.szh` file
2. parse dimension declaration
3. kindcheck (dimension is ξ by kind slot)
4. perpcheck (no cross-dimension leakage)
5. exhaustcheck (all members declared explicitly)
6. emit C enum → `include/gen/kind.h` etc.
7. emit TS union → `ordbok/out/*.ts`
8. emit SQL type → `ordbok/out/*.sql`

## omega
All four enum headers generated, all three targets agree on member ordering, `make bootstrap` stage 4 reports zero diff on enum files.

## DeltaR.k
None. Status: `resolved`. H=0.

## authority
- Source: `ordbok/compiler/kinds.szh`, `decl_types.szh`, `expr_types.szh`, `token_types.szh`
- Compiler: `src/parse_decl.c` (dimension parsing), `src/emit_c.c` (enum emission)
- Generated: `include/gen/kind.h`, `decl_type.h`, `expr_type.h`, `token_type.h`

## verification
```
make bootstrap   # stage 4 byte-compare passes
make test        # tests/m1_enums.sh passes
```

Completion evidence: PROJECT_JOURNAL.md §M1 (H remaining: 0).
