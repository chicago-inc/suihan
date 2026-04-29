/*
 * suhc — parser.c
 * Recursive descent parser for .szh files.
 *
 * Builds a Program (stack of Decls) from tokens.
 * Enforces ordbok ordering: forward references are errors.
 * Assigns kinds where syntactically determined.
 */

#include "parser.h"
#include "compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------ */
/* Parser helpers                                                */
/* ------------------------------------------------------------ */

/* Parse a @targets annotation from a comment token.
 * Format: -- @targets: NAME1, NAME2, ...
 * Stores results in parser's pending_targets buffer. */
static void parse_targets_annotation(Parser *p, Token *comment) {
    /* Find "@targets:" in the comment text */
    const char *text = comment->start;
    size_t len = comment->length;

    const char *at = NULL;
    for (size_t i = 0; i + 9 <= len; i++) {
        if (strncmp(text + i, "@targets:", 9) == 0) {
            at = text + i + 9;
            break;
        }
    }
    if (!at) return;

    /* Parse comma-separated identifiers */
    const char *end = text + len;
    while (at < end) {
        /* Skip whitespace */
        while (at < end && (*at == ' ' || *at == '\t' || *at == ',')) at++;
        if (at >= end) break;

        /* Read identifier */
        const char *start = at;
        while (at < end && *at != ',' && *at != ' ' && *at != '\t' && *at != '\n') at++;
        size_t ilen = (size_t)(at - start);
        if (ilen > 0) {
            p->pending_targets = realloc(p->pending_targets,
                (p->pending_target_count + 1) * sizeof(char *));
            p->pending_targets[p->pending_target_count] = strndup(start, ilen);
            p->pending_target_count++;
        }
    }
}

static void parser_advance(Parser *p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next(p->lexer);
        if (p->current.type != TOK_COMMENT) break;
        /* Check for @targets annotation before skipping */
        parse_targets_annotation(p, &p->current);
    }
}

static bool check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static bool match(Parser *p, TokenType type) {
    if (!check(p, type)) return false;
    parser_advance(p);
    return true;
}

static void skip_newlines(Parser *p) {
    while (check(p, TOK_NEWLINE)) parser_advance(p);
}

static bool expect(Parser *p, TokenType type, const char *msg) {
    if (check(p, type)) {
        parser_advance(p);
        return true;
    }
    diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
               p->current.line, p->current.col,
               "expected %s, got '%s'", msg, token_type_name(p->current.type));
    return false;
}

static char *token_text(Token t) {
    char *s = malloc(t.length + 1);
    memcpy(s, t.start, t.length);
    s[t.length] = '\0';
    return s;
}

static Name token_to_name(Token t) {
    return name_new(token_text(t), t.line, t.col);
}

/* ------------------------------------------------------------ */
/* Ordbok ordering                                               */
/* ------------------------------------------------------------ */

static void define_name(Parser *p, const char *name) {
    if (p->defined_count >= p->defined_capacity) {
        p->defined_capacity = p->defined_capacity ? p->defined_capacity * 2 : 64;
        p->defined_names = realloc(p->defined_names, p->defined_capacity * sizeof(char *));
    }
    p->defined_names[p->defined_count++] = strdup(name);
}

static bool is_defined(Parser *p, const char *name) {
    for (size_t i = 0; i < p->defined_count; i++) {
        if (strcmp(p->defined_names[i], name) == 0) return true;
    }
    /* Built-in type names are always defined */
    static const char *builtins[] = {
        "text", "time", "bool", "int", "float",
        "magnitude", "zero", "true", "false",
        NULL
    };
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(builtins[i], name) == 0) return true;
    }
    return false;
}

/* Forward reference check — Phase 0A: tracking only, not enforced yet.
 * Will be enabled when the ordbok ordering is fully specified. */
#if 0
static void check_forward_ref(Parser *p, const char *name, int line, int col) {
    if (name && strlen(name) > 0 && !is_defined(p, name)) {
        diag_warn(p->diags, DIAG_FORWARD_REFERENCE, NULL, line, col,
                  "'%s' referenced before definition", name);
    }
}
#endif

/* ------------------------------------------------------------ */
/* Expression parsing                                            */
/* ------------------------------------------------------------ */

/* Peek whether the token *after* the current one is an enum/arm separator
 * (pipe, newline, EOF, dedent, arrow). Used to disambiguate keywords that
 * could be either prefix operators (decidable, if) or bare identifier names
 * in enum expressions (a | decidable | undecidable) or projection arm
 * patterns (decidable -> "decidable"). */
static bool peek_is_enum_sep(Parser *p) {
    Token save_prev = p->previous;
    Token save_curr = p->current;
    Lexer save_lex = *p->lexer;

    parser_advance(p);  /* consume current keyword, look at what follows */
    bool is_sep = check(p, TOK_PIPE) || check(p, TOK_NEWLINE) ||
                  check(p, TOK_EOF)  || check(p, TOK_DEDENT) ||
                  check(p, TOK_ARROW);

    /* Restore */
    p->previous = save_prev;
    p->current = save_curr;
    *p->lexer = save_lex;
    return is_sep;
}

static Expr *parse_expr(Parser *p);
static Expr *parse_primary(Parser *p);

