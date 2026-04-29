/**
 * drift_test_spoxis.ts
 *
 * Synthetic Spoxis-side TypeScript file used by the drift tests
 * in tests/run_tests.sh. The drift detector compares this against
 * tests/drift_test.szh and reports whether the implementation
 * agrees with the ordbok.
 *
 * Designed to produce a specific mix of MATCH and DRIFT diagnostics:
 *   - app_version: matches the ordbok (both "2.1.0")
 *   - max_retries: drifts from the ordbok (5 in szh, 3 here)
 *   - status_label: missing entirely (ordbok-only, drift)
 *   - retry_delay: matches arity (both 2 params)
 */

export const app_version = "2.1.0";

export const max_retries = 3;

export function status_label(status: string): string {
  switch (status) {
    case "active": return "Active";
    case "inactive": return "Inactive";
    case "archived": return "Archived";
    default: return "Unknown";
  }
}

export function retry_delay(attempt: number, base_ms: number): number {
  return base_ms * Math.pow(2, attempt);
}
