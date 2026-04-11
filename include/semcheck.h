/*
 * suhc — semcheck.h
 * Cross-file semantic validation.
 *
 * Sprint 5A: Validates that projection case values match
 * their dimension members, across file boundaries.
 *
 * Constitutional grounding:
 *   Identity bridging: Two files modeling the same dimension
 *   must share type constraints.
 *   D13: The resolver's S against dimension semantics is 1.0
 *   without this check. Semcheck reduces S.
 */

#ifndef SUHC_SEMCHECK_H
#define SUHC_SEMCHECK_H

#include "ast.h"
#include "diagnostic.h"

typedef struct {
    int projections_checked;
    int dimensions_resolved;
    int dimension_mismatches;
    int cross_file_validations;
} SemcheckReport;

/* Run semantic checking on a program.
 * Issues notes (not errors) for dimension value mismatches. */
SemcheckReport semcheck(Program *prog, DiagList *diags);

#endif /* SUHC_SEMCHECK_H */
