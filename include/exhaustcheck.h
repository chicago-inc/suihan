/*
 * suhc — exhaustcheck.h
 * Exhaustiveness checker: verify projection case arms cover
 * all members of declared dimensions.
 *
 * Also validates meihua bodies for unknown function calls
 * and undefined identifier references.
 */

#ifndef SUHC_EXHAUSTCHECK_H
#define SUHC_EXHAUSTCHECK_H

#include "ast.h"
#include "diagnostic.h"

/* Run exhaustiveness checking on a parsed + resolved program.
 * Returns counts for audit reporting. */
typedef struct {
    int projections_checked;    /* projections with known-dimension invariants */
    int projections_exhaustive; /* fully exhaustive (all members covered) */
    int projections_defaulted;  /* has wildcard default but missing members */
    int projections_incomplete; /* missing members AND no default */
    int projections_no_default; /* projections without any wildcard arm */
    int cross_product_checked;  /* projections with two-axis dimension coverage */
    int cross_product_complete; /* all pairs covered */
    int meihua_checked;         /* meihua declarations validated */
    int meihua_clean;           /* no unknown calls or undefined idents */
    int meihua_arity_errors;    /* meihua-to-meihua arity mismatches */
} ExhaustReport;

ExhaustReport exhaustcheck(Program *prog, DiagList *diags);

#endif /* SUHC_EXHAUSTCHECK_H */
