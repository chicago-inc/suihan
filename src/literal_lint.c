/*
 * suhc — literal_lint.c
 * Line-precise hex / rgba detection in TypeScript source files.
 * Output format matches the `<path>:<line>:<col>: error: ...` convention
 * the rest of suhc uses, so editors can jump-to-line.
 *
 * Skipped files: theme.ts (the source-of-truth), .test.ts(x), node_modules,
 * .expo, build, dist, and anything inside src/primitives (generated tokens).
 */

#include "literal_lint.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

static int is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int file_skipped(const char *path) {
    /* theme.ts is the source of truth — exempt */
    if (strstr(path, "src/lib/theme.ts")) return 1;
    if (strstr(path, "src\\lib\\theme.ts")) return 1;
    /* generated tokens */
    if (strstr(path, "src/primitives/")) return 1;
    if (strstr(path, "src\\primitives\\")) return 1;
    /* tests */
    if (strstr(path, ".test.")) return 1;
    /* skipped trees */
    if (strstr(path, "node_modules")) return 1;
    if (strstr(path, ".expo")) return 1;
    if (strstr(path, "/build/")) return 1;
    if (strstr(path, "/dist/")) return 1;
    return 0;
}

static int has_ts_extension(const char *path) {
    size_t len = strlen(path);
    if (len < 3) return 0;
    if (strcmp(path + len - 3, ".ts") == 0) return 1;
    if (len >= 4 && strcmp(path + len - 4, ".tsx") == 0) return 1;
    return 0;
}

/* Scan one line for hex / rgba and emit diagnostics. Returns hits count. */
static int scan_line(const char *path, int line_no, const char *line) {
    int hits = 0;
    const char *p = line;
    while (*p) {
        /* #RRGGBB or #RGB */
        if (*p == '#') {
            int len = 0;
            while (is_hex(p[1 + len])) len++;
            if (len == 6 || len == 3) {
                /* Boundary check: char after must not be hex (would be 7+ digit) */
                if (!is_hex(p[1 + len])) {
                    int col = (int)(p - line) + 1;
                    printf("%s:%d:%d: error [literal]: inline hex '%.*s' "
                           "(use a colors.* token from src/lib/theme.ts)\n",
                           path, line_no, col, len + 1, p);
                    hits++;
                    p += 1 + len;
                    continue;
                }
            }
        }
        /* rgba( or rgb( */
        if ((p[0] == 'r' || p[0] == 'R') &&
            (p[1] == 'g' || p[1] == 'G') &&
            (p[2] == 'b' || p[2] == 'B')) {
            const char *q = p + 3;
            if (*q == 'a' || *q == 'A') q++;
            while (*q == ' ') q++;
            if (*q == '(') {
                /* Word-boundary check: char before must not be alphanumeric */
                if (p == line || (!isalnum((unsigned char)p[-1]) && p[-1] != '_')) {
                    int col = (int)(p - line) + 1;
                    /* Find closing paren for the snippet */
                    const char *close = q;
                    while (*close && *close != ')' && (close - q) < 60) close++;
                    int snippet_len = (int)(close - p) + 1;
                    printf("%s:%d:%d: error [literal]: inline color '%.*s' "
                           "(use a colors.* token from src/lib/theme.ts)\n",
                           path, line_no, col, snippet_len, p);
                    hits++;
                    p = close + 1;
                    continue;
                }
            }
        }
        p++;
    }
    return hits;
}

int literal_lint_file(const char *path) {
    if (!path || file_skipped(path) || !has_ts_extension(path)) return 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    int total = 0;
    char line[8192];
    int line_no = 0;
    while (fgets(line, sizeof(line), f)) {
        line_no++;
        /* Skip pure comment lines (// at column 0 ignoring leading ws) */
        const char *t = line;
        while (*t && isspace((unsigned char)*t)) t++;
        if (t[0] == '/' && t[1] == '/') continue;
        total += scan_line(path, line_no, line);
    }
    fclose(f);
    return total;
}

static int dir_skipped(const char *name) {
    if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0))) return 1;
    if (strcmp(name, "node_modules") == 0) return 1;
    if (strcmp(name, ".expo") == 0) return 1;
    if (strcmp(name, ".git") == 0) return 1;
    if (strcmp(name, "build") == 0) return 1;
    if (strcmp(name, "dist") == 0) return 1;
    return 0;
}

int literal_lint_tree(const char *root) {
    DIR *d = opendir(root);
    if (!d) return 0;
    int total = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (dir_skipped(e->d_name)) continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", root, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            total += literal_lint_tree(path);
        } else if (S_ISREG(st.st_mode)) {
            total += literal_lint_file(path);
        }
    }
    closedir(d);
    return total;
}
