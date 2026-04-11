/*
 * suhc — emit_c.c
 * C11 code generation from .szh declarations.
 *
 * Emission rules: the C emitter produces a single .h file containing:
 *   ξ → static const / #define (immutable identity values)
 *   dimension → enum + string conversion function
 *   unit/magnitude/vector → typedef'd numeric types
 *   dependency → struct typedef
 *   meihua → static inline pure function (double params)
 *   zhulin → static inline control flow function
 *   songqiao → static inline config function (returns const char *)
 *   projection → switch-based resolver function
 *   traversal → documented function stub
 *   morphism → documented function stub
 *
 * The emitted C is a self-contained header — include it and use it.
 * This is not a general-purpose C transpiler; it emits the declarative
 * and pure-computational portions of .szh files as C constructs that
 * can govern a hand-written C layer (such as the compiler itself).
 */

#include "emitter.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* M5: math function enum + name mappings (generated) */
#include "gen/math_fns.h"
#include "gen/math_fn_c.h"

/* ------------------------------------------------------------ */
/* Dimension registry — tracks known dimensions for enum-aware   */
/* projection emission (M2: string-table projections)            */
/* ------------------------------------------------------------ */

typedef struct {
    const char *name;       /* dimension name, e.g. "kind" */
    const char *prefix;     /* SCREAMING prefix, e.g. "KIND" */
    const char **members;   /* member names in declaration order */
    size_t member_count;
} DimInfo;

#define MAX_DIMS 64
static DimInfo dim_registry[MAX_DIMS];
static size_t dim_registry_count = 0;

static void dim_registry_clear(void) {
    for (size_t i = 0; i < dim_registry_count; i++) {
        free((void *)dim_registry[i].prefix);
        free(dim_registry[i].members);
    }
    dim_registry_count = 0;
}

static void dim_registry_add(Decl *d);  /* forward decl — needs to_screaming */

static const DimInfo *dim_registry_find(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < dim_registry_count; i++) {
        if (strcmp(dim_registry[i].name, name) == 0)
            return &dim_registry[i];
    }
    return NULL;
}

/* ------------------------------------------------------------ */
/* String utilities                                              */
/* ------------------------------------------------------------ */

/* SCREAMING_SNAKE from snake_case: structural_role → STRUCTURAL_ROLE */
static char *to_screaming(const char *snake) {
    if (!snake) return strdup("UNKNOWN");
    size_t len = strlen(snake);
    char *out = malloc(len + 1);
    for (size_t i = 0; i < len; i++)
        out[i] = toupper((unsigned char)snake[i]);
    out[len] = '\0';
    return out;
}

/* Sanitize a name for C identifier use (replace non-alnum with _) */
static char *c_ident(const char *name) {
    if (!name) return strdup("unknown");
    size_t len = strlen(name);
    char *out = malloc(len + 1);
    for (size_t i = 0; i < len; i++)
        out[i] = isalnum((unsigned char)name[i]) ? name[i] : '_';
    out[len] = '\0';
    return out;
}

/* ------------------------------------------------------------ */
/* Dimension registry population                                 */
/* ------------------------------------------------------------ */

static void dim_registry_add(Decl *d) {
    if (!d || d->type != DECL_DIMENSION || !d->name.text) return;
    if (dim_registry_count >= MAX_DIMS) return;
    if (dim_registry_find(d->name.text)) return;  /* already registered */

    Expr *members = d->as.dimension.members;
    if (!members) return;

    const char *mem_names[256];
    size_t count = 0;

    if (members->type == EXPR_ENUM) {
        for (size_t i = 0; i < members->as.enumeration.count && count < 256; i++) {
            if (members->as.enumeration.items[i].name.text)
                mem_names[count++] = members->as.enumeration.items[i].name.text;
        }
    } else if (members->type == EXPR_LIST) {
        for (size_t i = 0; i < members->as.list.count && count < 256; i++) {
            Expr *item = members->as.list.items[i];
            if (item && item->type == EXPR_IDENT && item->as.ident.name)
                mem_names[count++] = item->as.ident.name;
        }
    }

    if (count == 0) return;

    DimInfo *info = &dim_registry[dim_registry_count++];
    info->name = d->name.text;  /* borrowed from AST, lives long enough */
    info->prefix = to_screaming(d->name.text);
    info->member_count = count;
    info->members = malloc(count * sizeof(const char *));
    for (size_t i = 0; i < count; i++)
        info->members[i] = mem_names[i];  /* borrowed from AST */
}

/* ------------------------------------------------------------ */
/* Header guard and preamble                                     */
/* ------------------------------------------------------------ */

static void emit_c_header(FILE *out, const char *filename) {
    /* Derive guard name from filename */
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    char *guard = to_screaming(base);
    /* Replace . with _ for the guard */
    for (char *p = guard; *p; p++)
        if (*p == '.') *p = '_';

    fprintf(out, "/*\n");
    fprintf(out, " * Generated by suhc (the suihan compiler) from %s\n",
            filename ? filename : "<stdin>");
    fprintf(out, " * DO NOT EDIT — regenerate from the .szh source.\n");
    fprintf(out, " *\n");
    fprintf(out, " * This is a self-contained C11 header.\n");
    fprintf(out, " * Include it in exactly one translation unit, or use\n");
    fprintf(out, " * static inline functions (already the default).\n");
    fprintf(out, " */\n\n");
    fprintf(out, "#ifndef SUHC_GEN_%s\n", guard);
    fprintf(out, "#define SUHC_GEN_%s\n\n", guard);
    fprintf(out, "#include <math.h>\n");
    fprintf(out, "#include <stddef.h>\n");
    fprintf(out, "#include <stdbool.h>\n");
    fprintf(out, "#include <string.h>\n\n");
    free(guard);
}

static void emit_c_footer(FILE *out, const char *filename) {
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    char *guard = to_screaming(base);
    for (char *p = guard; *p; p++)
        if (*p == '.') *p = '_';
    fprintf(out, "#endif /* SUHC_GEN_%s */\n", guard);
    free(guard);
}

/* ------------------------------------------------------------ */
/* ξ → #define / static const                                    */
/* ------------------------------------------------------------ */

