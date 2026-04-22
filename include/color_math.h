/*
 * suhc — color_math.h
 *
 * Layer 1 (WCAG, inherited): sRGB → linear → relative luminance → contrast ratio.
 * Layer 2 (D35, 2026-04-22):  sRGB → OKLab → OKLCh → ΔL/ΔC/ΔH + two-channel rule.
 *
 * Used by exhaustcheck.c when evaluating axiom_contrast_aa predicates
 * against (surface_class × text_role) projection cross-products.
 *
 * All functions are pure. No allocations.
 *
 * D35 (SPOXIS_CONSTITUTION.md Perception Axiom) requires robust perceptual
 * distinction along ≥2 of three opponent-process channels (luminance, red-
 * green chroma, blue-yellow chroma). WCAG 2.1 contrast is a luminance-only
 * check — necessary but insufficient. Layer 2 adds the chromatic channels,
 * using OKLab as the perceptually-uniform substrate (Björn Ottosson 2020;
 * the lightness axis approximates luminance perception; the a/b axes
 * approximate R-G and B-Y opponent channels).
 */
#ifndef SUHC_COLOR_MATH_H
#define SUHC_COLOR_MATH_H

#include <stdbool.h>

/* ─── Layer 1: WCAG (inherited, unchanged) ─────────────────────────── */

/* Parse a hex color string ("#7C3AED", "#fff", "rgba(124,58,237,0.5)")
 * into 0–255 RGB channels. Returns false on parse failure.
 * Alpha is ignored (WCAG contrast is alpha-on-alpha undefined). */
bool color_parse(const char *s, int *r, int *g, int *b);

/* sRGB channel value [0,255] → linearized [0,1] per WCAG 2.1 §1.4.3. */
double color_srgb_to_linear(int channel_0_255);

/* Relative luminance per WCAG 2.1: 0.2126·R + 0.7152·G + 0.0722·B
 * after sRGB linearization. Inputs are 0–255. */
double color_relative_luminance(int r, int g, int b);

/* WCAG contrast ratio between two colors expressed as hex/rgba strings.
 * Returns -1.0 if either color fails to parse. Range: 1.0 (no contrast)
 * to 21.0 (max). AA threshold = 4.5 for normal text. */
double color_contrast_ratio(const char *fg, const char *bg);

/* Convenience: returns true iff contrast >= threshold (typically 4.5). */
bool color_passes_aa(const char *fg, const char *bg, double threshold);

/* ─── Layer 2: OKLab / OKLCh (D35, added 2026-04-22) ───────────────── */

/* OKLab (Björn Ottosson's perceptually uniform space):
 *   L  — lightness    [0, 1]     (approximates luminance channel)
 *   a  — green↔red    [-0.5, 0.5] (approximates R-G opponent channel)
 *   b  — blue↔yellow  [-0.5, 0.5] (approximates B-Y opponent channel)
 */
typedef struct {
    double L;
    double a;
    double b;
} OkLab;

/* OKLCh (polar form of OKLab):
 *   L  — lightness    [0, 1]          (same as OKLab.L)
 *   C  — chroma       [0, ~0.4]       (distance from neutral axis)
 *   h  — hue angle    [0, 360) degrees (atan2(b, a))
 * Gray colors have C ≈ 0 and an undefined / unstable h.
 */
typedef struct {
    double L;
    double C;
    double h;
} OkLCh;

/* Convert a parsed 0–255 sRGB triplet to OKLab.
 * The sRGB → linear → LMS → OKLab pipeline is exact (no iteration).
 * Reference: https://bottosson.github.io/posts/oklab/ */
OkLab color_srgb_to_oklab(int r, int g, int b);

/* Convert OKLab to OKLCh (polar). */
OkLCh color_oklab_to_oklch(OkLab lab);

/* One-shot: parse string (hex/rgba) → OKLCh. Returns {0,0,0} on parse failure
 * and sets *ok to false. */
OkLCh color_parse_to_oklch(const char *s, bool *ok);

/* Absolute hue delta in degrees, wrapped to [0, 180].
 * Undefined for near-gray pairs (caller must check chroma first). */
double color_hue_delta_deg(double h1, double h2);

/* ─── D35 two-channel predicate ───────────────────────────────────── */

/* The D35 two-channel rule (SPOXIS_CONSTITUTION.md D35):
 *
 *   passes  iff  dL ≥ 0.40
 *           OR   (dL ≥ 0.30 AND dC ≥ 0.10 AND dH ≥ 30°)
 *           OR   a non-chromatic cue is declared for the pair
 *                (the last clause is evaluated by exhaustcheck.c,
 *                not by this function).
 *
 * Returns true iff the (fg, bg) pair satisfies the chromatic two-channel
 * rule. The caller is responsible for the non-chromatic-cue case.
 *
 * Rationale: single-channel distinctions (luminance alone, chroma alone)
 * are D9-fragile — dark mode inverts luminance; ~8% of men with CVD
 * collapse red-green chroma; sunlight washes all chroma. A pair with
 * delta on only one channel fails under adversarial channel degradation.
 * Robust identity requires either (a) very large luminance delta or
 * (b) moderate delta on ≥2 channels. */
bool color_passes_two_channel(const char *fg, const char *bg);

/* Diagnostic variant: returns the deltas alongside the verdict. */
typedef struct {
    double dL;       /* OKLCh ΔL (|fg.L - bg.L|) */
    double dC;       /* OKLCh ΔC (|fg.C - bg.C|) */
    double dH_deg;   /* OKLCh hue delta in degrees, [0, 180] */
    bool   passes;   /* Two-channel rule verdict */
    bool   fg_parse_ok;
    bool   bg_parse_ok;
} TwoChannelResult;

TwoChannelResult color_eval_two_channel(const char *fg, const char *bg);

/* D35 thresholds (exposed so tests and diagnostic output can reference them) */
#define D35_STRONG_LUMINANCE_DELTA 0.40
#define D35_MODERATE_LUMINANCE_DELTA 0.30
#define D35_MIN_CHROMA_DELTA 0.10
#define D35_MIN_HUE_DELTA_DEG 30.0

#endif