static Expr *parse_primary(Parser *p) {
    if (check(p, TOK_NUMBER)) {
        parser_advance(p);
        Expr *e = expr_new(EXPR_NUMBER, p->previous.line, p->previous.col);
        e->as.number.text = token_text(p->previous);
        return e;
    }
    if (check(p, TOK_STRING)) {
        parser_advance(p);
        Expr *e = expr_new(EXPR_STRING, p->previous.line, p->previous.col);
        /* Strip quotes */
        char *raw = token_text(p->previous);
        size_t len = strlen(raw);
        if (len >= 2 && raw[0] == '"' && raw[len-1] == '"') {
            e->as.string.value = strndup(raw + 1, len - 2);
            free(raw);
        } else {
            e->as.string.value = raw;
        }
        return e;
    }
    if (check(p, TOK_UNDERSCORE)) {
        parser_advance(p);
        return expr_new(EXPR_WILDCARD, p->previous.line, p->previous.col);
    }
    if (check(p, TOK_DECIDABLE) && !peek_is_enum_sep(p)) {
        parser_advance(p);
        Expr *e = expr_new(EXPR_DECIDABLE, p->previous.line, p->previous.col);
        e->as.decidability.inner = parse_primary(p);
        return e;
    }
    if (check(p, TOK_UNDECIDABLE) && !peek_is_enum_sep(p)) {
        parser_advance(p);
        Expr *e = expr_new(EXPR_UNDECIDABLE, p->previous.line, p->previous.col);
        e->as.decidability.inner = parse_primary(p);
        return e;
    }
    if (check(p, TOK_LBRACKET)) {
        /* List: [a, b, c] */
        parser_advance(p);
        Expr *e = expr_new(EXPR_LIST, p->previous.line, p->previous.col);
        size_t cap = 8;
        e->as.list.items = malloc(cap * sizeof(Expr *));
        e->as.list.count = 0;

        skip_newlines(p);
        if (!check(p, TOK_RBRACKET)) {
            do {
                skip_newlines(p);
                if (check(p, TOK_RBRACKET)) break;
                if (e->as.list.count >= cap) {
                    cap *= 2;
                    e->as.list.items = realloc(e->as.list.items, cap * sizeof(Expr *));
                }
                e->as.list.items[e->as.list.count++] = parse_expr(p);
                skip_newlines(p);
            } while (match(p, TOK_COMMA));
        }
        expect(p, TOK_RBRACKET, "]");
        return e;
    }
    if (check(p, TOK_LBRACE)) {
        /* Compound: { key: val, ... } */
        parser_advance(p);
        Expr *e = expr_new(EXPR_COMPOUND, p->previous.line, p->previous.col);
        size_t cap = 8;
        e->as.compound.keys = malloc(cap * sizeof(Name));
        e->as.compound.values = malloc(cap * sizeof(Expr *));
        e->as.compound.count = 0;

        skip_newlines(p);
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            skip_newlines(p);
            if (check(p, TOK_RBRACE)) break;
            if (e->as.compound.count >= cap) {
                cap *= 2;
                e->as.compound.keys = realloc(e->as.compound.keys, cap * sizeof(Name));
                e->as.compound.values = realloc(e->as.compound.values, cap * sizeof(Expr *));
            }
            if (!check(p, TOK_IDENT) && !check(p, TOK_X)) {
                diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                           p->current.line, p->current.col,
                           "expected field name in compound");
                break;
            }
            parser_advance(p);
            Name key = token_to_name(p->previous);
            Expr *val = NULL;
            if (match(p, TOK_COLON) || match(p, TOK_EQUALS)) {
                val = parse_expr(p);
            }
            e->as.compound.keys[e->as.compound.count] = key;
            e->as.compound.values[e->as.compound.count] = val;
            e->as.compound.count++;
            skip_newlines(p);
            match(p, TOK_COMMA);
            match(p, TOK_SEMICOLON);
        }
        expect(p, TOK_RBRACE, "}");
        return e;
    }
    if (check(p, TOK_LPAREN)) {
        /* Parenthesized expression or tuple */
        parser_advance(p);
        Expr *inner = parse_expr(p);
        if (match(p, TOK_COMMA)) {
            /* Tuple — wrap as list */
            Expr *e = expr_new(EXPR_LIST, inner->line, inner->col);
            size_t cap = 8;
            e->as.list.items = malloc(cap * sizeof(Expr *));
            e->as.list.items[0] = inner;
            e->as.list.count = 1;
            do {
                if (e->as.list.count >= cap) {
                    cap *= 2;
                    e->as.list.items = realloc(e->as.list.items, cap * sizeof(Expr *));
                }
                e->as.list.items[e->as.list.count++] = parse_expr(p);
            } while (match(p, TOK_COMMA));
            expect(p, TOK_RPAREN, ")");
            return e;
        }
        expect(p, TOK_RPAREN, ")");
        return inner;
    }

    /* Match expression: match <discriminant> NEWLINE INDENT arm* DEDENT
     * Triggers when 'match' ident is followed by another ident (discriminant)
     * and then a newline (no call parens). The arms come from the INDENT block
     * that the section body parser would normally see. */
    if (check(p, TOK_IDENT) && p->current.length == 5 &&
        memcmp(p->current.start, "match", 5) == 0) {
        /* Save state in case this isn't a match expression */
        Token save_prev = p->previous;
        Token save_curr = p->current;
        Lexer save_lexer = *p->lexer;

        parser_advance(p); /* consume 'match' */
        int line = p->previous.line, col = p->previous.col;

        /* Is the next token an identifier (discriminant)? */
        if (check(p, TOK_IDENT) || check(p, TOK_X)) {
            parser_advance(p); /* consume discriminant */
            char *disc_name = token_text(p->previous);
            int dline = p->previous.line, dcol = p->previous.col;

            /* Is the next token end-of-expression (newline, EOF, dedent, or lbrace)?
             * If yes → this is a match expression (arms follow in block or braces).
             * If it's '(' → this is match_something(...), restore and fallthrough. */
            if (check(p, TOK_NEWLINE) || check(p, TOK_EOF) ||
                check(p, TOK_DEDENT) || check(p, TOK_INDENT) ||
                check(p, TOK_LBRACE)) {
                /* This IS a match expression */
                Expr *disc = expr_new(EXPR_IDENT, dline, dcol);
                disc->as.ident.name = disc_name;

                Expr *e = expr_new(EXPR_MATCH, line, col);
                e->as.match_expr.discriminant = disc;
                size_t acap = 8;
                e->as.match_expr.arms = malloc(acap * sizeof(e->as.match_expr.arms[0]));
                e->as.match_expr.arm_count = 0;

                /* Inline brace-delimited form: match disc { arm, arm, ... } */
                if (check(p, TOK_LBRACE)) {
                    parser_advance(p); /* consume { */
                    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                        skip_newlines(p);
                        if (check(p, TOK_RBRACE) || check(p, TOK_EOF)) break;

                        Expr *arm_expr = parse_expr(p);

                        if (e->as.match_expr.arm_count >= acap) {
                            acap *= 2;
                            e->as.match_expr.arms = realloc(
                                e->as.match_expr.arms,
                                acap * sizeof(e->as.match_expr.arms[0]));
                        }

                        if (arm_expr && arm_expr->type == EXPR_ARROW) {
                            e->as.match_expr.arms[e->as.match_expr.arm_count].pattern =
                                arm_expr->as.arrow.from;
                            e->as.match_expr.arms[e->as.match_expr.arm_count].body =
                                arm_expr->as.arrow.to;
                        } else {
                            e->as.match_expr.arms[e->as.match_expr.arm_count].pattern = arm_expr;
                            e->as.match_expr.arms[e->as.match_expr.arm_count].body = NULL;
                        }
                        e->as.match_expr.arm_count++;

                        /* Consume optional comma separator */
                        match(p, TOK_COMMA);
                        skip_newlines(p);
                    }
                    if (check(p, TOK_RBRACE)) parser_advance(p);
                    return e;
                }

                /* Indent-based block form (original) */
                skip_newlines(p);
                if (match(p, TOK_INDENT)) {
                    while (!check(p, TOK_DEDENT) && !check(p, TOK_EOF)) {
                        skip_newlines(p);
                        if (check(p, TOK_DEDENT) || check(p, TOK_EOF)) break;

                        Expr *arm_expr = parse_expr(p);

                        if (e->as.match_expr.arm_count >= acap) {
                            acap *= 2;
                            e->as.match_expr.arms = realloc(
                                e->as.match_expr.arms,
                                acap * sizeof(e->as.match_expr.arms[0]));
                        }

                        if (arm_expr && arm_expr->type == EXPR_ARROW) {
                            e->as.match_expr.arms[e->as.match_expr.arm_count].pattern =
                                arm_expr->as.arrow.from;
                            e->as.match_expr.arms[e->as.match_expr.arm_count].body =
                                arm_expr->as.arrow.to;
                        } else {
                            e->as.match_expr.arms[e->as.match_expr.arm_count].pattern = arm_expr;
                            e->as.match_expr.arms[e->as.match_expr.arm_count].body = NULL;
                        }
                        e->as.match_expr.arm_count++;
                        skip_newlines(p);
                    }
                    match(p, TOK_DEDENT);
                }
                return e;
            }
            /* Not a match expression — restore parser state */
            free(disc_name);
        }
        /* Restore: not a match expression */
        p->previous = save_prev;
        p->current = save_curr;
        *p->lexer = save_lexer;
    }

    /* Identifier (possibly with call).
     * Keywords are valid as identifiers in expression context —
     * 'projection(a, b)' is a function call, not a declaration.
     * All keyword tokens are accepted here so that dimension members
     * can use keyword names (e.g., dimension decl : unit | zero | ...).
     * This is context-sensitive: at declaration level the keywords trigger
     * their parse functions; inside expressions they are just names. */
    if (check(p, TOK_IDENT) || check(p, TOK_X) ||
        check(p, TOK_UNIT) || check(p, TOK_ZERO) ||
        check(p, TOK_MAGNITUDE) || check(p, TOK_VECTOR) ||
        check(p, TOK_DIMENSION) || check(p, TOK_PROJECTION) ||
        check(p, TOK_TRAVERSAL) || check(p, TOK_MORPHISM) ||
        check(p, TOK_CONTAINMENT) || check(p, TOK_DEPENDENCY) ||
        check(p, TOK_INCOMMENSURABLE) || check(p, TOK_COMMENSURABLE) ||
        check(p, TOK_PERPENDICULAR) ||
        check(p, TOK_FROM) || check(p, TOK_THROUGH) ||
        check(p, TOK_STRUCTURE) || check(p, TOK_OUTPUT) ||
        check(p, TOK_CONTEXT) || check(p, TOK_DATA) ||
        check(p, TOK_OPERATOR) || check(p, TOK_IDENTITY) ||
        check(p, TOK_CAST) ||
        check(p, TOK_MEIHUA) || check(p, TOK_ZHULIN) || check(p, TOK_SONGQIAO) ||
        check(p, TOK_IMPORT) ||
        check(p, TOK_JOURNEY) || check(p, TOK_PROGRAM) ||
        check(p, TOK_TERMINUS) || check(p, TOK_FAILURE_MODES) ||
        check(p, TOK_PRESCRIBES) || check(p, TOK_COMPOSITION) ||
        check(p, TOK_GRAPHICS_RULE) || check(p, TOK_APPLIES_TO) ||
        check(p, TOK_FOREGROUND_VECTOR) || check(p, TOK_BACKGROUND_VECTOR) ||
        check(p, TOK_MIN_CONTRAST) || check(p, TOK_FAILURE_MODE) ||
        check(p, TOK_DECIDABLE) || check(p, TOK_UNDECIDABLE) ||
        check(p, TOK_IF) || check(p, TOK_THEN) || check(p, TOK_ELSE) ||
        check(p, TOK_INVARIANT) || check(p, TOK_YIELDS) || check(p, TOK_CASES) ||
        check(p, TOK_YIELD) || check(p, TOK_CARRIES) ||
        check(p, TOK_OPENS) || check(p, TOK_GOVERNED_BY) ||
        check(p, TOK_PRESERVES) || check(p, TOK_CHANGES)) {
        parser_advance(p);
        char *name = token_text(p->previous);
        int line = p->previous.line, col = p->previous.col;

        /* Dotted access: a.b.c */
        while (match(p, TOK_DOT)) {
            if (!check(p, TOK_IDENT) && !check(p, TOK_X)) break;
            parser_advance(p);
            char *field = token_text(p->previous);
            size_t new_len = strlen(name) + 1 + strlen(field) + 1;
            char *dotted = malloc(new_len);
            snprintf(dotted, new_len, "%s.%s", name, field);
            free(name);
            free(field);
            name = dotted;
        }

        /* Function call: name(args) */
        if (check(p, TOK_LPAREN)) {
            parser_advance(p);
            Expr *e = expr_new(EXPR_CALL, line, col);
            e->as.call.callee = name;
            size_t cap = 8;
            e->as.call.args = malloc(cap * sizeof(Expr *));
            e->as.call.arg_count = 0;

            if (!check(p, TOK_RPAREN)) {
                do {
                    if (e->as.call.arg_count >= cap) {
                        cap *= 2;
                        e->as.call.args = realloc(e->as.call.args, cap * sizeof(Expr *));
                    }
                    e->as.call.args[e->as.call.arg_count++] = parse_expr(p);
                } while (match(p, TOK_COMMA));
            }
            expect(p, TOK_RPAREN, ")");
            return e;
        }

        Expr *e = expr_new(EXPR_IDENT, line, col);
        e->as.ident.name = name;
        return e;
    }

    /* Kind sigil in expression context */
    if (check(p, TOK_XI) || check(p, TOK_ZETA) || check(p, TOK_OMEGA) ||
        check(p, TOK_RK) || check(p, TOK_DELTA_RK)) {
        parser_advance(p);
        Expr *e = expr_new(EXPR_IDENT, p->previous.line, p->previous.col);
        e->as.ident.name = token_text(p->previous);
        return e;
    }

    diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
               p->current.line, p->current.col,
               "expected expression, got '%s'", token_type_name(p->current.type));
    parser_advance(p); /* consume to avoid infinite loop */
    return expr_new(EXPR_IDENT, p->previous.line, p->previous.col);
}

