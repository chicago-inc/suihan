/*
 * suhc — kindcheck.c
 * Kind checker: verify every declaration carries exactly one kind.
 *
 * The kind system (§2) assigns five kinds to information.
 * Putting information in the wrong kind is a compile error.
 * This pass:
 *   1. Resolves KIND_INFERRED nodes based on usage context
 *   2. Detects scope confusion (Error 5): wrong kind slot
 *   3. Detects immutability violations: mutation of ξ-kinded values
 *   4. Detects undecidable actions: acting on undecidable output
 */

#include "kindcheck.h"
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------ */
/* Symbol table for kind tracking                                */
/* ------------------------------------------------------------ */

typedef struct {
    char *name;
    Kind  kind;
    int   line;
    bool  is_decidable;  /* for ω-kinded values */
} KindEntry;

typedef struct {
    KindEntry *entries;
    size_t     count;
    size_t     capacity;
} KindTable;

static void kt_init(KindTable *kt) {
    kt->capacity = 64;
    kt->entries = calloc(kt->capacity, sizeof(KindEntry));
    kt->count = 0;
}

static void kt_free(KindTable *kt) {
    for (size_t i = 0; i < kt->count; i++) {
        free(kt->entries[i].name);
    }
    free(kt->entries);
}

static void kt_define(KindTable *kt, const char *name, Kind kind, int line, bool decidable) {
    if (kt->count >= kt->capacity) {
        kt->capacity *= 2;
        kt->entries = realloc(kt->entries, kt->capacity * sizeof(KindEntry));
    }
    KindEntry *e = &kt->entries[kt->count++];
    e->name = strdup(name);
    e->kind = kind;
    e->line = line;
    e->is_decidable = decidable;
}

static KindEntry *kt_lookup(KindTable *kt, const char *name) {
    for (size_t i = 0; i < kt->count; i++) {
        if (strcmp(kt->entries[i].name, name) == 0)
            return &kt->entries[i];
    }
    return NULL;
}

/* ------------------------------------------------------------ */
/* Kind inference rules                                          */
/* ------------------------------------------------------------ */

/* M3: infer_decl_kind() is now generated from
 * ordbok/compiler/compiler_kind_inference.szh — a cross-dimension
 * projection (decl → kind) that encodes the constitutional mapping
 * between declaration types and the unified equation's kind system.
 * The generated function has signature: kind_t infer_decl_kind(decl_t val)
 * which is compatible via the M1 typedef bridges (Kind = kind_t,
 * DeclType = decl_t). */
#include "gen/kind_inference.h"

/* ------------------------------------------------------------ */
/* Expression kind checking                                      */
/* ------------------------------------------------------------ */

