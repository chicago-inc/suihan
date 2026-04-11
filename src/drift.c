/*
 * suhc — drift.c
 * Ordbok ↔ Spoxis drift detection.
 *
 * D13 applied to the ordbok's relationship with its target
 * codebase. S measures how much of the ordbok's declared
 * structure is visible (matched) in the Spoxis source.
 */

#include "drift.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* ------------------------------------------------------------ */
/* Utilities                                                     */
/* ------------------------------------------------------------ */

static char *str_dup(const char *s) {
    return s ? strdup(s) : NULL;
}

/* Convert snake_case to camelCase for matching TS identifiers */
static char *to_camel(const char *snake) {
    if (!snake) return strdup("unknown");
    size_t len = strlen(snake);
    char *out = malloc(len + 1);
    size_t j = 0;
    bool cap_next = false;
    for (size_t i = 0; i < len; i++) {
        if (snake[i] == '_') {
            cap_next = true;
        } else {
            out[j++] = cap_next ? toupper((unsigned char)snake[i]) : snake[i];
            cap_next = false;
        }
    }
    out[j] = '\0';
    return out;
}

/* Case-insensitive string comparison */
static bool str_eq_ci(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++; b++;
    }
    return *a == *b;
}

/* Check if a name matches any of a declaration's target aliases */
static bool matches_target(Decl *d, const char *name) {
    if (!name) return false;
    /* Check primary name (snake_case and camelCase) */
    if (strcmp(name, d->name.text) == 0) return true;
    char *camel = to_camel(d->name.text);
    bool m = str_eq_ci(name, camel);
    free(camel);
    if (m) return true;
    /* Check @targets aliases */
    for (size_t i = 0; i < d->target_count; i++) {
        if (str_eq_ci(name, d->targets[i])) return true;
    }
    return false;
}

static void add_entry(DriftReport *r, const char *name, const char *type,
                       DriftStatus status, double s, const char *detail) {
    if (r->count >= r->capacity) {
        r->capacity = r->capacity ? r->capacity * 2 : 16;
        r->entries = realloc(r->entries, r->capacity * sizeof(DriftEntry));
    }
    DriftEntry *e = &r->entries[r->count++];
    e->decl_name = str_dup(name);
    e->decl_type = str_dup(type);
    e->status = status;
    e->s_value = s;
    e->detail = str_dup(detail);

    switch (status) {
    case DRIFT_MATCHED:     r->matched++; break;
    case DRIFT_ORDBOK_ONLY: r->ordbok_only++; break;
    case DRIFT_SPOXIS_ONLY: r->spoxis_only++; break;
    case DRIFT_DIVERGED:    r->diverged++; break;
    }
}

/* ------------------------------------------------------------ */
/* ξ constant comparison                                         */
/* ------------------------------------------------------------ */

static void compare_xi_const(Decl *d, TsScanResult *ts, DriftReport *r) {
    if (!d->name.text) return;

    /* Get the ordbok value */
    const char *ordbok_val = NULL;
    if (d->as.kinded.value) {
        Expr *v = d->as.kinded.value;
        if (v->type == EXPR_STRING) ordbok_val = v->as.string.value;
        else if (v->type == EXPR_NUMBER) ordbok_val = v->as.number.text;
    }

    /* Find matching TS const */
    char *camel = to_camel(d->name.text);
    for (size_t i = 0; i < ts->const_count; i++) {
        if (strcmp(ts->consts[i].name, camel) == 0 ||
            strcmp(ts->consts[i].name, d->name.text) == 0) {
            /* Found match — compare values */
            if (ordbok_val && ts->consts[i].value) {
                if (strcmp(ordbok_val, ts->consts[i].value) == 0) {
                    add_entry(r, d->name.text, "xi_const",
                              DRIFT_MATCHED, 0.0, "values match");
                } else {
                    char detail[512];
                    snprintf(detail, sizeof(detail),
                             "ordbok=%s spoxis=%s",
                             ordbok_val, ts->consts[i].value);
                    add_entry(r, d->name.text, "xi_const",
                              DRIFT_DIVERGED, 1.0, detail);
                }
            } else {
                add_entry(r, d->name.text, "xi_const",
                          DRIFT_MATCHED, 0.1, "names match, values not comparable");
            }
            free(camel);
            return;
        }
    }

    add_entry(r, d->name.text, "xi_const",
              DRIFT_ORDBOK_ONLY, 1.0, "no matching TS const found");
    free(camel);
}

