; kernel_entry.asm — Mongwu kernel v0.4.0
; Constitutional kernel with memory + scheduler + syscalls + user mode
;
; Memory layout:
;   0x000000 - 0x0FFFFF : Reserved (BIOS, VGA, boot structures)
;   0x100000 - 0x1FFFFF : Kernel code + data
;   0x200000 - 0x27FFFF : Page bitmap (512 KB)
;   0x280000 - 0x280FFF : Process table (16 PCBs × 160 bytes = 2560)
;   0x281000 - 0x281FFF : IDT (256 entries × 16 bytes = 4096)
;   0x290000+            : Process stacks (4 pages each)
;
; E820 memory map at 0x500 from bootloader

[bits 64]
[org 0x8000]

; ═══════════════════════════════════════════════════
; ξ (identity) — from ordbok, immutable
; ═══════════════════════════════════════════════════

PAGE_SIZE       equ 4096
BITMAP_START    equ 0x200000
BITMAP_SIZE     equ 0x80000
FREE_START      equ 0x280000
VGA_BASE        equ 0xB8000
VGA_WIDTH       equ 80
E820_COUNT      equ 0x500
E820_DATA       equ 0x504

; Process constants (from ordbok)
MAX_PROCS       equ 16
PCB_SIZE        equ 160
PCB_TABLE       equ 0x280000
IDT_BASE        equ 0x281000
STACK_BASE      equ 0x290000
STACK_PAGES     equ 4
STACK_SIZE      equ (STACK_PAGES * PAGE_SIZE)

; PCB offsets
PCB_PID         equ 0
PCB_STATE       equ 8
PCB_RSP         equ 16
PCB_RIP         equ 24
PCB_RAX         equ 32
PCB_RBX         equ 40
PCB_RCX         equ 48
PCB_RDX         equ 56
PCB_RSI         equ 64
PCB_RDI         equ 72
PCB_RBP         equ 80
PCB_R8          equ 88
PCB_R9          equ 96
PCB_R10         equ 104
PCB_R11         equ 112
PCB_R12         equ 120
PCB_R13         equ 128
PCB_R14         equ 136
PCB_R15         equ 144
PCB_FLAGS       equ 152

; Process states (dimension process_state)
STATE_RUNNING    equ 0
STATE_READY      equ 1
STATE_BLOCKED    equ 2
STATE_TERMINATED equ 3

; Colors
WHITE           equ 0x0F
GREY            equ 0x07
CYAN            equ 0x03
GREEN           equ 0x0A
YELLOW          equ 0x0E
RED             equ 0x0C

; TSS + User mode
TSS_BASE        equ 0x282000   ; Task State Segment
TSS_SIZE        equ 104        ; minimum TSS size
GDT_BASE        equ 0x7C00 + 91  ; GDT is in boot sector (gdt_start offset)
USER_CS         equ 0x28 | 3   ; user code segment, RPL=3
USER_DS         equ 0x30 | 3   ; user data segment, RPL=3
KERNEL_CS       equ 0x18       ; kernel code segment
KERNEL_DS       equ 0x20       ; kernel data segment

; Syscall constants (from ordbok)
SYS_WRITE       equ 0          ; write char to VGA (kernel mediates)
SYS_EXIT        equ 1          ; terminate process
SYS_YIELD       equ 2          ; voluntary yield
SYS_GETPID      equ 3          ; get own PID

; PIT / PIC constants
PIT_CMD         equ 0x43
PIT_CH0         equ 0x40
PIC1_CMD        equ 0x20
PIC1_DATA       equ 0x21
PIC2_CMD        equ 0xA0
PIC2_DATA       equ 0xA1
PIT_DIVISOR     equ 59659       ; 1193182 / 20 ≈ 50ms

; ═══════════════════════════════════════════════════
; zhulin kernel_main — entry point
; ═══════════════════════════════════════════════════

kernel_main:
    ; Clear screen
    mov rdi, VGA_BASE
    mov rcx, VGA_WIDTH * 25
    mov ax, 0x0F20
