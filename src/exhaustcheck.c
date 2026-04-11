/*
 * suhc — exhaustcheck.c
 * Exhaustiveness checker and meihua body validator.
 *
 * Derived from D13 (Implicit Solipsism): the compiler's S against
 * projection-dimension relationships. This pass reduces S to 0
 * for all projections whose invariants are declared dimensions.
 *
 * Two sub-passes:
 *   1. Projection exhaustiveness — verify case arm coverage
 *      against dimension members.
 *   2. Meihua validation — verify function calls are in the
 *      known-function whitelist and identifiers are parameters.
 */

#include "exhaustcheck.h"
#include "dim_registry.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------ */
/* New diagnostic categories for exhaustiveness                  */
/* We reuse existing categories where appropriate:               */
/*   DIAG_SCOPE_CONFUSION for missing arms (wrong slot)          */
/*   DIAG_YONEDA_GAP for missing default (unobserved morphism)   */
/*   DIAG_NICHE_PIPE for unknown meihua functions                */
/* ------------------------------------------------------------ */

/* ------------------------------------------------------------ */
/* Projection exhaustiveness                                     */
/* ------------------------------------------------------------ */

/* Extract the invariant field name from a projection's fields */
static const char *get_invariant_name(Decl *d) {
    for (size_t i = 0; i < d->as.projection.field_count; i++) {
        DeclField *f = &d->as.projection.fields[i];
        if (f->label.text && strcmp(f->label.text, "invariant") == 0) {
            if (f->value && f->value->type == EXPR_IDENT)
                return f->value->as.ident.name;
        }
    }
    return NULL;
}

/* Check if a projection has a wildcard default arm.
 * A wildcard is (_, _) or (_, <anything>) in the first position. */
static bool has_wildcard_arm(Decl *d) {
    for (size_t a = 0; a < d->as.projection.arm_count; a++) {
        ProjectionArm *arm = &d->as.projection.arms[a];
        if (!arm->pattern) continue;

        /* Check for direct wildcard */
        if (arm->pattern->type == EXPR_WILDCARD) return true;

        /* Check for tuple with wildcard in first position */
        if (arm->pattern->type == EXPR_LIST && arm->pattern->as.list.count >= 1) {
            Expr *first = arm->pattern->as.list.items[0];
            if (first && first->type == EXPR_WILDCARD) return true;
        }
    }
    return false;
}

/* Collect all invariant values (first pattern position) that appear
 * in the projection's case arms. Returns count. */
static size_t collect_covered_members(Decl *d, char **out, size_t max) {
    size_t count = 0;
    for (size_t a = 0; a < d->as.projection.arm_count && count < max; a++) {
        ProjectionArm *arm = &d->as.projection.arms[a];
        if (!arm->pattern) continue;

        const char *member = NULL;

        if (arm->pattern->type == EXPR_IDENT && arm->pattern->as.ident.name) {
            member = arm->pattern->as.ident.name;
        }
        else if (arm->pattern->type == EXPR_LIST && arm->pattern->as.list.count >= 1) {
            Expr *first = arm->pattern->as.list.items[0];
            if (first && first->type == EXPR_IDENT && first->as.ident.name) {
                member = first->as.ident.name;
            }
        }

        if (!member) continue;

        /* Deduplicate */
        bool already = false;
        for (size_t i = 0; i < count; i++) {
            if (strcmp(out[i], member) == 0) { already = true; break; }
        }
        if (!already) {
            out[count++] = strdup(member);
        }
    }
    return count;
}