/* ------------------------------------------------------------ */
/* Projection comparison                                         */
/* ------------------------------------------------------------ */

static void compare_projection(Decl *d, TsScanResult *ts, DriftReport *r) {
    if (!d->name.text) return;

    /* Collect ordbok case arm values (first element of each pattern tuple) */
    size_t ordbok_arms = d->as.projection.arm_count;
    char **ordbok_cases = malloc(ordbok_arms * sizeof(char *));
    size_t ordbok_case_count = 0;

    for (size_t i = 0; i < ordbok_arms; i++) {
        ProjectionArm *arm = &d->as.projection.arms[i];
        if (!arm->pattern) continue;

        /* Extract case values from tuple pattern */
        if (arm->pattern->type == EXPR_LIST && arm->pattern->as.list.count >= 1) {
            Expr *first = arm->pattern->as.list.items[0];
            if (first && first->type == EXPR_IDENT && first->as.ident.name) {
                if (strcmp(first->as.ident.name, "_") != 0) {
                    ordbok_cases[ordbok_case_count++] = strdup(first->as.ident.name);
                }
            }
        }
    }

    /* Find matching TS switch block */
    char *camel = to_camel(d->name.text);
    TsSwitchBlock *match = NULL;

    for (size_t i = 0; i < ts->switch_count; i++) {
        /* Match by switch variable or by function name containing projection name */
        if (ts->switches[i].switch_var) {
            /* Check if any switch has branches that overlap with ordbok cases */
            int overlap = 0;
            for (size_t j = 0; j < ts->switches[i].branch_count; j++) {
                for (size_t k = 0; k < ordbok_case_count; k++) {
                    if (str_eq_ci(ts->switches[i].branches[j].case_value,
                                  ordbok_cases[k])) {
                        overlap++;
                    }
                }
            }
            /* If >50% of ordbok cases match, consider it the corresponding switch */
            if (ordbok_case_count > 0 && overlap > 0 &&
                (double)overlap / ordbok_case_count > 0.3) {
                match = &ts->switches[i];
                break;
            }
        }
    }

    if (!match) {
        add_entry(r, d->name.text, "projection",
                  DRIFT_ORDBOK_ONLY, 1.0, "no matching TS switch/ternary found");
        free(camel);
        for (size_t i = 0; i < ordbok_case_count; i++) free(ordbok_cases[i]);
        free(ordbok_cases);
        return;
    }

    /* Compare case arms */
    int matched = 0, ordbok_only = 0, spoxis_only = 0;

    for (size_t i = 0; i < ordbok_case_count; i++) {
        bool found = false;
        for (size_t j = 0; j < match->branch_count; j++) {
            if (str_eq_ci(match->branches[j].case_value, ordbok_cases[i])) {
                found = true;
                matched++;
                break;
            }
        }
        if (!found) ordbok_only++;
    }

    for (size_t j = 0; j < match->branch_count; j++) {
        if (strcmp(match->branches[j].case_value, "_") == 0) continue;
        bool found = false;
        for (size_t i = 0; i < ordbok_case_count; i++) {
            if (str_eq_ci(match->branches[j].case_value, ordbok_cases[i])) {
                found = true;
                break;
            }
        }
        if (!found) spoxis_only++;
    }

    int total = matched + ordbok_only + spoxis_only;
    double s = total > 0 ? 1.0 - (double)matched / total : 1.0;

    char detail[512];
    snprintf(detail, sizeof(detail),
             "%d matched, %d ordbok-only, %d spoxis-only (of %zu ordbok arms)",
             matched, ordbok_only, spoxis_only, ordbok_case_count);

    DriftStatus status = (ordbok_only == 0 && spoxis_only == 0)
        ? DRIFT_MATCHED : DRIFT_DIVERGED;
    add_entry(r, d->name.text, "projection", status, s, detail);

    free(camel);
    for (size_t i = 0; i < ordbok_case_count; i++) free(ordbok_cases[i]);
    free(ordbok_cases);
}

