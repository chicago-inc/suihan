# Mongwu OS Roadmap

**From ordbok to operating system.** Every sprint adds capability by adding ordbok declarations, compiling to assembly, and booting. The kind system is the ISA mapping.

---

## Completed

### Sprint 1 — Genesis (v0.1.0)
- `emit_asm.c`: x86_64 NASM backend for suhc (M8 milestone)
- `boot.asm`: 512-byte MBR, real → protected → long mode
- `kernel_os.szh`: kernel ordbok with machine-specific songqiao
- **Proof:** "Mongwu" written to VGA framebuffer at 0xB8000

### Sprint 2 — Memory (v0.2.0)
- Hex literal support in lexer (`0xB8000` etc.)
- `memory.szh`: page allocation spec (meihua pure math)
- E820 BIOS memory detection in bootloader
- Bitmap page allocator (512 KB bitmap, 4M pages, 16 GB)
- **Proof:** E820 scan + bitmap init + clean exit

### Sprint 3 — Process (v0.3.0)
- `process.szh`: process spec with morphisms and projections
- IDT, PIC remap, PIT timer at 50ms
- Context switch: save 15 regs + iretq, round-robin next_pid
- Two test processes preempted by timer
- **Proof:** 153 timer interrupts, 3 processes, zero faults

### Sprint 4 — Syscalls (v0.4.0)
- `syscall.szh`: syscall interface spec (8 calls, dispatch projection)
- GDT ring-3 segments, TSS for ring transitions
- INT 0x80 handler (DPL=3), kernel-mediated VGA writes
- Processes run in user mode (ring 3, cpl=3)
- **Proof:** 2,316 syscalls at cpl=3, zero faults

---

## Phase 1 — Isolation (sprints 5-7)

The kind system becomes hardware-enforced. ξ physically cannot be written by user processes.

### Sprint 5 — Per-Process Page Tables
- Each process gets its own CR3 (page table root)
- Kernel pages mapped read-only in user space
- Process A cannot see Process B's memory
- Context switch reloads CR3
- **Ordbok:** `ordbok/vmem.szh` — virtual memory specification
- **Acceptance:** Process A writes to address X, Process B reads same virtual address and sees different data

### Sprint 6 — Proper ξ Enforcement
- Kernel .rodata mapped as read-only + supervisor in all page tables
- User attempt to write ξ address triggers page fault → process terminated
- songqiao mapped read-only to user, read-write to kernel only
- **Acceptance:** Test process attempts `mov [kernel_name], 0` → GPF → terminated. ξ is physically immutable.

### Sprint 7 — Process Lifecycle
- `sys_exit`: clean termination, page reclamation
- `sys_spawn`: create new process from entry point
- `sys_wait`: block until child terminates
- Process state machine fully wired: running → ready → blocked → terminated
- Zombie reaping: terminated processes release pages on next schedule tick
- **Ordbok:** extend `process.szh` with spawn/wait morphisms

---

## Phase 2 — I/O (sprints 8-10)

The user (Lilith) talks to Mongwu directly.

### Sprint 8 — Keyboard Driver (IRQ1)
- PS/2 keyboard interrupt handler
- Scancode → ASCII translation (meihua, pure lookup)
- Kernel ring buffer for key events
- `sys_read`: user process reads from keyboard buffer
- **Ordbok:** `ordbok/keyboard.szh` — scancode dimensions, translation projection
- **Acceptance:** User presses key → char appears on screen via syscall chain

### Sprint 9 — Shell
- First interactive process: reads keyboard, parses commands, executes
- Built-in commands: `ps` (list processes), `mem` (memory stats), `halt`, `echo`
- Command dispatch is a projection (command name → handler)
- **Ordbok:** `ordbok/shell.szh` — command dimension, dispatch projection
- **Acceptance:** User types "ps" → sees process list. Types "halt" → kernel shuts down.

### Sprint 10 — Serial Console
- UART driver for serial port (COM1)
- `sys_serial_write` / `sys_serial_read` syscalls
- Shell works over serial (QEMU `-serial stdio`)
- **Acceptance:** Full interactive session from host terminal over serial