static void emit_c_xi(FILE *out, Decl *d) {
    if (!d->name.text) return;
    char *upper = to_screaming(d->name.text);

    if (d->as.kinded.value) {
        Expr *val = d->as.kinded.value;
        if (val->type == EXPR_NUMBER && val->as.number.text) {
            fprintf(out, "#define %s %s\n", upper, val->as.number.text);
        } else if (val->type == EXPR_STRING && val->as.string.value) {
            fprintf(out, "static const char *%s = \"%s\";\n",
                    upper, val->as.string.value);
        } else if (val->type == EXPR_IDENT && val->as.ident.name) {
            fprintf(out, "/* ξ %s = %s */\n", d->name.text, val->as.ident.name);
        } else if (val->type == EXPR_ENUM) {
            /* ξ enum → C enum */
            char *id = c_ident(d->name.text);
            fprintf(out, "typedef enum {\n");
            for (size_t i = 0; i < val->as.enumeration.count; i++) {
                const char *member = val->as.enumeration.items[i].name.text;
                if (member) {
                    char *mu = to_screaming(member);
                    fprintf(out, "    %s_%s", upper, mu);
                    if (i + 1 < val->as.enumeration.count)
                        fprintf(out, ",");
                    fprintf(out, "\n");
                    free(mu);
                }
            }
            fprintf(out, "} %s_t;\n\n", id);
            free(id);
        } else {
            fprintf(out, "/* ξ %s (complex expression) */\n", d->name.text);
        }
    } else {
        fprintf(out, "/* ξ %s (no value) */\n", d->name.text);
    }

    free(upper);
}

/* ------------------------------------------------------------ */
/* dimension → enum + string function                            */
/* ------------------------------------------------------------ */

static void emit_c_dimension(FILE *out, Decl *d) {
    if (!d->name.text) return;
    Expr *members = d->as.dimension.members;
    if (!members) return;

    char *upper = to_screaming(d->name.text);
    char *id = c_ident(d->name.text);

    /* Collect member names — dimensions use EXPR_ENUM (a | b | c) */
    const char *names[256];
    size_t count = 0;

    if (members->type == EXPR_ENUM) {
        for (size_t i = 0; i < members->as.enumeration.count && count < 256; i++) {
            if (members->as.enumeration.items[i].name.text)
                names[count++] = members->as.enumeration.items[i].name.text;
        }
    } else if (members->type == EXPR_LIST) {
        for (size_t i = 0; i < members->as.list.count && count < 256; i++) {
            Expr *item = members->as.list.items[i];
            if (item && item->type == EXPR_IDENT && item->as.ident.name)
                names[count++] = item->as.ident.name;
        }
    }

    /* Skip if no members found (avoid empty enum, which is invalid C) */
    if (count == 0) {
        fprintf(out, "/* dimension %s (no members extracted) */\n\n",
                d->name.text);
        free(upper);
        free(id);
        return;
    }

    /* Enum declaration */
    fprintf(out, "/* dimension %s */\n", d->name.text);
    fprintf(out, "typedef enum {\n");
    for (size_t i = 0; i < count; i++) {
        char *mu = to_screaming(names[i]);
        fprintf(out, "    %s_%s", upper, mu);
        if (i + 1 < count) fprintf(out, ",");
        fprintf(out, "\n");
        free(mu);
    }
    fprintf(out, "} %s_t;\n\n", id);

    /* String conversion function (enum → string) */
    fprintf(out, "static inline const char *%s_to_str(%s_t val) {\n",
            id, id);
    fprintf(out, "    switch (val) {\n");
    for (size_t i = 0; i < count; i++) {
        char *mu = to_screaming(names[i]);
        fprintf(out, "    case %s_%s: return \"%s\";\n", upper, mu, names[i]);
        free(mu);
    }
    fprintf(out, "    }\n");
    fprintf(out, "    return \"unknown\";\n");
    fprintf(out, "}\n\n");

    /* M5: reverse lookup function (string → enum) */
    fprintf(out, "static inline %s_t %s_from_name(const char *name) {\n",
            id, id);
    for (size_t i = 0; i < count; i++) {
        char *mu = to_screaming(names[i]);
        fprintf(out, "    if (strcmp(name, \"%s\") == 0) return %s_%s;\n",
                names[i], upper, mu);
        free(mu);
    }
    fprintf(out, "    return (%s_t)-1;\n", id);
    fprintf(out, "}\n\n");

    free(upper);
    free(id);
}

/* ------------------------------------------------------------ */
/* unit/magnitude/vector → typedef                               */
/* ------------------------------------------------------------ */

static void emit_c_unit_type(FILE *out, Decl *d, const char *kind_label) {
    if (!d->name.text) return;
    char *id = c_ident(d->name.text);
    fprintf(out, "/* %s %s */\n", kind_label, d->name.text);
    fprintf(out, "typedef double %s_t;\n\n", id);
    free(id);
}

/* ------------------------------------------------------------ */
/* dependency → struct                                           */
/* ------------------------------------------------------------ */

static void emit_c_dependency(FILE *out, Decl *d) {
    if (!d->name.text) return;

    const char *from_name = NULL;
    const char *to_name = NULL;
    if (d->as.dependency.relation && d->as.dependency.relation->type == EXPR_ARROW) {
        Expr *rel = d->as.dependency.relation;
        if (rel->as.arrow.from && rel->as.arrow.from->type == EXPR_IDENT)
            from_name = rel->as.arrow.from->as.ident.name;
        if (rel->as.arrow.to && rel->as.arrow.to->type == EXPR_IDENT)
            to_name = rel->as.arrow.to->as.ident.name;
    }

    char *id = c_ident(d->name.text);

    fprintf(out, "/* dependency %s : %s -> %s */\n",
            d->name.text,
            from_name ? from_name : "?",
            to_name ? to_name : "?");
    fprintf(out, "typedef struct {\n");
    fprintf(out, "    long long id;\n");
    if (from_name) fprintf(out, "    long long %s_id;\n", from_name);
    if (to_name)   fprintf(out, "    long long %s_id;\n", to_name);

    /* Columns from 'carries' field */
    for (size_t i = 0; i < d->as.dependency.field_count; i++) {
        DeclField *f = &d->as.dependency.fields[i];
        if (!f->label.text || strcmp(f->label.text, "carries") != 0) continue;
        if (!f->value) continue;
        if (f->value->type == EXPR_IDENT && f->value->as.ident.name) {
            fprintf(out, "    const char *%s;\n", f->value->as.ident.name);
        } else if (f->value->type == EXPR_LIST) {
            for (size_t j = 0; j < f->value->as.list.count; j++) {
                Expr *item = f->value->as.list.items[j];
                if (item && item->type == EXPR_IDENT && item->as.ident.name)
                    fprintf(out, "    const char *%s;\n", item->as.ident.name);
            }
        }
    }

    fprintf(out, "} %s_t;\n\n", id);
    free(id);
}

