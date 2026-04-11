/*
 * suhc — the suihan compiler
 * diagnostic.h — error and warning reporting
 *
 * Every diagnostic is classified by one of the six bloat causes
 * (§8) or as a parse/lex error. There are no other categories.
 */

#ifndef SUHC_DIAGNOSTIC_H
#define SUHC_DIAGNOSTIC_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    SEV_ERROR,
    SEV_WARNING,
    SEV_NOTE,
} Severity;

/* The six bloat causes + structural errors */
typedef enum {
    DIAG_PARSE_ERROR,           /* structural: can't parse */
    DIAG_LEX_ERROR,             /* structural: can't tokenize */
    DIAG_REDUPLICATION,         /* bloat #1: same ξ at two addresses */
    DIAG_NICHE_PIPE,            /* bloat #2: ζ in ξ position */
    DIAG_TEMPORAL_SEDIMENT,     /* bloat #3: unreachable R.k path */
    DIAG_FAILURE_TO_DERIVE,     /* bloat #4: ω without R.k */
    DIAG_SCOPE_CONFUSION,       /* bloat #5: wrong kind slot */
    DIAG_OBTRUDING_DOC,         /* bloat #6: R.k leaking into ω */
    DIAG_FORWARD_REFERENCE,     /* ordbok ordering violation */
    DIAG_KIND_MISMATCH,         /* kind system violation */
    DIAG_PERPENDICULAR_CROSS,   /* dimensional violation */
    DIAG_IMMUTABILITY,          /* ξ mutation attempt */
    DIAG_UNDECIDABLE_ACTION,    /* acting on undecidable output */
    DIAG_MISSING_CAST,          /* traversal without ΔR.k */
    DIAG_YONEDA_GAP,            /* declared but unobserved morphism */
    DIAG_IMPORT_ERROR,          /* cannot find or read imported module */
    DIAG_CIRCULAR_IMPORT,       /* circular dependency detected */
    DIAG_TYPE_MISMATCH,         /* incommensurable types at call site */
    DIAG_ARITY_MISMATCH,        /* wrong number of arguments */
    DIAG_UNKNOWN_TYPE,          /* type annotation references undeclared type */
} DiagCategory;

const char *diag_category_name(DiagCategory cat);

typedef struct {
    Severity      severity;
    DiagCategory  category;
    int           line;
    int           col;
    char         *message;
    char         *filename;
} Diagnostic;

/* Cascade suppression limits */
#define DIAG_MAX_ERRORS_PER_CATEGORY 10
#define DIAG_MAX_TOTAL_ERRORS        50

/* Number of diagnostic categories (for per-category counting) */
#define DIAG_CATEGORY_COUNT 20

typedef struct {
    Diagnostic *items;
    size_t      count;
    size_t      capacity;
    int         error_count;
    int         warning_count;

    /* Cascade suppression */
    int         category_error_count[DIAG_CATEGORY_COUNT];
    int         suppressed_count;     /* errors hidden by cascade limits */
    int         total_limit_reached;  /* true if DIAG_MAX_TOTAL_ERRORS hit */
} DiagList;

DiagList *diag_list_new(void);
void      diag_list_free(DiagList *dl);

void diag_emit(DiagList *dl, Severity sev, DiagCategory cat,
               const char *filename, int line, int col,
               const char *fmt, ...);

/* Convenience macros */
#define diag_error(dl, cat, fn, ln, co, ...) \
    diag_emit(dl, SEV_ERROR, cat, fn, ln, co, __VA_ARGS__)
#define diag_warn(dl, cat, fn, ln, co, ...) \
    diag_emit(dl, SEV_WARNING, cat, fn, ln, co, __VA_ARGS__)
#define diag_note(dl, cat, fn, ln, co, ...) \
    diag_emit(dl, SEV_NOTE, cat, fn, ln, co, __VA_ARGS__)

/* Print all diagnostics to stderr. */
void diag_print_all(const DiagList *dl);

/* Returns true if any errors (not just warnings). */
int diag_has_errors(const DiagList *dl);

/* Output format for CI integration (P1A) */
typedef enum {
    OUTPUT_HUMAN,      /* default: colored terminal */
    OUTPUT_JSON,       /* JSON lines — one object per diagnostic */
    OUTPUT_SARIF       /* SARIF v2.1.0 for GitHub Code Scanning */
} OutputFormat;

/* Print all diagnostics in specified format. */
void diag_print_format(const DiagList *dl, OutputFormat fmt, FILE *out);

/* Print diagnostics as JSON lines to file handle. */
void diag_print_json(const DiagList *dl, FILE *out);

/* Print diagnostics as SARIF v2.1.0 to file handle. */
void diag_print_sarif(const DiagList *dl, FILE *out);

#endif /* SUHC_DIAGNOSTIC_H */
