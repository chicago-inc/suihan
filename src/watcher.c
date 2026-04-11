/*
 * suhc — watcher.c
 * File watcher for .szh files.
 *
 * Uses inotify on Linux for efficient event-driven watching.
 * Falls back to poll-based (stat()) if inotify fails.
 */

#include "watcher.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "resolve.h"
#include "kindcheck.h"
#include "perpcheck.h"
#include "bloatlint.h"
#include "decidability.h"
#include "exhaustcheck.h"
#include "convergence.h"
#include "diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/inotify.h>
#include <poll.h>
#define HAS_INOTIFY 1
#else
#define HAS_INOTIFY 0
#endif

/* Forward declaration for poll fallback */
static int watcher_run_poll(const char *dir, const char *ordbok_dir,
                             EmitTarget target);

/* ------------------------------------------------------------ */
/* Shared compile function                                       */
/* ------------------------------------------------------------ */

/* Read file to string. Caller frees. */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    size_t nread = fread(buf, 1, sz, f);
    (void)nread;
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static volatile int should_stop = 0;
static void handle_sigint(int sig) {
    (void)sig;
    should_stop = 1;
}

static void compile_file(const char *path, const char *ordbok_dir,
                          EmitTarget target) {
    /* Get base directory from file path */
    char base_dir[512];
    snprintf(base_dir, sizeof(base_dir), "%s", path);
    char *last_slash = strrchr(base_dir, '/');
    if (last_slash) *last_slash = '\0';
    else strcpy(base_dir, ".");

    char *source = read_file(path);
    if (!source) {
        fprintf(stderr, "  [watch] cannot read %s\n", path);
        return;
    }

    DiagList *diags = diag_list_new();
    Lexer lex;
    lexer_init(&lex, source);
    Parser parser;
    parser_init(&parser, &lex, diags);
    Program *prog = parser_parse(&parser, path);

    Resolver resolver;
    resolver_init(&resolver, ordbok_dir ? ordbok_dir : base_dir);
    resolve_imports(prog, &resolver, diags);

    kindcheck(prog, diags);
    perpcheck(prog, diags);
    bloatlint(prog, diags);
    decidability_check(prog, diags);
    ExhaustReport exhaust = exhaustcheck(prog, diags);
    (void)exhaust;

    diag_print_all(diags);

    /* Emit if target specified and no errors */
    if (target != TARGET_NONE && !diag_has_errors(diags)) {
        /* Derive output path */
        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s", path);
        size_t plen = strlen(out_path);
        if (plen > 4 && strcmp(out_path + plen - 4, ".szh") == 0) {
            out_path[plen - 4] = '\0';
        }
        if (target == TARGET_TYPESCRIPT) strcat(out_path, ".ts");
        else strcat(out_path, ".sql");

        FILE *out = fopen(out_path, "w");
        if (out) {
            if (target == TARGET_TYPESCRIPT) {
                emit_typescript(prog, out, diags);
            } else {
                emit_sql(prog, out, diags);
            }
            fclose(out);
            printf("  [watch] emitted → %s\n", out_path);
        }
    }

    printf("  [watch] %s: %d error%s, %d warning%s\n",
           path, diags->error_count, diags->error_count == 1 ? "" : "s",
           diags->warning_count, diags->warning_count == 1 ? "" : "s");

    resolver_free(&resolver);
    parser_free(&parser);
    program_free(prog);
    diag_list_free(diags);
    free(source);
}

/* ------------------------------------------------------------ */
/* inotify-based watcher                                         */
/* ------------------------------------------------------------ */

#if HAS_INOTIFY

int watcher_run(const char *dir, const char *ordbok_dir, EmitTarget target) {
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "suhc: inotify_init failed, falling back to poll\n");
        goto poll_fallback;
    }

    int wd = inotify_add_watch(fd, dir, IN_MODIFY | IN_CREATE | IN_CLOSE_WRITE);
    if (wd < 0) {
        fprintf(stderr, "suhc: cannot watch '%s'\n", dir);
        close(fd);
        return 1;
    }

    signal(SIGINT, handle_sigint);
    printf("suhc: watching %s for .szh changes (Ctrl+C to stop)\n", dir);

    char buf[4096];
    while (!should_stop) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 500); /* 500ms timeout */
        if (ret <= 0) continue;

        ssize_t len = read(fd, buf, sizeof(buf));
        if (len <= 0) continue;

        /* Process events */
        char *ptr = buf;
        while (ptr < buf + len) {
            struct inotify_event *event = (struct inotify_event *)ptr;
            if (event->len > 0 && event->name) {
                size_t nlen = strlen(event->name);
                if (nlen > 4 && strcmp(event->name + nlen - 4, ".szh") == 0) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s", dir, event->name);
                    printf("\n  [watch] changed: %s\n", event->name);
                    compile_file(path, ordbok_dir, target);
                }
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    printf("\nsuhc: watcher stopped\n");
    return 0;

poll_fallback:
    ;
    /* Fall through to poll-based implementation */
    return watcher_run_poll(dir, ordbok_dir, target);
}

#endif /* HAS_INOTIFY */

/* ------------------------------------------------------------ */
/* Poll-based watcher (fallback)                                 */
/* ------------------------------------------------------------ */

typedef struct {
    char   name[256];
    time_t mtime;
} FileEntry;

static int watcher_run_poll(const char *dir, const char *ordbok_dir,
                             EmitTarget target) {
    signal(SIGINT, handle_sigint);
    printf("suhc: watching %s for .szh changes [poll mode] (Ctrl+C to stop)\n", dir);

    FileEntry files[128];
    int n_files = 0;

    /* Initial scan */
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "suhc: cannot open '%s'\n", dir);
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n_files < 128) {
        size_t nlen = strlen(ent->d_name);
        if (nlen < 5 || strcmp(ent->d_name + nlen - 4, ".szh") != 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(path, &st) == 0) {
            strncpy(files[n_files].name, ent->d_name, 256);
            files[n_files].mtime = st.st_mtime;
            n_files++;
        }
    }
    closedir(d);

    while (!should_stop) {
        usleep(1000000); /* 1 second */

        for (int i = 0; i < n_files; i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", dir, files[i].name);
            struct stat st;
            if (stat(path, &st) == 0 && st.st_mtime != files[i].mtime) {
                files[i].mtime = st.st_mtime;
                printf("\n  [watch] changed: %s\n", files[i].name);
                compile_file(path, ordbok_dir, target);
            }
        }

        /* Check for new files */
        d = opendir(dir);
        if (!d) continue;
        while ((ent = readdir(d)) != NULL) {
            size_t nlen = strlen(ent->d_name);
            if (nlen < 5 || strcmp(ent->d_name + nlen - 4, ".szh") != 0) continue;

            bool found = false;
            for (int i = 0; i < n_files; i++) {
                if (strcmp(files[i].name, ent->d_name) == 0) { found = true; break; }
            }
            if (!found && n_files < 128) {
                char path[512];
                snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
                struct stat st;
                if (stat(path, &st) == 0) {
                    strncpy(files[n_files].name, ent->d_name, 256);
                    files[n_files].mtime = st.st_mtime;
                    n_files++;
                    printf("\n  [watch] new file: %s\n", ent->d_name);
                    compile_file(path, ordbok_dir, target);
                }
            }
        }
        closedir(d);
    }

    printf("\nsuhc: watcher stopped\n");
    return 0;
}

#if !HAS_INOTIFY
int watcher_run(const char *dir, const char *ordbok_dir, EmitTarget target) {
    return watcher_run_poll(dir, ordbok_dir, target);
}
#endif