.clear:
    mov [rdi], ax
    add rdi, 2
    dec rcx
    jnz .clear

    ; Row 0: "Mongwu v0.3.0 — Process Scheduler"
    mov rdi, VGA_BASE
    mov rsi, str_banner
    mov ah, WHITE
    call print_string

    ; Row 1: songqiao
    mov rdi, VGA_BASE + (VGA_WIDTH * 2)
    mov rsi, str_machine
    mov ah, CYAN
    call print_string

    ; ═══════════════════════════════════════════════════
    ; Memory init (from Sprint 2)
    ; ═══════════════════════════════════════════════════

    ; E820 scan
    xor r12, r12
    mov rsi, E820_DATA
    xor rcx, rcx
    mov cx, [E820_COUNT]
    test rcx, rcx
    jz .default_mem
.e820_loop:
    mov eax, [rsi + 16]
    cmp eax, 1
    jne .e820_next
    mov rax, [rsi + 8]
    add r12, rax
.e820_next:
    add rsi, 20
    dec rcx
    jnz .e820_loop
.default_mem:
    test r12, r12
    jnz .mem_ok
    mov r12, 17179869184        ; 16 GB fallback
.mem_ok:
    mov [mem_total], r12

    ; Zero bitmap
    mov rdi, BITMAP_START
    xor rax, rax
    mov rcx, BITMAP_SIZE / 8
    rep stosq

    ; Mark reserved pages
    mov rdi, BITMAP_START
    mov rcx, (STACK_BASE + MAX_PROCS * STACK_SIZE) / PAGE_SIZE / 8
    mov al, 0xFF
    rep stosb

    ; Row 3: memory status
    mov rdi, VGA_BASE + (VGA_WIDTH * 4)
    mov rsi, str_mem_ok
    mov ah, GREEN
    call print_string

    ; ═══════════════════════════════════════════════════
    ; Process table init
    ; ═══════════════════════════════════════════════════

    ; Zero PCB table
    mov rdi, PCB_TABLE
    xor rax, rax
    mov rcx, (MAX_PROCS * PCB_SIZE) / 8
    rep stosq

    ; Init process 0 (kernel idle) — already running
    mov rdi, PCB_TABLE
    mov qword [rdi + PCB_PID], 0
    mov qword [rdi + PCB_STATE], STATE_RUNNING
    mov dword [process_count], 1
    mov dword [current_pid], 0

    ; Create process A
    mov rdi, proc_a_entry
    mov rsi, 1                  ; PID 1
    call process_create

    ; Create process B
    mov rdi, proc_b_entry
    mov rsi, 2                  ; PID 2
    call process_create

    ; Row 4: processes created
    mov rdi, VGA_BASE + (VGA_WIDTH * 6)
    mov rsi, str_procs
    mov ah, GREEN
    call print_string

    ; ═══════════════════════════════════════════════════
    ; Set up IDT + PIC + PIT
    ; ═══════════════════════════════════════════════════

    call setup_idt
    call setup_pic
    call setup_pit
    call setup_tss

    ; Row 5: scheduler + user mode active
    mov rdi, VGA_BASE + (VGA_WIDTH * 8)
    mov rsi, str_sched
    mov ah, GREEN
    call print_string

    ; Row 10: axiom
    mov rdi, VGA_BASE + (VGA_WIDTH * 22)
    mov rsi, str_axiom
    mov ah, WHITE
    call print_string

    ; Save kernel stack for process 0 so context switch can return here
    mov rdi, PCB_TABLE
    mov [rdi + PCB_RSP], rsp

    ; Enable interrupts — scheduler goes live
    sti

    ; Kernel idle loop (process 0)
    ; Timer interrupts will preempt from here
.idle:
    hlt
    jmp .idle

; ═══════════════════════════════════════════════════
; zhulin process_create
; rdi = entry point address, rsi = pid
; Allocates stack, initializes PCB, marks as ready
; ═══════════════════════════════════════════════════

