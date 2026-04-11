/*
 * suhc — convergence.c
 * Convergence tracker: ΔR.k tracking across builds.
 *
 * Phase 0C: Every traversal declares its entropy cast (ΔR.k).
 * The convergence tracker:
 *   1. Counts total cast (possibility spaces opened)
 *   2. Checks governance (is each cast governed by an ordbok term?)
 *   3. Computes convergence ratio across builds
 *   4. Detects smegmacra (write-without-read paths)
 *   5. Counts Yoneda gaps (declared but unobserved morphisms)
 */

#include "convergence.h"
#include "lexer.h"
#include "parser.h"
#include "resolve.h"
#include "kindcheck.h"
#include "exhaustcheck.h"
#include "semcheck.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>

/* ------------------------------------------------------------ */
/* File I/O helper                                               */
/* ------------------------------------------------------------ */

static char *conv_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, len, f);
    buf[nread] = '\0';
    fclose(f);
    return buf;
}

/* ------------------------------------------------------------ */
/* Cast analysis                                                 */
/* ------------------------------------------------------------ */

/* Count ΔR.k sections and check governance */
static void count_cast(Program *prog, int *total, int *governed, int *ungoverned) {
    *total = 0;
    *governed = 0;
    *ungoverned = 0;

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_TRAVERSAL) continue;

        for (size_t s = 0; s < d->as.traversal.section_count; s++) {
            TraversalSection *sec = &d->as.traversal.sections[s];
            if (sec->section_kind != KIND_DELTA_RK) continue;

            (*total)++;

            /* A cast is "governed" if it has a governed_by reference
             * that points to another declaration in the program. */
            /* Phase 0C heuristic: if the cast section has body content,
             * treat it as an identified (potentially governed) cast. */
            if (sec->body && sec->body->type == EXPR_IDENT &&
                sec->body->as.ident.name) {
                /* Check if the governed_by target exists */
                bool found = false;
                for (size_t j = 0; j < prog->count && !found; j++) {
                    if (prog->decls[j]->name.text &&
                        strcmp(prog->decls[j]->name.text,
                               sec->body->as.ident.name) == 0) {
                        found = true;
                    }
                }
                if (found) (*governed)++;
                else (*ungoverned)++;
            } else {
                (*ungoverned)++;
            }
        }
    }
}

/* ------------------------------------------------------------ */
/* Smegmacrum detection                                          */
/* ------------------------------------------------------------ */

/* Count write-without-read paths.
 * A traversal that has data (x) input but no output (ω) referenced
 * by any other traversal is a potential smegmacrum. */
static int count_smegmacra(Program *prog) {
    int count = 0;

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_TRAVERSAL || !d->name.text) continue;

        bool has_input = false;
        bool has_output = false;

        for (size_t s = 0; s < d->as.traversal.section_count; s++) {
            if (d->as.traversal.sections[s].section_kind == KIND_X)
                has_input = true;
            if (d->as.traversal.sections[s].section_kind == KIND_OMEGA)
                has_output = true;
        }

        if (has_input && !has_output) {
            count++;
        }
    }

    return count;
}

/* ------------------------------------------------------------ */
/* Yoneda gap counting                                           */
/* ------------------------------------------------------------ */

static int count_yoneda_gaps(Program *prog) {
    int count = 0;

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type != DECL_MORPHISM || !d->name.text) continue;

        bool observed = false;
        for (size_t j = 0; j < prog->count && !observed; j++) {
            if (j == i) continue;
            Decl *other = prog->decls[j];
            if (other->type == DECL_TRAVERSAL || other->type == DECL_PROJECTION) {
                /* Simple name match */
                if (other->type == DECL_TRAVERSAL) {
                    for (size_t s = 0; s < other->as.traversal.section_count; s++) {
                        Expr *body = other->as.traversal.sections[s].body;
                        if (body && body->type == EXPR_IDENT &&
                            body->as.ident.name &&
                            strcmp(body->as.ident.name, d->name.text) == 0) {
                            observed = true;
                        }
                        if (body && body->type == EXPR_CALL &&
                            body->as.call.callee &&
                            strcmp(body->as.call.callee, d->name.text) == 0) {
                            observed = true;
                        }
                    }
                }
            }
        }

        if (!observed) count++;
    }

    return count;
}

/* ------------------------------------------------------------ */
/* History file I/O                                              */
/* ------------------------------------------------------------ */

double convergence_load_prev_ratio(const char *history_path) {
    if (!history_path) return -1.0;
    FILE *f = fopen(history_path, "r");
    if (!f) return -1.0;

    double last_ratio = -1.0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Each line: cast_count ratio */
        int cast;
        double ratio;
        if (sscanf(line, "%d %lf", &cast, &ratio) == 2) {
            last_ratio = ratio;
        }
    }
    fclose(f);
    return last_ratio;
}

