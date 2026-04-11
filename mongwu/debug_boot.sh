#!/bin/bash
cd /tmp

pkill -f qemu-system 2>/dev/null || true
sleep 1

# Boot with interrupt logging
timeout 6 qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -d int,in_asm \
  -D /tmp/debug.log \
  2>/dev/null || true

sleep 1

echo "=== Log size ==="
ls -la /tmp/debug.log 2>/dev/null

echo ""
echo "=== Kernel instructions (0x8xxx) ==="
grep "^0x0000000000008" /tmp/debug.log 2>/dev/null | head -60

echo ""
echo "=== Interrupts ==="
grep -i "interrupt\|exception\|vector\|EIP\|check_exception" /tmp/debug.log 2>/dev/null | head -20

echo ""
echo "=== Last 30 lines ==="
tail -30 /tmp/debug.log 2>/dev/null
