/*
 * suhc — emit_asm.c
 * x86_64 NASM assembly generation from .szh declarations.
 *
 * Emission rules — the kind system maps to hardware:
 *   ξ (KIND_XI)     → .rodata section (immutable, ROM-safe)
 *   meihua          → pure function (register-only, no memory side effects)
 *   zhulin          → control flow (jmp/call/cmp, pattern match → branch)
 *   songqiao        → .data section (mutable runtime configuration)
 *   dimension       → .rodata enum constants
 *   projection      → branch table (cmp/je cascade)
 *
 * Target: x86_64, NASM syntax, freestanding (no libc).
 * Output is a .asm file that assembles with: nasm -f elf64 -o out.o
 *
 * M8 milestone: the compiler targets bare metal.
 */

#include "emitter.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ------------------------------------------------------------ */
/* String utilities                                              */
/* ------------------------------------------------------------ */

static char *to_label(const char *name) {
    if (!name) return strdup("_unknown");
    size_t len = strlen(name);
    char *out = malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        out[i] = (name[i] == '-' || name[i] == ' ') ? '_' : name[i];
    }
    out[len] = '\0';
    return out;
}

/* ------------------------------------------------------------ */
/* Expression → assembly                                         */
/* ------------------------------------------------------------ */

/* Forward declarations */
static void emit_asm_expr(FILE *out, Expr *e);
static void emit_asm_expr_to_rax(FILE *out, Expr *e);

/*
 * Emit an expression that leaves its result in rax.
 * For bare-metal kernel code, we support:
 *   - number literals → mov rax, imm
 *   - string literals → mov rax, addr (pointer to .rodata)
 *   - identifiers → call label
 *   - binary ops → rax = left op right
 *   - function calls → push args, call, result in rax
 *   - if/else → cmp + conditional jump
 *   - match → cascading cmp/je
 */
