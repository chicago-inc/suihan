# Suihan Rule Detection Matrix

Every constitutional rule and compiler invariant classified by whether a detection mechanism exists. Adapted from Spoxis' `D_RULE_DETECTION_MATRIX.md`.

**Detection types:**
- `mechanical` — code/grep/script/pass can detect violation today
- `manual` — human review or pre-commit discipline required; detection criteria defined but not automated
- `dead` — rule has no plausible violation path OR is documenting a structural invariant enforced by another rule
- `gap` — rule exists, no detector of any kind; either build one or reclassify

**Audit priority** — **P0** = load-bearing rule with ungoverned violations likely in production; **P1** = governed elsewhere but detection incomplete; **P2** = low-priority detection debt.

---

## Constitutional Rules

| Rule | Name | Detection type | Detector location | Audit priority |
|---|---|---|---|---|
| Axiom | Identity | dead | Any violation produces a compile error via kindcheck; cannot be separately tested | — |
| Units are foundation | Commensurability + zero | mechanical | dimension declarations require explicit zero; perpcheck enforces incommensurable separation | — |
| The Kind System | ξ/ζ/x/R.k/ω/ΔR.k slots | mechanical | kindcheck pass (`src/kindcheck.c`) | P0 |
| Six Bloat Categories | #1 reduplication | mechanical | audit S-score surfaces duplicate declarations | P1 |
| Six Bloat Categories | #2 niche pipe | mechanical | ζ-in-ξ-position detector; lints hand-written equivalents of ordbok shapes | **P0** |
| Six Bloat Categories | #3 temporal sediment | mechanical (partial) | dead-code pass; reachability analysis incomplete for dispatch tables | P1 |
| Six Bloat Categories | #4 failure to derive | manual | requires recognizing that a hand-written pattern could be a dispatch table; no lint yet | **P0** |
| Six Bloat Categories | #5 scope confusion | mechanical | kindcheck rejects wrong-slot assignment | — |
| Six Bloat Categories | #6 obtruding documentation | manual | human review of doc-vs-rule alignment | P2 |
| Perpendicularity | Cross-dimension substitution | mechanical | `src/perpcheck.c` | — |
| Exhaustiveness | Every projection covers dimension | mechanical | `src/exhaustcheck.c`; wildcard permitted with warning | — |
| Ordbok Authority | Prescriptive structure, descriptive coverage | manual | audit convergence ratio flags anti-ontology; human judgment required | **P0** |
| Self-Hosting | Lakatos boundary position | manual | CONSTITUTION.md §Self-Hosting table; silent retreat is not automatically detected | **P0** |
| The Bootstrap | Byte-identity invariant | mechanical | `make bootstrap` stage 4; 63 comparisons | — |
| Dispatch Modes | Annotation → signature contract | mechanical | emitter dispatch-mode auto-detection + warning on unknown mode | P2 |
| Build Discipline -1 | Empirical gating | manual | human must verify measurement before reading docs | **P0** |
| Build Discipline 1 | Read before write | manual | CI cannot detect unread code | P2 |
| Build Discipline 2 | Verify bootstrap after change | mechanical (opt-in) | pre-commit hook candidate; not wired | **P0** |
| Build Discipline 6 | Immutable committed headers | mechanical | `include/gen/` must be produced by `make regenerate`; diff check catches hand-edits | P1 |
| Build Discipline 7 | No empty ordbok files | mechanical | `find ordbok -name '*.szh' -empty` trivially scriptable; not in CI | P2 |
| Build Discipline 8 | Categorical realism | manual | requires recognizing transient measurement treated as permanent; no detector | P1 |
| Cycle Prevention P1 | Morphism lock | manual | developer discipline | P2 |
| Cycle Prevention P2 | Binary exclusion | mechanical | `.gitignore` + pre-commit hook catches `*.o`, `*.d`, generated output | P2 |
| Cycle Prevention P3 | FTDF limit | manual | intervention-depth counter; same gap as Spoxis D22 | **P0** |
| Cycle Prevention P5 | Token gating | manual | agent self-discipline; no linter | P1 |
| Three Execution Layers | meihua/zhulin/songqiao perpendicular | mechanical | perpcheck over execution-layer axis | — |
| Amendment Protocol | Bootstrap must pass after amendment | mechanical | `make bootstrap` | — |
| Reserved Keywords | Dual use via lookahead | mechanical (Lakatos residual) | parser + lookahead disambiguator | — |
| Self-Application | Constitution is subject to itself | dead | enforced by the bootstrap — if principle fails its own test, bootstrap diverges | — |

---

## P0 Detector Debt (highest priority)

Six rules with no detector of any kind, or manual-only detectors that have known recurring violations:

| Rule | Governed domain | Why P0 |
|---|---|---|
| Kind slots (ξ/ζ/x/R.k/ω/ΔR.k) | core compiler invariant | kindcheck works, but new dispatch modes can accidentally route values to wrong slot; coverage needs periodic audit |
| Niche pipe detection (bloat #2) | hand-written equivalents of ordbok shapes | every hand-written mirror is a ticking time bomb; `suhc --diff` exists but is not scheduled |
| Failure to derive (bloat #4) | recognizing declarable patterns | requires recognizing that a hand-written if-ladder could be a dispatch table; no automated pattern detector |
| Ordbok authority — anti-ontology | claiming coverage not earned | audit convergence flags it only indirectly; adversarial review needed |
| Self-Hosting Lakatos boundary | silent retreat of declarable code to hand-C | no detector; only revealed by reading commits |
| BD #2 bootstrap-after-change | skipped `make bootstrap` | currently discipline-only; pre-commit hook would fix |
| Cycle Prevention P3 FTDF limit | intervention-depth counter | same gap Spoxis flagged; ~2 hours of tooling to fix |

**Next sprint candidate:** a `scripts/audit-compiler.sh` that: (1) greps for hand-written dispatch patterns shaped like `if ... else if ... else if` covering enum values, (2) runs `./suhc --diff` against known Python/TS/SQL mirror sites, (3) fails with non-zero if either returns results. Add a pre-commit hook invoking `make bootstrap`.

---

## Dead Entries

- Axiom (Identity) — any violation is a kindcheck failure; no separate detector needed
- Scope confusion (bloat #5) — always a kind-slot mismatch; kindcheck covers
- Self-Application — bootstrap is the self-test; no separate detector

---

## How To Use This Matrix

1. Before adding a new constitutional rule or compiler invariant, declare its detection type. `gap` entries require a detector commitment or reclassification to `manual` with explicit review cadence.
2. Each bootstrap verification updates `Last triggered` for the mechanical rules that fired.
3. Annually, P0 detector debt must decrease. A P0 rule ungoverned for more than two bootstraps is constitutional smegmacrum and should either gain a detector or be deleted.