static void check_projection_exhaustiveness(Decl *d, DimRegistry *dr,
                                             const char *filename,
                                             DiagList *diags,
                                             ExhaustReport *report) {
    if (d->type != DECL_PROJECTION) return;
    if (!d->name.text) return;

    const char *invariant_name = get_invariant_name(d);
    if (!invariant_name) return;

    /* Look up the dimension */
    DimEntry *dim = dim_registry_find(dr, invariant_name);
    if (!dim || dim->member_count == 0) return;

    /* This projection has a resolvable dimension — check it */
    report->projections_checked++;

    bool has_default = has_wildcard_arm(d);

    /* Check default completeness */
    if (!has_default) {
        report->projections_no_default++;
        diag_warn(diags, DIAG_YONEDA_GAP, filename, d->line, 0,
                  "projection '%s' has no wildcard default arm (_, _) — "
                  "unmatched inputs will produce NULL/undefined",
                  d->name.text);
    }

    /* Collect which members are covered */
    char *covered[256];
    size_t covered_count = collect_covered_members(d, covered, 256);

    /* Check each dimension member */
    bool all_covered = true;
    for (size_t m = 0; m < dim->member_count; m++) {
        const char *member = dim->members[m];
        bool found = false;
        for (size_t c = 0; c < covered_count; c++) {
            if (strcmp(covered[c], member) == 0) { found = true; break; }
        }
        if (!found) {
            all_covered = false;
            if (has_default) {
                diag_warn(diags, DIAG_SCOPE_CONFUSION, filename, d->line, 0,
                          "projection '%s' missing explicit case for "
                          "dimension member '%s' of '%s' "
                          "(wildcard default will match)",
                          d->name.text, member, dim->name);
            } else {
                diag_error(diags, DIAG_SCOPE_CONFUSION, filename, d->line, 0,
                           "projection '%s' has no case for dimension member "
                           "'%s' of '%s' and no default arm",
                           d->name.text, member, dim->name);
            }
        }
    }

    if (all_covered) {
        report->projections_exhaustive++;
    } else if (has_default) {
        report->projections_defaulted++;
    } else {
        report->projections_incomplete++;
    }

    /* Cleanup */
    for (size_t i = 0; i < covered_count; i++) free(covered[i]);
}

/* ------------------------------------------------------------ */
/* Cross-product exhaustiveness (Sprint 3B)                      */
/* Two-axis checking: when both invariant AND context are        */
/* declared dimensions, verify all (inv, ctx) pairs are covered. */
/* ------------------------------------------------------------ */

/* Extract the context field name from a projection's fields */
static const char *get_context_name(Decl *d) {
    for (size_t i = 0; i < d->as.projection.field_count; i++) {
        DeclField *f = &d->as.projection.fields[i];
        if (f->label.text && strcmp(f->label.text, "context") == 0) {
            if (f->value && f->value->type == EXPR_IDENT)
                return f->value->as.ident.name;
        }
    }
    return NULL;
}

/* Collect (invariant, context) pairs from case arms.
 * Returns count. Each pair stored as two adjacent strings in out. */
static size_t collect_arm_pairs(Decl *d, char **inv_out, char **ctx_out, size_t max) {
    size_t count = 0;
    for (size_t a = 0; a < d->as.projection.arm_count && count < max; a++) {
        ProjectionArm *arm = &d->as.projection.arms[a];
        if (!arm->pattern) continue;

        if (arm->pattern->type == EXPR_LIST && arm->pattern->as.list.count >= 2) {
            Expr *first = arm->pattern->as.list.items[0];
            Expr *second = arm->pattern->as.list.items[1];

            const char *inv_name = NULL;
            const char *ctx_name = NULL;
            bool inv_wild = false;
            bool ctx_wild = false;

            if (first && first->type == EXPR_IDENT) inv_name = first->as.ident.name;
            if (first && first->type == EXPR_WILDCARD) inv_wild = true;
            if (second && second->type == EXPR_IDENT) ctx_name = second->as.ident.name;
            if (second && second->type == EXPR_WILDCARD) ctx_wild = true;

            if (inv_name && ctx_name) {
                /* Concrete pair */
                inv_out[count] = strdup(inv_name);
                ctx_out[count] = strdup(ctx_name);
                count++;
            } else if (inv_name && ctx_wild) {
                /* Partial wildcard (inv, _) — mark with special sentinel */
                inv_out[count] = strdup(inv_name);
                ctx_out[count] = strdup("_");
                count++;
            } else if (inv_wild && ctx_name) {
                /* Partial wildcard (_, ctx) */
                inv_out[count] = strdup("_");
                ctx_out[count] = strdup(ctx_name);
                count++;
            } else if (inv_wild && ctx_wild) {
                /* Full wildcard (_, _) */
                inv_out[count] = strdup("_");
                ctx_out[count] = strdup("_");
                count++;
            }
        }
    }
    return count;
}

/* Coverage level for a (inv, ctx) pair:
 *   0 = not covered at all
 *   1 = covered only by full wildcard (_, _)
 *   2 = covered by partial wildcard (inv, _) or (_, ctx)
 *   3 = covered by exact match (inv, ctx) */
