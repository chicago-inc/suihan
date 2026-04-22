/*
 * suhc — ordbok_pairs.c
 *
 * See include/ordbok_pairs.h.
 *
 * Extracts text_color and surface_bundle projection cases from
 * visual.szh. The .szh projection syntax is regular:
 *
 *   projection text_color :
 *     invariant xi text_role
 *     context zeta surface_class
 *     yields omega color_token
 *     cases:
 *       (heading, card_default) -> "colors.textHeading"
 *       (heading, gradient_page) -> "colors.gradientHeading"
 *       ...
 *       (_, _) -> "colors.textBody"
 *
 * We identify the `projection <name> :` header, advance to `cases:`,
 * and parse `(<role>, <surface>) -> "colors.<token>"` arms until
 * the indentation drops or EOF.
 *
 * surface_bundle shape is the same with a `bg` context, so we filter
 * to cases where the second tuple element is `bg`.
 */

#include "ordbok_pairs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void ordbok_pairs_init(OrdbokPairs *p) {
    if (!p) return;
    p->items = NULL;
    p->count = 0;
    p->cap = 0;
}

void ordbok_pairs_free(OrdbokPairs *p) {
    if (!p) return;
    for (size_t i = 0; i < p->count; i++) {
        free(p->items[i].role);
        free(p->items[i].surface);
        free(p->items[i].fg_token);
        free(p->items[i].bg_token);
    }
    free(p->items);
    p->items = NULL;
    p->count = 0;
    p->cap = 0;
}

static char *xstrndup(const char *s, size_t n) {
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = 0;
    return out;
}

/* Match "projection <name> :" header at start of line (skipping ws). */
static int line_is_projection_header(const char *line, const char *name) {
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "projection", 10) != 0) return 0;
    p += 10;
    while (*p && isspace((unsigned char)*p)) p++;
    size_t nlen = strlen(name);
    if (strncmp(p, name, nlen) != 0) return 0;
    p += nlen;
    while (*p && isspace((unsigned char)*p)) p++;
    return *p == ':';
}

/* Match "cases:" marker */
static int line_is_cases(const char *line) {
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    return strncmp(p, "cases:", 6) == 0;
}

/* Parse `(a, b) -> "<something>"` on a line. Returns 1 on success.
 * Out params are pointers into the line with explicit lengths.
 * Wildcard (`_`) is recognized. The value is the quoted string
 * stripped of quotes. */
static int parse_case_arm(const char *line,
                          const char **a, size_t *alen,
                          const char **b, size_t *blen,
                          const char **v, size_t *vlen) {
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '(') return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *as = p;
    while (*p && *p != ',' && *p != ' ') p++;
    size_t al = p - as;
    if (al == 0) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ',') return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *bs = p;
    while (*p && *p != ')' && *p != ' ') p++;
    size_t bl = p - bs;
    if (bl == 0) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ')') return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!(p[0] == '-' && p[1] == '>')) return 0;
    p += 2;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return 0;
    p++;
    const char *vs = p;
    while (*p && *p != '"') p++;
    if (*p != '"') return 0;
    size_t vl = p - vs;

    *a = as; *alen = al;
    *b = bs; *blen = bl;
    *v = vs; *vlen = vl;
    return 1;
}

/* If the value is "colors.X", return X; else return NULL. */
static char *extract_colors_token(const char *v, size_t vlen) {
    if (vlen < 8) return NULL;
    if (strncmp(v, "colors.", 7) != 0) return NULL;
    return xstrndup(v + 7, vlen - 7);
}

/* Look up the bg token for a surface_class in a map. Returns NULL if
 * not found. Linear scan is fine — the surface set is small. */
static const char *lookup_bg(const OrdbokPairs *bg_map, const char *surface) {
    for (size_t i = 0; i < bg_map->count; i++) {
        if (strcmp(bg_map->items[i].role, surface) == 0) {
            return bg_map->items[i].bg_token;
        }
    }
    return NULL;
}

static void append_text_color_pair(OrdbokPairs *p, const char *role,
                                   const char *surface, const char *fg_token,
                                   const char *bg_token) {
    if (p->count == p->cap) {
        size_t ncap = p->cap ? p->cap * 2 : 16;
        OrdbokPair *ne = realloc(p->items, ncap * sizeof(OrdbokPair));
        if (!ne) return;
        p->items = ne;
        p->cap = ncap;
    }
    OrdbokPair *e = &p->items[p->count++];
    e->role = strdup(role);
    e->surface = strdup(surface);
    e->fg_token = strdup(fg_token);
    e->bg_token = strdup(bg_token);
}

/* Two-pass parse: first pass builds surface_bundle bg map, second
 * pass builds text_color pairs resolving bg from the map. */
