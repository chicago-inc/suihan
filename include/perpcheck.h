/*
 * suhc — the suihan compiler
 * perpcheck.h — perpendicularity checker interface
 *
 * Enforces dimensional constraints (§3). Values from
 * perpendicular dimensions cannot substitute for each other.
 * This is stronger than a type error — it is a dimensional error.
 */

#ifndef SUHC_PERPCHECK_H
#define SUHC_PERPCHECK_H

#include "ast.h"
#include "diagnostic.h"

/* Run the perpendicularity checker over a kind-checked program. */
void perpcheck(Program *prog, DiagList *diags);

#endif /* SUHC_PERPCHECK_H */