static void emit_asm_expr_to_rax(FILE *out, Expr *e) {
    if (!e) {
        fprintf(out, "    xor rax, rax            ; null expression\n");
        return;
    }

    switch (e->type) {
    case EXPR_NUMBER:
        if (e->as.number.text) {
            fprintf(out, "    mov rax, %s\n", e->as.number.text);
        } else {
            fprintf(out, "    xor rax, rax\n");
        }
        break;

    case EXPR_STRING: {
        /* String goes to .rodata, reference via label */
        static int str_counter = 0;
        int id = str_counter++;
        fprintf(out, "    mov rax, __str_%d       ; \"%s\"\n",
                id, e->as.string.value ? e->as.string.value : "");
        break;
    }

    case EXPR_IDENT:
        if (e->as.ident.name) {
            fprintf(out, "    call %s\n", e->as.ident.name);
            /* result already in rax by calling convention */
        }
        break;

    case EXPR_CALL: {
        if (!e->as.call.callee) break;
        /* Push args in reverse order (cdecl-like for simplicity) */
        /* For bare metal: first 6 args in rdi,rsi,rdx,rcx,r8,r9 */
        const char *arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        size_t nargs = e->as.call.arg_count;
        if (nargs > 6) nargs = 6;  /* cap at register args for now */

        for (size_t i = 0; i < nargs; i++) {
            emit_asm_expr_to_rax(out, e->as.call.args[i]);
            fprintf(out, "    mov %s, rax\n", arg_regs[i]);
        }
        fprintf(out, "    call %s\n", e->as.call.callee);
        break;
    }

    case EXPR_BINARY: {
        /* Evaluate left → rax, push, evaluate right → rax, pop rbx, operate */
        emit_asm_expr_to_rax(out, e->as.binary.left);
        fprintf(out, "    push rax\n");
        emit_asm_expr_to_rax(out, e->as.binary.right);
        fprintf(out, "    mov rbx, rax\n");
        fprintf(out, "    pop rax\n");
        if (!e->as.binary.op) break;
        if (strcmp(e->as.binary.op, "+") == 0) {
            fprintf(out, "    add rax, rbx\n");
        } else if (strcmp(e->as.binary.op, "-") == 0) {
            fprintf(out, "    sub rax, rbx\n");
        } else if (strcmp(e->as.binary.op, "*") == 0) {
            fprintf(out, "    imul rax, rbx\n");
        } else if (strcmp(e->as.binary.op, "/") == 0) {
            fprintf(out, "    xor rdx, rdx\n");
            fprintf(out, "    idiv rbx\n");
        } else if (strcmp(e->as.binary.op, "==") == 0) {
            fprintf(out, "    cmp rax, rbx\n");
            fprintf(out, "    sete al\n");
            fprintf(out, "    movzx rax, al\n");
        } else if (strcmp(e->as.binary.op, "!=") == 0) {
            fprintf(out, "    cmp rax, rbx\n");
            fprintf(out, "    setne al\n");
            fprintf(out, "    movzx rax, al\n");
        } else {
            fprintf(out, "    ; unsupported op: %s\n", e->as.binary.op);
        }
        break;
    }

    case EXPR_IF: {
        static int if_counter = 0;
        int id = if_counter++;
        emit_asm_expr_to_rax(out, e->as.if_expr.condition);
        fprintf(out, "    test rax, rax\n");
        fprintf(out, "    jz .else_%d\n", id);
        emit_asm_expr_to_rax(out, e->as.if_expr.then_branch);
        fprintf(out, "    jmp .endif_%d\n", id);
        fprintf(out, ".else_%d:\n", id);
        emit_asm_expr_to_rax(out, e->as.if_expr.else_branch);
        fprintf(out, ".endif_%d:\n", id);
        break;
    }

    case EXPR_MATCH: {
        static int match_counter = 0;
        int mid = match_counter++;
        /* Evaluate discriminant → rax, then cascade cmp */
        emit_asm_expr_to_rax(out, e->as.match_expr.discriminant);
        fprintf(out, "    mov rcx, rax            ; match discriminant\n");
        for (size_t i = 0; i < e->as.match_expr.arm_count; i++) {
            Expr *pat = e->as.match_expr.arms[i].pattern;
            if (pat && pat->type == EXPR_WILDCARD) {
                /* Default arm */
                emit_asm_expr_to_rax(out, e->as.match_expr.arms[i].body);
                fprintf(out, "    jmp .match_end_%d\n", mid);
            } else if (pat && pat->type == EXPR_NUMBER) {
                fprintf(out, "    cmp rcx, %s\n",
                        pat->as.number.text ? pat->as.number.text : "0");
                fprintf(out, "    jne .match_arm_%d_%zu\n", mid, i + 1);
                emit_asm_expr_to_rax(out, e->as.match_expr.arms[i].body);
                fprintf(out, "    jmp .match_end_%d\n", mid);
                fprintf(out, ".match_arm_%d_%zu:\n", mid, i + 1);
            } else if (pat && pat->type == EXPR_IDENT) {
                /* Enum member match — compare against constant */
                fprintf(out, "    cmp rcx, %s\n",
                        pat->as.ident.name ? pat->as.ident.name : "0");
                fprintf(out, "    jne .match_arm_%d_%zu\n", mid, i + 1);
                emit_asm_expr_to_rax(out, e->as.match_expr.arms[i].body);
                fprintf(out, "    jmp .match_end_%d\n", mid);
                fprintf(out, ".match_arm_%d_%zu:\n", mid, i + 1);
            }
        }
        fprintf(out, ".match_end_%d:\n", mid);
        break;
    }

    case EXPR_BLOCK: {
        /* Execute each statement, last one's result stays in rax */
        for (size_t i = 0; i < e->as.block.count; i++) {
            emit_asm_expr_to_rax(out, e->as.block.stmts[i]);
        }
        break;
    }

    default:
        fprintf(out, "    xor rax, rax            ; unhandled expr type %d\n",
                e->type);
        break;
    }
}

/* Emit an expression as a statement (result discarded) */
static void emit_asm_expr(FILE *out, Expr *e) {
    emit_asm_expr_to_rax(out, e);
}

/* ------------------------------------------------------------ */
/* String literal collection — two-pass for .rodata              */
/* ------------------------------------------------------------ */

typedef struct {
    const char *value;
    int         id;
} StrLiteral;

#define MAX_STRINGS 256
static StrLiteral str_table[MAX_STRINGS];
static int str_table_count = 0;

static void collect_strings_expr(Expr *e) {
    if (!e) return;
    switch (e->type) {
    case EXPR_STRING:
        if (str_table_count < MAX_STRINGS && e->as.string.value) {
            str_table[str_table_count].value = e->as.string.value;
            str_table[str_table_count].id = str_table_count;
            str_table_count++;
        }
        break;
    case EXPR_CALL:
        for (size_t i = 0; i < e->as.call.arg_count; i++)
            collect_strings_expr(e->as.call.args[i]);
        break;
    case EXPR_BINARY:
        collect_strings_expr(e->as.binary.left);
        collect_strings_expr(e->as.binary.right);
        break;
    case EXPR_BLOCK:
        for (size_t i = 0; i < e->as.block.count; i++)
            collect_strings_expr(e->as.block.stmts[i]);
        break;
    case EXPR_IF:
        collect_strings_expr(e->as.if_expr.condition);
        collect_strings_expr(e->as.if_expr.then_branch);
        collect_strings_expr(e->as.if_expr.else_branch);
        break;
    case EXPR_MATCH:
        collect_strings_expr(e->as.match_expr.discriminant);
        for (size_t i = 0; i < e->as.match_expr.arm_count; i++)
            collect_strings_expr(e->as.match_expr.arms[i].body);
        break;
    default:
        break;
    }
}