/* ------------------------------------------------------------ */
/* Detect if an expression tree returns strings                   */
/* (Used to decide whether zhulin should emit as string function) */
/* ------------------------------------------------------------ */

static bool expr_returns_string(Expr *e) {
    if (!e) return false;
    if (e->type == EXPR_STRING) return true;
    if (e->type == EXPR_MATCH) {
        for (size_t i = 0; i < e->as.match_expr.arm_count; i++) {
            if (expr_returns_string(e->as.match_expr.arms[i].body))
                return true;
        }
    }
    if (e->type == EXPR_IF)
        return expr_returns_string(e->as.if_expr.then_branch) ||
               expr_returns_string(e->as.if_expr.else_branch);
    if (e->type == EXPR_BLOCK && e->as.block.count > 0)
        return expr_returns_string(e->as.block.stmts[e->as.block.count - 1]);
    return false;
}

/* ------------------------------------------------------------ */
/* Meihua expression → C expression                              */
/* ------------------------------------------------------------ */

static char *expr_to_c(Expr *e, bool string_mode);

static char *meihua_expr_to_c(Expr *e) {
    return expr_to_c(e, false);
}

static char *songqiao_expr_to_c(Expr *e) {
    return expr_to_c(e, true);
}

static char *expr_to_c(Expr *e, bool string_mode) {
    if (!e) return strdup("0");

    switch (e->type) {
    case EXPR_CALL: {
        if (!e->as.call.callee) return strdup("0");
        const char *callee = e->as.call.callee;

        /* M5: map math functions via generated enum lookup */
        math_fn_t mfn = math_fn_from_name(callee);
        const char *c_fn = (mfn != (math_fn_t)-1) ? math_fn_c(mfn) : NULL;

        const char *fn = c_fn ? c_fn : callee;
        size_t total_len = strlen(fn) + 3;
        char **arg_strs = malloc(e->as.call.arg_count * sizeof(char *));
        for (size_t i = 0; i < e->as.call.arg_count; i++) {
            arg_strs[i] = expr_to_c(e->as.call.args[i], string_mode);
            total_len += strlen(arg_strs[i]) + 2;
        }
        char *buf = malloc(total_len + 1);
        int pos = sprintf(buf, "%s(", fn);
        for (size_t i = 0; i < e->as.call.arg_count; i++) {
            if (i > 0) pos += sprintf(buf + pos, ", ");
            pos += sprintf(buf + pos, "%s", arg_strs[i]);
            free(arg_strs[i]);
        }
        sprintf(buf + pos, ")");
        free(arg_strs);
        return buf;
    }
    case EXPR_BINARY: {
        char *left = expr_to_c(e->as.binary.left, string_mode);
        char *right = expr_to_c(e->as.binary.right, string_mode);
        const char *op = e->as.binary.op ? e->as.binary.op : "+";
        if (strcmp(op, "^") == 0) {
            char *buf = malloc(strlen(left) + strlen(right) + 16);
            sprintf(buf, "pow(%s, %s)", left, right);
            free(left); free(right);
            return buf;
        }
        char *buf = malloc(strlen(left) + strlen(right) + strlen(op) + 8);
        sprintf(buf, "(%s %s %s)", left, op, right);
        free(left); free(right);
        return buf;
    }
    case EXPR_IDENT:
        if (e->as.ident.name) return strdup(e->as.ident.name);
        return strdup("0");
    case EXPR_NUMBER:
        if (e->as.number.text) return strdup(e->as.number.text);
        return strdup("0");
    case EXPR_UNARY: {
        char *operand = expr_to_c(e->as.unary.operand, string_mode);
        const char *op = e->as.unary.op ? e->as.unary.op : "-";
        char *buf = malloc(strlen(operand) + strlen(op) + 4);
        sprintf(buf, "(%s%s)", op, operand);
        free(operand);
        return buf;
    }
    case EXPR_IF: {
        char *cond = expr_to_c(e->as.if_expr.condition, string_mode);
        char *then_b = expr_to_c(e->as.if_expr.then_branch, string_mode);
        char *else_b = expr_to_c(e->as.if_expr.else_branch, string_mode);
        char *buf = malloc(strlen(cond) + strlen(then_b) + strlen(else_b) + 16);
        sprintf(buf, "(%s) ? %s : %s", cond, then_b, else_b);
        free(cond); free(then_b); free(else_b);
        return buf;
    }
    case EXPR_COALESCE: {
        /* C has no ?? operator; use a ternary on != 0 for numerics */
        char *left = expr_to_c(e->as.coalesce.left, string_mode);
        char *right = expr_to_c(e->as.coalesce.right, string_mode);
        char *buf = malloc(strlen(left) * 2 + strlen(right) + 24);
        sprintf(buf, "(%s != 0 ? %s : %s)", left, left, right);
        free(left); free(right);
        return buf;
    }
    case EXPR_STRING: {
        const char *val = e->as.string.value ? e->as.string.value : "";
        char *buf = malloc(strlen(val) + 4);
        sprintf(buf, "\"%s\"", val);
        return buf;
    }
    case EXPR_LIST: {
        /* C doesn't have list literals; emit as comment */
        return strdup("0 /* list */");
    }
    case EXPR_MATCH: {
        /* match expr → nested ternary chain
         * Pattern idents are literal match values (not C variables).
         * Use strcmp for string-typed discriminants. */
        char *disc = expr_to_c(e->as.match_expr.discriminant, string_mode);
        /* Build from last arm to first */
        char *result = strdup("0 /* no match */");
        /* Walk backwards for clean nesting */
        for (int i = (int)e->as.match_expr.arm_count - 1; i >= 0; i--) {
            Expr *pat = e->as.match_expr.arms[i].pattern;
            Expr *body = e->as.match_expr.arms[i].body;
            char *body_str = body ? expr_to_c(body, string_mode) : strdup("0");

            if (pat && pat->type == EXPR_WILDCARD) {
                free(result);
                result = body_str;
            } else {
                char *cond_str;
                if (pat && pat->type == EXPR_NUMBER && pat->as.number.text) {
                    /* Numeric comparison: disc == 42 */
                    size_t clen = strlen(disc) + strlen(pat->as.number.text) + 8;
                    cond_str = malloc(clen);
                    sprintf(cond_str, "%s == %s", disc, pat->as.number.text);
                } else if (pat && pat->type == EXPR_IDENT && pat->as.ident.name) {
                    if (string_mode) {
                        /* String comparison: strcmp(disc, "name") == 0 */
                        size_t clen = strlen(disc) + strlen(pat->as.ident.name) + 24;
                        cond_str = malloc(clen);
                        sprintf(cond_str, "strcmp(%s, \"%s\") == 0",
                                disc, pat->as.ident.name);
                    } else {
                        /* Numeric/enum comparison: disc == NAME */
                        char *upper = to_screaming(pat->as.ident.name);
                        size_t clen = strlen(disc) + strlen(upper) + 8;
                        cond_str = malloc(clen);
                        sprintf(cond_str, "%s == %s", disc, upper);
                        free(upper);
                    }
                } else if (pat && pat->type == EXPR_STRING && pat->as.string.value) {
                    size_t clen = strlen(disc) + strlen(pat->as.string.value) + 24;
                    cond_str = malloc(clen);
                    sprintf(cond_str, "strcmp(%s, \"%s\") == 0",
                            disc, pat->as.string.value);
                } else {
                    char *pat_str = pat ? expr_to_c(pat, string_mode) : strdup("0");
                    size_t clen = strlen(disc) + strlen(pat_str) + 8;
                    cond_str = malloc(clen);
                    sprintf(cond_str, "%s == %s", disc, pat_str);
                    free(pat_str);
                }

                size_t len = strlen(cond_str) +
                             strlen(body_str) + strlen(result) + 16;
                char *new_result = malloc(len);
                sprintf(new_result, "(%s ? %s : %s)",
                        cond_str, body_str, result);
                free(cond_str);
                free(body_str);
                free(result);
                result = new_result;
            }
        }
        free(disc);
        return result;
    }
    default:
        return strdup("0 /* expr */");
    }
}

