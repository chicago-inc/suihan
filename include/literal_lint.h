/*
 * suhc — literal_lint.h
 * Line-precise detection of banned design literals (hex, rgba, off-scale
 * magnitudes) in TypeScript files.
 *
 * Drives the `suhc --literal-check <dir>` mode that wires into make bootstrap
 * for SCR-181 BD #41 enforcement.
 */
#ifndef SUHC_LITERAL_LINT_H
#define SUHC_LITERAL_LINT_H

#include <stddef.h>

/* Scan a single file for banned literal patterns. Writes diagnostics to
 * stdout in `path:line:col: error: <message>` format. Returns number of
 * violations found. */
int literal_lint_file(const char *path);

/* Recursively scan a directory tree, skipping node_modules, .expo, build,
 * and the theme.ts source-of-truth file. Returns total violation count. */
int literal_lint_tree(const char *root);

#endif
