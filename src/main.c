/*
 * suhc — main.c
 * The suihan compiler entry point.
 *
 * Usage:
 *   suhc <file.szh>                         Parse + check, report diagnostics
 *   suhc --check <file.szh>                 Same (for CI integration)
 *   suhc <file.szh> --target typescript      Emit TypeScript
 *   suhc <file.szh> --target sql             Emit SQL
 *   suhc <file.szh> --target typescript -o f Emit to specific file
 *   suhc --dump-tokens <file>                Dump lexer output
 *   suhc --dump-ast <file>                   Dump parsed AST
 */

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "kindcheck.h"
#include "perpcheck.h"
#include "bloatlint.h"
#include "decidability.h"
#include "resolve.h"
#include "emitter.h"
#include "convergence.h"
#include "exhaustcheck.h"
#include "diagnostic.h"
#include "ts_scanner.h"
#include "py_scanner.h"
#include "drift.h"
#include "watcher.h"
#include "sql_validate.h"
#include "graph.h"
#include "semcheck.h"
#include "type_registry.h"
#include "typecheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* ------------------------------------------------------------ */
/* File reading                                                  */
/* ------------------------------------------------------------ */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "suhc: cannot open '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "suhc: out of memory reading '%s'\n", path);
        return NULL;
    }
    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

/* ------------------------------------------------------------ */
/* Dump modes                                                    */
/* ------------------------------------------------------------ */

static void dump_tokens(const char *source) {
    Lexer lex;
    lexer_init(&lex, source);

    printf("%-6s %-20s %s\n", "LINE", "TYPE", "TEXT");
    printf("%-6s %-20s %s\n", "----", "----", "----");

    for (;;) {
        Token t = lexer_next(&lex);
        if (t.type == TOK_EOF) {
            printf("%-6d %-20s %s\n", t.line, "EOF", "");
            break;
        }
        if (t.type == TOK_NEWLINE) {
            printf("%-6d %-20s %s\n", t.line, "NEWLINE", "\\n");
        } else if (t.type == TOK_INDENT) {
            printf("%-6d %-20s %s\n", t.line, "INDENT", ">>");
        } else if (t.type == TOK_DEDENT) {
            printf("%-6d %-20s %s\n", t.line, "DEDENT", "<<");
        } else {
            char text[256];
            size_t len = t.length < 255 ? t.length : 255;
            memcpy(text, t.start, len);
            text[len] = '\0';
            printf("%-6d %-20s %s\n", t.line,
                   token_type_name(t.type), text);
        }
    }
}