static void collect_strings(Program *prog) {
    str_table_count = 0;
    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type == DECL_KINDED_VALUE && d->as.kinded.value) {
            collect_strings_expr(d->as.kinded.value);
        }
        if (d->type == DECL_MEIHUA || d->type == DECL_ZHULIN ||
            d->type == DECL_SONGQIAO) {
            collect_strings_expr(d->as.exec_layer.body);
        }
    }
}

/* ------------------------------------------------------------ */
/* Per-declaration emitters                                       */
/* ------------------------------------------------------------ */

/* ξ → .rodata constant */
static void emit_asm_kinded_value(FILE *out, Decl *d) {
    if (!d->name.text || !d->as.kinded.value) return;
    char *label = to_label(d->name.text);
    Expr *v = d->as.kinded.value;

    if (v->type == EXPR_STRING && v->as.string.value) {
        fprintf(out, "\n; ξ %s — identity (immutable)\n", d->name.text);
        fprintf(out, "%s:  db \"%s\", 0\n", label, v->as.string.value);
    } else if (v->type == EXPR_NUMBER && v->as.number.text) {
        fprintf(out, "\n; ξ %s — identity (immutable)\n", d->name.text);
        fprintf(out, "%s:  equ %s\n", label, v->as.number.text);
    }
    free(label);
}

/* dimension → enum constants */
static void emit_asm_dimension(FILE *out, Decl *d) {
    if (!d->name.text) return;
    Expr *m = d->as.dimension.members;
    if (!m || m->type != EXPR_ENUM) return;

    fprintf(out, "\n; dimension %s\n", d->name.text);
    for (size_t i = 0; i < m->as.enumeration.count; i++) {
        const char *member = m->as.enumeration.items[i].name.text;
        if (member) {
            char *label = to_label(member);
            fprintf(out, "%s:  equ %zu\n", label, i);
            free(label);
        }
    }
}

/* meihua → pure function (no side effects, result in rax) */
static void emit_asm_meihua(FILE *out, Decl *d) {
    if (!d->name.text) return;
    char *label = to_label(d->name.text);

    fprintf(out, "\n; meihua %s — pure computation\n", d->name.text);
    fprintf(out, "global %s\n", label);
    fprintf(out, "%s:\n", label);

    Expr *body = d->as.exec_layer.body;
    if (body && body->type == EXPR_BLOCK && body->as.block.count == 1)
        body = body->as.block.stmts[0];

    if (body) {
        emit_asm_expr_to_rax(out, body);
    }
    fprintf(out, "    ret\n");
    free(label);
}

/* zhulin → control flow (the kernel's main logic) */
static void emit_asm_zhulin(FILE *out, Decl *d) {
    if (!d->name.text) return;
    char *label = to_label(d->name.text);

    fprintf(out, "\n; zhulin %s — control flow\n", d->name.text);
    fprintf(out, "global %s\n", label);
    fprintf(out, "%s:\n", label);

    Expr *body = d->as.exec_layer.body;
    if (body) {
        if (body->type == EXPR_BLOCK) {
            for (size_t i = 0; i < body->as.block.count; i++) {
                emit_asm_expr(out, body->as.block.stmts[i]);
            }
        } else {
            emit_asm_expr(out, body);
        }
    }
    fprintf(out, "    ret\n");
    free(label);
}

/* songqiao → .data section (runtime configuration, mutable) */
static void emit_asm_songqiao(FILE *out, Decl *d) {
    if (!d->name.text) return;

    fprintf(out, "\n; songqiao %s — runtime configuration\n", d->name.text);

    Expr *body = d->as.exec_layer.body;
    if (body && body->type == EXPR_BLOCK && body->as.block.count == 1)
        body = body->as.block.stmts[0];

    if (body && body->type == EXPR_COMPOUND) {
        for (size_t i = 0; i < body->as.compound.count; i++) {
            const char *key = body->as.compound.keys[i].text;
            Expr *val = body->as.compound.values[i];
            if (!key) continue;

            char full_label[256];
            snprintf(full_label, sizeof(full_label), "%s_%s",
                     d->name.text, key);
            char *label = to_label(full_label);

            if (val && val->type == EXPR_NUMBER && val->as.number.text) {
                fprintf(out, "%s:  equ %s\n", label, val->as.number.text);
            } else if (val && val->type == EXPR_STRING && val->as.string.value) {
                fprintf(out, "%s:  db \"%s\", 0\n", label, val->as.string.value);
            }
            free(label);
        }
    }
}

