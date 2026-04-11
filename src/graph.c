/*
 * suhc — graph.c
 * Dependency graph emission for ordbok files.
 *
 * Sprint 4B: Traverses all .szh files in a directory, resolves
 * imports, and emits a Mermaid diagram showing:
 *   1. File-level import edges (A --> B)
 *   2. Declaration-level cross-references (which projections
 *      reference which dimensions across files)
 *
 * Constitutional grounding:
 *   Yoneda visualization: the graph makes the ordbok's
 *   morphism network observable. An isolated node is an
 *   unobserved morphism — a Yoneda gap in the ordbok.
 */

#include "graph.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "resolve.h"
#include "diagnostic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

/* ------------------------------------------------------------ */
/* Internal types                                                */
/* ------------------------------------------------------------ */

typedef struct {
    char filename[128];     /* e.g., "foundational" (no .szh) */
    Program *prog;
    char **imports;         /* imported module names */
    size_t import_count;
} GraphFile;

/* ------------------------------------------------------------ */
/* Helpers                                                       */
/* ------------------------------------------------------------ */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    size_t nread = fread(buf, 1, sz, f);
    (void)nread;
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

/* Strip .szh extension from filename */
static void strip_ext(const char *filename, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", filename);
    size_t len = strlen(out);
    if (len > 4 && strcmp(out + len - 4, ".szh") == 0) {
        out[len - 4] = '\0';
    }
}

/* ------------------------------------------------------------ */
/* Graph emission                                                */
/* ------------------------------------------------------------ */

int graph_emit(const char *ordbok_dir, const char *search_dir, FILE *out) {
    DIR *dir = opendir(ordbok_dir);
    if (!dir) {
        fprintf(stderr, "suhc: cannot open directory '%s'\n", ordbok_dir);
        return 1;
    }

    GraphFile files[64];
    int n_files = 0;

    /* Phase 1: Parse all .szh files */
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && n_files < 64) {
        size_t nlen = strlen(ent->d_name);
        if (nlen < 5 || strcmp(ent->d_name + nlen - 4, ".szh") != 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", ordbok_dir, ent->d_name);

        char *src = read_file(path);
        if (!src) continue;

        DiagList *diags = diag_list_new();
        Lexer lex;
        lexer_init(&lex, src);
        Parser parser;
        parser_init(&parser, &lex, diags);
        Program *prog = parser_parse(&parser, path);

        /* Resolve imports */
        Resolver resolver;
        resolver_init(&resolver, search_dir ? search_dir : ordbok_dir);
        resolve_imports(prog, &resolver, diags);

        /* Extract file info */
        GraphFile *gf = &files[n_files];
        strip_ext(ent->d_name, gf->filename, sizeof(gf->filename));
        gf->prog = prog;
        gf->imports = NULL;
        gf->import_count = 0;

        /* Collect import module names */
        for (size_t i = 0; i < prog->count; i++) {
            Decl *d = prog->decls[i];
            if (d->type == DECL_IMPORT && d->as.import_decl.module_name) {
                gf->imports = realloc(gf->imports,
                    (gf->import_count + 1) * sizeof(char *));
                gf->imports[gf->import_count] = strdup(d->as.import_decl.module_name);
                gf->import_count++;
            }
        }

        n_files++;

        resolver_free(&resolver);
        parser_free(&parser);
        diag_list_free(diags);
        free(src);
        /* prog is kept alive for declaration analysis */
    }
    closedir(dir);

    /* Phase 2: Emit Mermaid */
    fprintf(out, "```mermaid\n");
    fprintf(out, "graph LR\n");

    /* File-level import edges */
    for (int i = 0; i < n_files; i++) {
        for (size_t j = 0; j < files[i].import_count; j++) {
            fprintf(out, "  %s --> %s\n",
                    files[i].imports[j], files[i].filename);
        }
    }

    /* Declaration-level cross-references:
     * For each projection, check if its invariant/context fields
     * reference dimensions defined in other files */
    fprintf(out, "\n");
    fprintf(out, "  %%%% Declaration cross-references\n");

    for (int i = 0; i < n_files; i++) {
        Program *prog = files[i].prog;
        for (size_t d = 0; d < prog->count; d++) {
            Decl *decl = prog->decls[d];
            if (decl->type != DECL_PROJECTION) continue;

            /* Check each field's value for cross-file dimension references */
            for (size_t f = 0; f < decl->as.projection.field_count; f++) {
                Expr *fval = decl->as.projection.fields[f].value;
                const char *field_name = NULL;
                if (fval && fval->type == EXPR_IDENT && fval->as.ident.name)
                    field_name = fval->as.ident.name;
                if (!field_name) continue;

                /* Look for this name as a dimension in other files */
                for (int k = 0; k < n_files; k++) {
                    if (k == i) continue; /* skip self */
                    Program *other = files[k].prog;
                    for (size_t od = 0; od < other->count; od++) {
                        Decl *odecl = other->decls[od];
                        if (odecl->type == DECL_DIMENSION &&
                            odecl->name.text &&
                            strcmp(odecl->name.text, field_name) == 0) {
                            fprintf(out, "  %s -.->|%s| %s\n",
                                    files[i].filename,
                                    decl->name.text,
                                    files[k].filename);
                        }
                    }
                }
            }
        }
    }

    fprintf(out, "```\n");

    /* Emit summary */
    fprintf(out, "\n");
    fprintf(out, "<!-- Graph: %d files", n_files);
    int total_edges = 0;
    for (int i = 0; i < n_files; i++)
        total_edges += (int)files[i].import_count;
    fprintf(out, ", %d import edges -->\n", total_edges);

    /* Cleanup */
    for (int i = 0; i < n_files; i++) {
        for (size_t j = 0; j < files[i].import_count; j++)
            free(files[i].imports[j]);
        free(files[i].imports);
        program_free(files[i].prog);
    }

    return 0;
}