/* ------------------------------------------------------------ */
/* Pratt precedence climbing                                    */
/* ------------------------------------------------------------ */

/* M5: Precedence levels — generated from ordbok/compiler/compiler_prec_levels.szh */
#include "gen/prec_levels.h"
#include "gen/token_prec.h"

/* Get the infix precedence of the current token, or 0 if not infix.
 * M5: delegates to generated infix_token_prec() for clean token mappings.
 * TOK_PIPE requires conditional logic (length check) that can't be
 * encoded as a pure projection — handled here as a hand-written edge case. */
static int infix_prec(Parser *p) {
    /* TOK_PIPE: single | is enum separator, not pipe operator */
    if (p->current.type == TOK_PIPE) {
        if (p->current.length == 1 &&
            !(p->current.start[0] == '|' && p->current.start[1] == '>'))
            return PREC_ENUM_PREC;
        return PREC_NONE;
    }
    /* All other tokens: generated lookup (tok → string → prec enum) */
    return prec_from_name(infix_token_prec(p->current.type));
}

/* Map token type to operator string for EXPR_BINARY */
static const char *op_str(TokenType t) {
    switch (t) {
    case TOK_PLUS:          return "+";
    case TOK_MINUS:         return "-";
    case TOK_STAR:          return "*";
    case TOK_SLASH:         return "/";
    case TOK_PERCENT:       return "%";
    case TOK_CARET:         return "^";
    case TOK_DOUBLE_EQUALS: return "==";
    case TOK_BANG_EQ:       return "!=";
    case TOK_LT:            return "<";
    case TOK_GT:            return ">";
    case TOK_LT_EQ:         return "<=";
    case TOK_GT_EQ:         return ">=";
    case TOK_AND_AND:       return "&&";
    case TOK_OR_OR:         return "||";
    default:                return "?";
    }
}

static Expr *parse_prec(Parser *p, int min_prec);