/* ------------------------------------------------------------ */
/* meihua → static inline double function                        */
/* ------------------------------------------------------------ */

static void emit_c_meihua(FILE *out, Decl *d) {
    if (!d->name.text) return;

    fprintf(out, "/* meihua %s — pure computation */\n", d->name.text);
    fprintf(out, "static inline double %s(", d->name.text);

    for (size_t i = 0; i < d->as.exec_layer.param_count; i++) {
        if (d->as.exec_layer.params[i].text) {
            fprintf(out, "double %s", d->as.exec_layer.params[i].text);
            if (i + 1 < d->as.exec_layer.param_count) fprintf(out, ", ");
        }
    }
    if (d->as.exec_layer.param_count == 0) fprintf(out, "void");

    fprintf(out, ") {\n");

    Expr *body = d->as.exec_layer.body;
    if (body && body->type == EXPR_BLOCK && body->as.block.count == 1)
        body = body->as.block.stmts[0];

    if (body) {
        char *c_expr = meihua_expr_to_c(body);
        fprintf(out, "    return %s;\n", c_expr);
        free(c_expr);
    } else {
        fprintf(out, "    return 0; /* no body captured */\n");
    }

    fprintf(out, "}\n\n");
}

/* ------------------------------------------------------------ */
/* zhulin → static inline control flow function                  */
/* ------------------------------------------------------------ */

static void emit_c_zhulin(FILE *out, Decl *d) {
    if (!d->name.text) return;

    Expr *body = d->as.exec_layer.body;
    if (body && body->type == EXPR_BLOCK && body->as.block.count == 1)
        body = body->as.block.stmts[0];

    /* Detect whether this zhulin returns strings (e.g. match → "literal").
     * If so, emit as const char * function with const char * params and
     * use string_mode for expressions (strcmp for ident patterns). */
    bool is_string = expr_returns_string(body);

    const char *ret_type = is_string ? "const char *" : "double";
    const char *param_type = is_string ? "const char *" : "double";

    fprintf(out, "/* zhulin %s — control flow */\n", d->name.text);
    fprintf(out, "static inline %s%s(", ret_type, d->name.text);

    for (size_t i = 0; i < d->as.exec_layer.param_count; i++) {
        if (d->as.exec_layer.params[i].text) {
            fprintf(out, "%s%s", param_type, d->as.exec_layer.params[i].text);
            if (i + 1 < d->as.exec_layer.param_count) fprintf(out, ", ");
        }
    }
    if (d->as.exec_layer.param_count == 0) fprintf(out, "void");

    fprintf(out, ") {\n");

    if (body) {
        char *c_expr = is_string ? songqiao_expr_to_c(body)
                                 : meihua_expr_to_c(body);
        fprintf(out, "    return %s;\n", c_expr);
        free(c_expr);
    } else {
        fprintf(out, is_string ? "    return \"\";\n" : "    return 0;\n");
    }

    fprintf(out, "}\n\n");
}

/* ------------------------------------------------------------ */
/* songqiao → static inline config function                      */
/* ------------------------------------------------------------ */

static void emit_c_songqiao(FILE *out, Decl *d) {
    if (!d->name.text) return;

    fprintf(out, "/* songqiao %s — runtime configuration */\n", d->name.text);
    fprintf(out, "static inline const char *songqiao_%s(", d->name.text);

    for (size_t i = 0; i < d->as.exec_layer.param_count; i++) {
        if (d->as.exec_layer.params[i].text) {
            fprintf(out, "const char *%s", d->as.exec_layer.params[i].text);
            if (i + 1 < d->as.exec_layer.param_count) fprintf(out, ", ");
        }
    }
    if (d->as.exec_layer.param_count == 0) fprintf(out, "void");

    fprintf(out, ") {\n");

    Expr *body = d->as.exec_layer.body;
    if (body && body->type == EXPR_BLOCK && body->as.block.count == 1)
        body = body->as.block.stmts[0];

    if (body) {
        char *c_expr = songqiao_expr_to_c(body);
        fprintf(out, "    return %s;\n", c_expr);
        free(c_expr);
    } else {
        fprintf(out, "    return \"\";\n");
    }

    fprintf(out, "}\n\n");
}

