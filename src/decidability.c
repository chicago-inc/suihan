/*
 * suhc — decidability.c
 * Oracle ceiling enforcement (D10).
 *
 * The decidability checker enforces the constitutional principle
 * that undecidable output (where two reasonable users would decide
 * differently) must be PRESENTED, not ACTED ON.
 *
 * Two passes:
 *   Pass 1: For each traversal, inspect ω section for undecidable markers.
 *   Pass 2: For each traversal that references an undecidable traversal's
 *           output in its data section, check whether R.k performs an
 *           action or a presentation.
 */

#include "decidability.h"
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------ */
/* Pass 1: Classify traversal outputs                            */
/* ------------------------------------------------------------ */

typedef struct {
    const char *name;   /* traversal name */
    bool undecidable;   /* true if output contains undecidable marker */
} TraversalDecidability;

/* Check if an expression tree contains EXPR_UNDECIDABLE */
static bool expr_has_undecidable(Expr *e) {
    if (!e) return false;

    if (e->type == EXPR_UNDECIDABLE) return true;

    /* Recurse into sub-expressions */
    switch (e->type) {
    case EXPR_CALL:
        for (size_t i = 0; i < e->as.call.arg_count; i++) {
            if (expr_has_undecidable(e->as.call.args[i])) return true;
        }
        break;
    case EXPR_LIST:
        for (size_t i = 0; i < e->as.list.count; i++) {
            if (expr_has_undecidable(e->as.list.items[i])) return true;
        }
        break;
    case EXPR_BLOCK:
        for (size_t i = 0; i < e->as.block.count; i++) {
            if (expr_has_undecidable(e->as.block.stmts[i])) return true;
        }
        break;
    case EXPR_ARROW:
        if (expr_has_undecidable(e->as.arrow.from)) return true;
        if (expr_has_undecidable(e->as.arrow.to)) return true;
        break;
    case EXPR_COALESCE:
        if (expr_has_undecidable(e->as.coalesce.left)) return true;
        if (expr_has_undecidable(e->as.coalesce.right)) return true;
        break;
    case EXPR_BINARY:
        if (expr_has_undecidable(e->as.binary.left)) return true;
        if (expr_has_undecidable(e->as.binary.right)) return true;
        break;
    case EXPR_DECIDABLE:
    case EXPR_UNDECIDABLE:
        return e->type == EXPR_UNDECIDABLE;
    default:
        break;
    }
    return false;
}

/* ------------------------------------------------------------ */
/* Pass 2: Action detection in R.k                               */
/* ------------------------------------------------------------ */

/* Keywords indicating an ACTION (not presentation) */
static const char *action_keywords[] = {
    "assign", "write", "send", "delete", "update",
    "create", "execute", "activate", "complete",
    "remove", "insert", "commit", "approve", "reject",
    "confirm", "bind", "reserve",
    NULL
};

/* Keywords indicating PRESENTATION (not action) */
static const char *present_keywords[] = {
    "present", "show", "render", "display", "list",
    "filter", "map", "view", "format", "preview",
    NULL
};

/* Check if a name matches any action keyword */
static bool is_action_keyword(const char *name) {
    if (!name) return false;
    for (int i = 0; action_keywords[i]; i++) {
        if (strstr(name, action_keywords[i]) != NULL) return true;
    }
    return false;
}

/* Check if a name matches any presentation keyword */
static bool is_present_keyword(const char *name) {
    if (!name) return false;
    for (int i = 0; present_keywords[i]; i++) {
        if (strstr(name, present_keywords[i]) != NULL) return true;
    }
    return false;
}