process_create:
    push rbx
    push rcx

    ; Compute PCB address: PCB_TABLE + pid * PCB_SIZE
    mov rax, rsi
    mov rbx, PCB_SIZE
    imul rax, rbx
    add rax, PCB_TABLE
    mov rbx, rax               ; rbx = PCB address

    ; Set PID
    mov [rbx + PCB_PID], rsi

    ; Set state = READY
    mov qword [rbx + PCB_STATE], STATE_READY

    ; Compute stack top: STACK_BASE + (pid + 1) * STACK_SIZE
    mov rax, rsi
    inc rax
    mov rcx, STACK_SIZE
    imul rax, rcx
    add rax, STACK_BASE         ; stack top

    ; Set up initial stack frame for iretq
    ; The timer handler does: pop 15 regs, then iretq.
    ; iretq expects: [rsp] = RIP, [rsp+8] = CS, [rsp+16] = RFLAGS,
    ;                [rsp+24] = RSP, [rsp+32] = SS
    ; Before that, 15 general registers are popped.

    ; Start from stack top, push iretq frame for ring 3
    sub rax, 8
    mov qword [rax], USER_DS   ; SS (user data segment, RPL=3)
    sub rax, 8
    lea rcx, [rax + 48]        ; RSP will point past this frame
    mov [rax], rcx              ; RSP for the new process
    sub rax, 8
    mov qword [rax], 0x202     ; RFLAGS (IF=1)
    sub rax, 8
    mov qword [rax], USER_CS   ; CS (user code segment, RPL=3)
    sub rax, 8
    mov [rax], rdi              ; RIP = entry point

    ; Push 15 zero registers (r15..rax, matching pop order in timer_handler)
    mov rcx, 15
.zero_regs:
    sub rax, 8
    mov qword [rax], 0
    dec rcx
    jnz .zero_regs

    ; Save stack pointer
    mov [rbx + PCB_RSP], rax

    ; Save entry point
    mov [rbx + PCB_RIP], rdi

    ; Zero all saved registers
    xor rax, rax
    mov [rbx + PCB_RAX], rax
    mov [rbx + PCB_RBX], rax
    mov [rbx + PCB_RCX], rax
    mov [rbx + PCB_RDX], rax
    mov [rbx + PCB_RSI], rax
    mov [rbx + PCB_RDI], rax
    mov [rbx + PCB_RBP], rax

    ; Increment process count
    inc dword [process_count]

    pop rcx
    pop rbx
    ret

; ═══════════════════════════════════════════════════
; morphism context_switch — running → ready → running
; Timer interrupt handler (IRQ0 → vector 32)
; This is the scheduler's heartbeat
; ═══════════════════════════════════════════════════

timer_handler:
    ; Save current process registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Increment global tick counter
    inc qword [tick_count]

    ; Get current PCB
    xor rax, rax
    mov eax, [current_pid]
    mov rbx, PCB_SIZE
    imul rax, rbx
    add rax, PCB_TABLE          ; rax = current PCB

    ; Save RSP
    mov [rax + PCB_RSP], rsp

    ; Set current state = READY (morphism: running → ready)
    mov qword [rax + PCB_STATE], STATE_READY

    ; Find next READY process (round robin)
    xor ecx, ecx
    mov ecx, [current_pid]
.find_next:
    inc ecx
    cmp ecx, [process_count]
    jl .check_state
    xor ecx, ecx               ; wrap to 0
.check_state:
    ; Don't schedule back to same if others available
    ; Compute PCB address
    mov rax, rcx
    mov rbx, PCB_SIZE
    imul rax, rbx
    add rax, PCB_TABLE

    cmp qword [rax + PCB_STATE], STATE_READY
    je .found_next

    ; If we've checked all, stay on current (it's now READY so it'll match)
    cmp ecx, [current_pid]
    jne .find_next

.found_next:
    ; Set new current
    mov [current_pid], ecx

    ; Set new state = RUNNING (morphism: ready → running)
    mov qword [rax + PCB_STATE], STATE_RUNNING

    ; Restore RSP from new process
    mov rsp, [rax + PCB_RSP]

    ; Send EOI to PIC
    mov al, 0x20
    out PIC1_CMD, al

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq

; ═══════════════════════════════════════════════════
; setup_idt — Interrupt Descriptor Table
; ═══════════════════════════════════════════════════