void convergence_save_history(const ConvergenceReport *report,
                              const char *history_path) {
    if (!history_path) return;
    FILE *f = fopen(history_path, "a");
    if (!f) return;
    fprintf(f, "%d %.4f\n", report->total_cast, report->ratio);
    fclose(f);
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

ConvergenceReport convergence_analyze(Program *prog, DiagList *diags) {
    ConvergenceReport r;
    memset(&r, 0, sizeof(r));

    count_cast(prog, &r.total_cast, &r.governed, &r.ungoverned);
    r.smegmacra = count_smegmacra(prog);
    r.yoneda_gaps = count_yoneda_gaps(prog);
    r.prev_ratio = -1.0;

    /* Convergence ratio requires comparing against previous build.
     * If no previous ratio, report current cast count only. */
    if (r.prev_ratio >= 0 && r.total_cast > 0) {
        /* ratio = current_cast / previous_cast (approximation) */
        r.ratio = (double)r.total_cast / (r.total_cast + r.governed);
    } else {
        r.ratio = r.total_cast > 0 ?
            (double)r.ungoverned / (double)r.total_cast : 0.0;
    }

    /* Emit warnings for ungoverned cast */
    if (r.ungoverned > 0) {
        diag_warn(diags, DIAG_MISSING_CAST, prog->filename,
                  0, 0, "%d ungoverned entropy cast(s) — "
                  "these possibility spaces have no governing principle",
                  r.ungoverned);
    }

    return r;
}

void convergence_print(const ConvergenceReport *report, FILE *out) {
    fprintf(out, "  cast:           %d possibility space%s\n",
            report->total_cast, report->total_cast == 1 ? "" : "s");
    fprintf(out, "  governed:       %d", report->governed);
    if (report->governed > 0) fprintf(out, " (by existing ordbok terms)");
    fprintf(out, "\n");
    fprintf(out, "  ungoverned:     %d", report->ungoverned);
    if (report->ungoverned > 0) fprintf(out, " (require amendment)");
    fprintf(out, "\n");

    if (report->prev_ratio >= 0) {
        fprintf(out, "  convergence:    r = %.2f", report->ratio);
        if (report->ratio < 1.0) fprintf(out, " (converging)");
        else if (report->ratio == 1.0) fprintf(out, " (stagnant)");
        else fprintf(out, " (DIVERGING — scope creep)");
        fprintf(out, "\n");
        fprintf(out, "  prev ratio:     r₋₁ = %.2f\n", report->prev_ratio);
    } else {
        fprintf(out, "  convergence:    r = %.2f (first build — no trend yet)\n",
                report->ratio);
    }

    if (report->smegmacra > 0) {
        fprintf(out, "  smegmacra:      %d (input without output — write-without-read)\n",
                report->smegmacra);
    }
    if (report->yoneda_gaps > 0) {
        fprintf(out, "  yoneda gaps:    %d (declared but unobserved morphisms)\n",
                report->yoneda_gaps);
    }
}

/* ------------------------------------------------------------ */
/* .convergence.json baseline I/O                                */
/* ------------------------------------------------------------ */

static int load_baseline(const char *dir_path, int *prev_ungoverned) {
    char path[512];
    snprintf(path, sizeof(path), "%s/.convergence.json", dir_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    /* Minimal JSON parse: just look for "ungoverned": <number> */
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "\"ungoverned\"");
        if (p) {
            p = strchr(p, ':');
            if (p) *prev_ungoverned = atoi(p + 1);
        }
    }
    fclose(f);
    return 1;
}

static void save_baseline(const char *dir_path, const ConvergenceDirReport *r) {
    char path[512];
    snprintf(path, sizeof(path), "%s/.convergence.json", dir_path);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n");
    fprintf(f, "  \"total_spaces\": %d,\n", r->total_spaces);
    fprintf(f, "  \"governed\": %d,\n", r->governed_spaces);
    fprintf(f, "  \"ungoverned\": %d,\n", r->ungoverned_spaces);
    fprintf(f, "  \"ratio\": %.4f,\n", r->ratio);
    fprintf(f, "  \"file_count\": %d\n", r->file_count);
    fprintf(f, "}\n");
    fclose(f);
}

/* ------------------------------------------------------------ */
/* Count possibility spaces in a program                         */
/* ------------------------------------------------------------ */

