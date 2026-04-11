/*
 * suhc — py_scanner.h
 * Lightweight Python scanner for drift detection.
 *
 * Extracts structural patterns from Python source that map to
 * ordbok declarations:
 *   - frozenset/set literals → dimension members (F03, F08)
 *   - dict literals → projection cases (F01, F09)
 *   - if/elif chains on string equality → categorical realism (F07)
 *   - hardcoded numeric thresholds → potential scope confusion (F11, F12)
 *   - def/class declarations → traversal/projection signatures
 *
 * Reuses TsScanResult structures so drift.c works unchanged.
 * The scanner's S > 0 for complex Python patterns (decorators,
 * comprehensions, dynamic dict construction). Acknowledged (D13).
 */

#ifndef SUHC_PY_SCANNER_H
#define SUHC_PY_SCANNER_H

#include "ts_scanner.h"  /* reuses TsScanResult, TsConst, etc. */

/* Scan a Python source string. Returns a TsScanResult
 * (same structure as TypeScript scan) for drift comparison.
 *
 * Extraction mapping:
 *   frozenset({'a','b',...})  → TsSwitchBlock (var = frozenset name)
 *   dict {'a': 'x', ...}     → TsSwitchBlock (branches = key→value)
 *   if x == 'a': / elif      → TsSwitchBlock (ternary-chain equivalent)
 *   NAME = <number>           → TsConst (hardcoded threshold)
 *   def name(params):         → TsFunction
 *   class Name:               → TsFunction (is_exported = True)
 */
TsScanResult py_scan(const char *source, const char *filename);

#endif /* SUHC_PY_SCANNER_H */