static void dump_ast(Program *prog) {
    printf("Program: %s (%zu declarations)\n",
           prog->filename ? prog->filename : "<stdin>",
           prog->count);
    printf("=========================================\n");

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        printf("\n[%zu] %s", i + 1, decl_type_name(d->type));
        if (d->kind != KIND_NONE) {
            printf(" (%s)", kind_name(d->kind));
        }
        printf(": %s", d->name.text ? d->name.text : "<unnamed>");
        printf(" (line %d)\n", d->line);

        /* Print type-specific details */
        switch (d->type) {
        case DECL_KINDED_VALUE:
            if (d->as.kinded.value) {
                printf("    value type: %d\n", d->as.kinded.value->type);
            }
            break;
        case DECL_DIMENSION:
            if (d->as.dimension.members) {
                printf("    members: expression type %d\n",
                       d->as.dimension.members->type);
            }
            break;
        case DECL_DEPENDENCY:
            if (d->as.dependency.relation) {
                printf("    relation: expression type %d\n",
                       d->as.dependency.relation->type);
            }
            printf("    fields: %zu\n", d->as.dependency.field_count);
            break;
        case DECL_TRAVERSAL:
            printf("    sections: %zu\n", d->as.traversal.section_count);
            for (size_t s = 0; s < d->as.traversal.section_count; s++) {
                printf("      [%s] %s\n",
                       kind_name(d->as.traversal.sections[s].section_kind),
                       d->as.traversal.sections[s].label.text ?
                           d->as.traversal.sections[s].label.text : "?");
            }
            break;
        case DECL_INCOMMENSURABLE:
        case DECL_COMMENSURABLE:
        case DECL_PERPENDICULAR:
            printf("    names:");
            for (size_t n = 0; n < d->as.relation.count; n++) {
                printf(" %s", d->as.relation.names[n].text);
            }
            printf("\n");
            break;
        case DECL_MORPHISM:
            printf("    fields: %zu\n", d->as.morphism.field_count);
            break;
        case DECL_PROJECTION:
            printf("    fields: %zu\n", d->as.projection.field_count);
            printf("    arms: %zu\n", d->as.projection.arm_count);
            for (size_t a = 0; a < d->as.projection.arm_count; a++) {
                ProjectionArm *arm = &d->as.projection.arms[a];
                printf("      [%zu] pattern=%s body=%s\n", a,
                       arm->pattern ? (arm->pattern->type == EXPR_LIST ? "tuple" :
                           arm->pattern->type == EXPR_WILDCARD ? "_" : "other") : "null",
                       arm->body ? (arm->body->type == EXPR_STRING ? "string" :
                           arm->body->type == EXPR_CALL ? "call" :
                           arm->body->type == EXPR_IDENT ? "ident" : "other") : "null");
            }
            break;
        case DECL_MEIHUA:
        case DECL_ZHULIN:
        case DECL_SONGQIAO:
            printf("    params: %zu\n", d->as.exec_layer.param_count);
            for (size_t p = 0; p < d->as.exec_layer.param_count; p++) {
                const char *pname = d->as.exec_layer.params[p].text;
                const char *ptype = (d->as.exec_layer.param_types &&
                                     d->as.exec_layer.param_types[p].text)
                                    ? d->as.exec_layer.param_types[p].text
                                    : "<untyped>";
                printf("      [%zu] %s : %s\n", p,
                       pname ? pname : "?", ptype);
            }
            if (d->as.exec_layer.body) {
                printf("    body: expression type %d\n",
                       d->as.exec_layer.body->type);
            }
            break;
        case DECL_IMPORT:
            printf("    module: %s\n",
                   d->as.import_decl.module_name ?
                       d->as.import_decl.module_name : "<null>");
            printf("    resolved: %s\n",
                   d->as.import_decl.resolved ? "yes" : "no");
            if (d->as.import_decl.resolved) {
                printf("    imported decls: %zu\n",
                       d->as.import_decl.resolved->count);
            }
            break;
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------ */
/* Build report                                                  */
/* ------------------------------------------------------------ */

static void print_build_report(Program *prog, DiagList *diags,
                               ExhaustReport *exhaust,
                               TypecheckReport *typerpt) {
    /* Count declaration types */
    int traversals = 0, projections = 0, morphisms = 0;
    int meihua = 0, dependencies = 0, dimensions = 0;
    int xi_decls = 0, zeta_decls = 0, omega_decls = 0;

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type == DECL_TRAVERSAL) traversals++;
        if (d->type == DECL_PROJECTION) projections++;
        if (d->type == DECL_MORPHISM) morphisms++;
        if (d->type == DECL_MEIHUA) meihua++;
        if (d->type == DECL_DEPENDENCY) dependencies++;
        if (d->type == DECL_DIMENSION) dimensions++;
        if (d->kind == KIND_XI) xi_decls++;
        if (d->kind == KIND_ZETA) zeta_decls++;
        if (d->kind == KIND_OMEGA) omega_decls++;
    }

    /* Count imported declarations for ordbok coverage */
    size_t imp_count = 0;
    Decl **imp_decls = collect_imported_decls(prog, &imp_count);
    int imp_projections = 0, imp_meihua = 0, imp_traversals = 0;
    for (size_t i = 0; i < imp_count; i++) {
        if (imp_decls[i]->type == DECL_PROJECTION) imp_projections++;
        if (imp_decls[i]->type == DECL_MEIHUA) imp_meihua++;
        if (imp_decls[i]->type == DECL_TRAVERSAL) imp_traversals++;
    }
    free(imp_decls);

    printf("\n");
    printf("BUILD REPORT: %s\n", prog->filename ? prog->filename : "<stdin>");
    printf("==========================================\n");
    printf("  declarations:   %zu\n", prog->count);
    printf("  traversals:     %d\n", traversals);
    printf("  projections:    %d\n", projections);
    printf("  meihua:         %d\n", meihua);
    printf("  morphisms:      %d\n", morphisms);
    printf("  dependencies:   %d\n", dependencies);
    printf("  dimensions:     %d\n", dimensions);
    printf("  ξ (identity):   %d\n", xi_decls);
    printf("  ζ (shape):      %d\n", zeta_decls);
    printf("  ω (output):     %d\n", omega_decls);
    if (imp_count > 0) {
        printf("------------------------------------------\n");
        printf("  imported decls: %zu\n", imp_count);
        printf("  imp.projections:%d\n", imp_projections);
        printf("  imp.meihua:     %d\n", imp_meihua);
        printf("  imp.traversals: %d\n", imp_traversals);
    }
    if (exhaust && (exhaust->projections_checked > 0 || exhaust->meihua_checked > 0)) {
        printf("------------------------------------------\n");
        if (exhaust->projections_checked > 0) {
            printf("  exhaust checked:%d projection%s\n",
                   exhaust->projections_checked,
                   exhaust->projections_checked == 1 ? "" : "s");
            printf("  exhaustive:     %d / %d\n",
                   exhaust->projections_exhaustive,
                   exhaust->projections_checked);
        }
        if (exhaust->cross_product_checked > 0) {
            printf("  cross-product:  %d / %d complete\n",
                   exhaust->cross_product_complete,
                   exhaust->cross_product_checked);
        }
        if (exhaust->meihua_checked > 0) {
            printf("  meihua valid:   %d / %d\n",
                   exhaust->meihua_clean, exhaust->meihua_checked);
            if (exhaust->meihua_arity_errors > 0) {
                printf("  arity errors:   %d\n",
                       exhaust->meihua_arity_errors);
            }
        }

        /* Compute per-file S measurement */
        int total_checks = exhaust->projections_checked + exhaust->meihua_checked;
        int total_ok = exhaust->projections_exhaustive + exhaust->meihua_clean;
        if (total_checks > 0) {
            double s = 1.0 - (double)total_ok / total_checks;
            printf("  S (solipsism):  %.2f", s);
            if (s < 0.01) printf(" (fully situated)");
            else if (s < 0.2) printf(" (low)");
            else if (s < 0.5) printf(" (moderate)");
            else printf(" (high — reduce before trusting output)");
            printf("\n");
        }
    }
    if (typerpt && typerpt->total_params > 0) {
        printf("------------------------------------------\n");
        printf("  type coverage:  %d / %d params typed (%.0f%%)\n",
               typerpt->typed_params, typerpt->total_params,
               100.0 * typerpt->typed_params / typerpt->total_params);
        if (typerpt->arity_errors > 0)
            printf("  arity errors:   %d\n", typerpt->arity_errors);
        if (typerpt->type_errors > 0)
            printf("  type errors:    %d\n", typerpt->type_errors);
        if (typerpt->unknown_type_errors > 0)
            printf("  unknown types:  %d\n", typerpt->unknown_type_errors);
    }
    printf("------------------------------------------\n");
    printf("  errors:         %d\n", diags->error_count);
    printf("  warnings:       %d\n", diags->warning_count);
    printf("==========================================\n");
}

/* ------------------------------------------------------------ */
/* Main                                                          */
/* ------------------------------------------------------------ */

static void usage(void) {
    fprintf(stderr,
        "suhc — the suihan compiler v1.1.0 (P1 — Structural Lint)\n"
        "\n"
        "Usage:\n"
        "  suhc <file.szh>                          Parse and check\n"
        "  suhc <file.szh> --target typescript       Emit TypeScript\n"
        "  suhc <file.szh> --target sql              Emit SQL\n"
        "  suhc <file.szh> --target c                Emit C11 header\n"
        "  suhc <file.szh> --target typescript -o f  Emit to file\n"
        "  suhc <file.szh> --ordbok <dir>            Add ordbok search path\n"
        "  suhc --audit <dir>                        Audit all .szh in directory\n"
        "  suhc --convergence <dir>                   Convergence measurement (D14)\n"
        "  suhc --diff <ordbok.szh> <spoxis.ts>      Drift detection\n"
        "  suhc --watch <dir> [--target ts|sql]      Watch and recompile\n"
        "  suhc --map <ordbok_dir> --spoxis <src>    Generate mapping JSON\n"
        "  suhc --graph <ordbok_dir>                  Emit Mermaid dependency graph\n"
        "  suhc --validate-sql <file.sql>             Validate emitted SQL syntax\n"
        "  suhc --lint --ordbok <dir> --targets <dir>   Structural lint (P1)\n"
        "  suhc --lint --ordbok <dir> --targets <dir> --format json|sarif\n"
        "  suhc --lint --ordbok <dir> --targets <dir> --security  Security report\n"
        "  suhc --check <file.szh>                   CI mode\n"
        "  suhc --dump-tokens <file>                 Dump lexer output\n"
        "  suhc --dump-ast <file>                    Dump parsed AST\n"
        "  suhc --dump-types <file>                  Dump type registry\n"
        "  suhc --version                            Print version\n"
        "\n"
        "Exit codes:\n"
        "  0  No errors\n"
        "  1  Errors found\n"
        "  2  Usage error\n"
    );
}

