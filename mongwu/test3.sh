#!/bin/bash
MDIR="/mnt/c/Users/danie/OneDrive/Documents/8 Chicago/apps/mobile/suihan/mongwu"
cp "$MDIR/boot.asm" /tmp/boot.asm
cp "$MDIR/kernel_entry.asm" /tmp/kernel_entry.asm
cd /tmp
nasm -f bin -o boot.bin boot.asm
nasm -f bin -o kernel.bin kernel_entry.asm
dd if=/dev/zero of=pad.bin bs=512 count=60 2>/dev/null
cat boot.bin kernel.bin pad.bin > mongwu.img

echo "kernel: $(stat -c%s kernel.bin) bytes"

pkill -f qemu-system 2>/dev/null || true
sleep 1

timeout 8 qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -d int 2>/tmp/intlog.txt || true

echo "=== Timer fires (v=20) ==="
grep -c "v=20" /tmp/intlog.txt 2>/dev/null || echo "0"

echo "=== GPFs (v=0d) ==="
grep -c "v=0d" /tmp/intlog.txt 2>/dev/null || echo "0"

echo "=== Page faults (v=0e) ==="
grep -c "v=0e" /tmp/intlog.txt 2>/dev/null || echo "0"

echo "=== First 5 interrupts ==="
grep "v=" /tmp/intlog.txt 2>/dev/null | head -5

echo "=== Process A code? (0x8xxx high addresses) ==="
grep "pc=00000000000081" /tmp/intlog.txt 2>/dev/null | head -3

cp /tmp/mongwu.img "$MDIR/mongwu.img" 2>/dev/null || true
