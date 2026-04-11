/*
 * suhc — the suihan compiler
 * ast.h — abstract syntax tree definitions
 *
 * The AST is a stack of declarations. Each declaration carries
 * exactly one kind (§2). The parser builds this; the kind checker
 * validates it; the bloat linter audits it.
 */

#ifndef SUHC_AST_H
#define SUHC_AST_H

#include <stddef.h>
#include <stdbool.h>

/* ------------------------------------------------------------
 * The five kinds + entropy cast (§2)
 * Every AST node carries exactly one.
 * Generated from ordbok/compiler/compiler_kinds.szh — M1.
 * ------------------------------------------------------------ */
#include "gen/kinds.h"
typedef kind_t Kind;

/* M2: kind_name() generated from ordbok/compiler/compiler_kind_names.szh */
#include "gen/kind_names.h"

/* ------------------------------------------------------------
 * Declaration types — ordbok-derived
 * Generated from ordbok/compiler/compiler_decl_types.szh — M1.
 * ------------------------------------------------------------ */
#include "gen/decl_types.h"
typedef decl_t DeclType;

/* M2: decl_type_name() generated from ordbok/compiler/compiler_decl_type_names.szh */
#include "gen/decl_type_names.h"

/* ------------------------------------------------------------
 * Name — an identifier with source location
 * ------------------------------------------------------------ */
typedef struct {
    char   *text;
    int     line;
    int     col;
} Name;

/* ------------------------------------------------------------
 * Variant — a single arm of an enum or case
 * ------------------------------------------------------------ */
typedef struct {
    Name    name;
} Variant;

/* ------------------------------------------------------------
 * Expression types
 * Generated from ordbok/compiler/compiler_expr_types.szh — M1.
 * ------------------------------------------------------------ */
#include "gen/expr_types.h"
typedef expr_t ExprType;

typedef struct Expr Expr;

typedef struct {
    Name    pattern;    /* left side of -> in a case arm */
    Expr   *body;       /* right side */
} CaseArm;

struct Expr {
    ExprType type;
    int      line;
    int      col;

    union {
        /* EXPR_IDENT */
        struct { char *name; } ident;

        /* EXPR_NUMBER */
        struct { char *text; } number;  /* preserved as text for rationals */

        /* EXPR_STRING */
        struct { char *value; } string;

        /* EXPR_ENUM: variants separated by | */
        struct { Variant *items; size_t count; } enumeration;

        /* EXPR_LIST: items in [...] */
        struct { Expr **items; size_t count; } list;

        /* EXPR_CALL: name(args) */
        struct { char *callee; Expr **args; size_t arg_count; } call;

        /* EXPR_ARROW: from -> to */
        struct { Expr *from; Expr *to; } arrow;

        /* EXPR_CROSS: a × b */
        struct { Expr *left; Expr *right; } cross;

        /* EXPR_BLOCK: sequence of expressions */
        struct { Expr **stmts; size_t count; } block;

        /* EXPR_CASE */
        struct { CaseArm *arms; size_t count; } cases;

        /* EXPR_PIPE_CHAIN */
        struct { Expr **stages; size_t count; } pipe_chain;

        /* EXPR_BINARY */
        struct { Expr *left; char *op; Expr *right; } binary;

        /* EXPR_COALESCE */
        struct { Expr *left; Expr *right; } coalesce;

        /* EXPR_COMPOUND */
        struct { Name *keys; Expr **values; size_t count; } compound;

        /* EXPR_DECIDABLE / EXPR_UNDECIDABLE */
        struct { Expr *inner; } decidability;

        /* EXPR_UNARY */
        struct { char *op; Expr *operand; } unary;

        /* EXPR_MATCH */
        struct {
            Expr  *discriminant;    /* the value being matched */
            struct {
                Expr *pattern;      /* EXPR_IDENT or EXPR_WILDCARD */
                Expr *body;         /* result expression */
            } *arms;
            size_t arm_count;
        } match_expr;

        /* EXPR_IF */
        struct {
            Expr *condition;
            Expr *then_branch;
            Expr *else_branch;      /* required — no dangling else */
        } if_expr;
    } as;
};

