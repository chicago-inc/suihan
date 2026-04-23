# EQ-SUHC-007 — Bootstrap Verification Traversal (M7)

## EQ-ID
EQ-SUHC-007

## xi
Committed `include/gen/` headers + current `ordbok/` tree + most recently built `suhc` binary.

## *zeta
- stage-1 `suhc` built from committed headers
- stage-2 regenerated headers
- stage-3 `suhc` rebuilt from regenerated headers
- stage-4 byte comparison matrix (63 comparisons)
- stage-5 regression-test output

## *x
- byte-identity relation across stage-1 and stage-2 outputs
- test-pass relation on stage-2 binary

## bounds
- all five bootstrap stages must pass
- any divergence fails the equation — no partial credit
- Lakatos: Pratt algorithm, AST memory, lexer FSM, file I/O are procedural by constitutional declaration

## R.k
1. `make stage-1` — build `suhc` from committed `include/gen/`
2. `make regenerate` — run stage-1 suhc on `ordbok/` to produce new headers
3. `make stage-2` — rebuild `suhc` from regenerated headers
4. `make compare` — byte-compare stage-1 and stage-2 outputs (63 files)
5. `make test` — run 125 regression tests on stage-2 binary

## omega
Fixed point reached: the compiler built from its own output is identical to the compiler that produced that output. The ordbok and the compiler agree about what the compiler is.

Required artifacts:
- all 63 byte-compares == 0 diff
- all 125 regression tests pass
- `./suhc --audit ordbok/` reports no Yoneda gap introduced by the change

## DeltaR.k
- Status: `bounded`
- Pratt algorithm, AST memory management, lexer FSM, file I/O remain procedural. These are the declared Lakatos barrier. Shrinking this boundary requires constitutional amendment.

## authority
- `Makefile` bootstrap target
- `CONSTITUTION.md §The Bootstrap` and `§Self-Hosting`
- `tests/` regression suite

## verification
```
make bootstrap   # the traversal itself
```

Completion evidence: PROJECT_JOURNAL.md §M7 (fixed point reached, version 1.0.0).
