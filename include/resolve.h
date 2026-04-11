/*
 * suhc — the suihan compiler
 * resolve.h — import resolver
 *
 * Resolves `import name` declarations by locating the .szh file,
 * parsing it, and linking the result into the AST. Handles:
 *   - Search paths (importing file's directory first)
 *   - Parse cache (diamond dependencies)
 *   - Cycle detection (circular imports)
 */

#ifndef SUHC_RESOLVE_H
#define SUHC_RESOLVE_H

#include "ast.h"
#include "diagnostic.h"

#define RESOLVE_MAX_PATHS  8
#define RESOLVE_MAX_LOADED 64
#define RESOLVE_MAX_DEPTH  16

typedef struct {
    char       *search_paths[RESOLVE_MAX_PATHS];
    int         n_paths;

    /* Cache: already-parsed programs keyed by canonical name */
    Program    *loaded[RESOLVE_MAX_LOADED];
    char       *loaded_names[RESOLVE_MAX_LOADED];
    int         n_loaded;

    /* Cycle detection: stack of modules currently being resolved */
    char       *resolve_stack[RESOLVE_MAX_DEPTH];
    int         stack_depth;
} Resolver;

/* Initialize a resolver. base_dir is the directory of the root file
 * (searched first for imports). Can be NULL. */
void resolver_init(Resolver *r, const char *base_dir);
void resolver_free(Resolver *r);

/* Add an additional search path (e.g., ordbok directory). */
void resolver_add_path(Resolver *r, const char *path);

/* Resolve all DECL_IMPORT nodes in prog. Returns 0 on success,
 * number of unresolved imports on failure. Diagnostics are emitted
 * to diags. Each resolved import has its import_decl.resolved set. */
int resolve_imports(Program *prog, Resolver *r, DiagList *diags);

/* Retrieve a cached program by module name, or NULL. */
Program *resolver_get_cached(Resolver *r, const char *module_name);

#endif /* SUHC_RESOLVE_H */
