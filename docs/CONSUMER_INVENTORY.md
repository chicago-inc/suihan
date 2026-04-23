# Suihan Consumer Inventory

High-risk interface inventory for preventing incomplete cascades when an ordbok declaration changes.

This is not a full dependency graph replacement. It is the compact list of emission surfaces most likely to generate partial fixes when modified without a consumer sweep.

---

## Inventory

### Suhc Self-Consumption

- Interface: `ordbok/compiler/*.szh` → `include/gen/*.h` → suhc binary
- Consumer classes:
  - enum definitions consumed by parser, kindcheck, emitter
  - string tables consumed by audit + diagnostic reporters
  - dispatch tables consumed at runtime during emission
- Related equations: `EQ-SUHC-001`, `EQ-SUHC-002`, `EQ-SUHC-003`, `EQ-SUHC-004`, `EQ-SUHC-005`, `EQ-SUHC-006`, `EQ-SUHC-007`
- Change detector: `make bootstrap` — any self-consumption drift fails byte-identity stage

### Spoxis App (TS Consumer)

- Interface: `ordbok/*.szh` (domain files: `permissions.szh`, `rendering.szh`, `venue.szh`, etc.) → emitted TS types
- Consumer classes:
  - type imports in `src/lib/`
  - route param derivations
  - resource-shape resolution
- Related equations: `EQ-SUHC-009`
- Change detector: **missing** — no mechanical check today; hand-authored mirrors drift silently. This is the primary open ΔR.k.

### Spoxis App (SQL Consumer)

- Interface: `ordbok/*.szh` → emitted PostgreSQL functions and types
- Consumer classes:
  - migration files calling emitted RPCs
  - RLS policies over emitted enum types
- Related equations: `EQ-SUHC-009`
- Change detector: **missing** — migrations reference types by name; a rename in ordbok produces runtime SQL error, not compile-time error.

### Mongwu Kernel (C Consumer, future)

- Interface: `ordbok/kernel_*.szh` → emitted C + assembly
- Consumer classes:
  - kernel ABI boundaries
  - process/memory/scheduling tables
- Related equations: future `EQ-SUHC-010`
- Change detector: planned — kernel assembly regression test, not yet implemented

### Audit Tooling

- Interface: `./suhc --audit`, `./suhc --convergence`, `./suhc --diff`
- Consumer classes:
  - CI integration (future)
  - developer pre-commit check
  - this CONSUMER_INVENTORY itself (self-reference)
- Related equations: `EQ-SUHC-008`
- Change detector: present but ad-hoc; no scheduled run

---

## When To Sweep Consumers

A consumer sweep is required when any of the following change:

- an enum dimension (add, remove, or rename a member) → sweep both self-consumption and Spoxis TS/SQL
- a dispatch-mode annotation → sweep the emitter + every target-specific handler
- a reserved keyword → sweep the parser + any ordbok file using the keyword as a member
- an emission target's code-shape (e.g., TS class vs. type alias) → sweep all consumers in that target
- the Lakatos barrier position (moving Pratt into the ordbok, etc.) → sweep every `EQ-SUHC-*` boundary contract

If the change touches any row above and the consumer was **not** swept, the traversal has not closed.

---

## Forbidden Consumer Patterns

- **Hand-authored mirror:** a consumer duplicates an ordbok-declared shape by hand. Every such mirror is niche pipe (bloat #2) and a Yoneda gap.
- **Named import of removed member:** consumer references an enum member that ordbok no longer declares; surfaces at runtime as `undefined` in TS or missing enum label in SQL.
- **Target-specific divergence:** TS consumer uses one shape, SQL consumer uses another, for the same ordbok source; consistency is the emitter's job, not the consumer's.
