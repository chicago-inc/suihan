#!/bin/bash
set -e
MDIR="/mnt/c/Users/danie/OneDrive/Documents/8 Chicago/apps/mobile/suihan/mongwu"
cp "$MDIR/boot.asm" /tmp/boot.asm
cp "$MDIR/kernel_entry.asm" /tmp/kernel_entry.asm
cd /tmp
nasm -f bin -o boot.bin boot.asm
nasm -f bin -o kernel.bin kernel_entry.asm
dd if=/dev/zero of=pad.bin bs=512 count=60 2>/dev/null
cat boot.bin kernel.bin pad.bin > mongwu.img

pkill -f qemu-system 2>/dev/null || true
sleep 1

# Run with interrupt tracing to stderr
timeout 6 qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -d int 2>intlog.txt || true

echo "=== Log lines: $(wc -l < intlog.txt) ==="
echo ""
echo "=== Exceptions ==="
grep "check_exception" intlog.txt | head -5
echo ""
echo "=== Interrupt vectors ==="
grep "v=" intlog.txt | head -10
echo ""
echo "=== Triple fault? ==="
grep -i "triple\|shutdown\|reset" intlog.txt | head -5
echo ""
echo "=== Last 20 lines ==="
tail -20 intlog.txt
