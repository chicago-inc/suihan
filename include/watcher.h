/*
 * suhc — watcher.h
 * File watcher for .szh files.
 *
 * Uses inotify on Linux. Falls back to poll-based watching
 * (1s interval) if inotify is unavailable.
 *
 * Readiness-to-hand: the developer thinks about the ordbok,
 * not the compiler. The watcher makes suhc withdraw from
 * consciousness.
 */

#ifndef SUHC_WATCHER_H
#define SUHC_WATCHER_H

#include "emitter.h"

/* Watch a directory for .szh changes and recompile on change.
 * This function runs forever (or until interrupted).
 *
 * dir:        directory to watch
 * ordbok_dir: optional ordbok search path (NULL if not needed)
 * target:     emission target (TARGET_NONE for check-only)
 *
 * Returns 0 on clean exit, 1 on error. */
int watcher_run(const char *dir, const char *ordbok_dir, EmitTarget target);

#endif /* SUHC_WATCHER_H */
