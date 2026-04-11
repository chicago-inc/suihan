/*
 * suhc — dim_registry.h
 * Shared dimension registry: collects dimension declarations and
 * their members for use by multiple checker passes (perpcheck,
 * exhaustcheck).
 */

#ifndef SUHC_DIM_REGISTRY_H
#define SUHC_DIM_REGISTRY_H

#include "ast.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char  *name;         /* dimension name, e.g., "structural_roles" */
    char **members;      /* member values, e.g., ["owner","admin",...] */
    size_t member_count;
} DimEntry;

typedef struct {
    DimEntry *dims;
    size_t    count;
    size_t    capacity;
} DimRegistry;

void      dim_registry_init(DimRegistry *dr);
void      dim_registry_free(DimRegistry *dr);

/* Add a dimension from a DECL_DIMENSION node */
void      dim_registry_add(DimRegistry *dr, const char *name, Expr *members);

/* Add all dimensions from a program's imported declarations */
void      dim_registry_add_imported(DimRegistry *dr, Program *prog);

/* Add all dimensions from a program's local declarations */
void      dim_registry_add_local(DimRegistry *dr, Program *prog);

/* Find a dimension by name. Also matches singular forms against
 * plural dimension names (e.g., "structural_role" matches
 * "structural_roles"). Returns NULL if not found. */
DimEntry *dim_registry_find(DimRegistry *dr, const char *name);

#endif /* SUHC_DIM_REGISTRY_H */
