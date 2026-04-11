/*
 * suhc — diagnostic.c
 * Error and warning reporting.
 */

#include "diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

const char *diag_category_name(DiagCategory cat) {
    switch (cat) {
    case DIAG_PARSE_ERROR:       return "parse_error";
    case DIAG_LEX_ERROR:         return "lex_error";
    case DIAG_REDUPLICATION:     return "reduplication";
    case DIAG_NICHE_PIPE:        return "niche_pipe";
    case DIAG_TEMPORAL_SEDIMENT: return "temporal_sediment";
    case DIAG_FAILURE_TO_DERIVE: return "failure_to_derive";
    case DIAG_SCOPE_CONFUSION:   return "scope_confusion";
    case DIAG_OBTRUDING_DOC:     return "obtruding_documentation";
    case DIAG_FORWARD_REFERENCE: return "forward_reference";
    case DIAG_KIND_MISMATCH:     return "kind_mismatch";
    case DIAG_PERPENDICULAR_CROSS: return "perpendicular_cross";
    case DIAG_IMMUTABILITY:      return "immutability";
    case DIAG_UNDECIDABLE_ACTION:return "undecidable_action";
    case DIAG_MISSING_CAST:      return "missing_cast";
    case DIAG_YONEDA_GAP:        return "yoneda_gap";
    case DIAG_IMPORT_ERROR:      return "import_error";
    case DIAG_CIRCULAR_IMPORT:   return "circular_import";
    case DIAG_TYPE_MISMATCH:     return "type_mismatch";
    case DIAG_ARITY_MISMATCH:    return "arity_mismatch";
    case DIAG_UNKNOWN_TYPE:      return "unknown_type";
    }
    return "unknown";
}

DiagList *diag_list_new(void) {
    DiagList *dl = calloc(1, sizeof(DiagList));
    dl->capacity = 32;
    dl->items = calloc(dl->capacity, sizeof(Diagnostic));
    return dl;
}

void diag_list_free(DiagList *dl) {
    if (!dl) return;
    for (size_t i = 0; i < dl->count; i++) {
        free(dl->items[i].message);
        free(dl->items[i].filename);
    }
    free(dl->items);
    free(dl);
}

void diag_emit(DiagList *dl, Severity sev, DiagCategory cat,
               const char *filename, int line, int col,
               const char *fmt, ...) {
    /* Cascade suppression for errors only (warnings are never suppressed) */
    if (sev == SEV_ERROR) {
        int cat_idx = (int)cat;
        if (cat_idx >= 0 && cat_idx < DIAG_CATEGORY_COUNT) {
            dl->category_error_count[cat_idx]++;

            /* Suppress if per-category limit reached */
            if (dl->category_error_count[cat_idx] > DIAG_MAX_ERRORS_PER_CATEGORY) {
                dl->suppressed_count++;
                dl->error_count++;
                return;
            }
        }

        /* Suppress if total error limit reached */
        if (dl->error_count >= DIAG_MAX_TOTAL_ERRORS) {
            dl->suppressed_count++;
            dl->error_count++;
            if (!dl->total_limit_reached) {
                dl->total_limit_reached = 1;
            }
            return;
        }
    }

    if (dl->count >= dl->capacity) {
        dl->capacity *= 2;
        dl->items = realloc(dl->items, dl->capacity * sizeof(Diagnostic));
    }

    Diagnostic *d = &dl->items[dl->count++];
    d->severity = sev;
    d->category = cat;
    d->line = line;
    d->col = col;
    d->filename = filename ? strdup(filename) : NULL;

    va_list args;
    va_start(args, fmt);
    /* Measure needed size */
    va_list args2;
    va_copy(args2, args);
    int len = vsnprintf(NULL, 0, fmt, args2);
    va_end(args2);

    d->message = malloc(len + 1);
    vsnprintf(d->message, len + 1, fmt, args);
    va_end(args);

    if (sev == SEV_ERROR) dl->error_count++;
    else if (sev == SEV_WARNING) dl->warning_count++;
}

