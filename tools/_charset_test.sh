#!/bin/sh
GXX=$(command -v mips64r5900el-ps2-elf-g++)
echo "g++ = $GXX"
$GXX --version | head -1
printf 'const char *s = "\\xe6\\xb5\\x8b\\xe8\\xaf\\x95";\nint main(){return 0;}\n' > /tmp/t.cpp
$GXX -finput-charset=UTF-8 -fexec-charset=UTF-8 -c /tmp/t.cpp -o /tmp/t.o
echo "=== COMPILE WITH CHARSET FLAGS OK ==="
echo "=== .rodata section bytes ==="
mips64r5900el-ps2-elf-objcopy -O binary -j .rodata /tmp/t.o /tmp/t.bin
hexdump -C /tmp/t.bin
echo "=== default flags for comparison ==="
$GXX -c /tmp/t.cpp -o /tmp/t2.o
mips64r5900el-ps2-elf-objcopy -O binary -j .rodata /tmp/t2.o /tmp/t2.bin
hexdump -C /tmp/t2.bin