/* ------------------------------------------------------------ */
/* projection → switch-based resolver                            */
/* ------------------------------------------------------------ */

/* Check if a result string is a member of a known dimension.
 * Returns the DimInfo if found, NULL otherwise. */
static const DimInfo *result_is_dimension_member(const char *result) {
    if (!result) return NULL;
    for (size_t i = 0; i < dim_registry_count; i++) {
        for (size_t j = 0; j < dim_registry[i].member_count; j++) {
            if (strcmp(result, dim_registry[i].members[j]) == 0)
                return &dim_registry[i];
        }
    }
    return NULL;
}

/* Check if ALL result strings in a projection map to members of
 * the same dimension. Returns that dimension, or NULL. */
static const DimInfo *detect_codomain_dimension(Decl *d) {
    const DimInfo *codomain = NULL;
    for (size_t a = 0; a < d->as.projection.arm_count; a++) {
        ProjectionArm *arm = &d->as.projection.arms[a];
        if (!arm->body || arm->body->type != EXPR_STRING) continue;
        if (!arm->body->as.string.value) continue;
        /* Skip wildcard patterns — default can be any value */
        if (arm->pattern && arm->pattern->type == EXPR_WILDCARD) continue;

        const DimInfo *found = result_is_dimension_member(
            arm->body->as.string.value);
        if (!found) return NULL;  /* not a dimension member → mixed mode */
        if (!codomain) codomain = found;
        else if (codomain != found) return NULL;  /* different dimensions */
    }
    return codomain;
}

/* Emit an enum-switch projection: invariant is a known dimension.
 * Two modes:
 *   1. String mode: returns const char* (M2 pattern)
 *   2. Enum mode: if ALL results map to members of another dimension,
 *      returns that dimension's enum type (M3 pattern — cross-dimension projection)
 */
static void emit_c_enum_projection(FILE *out, Decl *d,
                                    const char *invariant_name,
                                    const DimInfo *dim) {
    (void)invariant_name;
    char *id = c_ident(dim->name);

    /* M3: detect if results form a codomain dimension (enum → enum).
     * Guard: if the codomain is the SAME dimension as the invariant,
     * it's a string table whose labels happen to match member names
     * (e.g., decl_type_names maps decl → "unit", "zero", ...),
     * not a cross-dimension enum → enum projection. */
    const DimInfo *codomain = detect_codomain_dimension(d);
    if (codomain == dim) codomain = NULL;

    if (codomain) {
        /* Enum → enum projection (cross-dimension) */
        char *co_id = c_ident(codomain->name);
        fprintf(out, "/* projection %s (%s → %s) */\n",
                d->name.text, dim->name, codomain->name);
        fprintf(out, "static inline %s_t %s(%s_t val) {\n",
                co_id, d->name.text, id);
        fprintf(out, "    switch (val) {\n");

        const char *default_result = NULL;

        for (size_t a = 0; a < d->as.projection.arm_count; a++) {
            ProjectionArm *arm = &d->as.projection.arms[a];
            if (!arm->pattern || !arm->body) continue;
            const char *result = NULL;
            if (arm->body->type == EXPR_STRING)
                result = arm->body->as.string.value;
            if (!result) continue;

            const char *pat_name = NULL;
            if (arm->pattern->type == EXPR_IDENT)
                pat_name = arm->pattern->as.ident.name;
            else if (arm->pattern->type == EXPR_WILDCARD)
                pat_name = "_";
            if (!pat_name) continue;

            if (strcmp(pat_name, "_") == 0) {
                default_result = result;
                continue;
            }

            char *mu = to_screaming(pat_name);
            char *rv = to_screaming(result);
            fprintf(out, "    case %s_%s: return %s_%s;\n",
                    dim->prefix, mu, codomain->prefix, rv);
            free(mu);
            free(rv);
        }

        fprintf(out, "    }\n");
        if (default_result) {
            char *rv = to_screaming(default_result);
            fprintf(out, "    return %s_%s;\n", codomain->prefix, rv);
            free(rv);
        } else {
            /* Fallback: return first member (best effort) */
            if (codomain->member_count > 0) {
                char *rv = to_screaming(codomain->members[0]);
                fprintf(out, "    return %s_%s;\n", codomain->prefix, rv);
                free(rv);
            } else {
                fprintf(out, "    return 0;\n");
            }
        }

        fprintf(out, "}\n\n");
        free(co_id);
    } else {
        /* String return mode (M2 pattern) */
        fprintf(out, "/* projection %s (enum switch over %s) */\n",
                d->name.text, dim->name);
        fprintf(out, "static inline const char *%s(%s_t val) {\n",
                d->name.text, id);
        fprintf(out, "    switch (val) {\n");

        const char *default_result = NULL;

        for (size_t a = 0; a < d->as.projection.arm_count; a++) {
            ProjectionArm *arm = &d->as.projection.arms[a];
            if (!arm->pattern || !arm->body) continue;
            const char *result = NULL;
            if (arm->body->type == EXPR_STRING)
                result = arm->body->as.string.value;
            if (!result) continue;

            const char *pat_name = NULL;
            if (arm->pattern->type == EXPR_IDENT)
                pat_name = arm->pattern->as.ident.name;
            else if (arm->pattern->type == EXPR_LIST &&
                     arm->pattern->as.list.count >= 1) {
                Expr *e1 = arm->pattern->as.list.items[0];
                if (e1->type == EXPR_IDENT)
                    pat_name = e1->as.ident.name;
                else if (e1->type == EXPR_WILDCARD)
                    pat_name = "_";
            } else if (arm->pattern->type == EXPR_WILDCARD) {
                pat_name = "_";
            }
            if (!pat_name) continue;

            if (strcmp(pat_name, "_") == 0) {
                default_result = result;
                continue;
            }

            char *mu = to_screaming(pat_name);
            fprintf(out, "    case %s_%s: return \"%s\";\n",
                    dim->prefix, mu, result);
            free(mu);
        }

        fprintf(out, "    }\n");
        fprintf(out, "    return \"%s\";\n",
                default_result ? default_result : "<unknown>");
        fprintf(out, "}\n\n");
    }

    free(id);
}

