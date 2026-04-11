/*
 * suhc — bloatlint.c
 * Bloat linter: all six error categories.
 *
 * The six bloat causes (§8) are the ONLY categories of
 * compile error/warning this pass produces.
 *
 * 1. Reduplication — same ξ at two addresses
 * 2. Niche pipe — ζ in ξ position (lossless projection exists)
 * 3. Temporal sediment — unreachable R.k path
 * 4. Failure to derive — ω without R.k derivation
 * 5. Scope confusion — information in wrong kind slot
 * 6. Obtruding documentation — R.k mechanism leaking into ω
 */

#include "bloatlint.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------ */
/* 1. Reduplication detection                                    */
/* ------------------------------------------------------------ */

/* Check if any ξ-kinded declaration has the same value as another.
 * (Name-based reduplication is caught by kindcheck; this checks
 * structural equivalence of values.) */
static void check_reduplication(Program *prog, DiagList *diags) {
    for (size_t i = 0; i < prog->count; i++) {
        Decl *a = prog->decls[i];
        if (a->kind != KIND_XI) continue;
        if (a->type != DECL_KINDED_VALUE) continue;
        if (!a->as.kinded.value) continue;

        for (size_t j = i + 1; j < prog->count; j++) {
            Decl *b = prog->decls[j];
            if (b->kind != KIND_XI) continue;
            if (b->type != DECL_KINDED_VALUE) continue;
            if (!b->as.kinded.value) continue;

            /* Check if both are enums with identical members */
            if (a->as.kinded.value->type == EXPR_ENUM &&
                b->as.kinded.value->type == EXPR_ENUM) {
                Expr *ea = a->as.kinded.value;
                Expr *eb = b->as.kinded.value;
                if (ea->as.enumeration.count == eb->as.enumeration.count) {
                    bool same = true;
                    for (size_t k = 0; k < ea->as.enumeration.count; k++) {
                        if (strcmp(ea->as.enumeration.items[k].name.text,
                                   eb->as.enumeration.items[k].name.text) != 0) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        diag_error(diags, DIAG_REDUPLICATION, prog->filename,
                                   b->line, b->name.col,
                                   "'%s' and '%s' are identical ξ enums — "
                                   "same information at two addresses",
                                   a->name.text, b->name.text);
                    }
                }
            }
        }
    }
}

/* ------------------------------------------------------------ */
/* 2. Niche pipe detection                                       */
/* ------------------------------------------------------------ */

/* Check if an ξ-kinded enum member is a projection of an existing member.
 * Classic example: 'commissioner' is admin in league context. */
static void check_niche_pipes(Program *prog, DiagList *diags) {
    /* Collect all projection declarations */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->kind != KIND_XI || d->type != DECL_KINDED_VALUE) continue;
        if (!d->as.kinded.value || d->as.kinded.value->type != EXPR_ENUM) continue;

        /* Check each enum member against projections */
        Expr *en = d->as.kinded.value;
        for (size_t m = 0; m < en->as.enumeration.count; m++) {
            const char *member = en->as.enumeration.items[m].name.text;

            /* Look for projections that could produce this member */
            for (size_t j = 0; j < prog->count; j++) {
                if (prog->decls[j]->type != DECL_PROJECTION) continue;

                /* Check projection fields for references to this enum */
                Decl *proj = prog->decls[j];
                for (size_t f = 0; f < proj->as.projection.field_count; f++) {
                    DeclField *field = &proj->as.projection.fields[f];
                    if (!field->value) continue;

                    /* If the projection yields a value matching this member
                     * AND the projection's invariant is another member of
                     * this same enum, it's a niche pipe. */
                    if (field->value->type == EXPR_STRING &&
                        field->value->as.string.value &&
                        strcasecmp(field->value->as.string.value, member) == 0) {
                        /* Found a projection that produces this label.
                         * This is a potential niche pipe — the member might
                         * just be a ζ-projection of another member. */
                        diag_warn(diags, DIAG_NICHE_PIPE, prog->filename,
                                  en->as.enumeration.items[m].name.line,
                                  en->as.enumeration.items[m].name.col,
                                  "'%s' in '%s' may be a niche pipe — "
                                  "projection '%s' can produce this label "
                                  "from existing identity values",
                                  member, d->name.text, proj->name.text);
                    }
                }
            }
        }
    }
}

/* ------------------------------------------------------------ */
/* 3. Temporal sediment detection                                */
/* ------------------------------------------------------------ */

