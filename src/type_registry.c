/*
 * suhc — type_registry.c
 * Type registry: collects type-relevant declarations for the type checker.
 *
 * Populates from the parsed + resolved AST. Walks imported declarations
 * (one level deep, same as collect_imported_decls). Linear scan for
 * lookups — acceptable at ordbok scale (~60 type-relevant declarations).
 *
 * Sprint 6A — Type System Foundation.
 */

#include "type_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char *type_reg_kind_name(TypeRegKind k) {
    switch (k) {
    case TREG_UNIT:      return "unit";
    case TREG_MAGNITUDE: return "magnitude";
    case TREG_VECTOR:    return "vector";
    case TREG_DIMENSION: return "dimension";
    case TREG_ZERO:      return "zero";
    }
    return "unknown";
}

/* ------------------------------------------------------------ */
/* Internal helpers                                              */
/* ------------------------------------------------------------ */

static void ensure_entry_capacity(TypeRegistry *reg) {
    if (reg->entry_count >= reg->entry_capacity) {
        reg->entry_capacity = reg->entry_capacity ? reg->entry_capacity * 2 : 64;
        reg->entries = realloc(reg->entries, reg->entry_capacity * sizeof(TypeEntry));
    }
}

static void ensure_rel_capacity(TypeRegistry *reg) {
    if (reg->rel_count >= reg->rel_capacity) {
        reg->rel_capacity = reg->rel_capacity ? reg->rel_capacity * 2 : 32;
        reg->relations = realloc(reg->relations, reg->rel_capacity * sizeof(TypeRelation));
    }
}

/* Check if entry already exists (avoid duplicates from imports) */
static bool has_entry(TypeRegistry *reg, const char *name) {
    for (size_t i = 0; i < reg->entry_count; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) return true;
    }
    return false;
}

static void add_entry(TypeRegistry *reg, const char *name, TypeRegKind kind) {
    if (has_entry(reg, name)) return;
    ensure_entry_capacity(reg);
    TypeEntry *e = &reg->entries[reg->entry_count++];
    e->name = strdup(name);
    e->reg_kind = kind;
    e->fields = NULL;
    e->field_count = 0;
    e->variants = NULL;
    e->variant_count = 0;
}

static void add_relation(TypeRegistry *reg, const char *left,
                          const char *right, TypeRelKind kind, int line) {
    ensure_rel_capacity(reg);
    TypeRelation *r = &reg->relations[reg->rel_count++];
    r->left = strdup(left);
    r->right = strdup(right);
    r->kind = kind;
    r->line = line;
}

/* ------------------------------------------------------------ */
/* Process a single declaration                                  */
/* ------------------------------------------------------------ */

static void process_decl(TypeRegistry *reg, Decl *d) {
    switch (d->type) {
    case DECL_UNIT:
        add_entry(reg, d->name.text, TREG_UNIT);
        break;

    case DECL_ZERO:
        add_entry(reg, d->name.text, TREG_ZERO);
        break;

    case DECL_MAGNITUDE:
        add_entry(reg, d->name.text, TREG_MAGNITUDE);
        break;

    case DECL_VECTOR: {
        add_entry(reg, d->name.text, TREG_VECTOR);
        /* Extract field names from the dimension's member list if present */
        TypeEntry *e = type_registry_lookup(reg, d->name.text);
        if (e && d->as.dimension.members &&
            d->as.dimension.members->type == EXPR_LIST) {
            Expr *list = d->as.dimension.members;
            e->field_count = list->as.list.count;
            e->fields = calloc(e->field_count, sizeof(char *));
            for (size_t i = 0; i < e->field_count; i++) {
                if (list->as.list.items[i]->type == EXPR_IDENT) {
                    e->fields[i] = strdup(list->as.list.items[i]->as.ident.name);
                }
            }
        }
        break;
    }

    case DECL_DIMENSION: {
        add_entry(reg, d->name.text, TREG_DIMENSION);
        /* Extract variant names from enum expression */
        TypeEntry *e = type_registry_lookup(reg, d->name.text);
        if (e && d->as.dimension.members &&
            d->as.dimension.members->type == EXPR_ENUM) {
            Expr *en = d->as.dimension.members;
            e->variant_count = en->as.enumeration.count;
            e->variants = calloc(e->variant_count, sizeof(char *));
            for (size_t i = 0; i < e->variant_count; i++) {
                e->variants[i] = strdup(en->as.enumeration.items[i].name.text);
            }
        }
        break;
    }

    case DECL_INCOMMENSURABLE:
        /* Add relation for each pair */
        for (size_t i = 0; i < d->as.relation.count; i++) {
            for (size_t j = i + 1; j < d->as.relation.count; j++) {
                add_relation(reg,
                    d->as.relation.names[i].text,
                    d->as.relation.names[j].text,
                    TREL_INCOMMENSURABLE, d->line);
            }
        }
        break;

    case DECL_COMMENSURABLE:
        for (size_t i = 0; i < d->as.relation.count; i++) {
            for (size_t j = i + 1; j < d->as.relation.count; j++) {
                add_relation(reg,
                    d->as.relation.names[i].text,
                    d->as.relation.names[j].text,
                    TREL_COMMENSURABLE, d->line);
            }
        }
        break;

    default:
        break; /* Not type-relevant */
    }
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

void type_registry_build(TypeRegistry *reg, Program *prog) {
    memset(reg, 0, sizeof(TypeRegistry));

    /* Process imported declarations first */
    size_t imp_count = 0;
    Decl **imported = collect_imported_decls(prog, &imp_count);
    for (size_t i = 0; i < imp_count; i++) {
        process_decl(reg, imported[i]);
    }
    free(imported);

    /* Process local declarations */
    for (size_t i = 0; i < prog->count; i++) {
        process_decl(reg, prog->decls[i]);
    }
}

TypeEntry *type_registry_lookup(TypeRegistry *reg, const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < reg->entry_count; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            return &reg->entries[i];
        }
    }
    return NULL;
}

