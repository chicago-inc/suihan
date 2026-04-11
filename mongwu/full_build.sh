#!/bin/bash
set -e

MDIR="/mnt/c/Users/danie/OneDrive/Documents/8 Chicago/apps/mobile/suihan/mongwu"

echo "=== Copy sources to /tmp ==="
cp "$MDIR/boot.asm" /tmp/boot.asm
cp "$MDIR/kernel_entry.asm" /tmp/kernel_entry.asm
cd /tmp

echo "=== Assemble ==="
nasm -f bin -o boot.bin boot.asm
nasm -f bin -o kernel.bin kernel_entry.asm
echo "boot.bin: $(stat -c%s boot.bin) bytes"
echo "kernel.bin: $(stat -c%s kernel.bin) bytes"

echo "=== Build image ==="
dd if=/dev/zero of=pad.bin bs=512 count=60 2>/dev/null
cat boot.bin kernel.bin pad.bin > mongwu.img
echo "mongwu.img: $(stat -c%s mongwu.img) bytes"

echo "=== Kill old QEMU ==="
pkill -f qemu-system 2>/dev/null || true
sleep 1

echo "=== Boot QEMU ==="
qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -no-reboot \
  -d in_asm \
  -D /tmp/trace.log \
  2>/dev/null &

sleep 5

echo ""
echo "=== EXECUTION TRACE ==="
if grep -q "0x0000000000008" /tmp/trace.log; then
  echo ">>> KERNEL CODE REACHED <<<"
  echo ""
  TOTAL=$(grep -c "0x0000000000008" /tmp/trace.log)
  echo "Kernel instructions executed: $TOTAL"
  echo ""
  echo "--- First 30 kernel instructions ---"
  grep "0x0000000000008" /tmp/trace.log | head -30
  echo ""
  echo "--- HLT (kernel done) ---"
  grep "hlt" /tmp/trace.log | tail -3
else
  echo ">>> KERNEL NOT REACHED <<<"
  echo ""
  echo "--- Last 30 instructions ---"
  tail -30 /tmp/trace.log
fi

pkill -f qemu-system 2>/dev/null || true

# Copy image back
cp /tmp/mongwu.img "$MDIR/mongwu.img" 2>/dev/null || true