/* ------------------------------------------------------------ */
/* M4: dispatch projection — emit raw case bodies for            */
/* #include-in-switch pattern. Each arm becomes:                 */
/*   case ENUM_MEMBER: handler(out, d); break;                   */
/* Empty string handler = no-op (break only).                    */
/* ------------------------------------------------------------ */

/* M6: parser_dispatch projection — emits:                       */
/*   static inline Decl* name(Parser *p, tok_t tok)              */
/*   Empty string = return NULL (no handler for this token).     */
/* ------------------------------------------------------------ */
static void emit_c_parser_dispatch_projection(FILE *out, Decl *d,
                                               const DimInfo *dim) {
    fprintf(out, "/* parser_dispatch: %s (%s → parse function) */\n",
            d->name.text, dim->name);
    fprintf(out, "/* Generated by suhc — do not edit by hand. */\n");
    fprintf(out, "/* Include after all parse function definitions. */\n\n");
    fprintf(out, "static inline Decl *%s(Parser *p, %s_t tok) {\n",
            d->name.text, dim->name);
    fprintf(out, "    switch (tok) {\n");

    for (size_t a = 0; a < d->as.projection.arm_count; a++) {
        ProjectionArm *arm = &d->as.projection.arms[a];
        if (!arm->pattern || !arm->body) continue;

        const char *result = NULL;
        if (arm->body->type == EXPR_STRING)
            result = arm->body->as.string.value;
        if (!result) continue;

        const char *pat_name = NULL;
        if (arm->pattern->type == EXPR_IDENT)
            pat_name = arm->pattern->as.ident.name;
        else if (arm->pattern->type == EXPR_WILDCARD)
            pat_name = "_";
        if (!pat_name) continue;

        if (strcmp(pat_name, "_") == 0) {
            fprintf(out, "    default: return NULL;\n");
            continue;
        }

        char *mu = to_screaming(pat_name);
        if (result[0] == '\0') {
            fprintf(out, "    case %s_%s: return NULL;\n", dim->prefix, mu);
        } else {
            fprintf(out, "    case %s_%s: return %s(p);\n",
                    dim->prefix, mu, result);
        }
        free(mu);
    }

    fprintf(out, "    }\n");
    fprintf(out, "    return NULL;\n");
    fprintf(out, "}\n\n");
}

/* M6: kind_sigil projection — emits:                            */
/*   static inline kind_t name(tok_t tok)                        */
/*   Returns (kind_t)-1 for non-sigil tokens.                    */
/* ------------------------------------------------------------ */
static void emit_c_kind_sigil_projection(FILE *out, Decl *d,
                                          const DimInfo *dim,
                                          const DimInfo *codomain) {
    fprintf(out, "/* kind_sigil: %s (%s → %s) */\n",
            d->name.text, dim->name, codomain->name);
    fprintf(out, "/* Generated by suhc — do not edit by hand. */\n\n");
    fprintf(out, "static inline %s_t %s(%s_t tok) {\n",
            codomain->name, d->name.text, dim->name);
    fprintf(out, "    switch (tok) {\n");

    for (size_t a = 0; a < d->as.projection.arm_count; a++) {
        ProjectionArm *arm = &d->as.projection.arms[a];
        if (!arm->pattern || !arm->body) continue;

        const char *result = NULL;
        if (arm->body->type == EXPR_STRING)
            result = arm->body->as.string.value;
        if (!result) continue;

        const char *pat_name = NULL;
        if (arm->pattern->type == EXPR_IDENT)
            pat_name = arm->pattern->as.ident.name;
        else if (arm->pattern->type == EXPR_WILDCARD)
            pat_name = "_";
        if (!pat_name) continue;

        if (strcmp(pat_name, "_") == 0) {
            fprintf(out, "    default: return (%s_t)-1;\n", codomain->name);
            continue;
        }

        char *pat_mu = to_screaming(pat_name);
        if (result[0] == '\0') {
            fprintf(out, "    case %s_%s: return (%s_t)-1;\n",
                    dim->prefix, pat_mu, codomain->name);
        } else {
            char *res_mu = to_screaming(result);
            fprintf(out, "    case %s_%s: return %s_%s;\n",
                    dim->prefix, pat_mu, codomain->prefix, res_mu);
            free(res_mu);
        }
        free(pat_mu);
    }

    fprintf(out, "    }\n");
    fprintf(out, "    return (%s_t)-1;\n", codomain->name);
    fprintf(out, "}\n\n");
}

static void emit_c_dispatch_projection(FILE *out, Decl *d,
                                        const DimInfo *dim) {
    char *id = c_ident(dim->name);

    fprintf(out, "/* dispatch: %s (%s → handler) */\n",
            d->name.text, dim->name);
    fprintf(out, "/* Generated by suhc — do not edit by hand. */\n");
    fprintf(out, "/* Include after all handler definitions.    */\n\n");
    fprintf(out, "static inline void %s(FILE *out, Decl *d) {\n",
            d->name.text);
    fprintf(out, "    switch (d->type) {\n");

    for (size_t a = 0; a < d->as.projection.arm_count; a++) {
        ProjectionArm *arm = &d->as.projection.arms[a];
        if (!arm->pattern || !arm->body) continue;

        const char *result = NULL;
        if (arm->body->type == EXPR_STRING)
            result = arm->body->as.string.value;
        if (!result) continue;

        const char *pat_name = NULL;
        if (arm->pattern->type == EXPR_IDENT)
            pat_name = arm->pattern->as.ident.name;
        else if (arm->pattern->type == EXPR_WILDCARD)
            pat_name = "_";
        if (!pat_name) continue;

        /* Wildcard becomes default case */
        if (strcmp(pat_name, "_") == 0) {
            if (result[0] != '\0') {
                fprintf(out, "    default: %s(out, d); break;\n", result);
            } else {
                fprintf(out, "    default: break;\n");
            }
            continue;
        }

        char *mu = to_screaming(pat_name);
        if (result[0] == '\0') {
            /* No-op case — compile-time only or no output needed */
            fprintf(out, "    case %s_%s: break;\n", dim->prefix, mu);
        } else {
            fprintf(out, "    case %s_%s: %s(out, d); break;\n",
                    dim->prefix, mu, result);
        }
        free(mu);
    }

    fprintf(out, "    }\n");
    fprintf(out, "}\n\n");
    free(id);
}