setup_idt:
    ; Zero the entire IDT
    mov rdi, IDT_BASE
    xor rax, rax
    mov rcx, 4096 / 8
    rep stosq

    ; Install timer handler at vector 32 (IRQ0 after remap)
    mov rdi, IDT_BASE + (32 * 16)   ; vector 32, 16 bytes per entry
    mov rax, timer_handler

    ; IDT entry format (64-bit):
    ; [0:1]  offset low (bits 0-15)
    ; [2:3]  segment selector (0x18 = 64-bit code segment)
    ; [4]    IST (0)
    ; [5]    type_attr (0x8E = present, ring 0, interrupt gate)
    ; [6:7]  offset mid (bits 16-31)
    ; [8:11] offset high (bits 32-63)
    ; [12:15] reserved (0)

    mov word [rdi], ax              ; offset low
    mov word [rdi + 2], 0x18        ; code segment selector (64-bit)
    mov byte [rdi + 4], 0           ; IST = 0
    mov byte [rdi + 5], 0x8E        ; present, ring 0, interrupt gate
    shr rax, 16
    mov word [rdi + 6], ax          ; offset mid
    shr rax, 16
    mov dword [rdi + 8], eax        ; offset high
    mov dword [rdi + 12], 0         ; reserved

    ; Install syscall handler at vector 0x80 (INT 0x80)
    ; DPL=3 so user-mode code can invoke it (0xEE = present, ring 3, interrupt gate)
    mov rdi, IDT_BASE + (0x80 * 16)
    mov rax, syscall_handler

    mov word [rdi], ax
    mov word [rdi + 2], KERNEL_CS
    mov byte [rdi + 4], 0
    mov byte [rdi + 5], 0xEE        ; present, DPL=3, interrupt gate
    shr rax, 16
    mov word [rdi + 6], ax
    shr rax, 16
    mov dword [rdi + 8], eax
    mov dword [rdi + 12], 0

    ; Load IDT
    lea rax, [idt_descriptor]
    lidt [rax]

    ret

; ═══════════════════════════════════════════════════
; setup_pic — Remap PIC to vectors 32-47
; ═══════════════════════════════════════════════════

setup_pic:
    ; ICW1: start init
    mov al, 0x11
    out PIC1_CMD, al
    out PIC2_CMD, al

    ; ICW2: vector offset
    mov al, 32                  ; IRQ 0-7 → vectors 32-39
    out PIC1_DATA, al
    mov al, 40                  ; IRQ 8-15 → vectors 40-47
    out PIC2_DATA, al

    ; ICW3: cascade
    mov al, 4                   ; PIC1: slave on IRQ2
    out PIC1_DATA, al
    mov al, 2                   ; PIC2: cascade identity
    out PIC2_DATA, al

    ; ICW4: 8086 mode
    mov al, 0x01
    out PIC1_DATA, al
    out PIC2_DATA, al

    ; Mask all IRQs except IRQ0 (timer)
    mov al, 0xFE               ; bit 0 clear = IRQ0 enabled
    out PIC1_DATA, al
    mov al, 0xFF               ; mask all on PIC2
    out PIC2_DATA, al

    ret

; ═══════════════════════════════════════════════════
; setup_pit — Programmable Interval Timer at 50ms
; ═══════════════════════════════════════════════════

setup_pit:
    ; Channel 0, access mode lobyte/hibyte, mode 2 (rate generator)
    mov al, 0x34                ; 00 11 010 0
    out PIT_CMD, al

    ; Set divisor (59659 = ~50ms)
    mov ax, PIT_DIVISOR
    out PIT_CH0, al             ; low byte
    mov al, ah
    out PIT_CH0, al             ; high byte

    ret

; ═══════════════════════════════════════════════════
; setup_tss — Task State Segment for ring transitions
; When CPU transitions ring 3 → ring 0 (interrupt/syscall),
; it loads RSP0 from the TSS. This is the kernel stack.
; ═══════════════════════════════════════════════════

