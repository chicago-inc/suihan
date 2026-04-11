/*
 * suhc — py_scanner.c
 * Lightweight Python scanner for drift detection.
 *
 * Extracts structural patterns from Python that correspond to
 * ordbok violations. Not a Python parser — intentional S > 0
 * for complex expressions (D13 applied to the tool itself).
 *
 * RAE audit findings → scanner targets:
 *   F01 VERBS dict         → dict literal extraction
 *   F02 mass noun sets     → frozenset/set extraction
 *   F03 CLOSED_CLASS       → frozenset extraction
 *   F07 if-chain on string → elif chain extraction
 *   F08 _PREPS frozenset   → frozenset extraction
 *   F09 CATEGORY_RELS etc  → set/frozenset extraction
 *   F11 hardcoded 0.6/0.8  → numeric threshold extraction
 *   F12 >100 / >50         → numeric threshold extraction
 */

#include "py_scanner.h"
#include "compat.h"
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

static const char *skip_ws_inline(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static const char *skip_to_eol(const char *p) {
    while (*p && *p != '\n') p++;
    return p;
}

static size_t read_ident(const char *p, char *buf, size_t bufsz) {
    size_t i = 0;
    if (!isalpha((unsigned char)*p) && *p != '_') return 0;
    while (i < bufsz - 1 && (isalnum((unsigned char)p[i]) || p[i] == '_')) {
        buf[i] = p[i];
        i++;
    }
    buf[i] = '\0';
    return i;
}

static char *read_string_literal(const char **pp) {
    const char *p = *pp;
    char quote = *p;
    if (quote != '\'' && quote != '"') return NULL;
    p++;
    char buf[1024];
    size_t i = 0;
    while (*p && *p != quote && *p != '\n' && i < sizeof(buf) - 1) {
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

static const char *skip_balanced(const char *p, char open, char close) {
    if (*p != open) return p;
    int depth = 1;
    p++;
    while (*p && depth > 0) {
        if (*p == open) depth++;
        else if (*p == close) depth--;
        else if (*p == '\'' || *p == '"') {
            char q = *p++;
            while (*p && *p != q && *p != '\n') {
                if (*p == '\\') p++;
                if (*p) p++;
            }
            if (*p == q) p++;
            continue;
        } else if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (depth > 0) p++;
    }
    return p;
}

static bool word_match(const char *p, const char *word) {
    size_t len = strlen(word);
    if (strncmp(p, word, len) != 0) return false;
    if (isalnum((unsigned char)p[len]) || p[len] == '_') return false;
    return true;
}

/* ------------------------------------------------------------ */
/* Result helpers (reuse TsScanResult structures)                */
/* ------------------------------------------------------------ */

static void add_const(TsScanResult *r, const char *name, const char *value) {
    r->consts = realloc(r->consts, (r->const_count + 1) * sizeof(TsConst));
    r->consts[r->const_count].name = str_dup(name);
    r->consts[r->const_count].value = str_dup(value);
    r->const_count++;
}

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

static void add_branch(TsSwitchBlock *sb, const char *val, const char *result) {
    if (sb->branch_count >= sb->branch_capacity) {
        sb->branch_capacity *= 2;
        sb->branches = realloc(sb->branches, sb->branch_capacity * sizeof(TsCaseBranch));
    }
    sb->branches[sb->branch_count].case_value = str_dup(val);
    sb->branches[sb->branch_count].result = str_dup(result);
    sb->branch_count++;
}

static void add_function(TsScanResult *r, const char *name,
                          char **params, size_t param_count,
                          bool is_exported) {
    r->functions = realloc(r->functions, (r->function_count + 1) * sizeof(TsFunction));
    TsFunction *f = &r->functions[r->function_count];
    f->name = str_dup(name);
    f->params = malloc((param_count ? param_count : 1) * sizeof(char *));
    for (size_t i = 0; i < param_count; i++) {
        f->params[i] = str_dup(params[i]);
    }
    f->param_count = param_count;
    f->is_exported = is_exported;
    r->function_count++;
}

/* ------------------------------------------------------------ */
/* Set/frozenset extraction                                      */
/* Captures: NAME = frozenset({...}) or NAME = {...} (set)       */
/* Maps to: TsSwitchBlock with each element as a branch          */
/* ------------------------------------------------------------ */

static const char *scan_set_literal(const char *p, const char *name,
                                     TsScanResult *r) {
    /* p points to '{' */
    add_switch(r);
    TsSwitchBlock *sb = &r->switches[r->switch_count - 1];
    sb->switch_var = strdup(name);

    p++; /* past '{' */
    while (*p && *p != '}') {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',') p++;
        if (*p == '#') { p = skip_to_eol(p); continue; }
        if (*p == '}') break;

        if (*p == '\'' || *p == '"') {
            char *val = read_string_literal(&p);
            if (val) {
                add_branch(sb, val, NULL);
                free(val);
            }
        } else {
            /* Non-string element — read identifier */
            char buf[256];
            size_t blen = read_ident(p, buf, sizeof(buf));
            if (blen > 0) {
                add_branch(sb, buf, NULL);
                p += blen;
            } else {
                p++;
            }
        }
    }
    if (*p == '}') p++;
    if (*p == ')') p++; /* closing paren of frozenset() */
    return p;
}

/* ------------------------------------------------------------ */
/* Dict extraction                                               */
/* Captures: NAME = {'key': 'value', ...}                        */
/* Maps to: TsSwitchBlock with key→value as branches             */
/* ------------------------------------------------------------ */

static const char *scan_dict_literal(const char *p, const char *name,
                                      TsScanResult *r) {
    /* p points to '{' */
    add_switch(r);
    TsSwitchBlock *sb = &r->switches[r->switch_count - 1];
    sb->switch_var = strdup(name);

    p++; /* past '{' */
    while (*p && *p != '}') {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',') p++;
        if (*p == '#') { p = skip_to_eol(p); continue; }
        if (*p == '}') break;

        char *key = NULL;
        if (*p == '\'' || *p == '"') {
            key = read_string_literal(&p);
        } else {
            char buf[256];
            size_t blen = read_ident(p, buf, sizeof(buf));
            if (blen > 0) { key = strdup(buf); p += blen; }
            else { p++; continue; }
        }

        /* Skip to colon */
        while (*p && *p != ':' && *p != '}') p++;
        if (*p == ':') p++;
        while (*p == ' ' || *p == '\t') p++;

        char *val = NULL;
        if (*p == '\'' || *p == '"') {
            val = read_string_literal(&p);
        } else {
            char buf[256];
            size_t blen = read_ident(p, buf, sizeof(buf));
            if (blen > 0) { val = strdup(buf); p += blen; }
        }

        if (key) {
            add_branch(sb, key, val);
            free(key);
        }
        free(val);

        /* Skip to comma or closing brace */
        while (*p && *p != ',' && *p != '}' && *p != '\n') {
            if (*p == '{') { p = skip_balanced(p, '{', '}'); continue; }
            if (*p == '(') { p = skip_balanced(p, '(', ')'); continue; }
            if (*p == '[') { p = skip_balanced(p, '[', ']'); continue; }
            p++;
        }
    }
    if (*p == '}') p++;
    return p;
}

/* ------------------------------------------------------------ */
/* If/elif chain extraction                                      */
/* Captures: if x == 'a': ... elif x == 'b': ...                 */
/* Maps to: TsSwitchBlock (same as switch/ternary in TS)         */
/* ------------------------------------------------------------ */

static const char *scan_elif_chain(const char *p, TsScanResult *r) {
    /* p points to "if" */
    p += 2;
    p = skip_ws_inline(p);

    /* Read the variable being compared */
    char var[256];
    size_t vlen = read_ident(p, var, sizeof(var));
    if (vlen == 0) return skip_to_eol(p);
    p += vlen;

    /* Check for dotted access: var.attr */
    while (*p == '.') {
        size_t pos = vlen;
        if (pos < sizeof(var) - 1) var[pos++] = '.';
        p++;
        char attr[256];
        size_t alen = read_ident(p, attr, sizeof(attr));
        for (size_t i = 0; i < alen && pos < sizeof(var) - 1; i++)
            var[pos++] = attr[i];
        var[pos] = '\0';
        vlen = pos;
        p += alen;
    }

    p = skip_ws_inline(p);

    /* Must be == comparison */
    if (!(p[0] == '=' && p[1] == '=')) return skip_to_eol(p);
    p += 2;
    p = skip_ws_inline(p);

    /* Must compare to string literal */
    if (*p != '\'' && *p != '"') return skip_to_eol(p);

    add_switch(r);
    TsSwitchBlock *sb = &r->switches[r->switch_count - 1];
    sb->switch_var = strdup(var);

    /* First case */
    char *val = read_string_literal(&p);
    if (val) {
        add_branch(sb, val, NULL);
        free(val);
    }

    /* Scan for elif with same variable */
    p = skip_to_eol(p);
    while (*p) {
        if (*p == '\n') p++;
        const char *line = skip_ws_inline(p);

        if (word_match(line, "elif")) {
            line += 4;
            line = skip_ws_inline(line);
            char evar[256];
            size_t evlen = read_ident(line, evar, sizeof(evar));
            if (evlen == 0) break;
            line += evlen;
            /* Dotted access */
            while (*line == '.') {
                size_t pos = evlen;
                if (pos < sizeof(evar) - 1) evar[pos++] = '.';
                line++;
                char attr[256];
                size_t alen = read_ident(line, attr, sizeof(attr));
                for (size_t i = 0; i < alen && pos < sizeof(evar) - 1; i++)
                    evar[pos++] = attr[i];
                evar[pos] = '\0';
                evlen = pos;
                line += alen;
            }
            line = skip_ws_inline(line);

            if (line[0] == '=' && line[1] == '=' && strcmp(evar, var) == 0) {
                line += 2;
                line = skip_ws_inline(line);
                if (*line == '\'' || *line == '"') {
                    val = read_string_literal(&line);
                    if (val) {
                        add_branch(sb, val, NULL);
                        free(val);
                    }
                }
            } else break;
            p = skip_to_eol(line);
        } else if (word_match(line, "else")) {
            add_branch(sb, "_", NULL);
            p = skip_to_eol(line);
            break;
        } else {
            break;
        }
    }

    return p;
}

/* ------------------------------------------------------------ */
/* Function/class extraction                                     */
/* ------------------------------------------------------------ */

static const char *scan_def(const char *p, TsScanResult *r) {
    p += 3; /* past "def" */
    p = skip_ws_inline(p);

    char name[256];
    size_t nlen = read_ident(p, name, sizeof(name));
    if (nlen == 0) return skip_to_eol(p);
    p += nlen;
    p = skip_ws_inline(p);

    if (*p != '(') return skip_to_eol(p);
    p++;

    char *params[32];
    size_t param_count = 0;

    while (*p && *p != ')' && param_count < 32) {
        p = skip_ws_inline(p);
        if (*p == ')') break;
        if (*p == '*' || *p == '/') { p++; if (*p == ',') p++; continue; }

        char pname[256];
        size_t plen = read_ident(p, pname, sizeof(pname));
        if (plen > 0) {
            /* Skip 'self' and 'cls' */
            if (strcmp(pname, "self") != 0 && strcmp(pname, "cls") != 0) {
                params[param_count++] = strdup(pname);
            }
            p += plen;
        }

        /* Skip type annotation and default */
        while (*p && *p != ',' && *p != ')') {
            if (*p == '(') { p = skip_balanced(p, '(', ')'); continue; }
            if (*p == '[') { p = skip_balanced(p, '[', ']'); continue; }
            p++;
        }
        if (*p == ',') p++;
    }
    if (*p == ')') p++;

    add_function(r, name, params, param_count, false);
    for (size_t i = 0; i < param_count; i++) free(params[i]);
    return skip_to_eol(p);
}

static const char *scan_class(const char *p, TsScanResult *r) {
    p += 5; /* past "class" */
    p = skip_ws_inline(p);

    char name[256];
    size_t nlen = read_ident(p, name, sizeof(name));
    if (nlen == 0) return skip_to_eol(p);

    add_function(r, name, NULL, 0, true);
    return skip_to_eol(p);
}

/* ------------------------------------------------------------ */
/* Main scanner                                                  */
/* ------------------------------------------------------------ */

TsScanResult py_scan(const char *source, const char *filename) {
    TsScanResult r;
    memset(&r, 0, sizeof(r));
    r.filename = str_dup(filename);

    const char *p = source;

    while (*p) {
        /* Skip blank lines and comments */
        const char *line_start = p;
        p = skip_ws_inline(p);

        if (*p == '#') { p = skip_to_eol(p); if (*p) p++; continue; }
        if (*p == '\n') { p++; continue; }
        if (!*p) break;

        /* class Name: */
        if (word_match(p, "class")) {
            p = scan_class(p, &r);
            if (*p == '\n') p++;
            continue;
        }

        /* def name(params): */
        if (word_match(p, "def")) {
            p = scan_def(p, &r);
            if (*p == '\n') p++;
            continue;
        }

        /* if var == 'string': (elif chain) */
        if (word_match(p, "if")) {
            /* Peek ahead to see if this is string comparison */
            const char *peek = p + 2;
            peek = skip_ws_inline(peek);
            char pvar[256];
            size_t pvlen = read_ident(peek, pvar, sizeof(pvar));
            if (pvlen > 0) {
                peek += pvlen;
                while (*peek == '.') {
                    peek++;
                    char attr[256];
                    size_t alen = read_ident(peek, attr, sizeof(attr));
                    peek += alen;
                }
                peek = skip_ws_inline(peek);
                if (peek[0] == '=' && peek[1] == '=') {
                    peek += 2;
                    peek = skip_ws_inline(peek);
                    if (*peek == '\'' || *peek == '"') {
                        p = scan_elif_chain(p, &r);
                        continue;
                    }
                }
            }
            p = skip_to_eol(p);
            if (*p == '\n') p++;
            continue;
        }

        /* UPPER_NAME = frozenset({...}) or NAME = {...} or NAME = number */
        {
            char name[256];
            size_t nlen = read_ident(p, name, sizeof(name));
            if (nlen > 0) {
                const char *after = p + nlen;
                after = skip_ws_inline(after);

                if (*after == '=') {
                    after++;
                    after = skip_ws_inline(after);

                    /* frozenset({...}) */
                    if (word_match(after, "frozenset")) {
                        after += 9;
                        after = skip_ws_inline(after);
                        if (*after == '(') {
                            after++;
                            after = skip_ws_inline(after);
                            if (*after == '{') {
                                p = scan_set_literal(after, name, &r);
                                if (*p == '\n') p++;
                                continue;
                            }
                        }
                    }

                    /* set({...}) — explicit set constructor */
                    if (word_match(after, "set")) {
                        const char *s = after + 3;
                        s = skip_ws_inline(s);
                        if (*s == '(') {
                            s++;
                            s = skip_ws_inline(s);
                            if (*s == '{') {
                                p = scan_set_literal(s, name, &r);
                                if (*p == '\n') p++;
                                continue;
                            }
                        }
                    }

                    /* {key: value, ...} dict literal */
                    if (*after == '{') {
                        /* Peek to distinguish dict from set */
                        const char *peek = after + 1;
                        while (*peek == ' ' || *peek == '\t' || *peek == '\n') peek++;
                        if (*peek == '\'' || *peek == '"') {
                            const char *q = peek;
                            char quote = *q++;
                            while (*q && *q != quote) { if (*q == '\\') q++; q++; }
                            if (*q == quote) q++;
                            while (*q == ' ' || *q == '\t') q++;
                            if (*q == ':') {
                                /* It's a dict */
                                p = scan_dict_literal(after, name, &r);
                                if (*p == '\n') p++;
                                continue;
                            } else if (*q == ',' || *q == '}') {
                                /* It's a set literal */
                                p = scan_set_literal(after, name, &r);
                                if (*p == '\n') p++;
                                continue;
                            }
                        }
                        /* Non-string key or identifier key — check for colon */
                        if (isalpha((unsigned char)*peek) || *peek == '_') {
                            char kbuf[256];
                            size_t klen = read_ident(peek, kbuf, sizeof(kbuf));
                            const char *kafter = peek + klen;
                            while (*kafter == ' ' || *kafter == '\t') kafter++;
                            if (*kafter == ':') {
                                p = scan_dict_literal(after, name, &r);
                                if (*p == '\n') p++;
                                continue;
                            } else {
                                p = scan_set_literal(after, name, &r);
                                if (*p == '\n') p++;
                                continue;
                            }
                        }
                    }

                    /* Numeric constant: NAME = 42 or NAME = 0.6
                     * Only flag as hardcoded if the ENTIRE RHS is a bare
                     * number literal. If there's an expression (function call,
                     * arithmetic, ternary), the value is computed — skip it.
                     * This prevents false positives on:
                     *   threshold_mult = max(0.4, min(1.0, 1.0 - cv))
                     *   S_before = 1.0 - (x / y) if y > 0 else 1.0
                     */
                    if (isdigit((unsigned char)*after) ||
                        (*after == '-' && isdigit((unsigned char)after[1])) ||
                        (*after == '.' && isdigit((unsigned char)after[1]))) {
                        char num[64];
                        size_t ni = 0;
                        const char *np = after;
                        if (*np == '-') num[ni++] = *np++;
                        while (ni < sizeof(num) - 1 &&
                               (isdigit((unsigned char)*np) || *np == '.')) {
                            num[ni++] = *np++;
                        }
                        num[ni] = '\0';

                        /* Check: is the rest of the line empty/comment?
                         * If so, it's a bare literal (hardcoded).
                         * If there's more content, it's part of an expression. */
                        const char *rest = np;
                        while (*rest == ' ' || *rest == '\t') rest++;
                        bool is_bare = (*rest == '\n' || *rest == '\r' ||
                                        *rest == '#' || *rest == '\0');
                        if (is_bare) {
                            add_const(&r, name, num);
                        }
                        p = skip_to_eol(np);
                        if (*p == '\n') p++;
                        continue;
                    }
                }
            }
        }

        /* ── Inline pattern detection (scans current line) ── */
        /* These detect patterns WITHIN lines, not just top-level assignments */
        {
            const char *line = p;
            const char *eol = line;
            while (*eol && *eol != '\n') eol++;
            size_t line_len = (size_t)(eol - line);

            /* 1. Inline set() construction: set('aeiou'), set([...])
             * Captures as a switch block with individual characters/items */
            const char *sp = line;
            while ((sp = strstr(sp, "set(")) != NULL && sp < eol) {
                if (sp > line && (isalnum((unsigned char)sp[-1]) || sp[-1] == '_')) {
                    sp += 4; continue;  /* frozenset already handled above */
                }
                const char *arg = sp + 4;
                if (*arg == '\'' || *arg == '"') {
                    /* set('chars') — each character becomes a branch */
                    char *str_val = read_string_literal(&arg);
                    if (str_val) {
                        add_switch(&r);
                        TsSwitchBlock *sb = &r.switches[r.switch_count - 1];
                        sb->switch_var = strdup("inline_set");
                        for (size_t ci = 0; str_val[ci]; ci++) {
                            char ch[2] = {str_val[ci], '\0'};
                            add_branch(sb, ch, NULL);
                        }
                        free(str_val);
                    }
                }
                sp = arg;
            }

            /* 2. Method calls with string arguments:
             * .split(), .lower(), .strip('...'), .endswith('...')
             * Each unique method+arg combo becomes a const entry */
            const char *mp = line;
            while (mp < eol) {
                if (*mp == '.') {
                    mp++;
                    char method[64];
                    size_t mlen = read_ident(mp, method, sizeof(method));
                    if (mlen > 0) {
                        mp += mlen;
                        if (*mp == '(') {
                            const char *args_start = mp + 1;
                            /* For key methods, record as a const */
                            if (strcmp(method, "split") == 0 ||
                                strcmp(method, "lower") == 0 ||
                                strcmp(method, "strip") == 0 ||
                                strcmp(method, "endswith") == 0 ||
                                strcmp(method, "startswith") == 0) {
                                const char *ap = args_start;
                                while (*ap == ' ' || *ap == '\t') ap++;
                                char name_buf[128];
                                snprintf(name_buf, sizeof(name_buf),
                                         "method_%s", method);
                                if (*ap == '\'' || *ap == '"') {
                                    char *arg_val = read_string_literal(&ap);
                                    if (arg_val) {
                                        add_const(&r, name_buf, arg_val);
                                        free(arg_val);
                                    }
                                } else if (*ap == '(') {
                                    /* Tuple arg like .startswith(('a', 'b'))
                                     * Record the method call with "tuple" marker */
                                    add_const(&r, name_buf, "(tuple)");
                                } else if (*ap == ')') {
                                    /* No-arg call like .split() or .lower() */
                                    add_const(&r, name_buf, "");
                                } else {
                                    /* Variable arg — still record the call */
                                    add_const(&r, name_buf, "(var)");
                                }
                            }
                        }
                    }
                } else {
                    mp++;
                }
            }

            /* 3. Hardcoded numeric comparisons:
             * >= 3, > 100, < 0.5, == 42, != 0
             * Captures the threshold value as a const */
            const char *cp = line;
            while (cp < eol - 2) {
                bool is_cmp = false;
                const char *after_op = NULL;
                if (cp[0] == '>' && cp[1] == '=') { is_cmp = true; after_op = cp + 2; }
                else if (cp[0] == '<' && cp[1] == '=') { is_cmp = true; after_op = cp + 2; }
                else if (cp[0] == '>' && cp[1] != '>') { is_cmp = true; after_op = cp + 1; }
                else if (cp[0] == '<' && cp[1] != '<') { is_cmp = true; after_op = cp + 1; }

                if (is_cmp && after_op) {
                    while (*after_op == ' ' || *after_op == '\t') after_op++;
                    if (isdigit((unsigned char)*after_op) ||
                        (*after_op == '-' && isdigit((unsigned char)after_op[1]))) {
                        char num[32];
                        size_t ni = 0;
                        const char *np2 = after_op;
                        if (*np2 == '-') num[ni++] = *np2++;
                        while (ni < sizeof(num) - 1 &&
                               (isdigit((unsigned char)*np2) || *np2 == '.')) {
                            num[ni++] = *np2++;
                        }
                        num[ni] = '\0';
                        add_const(&r, "threshold_comparison", num);
                        cp = np2;
                        continue;
                    }
                }
                cp++;
            }
        }

        /* Advance to next line */
        p = skip_to_eol(p);
        if (*p == '\n') p++;
    }

    return r;
}
