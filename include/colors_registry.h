/*
 * suhc — colors_registry.h
 *
 * In-memory registry of (name → color_value) pairs extracted from
 * theme.ts. Bridges visual.szh token references (colors.textHeading,
 * colors.white, etc.) to concrete sRGB values so the D35 two-channel
 * axiom can be evaluated at build time.
 *
 * Parse target: a TypeScript file containing `export const colors = {
 *   name: 'value', ...  }` block. Values may be '#hex', "#hex", or
 * 'rgba(...)'. Comments and other blocks are ignored.
 *
 * Values are stored verbatim (including the quote style used in source)
 * so the string can round-trip. Lookups return a pointer into registry
 * storage — do not free, do not mutate. Call colors_registry_free()
 * when the registry is no longer needed.
 */
#ifndef SUHC_COLORS_REGISTRY_H
#define SUHC_COLORS_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *name;    /* e.g., "textHeading" */
    char *value;   /* e.g., "#18181B" or "rgba(255,255,255,0.85)" */
} ColorEntry;

typedef struct {
    ColorEntry *entries;
    size_t      count;
    size_t      cap;
} ColorsRegistry;

/* Initialize an empty registry. */
void colors_registry_init(ColorsRegistry *r);

/* Free all memory owned by the registry. */
void colors_registry_free(ColorsRegistry *r);

/* Load entries by parsing a theme.ts source file. Returns the number of
 * entries loaded (0 on parse failure or missing file). Appends to the
 * registry — does not clear prior entries. */
size_t colors_registry_load_theme(ColorsRegistry *r, const char *theme_path);

/* Lookup a color value by token name. Returns NULL if not found.
 * Does not resolve aliases — an entry whose value is a `colors.other`
 * reference is returned as-is. Two-pass resolution (follow aliases)
 * is the caller's responsibility. */
const char *colors_registry_lookup(const ColorsRegistry *r, const char *name);

/* Resolve a token through one level of `colors.X` aliases. If the
 * registry contains `foo: "colors.bar"` and `bar: "#123456"`, calling
 * resolve with "foo" returns "#123456". Returns NULL if chain is broken
 * or exceeds max depth (prevents infinite cycles). Max depth is 8. */
const char *colors_registry_resolve(const ColorsRegistry *r, const char *name);

/* Print all entries to stdout (diagnostic). */
void colors_registry_dump(const ColorsRegistry *r);

#endif