setup_tss:
    ; Zero TSS
    mov rdi, TSS_BASE
    xor rax, rax
    mov rcx, TSS_SIZE / 8 + 1
    rep stosq

    ; Set RSP0 (kernel stack for ring transitions)
    ; Use the main kernel stack at 0x90000
    mov rdi, TSS_BASE
    mov qword [rdi + 4], 0x90000   ; RSP0

    ; Set IOPB offset to TSS_SIZE (no I/O bitmap)
    mov word [rdi + 102], TSS_SIZE

    ; Patch the GDT TSS descriptor with the TSS base address
    ; GDT is at the boot sector. TSS descriptor is at offset 0x38.
    ; We need to find it — it's inside the boot sector memory.
    ; The GDT was loaded at boot time from the MBR at 0x7C00.
    ; gdt_start is at a known offset within the boot sector.
    ; For now, reload GDT with a kernel-space copy.

    ; Actually, simpler: just use LTR with the TSS selector
    ; The GDT TSS entry base fields need to be set to TSS_BASE.
    ; Since we know the GDT layout, patch it in memory.

    ; The GDT starts at 0x7C00 + offset_of(gdt_start)
    ; gdt_start = boot_start + some offset. Let's compute:
    ; boot sector at 0x7C00, gdt_start is labeled.
    ; TSS entry is at GDT + 0x38 (7th entry, 0-indexed: null,code32,data32,code64,data64,ucode64,udata64,tss)
    ; GDT TSS descriptor format:
    ;   [0:1] limit low
    ;   [2:3] base low (bits 0-15)
    ;   [4]   base mid (bits 16-23)
    ;   [5]   access
    ;   [6]   flags + limit high
    ;   [7]   base high (bits 24-31)
    ;   [8:11] base upper (bits 32-63)

    ; We need to find gdt_start in memory. It's right after disk_error in the MBR.
    ; Rather than guess, let's just create a new GDT in kernel space.
    ; Copy the boot GDT and patch the TSS entry.

    ; Read current GDTR
    sgdt [tmp_gdtr]
    mov rdi, [tmp_gdtr + 2]     ; GDT base address
    add rdi, 0x38               ; offset to TSS descriptor

    ; Patch base address of TSS
    mov eax, TSS_BASE
    mov word [rdi + 2], ax      ; base low (bits 0-15)
    shr eax, 16
    mov byte [rdi + 4], al      ; base mid (bits 16-23)
    mov byte [rdi + 7], ah      ; base high (bits 24-31)
    mov dword [rdi + 8], 0      ; base upper (bits 32-63, TSS is below 4GB)

    ; Load TSS
    mov ax, 0x38                ; TSS selector
    ltr ax

    ret

; ═══════════════════════════════════════════════════
; syscall_handler — INT 0x80 handler
; Constitutional boundary: user → kernel
; rax = syscall number, rdi = arg1, rsi = arg2, rdx = arg3
; ═══════════════════════════════════════════════════

syscall_handler:
    ; We're now in ring 0 (CPU switched via interrupt gate)
    ; Save user registers
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp

    ; Dispatch based on syscall number in rax
    cmp rax, SYS_WRITE
    je .do_write
    cmp rax, SYS_EXIT
    je .do_exit
    cmp rax, SYS_YIELD
    je .do_yield
    cmp rax, SYS_GETPID
    je .do_getpid

    ; Unknown syscall — return -1
    mov rax, -1
    jmp .syscall_ret

.do_write:
    ; sys_write: rdi = char, rsi = x position, rdx = y position
    ; Kernel mediates VGA access — user can't write directly
    ; Compute VGA offset: VGA_BASE + y * 160 + x * 2
    mov rax, rdx
    imul rax, 160               ; y * 160
    mov rbx, rsi
    shl rbx, 1                  ; x * 2
    add rax, rbx
    add rax, VGA_BASE

    ; Write character + color (white on black)
    mov byte [rax], dil         ; character (low byte of rdi)
    mov byte [rax + 1], WHITE   ; color attribute
    xor rax, rax                ; return 0 = success
    jmp .syscall_ret

.do_exit:
    ; sys_exit: terminate current process
    ; Set state to TERMINATED
    xor rax, rax
    mov eax, [current_pid]
    mov rbx, PCB_SIZE
    imul rax, rbx
    add rax, PCB_TABLE
    mov qword [rax + PCB_STATE], STATE_TERMINATED

    ; Switch to next process immediately
    ; (simplified: just HLT and let timer pick it up)
    sti
    hlt
    jmp .do_exit                ; shouldn't reach here

.do_yield:
    ; sys_yield: voluntary context switch
    ; Trigger a software timer interrupt
    int 32                      ; trigger IRQ0 handler
    xor rax, rax
    jmp .syscall_ret

.do_getpid:
    ; sys_getpid: return current PID
    xor rax, rax
    mov eax, [current_pid]
    jmp .syscall_ret

.syscall_ret:
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    iretq

; ═══════════════════════════════════════════════════
; Process A — uses syscalls to write to VGA
; Runs in ring 3. Cannot access VGA directly.
; ═══════════════════════════════════════════════════

proc_a_entry:
    xor r13, r13               ; counter
.a_loop:
    inc r13

    ; Use syscall to write "A" at position (0, 10)
    ; sys_write: rax=0, rdi=char, rsi=x, rdx=y
    mov rax, SYS_WRITE
    mov rdi, 'A'
    mov rsi, 0
    mov rdx, 10
    int 0x80

    ; Write tick count digit at (2, 10)
    mov rax, SYS_WRITE
    mov rdi, r13
    and rdi, 0x0F               ; low nibble
    add rdi, '0'
    cmp rdi, '9'
    jle .a_digit_ok
    add rdi, 7                  ; A-F
