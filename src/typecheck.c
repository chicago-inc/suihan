/*
 * suhc — typecheck.c
 * Type checking pass: arity, type resolution, incommensurability.
 *
 * Gradual typing: untyped parameters accept any argument.
 * Typed parameters enforce their declared constraints.
 *
 * Sprint 6A — Type System Foundation.
 */

#include "typecheck.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------ */
/* Helpers: find declarations by name                            */
/* ------------------------------------------------------------ */

/* Look up a meihua/zhulin/songqiao declaration by callee name,
 * searching local and imported declarations. */
static Decl *find_exec_decl(Program *prog, const char *name) {
    /* Local declarations first */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if ((d->type == DECL_MEIHUA || d->type == DECL_ZHULIN ||
             d->type == DECL_SONGQIAO) &&
            d->name.text && strcmp(d->name.text, name) == 0) {
            return d;
        }
    }
    /* Imported declarations */
    size_t imp_count = 0;
    Decl **imported = collect_imported_decls(prog, &imp_count);
    Decl *found = NULL;
    for (size_t i = 0; i < imp_count; i++) {
        Decl *d = imported[i];
        if ((d->type == DECL_MEIHUA || d->type == DECL_ZHULIN ||
             d->type == DECL_SONGQIAO) &&
            d->name.text && strcmp(d->name.text, name) == 0) {
            found = d;
            break;
        }
    }
    free(imported);
    return found;
}

/* ------------------------------------------------------------ */
/* Check 1: Validate type annotations reference declared types   */
/* ------------------------------------------------------------ */

static void check_type_annotations(Decl *d, TypeRegistry *reg,
                                    DiagList *diags, TypecheckReport *rpt) {
    if (d->type != DECL_MEIHUA && d->type != DECL_ZHULIN &&
        d->type != DECL_SONGQIAO) return;
    if (d->as.exec_layer.param_count == 0) return;

    for (size_t i = 0; i < d->as.exec_layer.param_count; i++) {
        rpt->total_params++;

        if (!d->as.exec_layer.param_types) continue;
        Name *pty = &d->as.exec_layer.param_types[i];
        if (!pty->text) continue; /* untyped — gradual typing */

        rpt->typed_params++;

        /* Verify the type name exists in the registry or is a kind keyword.
         * Kind keywords (unit, magnitude, vector, dimension) are valid type
         * annotations even if no specific declaration carries that exact name.
         * They denote the type *kind* rather than a specific named type. */
        TypeEntry *te = type_registry_lookup(reg, pty->text);
        bool is_kind_keyword = (strcmp(pty->text, "unit") == 0 ||
                                strcmp(pty->text, "magnitude") == 0 ||
                                strcmp(pty->text, "vector") == 0 ||
                                strcmp(pty->text, "dimension") == 0 ||
                                strcmp(pty->text, "zero") == 0);
        if (!te && !is_kind_keyword) {
            diag_error(diags, DIAG_UNKNOWN_TYPE, NULL,
                       pty->line, pty->col,
                       "unknown type '%s' in annotation for parameter '%s' of '%s'",
                       pty->text,
                       d->as.exec_layer.params[i].text,
                       d->name.text);
            rpt->unknown_type_errors++;
        }
    }
}

/* ------------------------------------------------------------ */
/* Check 2: Arity checking at call sites                         */
/* ------------------------------------------------------------ */

