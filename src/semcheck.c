/*
 * suhc — semcheck.c
 * Cross-file semantic validation.
 *
 * Sprint 5A: For each projection, check if its invariant/context
 * field names match a declared dimension. If so, validate that
 * all case arm values are members of that dimension.
 *
 * This is structural validation, not type inference. The ordbok's
 * own dimension declarations are the type source.
 *
 * Constitutional grounding:
 *   Identity bridging: disconnected identity pipelines modeling
 *   the same entity must share observations. A dimension declared
 *   in foundational.szh used in permissions.szh must be validated
 *   at the boundary.
 *   D13: S = 1 - (resolved / total). This measures and reduces S.
 */

#include "semcheck.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------ */
/* Dimension lookup (same logic as emit_ts typed emission)       */
/* ------------------------------------------------------------ */

/* Find a dimension matching a field name with singular/plural normalization */
static Decl *find_dim(const char *field_name, Program *prog) {
    if (!field_name || !prog) return NULL;
    size_t flen = strlen(field_name);

    char singular[256], plural[256];
    snprintf(singular, sizeof(singular), "%s", field_name);
    snprintf(plural, sizeof(plural), "%ss", field_name);
    if (flen > 1 && field_name[flen - 1] == 's') {
        snprintf(singular, sizeof(singular), "%.*s", (int)(flen - 1), field_name);
    }

    /* Search local */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_DIMENSION || !d->name.text) continue;
        if (strcmp(d->name.text, field_name) == 0 ||
            strcmp(d->name.text, singular) == 0 ||
            strcmp(d->name.text, plural) == 0)
            return d;
    }

    /* Search imports */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_IMPORT || !d->as.import_decl.resolved) continue;
        Program *imp = d->as.import_decl.resolved;
        for (size_t j = 0; j < imp->count; j++) {
            Decl *id = imp->decls[j];
            if (id->type != DECL_DIMENSION || !id->name.text) continue;
            if (strcmp(id->name.text, field_name) == 0 ||
                strcmp(id->name.text, singular) == 0 ||
                strcmp(id->name.text, plural) == 0)
                return id;
        }
    }

    return NULL;
}

/* Check if a value is a member of a dimension's enum */
static bool is_member(Decl *dim, const char *value) {
    if (!dim || !value || dim->type != DECL_DIMENSION) return false;
    Expr *members = dim->as.dimension.members;
    if (!members || members->type != EXPR_ENUM) return false;

    for (size_t i = 0; i < members->as.enumeration.count; i++) {
        if (members->as.enumeration.items[i].name.text &&
            strcmp(members->as.enumeration.items[i].name.text, value) == 0)
            return true;
    }
    return false;
}

/* ------------------------------------------------------------ */
/* Extract field name from a projection's DeclField              */
/* ------------------------------------------------------------ */

static const char *get_field_name(DeclField *fields, size_t count,
                                   const char *label) {
    for (size_t i = 0; i < count; i++) {
        if (fields[i].label.text && strcmp(fields[i].label.text, label) == 0) {
            Expr *v = fields[i].value;
            if (v && v->type == EXPR_IDENT && v->as.ident.name)
                return v->as.ident.name;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------ */
/* Extract pattern values from a projection arm                  */
/* ------------------------------------------------------------ */

static void extract_pattern_values(Expr *pattern,
                                    char ***out, size_t *out_count) {
    *out = NULL;
    *out_count = 0;
    if (!pattern) return;

    if (pattern->type == EXPR_LIST) {
        *out_count = pattern->as.list.count;
        *out = malloc(pattern->as.list.count * sizeof(char *));
        for (size_t i = 0; i < pattern->as.list.count; i++) {
            Expr *item = pattern->as.list.items[i];
            if (item->type == EXPR_IDENT && item->as.ident.name)
                (*out)[i] = item->as.ident.name;
            else if (item->type == EXPR_WILDCARD)
                (*out)[i] = "_";
            else if (item->type == EXPR_STRING && item->as.string.value)
                (*out)[i] = item->as.string.value;
            else
                (*out)[i] = "_";
        }
    } else if (pattern->type == EXPR_IDENT && pattern->as.ident.name) {
        *out_count = 1;
        *out = malloc(sizeof(char *));
        (*out)[0] = pattern->as.ident.name;
    } else if (pattern->type == EXPR_WILDCARD) {
        *out_count = 1;
        *out = malloc(sizeof(char *));
        (*out)[0] = "_";
    }
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

SemcheckReport semcheck(Program *prog, DiagList *diags) {
    SemcheckReport report = {0, 0, 0, 0};

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_PROJECTION) continue;

        report.projections_checked++;

        const char *inv_name = get_field_name(
            d->as.projection.fields, d->as.projection.field_count, "invariant");
        const char *ctx_name = get_field_name(
            d->as.projection.fields, d->as.projection.field_count, "context");

        Decl *inv_dim = inv_name ? find_dim(inv_name, prog) : NULL;
        Decl *ctx_dim = ctx_name ? find_dim(ctx_name, prog) : NULL;

        if (inv_dim) report.dimensions_resolved++;
        if (ctx_dim) report.dimensions_resolved++;

        /* Check if dimensions were resolved from imports (cross-file) */
        bool inv_imported = false, ctx_imported = false;
        for (size_t j = 0; j < prog->count; j++) {
            if (prog->decls[j]->type != DECL_IMPORT) continue;
            Program *imp = prog->decls[j]->as.import_decl.resolved;
            if (!imp) continue;
            for (size_t k = 0; k < imp->count; k++) {
                if (imp->decls[k] == inv_dim) inv_imported = true;
                if (imp->decls[k] == ctx_dim) ctx_imported = true;
            }
        }
        if (inv_imported || ctx_imported) report.cross_file_validations++;

        /* Validate case arm values against resolved dimensions */
        for (size_t a = 0; a < d->as.projection.arm_count; a++) {
            ProjectionArm *arm = &d->as.projection.arms[a];
            char **pats = NULL;
            size_t n_pats = 0;
            extract_pattern_values(arm->pattern, &pats, &n_pats);

            /* Check invariant value (first pattern) */
            if (n_pats >= 1 && inv_dim && strcmp(pats[0], "_") != 0) {
                if (!is_member(inv_dim, pats[0])) {
                    diag_note(diags, DIAG_SCOPE_CONFUSION,
                              prog->filename,
                              arm->pattern ? arm->pattern->line : d->line,
                              arm->pattern ? arm->pattern->col : 0,
                              "projection '%s': value '%s' is not a member of dimension '%s'",
                              d->name.text, pats[0],
                              inv_dim->name.text);
                    report.dimension_mismatches++;
                }
            }

            /* Check context value (second pattern) */
            if (n_pats >= 2 && ctx_dim && strcmp(pats[1], "_") != 0) {
                if (!is_member(ctx_dim, pats[1])) {
                    diag_note(diags, DIAG_SCOPE_CONFUSION,
                              prog->filename,
                              arm->pattern ? arm->pattern->line : d->line,
                              arm->pattern ? arm->pattern->col : 0,
                              "projection '%s': value '%s' is not a member of dimension '%s'",
                              d->name.text, pats[1],
                              ctx_dim->name.text);
                    report.dimension_mismatches++;
                }
            }

            free(pats);
        }
    }

    return report;
}