.a_digit_ok:
    mov rsi, 2
    mov rdx, 10
    mov rax, SYS_WRITE
    int 0x80
    call print_hex

    ; Busy wait (so we don't spam too fast)
    mov rcx, 0x100000
.a_wait:
    dec rcx
    jnz .a_wait
    jmp .a_loop

; ═══════════════════════════════════════════════════
; Process B — uses syscalls to write to VGA
; Runs in ring 3.
; ═══════════════════════════════════════════════════

proc_b_entry:
    xor r14, r14               ; counter
.b_loop:
    inc r14

    ; sys_write: 'B' at (0, 12)
    mov rax, SYS_WRITE
    mov rdi, 'B'
    mov rsi, 0
    mov rdx, 12
    int 0x80

    ; Write tick count digit at (2, 12)
    mov rax, SYS_WRITE
    mov rdi, r14
    and rdi, 0x0F
    add rdi, '0'
    cmp rdi, '9'
    jle .b_digit_ok
    add rdi, 7
.b_digit_ok:
    mov rsi, 2
    mov rdx, 12
    mov rax, SYS_WRITE
    int 0x80

    ; Busy wait
    mov rcx, 0x100000
.b_wait:
    dec rcx
    jnz .b_wait
    jmp .b_loop

; ═══════════════════════════════════════════════════
; Utility: print null-terminated string
; rdi = VGA position, rsi = string, ah = color
; ═══════════════════════════════════════════════════

print_string:
    push rsi
.ps_loop:
    lodsb
    test al, al
    jz .ps_done
    mov [rdi], al
    mov [rdi + 1], ah
    add rdi, 2
    jmp .ps_loop
.ps_done:
    pop rsi
    ret

; ═══════════════════════════════════════════════════
; Utility: print hex (rax) at rdi, color in ah
; Prints 6 hex digits
; ═══════════════════════════════════════════════════

print_hex:
    push rbx
    push rcx
    push rdx

    mov byte [rdi], '0'
    mov [rdi + 1], ah
    mov byte [rdi + 2], 'x'
    mov [rdi + 3], ah
    add rdi, 4

    mov rbx, rax
    mov rcx, 5                  ; 6 nibbles (5 down to 0)
.hex_loop:
    mov rax, rbx
    push rcx
    mov cl, cl
    shl cl, 2                   ; nibble position
    shr rax, cl
    pop rcx
    and rax, 0x0F

    cmp al, 10
    jl .hex_dec
    add al, 'A' - 10
    jmp .hex_out
.hex_dec:
    add al, '0'
.hex_out:
    mov [rdi], al
    mov [rdi + 1], ah
    add rdi, 2

    dec rcx
    jns .hex_loop

    pop rdx
    pop rcx
    pop rbx
    ret

; ═══════════════════════════════════════════════════
; .rodata — ξ string constants
; ═══════════════════════════════════════════════════

str_banner:   db "Mongwu v0.4.0 — Syscalls + User Mode", 0
str_machine:  db "songqiao: Acer E5-576G (Ironman_SK) i5-8250U 16GB", 0
str_mem_ok:   db "Memory: initialized (bitmap at 0x200000)", 0
str_procs:    db "Processes: 3 created (kernel + A + B)", 0
str_sched:    db "Scheduler: round-robin 50ms, TSS + ring 3 LIVE", 0
str_proc_a:   db "Process A tick: ", 0
str_proc_b:   db "Process B tick: ", 0
str_axiom:    db "Everything is itself.", 0

; ═══════════════════════════════════════════════════
; IDT descriptor
; ═══════════════════════════════════════════════════

idt_descriptor:
    dw 4095                     ; limit (256 * 16 - 1)
    dq IDT_BASE                 ; base address

; ═══════════════════════════════════════════════════
; .bss — x (variable) mutable runtime state
; ═══════════════════════════════════════════════════

section .bss
mem_total:      resq 1
tick_count:     resq 1
current_pid:    resd 1
process_count:  resd 1
tmp_gdtr:       resb 10

; Pad to sector boundary
section .text
times 8192 - ($ - $$) db 0
