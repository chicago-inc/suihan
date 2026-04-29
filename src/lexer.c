/*
 * suhc — lexer.c
 * Tokenizer for .szh source files.
 *
 * Handles UTF-8 kind sigils (ξ, ζ, ω, Δ), significant
 * indentation, and all ordbok-derived keywords.
 */

#include "lexer.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ------------------------------------------------------------ */
/* UTF-8 helpers                                                 */
/* ------------------------------------------------------------ */

/* Check if current position starts with a specific UTF-8 sequence */
static bool match_utf8(const char *p, const char *seq) {
    while (*seq) {
        if (*p != *seq) return false;
        p++; seq++;
    }
    return true;
}

/* UTF-8 byte lengths: ξ = CE BE (2), ζ = CE B6 (2),
 * ω = CF 89 (2), Δ = CE 94 (2) */
static const char UTF8_XI[]    = "\xce\xbe";       /* ξ */
static const char UTF8_ZETA[]  = "\xce\xb6";       /* ζ */
static const char UTF8_OMEGA[] = "\xcf\x89";        /* ω */
static const char UTF8_DELTA[] = "\xce\x94";        /* Δ */
static const char UTF8_CROSS[] = "\xc3\x97";        /* × */
static const char UTF8_ALPHA[] = "\xce\xb1";        /* α */

/* ------------------------------------------------------------ */
/* Lexer internals                                               */
/* ------------------------------------------------------------ */

void lexer_init(Lexer *lex, const char *source) {
    memset(lex, 0, sizeof(Lexer));
    lex->source = source;
    lex->current = source;
    lex->start = source;
    lex->line = 1;
    lex->col = 1;
    lex->start_col = 1;
    lex->indent_stack[0] = 0;
    lex->indent_depth = 0;
    lex->at_line_start = true;
    lex->emit_newline = false;
    lex->pending_dedents = 0;
}

static bool is_at_end(Lexer *lex) {
    return *lex->current == '\0';
}