static void check_call_arity(Expr *e, Program *prog,
                              DiagList *diags, TypecheckReport *rpt) {
    if (!e) return;

    if (e->type == EXPR_CALL) {
        Decl *target = find_exec_decl(prog, e->as.call.callee);
        if (target) {
            size_t expected = target->as.exec_layer.param_count;
            size_t got = e->as.call.arg_count;
            if (expected != got) {
                diag_error(diags, DIAG_ARITY_MISMATCH, NULL,
                           e->line, e->col,
                           "arity mismatch in call to '%s': "
                           "expected %zu arguments, got %zu",
                           e->as.call.callee, expected, got);
                rpt->arity_errors++;
            }
        }
    }

    /* Recurse into sub-expressions */
    switch (e->type) {
    case EXPR_CALL:
        for (size_t i = 0; i < e->as.call.arg_count; i++)
            check_call_arity(e->as.call.args[i], prog, diags, rpt);
        break;
    case EXPR_BINARY:
        check_call_arity(e->as.binary.left, prog, diags, rpt);
        check_call_arity(e->as.binary.right, prog, diags, rpt);
        break;
    case EXPR_COALESCE:
        check_call_arity(e->as.coalesce.left, prog, diags, rpt);
        check_call_arity(e->as.coalesce.right, prog, diags, rpt);
        break;
    case EXPR_ARROW:
        check_call_arity(e->as.arrow.from, prog, diags, rpt);
        check_call_arity(e->as.arrow.to, prog, diags, rpt);
        break;
    case EXPR_CROSS:
        check_call_arity(e->as.cross.left, prog, diags, rpt);
        check_call_arity(e->as.cross.right, prog, diags, rpt);
        break;
    case EXPR_UNARY:
        check_call_arity(e->as.unary.operand, prog, diags, rpt);
        break;
    case EXPR_BLOCK:
        for (size_t i = 0; i < e->as.block.count; i++)
            check_call_arity(e->as.block.stmts[i], prog, diags, rpt);
        break;
    case EXPR_PIPE_CHAIN:
        for (size_t i = 0; i < e->as.pipe_chain.count; i++)
            check_call_arity(e->as.pipe_chain.stages[i], prog, diags, rpt);
        break;
    case EXPR_LIST:
        for (size_t i = 0; i < e->as.list.count; i++)
            check_call_arity(e->as.list.items[i], prog, diags, rpt);
        break;
    case EXPR_IF:
        check_call_arity(e->as.if_expr.condition, prog, diags, rpt);
        check_call_arity(e->as.if_expr.then_branch, prog, diags, rpt);
        if (e->as.if_expr.else_branch)
            check_call_arity(e->as.if_expr.else_branch, prog, diags, rpt);
        break;
    case EXPR_MATCH:
        check_call_arity(e->as.match_expr.discriminant, prog, diags, rpt);
        for (size_t i = 0; i < e->as.match_expr.arm_count; i++) {
            check_call_arity(e->as.match_expr.arms[i].body, prog, diags, rpt);
        }
        break;
    case EXPR_DECIDABLE:
    case EXPR_UNDECIDABLE:
        check_call_arity(e->as.decidability.inner, prog, diags, rpt);
        break;
    case EXPR_COMPOUND:
        for (size_t i = 0; i < e->as.compound.count; i++)
            check_call_arity(e->as.compound.values[i], prog, diags, rpt);
        break;
    case EXPR_CASE:
        for (size_t i = 0; i < e->as.cases.count; i++)
            check_call_arity(e->as.cases.arms[i].body, prog, diags, rpt);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------ */
/* Check 3: Incommensurability at typed call sites               */
/* ------------------------------------------------------------ */

/* Try to resolve the type name of an expression (best effort).
 * For EXPR_IDENT, look for a kinded value declaration with a type.
 * For EXPR_CALL, return the callee's declared return type if any.
 * Returns NULL if the type cannot be determined. */
static const char *resolve_expr_type(Expr *e, Program *prog) {
    if (!e) return NULL;

    if (e->type == EXPR_IDENT) {
        /* Look for a kinded value: `ξ foo : type = val` or param */
        for (size_t i = 0; i < prog->count; i++) {
            Decl *d = prog->decls[i];
            if (d->type == DECL_KINDED_VALUE &&
                d->name.text && strcmp(d->name.text, e->as.ident.name) == 0) {
                /* Kinded values don't yet carry type annotations in 6A.
                 * This is a future extension point. */
                return NULL;
            }
        }
    }

    /* Cannot determine type — gradual typing allows this */
    return NULL;
}

static void check_call_types(Expr *e, Program *prog, TypeRegistry *reg,
                              DiagList *diags, TypecheckReport *rpt) {
    if (!e) return;

    if (e->type == EXPR_CALL) {
        Decl *target = find_exec_decl(prog, e->as.call.callee);
        if (target && target->as.exec_layer.param_types) {
            size_t n = target->as.exec_layer.param_count;
            if (n > e->as.call.arg_count) n = e->as.call.arg_count;

            for (size_t i = 0; i < n; i++) {
                Name *pty = &target->as.exec_layer.param_types[i];
                if (!pty->text) continue; /* untyped param — skip */

                /* Try to resolve the argument's type */
                const char *arg_type = resolve_expr_type(
                    e->as.call.args[i], prog);
                if (!arg_type) continue; /* can't determine — gradual */

                /* If both types are known, check incommensurability */
                if (strcmp(pty->text, arg_type) != 0 &&
                    type_registry_are_incommensurable(reg, pty->text, arg_type)) {
                    diag_error(diags, DIAG_TYPE_MISMATCH, NULL,
                               e->as.call.args[i]->line,
                               e->as.call.args[i]->col,
                               "incommensurable types in call to '%s': "
                               "parameter '%s' expects '%s', got '%s'",
                               e->as.call.callee,
                               target->as.exec_layer.params[i].text,
                               pty->text, arg_type);
                    rpt->type_errors++;
                }
            }
        }
    }

    /* Recurse — same structure as arity check */
    switch (e->type) {
    case EXPR_CALL:
        for (size_t i = 0; i < e->as.call.arg_count; i++)
            check_call_types(e->as.call.args[i], prog, reg, diags, rpt);
        break;
    case EXPR_BINARY:
        check_call_types(e->as.binary.left, prog, reg, diags, rpt);
        check_call_types(e->as.binary.right, prog, reg, diags, rpt);
        break;
    case EXPR_COALESCE:
        check_call_types(e->as.coalesce.left, prog, reg, diags, rpt);
        check_call_types(e->as.coalesce.right, prog, reg, diags, rpt);
        break;
    case EXPR_ARROW:
        check_call_types(e->as.arrow.from, prog, reg, diags, rpt);
        check_call_types(e->as.arrow.to, prog, reg, diags, rpt);
        break;
    case EXPR_CROSS:
        check_call_types(e->as.cross.left, prog, reg, diags, rpt);
        check_call_types(e->as.cross.right, prog, reg, diags, rpt);
        break;
    case EXPR_UNARY:
        check_call_types(e->as.unary.operand, prog, reg, diags, rpt);
        break;
    case EXPR_BLOCK:
        for (size_t i = 0; i < e->as.block.count; i++)
            check_call_types(e->as.block.stmts[i], prog, reg, diags, rpt);
        break;
    case EXPR_IF:
        check_call_types(e->as.if_expr.condition, prog, reg, diags, rpt);
        check_call_types(e->as.if_expr.then_branch, prog, reg, diags, rpt);
        if (e->as.if_expr.else_branch)
            check_call_types(e->as.if_expr.else_branch, prog, reg, diags, rpt);
        break;
    case EXPR_MATCH:
        check_call_types(e->as.match_expr.discriminant, prog, reg, diags, rpt);
        for (size_t i = 0; i < e->as.match_expr.arm_count; i++)
            check_call_types(e->as.match_expr.arms[i].body, prog, reg, diags, rpt);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------ */
/* Walk all declarations                                         */
/* ------------------------------------------------------------ */

static void check_decl_body(Decl *d, Program *prog, TypeRegistry *reg,
                             DiagList *diags, TypecheckReport *rpt) {
    /* Arity + type checks on expression bodies */
    Expr *body = NULL;

    switch (d->type) {
    case DECL_MEIHUA:
    case DECL_ZHULIN:
    case DECL_SONGQIAO:
        body = d->as.exec_layer.body;
        break;
    case DECL_PROJECTION:
        for (size_t i = 0; i < d->as.projection.arm_count; i++) {
            check_call_arity(d->as.projection.arms[i].body, prog, diags, rpt);
            check_call_types(d->as.projection.arms[i].body, prog, reg, diags, rpt);
        }
        return;
    case DECL_TRAVERSAL:
        for (size_t i = 0; i < d->as.traversal.section_count; i++) {
            check_call_arity(d->as.traversal.sections[i].body, prog, diags, rpt);
            check_call_types(d->as.traversal.sections[i].body, prog, reg, diags, rpt);
        }
        return;
    case DECL_KINDED_VALUE:
        body = d->as.kinded.value;
        break;
    default:
        return;
    }

    if (body) {
        check_call_arity(body, prog, diags, rpt);
        check_call_types(body, prog, reg, diags, rpt);
    }
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

TypecheckReport typecheck(Program *prog, TypeRegistry *reg, DiagList *diags) {
    TypecheckReport rpt = {0};

    /* Pass 1: validate all type annotations */
    for (size_t i = 0; i < prog->count; i++) {
        check_type_annotations(prog->decls[i], reg, diags, &rpt);
    }

    /* Also check imported exec_layer declarations for annotation counts */
    size_t imp_count = 0;
    Decl **imported = collect_imported_decls(prog, &imp_count);
    for (size_t i = 0; i < imp_count; i++) {
        check_type_annotations(imported[i], reg, diags, &rpt);
    }
    free(imported);

    /* Pass 2: arity and type checks on all expression bodies */
    for (size_t i = 0; i < prog->count; i++) {
        check_decl_body(prog->decls[i], prog, reg, diags, &rpt);
    }

    return rpt;
}