void diag_print_all(const DiagList *dl) {
    for (size_t i = 0; i < dl->count; i++) {
        const Diagnostic *d = &dl->items[i];
        const char *sev_str = "note";
        if (d->severity == SEV_ERROR) sev_str = "error";
        else if (d->severity == SEV_WARNING) sev_str = "warning";

        const char *msg = d->message ? d->message : "<null>";
        if (d->filename) {
            fprintf(stderr, "%s:%d:%d: %s [%s]: %s\n",
                    d->filename, d->line, d->col,
                    sev_str, diag_category_name(d->category),
                    msg);
        } else {
            fprintf(stderr, "%d:%d: %s [%s]: %s\n",
                    d->line, d->col,
                    sev_str, diag_category_name(d->category),
                    msg);
        }
    }

    /* Print per-category suppression summaries */
    for (int c = 0; c < DIAG_CATEGORY_COUNT; c++) {
        int total = dl->category_error_count[c];
        if (total > DIAG_MAX_ERRORS_PER_CATEGORY) {
            int suppressed = total - DIAG_MAX_ERRORS_PER_CATEGORY;
            fprintf(stderr, "... and %d more %s error%s\n",
                    suppressed, diag_category_name((DiagCategory)c),
                    suppressed == 1 ? "" : "s");
        }
    }

    /* Print total limit message */
    if (dl->total_limit_reached) {
        fprintf(stderr, "error limit reached (%d errors). Fix these and recompile.\n",
                DIAG_MAX_TOTAL_ERRORS);
    }
}

int diag_has_errors(const DiagList *dl) {
    return dl->error_count > 0;
}

/* ── P1A: CI output formatters ──────────────────────────────────── */

static const char *sev_to_string(Severity sev) {
    switch (sev) {
    case SEV_ERROR:   return "error";
    case SEV_WARNING: return "warning";
    case SEV_NOTE:    return "note";
    }
    return "note";
}

static void json_escape(FILE *out, const char *s) {
    if (!s) { fprintf(out, "null"); return; }
    fputc('"', out);
    for (const char *p = s; *p; p++) {
        switch (*p) {
        case '"':  fprintf(out, "\\\""); break;
        case '\\': fprintf(out, "\\\\"); break;
        case '\n': fprintf(out, "\\n");  break;
        case '\t': fprintf(out, "\\t");  break;
        case '\r': fprintf(out, "\\r");  break;
        default:   fputc(*p, out);       break;
        }
    }
    fputc('"', out);
}

void diag_print_json(const DiagList *dl, FILE *out) {
    for (size_t i = 0; i < dl->count; i++) {
        const Diagnostic *d = &dl->items[i];
        fprintf(out, "{\"severity\":\"%s\",\"category\":\"%s\",\"file\":",
                sev_to_string(d->severity),
                diag_category_name(d->category));
        json_escape(out, d->filename);
        fprintf(out, ",\"line\":%d,\"col\":%d,\"message\":",
                d->line, d->col);
        json_escape(out, d->message ? d->message : "");
        fprintf(out, "}\n");
    }
}

static const char *sarif_rule_id(DiagCategory cat) {
    switch (cat) {
    case DIAG_PARSE_ERROR:       return "SUHC001";
    case DIAG_LEX_ERROR:         return "SUHC002";
    case DIAG_REDUPLICATION:     return "SUHC003";
    case DIAG_NICHE_PIPE:        return "SUHC004";
    case DIAG_TEMPORAL_SEDIMENT: return "SUHC005";
    case DIAG_FAILURE_TO_DERIVE: return "SUHC006";
    case DIAG_SCOPE_CONFUSION:   return "SUHC007";
    case DIAG_OBTRUDING_DOC:     return "SUHC008";
    case DIAG_FORWARD_REFERENCE: return "SUHC009";
    case DIAG_KIND_MISMATCH:     return "SUHC010";
    case DIAG_PERPENDICULAR_CROSS: return "SUHC011";
    case DIAG_IMMUTABILITY:      return "SUHC012";
    case DIAG_UNDECIDABLE_ACTION:return "SUHC013";
    case DIAG_MISSING_CAST:      return "SUHC014";
    case DIAG_YONEDA_GAP:        return "SUHC015";
    case DIAG_IMPORT_ERROR:      return "SUHC016";
    case DIAG_CIRCULAR_IMPORT:   return "SUHC017";
    case DIAG_TYPE_MISMATCH:     return "SUHC018";
    case DIAG_ARITY_MISMATCH:    return "SUHC019";
    case DIAG_UNKNOWN_TYPE:      return "SUHC020";
    }
    return "SUHC000";
}