/* Build default output filename from input: foo.szh → foo.ts / foo.sql / foo.h */
static char *default_output_name(const char *input, EmitTarget target) {
    const char *ext;
    switch (target) {
    case TARGET_TYPESCRIPT: ext = ".ts"; break;
    case TARGET_SQL:        ext = ".sql"; break;
    case TARGET_C:          ext = ".h"; break;
    case TARGET_ASM:        ext = ".asm"; break;
    default:                ext = ".out"; break;
    }
    const char *dot = strrchr(input, '.');
    size_t base_len = dot ? (size_t)(dot - input) : strlen(input);
    char *out = malloc(base_len + strlen(ext) + 1);
    memcpy(out, input, base_len);
    strcpy(out + base_len, ext);
    return out;
}

/* ------------------------------------------------------------ */
/* Recursive TS file walker for --lint mode                      */
/* ------------------------------------------------------------ */

static int walk_ts_files(const char *dir, TsScanResult *results, int *count, int max) {
    DIR *d = opendir(dir);
    if (!d) return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && *count < max) {
        if (ent->d_name[0] == '.') continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walk_ts_files(path, results, count, max);
            continue;
        }

        size_t nlen = strlen(ent->d_name);
        bool is_ts = (nlen > 3 && strcmp(ent->d_name + nlen - 3, ".ts") == 0);
        bool is_tsx = (nlen > 4 && strcmp(ent->d_name + nlen - 4, ".tsx") == 0);
        if (!is_ts && !is_tsx) continue;
        /* Skip .d.ts declaration files */
        if (nlen > 5 && strcmp(ent->d_name + nlen - 5, ".d.ts") == 0) continue;

        char *src = read_file(path);
        if (!src) continue;
        results[*count] = ts_scan(src, path);
        (*count)++;
        free(src);
    }
    closedir(d);
    return 0;
}

/* ------------------------------------------------------------ */
/* Recursive Python file walker for --lint mode                  */
/* ------------------------------------------------------------ */

