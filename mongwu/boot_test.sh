#!/bin/bash
set -e

MDIR="/mnt/c/Users/danie/OneDrive/Documents/8 Chicago/apps/mobile/suihan/mongwu"

pkill -f qemu-system 2>/dev/null || true
sleep 1

echo "=== Padding image ==="
dd if=/dev/zero of=/tmp/pad.bin bs=512 count=30 2>/dev/null
cat "$MDIR/boot.bin" "$MDIR/kernel.bin" /tmp/pad.bin > /tmp/mongwu.img
echo "Image: $(wc -c < /tmp/mongwu.img) bytes"

echo "=== Booting QEMU ==="
qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -no-reboot \
  -d in_asm \
  -D /tmp/qemu_trace.log \
  2>/dev/null &

QPID=$!
sleep 5

echo "=== Checking execution ==="
if grep -q "0x0000000000008" /tmp/qemu_trace.log 2>/dev/null; then
  echo "KERNEL CODE REACHED at 0x8000!"
  echo ""
  echo "=== Kernel instructions ==="
  grep "0x0000000000008" /tmp/qemu_trace.log | head -30
else
  echo "Kernel NOT reached. Last instructions:"
  tail -30 /tmp/qemu_trace.log
fi

kill $QPID 2>/dev/null || true
