/*
 * suhc — colors_registry.c
 *
 * See include/colors_registry.h for the contract.
 *
 * Parser strategy: locate the `export const colors = {` header, then walk
 * forward line by line matching `identifier: 'value',` or
 * `identifier: "value",` until the closing `};`. Comments and empty lines
 * are skipped. Values may contain commas inside rgba(...) parens, which
 * we track explicitly.
 *
 * This is a regex-adjacent line scanner, not a full TypeScript parser.
 * theme.ts is constitutionally required (BD #41) to use the flat
 * `name: 'value'` shape, so a token-level parser would be overkill.
 */

#include "colors_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void colors_registry_init(ColorsRegistry *r) {
    if (!r) return;
    r->entries = NULL;
    r->count = 0;
    r->cap = 0;
}

void colors_registry_free(ColorsRegistry *r) {
    if (!r) return;
    for (size_t i = 0; i < r->count; i++) {
        free(r->entries[i].name);
        free(r->entries[i].value);
    }
    free(r->entries);
    r->entries = NULL;
    r->count = 0;
    r->cap = 0;
}

static void registry_append(ColorsRegistry *r, const char *name, size_t nlen,
                            const char *val, size_t vlen) {
    if (r->count == r->cap) {
        size_t ncap = r->cap ? r->cap * 2 : 32;
        ColorEntry *ne = realloc(r->entries, ncap * sizeof(ColorEntry));
        if (!ne) return;
        r->entries = ne;
        r->cap = ncap;
    }
    ColorEntry *e = &r->entries[r->count++];
    e->name = malloc(nlen + 1);
    e->value = malloc(vlen + 1);
    if (!e->name || !e->value) {
        free(e->name); free(e->value);
        r->count--;
        return;
    }
    memcpy(e->name, name, nlen); e->name[nlen] = 0;
    memcpy(e->value, val, vlen); e->value[vlen] = 0;
}

/* Parse a line like `  textHeading: '#18181B',` or `  foo: "rgba(...)",`.
 * Returns 1 on success, 0 if the line isn't a valid entry. */
static int parse_entry_line(const char *line, const char **name, size_t *nlen,
                            const char **val, size_t *vlen) {
    const char *p = line;
    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;

    /* Identifier: [A-Za-z_][A-Za-z0-9_]* */
    const char *n_start = p;
    if (!(isalpha((unsigned char)*p) || *p == '_')) return 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
    size_t nl = p - n_start;
    if (nl == 0) return 0;

    /* Optional whitespace, then ':' */
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ':') return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;

    /* Value: either a quoted string or an identifier path (e.g., colors.other). */
    char quote = 0;
    if (*p == '\'' || *p == '"') { quote = *p; p++; }

    const char *v_start = p;
    if (quote) {
        while (*p && *p != quote) p++;
        if (*p != quote) return 0;  /* unterminated */
    } else {
        /* Accept identifier-path or hex literal without quotes (unlikely) */
        while (*p && *p != ',' && *p != '}' && !isspace((unsigned char)*p)) p++;
    }
    size_t vl = p - v_start;
    if (vl == 0) return 0;

    *name = n_start; *nlen = nl;
    *val = v_start; *vlen = vl;
    return 1;
}

size_t colors_registry_load_theme(ColorsRegistry *r, const char *theme_path) {
    if (!r || !theme_path) return 0;
    FILE *f = fopen(theme_path, "rb");
    if (!f) return 0;

    size_t added = 0;
    char line[4096];
    int in_colors = 0;
    int brace_depth = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = 0;
        }

        if (!in_colors) {
            /* Match `export const colors = {` (tolerate whitespace/spacing) */
            if (strstr(line, "export const colors") && strchr(line, '{')) {
                in_colors = 1;
                brace_depth = 1;
                continue;
            }
            continue;
        }

        /* Inside the block. Track braces. */
        for (const char *p = line; *p; p++) {
            if (*p == '{') brace_depth++;
            else if (*p == '}') brace_depth--;
        }
        if (brace_depth <= 0) break;

        /* Skip comment-only lines */
        const char *t = line;
        while (*t && isspace((unsigned char)*t)) t++;
        if (t[0] == '/' && t[1] == '/') continue;
        if (t[0] == 0) continue;

        const char *name, *val;
        size_t nlen, vlen;
        if (parse_entry_line(line, &name, &nlen, &val, &vlen)) {
            registry_append(r, name, nlen, val, vlen);
            added++;
        }
    }
    fclose(f);
    return added;
}

const char *colors_registry_lookup(const ColorsRegistry *r, const char *name) {
    if (!r || !name) return NULL;
    for (size_t i = 0; i < r->count; i++) {
        if (strcmp(r->entries[i].name, name) == 0) {
            return r->entries[i].value;
        }
    }
    return NULL;
}

const char *colors_registry_resolve(const ColorsRegistry *r, const char *name) {
    const char *cur = name;
    for (int i = 0; i < 8 && cur; i++) {
        const char *v = colors_registry_lookup(r, cur);
        if (!v) return NULL;
        /* If value starts with "colors." follow the alias */
        if (strncmp(v, "colors.", 7) == 0) {
            cur = v + 7;
            continue;
        }
        return v;
    }
    return NULL;
}

void colors_registry_dump(const ColorsRegistry *r) {
    if (!r) return;
    for (size_t i = 0; i < r->count; i++) {
        printf("  %-24s  %s\n", r->entries[i].name, r->entries[i].value);
    }
    printf("  (%zu entries)\n", r->count);
}