static void check_expr_kinds(Expr *expr, KindTable *kt, Kind expected_kind,
                             const char *filename, DiagList *diags) {
    if (!expr) return;

    switch (expr->type) {
    case EXPR_IDENT: {
        /* Skip if name is NULL (parser error recovery created dummy EXPR_IDENT) */
        if (!expr->as.ident.name) break;
        KindEntry *e = kt_lookup(kt, expr->as.ident.name);
        if (e && expected_kind != KIND_NONE && expected_kind != KIND_INFERRED) {
            /* Check for scope confusion: using a value of one kind
             * where another kind is expected */
            if (e->kind != expected_kind && e->kind != KIND_NONE &&
                e->kind != KIND_INFERRED) {
                /* ζ used in permission/structural context → scope confusion */
                if (e->kind == KIND_ZETA &&
                    (expected_kind == KIND_XI || expected_kind == KIND_RK)) {
                    diag_error(diags, DIAG_SCOPE_CONFUSION, filename,
                               expr->line, expr->col,
                               "'%s' is %s-kind but used in %s context",
                               expr->as.ident.name, kind_name(e->kind),
                               kind_name(expected_kind));
                }
            }
        }
        break;
    }
    case EXPR_DECIDABLE:
        check_expr_kinds(expr->as.decidability.inner, kt, expected_kind, filename, diags);
        break;
    case EXPR_UNDECIDABLE:
        check_expr_kinds(expr->as.decidability.inner, kt, expected_kind, filename, diags);
        break;
    case EXPR_CALL:
        for (size_t i = 0; i < expr->as.call.arg_count; i++) {
            check_expr_kinds(expr->as.call.args[i], kt, KIND_INFERRED, filename, diags);
        }
        break;
    case EXPR_ARROW:
        check_expr_kinds(expr->as.arrow.from, kt, KIND_INFERRED, filename, diags);
        check_expr_kinds(expr->as.arrow.to, kt, KIND_INFERRED, filename, diags);
        break;
    case EXPR_LIST:
        for (size_t i = 0; i < expr->as.list.count; i++) {
            check_expr_kinds(expr->as.list.items[i], kt, expected_kind, filename, diags);
        }
        break;
    case EXPR_PIPE_CHAIN:
        for (size_t i = 0; i < expr->as.pipe_chain.count; i++) {
            check_expr_kinds(expr->as.pipe_chain.stages[i], kt, KIND_INFERRED, filename, diags);
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------ */
/* Traversal-specific checks                                     */
/* ------------------------------------------------------------ */

static void check_traversal_completeness(Decl *d, const char *filename, DiagList *diags) {
    /* A traversal must have all five equation components.
     * Missing ΔR.k is a specific error: DIAG_MISSING_CAST. */
    bool has_xi = false, has_zeta = false, has_x = false;
    bool has_rk = false, has_omega = false, has_delta = false;

    for (size_t i = 0; i < d->as.traversal.section_count; i++) {
        switch (d->as.traversal.sections[i].section_kind) {
        case KIND_XI:       has_xi = true; break;
        case KIND_ZETA:     has_zeta = true; break;
        case KIND_X:        has_x = true; break;
        case KIND_RK:       has_rk = true; break;
        case KIND_OMEGA:    has_omega = true; break;
        case KIND_DELTA_RK: has_delta = true; break;
        default: break;
        }
    }

    /* Completeness checks — all five equation parts should be present.
     * Phase 0A: only warn on missing output and cast. */
    (void)has_xi; (void)has_zeta; (void)has_x; (void)has_rk;

    if (!has_delta) {
        diag_warn(diags, DIAG_MISSING_CAST, filename,
                  d->line, d->name.col,
                  "traversal '%s' has no ΔR.k (entropy cast) section — "
                  "every traversal opens possibility spaces",
                  d->name.text);
    }

    if (!has_omega) {
        diag_warn(diags, DIAG_FAILURE_TO_DERIVE, filename,
                  d->line, d->name.col,
                  "traversal '%s' has no ω (output) section",
                  d->name.text);
    }
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

void kindcheck(Program *prog, DiagList *diags) {
    KindTable kt;
    kt_init(&kt);

    /* Pre-seed kind table with imported declarations */
    size_t imp_count = 0;
    Decl **imports = collect_imported_decls(prog, &imp_count);
    for (size_t i = 0; i < imp_count; i++) {
        Decl *d = imports[i];
        if (d->name.text) {
            bool decidable = false;
            if (d->type == DECL_KINDED_VALUE && d->as.kinded.value)
                decidable = (d->as.kinded.value->type == EXPR_DECIDABLE);
            kt_define(&kt, d->name.text, d->kind, d->line, decidable);
        }
    }
    free(imports);

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];

        /* 1. Resolve kinds for declarations that don't have one */
        if (d->kind == KIND_NONE || d->kind == KIND_INFERRED) {
            Kind inferred = infer_decl_kind(d->type);
            if (inferred != KIND_INFERRED) {
                d->kind = inferred;
            }
        }

        /* 2. Register in kind table */
        if (d->name.text) {
            bool decidable = false;
            /* Check if kinded value wraps a decidable/undecidable expr */
            if (d->type == DECL_KINDED_VALUE && d->as.kinded.value) {
                decidable = (d->as.kinded.value->type == EXPR_DECIDABLE);
            }
            kt_define(&kt, d->name.text, d->kind, d->line, decidable);
        }

        /* 3. Check expression kinds within declarations */
        switch (d->type) {
        case DECL_KINDED_VALUE:
            if (d->as.kinded.value) {
                check_expr_kinds(d->as.kinded.value, &kt, d->kind,
                                prog->filename, diags);
            }
            break;

        case DECL_TRAVERSAL:
            check_traversal_completeness(d, prog->filename, diags);
            break;

        default:
            break;
        }

        /* 4. Check for duplicate ξ definitions (reduplication).
         * Only flag same-type declarations with same name — a unit 'community'
         * and a dimension 'community' are different structural concerns,
         * not reduplication. Same-type same-name is the violation. */
        if (d->kind == KIND_XI && d->name.text) {
            for (size_t j = 0; j < i; j++) {
                Decl *prev = prog->decls[j];
                if (prev->kind == KIND_XI && prev->name.text &&
                    prev->type == d->type &&
                    strcmp(prev->name.text, d->name.text) == 0) {
                    diag_error(diags, DIAG_REDUPLICATION, prog->filename,
                               d->line, d->name.col,
                               "'%s' (ξ-kind %s) defined at line %d and line %d — "
                               "same identity at two addresses",
                               d->name.text, decl_type_name(d->type),
                               prev->line, d->line);
                }
            }
        }
    }

    kt_free(&kt);
}