---

## Phase 3 — Storage (sprints 11-13)

The kernel persists state across boots.

### Sprint 11 — NVMe Driver (Basic)
- Songqiao already knows the disk: CT1000P3SSD8
- PCI enumeration to find NVMe controller
- Admin queue setup, identify controller
- Read one sector from disk
- **Ordbok:** `ordbok/nvme.szh` — NVMe command dimensions, queue structures

### Sprint 12 — Filesystem
- Minimal filesystem: flat directory, fixed-size files
- Constitutional constraint: files are containment (names → data blocks)
- `sys_open`, `sys_read_file`, `sys_write_file`, `sys_close`
- **Ordbok:** `ordbok/filesystem.szh` — file operations, block allocation

### Sprint 13 — Boot from Disk
- Kernel stored on NVMe, loaded by bootloader
- Filesystem contains shell and user programs as files
- System boots to shell prompt from disk, not from raw image
- **Acceptance:** Power on → BIOS → bootloader → kernel → shell prompt

---

## Phase 4 — Network (sprints 14-16)

Mongwu talks to the outside world.

### Sprint 14 — Intel AC3168 WiFi (Basic)
- PCI enumeration for WiFi controller
- Firmware loading (songqiao declares firmware path)
- Basic packet send/receive
- **Ordbok:** `ordbok/network.szh` — packet dimensions, protocol layers

### Sprint 15 — TCP/IP Stack
- IP, TCP, UDP, ICMP
- Socket syscalls: `sys_socket`, `sys_connect`, `sys_send`, `sys_recv`
- Meihua for checksum computation (pure)
- **Acceptance:** Ping external host

### Sprint 16 — HTTP Client
- Minimal HTTP/1.1 GET
- DNS resolution
- **Acceptance:** Fetch a web page from Mongwu

---

## Phase 5 — Self-Hosting (sprints 17-20)

Mongwu builds itself.

### Sprint 17 — Port GCC Cross-Compiler
- Minimal libc (meihua-derived: string ops, memory ops, I/O wrappers)
- GCC cross-compilation targeting Mongwu
- Compile and run a C program on Mongwu

### Sprint 18 — Port suhc to Mongwu
- The suihan compiler runs natively on Mongwu
- Compile a .szh file on Mongwu → assembly
- **Acceptance:** `suhc kernel_os.szh --target asm` runs on Mongwu itself

### Sprint 19 — Mongwu Builds Mongwu
- suhc compiles the kernel ordbok
- nasm assembles it
- Bootloader writes the new kernel to disk
- Reboot into self-compiled kernel
- **Acceptance:** The kernel running was compiled by the kernel it replaced

### Sprint 20 — Workspace (Mongwu Native UI)
- The Suihan workspace: Turing-complete graphical shell
- Vector rendering to framebuffer (GPU-accelerated via MX150)
- Keyboard → CLI, pointer → workspace (perpendicular delineation)
- Stack-based rendering: undo inherent, save inherent, share inherent
- **Ordbok:** `ordbok/workspace.szh` — the full workspace specification from SUIHAN.md §10
- **Acceptance:** Mongwu boots to a graphical workspace. The OS has arrived.

---

## Design Principles (every sprint)

1. **Write the ordbok first.** The specification compiles before the implementation exists.
2. **Kind system = hardware.** ξ → ROM. ζ → computed. x → RAM. R.k → .text. ω → output.
3. **Songqiao is situated.** Same kernel code, different songqiao = different machine.
4. **Meihua is pure.** No side effects. Register-only. Verifiable.
5. **Zhulin is control.** State transitions, interrupt handlers, scheduling.
6. **The axiom holds.** Everything is itself. α is α. α is not β.

---

## Current State

**Commit:** 2725d35
**Kernel size:** 8,192 bytes (8 KB)
**Capabilities:** Boot, memory detection, page allocation, preemptive scheduling, user-mode isolation, syscall interface
**Verified on:** QEMU x86_64, targeting Acer E5-576G (Ironman_SK)
