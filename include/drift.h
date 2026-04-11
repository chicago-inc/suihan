/*
 * suhc — drift.h
 * Ordbok ↔ Spoxis drift detection.
 *
 * Compares a compiled ordbok Program against a TS scan result.
 * Reports per-declaration S (implicit solipsism) and drift status.
 */

#ifndef SUHC_DRIFT_H
#define SUHC_DRIFT_H

#include "ast.h"
#include "ts_scanner.h"
#include <stdio.h>

/* Drift status for a single declaration */
typedef enum {
    DRIFT_MATCHED,      /* ordbok and Spoxis agree */
    DRIFT_ORDBOK_ONLY,  /* exists in ordbok, not in Spoxis */
    DRIFT_SPOXIS_ONLY,  /* exists in Spoxis, not in ordbok */
    DRIFT_DIVERGED      /* both exist but differ */
} DriftStatus;

/* Per-declaration drift report */
typedef struct {
    char        *decl_name;
    char        *decl_type;     /* "projection", "meihua", "xi_const" */
    DriftStatus  status;
    double       s_value;       /* 0.0 = perfect match, 1.0 = no match */
    char        *detail;        /* human-readable drift description */
} DriftEntry;

/* Complete drift report */
typedef struct {
    DriftEntry *entries;
    size_t      count;
    size_t      capacity;

    double      aggregate_s;    /* weighted average S */
    int         matched;
    int         ordbok_only;
    int         spoxis_only;
    int         diverged;
} DriftReport;

/* Compare an ordbok program against a TS scan result.
 * The program should already be parsed and checked. */
DriftReport drift_compare(Program *ordbok, TsScanResult *spoxis);

/* Print drift report to file handle. */
void drift_print(const DriftReport *report, FILE *out);

/* Print drift report as JSON to file handle. */
void drift_print_json(const DriftReport *report, FILE *out);

/* Free drift report. */
void drift_free(DriftReport *report);

#endif /* SUHC_DRIFT_H */
