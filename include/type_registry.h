/*
 * suhc — the suihan compiler
 * type_registry.h — type registry interface
 *
 * Collects all type-relevant declarations (units, magnitudes,
 * vectors, dimensions, incommensurability/commensurability
 * relations) into a queryable registry. Populated after resolve.c,
 * before typecheck.c.
 *
 * Completes the third leg of the scope triad:
 *   - dim_registry.h: dimensions (lists of vectors of like unit)
 *   - perpcheck.c: perpendicularity constraints
 *   - type_registry.h: typed-property lookup (this file)
 *
 * Sprint 6A — Type System Foundation.
 */

#ifndef SUHC_TYPE_REGISTRY_H
#define SUHC_TYPE_REGISTRY_H

#include "ast.h"
#include <stdbool.h>

/* The type kinds recognized by the registry */
typedef enum {
    TREG_UNIT,
    TREG_MAGNITUDE,
    TREG_VECTOR,
    TREG_DIMENSION,
    TREG_ZERO,
} TypeRegKind;

const char *type_reg_kind_name(TypeRegKind k);

/* A single type entry */
typedef struct {
    char       *name;
    TypeRegKind reg_kind;
    /* For vectors: field names */
    char      **fields;
    size_t      field_count;
    /* For dimensions: variant names */
    char      **variants;
    size_t      variant_count;
} TypeEntry;

/* A relation between two types */
typedef enum {
    TREL_INCOMMENSURABLE,
    TREL_COMMENSURABLE,
} TypeRelKind;

typedef struct {
    char       *left;
    char       *right;
    TypeRelKind kind;
    int         line;   /* source line of declaration */
} TypeRelation;

/* The registry itself */
typedef struct {
    TypeEntry    *entries;
    size_t        entry_count;
    size_t        entry_capacity;

    TypeRelation *relations;
    size_t        rel_count;
    size_t        rel_capacity;
} TypeRegistry;

/* Build the registry from a resolved program (walks imports). */
void type_registry_build(TypeRegistry *reg, Program *prog);

/* Look up a type by name. Returns NULL if not found. */
TypeEntry *type_registry_lookup(TypeRegistry *reg, const char *name);

/* Check if two types are declared incommensurable. */
bool type_registry_are_incommensurable(TypeRegistry *reg,
                                        const char *a, const char *b);

/* Check if two types are declared commensurable. */
bool type_registry_are_commensurable(TypeRegistry *reg,
                                      const char *a, const char *b);

/* Dump registry contents to stdout (--dump-types). */
void type_registry_dump(TypeRegistry *reg);

/* Free all registry memory. */
void type_registry_free(TypeRegistry *reg);

#endif /* SUHC_TYPE_REGISTRY_H */
