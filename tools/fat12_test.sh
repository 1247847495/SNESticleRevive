export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
mkdir -p /tmp/fftest && cd /tmp/fftest || exit 1

# normalize line endings (files arrive from Windows via docker cp)
sed -i 's/\r$//' fat12_mkimage.py fat12_diskio.c fat12_test.sh module_debug_stub.h 2>/dev/null

# PC stub of the PS2SDK IOP header (ps2sdk's FatFs ff.c pulls it in; both
# builds use the exact same .c sources, only ffconf.h differs).
cp module_debug_stub.h module_debug.h

python3 fat12_mkimage.py || exit 1

FATFS_NEW=/tmp/ps2sdk/common/external_deps/fatfs/source   # FF_LFN_UNICODE=2 (patched, same sources as rebuilt IRX)
FATFS_OLD=/tmp/fatfs/source                                # FF_LFN_UNICODE=0 (stock)

echo "=== build with PATCHED config (FF_LFN_UNICODE=2) ==="
gcc -O1 -Wall -I. -I"$FATFS_NEW/include" "$FATFS_NEW/ff.c" "$FATFS_NEW/ffunicode.c" fat12_diskio.c -o test_new 2>&1 || exit 1
./test_new

echo ""
echo "=== build with STOCK config (FF_LFN_UNICODE=0) ==="
gcc -O1 -Wall -I. -I"$FATFS_OLD/include" "$FATFS_OLD/ff.c" "$FATFS_OLD/ffunicode.c" fat12_diskio.c -o test_old 2>&1 || exit 1
./test_old
