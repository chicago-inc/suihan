/*
 * suhc — color_math.c
 * WCAG 2.1 sRGB color math. Pure functions. No allocations.
 */

#include "color_math.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

/* Parse one hex digit. Returns 0–15 or -1 on failure. */
static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool color_parse(const char *s, int *r, int *g, int *b) {
    if (!s || !r || !g || !b) return false;
    while (*s && isspace((unsigned char)*s)) s++;

    /* "#RRGGBB" or "#RGB" */
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

    /* "rgb(r,g,b)" or "rgba(r,g,b,a)" — alpha discarded */
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
