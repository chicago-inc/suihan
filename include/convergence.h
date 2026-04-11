/*
 * suhc — the suihan compiler
 * convergence.h — convergence tracker interface
 *
 * Phase 0C: Track ΔR.k across builds, compute convergence ratio,
 * detect smegmacra, measure S, report Yoneda completeness.
 */

#ifndef SUHC_CONVERGENCE_H
#define SUHC_CONVERGENCE_H

#include "ast.h"
#include "diagnostic.h"
#include <stdio.h>

/* Convergence report data */
typedef struct {
    int total_cast;         /* total ΔR.k possibility spaces */
    int governed;           /* cast governed by existing ordbok terms */
    int ungoverned;         /* cast requiring amendment */
    double ratio;           /* convergence ratio: |ΔR.k_{n+1}| / |ΔR.kₙ| */
    double prev_ratio;      /* previous build's ratio (-1 if no history) */
    int smegmacra;          /* write-without-read paths */
    int yoneda_gaps;        /* declared but unobserved morphisms */
} ConvergenceReport;

/* Compute convergence report for a program. */
ConvergenceReport convergence_analyze(Program *prog, DiagList *diags);

/* Print convergence report to a file handle. */
void convergence_print(const ConvergenceReport *report, FILE *out);

/* Save build history for trend tracking.
 * history_path: path to .suhc_history file (created if missing). */
void convergence_save_history(const ConvergenceReport *report,
                              const char *history_path);

/* Load previous ratio from history file. Returns -1.0 if no history. */
double convergence_load_prev_ratio(const char *history_path);

/* Per-file cast entry for directory-level reports */
typedef struct {
    char filename[256];
    int total;
    int governed;
    int ungoverned;
    int dimensions;
    int projections;
    int meihua;
} ConvergenceFileEntry;

/* Directory-level convergence report */
typedef struct {
    int total_spaces;       /* total possibility spaces (dimensions + proj arms + meihua params + imports) */
    int governed_spaces;    /* governed by validation (exhaustive proj, valid meihua, typed dims) */
    int ungoverned_spaces;  /* remaining */
    int prev_ungoverned;    /* from .convergence.json baseline */
    double ratio;           /* ungoverned_now / ungoverned_prev */
    bool has_baseline;      /* whether a .convergence.json was found */
    ConvergenceFileEntry files[128];
    int file_count;
} ConvergenceDirReport;

/* Scan all .szh files in a directory and produce an aggregate convergence report.
 * Reads/writes .convergence.json in the directory for trend tracking. */
int convergence_scan_dir(const char *dir_path, FILE *out);

#endif /* SUHC_CONVERGENCE_H */
