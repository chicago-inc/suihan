/*
 * suhc — graph.h
 * Dependency graph emission for ordbok files.
 *
 * Sprint 4B: `suhc --graph <ordbok_dir>` outputs a Mermaid diagram
 * showing the import graph and declaration cross-references.
 *
 * Constitutional grounding:
 *   Yoneda: A unit is fully determined by its morphisms.
 *   The dependency graph visualizes those morphisms across
 *   ordbok files — making the import structure observable.
 *   D14: Each edge is a governed traversal path.
 */

#ifndef SUHC_GRAPH_H
#define SUHC_GRAPH_H

#include <stdio.h>

/* Generate a Mermaid dependency graph for all .szh files in dir.
 * Outputs Mermaid markdown to the given FILE stream.
 * Returns 0 on success, non-zero on failure. */
int graph_emit(const char *ordbok_dir, const char *search_dir, FILE *out);

#endif /* SUHC_GRAPH_H */
