/*
 * suhc — the suihan compiler
 * parser.h — parser interface
 *
 * The parser builds a Program (stack of Decls) from tokens.
 * It enforces ordbok ordering: each type constructor may only
 * reference types that precede it. A forward reference is a
 * compile error.
 */

#ifndef SUHC_PARSER_H
#define SUHC_PARSER_H

#include "ast.h"
#include "lexer.h"
#include "diagnostic.h"

typedef struct {
    Lexer       *lexer;
    Token        current;
    Token        previous;
    DiagList    *diags;

    /* Ordbok ordering — names defined so far */
    char       **defined_names;
    size_t       defined_count;
    size_t       defined_capacity;

    /* @targets annotation buffer — attached to next declaration */
    char       **pending_targets;
    size_t       pending_target_count;
} Parser;

void     parser_init(Parser *p, Lexer *lex, DiagList *diags);
Program *parser_parse(Parser *p, const char *filename);
void     parser_free(Parser *p);

#endif /* SUHC_PARSER_H */