static int pair_coverage_level(const char *inv, const char *ctx,
                                char **inv_arms, char **ctx_arms, size_t arm_count) {
    int best = 0;
    for (size_t i = 0; i < arm_count; i++) {
        /* Exact match */
        if (strcmp(inv_arms[i], inv) == 0 && strcmp(ctx_arms[i], ctx) == 0)
            return 3;
        /* Partial wildcard (inv, _) covers (inv, anything) */
        if (strcmp(inv_arms[i], inv) == 0 && strcmp(ctx_arms[i], "_") == 0) {
            if (best < 2) best = 2;
        }
        /* Partial wildcard (_, ctx) covers (anything, ctx) */
        if (strcmp(inv_arms[i], "_") == 0 && strcmp(ctx_arms[i], ctx) == 0) {
            if (best < 2) best = 2;
        }
        /* Full wildcard (_, _) covers everything */
        if (strcmp(inv_arms[i], "_") == 0 && strcmp(ctx_arms[i], "_") == 0) {
            if (best < 1) best = 1;
        }
    }
    return best;
}


static void check_cross_product(Decl *d, DimRegistry *dr,
                                 const char *filename,
                                 DiagList *diags,
                                 ExhaustReport *report) {
    if (d->type != DECL_PROJECTION) return;
    if (!d->name.text) return;

    const char *invariant_name = get_invariant_name(d);
    const char *context_name = get_context_name(d);
    if (!invariant_name || !context_name) return;

    /* Both must be declared dimensions */
    DimEntry *inv_dim = dim_registry_find(dr, invariant_name);
    DimEntry *ctx_dim = dim_registry_find(dr, context_name);
    if (!inv_dim || inv_dim->member_count == 0) return;
    if (!ctx_dim || ctx_dim->member_count == 0) return;

    report->cross_product_checked++;

    /* Collect arm pairs */
    char *inv_arms[512], *ctx_arms[512];
    size_t arm_count = collect_arm_pairs(d, inv_arms, ctx_arms, 512);

    bool all_covered = true;

    /* Check each cross-product pair */
    for (size_t i = 0; i < inv_dim->member_count; i++) {
        for (size_t j = 0; j < ctx_dim->member_count; j++) {
            const char *inv = inv_dim->members[i];
            const char *ctx = ctx_dim->members[j];

            int level = pair_coverage_level(inv, ctx, inv_arms, ctx_arms, arm_count);
            if (level < 3) all_covered = false;

            if (level == 0) {
                /* Not covered at all — error */
                diag_error(diags, DIAG_SCOPE_CONFUSION, filename, d->line, 0,
                           "projection '%s' has no case for cross-product "
                           "pair (%s, %s) and no default arm",
                           d->name.text, inv, ctx);
            } else if (level == 1) {
                /* Covered only by full wildcard (_, _) — note severity */
                diag_note(diags, DIAG_SCOPE_CONFUSION, filename, d->line, 0,
                          "projection '%s' missing explicit case for pair "
                          "(%s, %s) — covered by wildcard default",
                          d->name.text, inv, ctx);
            }
            /* level 2 (partial wildcard) and 3 (exact) — no diagnostic */
        }
    }

    if (all_covered) {
        report->cross_product_complete++;
    }

    /* Cleanup */
    for (size_t i = 0; i < arm_count; i++) {
        free(inv_arms[i]);
        free(ctx_arms[i]);
    }
}

/* ------------------------------------------------------------ */
/* Meihua body validation                                        */
/* ------------------------------------------------------------ */

/* Known pure math functions (same set as meihua_expr_to_ts/sql) */
static const char *known_functions[] = {
    "sqrt", "abs", "floor", "ceil", "round",
    "min", "max", "log", "log2",
    "sin", "cos", "tan", "acos", "asin", "atan", "atan2", "pow",
    NULL
};

static bool is_known_function(const char *name) {
    for (size_t i = 0; known_functions[i]; i++) {
        if (strcmp(known_functions[i], name) == 0) return true;
    }
    return false;
}

/* Check if a name is in the parameter list */
static bool is_param(const char *name, Name *params, size_t param_count) {
    for (size_t i = 0; i < param_count; i++) {
        if (params[i].text && strcmp(params[i].text, name) == 0) return true;
    }
    return false;
}

