# EQ-SUHC-002 — String Table Traversal (M2)

## EQ-ID
EQ-SUHC-002

## xi
Enum dimensions (output of EQ-SUHC-001). The string-table traversal is a 1D projection over each declared dimension.

## *zeta
- TS position: exported functions `kind_name()`, `*_from_name()` per dimension
- SQL position: `CREATE FUNCTION ... RETURNS text` per dimension
- C position: array-indexed or switch-based name lookup

## *x
- member → string forward projection
- string → member reverse projection (`*_from_name`)
- target-specific casing/style rules

## bounds
- exhaustiveness: every declared member must have a string
- reversibility: forward and reverse must be mutual inverses on declared members
- emission: all three targets generate both directions

## R.k
1. read enum dimension (from EQ-SUHC-001 output)
2. auto-detect dispatch mode (1D string-table)
3. emit forward table per target
4. emit reverse table per target
5. bootstrap compare

## omega
`kind_name(k) → "owner"` etc. in all three targets; `kind_from_name("owner") → k` in all three. Reverse lookup does not exist for members not declared in ordbok.

## DeltaR.k
None. Status: `resolved`.

## authority
- Source: inherits from EQ-SUHC-001 sources
- Compiler: `src/emit_ts.c`, `src/emit_sql.c`, `src/emit_c.c` (string-table branch)
- Generated: `_name.c`, `_name.ts`, `_name.sql`

## verification
```
make bootstrap   # stage 4 byte-compare on _name.*
make test        # tests/m2_string_tables.sh passes
```

Completion evidence: PROJECT_JOURNAL.md §M2 (H remaining: 0).
