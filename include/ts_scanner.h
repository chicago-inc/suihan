/*
 * suhc — ts_scanner.h
 * Lightweight TypeScript scanner for drift detection.
 *
 * NOT a full TS parser. Extracts only the structural patterns
 * that matter for ordbok comparison:
 *   - export const NAME = VALUE
 *   - switch/case branches
 *   - function signatures (name + params)
 *   - ternary chains (x === 'VALUE' ? ... : ...)
 *
 * The scanner's S > 0 for complex TS patterns. This is
 * intentional and acknowledged (D13 applied to the tool itself).
 */

#ifndef SUHC_TS_SCANNER_H
#define SUHC_TS_SCANNER_H

#include <stddef.h>
#include <stdbool.h>

/* ------------------------------------------------------------ */
/* Extracted structures                                          */
/* ------------------------------------------------------------ */

/* An exported constant: export const NAME = VALUE */
typedef struct {
    char *name;        /* const name (original case) */
    char *value;       /* string literal or number literal */
} TsConst;

/* A case branch from a switch statement or ternary chain */
typedef struct {
    char *case_value;  /* the matched value (string literal content) */
    char *result;      /* the result expression (first string literal or identifier) */
} TsCaseBranch;

/* A switch block or ternary chain */
typedef struct {
    char          *switch_var;    /* the variable being switched on */
    TsCaseBranch  *branches;
    size_t         branch_count;
    size_t         branch_capacity;
} TsSwitchBlock;

/* A function signature */
typedef struct {
    char   *name;          /* function name */
    char  **params;        /* parameter names */
    size_t  param_count;
    bool    is_exported;
} TsFunction;

/* Complete scan result for a single TS file */
typedef struct {
    TsConst       *consts;
    size_t         const_count;

    TsSwitchBlock *switches;
    size_t         switch_count;

    TsFunction    *functions;
    size_t         function_count;

    char          *filename;
} TsScanResult;

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

/* Scan a TypeScript source string. Returns a TsScanResult
 * that must be freed with ts_scan_free(). */
TsScanResult ts_scan(const char *source, const char *filename);

/* Free all memory in a scan result. */
void ts_scan_free(TsScanResult *r);

/* Print a scan result summary to stdout (for debugging). */
void ts_scan_print(const TsScanResult *r);

#endif /* SUHC_TS_SCANNER_H */
