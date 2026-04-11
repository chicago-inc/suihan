/*
 * suhc — sql_validate.h
 * Lightweight SQL syntax validation for emitted code.
 *
 * Sprint 4B: Validates emitted SQL without external dependencies.
 * Checks structural validity: balanced delimiters, $$ pairing,
 * CREATE OR REPLACE FUNCTION structure, non-empty bodies.
 *
 * Constitutional grounding:
 *   Smegmacrum (eval rule §12): Emitted SQL that doesn't parse
 *   is a semblance without referent.
 *   D13: The compiler's S against its own output is 1.0 without
 *   validation. This reduces S.
 */

#ifndef SUHC_SQL_VALIDATE_H
#define SUHC_SQL_VALIDATE_H

#include <stdbool.h>
#include <stddef.h>

/* Validation result for a single SQL file/string */
typedef struct {
    bool valid;
    int  error_count;
    char errors[16][256];   /* up to 16 error messages */
} SqlValidateResult;

/* Validate a SQL string. Checks:
 *   - Balanced parentheses
 *   - Balanced single quotes (outside $$ blocks)
 *   - $$ delimiters properly paired
 *   - CREATE OR REPLACE FUNCTION structure present (if any)
 *   - No empty function bodies (between $$ pairs)
 *   - Semicolons terminate statements
 */
SqlValidateResult sql_validate(const char *sql, size_t len);

/* Validate a SQL file by path. Returns result. */
SqlValidateResult sql_validate_file(const char *path);

/* Print validation result to stderr */
void sql_validate_print(const SqlValidateResult *result, const char *filename);

#endif /* SUHC_SQL_VALIDATE_H */
