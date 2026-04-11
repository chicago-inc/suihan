#!/bin/bash
set -e

MDIR="/mnt/c/Users/danie/OneDrive/Documents/8 Chicago/apps/mobile/suihan/mongwu"

echo "=== Assembling ==="
cp "$MDIR/boot.asm" /tmp/boot.asm
cp "$MDIR/kernel_entry.asm" /tmp/kernel_entry.asm
cd /tmp

nasm -f bin -o boot.bin boot.asm
echo "boot.bin: $(stat -c%s boot.bin) bytes"

nasm -f bin -o kernel.bin kernel_entry.asm
echo "kernel.bin: $(stat -c%s kernel.bin) bytes"

echo "=== Building image ==="
dd if=/dev/zero of=pad.bin bs=512 count=60 2>/dev/null
cat boot.bin kernel.bin pad.bin > mongwu.img
echo "mongwu.img: $(stat -c%s mongwu.img) bytes"

echo "=== Booting QEMU ==="
pkill -f qemu-system 2>/dev/null || true
sleep 1

# Run for 8 seconds — enough for several timer ticks
timeout 8 qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -no-reboot 2>/dev/null
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
  echo "QEMU exited cleanly (kernel halted)"
elif [ $EXIT_CODE -eq 124 ]; then
  echo "QEMU timed out after 8s (kernel still running = SCHEDULER IS ALIVE)"
else
  echo "QEMU exit code: $EXIT_CODE"
fi

cp /tmp/mongwu.img "$MDIR/mongwu.img" 2>/dev/null || true
echo "DONE"
