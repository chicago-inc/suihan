#!/bin/bash
MDIR="/mnt/c/Users/danie/OneDrive/Documents/8 Chicago/apps/mobile/suihan/mongwu"
cp "$MDIR/boot.asm" /tmp/boot.asm
cp "$MDIR/kernel_entry.asm" /tmp/kernel_entry.asm
cd /tmp

echo "=== Assembling ==="
nasm -f bin -o boot.bin boot.asm 2>&1
if [ $? -ne 0 ]; then echo "BOOT ASM FAILED"; exit 1; fi
echo "boot: $(stat -c%s boot.bin)"

nasm -f bin -o kernel.bin kernel_entry.asm 2>&1
if [ $? -ne 0 ]; then echo "KERNEL ASM FAILED"; exit 1; fi
echo "kernel: $(stat -c%s kernel.bin)"

dd if=/dev/zero of=pad.bin bs=512 count=60 2>/dev/null
cat boot.bin kernel.bin pad.bin > mongwu.img

pkill -f qemu-system 2>/dev/null || true
sleep 1

echo "=== Booting (8s) ==="
timeout 8 qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -d int 2>/tmp/intlog.txt || true

echo ""
echo "=== Results ==="
echo "Timer (v=20): $(grep -c 'v=20' /tmp/intlog.txt 2>/dev/null)"
echo "Syscall (v=80): $(grep -c 'v=80' /tmp/intlog.txt 2>/dev/null)"
echo "GPF (v=0d): $(grep -c 'v=0d' /tmp/intlog.txt 2>/dev/null)"
echo "PF (v=0e): $(grep -c 'v=0e' /tmp/intlog.txt 2>/dev/null)"
echo "Double fault (v=08): $(grep -c 'v=08' /tmp/intlog.txt 2>/dev/null)"
echo ""
echo "=== First 5 interrupts ==="
grep "v=" /tmp/intlog.txt 2>/dev/null | head -5
echo ""
echo "=== Syscall samples ==="
grep "v=80" /tmp/intlog.txt 2>/dev/null | head -5

cp /tmp/mongwu.img "$MDIR/mongwu.img" 2>/dev/null || true