/* Find a meihua declaration by name in the program (local + imports).
 * Returns the Decl or NULL. */
static Decl *find_meihua_decl(const char *name, Program *prog) {
    for (size_t i = 0; i < prog->count; i++) {
        if (prog->decls[i]->type == DECL_MEIHUA &&
            prog->decls[i]->name.text &&
            strcmp(prog->decls[i]->name.text, name) == 0) {
            return prog->decls[i];
        }
    }
    /* Check imports */
    size_t imp_count = 0;
    Decl **imports = collect_imported_decls(prog, &imp_count);
    Decl *found = NULL;
    for (size_t i = 0; i < imp_count; i++) {
        if (imports[i]->type == DECL_MEIHUA &&
            imports[i]->name.text &&
            strcmp(imports[i]->name.text, name) == 0) {
            found = imports[i];
            break;
        }
    }
    free(imports);
    return found;
}

/* Check if a name is a known meihua in the same program */
static bool is_known_meihua(const char *name, Program *prog) {
    for (size_t i = 0; i < prog->count; i++) {
        if (prog->decls[i]->type == DECL_MEIHUA &&
            prog->decls[i]->name.text &&
            strcmp(prog->decls[i]->name.text, name) == 0) {
            return true;
        }
    }
    /* Also check imports */
    size_t imp_count = 0;
    Decl **imports = collect_imported_decls(prog, &imp_count);
    bool found = false;
    for (size_t i = 0; i < imp_count; i++) {
        if (imports[i]->type == DECL_MEIHUA &&
            imports[i]->name.text &&
            strcmp(imports[i]->name.text, name) == 0) {
            found = true;
            break;
        }
    }
    free(imports);
    return found;
}

static void validate_meihua_expr(Expr *e, const char *meihua_name,
                                  Name *params, size_t param_count,
                                  Program *prog,
                                  const char *filename, DiagList *diags,
                                  bool *has_issues, int *arity_errors) {
    if (!e) return;

    switch (e->type) {
    case EXPR_CALL:
        if (e->as.call.callee) {
            if (!is_known_function(e->as.call.callee) &&
                !is_known_meihua(e->as.call.callee, prog)) {
                *has_issues = true;
                diag_warn(diags, DIAG_NICHE_PIPE, filename, e->line, e->col,
                          "meihua '%s' calls unknown function '%s' — "
                          "will emit verbatim (not mapped to Math.*/SQL equivalent)",
                          meihua_name, e->as.call.callee);
            }
            /* Arity checking: if callee is a known meihua, verify arg count */
            Decl *callee_decl = find_meihua_decl(e->as.call.callee, prog);
            if (callee_decl) {
                size_t expected = callee_decl->as.exec_layer.param_count;
                size_t actual = e->as.call.arg_count;
                if (expected != actual) {
                    *has_issues = true;
                    (*arity_errors)++;
                    diag_warn(diags, DIAG_SCOPE_CONFUSION, filename, e->line, e->col,
                              "meihua '%s' calls '%s' with %zu args "
                              "but '%s' expects %zu parameters",
                              meihua_name, e->as.call.callee, actual,
                              e->as.call.callee, expected);
                }
            }
        }
        /* Validate args recursively */
        for (size_t i = 0; i < e->as.call.arg_count; i++) {
            validate_meihua_expr(e->as.call.args[i], meihua_name,
                                  params, param_count, prog,
                                  filename, diags, has_issues, arity_errors);
        }
        break;

    case EXPR_IDENT:
        if (e->as.ident.name &&
            !is_param(e->as.ident.name, params, param_count) &&
            !is_known_meihua(e->as.ident.name, prog)) {
            *has_issues = true;
            diag_warn(diags, DIAG_SCOPE_CONFUSION, filename, e->line, e->col,
                      "meihua '%s' references undefined identifier '%s' "
                      "(not a parameter)",
                      meihua_name, e->as.ident.name);
        }
        break;

    case EXPR_BINARY:
        validate_meihua_expr(e->as.binary.left, meihua_name,
                              params, param_count, prog,
                              filename, diags, has_issues, arity_errors);
        validate_meihua_expr(e->as.binary.right, meihua_name,
                              params, param_count, prog,
                              filename, diags, has_issues, arity_errors);
        break;

    case EXPR_UNARY:
        validate_meihua_expr(e->as.unary.operand, meihua_name,
                              params, param_count, prog,
                              filename, diags, has_issues, arity_errors);
        break;

    case EXPR_BLOCK:
        for (size_t i = 0; i < e->as.block.count; i++) {
            validate_meihua_expr(e->as.block.stmts[i], meihua_name,
                                  params, param_count, prog,
                                  filename, diags, has_issues, arity_errors);
        }
        break;

    case EXPR_NUMBER:
    case EXPR_STRING:
    case EXPR_WILDCARD:
        /* Terminals — always valid */
        break;

    case EXPR_COALESCE:
        validate_meihua_expr(e->as.coalesce.left, meihua_name,
                              params, param_count, prog,
                              filename, diags, has_issues, arity_errors);
        validate_meihua_expr(e->as.coalesce.right, meihua_name,
                              params, param_count, prog,
                              filename, diags, has_issues, arity_errors);
        break;

    case EXPR_LIST:
        for (size_t i = 0; i < e->as.list.count; i++) {
            validate_meihua_expr(e->as.list.items[i], meihua_name,
                                  params, param_count, prog,
                                  filename, diags, has_issues, arity_errors);
        }
        break;

    case EXPR_MATCH:
        if (e->as.match_expr.discriminant) {
            validate_meihua_expr(e->as.match_expr.discriminant, meihua_name,
                                  params, param_count, prog,
                                  filename, diags, has_issues, arity_errors);
        }
        for (size_t i = 0; i < e->as.match_expr.arm_count; i++) {
            if (e->as.match_expr.arms[i].body) {
                validate_meihua_expr(e->as.match_expr.arms[i].body, meihua_name,
                                      params, param_count, prog,
                                      filename, diags, has_issues, arity_errors);
            }
        }
        break;

    case EXPR_IF:
        validate_meihua_expr(e->as.if_expr.condition, meihua_name,
                              params, param_count, prog,
                              filename, diags, has_issues, arity_errors);
        validate_meihua_expr(e->as.if_expr.then_branch, meihua_name,
                              params, param_count, prog,
                              filename, diags, has_issues, arity_errors);
        validate_meihua_expr(e->as.if_expr.else_branch, meihua_name,
                              params, param_count, prog,
                              filename, diags, has_issues, arity_errors);
        break;

    default:
        break;
    }
}