/* ------------------------------------------------------------ */
/* Meihua comparison                                             */
/* ------------------------------------------------------------ */

static void compare_meihua(Decl *d, TsScanResult *ts, DriftReport *r) {
    if (!d->name.text) return;

    char *camel = to_camel(d->name.text);

    for (size_t i = 0; i < ts->function_count; i++) {
        if (strcmp(ts->functions[i].name, camel) == 0 ||
            strcmp(ts->functions[i].name, d->name.text) == 0) {
            /* Check parameter count */
            size_t ordbok_params = d->as.exec_layer.param_count;
            size_t ts_params = ts->functions[i].param_count;

            if (ordbok_params == ts_params) {
                add_entry(r, d->name.text, "meihua",
                          DRIFT_MATCHED, 0.0,
                          "function found with matching arity");
            } else {
                char detail[256];
                snprintf(detail, sizeof(detail),
                         "arity mismatch: ordbok=%zu, spoxis=%zu",
                         ordbok_params, ts_params);
                add_entry(r, d->name.text, "meihua",
                          DRIFT_DIVERGED, 0.5, detail);
            }
            free(camel);
            return;
        }
    }

    add_entry(r, d->name.text, "meihua",
              DRIFT_ORDBOK_ONLY, 1.0, "no matching TS function found");
    free(camel);
}

/* ------------------------------------------------------------ */
/* ζ (shape) comparison — matches against switch blocks (dicts,  */
/* frozensets, if/elif chains) by name                           */
/* ------------------------------------------------------------ */

static void compare_zeta(Decl *d, TsScanResult *ts, DriftReport *r) {
    if (!d->name.text) return;

    /* Check switch blocks (dicts, frozensets, if/elif chains) */
    for (size_t i = 0; i < ts->switch_count; i++) {
        if (!ts->switches[i].switch_var) continue;
        if (matches_target(d, ts->switches[i].switch_var)) {
            char detail[512];
            snprintf(detail, sizeof(detail),
                     "ζ declaration matched target '%s' (%zu branches) — "
                     "hardcoded structure that should be derived",
                     ts->switches[i].switch_var,
                     ts->switches[i].branch_count);
            add_entry(r, d->name.text, "zeta",
                      DRIFT_DIVERGED, 0.7, detail);
            return;
        }
    }

    /* Check consts (hardcoded values that should be computed) */
    for (size_t i = 0; i < ts->const_count; i++) {
        if (matches_target(d, ts->consts[i].name)) {
            char detail[256];
            snprintf(detail, sizeof(detail),
                     "ζ declaration matched const '%s' = %s (should be computed, not stored)",
                     ts->consts[i].name,
                     ts->consts[i].value ? ts->consts[i].value : "?");
            add_entry(r, d->name.text, "zeta",
                      DRIFT_DIVERGED, 0.8, detail);
            return;
        }
    }

    /* Check functions (ζ projections implemented as functions) */
    for (size_t i = 0; i < ts->function_count; i++) {
        if (matches_target(d, ts->functions[i].name)) {
            char detail[256];
            snprintf(detail, sizeof(detail),
                     "ζ declaration matched function '%s'",
                     ts->functions[i].name);
            add_entry(r, d->name.text, "zeta",
                      DRIFT_MATCHED, 0.1, detail);
            return;
        }
    }

    add_entry(r, d->name.text, "zeta",
              DRIFT_ORDBOK_ONLY, 1.0, "no matching target found");
}

/* ------------------------------------------------------------ */
/* Dimension comparison — matches member list against frozensets  */
/* ------------------------------------------------------------ */

