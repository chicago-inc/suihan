/*
 * suhc — compat.h
 * Portability shims for POSIX functions missing on Windows/MinGW.
 */
#ifndef SUHC_COMPAT_H
#define SUHC_COMPAT_H

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* strndup: POSIX but not in C11 or MinGW */
#if defined(_WIN32) || defined(__MINGW32__)

static inline char *suhc_strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    if (len > n) len = n;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
#define strndup suhc_strndup

/* strcasestr: GNU extension, not available on Windows */
static inline const char *suhc_strcasestr(const char *haystack, const char *needle) {
    if (!needle[0]) return haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            size_t i;
            for (i = 1; i < nlen; i++) {
                if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
                    break;
            }
            if (i == nlen) return haystack;
        }
    }
    return NULL;
}
#define strcasestr suhc_strcasestr

#endif /* _WIN32 || __MINGW32__ */
#endif /* SUHC_COMPAT_H */
