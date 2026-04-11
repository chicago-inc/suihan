/*
 * suhc — the suihan compiler
 * decidability.h — oracle ceiling enforcement
 *
 * The oracle ceiling (D10) as a type boundary:
 *   If a traversal produces undecidable output, downstream
 *   traversals that ACT on it without user confirmation are
 *   rejected. Presentation of undecidable output is valid.
 */

#ifndef SUHC_DECIDABILITY_H
#define SUHC_DECIDABILITY_H

#include "ast.h"
#include "diagnostic.h"

/* Run the decidability checker over a program.
 * Pass 1: classify each traversal's output as decidable/undecidable.
 * Pass 2: check downstream traversals for action on undecidable input. */
void decidability_check(Program *prog, DiagList *diags);

#endif /* SUHC_DECIDABILITY_H */
