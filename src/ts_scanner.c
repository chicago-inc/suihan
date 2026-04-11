/*
 * suhc — ts_scanner.c
 * Lightweight TypeScript scanner for drift detection.
 *
 * This scanner has intentional S > 0 (D13). It extracts the
 * structural patterns needed for ordbok comparison without
 * attempting to parse all of TypeScript. Complex patterns
 * (computed property names, template literals, generic types)
 * are invisible to this scanner. Its S is acknowledged and
 * measurable.
 */

#include "ts_scanner.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* ------------------------------------------------------------ */
/* Internal utilities                                            */
/* ------------------------------------------------------------ */

static char *str_dup(const char *s) {
    return s ? strdup(s) : NULL;
}

/* Skip whitespace and comments */
static const char *skip_ws(const char *p) {
    while (*p) {
        if (isspace((unsigned char)*p)) { p++; continue; }
        /* Single-line comment */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        /* Multi-line comment */
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }
        break;
    }
    return p;
}

/* Read an identifier at position. Returns length, 0 if not ident. */
static size_t read_ident(const char *p, char *buf, size_t bufsz) {
    size_t i = 0;
    if (!isalpha((unsigned char)*p) && *p != '_' && *p != '$') return 0;
    while (i < bufsz - 1 && (isalnum((unsigned char)p[i]) || p[i] == '_' || p[i] == '$')) {
        buf[i] = p[i];
        i++;
    }
    buf[i] = '\0';
    return i;
}

/* Read a string literal (single or double quoted). Returns content
 * without quotes. Advances *pp past the closing quote. */
static char *read_string_literal(const char **pp) {
    const char *p = *pp;
    char quote = *p;
    if (quote != '\'' && quote != '"' && quote != '`') return NULL;
    p++;

    char buf[1024];
    size_t i = 0;
    while (*p && *p != quote && i < sizeof(buf) - 1) {
        if (*p == '\\' && p[1]) {
            buf[i++] = p[1];
            p += 2;
        } else {
            buf[i++] = *p++;
        }
    }
    buf[i] = '\0';
    if (*p == quote) p++;
    *pp = p;
    return strdup(buf);
}

