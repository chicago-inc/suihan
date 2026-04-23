# EQ-SUHC-006 — Parser Dispatch Traversal (M6)

## EQ-ID
EQ-SUHC-006

## xi
`ordbok/compiler/parser_dispatch.szh` — keyword → parser-handler projection and kind-sigil → kind projection.

## *zeta
- keyword dispatch table
- kind-sigil dispatch table
- lookahead disambiguator (Lakatos, hand-C)

## *x
- TokenType × parser-handler projection
- kind-sigil character × Kind projection

## bounds
- exhaustiveness over reserved keyword set (CONSTITUTION.md §Reserved Keywords)
- **Lakatos boundary: the lookahead disambiguator stays in hand-C** (reserved keywords can appear as dimension member names; requires multi-token lookahead)
- emission: parser dispatch is table-driven

## R.k
1. parse `parser_dispatch.szh`
2. emit keyword dispatch table
3. emit kind-sigil dispatch table
4. at parse time, `parse_decl(Parser*, tok_t)` consumes the keyword table
5. kind sigils resolved via sigil table
6. ambiguous tokens routed to Lakatos lookahead
7. bootstrap compare

## omega
Every declaration keyword routes to the correct parser handler; every kind sigil resolves to a Kind. Adding a keyword requires one row in the dispatch table plus (if reserved) updating CONSTITUTION.md §Reserved Keywords.

## DeltaR.k
- Status: `bounded`
- Lookahead disambiguator remains hand-C. This is a declared Lakatos residual tied to the dual use of reserved words.

## authority
- Source: `ordbok/compiler/parser_dispatch.szh`
- Compiler: `src/parse_decl.c`
- Generated: keyword dispatch table, kind-sigil table

## verification
```
make bootstrap
make test        # tests/m6_parser_dispatch.sh
```

Completion evidence: PROJECT_JOURNAL.md §M6 (H remaining: ~0).