static void count_spaces(Program *prog, ExhaustReport *er,
                         int *total, int *governed, int *ungoverned,
                         int *dims, int *projs, int *meih) {
    *total = 0; *governed = 0; *ungoverned = 0;
    *dims = 0; *projs = 0; *meih = 0;

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        switch (d->type) {
        case DECL_DIMENSION:
            (*dims)++;
            if (d->as.dimension.members) {
                /* Dimension members stored as EXPR_ENUM with .enumeration.count */
                int mc = 1; /* at least 1 for the dimension itself */
                if (d->as.dimension.members->type == EXPR_ENUM)
                    mc = (int)d->as.dimension.members->as.enumeration.count;
                *total += mc;
                /* Dimensions with members are self-governing */
                *governed += mc;
            }
            break;
        case DECL_PROJECTION:
            (*projs)++;
            if (d->as.projection.fields) {
                *total += (int)d->as.projection.field_count;
                /* Governed if exhaustive (from ExhaustReport) */
            }
            break;
        case DECL_MEIHUA:
        case DECL_ZHULIN:
        case DECL_SONGQIAO:
            (*meih)++;
            *total += (int)d->as.exec_layer.param_count + 1; /* params + body */
            break;
        case DECL_TRAVERSAL:
            *total += (int)d->as.traversal.section_count;
            break;
        case DECL_IMPORT:
            *total += 1; /* each import is a possibility space (cross-file link) */
            *governed += 1; /* imports are validated by the resolver */
            break;
        default:
            break;
        }
    }

    /* Governed projections from exhaust report */
    *governed += er->projections_exhaustive;
    /* Governed meihua */
    *governed += er->meihua_clean;
    /* Governed traversals (with cast sections) */
    for (size_t i = 0; i < prog->count; i++) {
        if (prog->decls[i]->type == DECL_TRAVERSAL) {
            bool has_cast = false;
            for (size_t s = 0; s < prog->decls[i]->as.traversal.section_count; s++) {
                if (prog->decls[i]->as.traversal.sections[s].section_kind == KIND_DELTA_RK)
                    has_cast = true;
            }
            if (has_cast) *governed += (int)prog->decls[i]->as.traversal.section_count;
            else *ungoverned += (int)prog->decls[i]->as.traversal.section_count;
        }
    }

    *ungoverned = *total - *governed;
    if (*ungoverned < 0) *ungoverned = 0;
}

/* ------------------------------------------------------------ */
/* Directory-level convergence scan                              */
/* ------------------------------------------------------------ */

int convergence_scan_dir(const char *dir_path, FILE *out) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "suhc: cannot open directory '%s'\n", dir_path);
        return 2;
    }

    ConvergenceDirReport report;
    memset(&report, 0, sizeof(report));

    /* Load baseline */
    int prev_ung = 0;
    report.has_baseline = load_baseline(dir_path, &prev_ung);
    report.prev_ungoverned = prev_ung;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        size_t nlen = strlen(ent->d_name);
        if (nlen < 5 || strcmp(ent->d_name + nlen - 4, ".szh") != 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);

        char *src = conv_read_file(path);
        if (!src) continue;

        DiagList *diags = diag_list_new();
        Lexer lex;
        lexer_init(&lex, src);
        Parser parser;
        parser_init(&parser, &lex, diags);
        Program *prog = parser_parse(&parser, path);

        Resolver resolver;
        resolver_init(&resolver, dir_path);
        resolve_imports(prog, &resolver, diags);

        kindcheck(prog, diags);
        ExhaustReport er = exhaustcheck(prog, diags);

        int ft, fg, fu, fd, fp, fm;
        count_spaces(prog, &er, &ft, &fg, &fu, &fd, &fp, &fm);

        report.total_spaces += ft;
        report.governed_spaces += fg;
        report.ungoverned_spaces += fu;

        if (report.file_count < 128) {
            ConvergenceFileEntry *fe = &report.files[report.file_count];
            snprintf(fe->filename, 256, "%s", ent->d_name);
            fe->total = ft;
            fe->governed = fg;
            fe->ungoverned = fu;
            fe->dimensions = fd;
            fe->projections = fp;
            fe->meihua = fm;
            report.file_count++;
        }

        parser_free(&parser);
        program_free(prog);
        diag_list_free(diags);
        free(src);
    }
    closedir(dir);

    /* Compute convergence ratio */
    if (report.has_baseline && report.prev_ungoverned > 0) {
        report.ratio = (double)report.ungoverned_spaces / (double)report.prev_ungoverned;
    } else if (report.total_spaces > 0) {
        report.ratio = (double)report.ungoverned_spaces / (double)report.total_spaces;
    } else {
        report.ratio = 0.0;
    }

    /* Save new baseline */
    save_baseline(dir_path, &report);

    /* Print report */
    fprintf(out, "Convergence Report: %s/\n", dir_path);
    fprintf(out, "===========================\n");
    fprintf(out, "  possibility spaces:  %d\n", report.total_spaces);
    fprintf(out, "  governed:            %d\n", report.governed_spaces);
    fprintf(out, "  ungoverned:          %d\n", report.ungoverned_spaces);
    fprintf(out, "  ΔR.k:               %d\n", report.ungoverned_spaces);
    if (report.has_baseline) {
        fprintf(out, "  previous ΔR.k:      %d  (from .convergence.json)\n", report.prev_ungoverned);
        fprintf(out, "  ratio:               %.2f", report.ratio);
        if (report.ratio < 1.0) fprintf(out, "  (converging)");
        else if (report.ratio > 1.0) fprintf(out, "  (DIVERGING)");
        else fprintf(out, "  (stable)");
        fprintf(out, "\n");
    } else {
        fprintf(out, "  ratio:               %.2f  (first measurement — no baseline)\n",
                report.ratio);
    }
    fprintf(out, "---------------------------\n");
    fprintf(out, "  Cast by file:\n");
    for (int i = 0; i < report.file_count; i++) {
        ConvergenceFileEntry *fe = &report.files[i];
        fprintf(out, "    %-30s %d ungoverned / %d total\n",
                fe->filename, fe->ungoverned, fe->total);
    }
    fprintf(out, "===========================\n");

    return 0;
}
