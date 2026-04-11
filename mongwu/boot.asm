; boot.asm — Mongwu bootloader
; 512-byte MBR → protected mode → long mode (64-bit) → jump to kernel
;
; This is below the Lakatos barrier — raw procedural assembly.
; The same structural boundary as the lexer FSM in suhc.
;
; Assemble: nasm -f bin -o boot.bin boot.asm

[bits 16]
[org 0x7C00]

; ═══════════════════════════════════════════════════
; Stage 1: Real Mode (16-bit)
; ═══════════════════════════════════════════════════

boot_start:
    cli                         ; disable interrupts
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00             ; stack grows down from boot sector

    ; Load kernel from disk (sectors 2+) into 0x8000
    mov ah, 0x02               ; BIOS read sectors
    mov al, 32                 ; read 32 sectors (16KB — room for memory code)
    mov ch, 0                  ; cylinder 0
    mov cl, 2                  ; start at sector 2 (sector 1 = boot)
    mov dh, 0                  ; head 0
    mov dl, 0x80               ; first hard drive
    mov bx, 0x8000             ; destination
    int 0x13                   ; BIOS disk interrupt
    jc disk_error

    ; ═══════════════════════════════════════════════════
    ; Stage 1b: E820 Memory Map
    ; Query BIOS for physical memory layout.
    ; Store entries at 0x500 (safe below 0x7C00).
    ; Each entry: 20 bytes (base_lo, base_hi, len_lo, len_hi, type)
    ; Entry count stored at 0x500 - 4 (0x4FC)
    ; ═══════════════════════════════════════════════════

    mov di, 0x504              ; first entry at 0x504 (count at 0x500)
    xor ebx, ebx               ; continuation = 0 (start)
    xor bp, bp                  ; entry counter
    mov edx, 0x534D4150        ; 'SMAP' magic

.e820_loop:
    mov eax, 0xE820
    mov ecx, 20                ; entry size
    int 0x15
    jc .e820_done              ; carry = end or unsupported

    cmp eax, 0x534D4150        ; verify SMAP signature
    jne .e820_done

    inc bp                     ; count this entry
    add di, 20                 ; advance to next slot

    test ebx, ebx              ; ebx = 0 means last entry
    jz .e820_done
    jmp .e820_loop

.e820_done:
    mov [0x500], bp            ; store entry count at 0x500

    ; ═══════════════════════════════════════════════════
    ; Stage 2: Enter Protected Mode (32-bit)
    ; ═══════════════════════════════════════════════════

    lgdt [gdt_descriptor]      ; load GDT

    mov eax, cr0
    or eax, 1                  ; set PE (Protection Enable) bit
    mov cr0, eax

    jmp 0x08:protected_mode    ; far jump to 32-bit code segment

disk_error:
    ; Print 'E' on screen and halt
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    hlt

; ═══════════════════════════════════════════════════
; GDT — Global Descriptor Table
; Flat model: code and data segments span all 4GB
; ═══════════════════════════════════════════════════

gdt_start:
    dq 0                       ; null descriptor

gdt_code_32:                   ; 32-bit code segment
    dw 0xFFFF                  ; limit low
    dw 0                       ; base low
    db 0                       ; base mid
    db 10011010b               ; access: present, ring 0, code, readable
    db 11001111b               ; flags: 4KB granularity, 32-bit
    db 0                       ; base high

gdt_data_32:                   ; 32-bit data segment
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b               ; access: present, ring 0, data, writable
    db 11001111b
    db 0

gdt_code_64:                   ; 64-bit code segment
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b               ; access: present, ring 0, code, readable
    db 10101111b               ; flags: 4KB granularity, long mode
    db 0

gdt_data_64:                   ; 64-bit data segment (ring 0)
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 10101111b
    db 0

gdt_user_code_64:              ; 64-bit code segment (ring 3)
    dw 0xFFFF
    dw 0
    db 0
    db 11111010b               ; access: present, ring 3, code, readable
    db 10101111b               ; flags: 4KB granularity, long mode
    db 0

gdt_user_data_64:              ; 64-bit data segment (ring 3)
    dw 0xFFFF
    dw 0
    db 0
    db 11110010b               ; access: present, ring 3, data, writable
    db 10101111b
    db 0

