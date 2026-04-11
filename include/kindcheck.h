/*
 * suhc — the suihan compiler
 * kindcheck.h — kind checker interface
 *
 * Verifies every declaration carries exactly one kind (§2).
 * Detects scope confusion (§8 Error 5), immutability violations,
 * and kind mismatches at usage sites.
 */

#ifndef SUHC_KINDCHECK_H
#define SUHC_KINDCHECK_H

#include "ast.h"
#include "diagnostic.h"

/* Run the kind checker over a parsed program.
 * Fills in KIND_INFERRED nodes and emits diagnostics. */
void kindcheck(Program *prog, DiagList *diags);

#endif /* SUHC_KINDCHECK_H */
