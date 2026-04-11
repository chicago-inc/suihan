#!/bin/bash
set -e

MDIR="/mnt/c/Users/danie/OneDrive/Documents/8 Chicago/apps/mobile/suihan/mongwu"

# Assemble
cp "$MDIR/boot.asm" /tmp/boot.asm
cp "$MDIR/kernel_entry.asm" /tmp/kernel_entry.asm
cd /tmp
nasm -f bin -o boot.bin boot.asm
nasm -f bin -o kernel.bin kernel_entry.asm
dd if=/dev/zero of=pad.bin bs=512 count=60 2>/dev/null
cat boot.bin kernel.bin pad.bin > mongwu.img

echo "Built: boot=$(stat -c%s boot.bin) kernel=$(stat -c%s kernel.bin) image=$(stat -c%s mongwu.img)"

# Boot with monitor
pkill -f qemu-system 2>/dev/null || true
sleep 1

qemu-system-x86_64 \
  -drive format=raw,file=/tmp/mongwu.img \
  -display none \
  -monitor telnet:127.0.0.1:4444,server,nowait &

sleep 4

# Dump VGA rows
echo "=== VGA FRAMEBUFFER DUMP ==="
echo ""
for ROW in 0 1 2 4 6 7 8 9 11; do
  ADDR=$(printf "0x%x" $((0xb8000 + ROW * 160)))
  CHARS=$(echo "xp /80cb $ADDR" | nc -q 1 127.0.0.1 4444 2>/dev/null | grep ":" | sed 's/.*: //' | tr -d '\n')
  # Extract just the printable ASCII characters (every other byte)
  echo "Row $ROW: $(echo "$CHARS" | tr ' ' '\n' | awk 'NR%2==1' | while read -r byte; do
    val=$((byte))
    if [ "$val" -ge 32 ] && [ "$val" -le 126 ]; then
      printf "\\x$(printf '%02x' $val)"
    fi
  done)"
done

echo ""
echo "=== RAW ROW 0 (should be 'Mongwu') ==="
echo "xp /12cb 0xb8000" | nc -q 1 127.0.0.1 4444 2>/dev/null

echo ""
echo "quit" | nc -q 1 127.0.0.1 4444 2>/dev/null || true
pkill -f qemu-system 2>/dev/null || true

# Copy image back
cp /tmp/mongwu.img "$MDIR/mongwu.img" 2>/dev/null || true
echo "DONE"
