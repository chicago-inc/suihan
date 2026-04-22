/*
 * suhc — color_math.c
 *
 * Layer 1 (WCAG, inherited): sRGB → relative luminance → contrast ratio.
 * Layer 2 (D35, 2026-04-22):  sRGB → OKLab → OKLCh → two-channel rule.
 *
 * See include/color_math.h for API notes and D35 rationale.
 * Pure functions. No allocations.
 */

#include "color_math.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Layer 1: WCAG (unchanged) ────────────────────────────────────── */

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool color_parse(const char *s, int *r, int *g, int *b) {
    if (!s || !r || !g || !b) return false;
    while (*s && isspace((unsigned char)*s)) s++;

    if (*s == '#') {
        s++;
        size_t len = strlen(s);
        if (len >= 6) {
            int h0 = hex_digit(s[0]), h1 = hex_digit(s[1]);
            int h2 = hex_digit(s[2]), h3 = hex_digit(s[3]);
            int h4 = hex_digit(s[4]), h5 = hex_digit(s[5]);
            if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0 || h5 < 0) return false;
            *r = (h0 << 4) | h1;
            *g = (h2 << 4) | h3;
            *b = (h4 << 4) | h5;
            return true;
        }
        if (len >= 3) {
            int h0 = hex_digit(s[0]), h1 = hex_digit(s[1]), h2 = hex_digit(s[2]);
            if (h0 < 0 || h1 < 0 || h2 < 0) return false;
            *r = (h0 << 4) | h0;
            *g = (h1 << 4) | h1;
            *b = (h2 << 4) | h2;
            return true;
        }
        return false;
    }

    if (strncasecmp(s, "rgb", 3) == 0) {
        const char *open = strchr(s, '(');
        if (!open) return false;
        int rr, gg, bb;
        float aa;
        int matched = sscanf(open + 1, "%d,%d,%d,%f", &rr, &gg, &bb, &aa);
        if (matched < 3) {
            matched = sscanf(open + 1, "%d , %d , %d , %f", &rr, &gg, &bb, &aa);
        }
        if (matched < 3) return false;
        if (rr < 0 || rr > 255 || gg < 0 || gg > 255 || bb < 0 || bb > 255) return false;
        *r = rr; *g = gg; *b = bb;
        return true;
    }

    return false;
}

double color_srgb_to_linear(int c) {
    if (c < 0) c = 0;
    if (c > 255) c = 255;
    double v = c / 255.0;
    if (v <= 0.03928) return v / 12.92;
    return pow((v + 0.055) / 1.055, 2.4);
}

double color_relative_luminance(int r, int g, int b) {
    double rl = color_srgb_to_linear(r);
    double gl = color_srgb_to_linear(g);
    double bl = color_srgb_to_linear(b);
    return 0.2126 * rl + 0.7152 * gl + 0.0722 * bl;
}

double color_contrast_ratio(const char *fg, const char *bg) {
    int fr, fg_, fb, br, bg_, bb;
    if (!color_parse(fg, &fr, &fg_, &fb)) return -1.0;
    if (!color_parse(bg, &br, &bg_, &bb)) return -1.0;
    double l1 = color_relative_luminance(fr, fg_, fb);
    double l2 = color_relative_luminance(br, bg_, bb);
    double lighter = l1 > l2 ? l1 : l2;
    double darker  = l1 > l2 ? l2 : l1;
    return (lighter + 0.05) / (darker + 0.05);
}

bool color_passes_aa(const char *fg, const char *bg, double threshold) {
    double r = color_contrast_ratio(fg, bg);
    return r >= threshold;
}

/* ─── Layer 2: OKLab / OKLCh (D35) ─────────────────────────────────── */

/*
 * Ottosson's OKLab: https://bottosson.github.io/posts/oklab/
 *
 * Pipeline:
 *   sRGB_0_255  → sRGB_linear  (gamma decode, same as WCAG)
 *              → LMS_linear    (3x3 matrix M1)
 *              → LMS_cube_root (per-channel cbrt)
 *              → OKLab         (3x3 matrix M2)
 *
 * Matrices are the exact constants from Ottosson's post. Reproduced here
 * to keep suhc dependency-free; readers should verify against the source.
 */