static void emit_c_projection(FILE *out, Decl *d) {
    if (!d->name.text) return;

    /* Extract field names */
    const char *invariant_name = NULL;
    const char *context_name = NULL;
    const char *yields_name = NULL;

    for (size_t i = 0; i < d->as.projection.field_count; i++) {
        DeclField *f = &d->as.projection.fields[i];
        if (!f->label.text) continue;
        if (strcmp(f->label.text, "invariant") == 0) {
            if (f->value && f->value->type == EXPR_IDENT)
                invariant_name = f->value->as.ident.name;
        }
        if (strcmp(f->label.text, "context") == 0) {
            if (f->value && f->value->type == EXPR_IDENT)
                context_name = f->value->as.ident.name;
        }
        if (strcmp(f->label.text, "yields") == 0) {
            if (f->value && f->value->type == EXPR_IDENT)
                yields_name = f->value->as.ident.name;
        }
    }

    /* M4: dispatch mode — yields ω dispatch with invariant as known dimension.
     * Emits raw case bodies for #include-in-switch pattern. */
    if (invariant_name && !context_name && yields_name &&
        strcmp(yields_name, "dispatch") == 0) {
        const DimInfo *dim = dim_registry_find(invariant_name);
        if (dim) {
            emit_c_dispatch_projection(out, d, dim);
            return;
        }
    }

    /* M6: parser_dispatch mode — yields ω parser_dispatch.
     * Emits: Decl* name(Parser *p, tok_t tok) with switch on tok. */
    if (invariant_name && !context_name && yields_name &&
        strcmp(yields_name, "parser_dispatch") == 0) {
        const DimInfo *dim = dim_registry_find(invariant_name);
        if (dim) {
            emit_c_parser_dispatch_projection(out, d, dim);
            return;
        }
    }

    /* M6: kind_sigil mode — yields ω kind_sigil.
     * Emits: kind_t name(tok_t tok) — cross-dimension enum→enum. */
    if (invariant_name && !context_name && yields_name &&
        strcmp(yields_name, "kind_sigil") == 0) {
        const DimInfo *dim = dim_registry_find(invariant_name);
        if (dim) {
            /* Find the codomain dimension — "kind" */
            const DimInfo *codomain = dim_registry_find("kind");
            if (codomain) {
                emit_c_kind_sigil_projection(out, d, dim, codomain);
                return;
            }
        }
    }

    /* M2: if invariant is a known dimension and there's no context,
     * emit an enum-based switch function instead of strcmp chain */
    if (invariant_name && !context_name) {
        const DimInfo *dim = dim_registry_find(invariant_name);
        if (dim) {
            emit_c_enum_projection(out, d, invariant_name, dim);
            return;
        }
    }

    /* Fall through: string-based if/else chain (original behavior) */
    fprintf(out, "/* projection %s */\n", d->name.text);
    fprintf(out, "static inline const char *resolve_%s(\n", d->name.text);
    fprintf(out, "    const char *%s",
            invariant_name ? invariant_name : "invariant");
    if (context_name)
        fprintf(out, ",\n    const char *%s", context_name);
    fprintf(out, "\n) {\n");

    /* Emit pattern matching as if/else chain (strings can't switch in C) */
    bool first = true;
    bool has_default = false;
    const char *default_result = NULL;

    for (size_t a = 0; a < d->as.projection.arm_count; a++) {
        ProjectionArm *arm = &d->as.projection.arms[a];
        if (!arm->pattern || !arm->body) continue;

        /* Extract pattern components */
        const char *p1 = NULL, *p2 = NULL;
        if (arm->pattern->type == EXPR_LIST && arm->pattern->as.list.count >= 2) {
            Expr *e1 = arm->pattern->as.list.items[0];
            Expr *e2 = arm->pattern->as.list.items[1];
            if (e1->type == EXPR_IDENT) p1 = e1->as.ident.name;
            if (e1->type == EXPR_WILDCARD) p1 = "_";
            if (e2->type == EXPR_IDENT) p2 = e2->as.ident.name;
            if (e2->type == EXPR_WILDCARD) p2 = "_";
        } else if (arm->pattern->type == EXPR_WILDCARD) {
            p1 = "_"; p2 = "_";
        }

        /* Extract result string */
        const char *result = NULL;
        if (arm->body->type == EXPR_STRING)
            result = arm->body->as.string.value;
        if (!result) continue;

        /* Wildcard → default */
        if (p1 && strcmp(p1, "_") == 0 && (!p2 || strcmp(p2, "_") == 0)) {
            has_default = true;
            default_result = result;
            continue;
        }

        if (!p1) continue;

        fprintf(out, "    %sif (", first ? "" : "} else ");
        first = false;

        bool need_and = false;
        if (strcmp(p1, "_") != 0) {
            fprintf(out, "strcmp(%s, \"%s\") == 0",
                    invariant_name ? invariant_name : "invariant", p1);
            need_and = true;
        }
        if (p2 && strcmp(p2, "_") != 0) {
            if (need_and) fprintf(out, " && ");
            fprintf(out, "strcmp(%s, \"%s\") == 0",
                    context_name ? context_name : "context", p2);
        }
        fprintf(out, ") {\n");
        fprintf(out, "        return \"%s\";\n", result);
    }

    if (!first) {
        fprintf(out, "    }\n");
    }

    if (has_default && default_result) {
        fprintf(out, "    return \"%s\";\n", default_result);
    } else {
        fprintf(out, "    return \"\";\n");
    }

    fprintf(out, "}\n\n");
}

/* ------------------------------------------------------------ */
/* traversal → documented function stub                          */
/* ------------------------------------------------------------ */

