/*
 * suhc — color_math.h
 * WCAG-compliant color math: sRGB → relative luminance → contrast ratio.
 *
 * Used by exhaustcheck.c when evaluating axiom_contrast_aa predicates
 * against (surface_class × text_role) projection cross-products.
 *
 * All functions are pure. No allocations.
 */
#ifndef SUHC_COLOR_MATH_H
#define SUHC_COLOR_MATH_H

#include <stdbool.h>

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

#endif
