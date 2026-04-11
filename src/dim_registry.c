/*
 * suhc — dim_registry.c
 * Shared dimension registry implementation.
 *
 * Collects dimension declarations and their members so multiple
 * checker passes can query dimension membership without rebuilding
 * the registry independently.
 */

#include "dim_registry.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void dim_registry_init(DimRegistry *dr) {
    dr->capacity = 16;
    dr->dims = calloc(dr->capacity, sizeof(DimEntry));
    dr->count = 0;
}

void dim_registry_free(DimRegistry *dr) {
    for (size_t i = 0; i < dr->count; i++) {
        free(dr->dims[i].name);
        for (size_t j = 0; j < dr->dims[i].member_count; j++) {
            free(dr->dims[i].members[j]);
        }
        free(dr->dims[i].members);
    }
    free(dr->dims);
}

void dim_registry_add(DimRegistry *dr, const char *name, Expr *members) {
    if (!name) return;

    if (dr->count >= dr->capacity) {
        dr->capacity *= 2;
        dr->dims = realloc(dr->dims, dr->capacity * sizeof(DimEntry));
    }
    DimEntry *de = &dr->dims[dr->count++];
    de->name = strdup(name);
    de->members = NULL;
    de->member_count = 0;

    if (!members) return;

    size_t cap = 8;
    de->members = malloc(cap * sizeof(char *));

    if (members->type == EXPR_LIST) {
        for (size_t i = 0; i < members->as.list.count; i++) {
            Expr *item = members->as.list.items[i];
            if (item && item->type == EXPR_IDENT && item->as.ident.name) {
                if (de->member_count >= cap) {
                    cap *= 2;
                    de->members = realloc(de->members, cap * sizeof(char *));
                }
                de->members[de->member_count++] = strdup(item->as.ident.name);
            }
        }
    } else if (members->type == EXPR_ENUM) {
        for (size_t i = 0; i < members->as.enumeration.count; i++) {
            if (members->as.enumeration.items[i].name.text) {
                if (de->member_count >= cap) {
                    cap *= 2;
                    de->members = realloc(de->members, cap * sizeof(char *));
                }
                de->members[de->member_count++] =
                    strdup(members->as.enumeration.items[i].name.text);
            }
        }
    }
}

void dim_registry_add_imported(DimRegistry *dr, Program *prog) {
    size_t imp_count = 0;
    Decl **imports = collect_imported_decls(prog, &imp_count);
    for (size_t i = 0; i < imp_count; i++) {
        if (imports[i]->type == DECL_DIMENSION && imports[i]->name.text) {
            dim_registry_add(dr, imports[i]->name.text,
                             imports[i]->as.dimension.members);
        }
    }
    free(imports);
}

void dim_registry_add_local(DimRegistry *dr, Program *prog) {
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type == DECL_DIMENSION && d->name.text) {
            dim_registry_add(dr, d->name.text, d->as.dimension.members);
        }
    }
}

DimEntry *dim_registry_find(DimRegistry *dr, const char *name) {
    if (!name) return NULL;

    /* Exact match first */
    for (size_t i = 0; i < dr->count; i++) {
        if (strcmp(dr->dims[i].name, name) == 0) return &dr->dims[i];
    }

    /* Singular/plural fuzzy match: try appending 's' to the query name */
    size_t len = strlen(name);
    char *plural = malloc(len + 2);
    sprintf(plural, "%ss", name);
    for (size_t i = 0; i < dr->count; i++) {
        if (strcmp(dr->dims[i].name, plural) == 0) {
            free(plural);
            return &dr->dims[i];
        }
    }
    free(plural);

    /* Try stripping trailing 's' from query */
    if (len > 1 && name[len - 1] == 's') {
        char *singular = strndup(name, len - 1);
        for (size_t i = 0; i < dr->count; i++) {
            if (strcmp(dr->dims[i].name, singular) == 0) {
                free(singular);
                return &dr->dims[i];
            }
        }
        free(singular);
    }

    return NULL;
}