size_t ordbok_pairs_load(OrdbokPairs *pairs, const char *visual_szh_path) {
    if (!pairs || !visual_szh_path) return 0;

    /* Pass 1: surface_bundle bg map. We store surface -> bg_token in
     * a temporary OrdbokPairs by abusing the `role` field as surface
     * and `bg_token` as bg. fg_token and surface stay empty. */
    OrdbokPairs bg_map;
    ordbok_pairs_init(&bg_map);

    FILE *f = fopen(visual_szh_path, "rb");
    if (!f) return 0;

    char line[4096];
    int in_surface_bundle = 0;
    int saw_cases_sb = 0;

    while (fgets(line, sizeof(line), f)) {
        if (!in_surface_bundle) {
            if (line_is_projection_header(line, "surface_bundle")) {
                in_surface_bundle = 1;
                saw_cases_sb = 0;
            }
            continue;
        }
        if (!saw_cases_sb) {
            if (line_is_cases(line)) saw_cases_sb = 1;
            continue;
        }
        /* Blank line or next projection ends the block */
        const char *t = line;
        while (*t && isspace((unsigned char)*t)) t++;
        if (*t == 0) { in_surface_bundle = 0; continue; }
        if (strncmp(t, "projection", 10) == 0) { in_surface_bundle = 0; continue; }
        if (strncmp(t, "ξ", 2) == 0 || t[0] == 'x') {
            /* Rough heuristic: could be 'xi axiom_...' — break out */
            if (strstr(line, "axiom_") || strstr(line, "ξ ") == line) {
                in_surface_bundle = 0;
                continue;
            }
        }

        const char *a, *b, *v;
        size_t al, bl, vl;
        if (!parse_case_arm(line, &a, &al, &b, &bl, &v, &vl)) continue;
        /* Only interested in (<surface>, bg) cases */
        if (bl != 2 || strncmp(b, "bg", 2) != 0) continue;
        /* Skip wildcard */
        if (al == 1 && a[0] == '_') continue;

        char *token = extract_colors_token(v, vl);
        if (!token) continue;
        char *surface = xstrndup(a, al);

        /* Append to bg_map */
        if (bg_map.count == bg_map.cap) {
            size_t ncap = bg_map.cap ? bg_map.cap * 2 : 16;
            OrdbokPair *ne = realloc(bg_map.items, ncap * sizeof(OrdbokPair));
            if (ne) { bg_map.items = ne; bg_map.cap = ncap; }
        }
        OrdbokPair *entry = &bg_map.items[bg_map.count++];
        entry->role = surface;          /* reused as surface key */
        entry->surface = strdup("");
        entry->fg_token = strdup("");
        entry->bg_token = token;
    }
    fclose(f);

    /* Pass 2: text_color cases, resolving bg via bg_map. */
    f = fopen(visual_szh_path, "rb");
    if (!f) {
        ordbok_pairs_free(&bg_map);
        return 0;
    }

    int in_text_color = 0;
    int saw_cases_tc = 0;
    size_t added = 0;

    while (fgets(line, sizeof(line), f)) {
        if (!in_text_color) {
            if (line_is_projection_header(line, "text_color")) {
                in_text_color = 1;
                saw_cases_tc = 0;
            }
            continue;
        }
        if (!saw_cases_tc) {
            if (line_is_cases(line)) saw_cases_tc = 1;
            continue;
        }
        const char *t = line;
        while (*t && isspace((unsigned char)*t)) t++;
        if (*t == 0) { in_text_color = 0; continue; }
        if (strncmp(t, "projection", 10) == 0) { in_text_color = 0; continue; }

        const char *a, *b, *v;
        size_t al, bl, vl;
        if (!parse_case_arm(line, &a, &al, &b, &bl, &v, &vl)) continue;

        /* Skip wildcards — they're catch-alls, not canonical pairs */
        if (al == 1 && a[0] == '_') continue;
        if (bl == 1 && b[0] == '_') continue;

        char *fg_token = extract_colors_token(v, vl);
        if (!fg_token) continue;

        char role_buf[128];
        char surf_buf[128];
        if (al >= sizeof(role_buf) || bl >= sizeof(surf_buf)) {
            free(fg_token); continue;
        }
        memcpy(role_buf, a, al); role_buf[al] = 0;
        memcpy(surf_buf, b, bl); surf_buf[bl] = 0;

        const char *bg_token = lookup_bg(&bg_map, surf_buf);
        if (!bg_token) { free(fg_token); continue; }

        append_text_color_pair(pairs, role_buf, surf_buf, fg_token, bg_token);
        free(fg_token);
        added++;
    }
    fclose(f);

    ordbok_pairs_free(&bg_map);
    return added;
}