bool type_registry_are_incommensurable(TypeRegistry *reg,
                                        const char *a, const char *b) {
    for (size_t i = 0; i < reg->rel_count; i++) {
        TypeRelation *r = &reg->relations[i];
        if (r->kind != TREL_INCOMMENSURABLE) continue;
        if ((strcmp(r->left, a) == 0 && strcmp(r->right, b) == 0) ||
            (strcmp(r->left, b) == 0 && strcmp(r->right, a) == 0)) {
            return true;
        }
    }
    return false;
}

bool type_registry_are_commensurable(TypeRegistry *reg,
                                      const char *a, const char *b) {
    for (size_t i = 0; i < reg->rel_count; i++) {
        TypeRelation *r = &reg->relations[i];
        if (r->kind != TREL_COMMENSURABLE) continue;
        if ((strcmp(r->left, a) == 0 && strcmp(r->right, b) == 0) ||
            (strcmp(r->left, b) == 0 && strcmp(r->right, a) == 0)) {
            return true;
        }
    }
    return false;
}

void type_registry_dump(TypeRegistry *reg) {
    printf("Type Registry\n");
    printf("=============\n");
    printf("  entries: %zu\n", reg->entry_count);
    printf("  relations: %zu\n\n", reg->rel_count);

    printf("  Types:\n");
    for (size_t i = 0; i < reg->entry_count; i++) {
        TypeEntry *e = &reg->entries[i];
        printf("    %-24s %s", e->name, type_reg_kind_name(e->reg_kind));
        if (e->variant_count > 0) {
            printf("  variants:");
            for (size_t j = 0; j < e->variant_count; j++) {
                printf(" %s", e->variants[j]);
                if (j + 1 < e->variant_count) printf(",");
            }
        }
        if (e->field_count > 0) {
            printf("  fields:");
            for (size_t j = 0; j < e->field_count; j++) {
                printf(" %s", e->fields[j] ? e->fields[j] : "?");
                if (j + 1 < e->field_count) printf(",");
            }
        }
        printf("\n");
    }

    if (reg->rel_count > 0) {
        printf("\n  Relations:\n");
        for (size_t i = 0; i < reg->rel_count; i++) {
            TypeRelation *r = &reg->relations[i];
            printf("    %s %s %s  (line %d)\n",
                   r->left,
                   r->kind == TREL_INCOMMENSURABLE ? "≠" : "≈",
                   r->right, r->line);
        }
    }
    printf("\n");
}

void type_registry_free(TypeRegistry *reg) {
    for (size_t i = 0; i < reg->entry_count; i++) {
        TypeEntry *e = &reg->entries[i];
        free(e->name);
        for (size_t j = 0; j < e->field_count; j++) free(e->fields[j]);
        free(e->fields);
        for (size_t j = 0; j < e->variant_count; j++) free(e->variants[j]);
        free(e->variants);
    }
    free(reg->entries);
    for (size_t i = 0; i < reg->rel_count; i++) {
        free(reg->relations[i].left);
        free(reg->relations[i].right);
    }
    free(reg->relations);
    memset(reg, 0, sizeof(TypeRegistry));
}