/* Find traversals/morphisms/projections that no other declaration references. */
static void check_temporal_sediment(Program *prog, DiagList *diags) {
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_TRAVERSAL && d->type != DECL_MORPHISM &&
            d->type != DECL_PROJECTION && d->type != DECL_MEIHUA &&
            d->type != DECL_ZHULIN) {
            continue;
        }
        if (!d->name.text) continue;

        /* Search all other declarations for references to this name */
        bool referenced = false;
        for (size_t j = 0; j < prog->count && !referenced; j++) {
            if (j == i) continue;
            /* Simple text search in expression trees — Phase 0A approximation.
             * A full implementation would walk the expression tree. */
            /* For now, we check if any kinded value or traversal section
             * references this name. This catches the common case. */
            Decl *other = prog->decls[j];
            if (other->type == DECL_KINDED_VALUE && other->as.kinded.value) {
                if (other->as.kinded.value->type == EXPR_IDENT &&
                    other->as.kinded.value->as.ident.name &&
                    strstr(other->as.kinded.value->as.ident.name, d->name.text)) {
                    referenced = true;
                }
                if (other->as.kinded.value->type == EXPR_CALL &&
                    other->as.kinded.value->as.call.callee &&
                    strcmp(other->as.kinded.value->as.call.callee, d->name.text) == 0) {
                    referenced = true;
                }
            }
            if (other->type == DECL_TRAVERSAL) {
                for (size_t s = 0; s < other->as.traversal.section_count && !referenced; s++) {
                    Expr *body = other->as.traversal.sections[s].body;
                    if (body && body->type == EXPR_IDENT &&
                        body->as.ident.name &&
                        strstr(body->as.ident.name, d->name.text)) {
                        referenced = true;
                    }
                }
            }
            /* Check dependency/morphism field values */
            if (other->type == DECL_DEPENDENCY) {
                for (size_t f = 0; f < other->as.dependency.field_count && !referenced; f++) {
                    if (other->as.dependency.fields[f].value &&
                        other->as.dependency.fields[f].value->type == EXPR_IDENT &&
                        other->as.dependency.fields[f].value->as.ident.name &&
                        strcmp(other->as.dependency.fields[f].value->as.ident.name, d->name.text) == 0) {
                        referenced = true;
                    }
                }
            }
        }

        if (!referenced) {
            diag_warn(diags, DIAG_TEMPORAL_SEDIMENT, prog->filename,
                      d->line, d->name.col,
                      "%s '%s' is not referenced by any other declaration — "
                      "possible temporal sediment (unreachable R.k path)",
                      decl_type_name(d->type), d->name.text);
        }
    }
}

/* ------------------------------------------------------------ */
/* 4. Failure to derive detection                                */
/* ------------------------------------------------------------ */

/* Find ω-kinded values that have no traversal path (R.k) producing them. */
static void check_failure_to_derive(Program *prog, DiagList *diags) {
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->kind != KIND_OMEGA) continue;
        if (d->type != DECL_KINDED_VALUE) continue;

        /* Check if this ω value is derived from a traversal, projection,
         * or other R.k path. In a well-formed program, ω values are
         * outputs of traversals, not standalone declarations. */
        bool has_derivation = false;

        /* A traversal that produces an output referencing this name */
        for (size_t j = 0; j < prog->count; j++) {
            if (prog->decls[j]->type == DECL_TRAVERSAL) {
                for (size_t s = 0; s < prog->decls[j]->as.traversal.section_count; s++) {
                    if (prog->decls[j]->as.traversal.sections[s].section_kind == KIND_OMEGA) {
                        has_derivation = true;
                        break;
                    }
                }
            }
            if (has_derivation) break;
        }

        if (!has_derivation) {
            /* A standalone ω without derivation is a fiat declaration */
            if (d->as.kinded.value && d->as.kinded.value->type == EXPR_LIST) {
                diag_error(diags, DIAG_FAILURE_TO_DERIVE, prog->filename,
                           d->line, d->name.col,
                           "output '%s' is a fiat list with no derivation path — "
                           "ω values must be derived through R.k traversal",
                           d->name.text);
            }
        }
    }
}

/* ------------------------------------------------------------ */
/* 5. Scope confusion detection                                  */
/* ------------------------------------------------------------ */

/* Already partially handled by kindcheck. This pass catches
 * additional patterns: ξ values that should be ζ (they change
 * between contexts), x values in ξ positions, etc. */
static void check_scope_confusion(Program *prog, DiagList *diags) {
    /* Check: are any ξ-kinded values likely to be x-kinded?
     * Heuristic: numbers in ξ position are suspicious. */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->kind != KIND_XI || d->type != DECL_KINDED_VALUE) continue;
        if (!d->as.kinded.value) continue;

        if (d->as.kinded.value->type == EXPR_NUMBER) {
            diag_warn(diags, DIAG_SCOPE_CONFUSION, prog->filename,
                      d->line, d->name.col,
                      "'%s' is ξ-kind (identity) but holds a numeric value — "
                      "numeric values that change belong in x-kind (variable)",
                      d->name.text);
        }
    }
}