/* ------------------------------------------------------------
 * Sub-components of complex declarations
 * ------------------------------------------------------------ */

/* A named field within a declaration body */
typedef struct {
    Name    label;      /* e.g., "carries", "structure", "preserves" */
    Expr   *value;
} DeclField;

/* A projection case arm: (pattern, pattern) -> expr */
typedef struct {
    Expr   *pattern;    /* tuple pattern or wildcard */
    Expr   *body;       /* result expression */
} ProjectionArm;

/* Traversal section — one of the five equation components */
typedef struct {
    Kind    section_kind;   /* which part of the equation */
    Name    label;          /* e.g., "identity", "context", "data" */
    Expr   *body;           /* the content */
} TraversalSection;

/* ------------------------------------------------------------
 * Declaration — the fundamental AST node
 * A .szh file is a stack of declarations.
 * ------------------------------------------------------------ */
typedef struct Decl Decl;
typedef struct Program Program;  /* forward decl for import_decl */

struct Decl {
    DeclType    type;
    Kind        kind;       /* assigned by parser or kind checker */
    Name        name;       /* primary identifier */
    int         line;       /* source line of declaration start */

    /* @targets annotation: Python/TS identifiers this declaration governs */
    char      **targets;
    size_t      target_count;

    union {
        /* DECL_UNIT */
        struct { /* name is sufficient */ int _unused; } unit;

        /* DECL_ZERO */
        struct { Name target; } zero_decl;

        /* DECL_DIMENSION */
        struct {
            Expr *members;      /* list expression */
        } dimension;

        /* DECL_DEPENDENCY */
        struct {
            Expr       *relation;   /* arrow expression: user -> community */
            DeclField  *fields;
            size_t      field_count;
        } dependency;

        /* DECL_CONTAINMENT */
        struct {
            Expr *members;      /* list expression */
        } containment;

        /* DECL_MORPHISM */
        struct {
            Expr       *signature;  /* arrow: member -> admin */
            DeclField  *fields;
            size_t      field_count;
        } morphism;

        /* DECL_PROJECTION */
        struct {
            DeclField      *fields;         /* invariant, context, yields */
            size_t          field_count;
            ProjectionArm  *arms;
            size_t          arm_count;
        } projection;

        /* DECL_TRAVERSAL */
        struct {
            TraversalSection *sections;
            size_t            section_count;
        } traversal;

        /* DECL_INCOMMENSURABLE / DECL_COMMENSURABLE / DECL_PERPENDICULAR */
        struct {
            Name   *names;
            size_t  count;
        } relation;

        /* DECL_KINDED_VALUE: ξ name : expr */
        struct {
            Expr *value;
        } kinded;

        /* DECL_MEIHUA / DECL_ZHULIN / DECL_SONGQIAO */
        struct {
            Name   *params;
            Name   *param_types;    /* parallel array; .text == NULL if untyped */
            size_t  param_count;
            Expr   *body;
        } exec_layer;

        /* DECL_IMPORT */
        struct {
            char    *module_name;    /* "foundational" — no extension */
            Program *resolved;       /* populated by resolver, NULL initially */
        } import_decl;
    } as;
};

/* ------------------------------------------------------------
 * Program — the top-level: a stack of declarations
 * ------------------------------------------------------------ */
struct Program {
    Decl  **decls;
    size_t  count;
    size_t  capacity;
    char   *filename;
};

/* Constructors */
Program *program_new(const char *filename);
void     program_push(Program *prog, Decl *decl);
void     program_free(Program *prog);

Decl *decl_new(DeclType type, Kind kind, Name name, int line);
Expr *expr_new(ExprType type, int line, int col);

/* Utilities */
Name  name_new(const char *text, int line, int col);
Name  name_dup(Name n);
void  name_free(Name *n);

/* Collect all imported declarations (non-recursive: one level deep).
 * Returns an array of Decl pointers and sets *out_count.
 * Caller must free the array (but not the Decls — they're owned
 * by the imported Programs). */
Decl **collect_imported_decls(Program *prog, size_t *out_count);

#endif /* SUHC_AST_H */