static const char *sarif_rule_desc(DiagCategory cat) {
    switch (cat) {
    case DIAG_PARSE_ERROR:       return "Parse error";
    case DIAG_LEX_ERROR:         return "Lexer error";
    case DIAG_REDUPLICATION:     return "Reduplication (bloat #1)";
    case DIAG_NICHE_PIPE:        return "Niche pipe (bloat #2)";
    case DIAG_TEMPORAL_SEDIMENT: return "Temporal sediment (bloat #3)";
    case DIAG_FAILURE_TO_DERIVE: return "Failure to derive (bloat #4)";
    case DIAG_SCOPE_CONFUSION:   return "Scope confusion (bloat #5)";
    case DIAG_OBTRUDING_DOC:     return "Obtruding documentation (bloat #6)";
    case DIAG_FORWARD_REFERENCE: return "Forward reference";
    case DIAG_KIND_MISMATCH:     return "Kind mismatch";
    case DIAG_PERPENDICULAR_CROSS: return "Perpendicular cross";
    case DIAG_IMMUTABILITY:      return "Immutability violation";
    case DIAG_UNDECIDABLE_ACTION:return "Undecidable action";
    case DIAG_MISSING_CAST:      return "Missing entropy cast";
    case DIAG_YONEDA_GAP:        return "Yoneda gap";
    case DIAG_IMPORT_ERROR:      return "Import error";
    case DIAG_CIRCULAR_IMPORT:   return "Circular import";
    case DIAG_TYPE_MISMATCH:     return "Type mismatch";
    case DIAG_ARITY_MISMATCH:    return "Arity mismatch";
    case DIAG_UNKNOWN_TYPE:      return "Unknown type";
    }
    return "Unknown diagnostic";
}

void diag_print_sarif(const DiagList *dl, FILE *out) {
    fprintf(out,
        "{\"$schema\":\"https://raw.githubusercontent.com/oasis-tcs/"
        "sarif-spec/main/sarif-2.1/schema/sarif-schema-2.1.0.json\","
        "\"version\":\"2.1.0\",\"runs\":[{\"tool\":{\"driver\":{"
        "\"name\":\"suhc\",\"version\":\"1.1.0\",\"rules\":[");

    /* Emit rule descriptors for all categories */
    for (int c = 0; c < DIAG_CATEGORY_COUNT; c++) {
        if (c > 0) fputc(',', out);
        fprintf(out, "{\"id\":\"%s\",\"shortDescription\":{\"text\":\"%s\"}}",
                sarif_rule_id((DiagCategory)c),
                sarif_rule_desc((DiagCategory)c));
    }

    fprintf(out, "]}},\"results\":[");

    /* Emit each diagnostic as a result */
    for (size_t i = 0; i < dl->count; i++) {
        const Diagnostic *d = &dl->items[i];
        if (i > 0) fputc(',', out);

        fprintf(out, "{\"ruleId\":\"%s\",\"level\":\"%s\",\"message\":{\"text\":",
                sarif_rule_id(d->category),
                sev_to_string(d->severity));
        json_escape(out, d->message ? d->message : "");
        fprintf(out, "},\"locations\":[{\"physicalLocation\":{"
                "\"artifactLocation\":{\"uri\":");
        json_escape(out, d->filename ? d->filename : "");
        fprintf(out, "},\"region\":{\"startLine\":%d,\"startColumn\":%d}}}]}",
                d->line, d->col);
    }

    fprintf(out, "]}]}\n");
}

void diag_print_format(const DiagList *dl, OutputFormat fmt, FILE *out) {
    switch (fmt) {
    case OUTPUT_HUMAN:
        diag_print_all(dl);
        break;
    case OUTPUT_JSON:
        diag_print_json(dl, out);
        break;
    case OUTPUT_SARIF:
        diag_print_sarif(dl, out);
        break;
    }
}
