#!/bin/bash
pkill -f qemu-system 2>/dev/null || true
sleep 1

# Start QEMU with telnet monitor
qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -monitor telnet:127.0.0.1:4444,server,nowait &

QPID=$!
sleep 4

# Dump VGA text buffer - each char is 2 bytes (char + attribute)
# Row 0: "Mongwu" (12 bytes = 6 chars)
# Row 1: version string
# Row 2: songqiao
# Row 4: memory info
# Row 6: alloc test header
# Row 7-9: allocated addresses
# Row 11: axiom
{
echo "xp /80cb 0xb8000"   # Row 0
sleep 0.5
echo "xp /80cb 0xb80a0"   # Row 1
sleep 0.5
echo "xp /80cb 0xb8140"   # Row 2
sleep 0.5
echo "xp /80cb 0xb8280"   # Row 4 (memory)
sleep 0.5
echo "xp /80cb 0xb8420"   # Row 6 (alloc header)
sleep 0.5
echo "xp /80cb 0xb84c0"   # Row 7 (page 1)
sleep 0.5
echo "xp /80cb 0xb8560"   # Row 8 (page 2)
sleep 0.5
echo "xp /80cb 0xb8600"   # Row 9 (page 3)
sleep 0.5
echo "xp /80cb 0xb8780"   # Row 11 (axiom)
sleep 0.5
echo "quit"
} | nc -q 2 127.0.0.1 4444

wait $QPID 2>/dev/null || true
