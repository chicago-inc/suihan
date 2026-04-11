/*
 * suhc — resolve.c
 * Import resolver — locates, parses, and caches imported .szh modules.
 *
 * Search order:
 *   1. Same directory as the importing file
 *   2. Additional search paths (ordbok/, etc.)
 *
 * Caching: each module is parsed once and shared across all importers
 * (diamond dependency safe). Circular imports are detected via a
 * resolution stack.
 */

#include "resolve.h"
#include "lexer.h"
#include "parser.h"
#include "kindcheck.h"
#include "perpcheck.h"
#include "bloatlint.h"
#include "decidability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ------------------------------------------------------------ */
/* File utilities                                                */
/* ------------------------------------------------------------ */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Extract the directory portion of a path.
 * "/foo/bar/baz.szh" → "/foo/bar"
 * "baz.szh" → "." */
static char *dir_of(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) return strdup(".");
    size_t len = (size_t)(slash - path);
    char *dir = malloc(len + 1);
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

/* ------------------------------------------------------------ */
/* Resolver lifecycle                                            */
/* ------------------------------------------------------------ */

void resolver_init(Resolver *r, const char *base_dir) {
    memset(r, 0, sizeof(Resolver));
    if (base_dir) {
        r->search_paths[0] = strdup(base_dir);
        r->n_paths = 1;
    }
}

void resolver_free(Resolver *r) {
    for (int i = 0; i < r->n_paths; i++)
        free(r->search_paths[i]);
    for (int i = 0; i < r->n_loaded; i++) {
        free(r->loaded_names[i]);
        /* Note: Programs are owned by their importing Decl nodes
         * or freed by program_free — we don't double-free here.
         * In Phase 1A the resolver owns the cache and the
         * import_decl.resolved pointers alias into it. */
    }
}

void resolver_add_path(Resolver *r, const char *path) {
    if (r->n_paths >= RESOLVE_MAX_PATHS) return;
    r->search_paths[r->n_paths++] = strdup(path);
}

/* ------------------------------------------------------------ */
/* Cache                                                         */
/* ------------------------------------------------------------ */

Program *resolver_get_cached(Resolver *r, const char *module_name) {
    for (int i = 0; i < r->n_loaded; i++) {
        if (strcmp(r->loaded_names[i], module_name) == 0)
            return r->loaded[i];
    }
    return NULL;
}

static void resolver_cache(Resolver *r, const char *module_name, Program *prog) {
    if (r->n_loaded >= RESOLVE_MAX_LOADED) return;
    r->loaded_names[r->n_loaded] = strdup(module_name);
    r->loaded[r->n_loaded] = prog;
    r->n_loaded++;
}

/* ------------------------------------------------------------ */
/* Cycle detection                                               */
/* ------------------------------------------------------------ */

static int stack_contains(Resolver *r, const char *name) {
    for (int i = 0; i < r->stack_depth; i++) {
        if (strcmp(r->resolve_stack[i], name) == 0)
            return 1;
    }
    return 0;
}

static void stack_push(Resolver *r, const char *name) {
    if (r->stack_depth < RESOLVE_MAX_DEPTH)
        r->resolve_stack[r->stack_depth++] = strdup(name);
}

static void stack_pop(Resolver *r) {
    if (r->stack_depth > 0) {
        r->stack_depth--;
        free(r->resolve_stack[r->stack_depth]);
        r->resolve_stack[r->stack_depth] = NULL;
    }
}

/* Build a string showing the cycle path for diagnostics */
static char *cycle_path(Resolver *r, const char *name) {
    /* e.g., "a -> b -> c -> a" */
    size_t total = 0;
    for (int i = 0; i < r->stack_depth; i++)
        total += strlen(r->resolve_stack[i]) + 4; /* " -> " */
    total += strlen(name) + 1;

    char *buf = malloc(total);
    buf[0] = '\0';

    int started = 0;
    for (int i = 0; i < r->stack_depth; i++) {
        if (!started && strcmp(r->resolve_stack[i], name) == 0)
            started = 1;
        if (started) {
            if (buf[0] != '\0') strcat(buf, " -> ");
            strcat(buf, r->resolve_stack[i]);
        }
    }
    if (buf[0] != '\0') strcat(buf, " -> ");
    strcat(buf, name);
    return buf;
}

