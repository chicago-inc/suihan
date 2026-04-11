#!/bin/bash
set -e
cd /tmp

# Build image
dd if=/dev/zero of=pad.bin bs=512 count=60 2>/dev/null
cat boot.bin kernel.bin pad.bin > mongwu.img
ls -la mongwu.img

# Kill old qemu
pkill -f qemu-system 2>/dev/null || true
sleep 1

# Boot
qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -no-reboot \
  -d in_asm \
  -D /tmp/mongwu_trace.log \
  2>/dev/null &

sleep 5

echo "=== Kernel execution check ==="
if grep -q "0x0000000000008" /tmp/mongwu_trace.log; then
  echo "KERNEL CODE REACHED!"
  echo ""
  echo "=== Screen clear + VGA writes ==="
  grep "0x0000000000008" /tmp/mongwu_trace.log | head -50
  echo ""
  echo "=== page_alloc execution ==="
  grep -c "page_alloc\|alloc_scan\|bts" /tmp/mongwu_trace.log || true
  echo "instruction lines found"
  echo ""
  echo "=== HLT (kernel finished) ==="
  grep "hlt" /tmp/mongwu_trace.log | tail -3
else
  echo "Kernel NOT reached. Last instructions:"
  tail -40 /tmp/mongwu_trace.log
fi

pkill -f qemu-system 2>/dev/null || true