static void compare_dimension(Decl *d, TsScanResult *ts, DriftReport *r) {
    if (!d->name.text) return;

    /* Collect ordbok dimension members */
    size_t ordbok_count = 0;
    char **ordbok_members = NULL;
    if (d->as.dimension.members && d->as.dimension.members->type == EXPR_LIST) {
        ordbok_count = d->as.dimension.members->as.list.count;
        ordbok_members = malloc(ordbok_count * sizeof(char *));
        for (size_t i = 0; i < ordbok_count; i++) {
            Expr *m = d->as.dimension.members->as.list.items[i];
            ordbok_members[i] = (m && m->type == EXPR_IDENT && m->as.ident.name)
                ? strdup(m->as.ident.name) : strdup("?");
        }
    }

    /* Find matching switch block (frozenset/set in Python) by member overlap */
    TsSwitchBlock *best = NULL;
    int best_overlap = 0;

    for (size_t i = 0; i < ts->switch_count; i++) {
        int overlap = 0;
        for (size_t j = 0; j < ts->switches[i].branch_count; j++) {
            for (size_t k = 0; k < ordbok_count; k++) {
                if (str_eq_ci(ts->switches[i].branches[j].case_value,
                              ordbok_members[k])) {
                    overlap++;
                    break;
                }
            }
        }
        if (overlap > best_overlap) {
            best_overlap = overlap;
            best = &ts->switches[i];
        }
    }

    if (best && ordbok_count > 0 && (double)best_overlap / ordbok_count > 0.3) {
        int target_extra = (int)best->branch_count - best_overlap;
        char detail[512];
        snprintf(detail, sizeof(detail),
                 "dimension matched target '%s': %d/%zu members overlap, "
                 "%d target-only (total target: %zu)",
                 best->switch_var ? best->switch_var : "?",
                 best_overlap, ordbok_count,
                 target_extra > 0 ? target_extra : 0,
                 best->branch_count);

        DriftStatus status = (best_overlap == (int)ordbok_count && target_extra == 0)
            ? DRIFT_MATCHED : DRIFT_DIVERGED;
        double s = 1.0 - (double)best_overlap / (ordbok_count + (target_extra > 0 ? target_extra : 0));
        add_entry(r, d->name.text, "dimension", status, s, detail);
    } else {
        add_entry(r, d->name.text, "dimension",
                  DRIFT_ORDBOK_ONLY, 1.0, "no matching target set/frozenset found");
    }

    for (size_t i = 0; i < ordbok_count; i++) free(ordbok_members[i]);
    free(ordbok_members);
}

/* ------------------------------------------------------------ */
/* Traversal comparison — matches name against functions          */
/* ------------------------------------------------------------ */

static void compare_traversal(Decl *d, TsScanResult *ts, DriftReport *r) {
    if (!d->name.text) return;

    for (size_t i = 0; i < ts->function_count; i++) {
        if (matches_target(d, ts->functions[i].name)) {
            char detail[256];
            snprintf(detail, sizeof(detail),
                     "traversal matched function '%s'",
                     ts->functions[i].name);
            add_entry(r, d->name.text, "traversal",
                      DRIFT_MATCHED, 0.0, detail);
            return;
        }
    }

    add_entry(r, d->name.text, "traversal",
              DRIFT_ORDBOK_ONLY, 0.5, "no matching target function (expected for new declarations)");
}

/* ------------------------------------------------------------ */
/* Public API                                                    */
/* ------------------------------------------------------------ */