gdt_tss:                       ; TSS descriptor (16 bytes in long mode)
    dw 0x0067                  ; limit (103 bytes = minimum TSS)
    dw 0                       ; base low (patched by kernel)
    db 0                       ; base mid
    db 10001001b               ; access: present, ring 0, TSS available
    db 00000000b               ; flags
    db 0                       ; base high
    dd 0                       ; base upper 32 bits
    dd 0                       ; reserved

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; size
    dd gdt_start               ; offset

; Segment selectors
CODE_SEG_32 equ gdt_code_32 - gdt_start   ; 0x08
DATA_SEG_32 equ gdt_data_32 - gdt_start   ; 0x10
CODE_SEG_64 equ gdt_code_64 - gdt_start        ; 0x18
DATA_SEG_64 equ gdt_data_64 - gdt_start        ; 0x20
USER_CODE_64 equ gdt_user_code_64 - gdt_start  ; 0x28
USER_DATA_64 equ gdt_user_data_64 - gdt_start  ; 0x30
TSS_SEG equ gdt_tss - gdt_start                ; 0x38

; ═══════════════════════════════════════════════════
; Stage 3: Protected Mode (32-bit)
; Set up paging for long mode transition
; ═══════════════════════════════════════════════════

[bits 32]
protected_mode:
    mov ax, DATA_SEG_32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000           ; stack at 576KB

    ; Set up identity-mapped page tables for first 16MB
    ; PML4 → PDPT → PD → 8 × 2MB huge pages
    ; Covers: kernel, VGA, bitmap, PCBs, IDT, process stacks

    ; Clear page table area (0x1000 - 0x4FFF)
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 0x1000            ; 4096 dwords = 16KB
    rep stosd

    ; PML4[0] → PDPT at 0x2000 (0x7 = present + writable + user)
    mov dword [0x1000], 0x2007

    ; PDPT[0] → PD at 0x3000 (0x7 = present + writable + user)
    mov dword [0x2000], 0x3007

    ; PD[0..7] → 8 × 2MB huge pages (0MB - 16MB identity mapped)
    ; 0x87 = present + writable + user + huge (U/S bit set for ring 3)
    ; Kernel will enforce protection via syscall interface, not page tables
    ; (proper per-process page tables are a future sprint)
    mov dword [0x3000], 0x000087    ; 0x000000 - 0x1FFFFF (kernel+VGA)
    mov dword [0x3008], 0x200087    ; 0x200000 - 0x3FFFFF (bitmap)
    mov dword [0x3010], 0x400087    ; 0x400000 - 0x5FFFFF (stacks)
    mov dword [0x3018], 0x600087    ; 0x600000 - 0x7FFFFF
    mov dword [0x3020], 0x800087    ; 0x800000 - 0x9FFFFF
    mov dword [0x3028], 0xA00087    ; 0xA00000 - 0xBFFFFF (VGA phys)
    mov dword [0x3030], 0xC00087    ; 0xC00000 - 0xDFFFFF
    mov dword [0x3038], 0xE00087    ; 0xE00000 - 0xFFFFFF

    ; Enable PAE (Physical Address Extension)
    mov eax, cr4
    or eax, (1 << 5)          ; CR4.PAE
    mov cr4, eax

    ; Load PML4 into CR3
    mov eax, 0x1000
    mov cr3, eax

    ; Enable long mode via EFER MSR
    mov ecx, 0xC0000080        ; IA32_EFER
    rdmsr
    or eax, (1 << 8)          ; EFER.LME (Long Mode Enable)
    wrmsr

    ; Enable paging (activates long mode)
    mov eax, cr0
    or eax, (1 << 31)         ; CR0.PG
    mov cr0, eax

    ; Far jump to 64-bit code segment
    jmp CODE_SEG_64:long_mode

; ═══════════════════════════════════════════════════
; Stage 4: Long Mode (64-bit)
; The constitutional kernel begins here
; ═══════════════════════════════════════════════════

[bits 64]
long_mode:
    mov ax, DATA_SEG_64
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000           ; 64-bit stack

    ; Jump to kernel loaded at 0x8000
    jmp 0x8000

; ═══════════════════════════════════════════════════
; Boot sector padding and signature
; ═══════════════════════════════════════════════════

times 510 - ($ - $$) db 0     ; pad to 510 bytes
dw 0xAA55                      ; boot signature