static Expr *parse_prefix(Parser *p) {
    /* Unary prefix: - and ! */
    if (check(p, TOK_MINUS) || check(p, TOK_BANG)) {
        parser_advance(p);
        const char *op = p->previous.type == TOK_MINUS ? "-" : "!";
        int line = p->previous.line, col = p->previous.col;
        Expr *operand = parse_prec(p, PREC_POWER); /* unary binds tighter than most */
        Expr *e = expr_new(EXPR_UNARY, line, col);
        e->as.unary.op = strdup(op);
        e->as.unary.operand = operand;
        return e;
    }

    /* if/then/else conditional expression —
     * but not when 'if' appears as a dimension member (followed by | or newline) */
    if (check(p, TOK_IF) && !peek_is_enum_sep(p)) {
        parser_advance(p);
        int line = p->previous.line, col = p->previous.col;
        Expr *condition = parse_prec(p, PREC_NONE + 1);
        if (!check(p, TOK_THEN)) {
            diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                       p->current.line, p->current.col,
                       "expected 'then' after if condition");
        } else {
            parser_advance(p); /* consume 'then' */
        }
        Expr *then_branch = parse_prec(p, PREC_NONE + 1);
        if (!check(p, TOK_ELSE)) {
            diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                       p->current.line, p->current.col,
                       "expected 'else' in if expression (all branches required)");
        } else {
            parser_advance(p); /* consume 'else' */
        }
        /* else branch parses at lowest prec to allow nested if */
        Expr *else_branch = parse_prec(p, PREC_NONE + 1);
        Expr *e = expr_new(EXPR_IF, line, col);
        e->as.if_expr.condition = condition;
        e->as.if_expr.then_branch = then_branch;
        e->as.if_expr.else_branch = else_branch;
        return e;
    }

    return parse_primary(p);
}

static Expr *parse_prec(Parser *p, int min_prec) {
    Expr *left = parse_prefix(p);

    for (;;) {
        int prec = infix_prec(p);
        if (prec < min_prec) break;

        TokenType op_tok = p->current.type;

        /* Special handling for enum (|) — builds EXPR_ENUM */
        if (op_tok == TOK_PIPE) {
            parser_advance(p);
            Expr *e = expr_new(EXPR_ENUM, left->line, left->col);
            size_t cap = 8;
            e->as.enumeration.items = malloc(cap * sizeof(Variant));
            e->as.enumeration.count = 0;

            if (left->type == EXPR_IDENT) {
                e->as.enumeration.items[e->as.enumeration.count].name =
                    name_new(left->as.ident.name, left->line, left->col);
                e->as.enumeration.count++;
            }
            Expr *next = parse_prefix(p);
            if (next->type == EXPR_IDENT) {
                if (e->as.enumeration.count >= cap) {
                    cap *= 2;
                    e->as.enumeration.items = realloc(e->as.enumeration.items, cap * sizeof(Variant));
                }
                e->as.enumeration.items[e->as.enumeration.count].name =
                    name_new(next->as.ident.name, next->line, next->col);
                e->as.enumeration.count++;
            }
            while (match(p, TOK_PIPE)) {
                next = parse_prefix(p);
                if (next->type == EXPR_IDENT) {
                    if (e->as.enumeration.count >= cap) {
                        cap *= 2;
                        e->as.enumeration.items = realloc(e->as.enumeration.items, cap * sizeof(Variant));
                    }
                    e->as.enumeration.items[e->as.enumeration.count].name =
                        name_new(next->as.ident.name, next->line, next->col);
                    e->as.enumeration.count++;
                }
            }
            left = e;
            continue;
        }

        /* Special handling for arrow (->)  — builds EXPR_ARROW */
        if (op_tok == TOK_ARROW) {
            parser_advance(p);
            Expr *right = parse_prec(p, prec + 1);
            Expr *e = expr_new(EXPR_ARROW, left->line, left->col);
            e->as.arrow.from = left;
            e->as.arrow.to = right;
            left = e;
            continue;
        }

        /* Special handling for pipe chain (|>) — builds EXPR_PIPE_CHAIN */
        if (op_tok == TOK_PIPE_ARROW) {
            parser_advance(p);
            Expr *e = expr_new(EXPR_PIPE_CHAIN, left->line, left->col);
            size_t cap = 8;
            e->as.pipe_chain.stages = malloc(cap * sizeof(Expr *));
            e->as.pipe_chain.stages[0] = left;
            e->as.pipe_chain.count = 1;
            e->as.pipe_chain.stages[e->as.pipe_chain.count++] = parse_prefix(p);
            while (match(p, TOK_PIPE_ARROW)) {
                if (e->as.pipe_chain.count >= cap) {
                    cap *= 2;
                    e->as.pipe_chain.stages = realloc(e->as.pipe_chain.stages, cap * sizeof(Expr *));
                }
                e->as.pipe_chain.stages[e->as.pipe_chain.count++] = parse_prefix(p);
            }
            left = e;
            continue;
        }

        /* Special handling for coalesce (??) — builds EXPR_COALESCE */
        if (op_tok == TOK_DOUBLE_QUESTION) {
            parser_advance(p);
            Expr *right = parse_prec(p, prec + 1);
            Expr *e = expr_new(EXPR_COALESCE, left->line, left->col);
            e->as.coalesce.left = left;
            e->as.coalesce.right = right;
            left = e;
            continue;
        }

        /* Special handling for cross product (×) — builds EXPR_CROSS */
        if (op_tok == TOK_CROSS) {
            parser_advance(p);
            Expr *right = parse_prec(p, prec + 1);
            Expr *e = expr_new(EXPR_CROSS, left->line, left->col);
            e->as.cross.left = left;
            e->as.cross.right = right;
            left = e;
            continue;
        }

        /* Generic binary operators — builds EXPR_BINARY */
        parser_advance(p);
        /* Right-associative for ^ (power), left-associative for everything else */
        int next_prec = (op_tok == TOK_CARET) ? prec : prec + 1;
        Expr *right = parse_prec(p, next_prec);
        Expr *e = expr_new(EXPR_BINARY, left->line, left->col);
        e->as.binary.left = left;
        e->as.binary.op = strdup(op_str(op_tok));
        e->as.binary.right = right;
        left = e;
    }

    return left;
}

static Expr *parse_expr(Parser *p) {
    return parse_prec(p, PREC_NONE + 1);
}

/* ------------------------------------------------------------ */
/* Declaration parsing                                           */
/* ------------------------------------------------------------ */

static DeclField parse_field(Parser *p) {
    DeclField f;
    memset(&f, 0, sizeof(f));

    parser_advance(p);
    f.label = token_to_name(p->previous);

    if (match(p, TOK_COLON) || match(p, TOK_EQUALS)) {
        f.value = parse_expr(p);
    }
    return f;
}

