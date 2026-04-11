/*
 * suhc — the suihan compiler
 * typecheck.h — type checking pass
 *
 * Inserted after kindcheck and before exhaustcheck in the pipeline:
 *   resolve → kindcheck → typecheck → exhaustcheck → perpcheck → ...
 *
 * Validates:
 *   1. Parameter type resolution — annotations reference declared types
 *   2. Call-site arity — argument count matches parameter count
 *   3. Incommensurability enforcement — typed args respect relations
 *   4. Gradual typing — untyped params accept any argument
 *
 * Sprint 6A — Type System Foundation.
 */

#ifndef SUHC_TYPECHECK_H
#define SUHC_TYPECHECK_H

#include "ast.h"
#include "diagnostic.h"
#include "type_registry.h"

typedef struct {
    int  arity_errors;
    int  type_errors;
    int  unknown_type_errors;
    int  typed_params;
    int  total_params;
} TypecheckReport;

/* Run the type checker. Requires a built TypeRegistry. */
TypecheckReport typecheck(Program *prog, TypeRegistry *reg, DiagList *diags);

#endif /* SUHC_TYPECHECK_H */