DriftReport drift_compare(Program *ordbok, TsScanResult *spoxis) {
    DriftReport r;
    memset(&r, 0, sizeof(r));

    for (size_t i = 0; i < ordbok->count; i++) {
        Decl *d = ordbok->decls[i];

        switch (d->type) {
        case DECL_KINDED_VALUE:
            if (d->kind == KIND_XI) {
                compare_xi_const(d, spoxis, &r);
            } else if (d->kind == KIND_ZETA) {
                compare_zeta(d, spoxis, &r);
            }
            break;

        case DECL_PROJECTION:
            compare_projection(d, spoxis, &r);
            break;

        case DECL_MEIHUA:
            compare_meihua(d, spoxis, &r);
            break;

        case DECL_DIMENSION:
            compare_dimension(d, spoxis, &r);
            break;

        case DECL_TRAVERSAL:
            compare_traversal(d, spoxis, &r);
            break;

        default:
            break;
        }
    }

    /* Compute aggregate S */
    if (r.count > 0) {
        double total_s = 0;
        for (size_t i = 0; i < r.count; i++) {
            total_s += r.entries[i].s_value;
        }
        r.aggregate_s = total_s / r.count;
    }

    return r;
}

void drift_print(const DriftReport *report, FILE *out) {
    fprintf(out, "DRIFT REPORT\n");
    fprintf(out, "==========================================\n");
    fprintf(out, "  declarations compared: %zu\n", report->count);
    fprintf(out, "  matched:              %d\n", report->matched);
    fprintf(out, "  ordbok-only:          %d\n", report->ordbok_only);
    fprintf(out, "  spoxis-only:          %d\n", report->spoxis_only);
    fprintf(out, "  diverged:             %d\n", report->diverged);
    fprintf(out, "  aggregate S:          %.2f\n", report->aggregate_s);
    fprintf(out, "------------------------------------------\n");

    for (size_t i = 0; i < report->count; i++) {
        const DriftEntry *e = &report->entries[i];
        const char *status_str = "???";
        switch (e->status) {
        case DRIFT_MATCHED:     status_str = "MATCH"; break;
        case DRIFT_ORDBOK_ONLY: status_str = "ORDBOK"; break;
        case DRIFT_SPOXIS_ONLY: status_str = "SPOXIS"; break;
        case DRIFT_DIVERGED:    status_str = "DRIFT"; break;
        }
        fprintf(out, "  %-6s S=%.2f %-12s %s",
                status_str, e->s_value, e->decl_type, e->decl_name);
        if (e->detail) fprintf(out, " — %s", e->detail);
        fprintf(out, "\n");
    }
    fprintf(out, "==========================================\n");
}

void drift_print_json(const DriftReport *report, FILE *out) {
    fprintf(out, "{\n");
    fprintf(out, "  \"aggregate_s\": %.4f,\n", report->aggregate_s);
    fprintf(out, "  \"total_declarations\": %zu,\n", report->count);
    fprintf(out, "  \"matched\": %d,\n", report->matched);
    fprintf(out, "  \"ordbok_only\": %d,\n", report->ordbok_only);
    fprintf(out, "  \"spoxis_only\": %d,\n", report->spoxis_only);
    fprintf(out, "  \"diverged\": %d,\n", report->diverged);
    fprintf(out, "  \"mappings\": [\n");

    for (size_t i = 0; i < report->count; i++) {
        const DriftEntry *e = &report->entries[i];
        const char *status_str = "unknown";
        switch (e->status) {
        case DRIFT_MATCHED:     status_str = "matched"; break;
        case DRIFT_ORDBOK_ONLY: status_str = "ordbok_only"; break;
        case DRIFT_SPOXIS_ONLY: status_str = "spoxis_only"; break;
        case DRIFT_DIVERGED:    status_str = "diverged"; break;
        }
        fprintf(out, "    {\n");
        fprintf(out, "      \"name\": \"%s\",\n", e->decl_name);
        fprintf(out, "      \"type\": \"%s\",\n", e->decl_type);
        fprintf(out, "      \"status\": \"%s\",\n", status_str);
        fprintf(out, "      \"s_value\": %.4f", e->s_value);
        if (e->detail) {
            fprintf(out, ",\n      \"detail\": \"%s\"", e->detail);
        }
        fprintf(out, "\n    }%s\n", i + 1 < report->count ? "," : "");
    }

    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
}

void drift_free(DriftReport *report) {
    for (size_t i = 0; i < report->count; i++) {
        free(report->entries[i].decl_name);
        free(report->entries[i].decl_type);
        free(report->entries[i].detail);
    }
    free(report->entries);
}
