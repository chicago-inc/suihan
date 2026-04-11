/*
 * suhc — sql_validate.c
 * Lightweight SQL syntax validation for emitted code.
 *
 * Sprint 4B: No external dependencies. Validates structural
 * correctness of emitted SQL — the minimum needed to ensure
 * the compiler's output is not a smegmacrum (eval rule §12).
 *
 * Does NOT validate SQL semantics (type correctness, name
 * resolution, etc.) — that would require a real database.
 * The S measurement of this validator is intentionally > 0
 * for semantic checks (D13 applied to the tool itself).
 */

#include "sql_validate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* ------------------------------------------------------------ */
/* Helpers                                                       */
/* ------------------------------------------------------------ */

static void add_error(SqlValidateResult *r, const char *fmt, ...) {
    if (r->error_count >= 16) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->errors[r->error_count], 256, fmt, ap);
    va_end(ap);
    r->error_count++;
    r->valid = false;
}

/* Case-insensitive prefix match */
static bool prefix_ci(const char *str, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix))
            return false;
        str++;
        prefix++;
    }
    return true;
}

/* Skip whitespace, return new position */
static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Count occurrences of a substring */
static int count_substr(const char *haystack, const char *needle) {
    int count = 0;
    size_t nlen = strlen(needle);
    const char *p = haystack;
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += nlen;
    }
    return count;
}

/* ------------------------------------------------------------ */
/* Core validation                                               */
/* ------------------------------------------------------------ */

SqlValidateResult sql_validate(const char *sql, size_t len) {
    SqlValidateResult result;
    memset(&result, 0, sizeof(result));
    result.valid = true;

    if (!sql || len == 0) {
        add_error(&result, "empty SQL input");
        return result;
    }

    /* --- Check 1: Balanced parentheses --- */
    int paren_depth = 0;
    bool in_string = false;
    bool in_dollar = false;
    int line = 1;

    const char *p = sql;
    const char *end = sql + len;

    while (p < end) {
        if (*p == '\n') line++;

        /* Track $$ blocks */
        if (!in_string && p + 1 < end && p[0] == '$' && p[1] == '$') {
            in_dollar = !in_dollar;
            p += 2;
            continue;
        }

        /* Skip content inside $$ blocks for paren/quote checks */
        if (in_dollar) {
            p++;
            continue;
        }

        /* Track single-quoted strings */
        if (*p == '\'') {
            if (in_string) {
                /* Check for escaped quote '' */
                if (p + 1 < end && p[1] == '\'') {
                    p += 2;
                    continue;
                }
                in_string = false;
            } else {
                in_string = true;
            }
            p++;
            continue;
        }

        if (in_string) {
            p++;
            continue;
        }

        /* Track -- line comments */
        if (*p == '-' && p + 1 < end && p[1] == '-') {
            while (p < end && *p != '\n') p++;
            continue;
        }

        /* Track parentheses */
        if (*p == '(') paren_depth++;
        if (*p == ')') {
            paren_depth--;
            if (paren_depth < 0) {
                add_error(&result, "line %d: unmatched closing parenthesis", line);
                paren_depth = 0; /* recover */
            }
        }

        p++;
    }

    if (paren_depth > 0) {
        add_error(&result, "unclosed parenthesis (%d open at end)", paren_depth);
    }
    if (in_string) {
        add_error(&result, "unterminated string literal at end of input");
    }

    /* --- Check 2: $$ delimiters paired --- */
    int dollar_count = count_substr(sql, "$$");
    if (dollar_count % 2 != 0) {
        add_error(&result, "$$ delimiters not paired (found %d)", dollar_count);
    }

    /* --- Check 3: CREATE OR REPLACE FUNCTION structure --- */
    /* Search case-insensitively for CREATE */
    const char *create_pos = sql;
    while (create_pos && (size_t)(create_pos - sql) < len) {
        /* Find next 'CREATE' (case-insensitive scan) */
        const char *found = NULL;
        for (const char *cp = create_pos; cp + 6 <= sql + len; cp++) {
            if (prefix_ci(cp, "create")) { found = cp; break; }
        }
        if (!found) break;
        create_pos = found;

        const char *after = skip_ws(create_pos + 6);
        if (prefix_ci(after, "or replace")) {
            after = skip_ws(after + 10);
            if (prefix_ci(after, "function")) {
                /* Found CREATE OR REPLACE FUNCTION — check for $$ body */
                const char *fn_start = after + 8;
                const char *body_start = strstr(fn_start, "$$");
                if (body_start) {
                    const char *body_end = strstr(body_start + 2, "$$");
                    if (body_end) {
                        /* Check body is non-empty */
                        const char *body_content = body_start + 2;
                        const char *bp = body_content;
                        bool has_content = false;
                        while (bp < body_end) {
                            if (!isspace((unsigned char)*bp)) {
                                has_content = true;
                                break;
                            }
                            bp++;
                        }
                        if (!has_content) {
                            add_error(&result, "empty function body between $$ delimiters");
                        }
                    }
                }
            }
        }

        create_pos++;
        if ((size_t)(create_pos - sql) >= len) break;
    }

    /* --- Check 4: Statements end with semicolons --- */
    /* Find the last non-whitespace character */
    const char *last = sql + len - 1;
    while (last > sql && isspace((unsigned char)*last)) last--;
    if (last > sql && *last != ';') {
        /* Check if it's just comments at the end */
        bool only_comments = true;
        const char *lp = sql;
        while (lp <= last) {
            lp = skip_ws(lp);
            if (lp > last) break;
            if (*lp == '-' && lp + 1 <= last && lp[1] == '-') {
                while (lp <= last && *lp != '\n') lp++;
                continue;
            }
            if (*lp && !isspace((unsigned char)*lp)) {
                only_comments = false;
                break;
            }
            lp++;
        }
        if (!only_comments) {
            add_error(&result, "SQL does not end with semicolon");
        }
    }

    return result;
}

/* ------------------------------------------------------------ */
/* File-based validation                                         */
/* ------------------------------------------------------------ */

SqlValidateResult sql_validate_file(const char *path) {
    SqlValidateResult result;
    memset(&result, 0, sizeof(result));
    result.valid = true;

    FILE *f = fopen(path, "rb");
    if (!f) {
        add_error(&result, "cannot open '%s'", path);
        return result;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sz + 1);
    if (!buf) {
        fclose(f);
        add_error(&result, "out of memory reading '%s'", path);
        return result;
    }

    size_t nread = fread(buf, 1, sz, f);
    (void)nread;
    buf[sz] = '\0';
    fclose(f);

    result = sql_validate(buf, (size_t)sz);
    free(buf);
    return result;
}

/* ------------------------------------------------------------ */
/* Reporting                                                     */
/* ------------------------------------------------------------ */

void sql_validate_print(const SqlValidateResult *result, const char *filename) {
    if (result->valid) {
        printf("  SQL OK: %s\n", filename);
    } else {
        fprintf(stderr, "  SQL INVALID: %s\n", filename);
        for (int i = 0; i < result->error_count; i++) {
            fprintf(stderr, "    - %s\n", result->errors[i]);
        }
    }
}