/* Check if an R.k expression contains action keywords */
static bool rk_has_action(Expr *e) {
    if (!e) return false;

    switch (e->type) {
    case EXPR_IDENT:
        return is_action_keyword(e->as.ident.name);
    case EXPR_CALL:
        if (is_action_keyword(e->as.call.callee)) return true;
        for (size_t i = 0; i < e->as.call.arg_count; i++) {
            if (rk_has_action(e->as.call.args[i])) return true;
        }
        break;
    case EXPR_BLOCK:
        for (size_t i = 0; i < e->as.block.count; i++) {
            if (rk_has_action(e->as.block.stmts[i])) return true;
        }
        break;
    case EXPR_PIPE_CHAIN:
        for (size_t i = 0; i < e->as.pipe_chain.count; i++) {
            if (rk_has_action(e->as.pipe_chain.stages[i])) return true;
        }
        break;
    default:
        break;
    }
    return false;
}

/* Check if an R.k expression contains only presentation keywords */
static bool rk_is_presentation(Expr *e) {
    if (!e) return false;

    switch (e->type) {
    case EXPR_IDENT:
        return is_present_keyword(e->as.ident.name);
    case EXPR_CALL:
        return is_present_keyword(e->as.call.callee);
    default:
        break;
    }
    return false;
}

/* Check if a data section references a named traversal's output */
static bool data_references_traversal(Decl *trav, const char *target_name) {
    if (!trav || !target_name) return false;

    for (size_t s = 0; s < trav->as.traversal.section_count; s++) {
        TraversalSection *sec = &trav->as.traversal.sections[s];
        if (sec->section_kind != KIND_X) continue;

        /* Check body for references to target_name.output */
        Expr *body = sec->body;
        if (!body) continue;

        /* Check ident references like suggest_table.output */
        if (body->type == EXPR_IDENT && body->as.ident.name) {
            if (strstr(body->as.ident.name, target_name) != NULL)
                return true;
        }
        /* Check block of expressions */
        if (body->type == EXPR_BLOCK) {
            for (size_t i = 0; i < body->as.block.count; i++) {
                Expr *stmt = body->as.block.stmts[i];
                if (stmt && stmt->type == EXPR_IDENT && stmt->as.ident.name) {
                    if (strstr(stmt->as.ident.name, target_name) != NULL)
                        return true;
                }
            }
        }
    }
    return false;
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

void decidability_check(Program *prog, DiagList *diags) {
    if (!prog || prog->count == 0) return;

    /* Pass 1: classify all traversal outputs */
    size_t n_trav = 0;
    for (size_t i = 0; i < prog->count; i++) {
        if (prog->decls[i]->type == DECL_TRAVERSAL) n_trav++;
    }
    if (n_trav == 0) return;

    TraversalDecidability *td = calloc(n_trav, sizeof(TraversalDecidability));
    size_t ti = 0;

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_TRAVERSAL) continue;

        td[ti].name = d->name.text;
        td[ti].undecidable = false;

        /* Check omega sections for undecidable markers */
        for (size_t s = 0; s < d->as.traversal.section_count; s++) {
            TraversalSection *sec = &d->as.traversal.sections[s];
            if (sec->section_kind != KIND_OMEGA) continue;

            if (expr_has_undecidable(sec->body)) {
                td[ti].undecidable = true;
            }
        }
        ti++;
    }

    /* Pass 2: check downstream traversals for action on undecidable input */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_TRAVERSAL || !d->name.text) continue;

        /* For each undecidable upstream traversal */
        for (size_t u = 0; u < n_trav; u++) {
            if (!td[u].undecidable || !td[u].name) continue;

            /* Does this traversal reference the undecidable output? */
            if (!data_references_traversal(d, td[u].name)) continue;

            /* Check R.k for action */
            Expr *rk_body = NULL;
            for (size_t s = 0; s < d->as.traversal.section_count; s++) {
                if (d->as.traversal.sections[s].section_kind == KIND_RK) {
                    rk_body = d->as.traversal.sections[s].body;
                    break;
                }
            }

            if (rk_has_action(rk_body) && !rk_is_presentation(rk_body)) {
                diag_error(diags, DIAG_UNDECIDABLE_ACTION, prog->filename,
                           d->name.line, d->name.col,
                           "traversal '%s' acts on undecidable output from '%s' "
                           "without user confirmation — oracle ceiling violation (D10)",
                           d->name.text, td[u].name);
            }
        }
    }

    free(td);
}