static void check_meihua_validation(Decl *d, Program *prog,
                                     const char *filename,
                                     DiagList *diags,
                                     ExhaustReport *report) {
    if (d->type != DECL_MEIHUA) return;
    if (!d->name.text) return;

    report->meihua_checked++;

    Expr *body = d->as.exec_layer.body;
    /* Unwrap single-statement blocks */
    if (body && body->type == EXPR_BLOCK && body->as.block.count == 1) {
        body = body->as.block.stmts[0];
    }

    bool has_issues = false;
    int arity_errors = 0;
    validate_meihua_expr(body, d->name.text,
                          d->as.exec_layer.params,
                          d->as.exec_layer.param_count,
                          prog, filename, diags, &has_issues, &arity_errors);
    report->meihua_arity_errors += arity_errors;

    if (!has_issues) {
        report->meihua_clean++;
    }
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

ExhaustReport exhaustcheck(Program *prog, DiagList *diags) {
    ExhaustReport report = {0};

    /* Build dimension registry from imports + local */
    DimRegistry dr;
    dim_registry_init(&dr);
    dim_registry_add_imported(&dr, prog);
    dim_registry_add_local(&dr, prog);

    /* Check all projections for exhaustiveness */
    for (size_t i = 0; i < prog->count; i++) {
        check_projection_exhaustiveness(prog->decls[i], &dr,
                                         prog->filename, diags, &report);
    }

    /* Check cross-product exhaustiveness (both axes are dimensions) */
    for (size_t i = 0; i < prog->count; i++) {
        check_cross_product(prog->decls[i], &dr,
                             prog->filename, diags, &report);
    }

    /* Validate all meihua bodies */
    for (size_t i = 0; i < prog->count; i++) {
        check_meihua_validation(prog->decls[i], prog,
                                 prog->filename, diags, &report);
    }

    dim_registry_free(&dr);
    return report;
}
