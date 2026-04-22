/*
 * suhc — ordbok_pairs.h
 *
 * Loader for (role, surface, fg_token, bg_token) canonical pairs
 * extracted from visual.szh's text_color + surface_bundle projections.
 *
 * Used by --contrast-audit to enumerate the pair matrix without
 * requiring main.c to carry a hardcoded list that drifts from the
 * ordbok. This is UW-026 Sprint 2E's deliverable.
 *
 * Parser strategy: regex-adjacent line scanner over visual.szh.
 * Identifies `projection text_color :` and `projection surface_bundle :`
 * blocks, walks their `cases:` bodies, extracts `(a, b) -> "colors.X"`
 * tuples. Not a full .szh parser — the projection shape is regular
 * enough for line scanning.
 *
 * Output: a list of Pair entries suitable for the --contrast-audit
 * loop in main.c. Each surface is resolved to its `bg` token via the
 * surface_bundle projection; wildcards are not expanded (they expand
 * at audit time via the color_math pipeline's surface heuristic).
 */
#ifndef SUHC_ORDBOK_PAIRS_H
#define SUHC_ORDBOK_PAIRS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *role;        /* e.g., "heading" */
    char *surface;     /* e.g., "card_default" */
    char *fg_token;    /* e.g., "textHeading" (strips the "colors." prefix) */
    char *bg_token;    /* e.g., "white" (strips the "colors." prefix) */
} OrdbokPair;

typedef struct {
    OrdbokPair *items;
    size_t      count;
    size_t      cap;
} OrdbokPairs;

void ordbok_pairs_init(OrdbokPairs *p);
void ordbok_pairs_free(OrdbokPairs *p);

/* Parse visual.szh at the given path. Populates the pairs array with
 * one entry per (role, surface) case declared in the text_color
 * projection, with bg_token resolved from the surface_bundle
 * projection's `bg` field. Wildcard cases (_, _) are skipped. Returns
 * the number of pairs loaded. */
size_t ordbok_pairs_load(OrdbokPairs *p, const char *visual_szh_path);

/* Same as ordbok_pairs_load but parses any named projection. Used by
 * --contrast-audit to enumerate icon_color pairs alongside text_color.
 * Sprint 2K. */
size_t ordbok_pairs_load_projection(OrdbokPairs *p,
                                     const char *visual_szh_path,
                                     const char *projection_name);

#endif
