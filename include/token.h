/*
 * suhc — the suihan compiler
 * token.h — lexer token definitions
 *
 * Every token is a unit. The lexer produces a stack of them.
 */

#ifndef SUHC_TOKEN_H
#define SUHC_TOKEN_H

#include <stddef.h>

/* ------------------------------------------------------------
 * Token types — derived from the language's structural grammar.
 * Generated from ordbok/compiler/compiler_token_types.szh — M1.
 * No token is "unknown." If the lexer can't classify input,
 * it emits TOK_ERROR with the offending span.
 * ------------------------------------------------------------ */
#include "gen/token_types.h"
typedef tok_t TokenType;

/* A token is a span of source with a classification. */
typedef struct {
    TokenType   type;
    const char *start;      /* pointer into source buffer */
    size_t      length;     /* byte length of lexeme */
    int         line;       /* 1-based line number */
    int         col;        /* 1-based column */
} Token;

/* M2: token_type_name() generated from ordbok/compiler/compiler_token_type_names.szh */
#include "gen/token_type_names.h"

#endif /* SUHC_TOKEN_H */
