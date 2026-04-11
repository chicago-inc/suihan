/*
 * suhc — the suihan compiler
 * lexer.h — tokenizer interface
 */

#ifndef SUHC_LEXER_H
#define SUHC_LEXER_H

#include "token.h"
#include <stdbool.h>

typedef struct {
    const char *source;     /* full source buffer (not owned) */
    const char *current;    /* read cursor */
    const char *start;      /* start of current lexeme */
    int         line;
    int         col;
    int         start_col;

    /* Indentation tracking — significant whitespace */
    int         indent_stack[128];
    int         indent_depth;
    int         pending_dedents;
    bool        at_line_start;
    bool        emit_newline;
} Lexer;

/* Initialize a lexer over a source buffer. */
void  lexer_init(Lexer *lex, const char *source);

/* Produce the next token. */
Token lexer_next(Lexer *lex);

/* Peek at the next token without consuming. */
Token lexer_peek(Lexer *lex);

#endif /* SUHC_LEXER_H */
