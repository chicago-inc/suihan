/*
 * suhc — ast.c
 * AST node constructors and utilities.
 */

#include "ast.h"
#include <stdlib.h>
#include <string.h>

/* M2: kind_name() and decl_type_name() are now generated from
 * ordbok/compiler/compiler_kind_names.szh and compiler_decl_type_names.szh.
 * The static inline functions are included via ast.h → gen/kind_names.h
 * and gen/decl_type_names.h. No hand-written implementations needed. */

Name name_new(const char *text, int line, int col) {
    Name n;
    n.text = text ? strdup(text) : NULL;
    n.line = line;
    n.col = col;
    return n;
}

Name name_dup(Name n) {
    return name_new(n.text, n.line, n.col);
}

void name_free(Name *n) {
    if (n && n->text) {
        free(n->text);
        n->text = NULL;
    }
}

Program *program_new(const char *filename) {
    Program *p = calloc(1, sizeof(Program));
    p->capacity = 64;
    p->decls = calloc(p->capacity, sizeof(Decl *));
    p->filename = filename ? strdup(filename) : NULL;
    return p;
}

void program_push(Program *prog, Decl *decl) {
    if (prog->count >= prog->capacity) {
        prog->capacity *= 2;
        prog->decls = realloc(prog->decls, prog->capacity * sizeof(Decl *));
    }
    prog->decls[prog->count++] = decl;
}

void program_free(Program *prog) {
    if (!prog) return;
    /* Note: full recursive free of Decl/Expr trees omitted for brevity
     * in Phase 0A. Acceptable since suhc is a short-lived process. */
    free(prog->decls);
    free(prog->filename);
    free(prog);
}

Decl *decl_new(DeclType type, Kind kind, Name name, int line) {
    Decl *d = calloc(1, sizeof(Decl));
    d->type = type;
    d->kind = kind;
    d->name = name;
    d->line = line;
    return d;
}

Expr *expr_new(ExprType type, int line, int col) {
    Expr *e = calloc(1, sizeof(Expr));
    e->type = type;
    e->line = line;
    e->col = col;
    return e;
}

Decl **collect_imported_decls(Program *prog, size_t *out_count) {
    size_t cap = 64;
    size_t count = 0;
    Decl **result = calloc(cap, sizeof(Decl *));

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_IMPORT) continue;
        if (!d->as.import_decl.resolved) continue;

        Program *imported = d->as.import_decl.resolved;
        for (size_t j = 0; j < imported->count; j++) {
            if (count >= cap) {
                cap *= 2;
                result = realloc(result, cap * sizeof(Decl *));
            }
            result[count++] = imported->decls[j];
        }
    }

    *out_count = count;
    return result;
}