static void emit_c_traversal(FILE *out, Decl *d) {
    if (!d->name.text) return;

    /* Document the kind sections */
    fprintf(out, "/*\n");
    fprintf(out, " * traversal %s\n", d->name.text);
    fprintf(out, " * Pipeline: ");

    for (size_t i = 0; i < d->as.traversal.section_count; i++) {
        TraversalSection *sec = &d->as.traversal.sections[i];
        const char *kind_str = "?";
        switch (sec->section_kind) {
        case KIND_XI:       kind_str = "ξ"; break;
        case KIND_ZETA:     kind_str = "ζ"; break;
        case KIND_X:        kind_str = "x"; break;
        case KIND_RK:       kind_str = "R.k"; break;
        case KIND_OMEGA:    kind_str = "ω"; break;
        case KIND_DELTA_RK: kind_str = "ΔR.k"; break;
        default: break;
        }
        if (sec->label.text)
            fprintf(out, "%s (%s)", sec->label.text, kind_str);
        else
            fprintf(out, "(%s)", kind_str);
        if (i + 1 < d->as.traversal.section_count) fprintf(out, " → ");
    }
    fprintf(out, "\n");
    fprintf(out, " * TODO: Implement. Generated stub only.\n");
    fprintf(out, " */\n");

    /* Stub: returns NULL, params based on kind sections */
    fprintf(out, "static inline void *resolve_%s(", d->name.text);

    bool first_param = true;
    for (size_t i = 0; i < d->as.traversal.section_count; i++) {
        TraversalSection *sec = &d->as.traversal.sections[i];
        if (!sec->label.text) continue;
        if (sec->section_kind == KIND_RK || sec->section_kind == KIND_OMEGA ||
            sec->section_kind == KIND_DELTA_RK)
            continue; /* These are output/operator, not input params */

        if (!first_param) fprintf(out, ", ");
        first_param = false;
        fprintf(out, "const char *%s", sec->label.text);
    }
    if (first_param) fprintf(out, "void");

    fprintf(out, ") {\n");
    fprintf(out, "    return NULL; /* stub — implement in hand-written C */\n");
    fprintf(out, "}\n\n");
}

/* ------------------------------------------------------------ */
/* morphism → documented stub                                    */
/* ------------------------------------------------------------ */

static void emit_c_morphism(FILE *out, Decl *d) {
    if (!d->name.text) return;

    const char *from_name = NULL;
    const char *to_name = NULL;
    if (d->as.morphism.signature && d->as.morphism.signature->type == EXPR_ARROW) {
        Expr *rel = d->as.morphism.signature;
        if (rel->as.arrow.from && rel->as.arrow.from->type == EXPR_IDENT)
            from_name = rel->as.arrow.from->as.ident.name;
        if (rel->as.arrow.to && rel->as.arrow.to->type == EXPR_IDENT)
            to_name = rel->as.arrow.to->as.ident.name;
    }

    fprintf(out, "/* morphism %s : %s -> %s */\n",
            d->name.text,
            from_name ? from_name : "?",
            to_name ? to_name : "?");
    fprintf(out, "/* TODO: Implement — this is a state transition. */\n\n");
}

/* ------------------------------------------------------------ */
/* ζ / x / ω / ΔR.k → comments                                  */
/* ------------------------------------------------------------ */

static void emit_c_kinded_value(FILE *out, Decl *d) {
    if (!d->name.text) return;
    const char *kind_str = "?";
    switch (d->kind) {
    case KIND_XI:       kind_str = "ξ"; break;
    case KIND_ZETA:     kind_str = "ζ"; break;
    case KIND_X:        kind_str = "x"; break;
    case KIND_RK:       kind_str = "R.k"; break;
    case KIND_OMEGA:    kind_str = "ω"; break;
    case KIND_DELTA_RK: kind_str = "ΔR.k"; break;
    default: break;
    }

    if (d->kind == KIND_XI) {
        emit_c_xi(out, d);
    } else {
        /* ζ, x, ω, ΔR.k are context-dependent; emit as comments */
        if (d->as.kinded.value && d->as.kinded.value->type == EXPR_STRING) {
            fprintf(out, "/* %s %s = \"%s\" */\n",
                    kind_str, d->name.text,
                    d->as.kinded.value->as.string.value ?
                    d->as.kinded.value->as.string.value : "");
        } else if (d->as.kinded.value && d->as.kinded.value->type == EXPR_NUMBER) {
            fprintf(out, "/* %s %s = %s */\n",
                    kind_str, d->name.text,
                    d->as.kinded.value->as.number.text ?
                    d->as.kinded.value->as.number.text : "?");
        } else {
            fprintf(out, "/* %s %s */\n", kind_str, d->name.text);
        }
    }
}

/* ------------------------------------------------------------ */
/* M4: dispatch wrappers — uniform (FILE*, Decl*) signature      */
/* for handlers that originally took extra arguments.             */
/* ------------------------------------------------------------ */

static void emit_c_unit(FILE *out, Decl *d) {
    emit_c_unit_type(out, d, "unit");
}
static void emit_c_magnitude(FILE *out, Decl *d) {
    emit_c_unit_type(out, d, "magnitude");
}
static void emit_c_vector(FILE *out, Decl *d) {
    emit_c_unit_type(out, d, "vector");
}
static void emit_c_import_comment(FILE *out, Decl *d) {
    fprintf(out, "/* import %s */\n",
            d->as.import_decl.module_name ?
            d->as.import_decl.module_name : "?");
}

/* M4: generated dispatch — replaces hand-written switch in emit_c().
 * Included after all handler definitions so the static inline
 * dispatch function can reference every handler by name. */
#include "gen/emit_c_dispatch.h"

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

int emit_c(Program *prog, FILE *out, DiagList *diags) {
    (void)diags;

    /* M2: populate dimension registry from local + imported declarations
     * so that projections over dimensions emit enum switches */
    dim_registry_clear();

    /* Scan imported declarations first */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type == DECL_IMPORT && d->as.import_decl.resolved) {
            Program *imp = d->as.import_decl.resolved;
            for (size_t j = 0; j < imp->count; j++)
                dim_registry_add(imp->decls[j]);
        }
    }
    /* Then local declarations */
    for (size_t i = 0; i < prog->count; i++)
        dim_registry_add(prog->decls[i]);

    emit_c_header(out, prog->filename ? prog->filename : "generated");

    /* M4: dispatch via generated switch from ordbok.
     * Replaces hand-written 18-case DeclType switch.
     * Source of truth: ordbok/compiler/compiler_emit_c_dispatch.szh */
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        emit_c_dispatch(out, d);
    }

    emit_c_footer(out, prog->filename ? prog->filename : "generated");
    return 0;
}
