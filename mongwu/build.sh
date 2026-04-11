#!/bin/bash
# Mongwu OS build script
# Builds the constitutional kernel from ordbok → assembly → bare metal
#
# Usage: ./build.sh
# Requires: suhc (suihan compiler), nasm, qemu-system-x86_64

set -e

SUIHAN_DIR="$(cd "$(dirname "$0")/.." && pwd)"
MONGWU_DIR="$SUIHAN_DIR/mongwu"
SUHC="$SUIHAN_DIR/suhc"

echo "╔══════════════════════════════════════════════════════╗"
echo "║         Mongwu OS — Constitutional Kernel Build      ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# Phase 1: Compile ordbok → assembly
echo "── Phase 1: suhc kernel_os.szh → kernel.asm ──"
$SUHC "$SUIHAN_DIR/ordbok/kernel_os.szh" --target asm -o "$MONGWU_DIR/kernel.asm"
echo ""

# Phase 2: Assemble bootloader
echo "── Phase 2: nasm boot.asm → boot.bin ──"
nasm -f bin -o "$MONGWU_DIR/boot.bin" "$MONGWU_DIR/boot.asm"
echo "   boot.bin: $(wc -c < "$MONGWU_DIR/boot.bin") bytes"

# Phase 3: Assemble kernel entry
echo "── Phase 3: nasm kernel_entry.asm → kernel.bin ──"
nasm -f bin -o "$MONGWU_DIR/kernel.bin" "$MONGWU_DIR/kernel_entry.asm"
echo "   kernel.bin: $(wc -c < "$MONGWU_DIR/kernel.bin") bytes"

# Phase 4: Create disk image
echo "── Phase 4: cat boot.bin + kernel.bin → mongwu.img ──"
cat "$MONGWU_DIR/boot.bin" "$MONGWU_DIR/kernel.bin" > "$MONGWU_DIR/mongwu.img"
echo "   mongwu.img: $(wc -c < "$MONGWU_DIR/mongwu.img") bytes"

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║              Build complete.                          ║"
echo "║  Run: qemu-system-x86_64 -drive format=raw,file=mongwu/mongwu.img ║"
echo "╚══════════════════════════════════════════════════════╝"