static int walk_py_files(const char *dir, TsScanResult *results, int *count, int max) {
    DIR *d = opendir(dir);
    if (!d) return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && *count < max) {
        if (ent->d_name[0] == '.') continue;
        if (strcmp(ent->d_name, "__pycache__") == 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walk_py_files(path, results, count, max);
            continue;
        }

        size_t nlen = strlen(ent->d_name);
        bool is_py = (nlen > 3 && strcmp(ent->d_name + nlen - 3, ".py") == 0);
        if (!is_py) continue;

        char *src = read_file(path);
        if (!src) continue;
        results[*count] = py_scan(src, path);
        (*count)++;
        free(src);
    }
    closedir(d);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 2;
    }

    /* Parse arguments */
    const char *filename = NULL;
    const char *output_path = NULL;
    const char *audit_dir = NULL;
    const char *ordbok_dir = NULL;
    const char *diff_szh = NULL;
    const char *diff_ts = NULL;
    const char *watch_dir = NULL;
    const char *map_dir = NULL;
    const char *spoxis_dir = NULL;
    const char *graph_dir = NULL;
    const char *validate_sql_path = NULL;
    const char *convergence_dir = NULL;
    bool dump_tok = false;
    bool dump_tree = false;
    bool dump_types = false;
    bool json_output = false;
    bool lint_mode = false;
    const char *targets_dir = NULL;
    const char *format_str = NULL;
    bool security_report = false;
    EmitTarget target = TARGET_NONE;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("suhc 1.0.0 (M7 — full bootstrap, fixed point reached)\n");
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        }
        if (strcmp(argv[i], "--dump-tokens") == 0) {
            dump_tok = true;
            continue;
        }
        if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_tree = true;
            continue;
        }
        if (strcmp(argv[i], "--dump-types") == 0) {
            dump_types = true;
            continue;
        }
        if (strcmp(argv[i], "--check") == 0) {
            continue; /* default behavior */
        }
        if (strcmp(argv[i], "--audit") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --audit requires a directory\n");
                return 2;
            }
            audit_dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--convergence") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --convergence requires a directory\n");
                return 2;
            }
            convergence_dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--ordbok") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --ordbok requires a directory\n");
                return 2;
            }
            ordbok_dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--target") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --target requires an argument (typescript, sql)\n");
                return 2;
            }
            i++;
            if (strcmp(argv[i], "typescript") == 0 || strcmp(argv[i], "ts") == 0) {
                target = TARGET_TYPESCRIPT;
            } else if (strcmp(argv[i], "sql") == 0) {
                target = TARGET_SQL;
            } else if (strcmp(argv[i], "c") == 0 || strcmp(argv[i], "c11") == 0) {
                target = TARGET_C;
            } else if (strcmp(argv[i], "asm") == 0 || strcmp(argv[i], "x86_64") == 0) {
                target = TARGET_ASM;
            } else {
                fprintf(stderr, "suhc: unknown target '%s' (use: typescript, sql, c, asm)\n", argv[i]);
                return 2;
            }
            continue;
        }
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: -o requires a filename\n");
                return 2;
            }
            output_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--diff") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "suhc: --diff requires <ordbok.szh> <spoxis.ts>\n");
                return 2;
            }
            diff_szh = argv[++i];
            diff_ts = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--watch") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --watch requires a directory\n");
                return 2;
            }
            watch_dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--map") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --map requires an ordbok directory\n");
                return 2;
            }
            map_dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--spoxis") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --spoxis requires a source directory\n");
                return 2;
            }
            spoxis_dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--graph") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --graph requires an ordbok directory\n");
                return 2;
            }
            graph_dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--validate-sql") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --validate-sql requires a .sql file\n");
                return 2;
            }
            validate_sql_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--lint") == 0) {
            lint_mode = true;
            continue;
        }
        if (strcmp(argv[i], "--targets") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --targets requires a directory\n");
                return 2;
            }
            targets_dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--format") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "suhc: --format requires an argument (json, sarif)\n");
                return 2;
            }
            format_str = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--security") == 0) {
            security_report = true;
            continue;
        }
        if (strcmp(argv[i], "--json") == 0) {
            json_output = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "suhc: unknown option '%s'\n", argv[i]);
            return 2;
        }
        filename = argv[i];
    }

    /* --lint mode: combined audit + drift in a single pass */
    if (lint_mode) {
        if (!ordbok_dir) {
            fprintf(stderr, "suhc: --lint requires --ordbok <dir>\n");
            return 2;
        }
        if (!targets_dir) {
            fprintf(stderr, "suhc: --lint requires --targets <dir>\n");
            return 2;
        }

        OutputFormat output_fmt = OUTPUT_HUMAN;
        if (format_str) {
            if (strcmp(format_str, "json") == 0) output_fmt = OUTPUT_JSON;
            else if (strcmp(format_str, "sarif") == 0) output_fmt = OUTPUT_SARIF;
            else { fprintf(stderr, "suhc: unknown format '%s' (use: json, sarif)\n", format_str); return 2; }
        }

        /* Phase 1: Walk ordbok directory, parse+check every .szh file */
        DIR *odir = opendir(ordbok_dir);
        if (!odir) {
            fprintf(stderr, "suhc: cannot open ordbok directory '%s'\n", ordbok_dir);
            return 2;
        }

        DiagList *lint_diags = diag_list_new();
        Program *ordbok_progs[64];
        int n_progs = 0;
        int lint_total_decls = 0;

        struct dirent *oent;
        while ((oent = readdir(odir)) != NULL && n_progs < 64) {
            size_t nlen = strlen(oent->d_name);
            if (nlen < 5 || strcmp(oent->d_name + nlen - 4, ".szh") != 0) continue;

            char opath[512];
            snprintf(opath, sizeof(opath), "%s/%s", ordbok_dir, oent->d_name);

            char *osrc = read_file(opath);
            if (!osrc) continue;

            DiagList *fd = diag_list_new();
            Lexer olex;
            lexer_init(&olex, osrc);
            Parser oparser;
            parser_init(&oparser, &olex, fd);
            Program *oprog = parser_parse(&oparser, opath);

            Resolver oresolver;
            resolver_init(&oresolver, ordbok_dir);
            resolve_imports(oprog, &oresolver, fd);

            kindcheck(oprog, fd);
            perpcheck(oprog, fd);
            bloatlint(oprog, fd);
            decidability_check(oprog, fd);
            exhaustcheck(oprog, fd);
            semcheck(oprog, fd);

            /* Merge diagnostics into combined list */
            for (size_t di = 0; di < fd->count; di++) {
                Diagnostic *src_d = &fd->items[di];
                diag_emit(lint_diags, src_d->severity, src_d->category,
                          src_d->filename, src_d->line, src_d->col,
                          "%s", src_d->message);
            }

            lint_total_decls += (int)oprog->count;
            ordbok_progs[n_progs++] = oprog;

            resolver_free(&oresolver);
            parser_free(&oparser);
            diag_list_free(fd);
            free(osrc);
        }
        closedir(odir);

        /* Phase 2: Walk targets directory for .ts/.tsx and .py files */
        TsScanResult ts_results[256];
        int n_ts = 0;
        walk_ts_files(targets_dir, ts_results, &n_ts, 256);
        walk_py_files(targets_dir, ts_results, &n_ts, 256);

        /* Merge all scans into one composite result */
        TsScanResult merged;
        memset(&merged, 0, sizeof(merged));
        merged.filename = strdup(targets_dir);
        for (int t = 0; t < n_ts; t++) {
            TsScanResult *sr = &ts_results[t];
            for (size_t c = 0; c < sr->const_count; c++) {
                merged.consts = realloc(merged.consts,
                    (merged.const_count + 1) * sizeof(TsConst));
                merged.consts[merged.const_count].name = strdup(sr->consts[c].name);
                merged.consts[merged.const_count].value =
                    sr->consts[c].value ? strdup(sr->consts[c].value) : NULL;
                merged.const_count++;
            }
            for (size_t s = 0; s < sr->switch_count; s++) {
                merged.switches = realloc(merged.switches,
                    (merged.switch_count + 1) * sizeof(TsSwitchBlock));
                TsSwitchBlock *ob = &sr->switches[s];
                TsSwitchBlock *nb = &merged.switches[merged.switch_count];
                nb->switch_var = ob->switch_var ? strdup(ob->switch_var) : NULL;
                nb->branch_count = ob->branch_count;
                nb->branch_capacity = ob->branch_count;
                nb->branches = malloc(nb->branch_count * sizeof(TsCaseBranch));
                for (size_t b = 0; b < ob->branch_count; b++) {
                    nb->branches[b].case_value =
                        ob->branches[b].case_value ? strdup(ob->branches[b].case_value) : NULL;
                    nb->branches[b].result =
                        ob->branches[b].result ? strdup(ob->branches[b].result) : NULL;
                }
                merged.switch_count++;
            }
            for (size_t f = 0; f < sr->function_count; f++) {
                merged.functions = realloc(merged.functions,
                    (merged.function_count + 1) * sizeof(TsFunction));
                TsFunction *of = &sr->functions[f];
                TsFunction *nf = &merged.functions[merged.function_count];
                nf->name = strdup(of->name);
                nf->param_count = of->param_count;
                nf->params = malloc(of->param_count * sizeof(char *));
                for (size_t p = 0; p < of->param_count; p++)
                    nf->params[p] = strdup(of->params[p]);
                nf->is_exported = of->is_exported;
                merged.function_count++;
            }
        }

        /* Phase 3: Run drift_compare on each ordbok program */
        DriftReport total_drift;
        memset(&total_drift, 0, sizeof(total_drift));

        for (int pi = 0; pi < n_progs; pi++) {
            DriftReport dr = drift_compare(ordbok_progs[pi], &merged);
            for (size_t e = 0; e < dr.count; e++) {
                if (total_drift.count >= total_drift.capacity) {
                    total_drift.capacity = total_drift.capacity ? total_drift.capacity * 2 : 32;
                    total_drift.entries = realloc(total_drift.entries,
                        total_drift.capacity * sizeof(DriftEntry));
                }
                DriftEntry *se = &dr.entries[e];
                DriftEntry *de = &total_drift.entries[total_drift.count++];
                de->decl_name = strdup(se->decl_name);
                de->decl_type = strdup(se->decl_type);
                de->status = se->status;
                de->s_value = se->s_value;
                de->detail = se->detail ? strdup(se->detail) : NULL;

                switch (se->status) {
                case DRIFT_MATCHED:     total_drift.matched++; break;
                case DRIFT_ORDBOK_ONLY: total_drift.ordbok_only++; break;
                case DRIFT_SPOXIS_ONLY: total_drift.spoxis_only++; break;
                case DRIFT_DIVERGED:    total_drift.diverged++; break;
                }
            }
            drift_free(&dr);
        }

        /* Compute aggregate S */
        if (total_drift.count > 0) {
            double total_s = 0;
            for (size_t i = 0; i < total_drift.count; i++)
                total_s += total_drift.entries[i].s_value;
            total_drift.aggregate_s = total_s / total_drift.count;
        }

        /* Convergence analysis on ordbok programs */
        int agg_smegmacra = 0;
        for (int pi = 0; pi < n_progs; pi++) {
            ConvergenceReport cr = convergence_analyze(ordbok_progs[pi], lint_diags);
            agg_smegmacra += cr.smegmacra;
        }

        /* Phase 4: Output */
        if (output_fmt != OUTPUT_HUMAN) {
            diag_print_format(lint_diags, output_fmt, stdout);
        } else {
            int total_ts_decls = (int)(merged.const_count + merged.function_count + merged.switch_count);
            int governed = total_drift.matched + total_drift.diverged;
            int total_scope = governed + total_drift.ordbok_only + total_drift.spoxis_only;

            printf("\n");
            printf("LINT REPORT\n");
            printf("==========================================\n");

            /* Coverage */
            printf("\n  COVERAGE (S score)\n");
            printf("  ------------------------------------------\n");
            printf("  ordbok declarations: %d\n", lint_total_decls);
            printf("  target declarations: %d\n", total_ts_decls);
            if (total_scope > 0) {
                double s_coverage = 1.0 - (double)governed / total_scope;
                printf("  governed:            %d / %d\n", governed, total_scope);
                printf("  S (coverage):        %.2f", s_coverage);
                if (s_coverage < 0.01) printf(" (fully situated)");
                else if (s_coverage < 0.2) printf(" (low)");
                else if (s_coverage < 0.5) printf(" (moderate)");
                else printf(" (high — reduce before trusting output)");
                printf("\n");
            }

            /* Conformance */
            printf("\n  CONFORMANCE (bloat errors)\n");
            printf("  ------------------------------------------\n");
            printf("  errors:              %d\n", lint_diags->error_count);
            printf("  warnings:            %d\n", lint_diags->warning_count);

            /* Completeness */
            printf("\n  COMPLETENESS (drift)\n");
            printf("  ------------------------------------------\n");
            printf("  matched:             %d\n", total_drift.matched);
            printf("  ordbok-only:         %d (missing implementation)\n", total_drift.ordbok_only);
            printf("  spoxis-only:         %d (ungoverned code)\n", total_drift.spoxis_only);
            printf("  diverged:            %d\n", total_drift.diverged);
            printf("  aggregate S:         %.2f\n", total_drift.aggregate_s);

            /* Per-entry drift detail */
            if (total_drift.count > 0) {
                printf("\n  DRIFT DETAIL\n");
                printf("  ------------------------------------------\n");
                for (size_t i = 0; i < total_drift.count; i++) {
                    const DriftEntry *e = &total_drift.entries[i];
                    const char *st = "???";
                    switch (e->status) {
                    case DRIFT_MATCHED:     st = "MATCH "; break;
                    case DRIFT_ORDBOK_ONLY: st = "ORDBOK"; break;
                    case DRIFT_SPOXIS_ONLY: st = "TARGET"; break;
                    case DRIFT_DIVERGED:    st = "DRIFT "; break;
                    }
                    printf("  %s S=%.2f %-12s %s", st, e->s_value, e->decl_type, e->decl_name);
                    if (e->detail) printf(" — %s", e->detail);
                    printf("\n");
                }
            }

            /* Security report (optional) */
            if (security_report) {
                printf("\n  SECURITY AUDIT\n");
                printf("  ------------------------------------------\n");

                /* Ungoverned data flows (DRIFT_SPOXIS_ONLY) */
                int ungoverned_flows = 0;
                for (size_t i = 0; i < total_drift.count; i++) {
                    if (total_drift.entries[i].status == DRIFT_SPOXIS_ONLY)
                        ungoverned_flows++;
                }
                printf("  ungoverned data flows: %d\n", ungoverned_flows);
                for (size_t i = 0; i < total_drift.count; i++) {
                    if (total_drift.entries[i].status == DRIFT_SPOXIS_ONLY) {
                        printf("    - %s (%s)\n",
                               total_drift.entries[i].decl_name,
                               total_drift.entries[i].decl_type);
                    }
                }

                /* Oracle ceiling violations */
                int oracle_violations = 0;
                for (size_t i = 0; i < lint_diags->count; i++) {
                    if (lint_diags->items[i].category == DIAG_UNDECIDABLE_ACTION)
                        oracle_violations++;
                }
                printf("  oracle ceiling violations: %d\n", oracle_violations);
                for (size_t i = 0; i < lint_diags->count; i++) {
                    if (lint_diags->items[i].category == DIAG_UNDECIDABLE_ACTION) {
                        printf("    - %s:%d: %s\n",
                               lint_diags->items[i].filename ? lint_diags->items[i].filename : "?",
                               lint_diags->items[i].line,
                               lint_diags->items[i].message);
                    }
                }

                /* Smegmacra count */
                printf("  smegmacra:             %d\n", agg_smegmacra);
            }

            printf("==========================================\n");
        }

        /* Determine exit code: 0 = clean, 1 = warnings only, 2 = errors */
        int lint_exit = 0;
        if (lint_diags->error_count > 0 || total_drift.diverged > 0)
            lint_exit = 2;
        else if (lint_diags->warning_count > 0 || total_drift.ordbok_only > 0 || total_drift.spoxis_only > 0)
            lint_exit = 1;

        /* Cleanup */
        drift_free(&total_drift);
        ts_scan_free(&merged);
        for (int t = 0; t < n_ts; t++) ts_scan_free(&ts_results[t]);
        for (int pi = 0; pi < n_progs; pi++) program_free(ordbok_progs[pi]);
        diag_list_free(lint_diags);

        return lint_exit;
    }

    /* --diff mode: ordbok ↔ Spoxis drift detection */
    if (diff_szh && diff_ts) {
        /* Parse the ordbok file */
        char *szh_source = read_file(diff_szh);
        if (!szh_source) {
            fprintf(stderr, "suhc: cannot read '%s'\n", diff_szh);
            return 2;
        }

        DiagList *ddiags = diag_list_new();
        Lexer dlex;
        lexer_init(&dlex, szh_source);
        Parser dparser;
        parser_init(&dparser, &dlex, ddiags);
        Program *dprog = parser_parse(&dparser, diff_szh);

        /* Resolve imports from ordbok dir if provided */
        char diff_base[512];
        snprintf(diff_base, sizeof(diff_base), "%s", diff_szh);
        char *ds = strrchr(diff_base, '/');
        if (ds) *ds = '\0'; else strcpy(diff_base, ".");

        Resolver dresolver;
        resolver_init(&dresolver, ordbok_dir ? ordbok_dir : diff_base);
        resolve_imports(dprog, &dresolver, ddiags);
        kindcheck(dprog, ddiags);

        /* Scan the TS file */
        char *ts_source = read_file(diff_ts);
        if (!ts_source) {
            fprintf(stderr, "suhc: cannot read '%s'\n", diff_ts);
            parser_free(&dparser);
            program_free(dprog);
            diag_list_free(ddiags);
            free(szh_source);
            return 2;
        }

        TsScanResult ts_scan_result = ts_scan(ts_source, diff_ts);

        /* Compare */
        DriftReport dreport = drift_compare(dprog, &ts_scan_result);
        if (json_output) {
            drift_print_json(&dreport, stdout);
        } else {
            drift_print(&dreport, stdout);
        }

        int ret = dreport.diverged > 0 ? 1 : 0;

        drift_free(&dreport);
        ts_scan_free(&ts_scan_result);
        resolver_free(&dresolver);
        parser_free(&dparser);
        program_free(dprog);
        diag_list_free(ddiags);
        free(szh_source);
        free(ts_source);
        return ret;
    }

    /* --graph mode: emit Mermaid dependency graph */
    if (graph_dir) {
        return graph_emit(graph_dir, ordbok_dir, stdout);
    }

    /* --validate-sql mode: validate emitted SQL */
    if (validate_sql_path) {
        SqlValidateResult svr = sql_validate_file(validate_sql_path);
        sql_validate_print(&svr, validate_sql_path);
        return svr.valid ? 0 : 1;
    }

    /* --watch mode: file watcher */
    if (watch_dir) {
        return watcher_run(watch_dir, ordbok_dir, target);
    }

    /* --map mode: generate ordbok ↔ Spoxis mapping JSON */
    if (map_dir) {
        if (!spoxis_dir) {
            fprintf(stderr, "suhc: --map requires --spoxis <dir>\n");
            return 2;
        }

        /* Scan all .szh files in ordbok dir */
        DIR *mdir = opendir(map_dir);
        if (!mdir) {
            fprintf(stderr, "suhc: cannot open '%s'\n", map_dir);
            return 2;
        }

        /* Scan all .ts files in Spoxis dir */
        DIR *sdir = opendir(spoxis_dir);
        if (!sdir) {
            fprintf(stderr, "suhc: cannot open '%s'\n", spoxis_dir);
            closedir(mdir);
            return 2;
        }

        /* Collect all TS scan results */
        TsScanResult ts_results[64];
        int n_ts = 0;
        struct dirent *sent;
        while ((sent = readdir(sdir)) != NULL && n_ts < 64) {
            size_t nlen = strlen(sent->d_name);
            if (nlen < 4 || strcmp(sent->d_name + nlen - 3, ".ts") != 0) continue;
            if (nlen > 5 && strcmp(sent->d_name + nlen - 5, ".d.ts") == 0) continue;

            char tpath[512];
            snprintf(tpath, sizeof(tpath), "%s/%s", spoxis_dir, sent->d_name);
            char *tsrc = read_file(tpath);
            if (!tsrc) continue;
            ts_results[n_ts++] = ts_scan(tsrc, tpath);
            free(tsrc);
        }
        closedir(sdir);

        /* Merge all TS scans into one composite result */
        TsScanResult merged;
        memset(&merged, 0, sizeof(merged));
        merged.filename = strdup(spoxis_dir);
        for (int t = 0; t < n_ts; t++) {
            TsScanResult *sr = &ts_results[t];
            for (size_t c = 0; c < sr->const_count; c++) {
                merged.consts = realloc(merged.consts,
                    (merged.const_count + 1) * sizeof(TsConst));
                merged.consts[merged.const_count].name = strdup(sr->consts[c].name);
                merged.consts[merged.const_count].value =
                    sr->consts[c].value ? strdup(sr->consts[c].value) : NULL;
                merged.const_count++;
            }
            for (size_t s = 0; s < sr->switch_count; s++) {
                merged.switches = realloc(merged.switches,
                    (merged.switch_count + 1) * sizeof(TsSwitchBlock));
                TsSwitchBlock *ob = &sr->switches[s];
                TsSwitchBlock *nb = &merged.switches[merged.switch_count];
                nb->switch_var = ob->switch_var ? strdup(ob->switch_var) : NULL;
                nb->branch_count = ob->branch_count;
                nb->branch_capacity = ob->branch_count;
                nb->branches = malloc(nb->branch_count * sizeof(TsCaseBranch));
                for (size_t b = 0; b < ob->branch_count; b++) {
                    nb->branches[b].case_value =
                        ob->branches[b].case_value ? strdup(ob->branches[b].case_value) : NULL;
                    nb->branches[b].result =
                        ob->branches[b].result ? strdup(ob->branches[b].result) : NULL;
                }
                merged.switch_count++;
            }
            for (size_t f = 0; f < sr->function_count; f++) {
                merged.functions = realloc(merged.functions,
                    (merged.function_count + 1) * sizeof(TsFunction));
                TsFunction *of = &sr->functions[f];
                TsFunction *nf = &merged.functions[merged.function_count];
                nf->name = strdup(of->name);
                nf->param_count = of->param_count;
                nf->params = malloc(of->param_count * sizeof(char *));
                for (size_t p = 0; p < of->param_count; p++)
                    nf->params[p] = strdup(of->params[p]);
                nf->is_exported = of->is_exported;
                merged.function_count++;
            }
        }

        /* Process each ordbok file against merged TS */
        DriftReport total_report;
        memset(&total_report, 0, sizeof(total_report));

        struct dirent *ment;
        while ((ment = readdir(mdir)) != NULL) {
            size_t nlen = strlen(ment->d_name);
            if (nlen < 5 || strcmp(ment->d_name + nlen - 4, ".szh") != 0) continue;

            char opath[512];
            snprintf(opath, sizeof(opath), "%s/%s", map_dir, ment->d_name);

            char *osrc = read_file(opath);
            if (!osrc) continue;

            DiagList *md = diag_list_new();
            Lexer mlex;
            lexer_init(&mlex, osrc);
            Parser mparser;
            parser_init(&mparser, &mlex, md);
            Program *mprog = parser_parse(&mparser, opath);

            Resolver mresolver;
            resolver_init(&mresolver, map_dir);
            resolve_imports(mprog, &mresolver, md);

            DriftReport dr = drift_compare(mprog, &merged);

            /* Accumulate into total */
            for (size_t e = 0; e < dr.count; e++) {
                if (total_report.count >= total_report.capacity) {
                    total_report.capacity = total_report.capacity ? total_report.capacity * 2 : 32;
                    total_report.entries = realloc(total_report.entries,
                        total_report.capacity * sizeof(DriftEntry));
                }
                DriftEntry *src_e = &dr.entries[e];
                DriftEntry *dst_e = &total_report.entries[total_report.count++];
                dst_e->decl_name = strdup(src_e->decl_name);
                dst_e->decl_type = strdup(src_e->decl_type);
                dst_e->status = src_e->status;
                dst_e->s_value = src_e->s_value;
                dst_e->detail = src_e->detail ? strdup(src_e->detail) : NULL;

                switch (src_e->status) {
                case DRIFT_MATCHED:     total_report.matched++; break;
                case DRIFT_ORDBOK_ONLY: total_report.ordbok_only++; break;
                case DRIFT_SPOXIS_ONLY: total_report.spoxis_only++; break;
                case DRIFT_DIVERGED:    total_report.diverged++; break;
                }
            }

            drift_free(&dr);
            resolver_free(&mresolver);
            parser_free(&mparser);
            program_free(mprog);
            diag_list_free(md);
            free(osrc);
        }
        closedir(mdir);

        /* Compute aggregate S */
        if (total_report.count > 0) {
            double total_s = 0;
            for (size_t i = 0; i < total_report.count; i++)
                total_s += total_report.entries[i].s_value;
            total_report.aggregate_s = total_s / total_report.count;
        }

        /* Output */
        if (json_output) {
            drift_print_json(&total_report, stdout);
        } else {
            drift_print(&total_report, stdout);
        }

        drift_free(&total_report);
        ts_scan_free(&merged);
        for (int t = 0; t < n_ts; t++) ts_scan_free(&ts_results[t]);

        return total_report.diverged > 0 ? 1 : 0;
    }

    /* Convergence mode: measure ΔR.k across all files in a directory (D14) */
    if (convergence_dir) {
        return convergence_scan_dir(convergence_dir, stdout);
    }

    /* Audit mode: scan all .szh in a directory */
    if (audit_dir) {
        DIR *dir = opendir(audit_dir);
        if (!dir) {
            fprintf(stderr, "suhc: cannot open directory '%s'\n", audit_dir);
            return 2;
        }

        printf("AUDIT REPORT: %s\n", audit_dir);
        printf("==========================================\n");

        int total_files = 0, total_decls = 0;
        int total_errors = 0, total_warnings = 0;
        int total_traversals = 0, total_projections = 0, total_morphisms = 0;
        int total_meihua = 0;
        int agg_cast = 0, agg_governed = 0, agg_ungoverned = 0;
        int agg_yoneda = 0;
        int agg_exhaust_checked = 0, agg_exhaust_ok = 0;
        int agg_meihua_checked = 0, agg_meihua_clean = 0;

        /* Per-file storage */
        typedef struct { char name[256]; int decls; int errors; int warnings; double s_value; } FileResult;
        FileResult results[64];
        int n_results = 0;

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            size_t nlen = strlen(ent->d_name);
            if (nlen < 5 || strcmp(ent->d_name + nlen - 4, ".szh") != 0) continue;

            char path[512];
            snprintf(path, sizeof(path), "%s/%s", audit_dir, ent->d_name);

            char *src = read_file(path);
            if (!src) continue;

            DiagList *ad = diag_list_new();
            Lexer alex;
            lexer_init(&alex, src);
            Parser aparser;
            parser_init(&aparser, &alex, ad);
            Program *aprog = parser_parse(&aparser, path);

            /* Resolve imports for audit files */
            Resolver aresolver;
            resolver_init(&aresolver, audit_dir);
            resolve_imports(aprog, &aresolver, ad);

            kindcheck(aprog, ad);
            perpcheck(aprog, ad);
            bloatlint(aprog, ad);
            decidability_check(aprog, ad);
            ExhaustReport ae = exhaustcheck(aprog, ad);
            semcheck(aprog, ad);

            ConvergenceReport ac = convergence_analyze(aprog, ad);

            /* Accumulate */
            total_files++;
            total_decls += (int)aprog->count;
            total_errors += ad->error_count;
            total_warnings += ad->warning_count;

            for (size_t ci = 0; ci < aprog->count; ci++) {
                if (aprog->decls[ci]->type == DECL_TRAVERSAL) total_traversals++;
                if (aprog->decls[ci]->type == DECL_PROJECTION) total_projections++;
                if (aprog->decls[ci]->type == DECL_MORPHISM) total_morphisms++;
                if (aprog->decls[ci]->type == DECL_MEIHUA) total_meihua++;
            }

            agg_cast += ac.total_cast;
            agg_governed += ac.governed;
            agg_ungoverned += ac.ungoverned;
            agg_yoneda += ac.yoneda_gaps;
            agg_exhaust_checked += ae.projections_checked;
            agg_exhaust_ok += ae.projections_exhaustive;
            agg_meihua_checked += ae.meihua_checked;
            agg_meihua_clean += ae.meihua_clean;

            if (n_results < 64) {
                snprintf(results[n_results].name, 256, "%s", ent->d_name);
                results[n_results].decls = (int)aprog->count;
                results[n_results].errors = ad->error_count;
                results[n_results].warnings = ad->warning_count;
                /* Compute per-file S */
                int file_checks = ae.projections_checked + ae.meihua_checked;
                int file_ok = ae.projections_exhaustive + ae.meihua_clean;
                if (file_checks > 0) {
                    results[n_results].s_value = 1.0 - (double)file_ok / file_checks;
                } else {
                    results[n_results].s_value = 0.0;
                }
                n_results++;
            }

            resolver_free(&aresolver);
            parser_free(&aparser);
            program_free(aprog);
            diag_list_free(ad);
            free(src);
        }
        closedir(dir);

        printf("  files:          %d\n", total_files);
        printf("  declarations:   %d\n", total_decls);
        printf("  traversals:     %d\n", total_traversals);
        printf("  projections:    %d\n", total_projections);
        printf("  meihua:         %d\n", total_meihua);
        printf("  morphisms:      %d\n", total_morphisms);
        printf("  total cast:     %d possibility space%s\n", agg_cast, agg_cast == 1 ? "" : "s");
        printf("  governed:       %d\n", agg_governed);
        printf("  ungoverned:     %d\n", agg_ungoverned);
        if (agg_cast > 0) {
            double r = agg_ungoverned > 0 ? (double)agg_ungoverned / (double)agg_cast : 0.0;
            printf("  convergence:    r = %.2f (first audit — no trend yet)\n", r);
        }
        if (agg_yoneda > 0) {
            printf("  yoneda gaps:    %d\n", agg_yoneda);
        }
        if (agg_exhaust_checked > 0) {
            printf("  exhaust proj:   %d / %d (%d%%)\n",
                   agg_exhaust_ok, agg_exhaust_checked,
                   agg_exhaust_checked > 0 ?
                       agg_exhaust_ok * 100 / agg_exhaust_checked : 0);
        }
        if (agg_meihua_checked > 0) {
            printf("  meihua valid:   %d / %d (%d%%)\n",
                   agg_meihua_clean, agg_meihua_checked,
                   agg_meihua_checked > 0 ?
                       agg_meihua_clean * 100 / agg_meihua_checked : 0);
        }
        /* Compute aggregate S */
        double aggregate_s = 0.0;
        int total_s_checks = agg_exhaust_checked + agg_meihua_checked;
        int total_s_ok = agg_exhaust_ok + agg_meihua_clean;
        if (total_s_checks > 0) {
            aggregate_s = 1.0 - (double)total_s_ok / total_s_checks;
        }
        printf("  aggregate S:    %.2f\n", aggregate_s);
        printf("==========================================\n");
        printf("  Per-file breakdown:\n");
        for (int fi = 0; fi < n_results; fi++) {
            printf("    %-24s %3d decls  S = %.2f  %d err  %d warn\n",
                   results[fi].name, results[fi].decls,
                   results[fi].s_value,
                   results[fi].errors, results[fi].warnings);
        }
        printf("==========================================\n");

        return total_errors > 0 ? 1 : 0;
    }

    if (!filename) {
        fprintf(stderr, "suhc: no input file\n");
        return 2;
    }

    /* Read source */
    char *source = read_file(filename);
    if (!source) return 2;

    /* Token dump mode */
    if (dump_tok) {
        dump_tokens(source);
        free(source);
        return 0;
    }

    /* Parse */
    DiagList *diags = diag_list_new();
    Lexer lex;
    lexer_init(&lex, source);

    Parser parser;
    parser_init(&parser, &lex, diags);
    Program *prog = parser_parse(&parser, filename);

    /* AST dump mode */
    if (dump_tree) {
        dump_ast(prog);
        if (diags->count > 0) {
            printf("\n--- Diagnostics ---\n");
            diag_print_all(diags);
        }
        int err = diag_has_errors(diags);
        parser_free(&parser);
        program_free(prog);
        diag_list_free(diags);
        free(source);
        return err ? 1 : 0;
    }

    /* Resolve imports */
    char *base_dir = NULL;
    {
        const char *slash = strrchr(filename, '/');
        if (slash) {
            size_t dlen = (size_t)(slash - filename);
            base_dir = malloc(dlen + 1);
            memcpy(base_dir, filename, dlen);
            base_dir[dlen] = '\0';
        } else {
            base_dir = strdup(".");
        }
    }
    Resolver resolver;
    resolver_init(&resolver, base_dir);
    if (ordbok_dir)
        resolver_add_path(&resolver, ordbok_dir);
    resolve_imports(prog, &resolver, diags);

    /* Run all checker passes */
    kindcheck(prog, diags);

    /* Build type registry from resolved declarations */
    TypeRegistry type_reg;
    type_registry_build(&type_reg, prog);

    if (dump_types) {
        type_registry_dump(&type_reg);
    }

    /* Type checking: arity + type annotations + incommensurability */
    TypecheckReport typerpt = typecheck(prog, &type_reg, diags);

    perpcheck(prog, diags);
    bloatlint(prog, diags);
    decidability_check(prog, diags);

    /* Run exhaustiveness checking and meihua validation */
    ExhaustReport exhaust = exhaustcheck(prog, diags);

    /* Run cross-file semantic validation */
    SemcheckReport semrep = semcheck(prog, diags);
    (void)semrep; /* report used in build report if needed */

    /* Run convergence analysis */
    ConvergenceReport conv = convergence_analyze(prog, diags);

    /* Print diagnostics */
    diag_print_all(diags);

    /* If emitting and there are errors, refuse to generate code */
    if (target != TARGET_NONE && diag_has_errors(diags)) {
        fprintf(stderr, "\nsuhc: errors found — no code generated.\n");
        print_build_report(prog, diags, &exhaust, &typerpt);
        type_registry_free(&type_reg);
        resolver_free(&resolver);
        free(base_dir);
        parser_free(&parser);
        program_free(prog);
        diag_list_free(diags);
        free(source);
        return 1;
    }

    /* Emit if target specified */
    if (target != TARGET_NONE) {
        char *default_out = NULL;
        const char *out_path = output_path;
        if (!out_path) {
            default_out = default_output_name(filename, target);
            out_path = default_out;
        }

        FILE *out = fopen(out_path, "w");
        if (!out) {
            fprintf(stderr, "suhc: cannot open '%s' for writing\n", out_path);
            free(default_out);
            parser_free(&parser);
            program_free(prog);
            diag_list_free(diags);
            free(source);
            return 2;
        }

        int emit_result = 0;
        if (target == TARGET_TYPESCRIPT) {
            emit_result = emit_typescript(prog, out, diags);
        } else if (target == TARGET_SQL) {
            emit_result = emit_sql(prog, out, diags);
        } else if (target == TARGET_C) {
            emit_result = emit_c(prog, out, diags);
        } else if (target == TARGET_ASM) {
            emit_result = emit_asm(prog, out, diags);
        }

        fclose(out);

        if (emit_result == 0) {
            const char *target_name = "?";
            switch (target) {
            case TARGET_TYPESCRIPT: target_name = "TypeScript"; break;
            case TARGET_SQL:        target_name = "SQL"; break;
            case TARGET_C:          target_name = "C"; break;
            case TARGET_ASM:        target_name = "x86_64 ASM"; break;
            default: break;
            }
            fprintf(stderr, "suhc: emitted %s → %s\n", target_name, out_path);
        }

        free(default_out);
    }

    /* Print build report */
    print_build_report(prog, diags, &exhaust, &typerpt);
    convergence_print(&conv, stdout);

    /* Save convergence history for trending */
    if (filename) {
        /* Build history path: same directory as input, .suhc_history */
        const char *slash = strrchr(filename, '/');
        char history_path[512];
        if (slash) {
            size_t dir_len = (size_t)(slash - filename);
            snprintf(history_path, sizeof(history_path), "%.*s/.suhc_history",
                     (int)dir_len, filename);
        } else {
            snprintf(history_path, sizeof(history_path), ".suhc_history");
        }

        /* Load previous ratio for trend display */
        double prev = convergence_load_prev_ratio(history_path);
        if (prev >= 0 && conv.total_cast > 0) {
            printf("  trend:          r₋₁ = %.2f", prev);
            if (conv.ratio < prev) printf(" (improving)");
            else if (conv.ratio > prev) printf(" (worsening)");
            printf("\n");
        }

        convergence_save_history(&conv, history_path);
    }

    int result = diag_has_errors(diags) ? 1 : 0;

    /* Cleanup */
    type_registry_free(&type_reg);
    resolver_free(&resolver);
    free(base_dir);
    parser_free(&parser);
    program_free(prog);
    diag_list_free(diags);
    free(source);

    return result;
}
