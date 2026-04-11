/*
 * suhc — the suihan compiler
 * emitter.h — code generation interfaces
 *
 * Phase 0B: Compile .szh declarations to TypeScript and SQL.
 * The kind system and bloat checks run before emission.
 * If the input has errors, no code is generated.
 */

#ifndef SUHC_EMITTER_H
#define SUHC_EMITTER_H

#include "ast.h"
#include "diagnostic.h"
#include <stdio.h>

/* Emission target selector */
typedef enum {
    TARGET_NONE,
    TARGET_TYPESCRIPT,
    TARGET_SQL,
    TARGET_C,
    TARGET_ASM
} EmitTarget;

/* Emit TypeScript from a fully-checked program.
 * Returns 0 on success, non-zero on failure. */
int emit_typescript(Program *prog, FILE *out, DiagList *diags);

/* Emit SQL (PostgreSQL) from a fully-checked program.
 * Returns 0 on success, non-zero on failure. */
int emit_sql(Program *prog, FILE *out, DiagList *diags);

/* Emit C11 header from a fully-checked program.
 * Returns 0 on success, non-zero on failure. */
int emit_c(Program *prog, FILE *out, DiagList *diags);

/* Emit x86_64 NASM assembly from a fully-checked program.
 * Returns 0 on success, non-zero on failure. */
int emit_asm(Program *prog, FILE *out, DiagList *diags);

#endif /* SUHC_EMITTER_H */
