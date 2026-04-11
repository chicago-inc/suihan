# Ordbok–Spoxis Equivalence Report

Generated: Sprint 2B
Compiler: suhc 0.4.0 (7,625 lines C)
Ordbok: 9 files, 82 declarations, 465 lines

---

## Coverage Summary

| Metric | Count | Notes |
|---|---|---|
| Spoxis lib/ files total | ~46 | All .ts files in src/lib/ |
| Structural files (ζ-projections + pure computations) | ~22 | Target population |
| Ordbok-covered files | 19 | 9 already + 10 new |
| Coverage (structural) | **86%** | 19 / 22 |
| Coverage (all lib/) | **41%** | 19 / 46 |
| Projections encoded | 13 | |
| Meihua encoded | 9 | |
| Traversals encoded | 5 | |

---

## Per-File Equivalence

### Structural Match (S ≈ 0) — Generated ≅ Hand-Written

| Spoxis File | Ordbok Location | Arms/Cases | Status |
|---|---|---|---|
| roleLabels.ts | structural.szh :: role_label | 10 | **Match** — same role×type matrix |
| resourceShape.ts | structural.szh :: resource_shape | 4 | **Match** — headedness→shape |
| paymentShape.ts | structural.szh :: payment_shape | 4 | **Match** — payment_model→shape |
| reserveConstants.ts | structural.szh :: reserve_status | 15 | **Match** — status→label+color |
| listingLabels.ts | structural.szh :: listing_labels | 5 | **Match** — type×visibility→label |

### Structural Match with Extra Coverage

| Spoxis File | Ordbok Location | Arms | Status |
|---|---|---|---|
| projectLabels.ts | structural.szh :: project_labels | 16 | **Match+** — ordbok covers 4 industries, Spoxis covers ~3; ordbok adds church |
| organizationLabels.ts | structural.szh :: organization_labels | 16 | **Match+** — ordbok covers 4 industries + defaults |

### Missing Arms (S > 0) — Ordbok Incomplete

| Spoxis File | Ordbok Location | Ordbok Arms | Spoxis Arms (est.) | Gap |
|---|---|---|---|---|
| notificationMeta.ts | notifications.szh :: notification_meta | 36 | ~40+ | ~4 notification types not yet encoded (e.g., system_alert, resource_update) |
| notificationRouting.ts | notifications.szh :: notification_route | 13 | ~15 | ~2 reference_types missing |
| eventPermissions.ts | permissions.szh :: event_permission | 25 | ~30 | Spoxis may have additional actions (e.g., export, share) |
| projectPermissions.ts | permissions.szh :: project_permission | 25 | ~28 | ~3 project-specific actions |

### Signature Match — Different Structure

| Spoxis File | Ordbok Location | Status |
|---|---|---|
| attendeeVisibility.ts | pipeline.szh :: resolve_attendee_visibility | **Traversal** — ordbok models as full URR pipeline; Spoxis is a switch statement. Structural equivalence confirmed but representation differs. |
| geocoding.ts (haversine) | pipeline.szh :: haversine_distance | **Match** — identical formula, ordbok emits both TS (Math.*) and SQL (PostgreSQL math) |

### New Meihua — No Direct Spoxis Equivalent

| Ordbok Function | Spoxis Equivalent | Status |
|---|---|---|
| compliance_score | complianceCalculator.ts | **Partial** — ordbok captures core formula; Spoxis has additional category breakdowns |
| hot_cost | hotCostCalculator.ts | **Partial** — ordbok captures base formula; Spoxis has tax/tip layers |
| token_entropy | entropy.ts | **Match** — identical formula: length × log₂(62) |
| days_between | dateUtils.ts | **Partial** — ordbok captures ms→days; Spoxis has timezone-aware variants |
| delivery_capacity | logistics.ts | **Partial** — ordbok captures throughput formula; Spoxis has route optimization |
| percentage | (utility) | **New** — common computation, no single Spoxis file |
| lerp | (utility) | **New** — common computation, no single Spoxis file |
| clamp | (utility) | **New** — common computation, no single Spoxis file |

### Venue Constants

| Spoxis File | Ordbok Location | Status |
|---|---|---|
| venueConstants.ts | venue.szh :: venue_type_label | **Match** — type→label/icon/description matrix |
| venueConstants.ts | venue.szh :: venue_status | **Match** — booking_status→label+color |
| venueConstants.ts | venue.szh :: ξ constants | **Match** — default_map_zoom, venue_search_radius |

### Out of Scope (Deferred)

| Spoxis File | Reason |
|---|---|
| resourcePermissions.ts | Complex multi-table RLS logic — traversal territory |
| projectDomain.ts | Vocabulary resolution — governs projectLabels upstream |
| activityTracker.ts | Side-effecting (writes to DB) |
| guestCrm.ts | Complex multi-entity pipeline |
| subscription.ts, share.ts, referral.ts | Third-party integration |
| Upload files (8) | Side-effecting I/O |
| supabase.ts, theme.ts, presence.ts | Infrastructure |

---

## S Measurement

**Ordbok S against Spoxis structural files:**

- Sprint 2A: S ≈ 0.59 (9 / 22 files covered)
- Sprint 2B: S ≈ 0.14 (19 / 22 files covered)

**Remaining gap (S = 0.14):**
- resourcePermissions.ts — complex RLS, needs traversal-level encoding
- projectDomain.ts — vocabulary resolution, upstream of projectLabels
- activityTracker.ts — side-effecting, may not be encodable as projection/meihua

**Per-declaration S:**
- Full structural match: 9 projections (S = 0)
- Match with gaps: 4 projections (S ≈ 0.1–0.2)
- Partial capture: 5 meihua (S ≈ 0.3 — formulas correct but Spoxis has richer variants)
- New utility meihua: 3 (S = 0 — ordbok is source of truth)

---

## Observations

1. **Projections are the highest-fidelity encodings.** The case-arm structure maps 1:1 between ordbok projections and Spoxis switch statements. This is the ordbok's strongest axis.

2. **Meihua capture core formulas but miss application-layer variants.** The hot_cost formula is correct but Spoxis wraps it with tax, tip, and discount layers. This is expected — those layers are ζ-computation (application context), not ξ (the formula itself).

3. **Traversals are structurally correct but representationally different.** The ordbok models the full URR pipeline; Spoxis implements as flat functions. The structural equivalence is there — the ordbok is more rigorous.

4. **SQL emission is a novel capability.** No Spoxis file has a SQL equivalent. The ordbok generates both TS and SQL from the same source. This is the first demonstration of the compiler's dual-target value.

5. **The ordbok is becoming prescriptive.** For the 9 fully-matching projections, either representation could be source of truth. For the 3 utility meihua (percentage, lerp, clamp), the ordbok IS the source of truth — they have no Spoxis equivalent. The governance question (descriptive vs. prescriptive) is the ungoverned cast from this sprint.