static char advance(Lexer *lex) {
    char c = *lex->current++;
    if (c == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return c;
}

static char peek(Lexer *lex) {
    return *lex->current;
}

static char peek_next(Lexer *lex) {
    if (is_at_end(lex)) return '\0';
    return lex->current[1];
}

static bool match_char(Lexer *lex, char expected) {
    if (is_at_end(lex) || *lex->current != expected) return false;
    advance(lex);
    return true;
}

static Token make_token(Lexer *lex, TokenType type) {
    Token t;
    t.type = type;
    t.start = lex->start;
    t.length = (size_t)(lex->current - lex->start);
    t.line = lex->line;
    t.col = lex->start_col;
    /* Adjust line for tokens starting before a newline */
    if (type == TOK_NEWLINE || type == TOK_INDENT || type == TOK_DEDENT) {
        /* These tokens are positional markers */
    }
    return t;
}

static Token error_token(Lexer *lex, const char *msg) {
    Token t;
    t.type = TOK_ERROR;
    t.start = msg;
    t.length = strlen(msg);
    t.line = lex->line;
    t.col = lex->col;
    return t;
}

/* ------------------------------------------------------------ */
/* Keyword table                                                 */
/* ------------------------------------------------------------ */

typedef struct {
    const char *word;
    TokenType   type;
} Keyword;

static const Keyword keywords[] = {
    {"unit",            TOK_UNIT},
    {"zero",            TOK_ZERO},
    {"magnitude",       TOK_MAGNITUDE},
    {"vector",          TOK_VECTOR},
    {"dimension",       TOK_DIMENSION},
    {"dependency",      TOK_DEPENDENCY},
    {"containment",     TOK_CONTAINMENT},
    {"morphism",        TOK_MORPHISM},
    {"projection",      TOK_PROJECTION},
    {"traversal",       TOK_TRAVERSAL},
    {"incommensurable", TOK_INCOMMENSURABLE},
    {"commensurable",   TOK_COMMENSURABLE},
    {"perpendicular",   TOK_PERPENDICULAR},
    {"invariant",       TOK_INVARIANT},
    {"context",         TOK_CONTEXT},
    {"data",            TOK_DATA},
    {"operator",        TOK_OPERATOR},
    {"output",          TOK_OUTPUT},
    {"cast",            TOK_CAST},
    {"identity",        TOK_IDENTITY},
    {"yields",          TOK_YIELDS},
    {"cases",           TOK_CASES},
    {"from",            TOK_FROM},
    {"through",         TOK_THROUGH},
    {"yield",           TOK_YIELD},
    {"carries",         TOK_CARRIES},
    {"structure",       TOK_STRUCTURE},
    {"opens",           TOK_OPENS},
    {"governed_by",     TOK_GOVERNED_BY},
    {"preserves",       TOK_PRESERVES},
    {"changes",         TOK_CHANGES},
    {"meihua",          TOK_MEIHUA},
    {"zhulin",          TOK_ZHULIN},
    {"songqiao",        TOK_SONGQIAO},
    {"decidable",       TOK_DECIDABLE},
    {"undecidable",     TOK_UNDECIDABLE},
    {"import",          TOK_IMPORT},
    {"journey",         TOK_JOURNEY},
    {"program",         TOK_PROGRAM},
    {"terminus",        TOK_TERMINUS},
    {"failure_modes",   TOK_FAILURE_MODES},
    {"prescribes",      TOK_PRESCRIBES},
    {"composition",     TOK_COMPOSITION},
    {"if",              TOK_IF},
    {"then",            TOK_THEN},
    {"else",            TOK_ELSE},
    {NULL, TOK_EOF}
};

static TokenType check_keyword(const char *start, size_t length) {
    for (int i = 0; keywords[i].word != NULL; i++) {
        if (strlen(keywords[i].word) == length &&
            memcmp(keywords[i].word, start, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

/* ------------------------------------------------------------ */
/* Scanning functions                                            */
/* ------------------------------------------------------------ */

static void skip_whitespace_inline(Lexer *lex) {
    /* Skip spaces and tabs on the same line — NOT newlines */
    while (!is_at_end(lex) && (*lex->current == ' ' || *lex->current == '\t')) {
        advance(lex);
    }
}

static Token scan_string(Lexer *lex) {
    /* Opening quote already consumed */
    while (!is_at_end(lex) && peek(lex) != '"') {
        if (peek(lex) == '\\') advance(lex); /* skip escaped char */
        if (peek(lex) == '\n') {
            return error_token(lex, "unterminated string");
        }
        advance(lex);
    }
    if (is_at_end(lex)) return error_token(lex, "unterminated string");
    advance(lex); /* closing quote */
    return make_token(lex, TOK_STRING);
}

static Token scan_number(Lexer *lex) {
    /* Hex literal: 0x... or 0X... */
    if ((lex->current - lex->start) == 1 && lex->start[0] == '0' &&
        !is_at_end(lex) && (peek(lex) == 'x' || peek(lex) == 'X')) {
        advance(lex); /* consume 'x' */
        while (!is_at_end(lex) && isxdigit(peek(lex))) advance(lex);
        return make_token(lex, TOK_NUMBER);
    }

    while (!is_at_end(lex) && isdigit(peek(lex))) advance(lex);

    /* Rational: 1/3 */
    if (peek(lex) == '/' && isdigit(peek_next(lex))) {
        advance(lex); /* consume / */
        while (!is_at_end(lex) && isdigit(peek(lex))) advance(lex);
    }
    /* Decimal: 0.33 */
    else if (peek(lex) == '.' && isdigit(peek_next(lex))) {
        advance(lex); /* consume . */
        while (!is_at_end(lex) && isdigit(peek(lex))) advance(lex);
    }

    return make_token(lex, TOK_NUMBER);
}

static Token scan_identifier(Lexer *lex) {
    while (!is_at_end(lex) &&
           (isalnum(peek(lex)) || peek(lex) == '_')) {
        advance(lex);
    }

    size_t length = (size_t)(lex->current - lex->start);

    /* Special: lone 'x' as kind sigil — only at start of declaration */
    if (length == 1 && *lex->start == 'x') {
        return make_token(lex, TOK_X);
    }

    TokenType type = check_keyword(lex->start, length);
    return make_token(lex, type);
}

static Token scan_comment(Lexer *lex) {
    /* -- already consumed, read to end of line */
    while (!is_at_end(lex) && peek(lex) != '\n') {
        advance(lex);
    }
    return make_token(lex, TOK_COMMENT);
}

/* ------------------------------------------------------------ */
/* Indentation handling                                          */
/* ------------------------------------------------------------ */

static int measure_indent(Lexer *lex) {
    int spaces = 0;
    const char *p = lex->current;
    while (*p == ' ') { spaces++; p++; }
    while (*p == '\t') { spaces += 4; p++; } /* tabs = 4 spaces */
    return spaces;
}

/* ------------------------------------------------------------ */
/* Main scanning                                                 */
/* ------------------------------------------------------------ */

Token lexer_next(Lexer *lex) {
    /* Emit pending dedents first */
    if (lex->pending_dedents > 0) {
        lex->pending_dedents--;
        return make_token(lex, TOK_DEDENT);
    }

    /* Handle indentation at start of line */
    if (lex->at_line_start && !is_at_end(lex)) {
        lex->at_line_start = false;
        int indent = measure_indent(lex);

        /* Skip blank lines and comment-only lines */
        const char *p = lex->current;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r' || (*p == '-' && *(p+1) == '-') || *p == '\0') {
            /* Let normal scanning handle it */
            goto scan;
        }

        int current_indent = lex->indent_stack[lex->indent_depth];

        if (indent > current_indent) {
            /* Push new indent level */
            if (lex->indent_depth < 127) {
                lex->indent_depth++;
                lex->indent_stack[lex->indent_depth] = indent;
            }
            /* Advance past whitespace */
            while (!is_at_end(lex) && (*lex->current == ' ' || *lex->current == '\t')) {
                advance(lex);
            }
            lex->start = lex->current;
            lex->start_col = lex->col;
            return make_token(lex, TOK_INDENT);
        } else if (indent < current_indent) {
            /* Pop indent levels */
            while (lex->indent_depth > 0 &&
                   lex->indent_stack[lex->indent_depth] > indent) {
                lex->indent_depth--;
                lex->pending_dedents++;
            }
            /* Advance past whitespace */
            while (!is_at_end(lex) && (*lex->current == ' ' || *lex->current == '\t')) {
                advance(lex);
            }
            lex->start = lex->current;
            lex->start_col = lex->col;
            if (lex->pending_dedents > 0) {
                lex->pending_dedents--;
                return make_token(lex, TOK_DEDENT);
            }
        } else {
            /* Same indent — advance past whitespace */
            while (!is_at_end(lex) && (*lex->current == ' ' || *lex->current == '\t')) {
                advance(lex);
            }
        }
    }

scan:
    skip_whitespace_inline(lex);

    lex->start = lex->current;
    lex->start_col = lex->col;

    if (is_at_end(lex)) {
        /* Emit remaining dedents before EOF */
        if (lex->indent_depth > 0) {
            lex->indent_depth--;
            lex->pending_dedents = lex->indent_depth; /* remaining */
            lex->indent_depth = 0;
            return make_token(lex, TOK_DEDENT);
        }
        return make_token(lex, TOK_EOF);
    }

    /* Check for UTF-8 kind sigils */
    if (match_utf8(lex->current, UTF8_XI)) {
        lex->current += 2; lex->col++;
        return make_token(lex, TOK_XI);
    }
    if (match_utf8(lex->current, UTF8_ZETA)) {
        lex->current += 2; lex->col++;
        return make_token(lex, TOK_ZETA);
    }
    if (match_utf8(lex->current, UTF8_OMEGA)) {
        lex->current += 2; lex->col++;
        return make_token(lex, TOK_OMEGA);
    }
    if (match_utf8(lex->current, UTF8_DELTA)) {
        /* Check for ΔR.k */
        if (match_utf8(lex->current + 2, "R.k")) {
            lex->current += 5; lex->col += 4;
            return make_token(lex, TOK_DELTA_RK);
        }
        lex->current += 2; lex->col++;
        /* Lone Δ — treat as identifier for now */
        return make_token(lex, TOK_IDENT);
    }
    if (match_utf8(lex->current, UTF8_CROSS)) {
        lex->current += 2; lex->col++;
        return make_token(lex, TOK_CROSS);
    }
    if (match_utf8(lex->current, UTF8_ALPHA)) {
        /* α — treat as identifier */
        lex->current += 2; lex->col++;
        /* Continue consuming if followed by alnum */
        while (!is_at_end(lex) && (isalnum(peek(lex)) || peek(lex) == '_')) {
            advance(lex);
        }
        return make_token(lex, TOK_IDENT);
    }

    char c = advance(lex);

    switch (c) {
    /* Newline — emit NEWLINE, mark at_line_start */
    case '\n':
        lex->at_line_start = true;
        return make_token(lex, TOK_NEWLINE);
    case '\r':
        if (peek(lex) == '\n') advance(lex);
        lex->at_line_start = true;
        return make_token(lex, TOK_NEWLINE);

    /* Single-char punctuation */
    case ':': return make_token(lex, TOK_COLON);
    case ',': return make_token(lex, TOK_COMMA);
    case '.': return make_token(lex, TOK_DOT);
    case '(': return make_token(lex, TOK_LPAREN);
    case ')': return make_token(lex, TOK_RPAREN);
    case '[': return make_token(lex, TOK_LBRACKET);
    case ']': return make_token(lex, TOK_RBRACKET);
    case '{': return make_token(lex, TOK_LBRACE);
    case '}': return make_token(lex, TOK_RBRACE);
    case ';': return make_token(lex, TOK_SEMICOLON);
    case '*': return make_token(lex, TOK_STAR);
    case '+': return make_token(lex, TOK_PLUS);
    case '%': return make_token(lex, TOK_PERCENT);
    case '^': return make_token(lex, TOK_CARET);

    /* Potentially multi-char */
    case '-':
        if (match_char(lex, '-')) return scan_comment(lex);
        if (match_char(lex, '>')) return make_token(lex, TOK_ARROW);
        /* Negative number: only if no preceding expr (heuristic: at line start or after operator) */
        if (isdigit(peek(lex)) && lex->start_col <= 1) return scan_number(lex);
        return make_token(lex, TOK_MINUS);

    case '/':
        /* Standalone slash — not inside a number literal (that's handled by scan_number) */
        return make_token(lex, TOK_SLASH);

    case '!':
        if (match_char(lex, '=')) return make_token(lex, TOK_BANG_EQ);
        return make_token(lex, TOK_BANG);

    case '<':
        if (match_char(lex, '=')) return make_token(lex, TOK_LT_EQ);
        return make_token(lex, TOK_LT);

    case '>':
        if (match_char(lex, '=')) return make_token(lex, TOK_GT_EQ);
        return make_token(lex, TOK_GT);

    case '=':
        if (match_char(lex, '=')) return make_token(lex, TOK_DOUBLE_EQUALS);
        if (match_char(lex, '>')) return make_token(lex, TOK_FAT_ARROW);
        return make_token(lex, TOK_EQUALS);

    case '|':
        if (match_char(lex, '>')) return make_token(lex, TOK_PIPE_ARROW);
        if (match_char(lex, '|')) return make_token(lex, TOK_OR_OR);
        return make_token(lex, TOK_PIPE);

    case '&':
        if (match_char(lex, '&')) return make_token(lex, TOK_AND_AND);
        return error_token(lex, "unexpected '&' (did you mean '&&'?)");

    case '?':
        if (match_char(lex, '?')) return make_token(lex, TOK_DOUBLE_QUESTION);
        return make_token(lex, TOK_QUESTION);

    case '_':
        if (!isalnum(peek(lex))) return make_token(lex, TOK_UNDERSCORE);
        /* _identifier — continue as ident */
        while (!is_at_end(lex) && (isalnum(peek(lex)) || peek(lex) == '_')) {
            advance(lex);
        }
        return make_token(lex, TOK_IDENT);

    case '"': return scan_string(lex);

    /* R.k sigil */
    case 'R':
        if (peek(lex) == '.' && peek_next(lex) == 'k') {
            advance(lex); advance(lex); /* consume .k */
            return make_token(lex, TOK_RK);
        }
        /* Fall through to identifier */
        while (!is_at_end(lex) && (isalnum(peek(lex)) || peek(lex) == '_')) {
            advance(lex);
        }
        return make_token(lex, check_keyword(lex->start, (size_t)(lex->current - lex->start)) != TOK_IDENT
                          ? check_keyword(lex->start, (size_t)(lex->current - lex->start))
                          : TOK_IDENT);

    default:
        if (isdigit(c)) return scan_number(lex);
        if (isalpha(c)) return scan_identifier(lex);
        break;
    }

    return error_token(lex, "unexpected character");
}

Token lexer_peek(Lexer *lex) {
    /* Save state */
    Lexer saved = *lex;
    Token t = lexer_next(lex);
    /* Restore state */
    *lex = saved;
    return t;
}
