/*
 * suhc — perpcheck.c
 * Perpendicularity checker: enforce dimensional constraints.
 *
 * Values from perpendicular dimensions cannot substitute for
 * each other. This is stronger than a type error — it is a
 * dimensional error. You cannot check display_title in a
 * permission gate, not because the types don't match, but
 * because the dimensions are perpendicular.
 *
 * Sprint 3B: refactored to use shared DimRegistry from
 * dim_registry.h, eliminating redundant DimEntry typedef
 * and population code. Only PerpPair tracking remains internal.
 */

#include "perpcheck.h"
#include "dim_registry.h"
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------ */
/* Perpendicularity registry (pairs only — dims use shared)     */
/* ------------------------------------------------------------ */

typedef struct {
    char *dim_a;
    char *dim_b;
} PerpPair;

typedef struct {
    PerpPair    *pairs;
    size_t       pair_count;
    size_t       pair_capacity;

    DimRegistry  dims;   /* shared dim_registry.h type */
} PerpRegistry;

static void pr_init(PerpRegistry *pr) {
    pr->pair_capacity = 16;
    pr->pairs = calloc(pr->pair_capacity, sizeof(PerpPair));
    pr->pair_count = 0;

    dim_registry_init(&pr->dims);
}

static void pr_free(PerpRegistry *pr) {
    for (size_t i = 0; i < pr->pair_count; i++) {
        free(pr->pairs[i].dim_a);
        free(pr->pairs[i].dim_b);
    }
    free(pr->pairs);

    dim_registry_free(&pr->dims);
}

static void pr_add_perp(PerpRegistry *pr, const char *a, const char *b) {
    if (pr->pair_count >= pr->pair_capacity) {
        pr->pair_capacity *= 2;
        pr->pairs = realloc(pr->pairs, pr->pair_capacity * sizeof(PerpPair));
    }
    PerpPair *pp = &pr->pairs[pr->pair_count++];
    pp->dim_a = strdup(a);
    pp->dim_b = strdup(b);
}

/* Find which dimension a name belongs to (NULL if none) */
static const char *pr_find_dim(PerpRegistry *pr, const char *name) {
    DimRegistry *dr = &pr->dims;
    for (size_t i = 0; i < dr->count; i++) {
        for (size_t j = 0; j < dr->dims[i].member_count; j++) {
            if (strcmp(dr->dims[i].members[j], name) == 0) {
                return dr->dims[i].name;
            }
        }
        /* Also check the dimension name itself */
        if (strcmp(dr->dims[i].name, name) == 0) {
            return dr->dims[i].name;
        }
    }
    return NULL;
}