/* Skip to end of current block/line for error recovery */
static void synchronize(Parser *p) {
    while (!check(p, TOK_EOF)) {
        if (check(p, TOK_NEWLINE) || check(p, TOK_DEDENT)) {
            parser_advance(p);
            return;
        }
        /* Top-level declaration starters */
        if (check(p, TOK_XI) || check(p, TOK_ZETA) || check(p, TOK_OMEGA) ||
            check(p, TOK_RK) || check(p, TOK_DELTA_RK) || check(p, TOK_X) ||
            check(p, TOK_UNIT) || check(p, TOK_DIMENSION) ||
            check(p, TOK_DEPENDENCY) || check(p, TOK_MORPHISM) ||
            check(p, TOK_PROJECTION) || check(p, TOK_TRAVERSAL) ||
            check(p, TOK_INCOMMENSURABLE) || check(p, TOK_COMMENSURABLE) ||
            check(p, TOK_PERPENDICULAR) || check(p, TOK_CONTAINMENT) ||
            check(p, TOK_MEIHUA) || check(p, TOK_ZHULIN) ||
            check(p, TOK_SONGQIAO)) {
            return;
        }
        parser_advance(p);
    }
}

/* Parse a name list: a, b, c */
static void parse_name_list(Parser *p, Name **names, size_t *count) {
    size_t cap = 8;
    *names = malloc(cap * sizeof(Name));
    *count = 0;

    do {
        skip_newlines(p);
        if (!check(p, TOK_IDENT) && !check(p, TOK_X)) break;
        parser_advance(p);
        if (*count >= cap) {
            cap *= 2;
            *names = realloc(*names, cap * sizeof(Name));
        }
        (*names)[(*count)++] = token_to_name(p->previous);
    } while (match(p, TOK_COMMA));
}

/*
 * Parse a typed parameter list: a : type, b : type, c
 * Each param may optionally have `: type_name` after it.
 * Populates parallel arrays: names and types (types[i].text == NULL if untyped).
 */
static void parse_typed_param_list(Parser *p, Name **names, Name **types, size_t *count) {
    size_t cap = 8;
    *names = malloc(cap * sizeof(Name));
    *types = malloc(cap * sizeof(Name));
    *count = 0;

    do {
        skip_newlines(p);
        if (!check(p, TOK_IDENT) && !check(p, TOK_X)) break;
        parser_advance(p);
        if (*count >= cap) {
            cap *= 2;
            *names = realloc(*names, cap * sizeof(Name));
            *types = realloc(*types, cap * sizeof(Name));
        }
        (*names)[*count] = token_to_name(p->previous);

        /* Check for optional type annotation: `: type_name` */
        if (match(p, TOK_COLON)) {
            skip_newlines(p);
            if (check(p, TOK_IDENT) || check(p, TOK_UNIT) ||
                check(p, TOK_MAGNITUDE) || check(p, TOK_VECTOR) ||
                check(p, TOK_DIMENSION)) {
                parser_advance(p);
                (*types)[*count] = token_to_name(p->previous);
            } else {
                diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                           p->current.line, p->current.col,
                           "expected type name after ':'");
                (*types)[*count] = name_new(NULL, 0, 0);
            }
        } else {
            /* No type annotation — untyped parameter */
            (*types)[*count] = name_new(NULL, 0, 0);
        }
        (*count)++;
    } while (match(p, TOK_COMMA));
}

/* Parse body fields in an indented block */
static void parse_body_fields(Parser *p, DeclField **fields, size_t *count) {
    size_t cap = 8;
    *fields = malloc(cap * sizeof(DeclField));
    *count = 0;

    if (!match(p, TOK_INDENT)) return;

    while (!check(p, TOK_DEDENT) && !check(p, TOK_EOF)) {
        skip_newlines(p);
        if (check(p, TOK_DEDENT) || check(p, TOK_EOF)) break;

        if (*count >= cap) {
            cap *= 2;
            *fields = realloc(*fields, cap * sizeof(DeclField));
        }

        if (check(p, TOK_IDENT) || check(p, TOK_CARRIES) ||
            check(p, TOK_STRUCTURE) || check(p, TOK_PRESERVES) ||
            check(p, TOK_CHANGES) || check(p, TOK_OPENS) ||
            check(p, TOK_GOVERNED_BY) || check(p, TOK_INVARIANT) ||
            check(p, TOK_CONTEXT) || check(p, TOK_DATA) ||
            check(p, TOK_OPERATOR) || check(p, TOK_OUTPUT) ||
            check(p, TOK_CAST) || check(p, TOK_YIELDS) ||
            check(p, TOK_TERMINUS) || check(p, TOK_FAILURE_MODES) ||
            check(p, TOK_PRESCRIBES) || check(p, TOK_COMPOSITION) ||
            check(p, TOK_APPLIES_TO) || check(p, TOK_FOREGROUND_VECTOR) ||
            check(p, TOK_BACKGROUND_VECTOR) || check(p, TOK_MIN_CONTRAST) ||
            check(p, TOK_FAILURE_MODE) ||
            check(p, TOK_RK) || check(p, TOK_XI) || check(p, TOK_ZETA) ||
            check(p, TOK_OMEGA)) {
            (*fields)[(*count)++] = parse_field(p);
        } else {
            parser_advance(p);
        }
        skip_newlines(p);
    }
    match(p, TOK_DEDENT);
}

/* ------------------------------------------------------------ */
/* Top-level declaration parsers                                 */
/* ------------------------------------------------------------ */

static Decl *parse_unit_like(Parser *p, DeclType dtype) {
    /* unit/magnitude/vector name */
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected name after type keyword");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(dtype, KIND_NONE, name, name.line);
    return d;
}

static Decl *parse_dimension(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected dimension name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_DIMENSION, KIND_NONE, name, name.line);
    if (match(p, TOK_COLON)) {
        d->as.dimension.members = parse_expr(p);
    }
    return d;
}

static Decl *parse_dependency(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected dependency name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_DEPENDENCY, KIND_NONE, name, name.line);

    if (match(p, TOK_COLON)) {
        d->as.dependency.relation = parse_expr(p);
    }
    skip_newlines(p);

    parse_body_fields(p, &d->as.dependency.fields, &d->as.dependency.field_count);
    return d;
}

static Decl *parse_containment(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected containment name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_CONTAINMENT, KIND_NONE, name, name.line);
    if (match(p, TOK_COLON)) {
        d->as.containment.members = parse_expr(p);
    }
    return d;
}

static Decl *parse_morphism(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected morphism name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_MORPHISM, KIND_NONE, name, name.line);

    if (match(p, TOK_COLON)) {
        d->as.morphism.signature = parse_expr(p);
    }
    skip_newlines(p);

    parse_body_fields(p, &d->as.morphism.fields, &d->as.morphism.field_count);
    return d;
}

/* Parse a journey declaration:
 *   journey <name>:
 *     xi: { ... }
 *     zeta: { ... }
 *     rk: { ... }
 *     omega: { ... }
 *     terminus: <expr>
 *     failure_modes: { ... }
 * The journey-unit per the constitution's journey ordbok entry —
 * an ordered, irreversible sequence of situated traversals through
 * which an identity is transformed by context. */
static Decl *parse_journey(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected journey name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_JOURNEY, KIND_NONE, name, name.line);

    /* Optional colon before the body */
    match(p, TOK_COLON);
    skip_newlines(p);

    parse_body_fields(p, &d->as.journey.fields, &d->as.journey.field_count);
    return d;
}

/* Parse a program declaration:
 *   program <name>:
 *     prescribes: [<journey_name>, ...]
 *     rk: <int>
 *     composition: <serial | parallel | conditional>
 * The program-primitive per D55 — a prescribed journey. */