/* ------------------------------------------------------------ */
/* Locate a module file                                          */
/* ------------------------------------------------------------ */

/* Try to find `module_name.szh` in each search path.
 * Returns allocated full path on success, NULL on failure. */
static char *find_module(Resolver *r, const char *module_name) {
    char path[512];
    for (int i = 0; i < r->n_paths; i++) {
        snprintf(path, sizeof(path), "%s/%s.szh",
                 r->search_paths[i], module_name);
        if (file_exists(path))
            return strdup(path);
    }
    return NULL;
}

/* ------------------------------------------------------------ */
/* Resolve a single module (recursive for transitive imports)    */
/* ------------------------------------------------------------ */

static Program *resolve_one(Resolver *r, const char *module_name,
                            const char *importing_file, int line,
                            DiagList *diags) {
    /* Check cache first */
    Program *cached = resolver_get_cached(r, module_name);
    if (cached) return cached;

    /* Cycle detection */
    if (stack_contains(r, module_name)) {
        char *path = cycle_path(r, module_name);
        diag_error(diags, DIAG_CIRCULAR_IMPORT,
                   importing_file, line, 0,
                   "circular import: %s", path);
        free(path);
        return NULL;
    }

    /* Locate the file */
    char *file_path = find_module(r, module_name);
    if (!file_path) {
        diag_error(diags, DIAG_IMPORT_ERROR,
                   importing_file, line, 0,
                   "cannot find module '%s' (searched %d path%s)",
                   module_name, r->n_paths,
                   r->n_paths == 1 ? "" : "s");
        return NULL;
    }

    /* Read and parse */
    char *source = read_file(file_path);
    if (!source) {
        diag_error(diags, DIAG_IMPORT_ERROR,
                   importing_file, line, 0,
                   "cannot read '%s'", file_path);
        free(file_path);
        return NULL;
    }

    /* Push onto resolution stack before parsing (the parsed file
     * may contain its own imports) */
    stack_push(r, module_name);

    DiagList *import_diags = diag_list_new();
    Lexer lex;
    lexer_init(&lex, source);
    Parser parser;
    parser_init(&parser, &lex, import_diags);
    Program *prog = parser_parse(&parser, file_path);

    /* Recursively resolve imports in the imported file */
    resolve_imports(prog, r, import_diags);

    /* Run checker passes on the imported program */
    kindcheck(prog, import_diags);
    perpcheck(prog, import_diags);
    bloatlint(prog, import_diags);
    decidability_check(prog, import_diags);

    /* Merge diagnostics into parent */
    for (size_t i = 0; i < import_diags->count; i++) {
        Diagnostic *d = &import_diags->items[i];
        diag_emit(diags, d->severity, d->category,
                  d->filename, d->line, d->col,
                  "%s", d->message);
    }

    stack_pop(r);

    /* Cache the result */
    resolver_cache(r, module_name, prog);

    /* Cleanup parser (but not prog — it's cached) */
    parser_free(&parser);
    diag_list_free(import_diags);
    free(source);
    free(file_path);

    return prog;
}

/* ------------------------------------------------------------ */
/* Public API: resolve all imports in a program                  */
/* ------------------------------------------------------------ */

int resolve_imports(Program *prog, Resolver *r, DiagList *diags) {
    int unresolved = 0;

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_IMPORT) continue;

        const char *module_name = d->as.import_decl.module_name;
        if (!module_name) {
            unresolved++;
            continue;
        }

        /* Already resolved (e.g., from a previous pass) */
        if (d->as.import_decl.resolved) continue;

        /* Derive importing file's directory as first search path
         * if not already present */
        if (prog->filename && r->n_paths == 0) {
            char *base = dir_of(prog->filename);
            resolver_add_path(r, base);
            free(base);
        }

        Program *resolved = resolve_one(r, module_name,
                                        prog->filename, d->line, diags);
        if (resolved) {
            d->as.import_decl.resolved = resolved;
        } else {
            unresolved++;
        }
    }

    return unresolved;
}