/* Read a number literal. Advances *pp past the number. */
static char *read_number(const char **pp) {
    const char *p = *pp;
    char buf[64];
    size_t i = 0;
    if (*p == '-') buf[i++] = *p++;
    while (i < sizeof(buf) - 1 && (isdigit((unsigned char)*p) || *p == '.')) {
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    *pp = p;
    return strdup(buf);
}

/* Advance past balanced braces/parens */
static const char *skip_balanced(const char *p, char open, char close) {
    if (*p != open) return p;
    int depth = 1;
    p++;
    while (*p && depth > 0) {
        if (*p == open) depth++;
        else if (*p == close) depth--;
        else if (*p == '\'' || *p == '"' || *p == '`') {
            char q = *p++;
            while (*p && *p != q) {
                if (*p == '\\') p++;
                if (*p) p++;
            }
            if (*p) p++;
            continue;
        }
        if (depth > 0) p++;
    }
    return p;
}

/* Find word boundary match */
static bool word_match(const char *p, const char *word) {
    size_t len = strlen(word);
    if (strncmp(p, word, len) != 0) return false;
    if (isalnum((unsigned char)p[len]) || p[len] == '_') return false;
    return true;
}

/* ------------------------------------------------------------ */
/* Const extraction                                              */
/* ------------------------------------------------------------ */

static void add_const(TsScanResult *r, const char *name, const char *value) {
    size_t cap = r->const_count + 1;
    r->consts = realloc(r->consts, cap * sizeof(TsConst));
    r->consts[r->const_count].name = str_dup(name);
    r->consts[r->const_count].value = str_dup(value);
    r->const_count++;
}

/* ------------------------------------------------------------ */
/* Switch/case extraction                                        */
/* ------------------------------------------------------------ */

static void add_switch(TsScanResult *r) {
    r->switches = realloc(r->switches, (r->switch_count + 1) * sizeof(TsSwitchBlock));
    TsSwitchBlock *sb = &r->switches[r->switch_count];
    sb->switch_var = NULL;
    sb->branches = NULL;
    sb->branch_count = 0;
    sb->branch_capacity = 16;
    sb->branches = malloc(sb->branch_capacity * sizeof(TsCaseBranch));
    r->switch_count++;
}

static void add_branch(TsSwitchBlock *sb, const char *case_val, const char *result) {
    if (sb->branch_count >= sb->branch_capacity) {
        sb->branch_capacity *= 2;
        sb->branches = realloc(sb->branches, sb->branch_capacity * sizeof(TsCaseBranch));
    }
    sb->branches[sb->branch_count].case_value = str_dup(case_val);
    sb->branches[sb->branch_count].result = str_dup(result);
    sb->branch_count++;
}

/* Scan a switch block: switch (var) { case 'x': ... case 'y': ... } */
static const char *scan_switch(const char *p, TsScanResult *r) {
    /* "switch" already confirmed */
    p += 6;
    p = skip_ws(p);

    if (*p != '(') return p;
    p++;
    p = skip_ws(p);

    /* Read switch variable */
    char var[256];
    size_t vlen = 0;
    /* Read until closing paren, collecting dotted identifiers */
    while (*p && *p != ')' && vlen < sizeof(var) - 1) {
        var[vlen++] = *p++;
    }
    var[vlen] = '\0';
    if (*p == ')') p++;
    p = skip_ws(p);

    if (*p != '{') return p;
    p++;

    add_switch(r);
    TsSwitchBlock *sb = &r->switches[r->switch_count - 1];
    sb->switch_var = strdup(var);

    /* Parse case labels */
    int depth = 1;
    while (*p && depth > 0) {
        p = skip_ws(p);
        if (*p == '{') { depth++; p++; continue; }
        if (*p == '}') { depth--; p++; continue; }

        if (word_match(p, "case")) {
            p += 4;
            p = skip_ws(p);
            char *case_val = NULL;
            if (*p == '\'' || *p == '"') {
                case_val = read_string_literal(&p);
            } else {
                char buf[256];
                size_t blen = read_ident(p, buf, sizeof(buf));
                if (blen > 0) {
                    case_val = strdup(buf);
                    p += blen;
                }
            }
            p = skip_ws(p);
            if (*p == ':') p++;

            /* Try to read the first return/result string */
            p = skip_ws(p);
            char *result = NULL;
            if (word_match(p, "return")) {
                p += 6;
                p = skip_ws(p);
                if (*p == '\'' || *p == '"') {
                    result = read_string_literal(&p);
                } else {
                    char rbuf[256];
                    size_t rlen = read_ident(p, rbuf, sizeof(rbuf));
                    if (rlen > 0) result = strdup(rbuf);
                }
            }

            if (case_val) {
                add_branch(sb, case_val, result);
                free(case_val);
            }
            free(result);
        } else if (word_match(p, "default")) {
            p += 7;
            p = skip_ws(p);
            if (*p == ':') p++;
            add_branch(sb, "_", NULL);
        } else {
            p++;
        }
    }

    return p;
}

/* ------------------------------------------------------------ */
/* Ternary chain extraction                                      */
/* ------------------------------------------------------------ */

/* Scan ternary chains: x === 'a' ? result1 : x === 'b' ? result2 : default
 * These appear as inline switch equivalents in many Spoxis files. */
static const char *scan_ternary_chain(const char *p, const char *var_name,
                                       TsScanResult *r) {
    /* We've detected: IDENT === 'VALUE' ? ... pattern */
    add_switch(r);
    TsSwitchBlock *sb = &r->switches[r->switch_count - 1];
    sb->switch_var = strdup(var_name);

    while (*p) {
        p = skip_ws(p);

        /* Check for: var === 'value' */
        char ident[256];
        size_t ilen = read_ident(p, ident, sizeof(ident));
        if (ilen == 0 || strcmp(ident, var_name) != 0) break;
        p += ilen;
        p = skip_ws(p);

        if (!(p[0] == '=' && p[1] == '=' && p[2] == '=')) break;
        p += 3;
        p = skip_ws(p);

        char *case_val = NULL;
        if (*p == '\'' || *p == '"') {
            case_val = read_string_literal(&p);
        } else break;

        p = skip_ws(p);
        if (*p != '?') { free(case_val); break; }
        p++;
        p = skip_ws(p);

        /* Read result (first string literal or identifier) */
        char *result = NULL;
        if (*p == '\'' || *p == '"') {
            result = read_string_literal(&p);
        } else {
            char rbuf[256];
            size_t rlen = read_ident(p, rbuf, sizeof(rbuf));
            if (rlen > 0) result = strdup(rbuf);
        }

        add_branch(sb, case_val, result);
        free(case_val);
        free(result);

        /* Skip to the ':' that separates ternary branches */
        int tdepth = 0;
        while (*p) {
            if (*p == '?' && tdepth == 0) {
                /* Nested ternary — skip to matching : */
                tdepth++;
                p++;
                continue;
            }
            if (*p == ':') {
                if (tdepth > 0) { tdepth--; p++; continue; }
                p++;
                break;
            }
            if (*p == '\'' || *p == '"') {
                char q = *p++;
                while (*p && *p != q) { if (*p == '\\') p++; if (*p) p++; }
                if (*p) p++;
                continue;
            }
            if (*p == '(' || *p == '[' || *p == '{') {
                char close = (*p == '(' ? ')' : *p == '[' ? ']' : '}');
                p = skip_balanced(p, *p, close);
                continue;
            }
            p++;
        }
        p = skip_ws(p);
    }

    return p;
}

/* ------------------------------------------------------------ */
/* Function extraction                                           */
/* ------------------------------------------------------------ */

static void add_function(TsScanResult *r, const char *name,
                          char **params, size_t param_count,
                          bool is_exported) {
    r->functions = realloc(r->functions, (r->function_count + 1) * sizeof(TsFunction));
    TsFunction *f = &r->functions[r->function_count];
    f->name = str_dup(name);
    f->params = malloc(param_count * sizeof(char *));
    for (size_t i = 0; i < param_count; i++) {
        f->params[i] = str_dup(params[i]);
    }
    f->param_count = param_count;
    f->is_exported = is_exported;
    r->function_count++;
}

/* Scan a function declaration or const arrow function */
static const char *scan_function(const char *p, TsScanResult *r,
                                  bool is_exported) {
    /* "function" already confirmed */
    p += 8;
    p = skip_ws(p);

    char name[256];
    size_t nlen = read_ident(p, name, sizeof(name));
    if (nlen == 0) return p;
    p += nlen;
    p = skip_ws(p);

    /* Skip generic type parameters <T, U> */
    if (*p == '<') {
        p = skip_balanced(p, '<', '>');
        p = skip_ws(p);
    }

    if (*p != '(') return p;
    p++;

    /* Read parameter names (skip types) */
    char *params[32];
    size_t param_count = 0;

    while (*p && *p != ')' && param_count < 32) {
        p = skip_ws(p);
        if (*p == ')') break;

        char pname[256];
        size_t plen = read_ident(p, pname, sizeof(pname));
        if (plen > 0) {
            params[param_count++] = strdup(pname);
            p += plen;
        }

        /* Skip type annotation and default value */
        while (*p && *p != ',' && *p != ')') {
            if (*p == '(' || *p == '{' || *p == '[') {
                char close = (*p == '(' ? ')' : *p == '{' ? '}' : ']');
                p = skip_balanced(p, *p, close);
            } else {
                p++;
            }
        }
        if (*p == ',') p++;
    }
    if (*p == ')') p++;

    add_function(r, name, params, param_count, is_exported);

    for (size_t i = 0; i < param_count; i++) free(params[i]);
    return p;
}

/* Scan a const arrow function: [export] const NAME = (params) => { ... } */
static const char *scan_arrow_function(const char *p, TsScanResult *r,
                                        bool is_exported) {
    /* Already past "const", at the name */
    char name[256];
    size_t nlen = read_ident(p, name, sizeof(name));
    if (nlen == 0) return p;
    p += nlen;
    p = skip_ws(p);

    /* Skip type annotation */
    if (*p == ':') {
        p++;
        while (*p && *p != '=' && *p != ';') {
            if (*p == '<') { p = skip_balanced(p, '<', '>'); continue; }
            if (*p == '(') { p = skip_balanced(p, '(', ')'); continue; }
            p++;
        }
        p = skip_ws(p);
    }

    if (*p != '=') return p;
    p++;
    p = skip_ws(p);

    if (*p != '(') return p;
    p++;

    /* Read parameter names */
    char *params[32];
    size_t param_count = 0;

    while (*p && *p != ')' && param_count < 32) {
        p = skip_ws(p);
        if (*p == ')') break;

        char pname[256];
        size_t plen = read_ident(p, pname, sizeof(pname));
        if (plen > 0) {
            params[param_count++] = strdup(pname);
            p += plen;
        }

        while (*p && *p != ',' && *p != ')') {
            if (*p == '(' || *p == '{' || *p == '[') {
                char close = (*p == '(' ? ')' : *p == '{' ? '}' : ']');
                p = skip_balanced(p, *p, close);
            } else {
                p++;
            }
        }
        if (*p == ',') p++;
    }
    if (*p == ')') p++;
    p = skip_ws(p);

    /* Skip return type annotation: ): Type => */
    if (*p == ':') {
        p++;
        while (*p && !(p[0] == '=' && p[1] == '>') && *p != '{' && *p != ';') {
            if (*p == '<') { p = skip_balanced(p, '<', '>'); continue; }
            p++;
        }
        p = skip_ws(p);
    }

    /* Look for => to confirm it's an arrow function */
    if (p[0] == '=' && p[1] == '>') {
        add_function(r, name, params, param_count, is_exported);
    }

    for (size_t i = 0; i < param_count; i++) free(params[i]);
    return p;
}

/* ------------------------------------------------------------ */
/* Main scanner                                                  */
/* ------------------------------------------------------------ */

TsScanResult ts_scan(const char *source, const char *filename) {
    TsScanResult r;
    memset(&r, 0, sizeof(r));
    r.filename = str_dup(filename);

    const char *p = source;

    while (*p) {
        p = skip_ws(p);
        if (!*p) break;

        bool is_exported = false;

        /* Check for 'export' keyword */
        if (word_match(p, "export")) {
            is_exported = true;
            p += 6;
            p = skip_ws(p);

            if (word_match(p, "default")) {
                p += 7;
                p = skip_ws(p);
            }
        }

        /* export const NAME = VALUE or export const NAME = (...) => */
        if (word_match(p, "const")) {
            const char *save = p;
            p += 5;
            p = skip_ws(p);

            char name[256];
            size_t nlen = read_ident(p, name, sizeof(name));
            if (nlen > 0) {
                const char *after_name = p + nlen;
                const char *aw = skip_ws(after_name);

                /* Skip type annotation */
                if (*aw == ':') {
                    aw++;
                    while (*aw && *aw != '=' && *aw != ';') {
                        if (*aw == '<') { aw = skip_balanced(aw, '<', '>'); continue; }
                        if (*aw == '(') { aw = skip_balanced(aw, '(', ')'); continue; }
                        aw++;
                    }
                    aw = skip_ws(aw);
                }

                if (*aw == '=') {
                    const char *after_eq = skip_ws(aw + 1);
                    /* Arrow function? */
                    if (*after_eq == '(') {
                        p = scan_arrow_function(p, &r, is_exported);
                        continue;
                    }
                }
            }

            /* Fall back to regular const extraction */
            p = save;
            if (is_exported) {
                /* Re-prepend "export " for scan_export_const */
                p = save - 7; /* back to "export " */
                while (*p && isspace((unsigned char)*p)) p--; /* but skip_ws moved us past, so... */
                /* Simpler: just call from const position */
                p = save;
                p += 5; /* past "const" */
                p = skip_ws(p);
                nlen = read_ident(p, name, sizeof(name));
                if (nlen > 0) {
                    p += nlen;
                    p = skip_ws(p);
                    if (*p == ':') {
                        p++;
                        while (*p && *p != '=' && *p != ';' && *p != '\n') p++;
                        p = skip_ws(p);
                    }
                    if (*p == '=') {
                        p++;
                        p = skip_ws(p);
                        if (*p == '\'' || *p == '"') {
                            char *val = read_string_literal(&p);
                            add_const(&r, name, val);
                            free(val);
                        } else if (isdigit((unsigned char)*p) ||
                                   (*p == '-' && isdigit((unsigned char)p[1]))) {
                            char *val = read_number(&p);
                            add_const(&r, name, val);
                            free(val);
                        }
                    }
                }
            } else {
                /* Non-exported const — skip */
                while (*p && *p != '\n' && *p != ';') p++;
            }
            continue;
        }

        /* function NAME(...) { ... } */
        if (word_match(p, "function")) {
            p = scan_function(p, &r, is_exported);
            continue;
        }

        /* switch (expr) { ... } */
        if (word_match(p, "switch")) {
            p = scan_switch(p, &r);
            continue;
        }

        /* Ternary chain detection: IDENT === 'VALUE' ? ... */
        {
            char ident[256];
            size_t ilen = read_ident(p, ident, sizeof(ident));
            if (ilen > 0) {
                const char *after = skip_ws(p + ilen);
                if (after[0] == '=' && after[1] == '=' && after[2] == '=') {
                    const char *val_pos = skip_ws(after + 3);
                    if (*val_pos == '\'' || *val_pos == '"') {
                        p = scan_ternary_chain(p, ident, &r);
                        continue;
                    }
                }
            }
        }

        /* Advance past current token — DON'T skip {} blocks
         * so we can find switches inside function bodies */
        if (*p == '\'' || *p == '"' || *p == '`') {
            char q = *p++;
            while (*p && *p != q) { if (*p == '\\') p++; if (*p) p++; }
            if (*p) p++;
        } else {
            p++;
        }
    }

    return r;
}

/* ------------------------------------------------------------ */
/* Cleanup                                                       */
/* ------------------------------------------------------------ */

void ts_scan_free(TsScanResult *r) {
    free(r->filename);

    for (size_t i = 0; i < r->const_count; i++) {
        free(r->consts[i].name);
        free(r->consts[i].value);
    }
    free(r->consts);

    for (size_t i = 0; i < r->switch_count; i++) {
        free(r->switches[i].switch_var);
        for (size_t j = 0; j < r->switches[i].branch_count; j++) {
            free(r->switches[i].branches[j].case_value);
            free(r->switches[i].branches[j].result);
        }
        free(r->switches[i].branches);
    }
    free(r->switches);

    for (size_t i = 0; i < r->function_count; i++) {
        free(r->functions[i].name);
        for (size_t j = 0; j < r->functions[i].param_count; j++) {
            free(r->functions[i].params[j]);
        }
        free(r->functions[i].params);
    }
    free(r->functions);
}

/* ------------------------------------------------------------ */
/* Debug print                                                   */
/* ------------------------------------------------------------ */

void ts_scan_print(const TsScanResult *r) {
    printf("TS SCAN: %s\n", r->filename ? r->filename : "<unknown>");
    printf("  consts: %zu\n", r->const_count);
    for (size_t i = 0; i < r->const_count; i++) {
        printf("    %s = %s\n", r->consts[i].name,
               r->consts[i].value ? r->consts[i].value : "<complex>");
    }
    printf("  switch blocks: %zu\n", r->switch_count);
    for (size_t i = 0; i < r->switch_count; i++) {
        printf("    switch(%s) — %zu branches\n",
               r->switches[i].switch_var ? r->switches[i].switch_var : "?",
               r->switches[i].branch_count);
        for (size_t j = 0; j < r->switches[i].branch_count; j++) {
            printf("      case '%s' → %s\n",
                   r->switches[i].branches[j].case_value,
                   r->switches[i].branches[j].result ?
                       r->switches[i].branches[j].result : "...");
        }
    }
    printf("  functions: %zu\n", r->function_count);
    for (size_t i = 0; i < r->function_count; i++) {
        printf("    %s%s(",
               r->functions[i].is_exported ? "export " : "",
               r->functions[i].name);
        for (size_t j = 0; j < r->functions[i].param_count; j++) {
            if (j > 0) printf(", ");
            printf("%s", r->functions[i].params[j]);
        }
        printf(")\n");
    }
}