static Decl *parse_program(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected program name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_PROGRAM, KIND_NONE, name, name.line);

    /* Optional colon before the body */
    match(p, TOK_COLON);
    skip_newlines(p);

    parse_body_fields(p, &d->as.program_decl.fields, &d->as.program_decl.field_count);
    return d;
}

/* Parse a graphics_rule declaration:
 *   graphics_rule <name>:
 *     applies_to: <surface_class>
 *     foreground_vector: <hex|token>
 *     background_vector: <hex|token>
 *     min_contrast: <ratio>
 *     failure_mode: <invisible | low_contrast | inverted>
 * Phase 5b — a contrast/visibility constraint on a surface class. */
static Decl *parse_graphics_rule(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected graphics_rule name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_GRAPHICS_RULE, KIND_NONE, name, name.line);

    /* Optional colon before the body */
    match(p, TOK_COLON);
    skip_newlines(p);

    parse_body_fields(p, &d->as.graphics_rule.fields, &d->as.graphics_rule.field_count);
    return d;
}

/* Parse projection case arms: (pattern, pattern) -> result */
static void parse_projection_cases(Parser *p, ProjectionArm **arms, size_t *arm_count) {
    size_t cap = 16;
    *arms = malloc(cap * sizeof(ProjectionArm));
    *arm_count = 0;

    /* Expect INDENT after cases: */
    if (!match(p, TOK_INDENT)) return;

    while (!check(p, TOK_DEDENT) && !check(p, TOK_EOF)) {
        skip_newlines(p);
        if (check(p, TOK_DEDENT) || check(p, TOK_EOF)) break;

        if (*arm_count >= cap) {
            cap *= 2;
            *arms = realloc(*arms, cap * sizeof(ProjectionArm));
        }

        /* parse_expr will consume the entire "(a, b) -> result" as an EXPR_ARROW.
         * We decompose it: pattern = arrow.from, body = arrow.to */
        Expr *full = parse_expr(p);

        ProjectionArm *arm = &(*arms)[(*arm_count)++];
        if (full && full->type == EXPR_ARROW) {
            arm->pattern = full->as.arrow.from;
            arm->body = full->as.arrow.to;
        } else {
            /* No arrow — pattern only (shouldn't happen in well-formed cases) */
            arm->pattern = full;
            arm->body = NULL;
        }

        skip_newlines(p);
    }
    match(p, TOK_DEDENT);
}

static Decl *parse_projection(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected projection name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_PROJECTION, KIND_NONE, name, name.line);

    if (match(p, TOK_COLON)) {
        /* Skip to newline — projection header may have extra syntax */
        while (!check(p, TOK_NEWLINE) && !check(p, TOK_EOF)) {
            parser_advance(p);
        }
    }
    skip_newlines(p);

    /* Parse body: fields (invariant, context, yields) and then cases */
    size_t field_cap = 8;
    d->as.projection.fields = malloc(field_cap * sizeof(DeclField));
    d->as.projection.field_count = 0;
    d->as.projection.arms = NULL;
    d->as.projection.arm_count = 0;

    if (match(p, TOK_INDENT)) {
        while (!check(p, TOK_DEDENT) && !check(p, TOK_EOF)) {
            skip_newlines(p);
            if (check(p, TOK_DEDENT) || check(p, TOK_EOF)) break;

            /* Check for cases: keyword — switch to case arm parsing */
            if (check(p, TOK_CASES) || (check(p, TOK_IDENT) &&
                p->current.length == 5 &&
                memcmp(p->current.start, "cases", 5) == 0)) {
                parser_advance(p);
                match(p, TOK_COLON);
                skip_newlines(p);
                parse_projection_cases(p, &d->as.projection.arms, &d->as.projection.arm_count);
            }
            /* Standard fields: invariant ξ name, context ζ name, yields ω name */
            else if (check(p, TOK_INVARIANT) || check(p, TOK_CONTEXT) ||
                     check(p, TOK_YIELDS)) {
                if (d->as.projection.field_count >= field_cap) {
                    field_cap *= 2;
                    d->as.projection.fields = realloc(d->as.projection.fields,
                                                       field_cap * sizeof(DeclField));
                }
                DeclField f;
                memset(&f, 0, sizeof(f));
                parser_advance(p);
                f.label = token_to_name(p->previous);

                /* Skip optional kind sigil (ξ, ζ, ω, etc.) */
                if (check(p, TOK_XI) || check(p, TOK_ZETA) || check(p, TOK_OMEGA) ||
                    check(p, TOK_X) || check(p, TOK_RK) || check(p, TOK_DELTA_RK)) {
                    parser_advance(p);
                }
                /* Optionally consume colon */
                match(p, TOK_COLON);

                /* Parse the actual name/value */
                if (!check(p, TOK_NEWLINE) && !check(p, TOK_DEDENT) && !check(p, TOK_EOF)) {
                    f.value = parse_expr(p);
                }
                d->as.projection.fields[d->as.projection.field_count++] = f;
            }
            else if (check(p, TOK_IDENT) || check(p, TOK_DATA) ||
                     check(p, TOK_OPERATOR) || check(p, TOK_OUTPUT) ||
                     check(p, TOK_CAST)) {
                if (d->as.projection.field_count >= field_cap) {
                    field_cap *= 2;
                    d->as.projection.fields = realloc(d->as.projection.fields,
                                                       field_cap * sizeof(DeclField));
                }
                d->as.projection.fields[d->as.projection.field_count++] = parse_field(p);
            } else {
                parser_advance(p);
            }
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
    }

    return d;
}