static double srgb_linear_to_lms_l(double r, double g, double b) {
    return 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b;
}
static double srgb_linear_to_lms_m(double r, double g, double b) {
    return 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b;
}
static double srgb_linear_to_lms_s(double r, double g, double b) {
    return 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b;
}

OkLab color_srgb_to_oklab(int r, int g, int b) {
    double rl = color_srgb_to_linear(r);
    double gl = color_srgb_to_linear(g);
    double bl = color_srgb_to_linear(b);

    double l = srgb_linear_to_lms_l(rl, gl, bl);
    double m = srgb_linear_to_lms_m(rl, gl, bl);
    double s = srgb_linear_to_lms_s(rl, gl, bl);

    /* Nonlinear compression: cube root. OKLab uses cbrt rather than log
     * because cbrt(x) ≈ log(x) locally but is well-defined at zero. */
    double l_ = cbrt(l);
    double m_ = cbrt(m);
    double s_ = cbrt(s);

    OkLab out;
    out.L = 0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_;
    out.a = 1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_;
    out.b = 0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_;
    return out;
}

OkLCh color_oklab_to_oklch(OkLab lab) {
    OkLCh out;
    out.L = lab.L;
    out.C = sqrt(lab.a * lab.a + lab.b * lab.b);
    double h_rad = atan2(lab.b, lab.a);
    double h_deg = h_rad * (180.0 / M_PI);
    if (h_deg < 0.0) h_deg += 360.0;
    out.h = h_deg;
    return out;
}

OkLCh color_parse_to_oklch(const char *s, bool *ok) {
    OkLCh zero = {0.0, 0.0, 0.0};
    int r, g, b;
    if (!color_parse(s, &r, &g, &b)) {
        if (ok) *ok = false;
        return zero;
    }
    if (ok) *ok = true;
    return color_oklab_to_oklch(color_srgb_to_oklab(r, g, b));
}

double color_hue_delta_deg(double h1, double h2) {
    double d = fabs(h1 - h2);
    if (d > 180.0) d = 360.0 - d;
    return d;
}

/* ─── D35 two-channel predicate ───────────────────────────────────── */

TwoChannelResult color_eval_two_channel(const char *fg, const char *bg) {
    TwoChannelResult r;
    r.dL = 0.0; r.dC = 0.0; r.dH_deg = 0.0;
    r.passes = false; r.fg_parse_ok = false; r.bg_parse_ok = false;

    bool ok_fg = false, ok_bg = false;
    OkLCh f = color_parse_to_oklch(fg, &ok_fg);
    OkLCh b = color_parse_to_oklch(bg, &ok_bg);
    r.fg_parse_ok = ok_fg;
    r.bg_parse_ok = ok_bg;
    if (!ok_fg || !ok_bg) return r;

    r.dL = fabs(f.L - b.L);
    r.dC = fabs(f.C - b.C);

    /* Hue delta is unstable for near-gray pairs (C ≈ 0). If either side
     * is effectively gray, treat hue delta as zero — the pair must then
     * pass on luminance alone (the strong-L clause) or fail. */
    bool fg_chromatic = f.C >= 0.02;
    bool bg_chromatic = b.C >= 0.02;
    if (fg_chromatic && bg_chromatic) {
        r.dH_deg = color_hue_delta_deg(f.h, b.h);
    } else {
        r.dH_deg = 0.0;
    }

    /* D35 two-channel rule */
    bool strong_luminance = r.dL >= D35_STRONG_LUMINANCE_DELTA;
    bool moderate_luminance_plus_chroma =
        r.dL >= D35_MODERATE_LUMINANCE_DELTA &&
        r.dC >= D35_MIN_CHROMA_DELTA &&
        r.dH_deg >= D35_MIN_HUE_DELTA_DEG;

    r.passes = strong_luminance || moderate_luminance_plus_chroma;
    return r;
}

bool color_passes_two_channel(const char *fg, const char *bg) {
    return color_eval_two_channel(fg, bg).passes;
}