/* ------------------------------------------------------------ */
/* 6. Obtruding documentation detection                          */
/* ------------------------------------------------------------ */

/* Check for R.k-class information in ω-class outputs.
 * Heuristic: string literals in ω outputs mentioning implementation
 * details (session numbers, sprint references, etc.) */
static void check_obtruding_doc(Program *prog, DiagList *diags) {
    static const char *session_markers[] = {
        "session", "sprint", "renamed", "refactored",
        "migrated", "we changed", "previously", NULL
    };

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->kind != KIND_OMEGA) continue;

        /* Check string literals in the value */
        Expr *val = NULL;
        if (d->type == DECL_KINDED_VALUE) val = d->as.kinded.value;

        if (val && val->type == EXPR_STRING && val->as.string.value) {
            for (int m = 0; session_markers[m]; m++) {
                if (strcasestr(val->as.string.value, session_markers[m])) {
                    diag_warn(diags, DIAG_OBTRUDING_DOC, prog->filename,
                              val->line, val->col,
                              "ω output contains '%s' — this looks like R.k-class "
                              "information (traversal history) leaking into output",
                              session_markers[m]);
                    break;
                }
            }
        }
    }
}

/* ------------------------------------------------------------ */
/* Yoneda gap detection (bonus: not one of the six, but derived) */
/* ------------------------------------------------------------ */

static void check_yoneda_gaps(Program *prog, DiagList *diags) {
    /* Check: declared morphisms that are never used in a traversal
     * or projection. An unobserved morphism is a gap in identity. */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_MORPHISM) continue;
        if (!d->name.text) continue;

        bool observed = false;
        for (size_t j = 0; j < prog->count && !observed; j++) {
            if (j == i) continue;
            Decl *other = prog->decls[j];
            if (other->type == DECL_TRAVERSAL || other->type == DECL_PROJECTION) {
                /* Simple check: does any field reference this morphism? */
                /* Phase 0A: text match approximation */
                if (other->type == DECL_TRAVERSAL) {
                    for (size_t s = 0; s < other->as.traversal.section_count; s++) {
                        Expr *body = other->as.traversal.sections[s].body;
                        if (body && body->type == EXPR_IDENT &&
                            body->as.ident.name &&
                            strcmp(body->as.ident.name, d->name.text) == 0) {
                            observed = true;
                        }
                        if (body && body->type == EXPR_CALL &&
                            body->as.call.callee &&
                            strcmp(body->as.call.callee, d->name.text) == 0) {
                            observed = true;
                        }
                    }
                }
            }
        }

        if (!observed) {
            diag_warn(diags, DIAG_YONEDA_GAP, prog->filename,
                      d->line, d->name.col,
                      "morphism '%s' is declared but not observed by any "
                      "traversal or projection — Yoneda gap",
                      d->name.text);
        }
    }
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

/* Build a merged program containing both local and imported declarations.
 * The merged program is used by lint checks that need cross-file visibility.
 * Caller must free the merged program (but not the Decls). */
static Program *build_merged_view(Program *prog) {
    size_t imp_count = 0;
    Decl **imports = collect_imported_decls(prog, &imp_count);

    Program *merged = calloc(1, sizeof(Program));
    merged->capacity = prog->count + imp_count;
    merged->decls = calloc(merged->capacity, sizeof(Decl *));
    merged->filename = prog->filename; /* borrowed, not owned */

    /* Imported declarations first (they precede local in ordbok order) */
    for (size_t i = 0; i < imp_count; i++)
        merged->decls[merged->count++] = imports[i];
    for (size_t i = 0; i < prog->count; i++)
        merged->decls[merged->count++] = prog->decls[i];

    free(imports);
    return merged;
}

void bloatlint(Program *prog, DiagList *diags) {
    /* Build merged view for cross-file reference checks */
    Program *merged = build_merged_view(prog);

    check_reduplication(prog, diags);     /* local only — cross-file reduplication is intentional sharing */
    check_niche_pipes(merged, diags);     /* need to see imported projections */
    check_temporal_sediment(merged, diags); /* imported refs count as references */
    check_failure_to_derive(merged, diags); /* imported traversals count */
    check_scope_confusion(prog, diags);    /* local only */
    check_obtruding_doc(prog, diags);      /* local only */
    check_yoneda_gaps(merged, diags);      /* imported traversals observe morphisms */

    free(merged->decls);
    free(merged);
}
