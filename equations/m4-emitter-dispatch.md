# EQ-SUHC-004 — Emitter Dispatch Traversal (M4)

## EQ-ID
EQ-SUHC-004

## xi
`ordbok/compiler/emit_dispatch.szh` with `yields ω dispatch` annotations, one per emission target.

## *zeta
- TS emitter dispatch table
- SQL emitter dispatch table
- C emitter dispatch table
- per-DeclType handler functions

## *x
- DeclType × handler × emission-target triple product
- handler output kind must be ω (terminal — no further transform)

## bounds
- exhaustiveness: every DeclType has a handler in every target
- kind slot: handler output is always ω, never ζ or x
- emission: no silent fallthrough for unknown DeclType

## R.k
1. parse `emit_dispatch.szh`
2. detect emitter dispatch mode
3. emit per-target dispatch tables
4. at compile time, `emit_decl(FILE*, Decl*)` consumes its target's table
5. per-target handler runs
6. bootstrap compare

## omega
Adding a new DeclType to the ordbok requires adding one handler per target; emitter dispatch is table-driven, not if-ladder.

## DeltaR.k
None. Status: `resolved`.

## authority
- Source: `ordbok/compiler/emit_dispatch.szh`
- Compiler: `src/emit_ts.c`, `src/emit_sql.c`, `src/emit_c.c`
- Generated: dispatch tables embedded in emitter source

## verification
```
make bootstrap
make test        # tests/m4_emit_dispatch.sh
```

Completion evidence: PROJECT_JOURNAL.md §M4 (H remaining: 0).
