/*
 * suhc — the suihan compiler
 * bloatlint.h — bloat linter interface
 *
 * The six bloat causes (§8) are the six categories of compile
 * error/warning. Every diagnostic this pass produces is an
 * instance of one of these six. There are no other categories.
 *
 * 1. Reduplication — same ξ at two addresses
 * 2. Niche pipe — ζ in ξ position
 * 3. Temporal sediment — unreachable R.k
 * 4. Failure to derive — ω without R.k
 * 5. Scope confusion — wrong kind slot
 * 6. Obtruding documentation — R.k leaking into ω
 */

#ifndef SUHC_BLOATLINT_H
#define SUHC_BLOATLINT_H

#include "ast.h"
#include "diagnostic.h"

/* Run the bloat linter over a fully-checked program.
 * Emits diagnostics for all six bloat categories. */
void bloatlint(Program *prog, DiagList *diags);

#endif /* SUHC_BLOATLINT_H */