static Decl *parse_traversal(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected traversal name");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_TRAVERSAL, KIND_NONE, name, name.line);

    if (match(p, TOK_COLON)) {
        /* Skip trailing content on declaration line */
        while (!check(p, TOK_NEWLINE) && !check(p, TOK_EOF)) {
            parser_advance(p);
        }
    }
    skip_newlines(p);

    /* Parse traversal sections as body fields */
    size_t cap = 8;
    d->as.traversal.sections = malloc(cap * sizeof(TraversalSection));
    d->as.traversal.section_count = 0;

    if (match(p, TOK_INDENT)) {
        while (!check(p, TOK_DEDENT) && !check(p, TOK_EOF)) {
            skip_newlines(p);
            if (check(p, TOK_DEDENT) || check(p, TOK_EOF)) break;

            /* Detect section headers: identity ξ :, context ζ :, etc. */
            Kind section_kind = KIND_NONE;
            if (check(p, TOK_IDENTITY)) { section_kind = KIND_XI; }
            else if (check(p, TOK_CONTEXT)) { section_kind = KIND_ZETA; }
            else if (check(p, TOK_DATA)) { section_kind = KIND_X; }
            else if (check(p, TOK_OPERATOR)) { section_kind = KIND_RK; }
            else if (check(p, TOK_OUTPUT)) { section_kind = KIND_OMEGA; }
            else if (check(p, TOK_CAST)) { section_kind = KIND_DELTA_RK; }

            if (section_kind != KIND_NONE) {
                parser_advance(p); /* consume section keyword (identity/context/etc.) */

                /* Skip optional kind sigil */
                if (check(p, TOK_XI) || check(p, TOK_ZETA) || check(p, TOK_X) ||
                    check(p, TOK_RK) || check(p, TOK_OMEGA) || check(p, TOK_DELTA_RK)) {
                    parser_advance(p);
                }

                /* The label is the next identifier */
                Name label;
                if (check(p, TOK_IDENT) || check(p, TOK_X)) {
                    parser_advance(p);
                    label = token_to_name(p->previous);
                } else {
                    /* Fall back to using the section keyword as label */
                    label = token_to_name(p->previous);
                }

                /* Optional colon introduces body */
                match(p, TOK_COLON);

                /* Parse the section body */
                Expr *body = NULL;
                if (!check(p, TOK_NEWLINE) && !check(p, TOK_INDENT)) {
                    body = parse_expr(p);
                }
                skip_newlines(p);

                /* Nested indent block for section body — capture into block expr */
                if (check(p, TOK_INDENT)) {
                    parser_advance(p);
                    int body_depth = 1; /* track nested indent/dedent within body */
                    Expr *block = expr_new(EXPR_BLOCK, p->current.line, p->current.col);
                    size_t bcap = 8;
                    block->as.block.stmts = malloc(bcap * sizeof(Expr *));
                    block->as.block.count = 0;
                    while (body_depth > 0 && !check(p, TOK_EOF)) {
                        skip_newlines(p);
                        /* Track nested indentation within the body block */
                        if (check(p, TOK_INDENT)) {
                            parser_advance(p);
                            body_depth++;
                            continue;
                        }
                        if (check(p, TOK_DEDENT)) {
                            body_depth--;
                            if (body_depth > 0) {
                                parser_advance(p); /* consume inner dedent */
                                continue;
                            }
                            break; /* outer dedent — end of body */
                        }
                        Expr *stmt;
                        if (check(p, TOK_PIPE_ARROW) && block->as.block.count > 0) {
                            /* |> at start of line: continuation of previous expression */
                            stmt = block->as.block.stmts[--block->as.block.count]; /* pop previous */
                            while (check(p, TOK_PIPE_ARROW)) {
                                parser_advance(p);
                                Expr *next_stage = parse_prefix(p);
                                if (stmt->type == EXPR_PIPE_CHAIN) {
                                    stmt->as.pipe_chain.stages = realloc(
                                        stmt->as.pipe_chain.stages,
                                        (stmt->as.pipe_chain.count + 1) * sizeof(Expr *));
                                    stmt->as.pipe_chain.stages[stmt->as.pipe_chain.count++] = next_stage;
                                } else {
                                    Expr *chain = expr_new(EXPR_PIPE_CHAIN, stmt->line, stmt->col);
                                    chain->as.pipe_chain.stages = malloc(8 * sizeof(Expr *));
                                    chain->as.pipe_chain.stages[0] = stmt;
                                    chain->as.pipe_chain.stages[1] = next_stage;
                                    chain->as.pipe_chain.count = 2;
                                    stmt = chain;
                                }
                            }
                        } else {
                            stmt = parse_expr(p);
                        }
                        if (block->as.block.count >= bcap) {
                            bcap *= 2;
                            block->as.block.stmts = realloc(block->as.block.stmts, bcap * sizeof(Expr *));
                        }
                        block->as.block.stmts[block->as.block.count++] = stmt;
                        skip_newlines(p);
                    }
                    match(p, TOK_DEDENT);
                    /* If no inline body, the block becomes the body.
                     * If inline body exists, block extends it. */
                    if (!body) {
                        if (block->as.block.count == 1) {
                            body = block->as.block.stmts[0]; /* unwrap single-expr block */
                        } else if (block->as.block.count > 0) {
                            body = block;
                        }
                    } else {
                        /* Combine: wrap inline + block into a block */
                        Expr *combined = expr_new(EXPR_BLOCK, body->line, body->col);
                        combined->as.block.stmts = malloc((block->as.block.count + 1) * sizeof(Expr *));
                        combined->as.block.stmts[0] = body;
                        for (size_t bi = 0; bi < block->as.block.count; bi++) {
                            combined->as.block.stmts[bi + 1] = block->as.block.stmts[bi];
                        }
                        combined->as.block.count = block->as.block.count + 1;
                        body = combined;
                    }
                }

                if (d->as.traversal.section_count >= cap) {
                    cap *= 2;
                    d->as.traversal.sections = realloc(
                        d->as.traversal.sections, cap * sizeof(TraversalSection));
                }
                TraversalSection *sec = &d->as.traversal.sections[d->as.traversal.section_count++];
                sec->section_kind = section_kind;
                sec->label = label;
                sec->body = body;
            } else {
                /* Unknown section — skip */
                parser_advance(p);
                skip_newlines(p);
            }
        }
        match(p, TOK_DEDENT);
    }

    return d;
}

static Decl *parse_relation(Parser *p, DeclType type) {
    /* incommensurable a, b or perpendicular a, b */
    Name *names;
    size_t count;
    parse_name_list(p, &names, &count);

    if (count < 2) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->previous.line, p->previous.col,
                   "%s requires at least two names", decl_type_name(type));
    }

    Name decl_name = count > 0 ? name_dup(names[0]) : name_new("<error>", p->previous.line, p->previous.col);
    Decl *d = decl_new(type, KIND_NONE, decl_name, p->previous.line);
    d->as.relation.names = names;
    d->as.relation.count = count;
    return d;
}

static Decl *parse_kinded_value(Parser *p, Kind kind) {
    /* ξ name : value */
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected name after kind sigil");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Decl *d = decl_new(DECL_KINDED_VALUE, kind, name, name.line);

    if (match(p, TOK_COLON)) {
        d->as.kinded.value = parse_expr(p);
    }
    return d;
}