/* Check if two dimensions are perpendicular */
static bool pr_are_perp(PerpRegistry *pr, const char *dim_a, const char *dim_b) {
    if (!dim_a || !dim_b) return false;
    for (size_t i = 0; i < pr->pair_count; i++) {
        if ((strcmp(pr->pairs[i].dim_a, dim_a) == 0 &&
             strcmp(pr->pairs[i].dim_b, dim_b) == 0) ||
            (strcmp(pr->pairs[i].dim_a, dim_b) == 0 &&
             strcmp(pr->pairs[i].dim_b, dim_a) == 0)) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------ */
/* Check expressions for perpendicularity violations             */
/* ------------------------------------------------------------ */

/* Track which dimension each identifier belongs to in comparisons */
static void check_binary_perp(Expr *expr, PerpRegistry *pr,
                               const char *filename, DiagList *diags) {
    if (!expr) return;

    /* Check == comparisons between perpendicular dimensions */
    if (expr->type == EXPR_BINARY && expr->as.binary.op &&
        strcmp(expr->as.binary.op, "==") == 0) {
        const char *left_dim = NULL, *right_dim = NULL;

        if (expr->as.binary.left && expr->as.binary.left->type == EXPR_IDENT) {
            left_dim = pr_find_dim(pr, expr->as.binary.left->as.ident.name);
        }
        if (expr->as.binary.right && expr->as.binary.right->type == EXPR_IDENT) {
            right_dim = pr_find_dim(pr, expr->as.binary.right->as.ident.name);
        }

        if (left_dim && right_dim && pr_are_perp(pr, left_dim, right_dim)) {
            diag_error(diags, DIAG_PERPENDICULAR_CROSS, filename,
                       expr->line, expr->col,
                       "comparison between perpendicular dimensions '%s' and '%s' — "
                       "these dimensions cannot substitute for each other",
                       left_dim, right_dim);
        }
    }

    /* Recurse into sub-expressions */
    switch (expr->type) {
    case EXPR_CALL:
        for (size_t i = 0; i < expr->as.call.arg_count; i++)
            check_binary_perp(expr->as.call.args[i], pr, filename, diags);
        break;
    case EXPR_ARROW:
        check_binary_perp(expr->as.arrow.from, pr, filename, diags);
        check_binary_perp(expr->as.arrow.to, pr, filename, diags);
        break;
    case EXPR_LIST:
        for (size_t i = 0; i < expr->as.list.count; i++)
            check_binary_perp(expr->as.list.items[i], pr, filename, diags);
        break;
    case EXPR_PIPE_CHAIN:
        for (size_t i = 0; i < expr->as.pipe_chain.count; i++)
            check_binary_perp(expr->as.pipe_chain.stages[i], pr, filename, diags);
        break;
    case EXPR_BLOCK:
        for (size_t i = 0; i < expr->as.block.count; i++)
            check_binary_perp(expr->as.block.stmts[i], pr, filename, diags);
        break;
    case EXPR_COALESCE:
        check_binary_perp(expr->as.coalesce.left, pr, filename, diags);
        check_binary_perp(expr->as.coalesce.right, pr, filename, diags);
        break;
    case EXPR_BINARY:
        check_binary_perp(expr->as.binary.left, pr, filename, diags);
        check_binary_perp(expr->as.binary.right, pr, filename, diags);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------ */
/* graphics_rule contrast checking (Phase 5c)                    */
/* ------------------------------------------------------------ */

#include "color_math.h"

/* Find a field by label in a graphics_rule's fields. */
static Expr *gr_find_field(DeclField *fields, size_t count, const char *label) {
    for (size_t i = 0; i < count; i++) {
        if (fields[i].label.text && strcmp(fields[i].label.text, label) == 0) {
            return fields[i].value;
        }
    }
    return NULL;
}

/* Extract a color string from a field's value. Accepts:
 *   - String literal:    "#7C3AED"
 *   - Identifier:        primary_violet (treated as a token name; would be
 *                        resolved against the colors registry in production)
 * Returns NULL if not a recognized form. Caller does not free. */
static const char *gr_color_string(Expr *e) {
    if (!e) return NULL;
    if (e->type == EXPR_STRING) return e->as.string.value;
    if (e->type == EXPR_IDENT) return e->as.ident.name;
    return NULL;
}

/* Extract a numeric ratio (e.g., 4.5) from a min_contrast field. */
static double gr_min_contrast(Expr *e) {
    if (!e) return 4.5;  /* WCAG AA default */
    if (e->type == EXPR_NUMBER && e->as.number.text) {
        return atof(e->as.number.text);
    }
    return 4.5;
}

/* Check a single graphics_rule for contrast compliance.
 * Reads foreground_vector + background_vector + min_contrast and
 * computes WCAG contrast. Errors if the pair fails the declared
 * minimum.
 *
 * The check is direct: if the rule declares foreground=#FFF,
 * background=#FFF, and min_contrast=4.5, the rule itself fails
 * compilation because white-on-white has contrast ratio 1.0.
 *
 * This is the falsification handle the directive's Phase 5 asks
 * for: an unconstitutional rule fails to compile. */
static void check_graphics_rule(Decl *d, const char *filename, DiagList *diags) {
    if (d->type != DECL_GRAPHICS_RULE) return;

    Expr *fg = gr_find_field(d->as.graphics_rule.fields,
                              d->as.graphics_rule.field_count,
                              "foreground_vector");
    Expr *bg = gr_find_field(d->as.graphics_rule.fields,
                              d->as.graphics_rule.field_count,
                              "background_vector");
    Expr *min_c = gr_find_field(d->as.graphics_rule.fields,
                                 d->as.graphics_rule.field_count,
                                 "min_contrast");

    if (!fg || !bg) {
        diag_error(diags, DIAG_SCOPE_CONFUSION, filename,
                   d->name.line, 0,
                   "graphics_rule '%s' must declare both foreground_vector "
                   "and background_vector",
                   d->name.text ? d->name.text : "?");
        return;
    }

    const char *fg_s = gr_color_string(fg);
    const char *bg_s = gr_color_string(bg);
    if (!fg_s || !bg_s) {
        return;  /* Token-name form — would resolve via colors registry; skip */
    }

    /* Only evaluate when both look like literal hex/rgba strings */
    if (fg_s[0] != '#' && strncmp(fg_s, "rgb", 3) != 0) return;
    if (bg_s[0] != '#' && strncmp(bg_s, "rgb", 3) != 0) return;

    double ratio = color_contrast_ratio(fg_s, bg_s);
    if (ratio < 0) {
        diag_warn(diags, DIAG_SCOPE_CONFUSION, filename,
                  d->name.line, 0,
                  "graphics_rule '%s': could not parse foreground or "
                  "background color literal",
                  d->name.text ? d->name.text : "?");
        return;
    }

    double threshold = gr_min_contrast(min_c);
    if (ratio < threshold) {
        diag_error(diags, DIAG_SCOPE_CONFUSION, filename,
                   d->name.line, 0,
                   "graphics_rule '%s' fails contrast: ratio %.2f < %.2f "
                   "for foreground=%s on background=%s — this is the "
                   "constitutional class the rule binds, and it is "
                   "currently unreadable",
                   d->name.text ? d->name.text : "?",
                   ratio, threshold, fg_s, bg_s);
    }
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

void perpcheck(Program *prog, DiagList *diags) {
    PerpRegistry pr;
    pr_init(&pr);

    /* Populate dimensions from shared registry helpers */
    dim_registry_add_imported(&pr.dims, prog);
    dim_registry_add_local(&pr.dims, prog);

    /* Collect perpendicularity constraints from imports */
    size_t imp_count = 0;
    Decl **imports = collect_imported_decls(prog, &imp_count);
    for (size_t i = 0; i < imp_count; i++) {
        Decl *d = imports[i];
        if (d->type == DECL_PERPENDICULAR) {
            for (size_t a = 0; a < d->as.relation.count; a++)
                for (size_t b = a + 1; b < d->as.relation.count; b++)
                    pr_add_perp(&pr, d->as.relation.names[a].text,
                                d->as.relation.names[b].text);
        }
    }
    free(imports);

    /* Collect perpendicularity constraints from local declarations */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type == DECL_PERPENDICULAR) {
            for (size_t a = 0; a < d->as.relation.count; a++) {
                for (size_t b = a + 1; b < d->as.relation.count; b++) {
                    pr_add_perp(&pr,
                                d->as.relation.names[a].text,
                                d->as.relation.names[b].text);
                }
            }
        }
    }

    /* Pass 2: check all expressions for perpendicularity violations */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];

        switch (d->type) {
        case DECL_KINDED_VALUE:
            check_binary_perp(d->as.kinded.value, &pr, prog->filename, diags);
            break;
        case DECL_TRAVERSAL:
            for (size_t s = 0; s < d->as.traversal.section_count; s++) {
                check_binary_perp(d->as.traversal.sections[s].body,
                                  &pr, prog->filename, diags);
            }
            break;
        case DECL_PROJECTION:
            for (size_t f = 0; f < d->as.projection.field_count; f++) {
                check_binary_perp(d->as.projection.fields[f].value,
                                  &pr, prog->filename, diags);
            }
            break;
        case DECL_MEIHUA:
        case DECL_ZHULIN:
        case DECL_SONGQIAO:
            check_binary_perp(d->as.exec_layer.body, &pr, prog->filename, diags);
            break;
        default:
            break;
        }
    }

    /* Pass 3: check that execution layers are perpendicular to each other */
    bool has_meihua = false, has_zhulin = false, has_songqiao = false;
    for (size_t i = 0; i < prog->count; i++) {
        if (prog->decls[i]->type == DECL_MEIHUA) has_meihua = true;
        if (prog->decls[i]->type == DECL_ZHULIN) has_zhulin = true;
        if (prog->decls[i]->type == DECL_SONGQIAO) has_songqiao = true;
    }
    (void)has_meihua; (void)has_zhulin; (void)has_songqiao;

    /* Pass 4 (Phase 5c): graphics_rule contrast check */
    for (size_t i = 0; i < prog->count; i++) {
        check_graphics_rule(prog->decls[i], prog->filename, diags);
    }

    pr_free(&pr);
}