/* projection → cmp/je cascade */
static void emit_asm_projection(FILE *out, Decl *d) {
    if (!d->name.text) return;
    char *label = to_label(d->name.text);

    fprintf(out, "\n; projection %s — ζ-computation\n", d->name.text);
    fprintf(out, "global %s\n", label);
    fprintf(out, "%s:\n", label);
    fprintf(out, "    ; input in rdi, output in rax\n");

    for (size_t i = 0; i < d->as.projection.arm_count; i++) {
        ProjectionArm *arm = &d->as.projection.arms[i];
        if (arm->pattern && arm->pattern->type == EXPR_WILDCARD) {
            /* Default arm */
            if (arm->body) emit_asm_expr_to_rax(out, arm->body);
            fprintf(out, "    ret\n");
        } else if (arm->pattern) {
            static int proj_arm_id = 0;
            int aid = proj_arm_id++;
            /* Compare rdi against pattern value */
            if (arm->pattern->type == EXPR_IDENT && arm->pattern->as.ident.name) {
                fprintf(out, "    cmp rdi, %s\n", arm->pattern->as.ident.name);
            } else if (arm->pattern->type == EXPR_NUMBER && arm->pattern->as.number.text) {
                fprintf(out, "    cmp rdi, %s\n", arm->pattern->as.number.text);
            }
            fprintf(out, "    jne .proj_skip_%d\n", aid);
            if (arm->body) emit_asm_expr_to_rax(out, arm->body);
            fprintf(out, "    ret\n");
            fprintf(out, ".proj_skip_%d:\n", aid);
        }
    }
    fprintf(out, "    xor rax, rax\n");
    fprintf(out, "    ret\n");
    free(label);
}

/* ------------------------------------------------------------ */
/* M8: Generated dispatch from ordbok                            */
/* ------------------------------------------------------------ */

#include "gen/emit_asm_dispatch.h"

/* ------------------------------------------------------------ */
/* Main emission entry point                                     */
/* ------------------------------------------------------------ */

int emit_asm(Program *prog, FILE *out, DiagList *diags) {
    (void)diags;

    /* Collect string literals for .rodata */
    collect_strings(prog);

    /* File header */
    fprintf(out, "; Generated by suhc (the suihan compiler)\n");
    fprintf(out, "; Target: x86_64, NASM syntax, freestanding\n");
    fprintf(out, "; Assemble: nasm -f elf64 -o kernel.o kernel.asm\n");
    fprintf(out, ";\n");
    fprintf(out, "; Kind → hardware mapping:\n");
    fprintf(out, ";   ξ  (identity)  → .rodata (immutable, ROM-safe)\n");
    fprintf(out, ";   ζ  (shape)     → computed at runtime (never stored)\n");
    fprintf(out, ";   x  (variable)  → .bss / registers (mutable)\n");
    fprintf(out, ";   R.k (operator) → .text (instruction stream)\n");
    fprintf(out, ";   ω  (output)    → framebuffer / port I/O (terminal)\n");
    fprintf(out, ";   ΔR.k (cast)    → documented, not emitted\n");
    fprintf(out, "\n");
    fprintf(out, "[bits 64]\n");
    fprintf(out, "\n");

    /* Emit .rodata section first (ξ values, strings, dimensions) */
    fprintf(out, "section .rodata\n");
    fprintf(out, "; === ξ (identity) — immutable constants ===\n");

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type == DECL_KINDED_VALUE) {
            emit_asm_kinded_value(out, d);
        }
        if (d->type == DECL_DIMENSION) {
            emit_asm_dimension(out, d);
        }
    }

    /* Emit string literals */
    if (str_table_count > 0) {
        fprintf(out, "\n; === string literals ===\n");
        for (int i = 0; i < str_table_count; i++) {
            fprintf(out, "__str_%d:  db \"%s\", 0\n",
                    str_table[i].id,
                    str_table[i].value ? str_table[i].value : "");
        }
    }

    /* Emit .data section (songqiao — runtime config) */
    fprintf(out, "\nsection .data\n");
    fprintf(out, "; === songqiao — runtime configuration ===\n");
    for (size_t i = 0; i < prog->count; i++) {
        if (prog->decls[i]->type == DECL_SONGQIAO) {
            emit_asm_songqiao(out, prog->decls[i]);
        }
    }

    /* Emit .bss section (runtime variables) */
    fprintf(out, "\nsection .bss\n");
    fprintf(out, "; === x (variable) — mutable runtime state ===\n");

    /* Emit .text section (meihua, zhulin, projections) */
    fprintf(out, "\nsection .text\n");
    fprintf(out, "; === R.k (operator) — instruction stream ===\n");

    for (size_t i = 0; i < prog->count; i++) {
        Decl *d = prog->decls[i];
        if (d->type == DECL_MEIHUA || d->type == DECL_ZHULIN ||
            d->type == DECL_PROJECTION) {
            emit_asm_dispatch(out, d);
        }
    }

    return 0;
}