static Decl *parse_exec_layer(Parser *p, DeclType type) {
    /* meihua name(params) : body */
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected name after %s", decl_type_name(type));
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    define_name(p, name.text);

    Kind kind = KIND_NONE;
    if (type == DECL_MEIHUA) kind = KIND_XI;  /* pure expressions */
    else if (type == DECL_ZHULIN) kind = KIND_RK; /* flow control */
    else if (type == DECL_SONGQIAO) kind = KIND_ZETA; /* runtime context */

    Decl *d = decl_new(type, kind, name, name.line);

    /* Optional params — with optional type annotations */
    d->as.exec_layer.param_types = NULL;
    if (match(p, TOK_LPAREN)) {
        parse_typed_param_list(p,
            &d->as.exec_layer.params,
            &d->as.exec_layer.param_types,
            &d->as.exec_layer.param_count);
        expect(p, TOK_RPAREN, ")");
    }

    if (match(p, TOK_COLON)) {
        /* Parse body — inline or indented */
        if (!check(p, TOK_NEWLINE)) {
            d->as.exec_layer.body = parse_expr(p);
        }
    }
    skip_newlines(p);

    /* Indented body */
    if (check(p, TOK_INDENT)) {
        parser_advance(p);
        Expr *block = expr_new(EXPR_BLOCK, p->previous.line, p->previous.col);
        size_t cap = 16;
        block->as.block.stmts = malloc(cap * sizeof(Expr *));
        block->as.block.count = 0;

        while (!check(p, TOK_DEDENT) && !check(p, TOK_EOF)) {
            skip_newlines(p);
            if (check(p, TOK_DEDENT) || check(p, TOK_EOF)) break;
            if (block->as.block.count >= cap) {
                cap *= 2;
                block->as.block.stmts = realloc(block->as.block.stmts, cap * sizeof(Expr *));
            }
            block->as.block.stmts[block->as.block.count++] = parse_expr(p);
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
        d->as.exec_layer.body = block;
    }

    return d;
}

/* ------------------------------------------------------------ */
/* M6: Uniform-signature parse wrappers                          */
/* These adapt parameterized parse functions to the uniform      */
/* Decl* handler(Parser*) signature used by keyword_dispatch.    */
/* ------------------------------------------------------------ */

static Decl *parse_unit_like_unit(Parser *p)         { return parse_unit_like(p, DECL_UNIT); }
static Decl *parse_unit_like_magnitude(Parser *p)    { return parse_unit_like(p, DECL_MAGNITUDE); }
static Decl *parse_unit_like_vector(Parser *p)       { return parse_unit_like(p, DECL_VECTOR); }
static Decl *parse_relation_incommensurable(Parser *p) { return parse_relation(p, DECL_INCOMMENSURABLE); }
static Decl *parse_relation_commensurable(Parser *p)   { return parse_relation(p, DECL_COMMENSURABLE); }
static Decl *parse_relation_perpendicular(Parser *p)   { return parse_relation(p, DECL_PERPENDICULAR); }
static Decl *parse_exec_layer_meihua(Parser *p)      { return parse_exec_layer(p, DECL_MEIHUA); }
static Decl *parse_exec_layer_zhulin(Parser *p)      { return parse_exec_layer(p, DECL_ZHULIN); }
static Decl *parse_exec_layer_songqiao(Parser *p)    { return parse_exec_layer(p, DECL_SONGQIAO); }

/* M6: Extracted import parsing (was inline in parse_declaration) */
static Decl *parse_import(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected module name after 'import'");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name name = token_to_name(p->previous);
    Decl *d = decl_new(DECL_IMPORT, KIND_NONE, name, name.line);
    d->as.import_decl.module_name = strdup(name.text);
    d->as.import_decl.resolved = NULL;
    return d;
}

/* M6: Extracted zero parsing (was inline in parse_declaration) */
static Decl *parse_zero_decl(Parser *p) {
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
                   p->current.line, p->current.col,
                   "expected name after 'zero'");
        synchronize(p);
        return NULL;
    }
    parser_advance(p);
    Name target = token_to_name(p->previous);
    Name name = name_dup(target);
    Decl *d = decl_new(DECL_ZERO, KIND_NONE, name, p->previous.line);
    d->as.zero_decl.target = target;
    return d;
}

/* M6: Generated keyword dispatch — replaces hand-written if/match chain.
 * Source of truth: ordbok/compiler/compiler_keyword_dispatch.szh
 * Includes after all parse function definitions so static functions
 * are accessible. */
#include "gen/keyword_dispatch.h"

/* M6: Generated kind sigil dispatch — replaces 6 if-statements.
 * Source of truth: ordbok/compiler/compiler_kind_sigil_dispatch.szh */
#include "gen/kind_sigil_dispatch.h"

/* ------------------------------------------------------------ */
/* Top-level parser                                              */
/* ------------------------------------------------------------ */

static Decl *parse_declaration(Parser *p) {
    skip_newlines(p);
    if (check(p, TOK_EOF)) return NULL;

    /* M6: Kind sigils — generated cross-dimension dispatch (tok → kind).
     * Replaces 6 hand-written if-statements. */
    Kind k = kind_sigil_dispatch(p->current.type);
    if (k != (Kind)-1) {
        parser_advance(p);
        return parse_kinded_value(p, k);
    }

    /* M6: Keyword dispatch — generated from ordbok.
     * All parse functions expect the keyword already consumed.
     * keyword_dispatch returns NULL for non-keyword tokens, so we
     * use a two-step: peek to see if the dispatch would handle it,
     * then advance + dispatch. The trick: we save/restore the parser
     * state only if needed. Since keyword_dispatch is a pure switch
     * when the token is NOT a keyword (returns NULL immediately without
     * touching Parser*), we can safely call it as a test. For keyword
     * tokens, the handler will run — so we must advance first. */
    {
        tok_t kw = p->current.type;
        /* Advance past the keyword. If this wasn't a keyword, the error
         * handler below will report it using p->previous. */
        parser_advance(p);
        Decl *d = keyword_dispatch(p, kw);
        if (d) return d;
        /* NULL return from keyword_dispatch means either:
         * (a) kw was not a keyword — dispatch hit default, returned NULL
         *     without calling any handler. p->previous has the bad token.
         * (b) kw was a keyword but the handler returned NULL (error recovery
         *     in parse_import or parse_zero_decl). Error already reported. */
        /* Detect case (b): the handler consumed additional tokens via
         * synchronize(), so p->current moved further. In case (a),
         * no handler ran, so only our one advance happened. We can
         * distinguish by checking if kw was in the dispatch table. */
        switch (kw) {
        case TOK_UNIT: case TOK_MAGNITUDE: case TOK_VECTOR:
        case TOK_DIMENSION: case TOK_DEPENDENCY: case TOK_CONTAINMENT:
        case TOK_MORPHISM: case TOK_PROJECTION: case TOK_TRAVERSAL:
        case TOK_INCOMMENSURABLE: case TOK_COMMENSURABLE: case TOK_PERPENDICULAR:
        case TOK_MEIHUA: case TOK_ZHULIN: case TOK_SONGQIAO:
        case TOK_IMPORT: case TOK_ZERO:
            return NULL;  /* case (b): handler error, already reported */
        default:
            break;  /* case (a): not a keyword, fall through to error */
        }
    }

    /* Unrecognized token — p->previous holds the consumed token
     * (we advanced past it in the dispatch block above). */
    diag_error(p->diags, DIAG_PARSE_ERROR, NULL,
               p->previous.line, p->previous.col,
               "unexpected token '%s' at top level", token_type_name(p->previous.type));
    synchronize(p);
    return NULL;
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

void parser_init(Parser *p, Lexer *lex, DiagList *diags) {
    memset(p, 0, sizeof(Parser));
    p->lexer = lex;
    p->diags = diags;
    parser_advance(p); /* prime the first token */
}

Program *parser_parse(Parser *p, const char *filename) {
    Program *prog = program_new(filename);

    while (!check(p, TOK_EOF)) {
        Decl *d = parse_declaration(p);
        if (d) {
            /* Attach any pending @targets annotation to this declaration */
            if (p->pending_target_count > 0) {
                d->targets = p->pending_targets;
                d->target_count = p->pending_target_count;
                p->pending_targets = NULL;
                p->pending_target_count = 0;
            }
            program_push(prog, d);
        }
    }

    return prog;
}

void parser_free(Parser *p) {
    if (p->defined_names) {
        for (size_t i = 0; i < p->defined_count; i++) {
            free(p->defined_names[i]);
        }
        free(p->defined_names);
    }
    if (p->pending_targets) {
        for (size_t i = 0; i < p->pending_target_count; i++)
            free(p->pending_targets[i]);
        free(p->pending_targets);
    }
}
