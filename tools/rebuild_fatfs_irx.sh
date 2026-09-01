#!/bin/sh
# =====================================================================
# Rebuild irx/bdmfs_fatfs.irx with UTF-8 long-filename output.
#
# Run inside the ps2dev/ps2dev:latest container:
#   docker run --rm -t -v "%CD%:/src" -w /src ps2dev/ps2dev:latest \
#       sh /src/tools/rebuild_fatfs_irx.sh
#
# Steps:
#   1. Fetch ps2dev/ps2sdk @ f08e889f (same commit as all pinned IRX).
#   2. Clone fjtrujy/FatFs branch iop-r0.16 (what download_dependencies.sh
#      pulls for that commit) into common/external_deps/fatfs.
#   3. Patch ffconf.h: FF_LFN_UNICODE 0 -> 2 (TCHAR becomes UTF-8, so
#      f_readdir/f_getstat hand UTF-8 names to the EE).
#   4. Build the host srxfixup tool, then only the bdmfs_fatfs module
#      (link is -r relocatable, no prebuilt SDK libs needed).
#   5. Copy the result over /src/irx/bdmfs_fatfs.irx.
# =====================================================================
set -e

SDK_COMMIT=f08e889fef8ab361f863c44ebe78212ced2839ca
FATFS_BRANCH=iop-r0.16

echo "[SETUP] installing host tools..."
apk add --no-cache bash make gcc musl-dev git >/tmp/apk.log 2>&1 || { cat /tmp/apk.log; exit 1; }

export PS2SDKSRC=/tmp/ps2sdk
rm -rf /tmp/ps2sdk /tmp/ps2sdk.fetch

echo "[FETCH] ps2sdk @ ${SDK_COMMIT}"
git clone -q --filter=blob:none --no-checkout https://github.com/ps2dev/ps2sdk.git /tmp/ps2sdk
cd /tmp/ps2sdk
git checkout -q "${SDK_COMMIT}" || { git fetch -q --depth 1 origin "${SDK_COMMIT}" && git checkout -q FETCH_HEAD; }
cd /tmp
echo "[DEBUG] ps2sdk root:"
ls /tmp/ps2sdk
test -f /tmp/ps2sdk/tools/srxfixup/Makefile || { echo "[ERROR] tools/srxfixup missing"; exit 1; }

echo "[FETCH] FatFs branch ${FATFS_BRANCH}"
mkdir -p /tmp/ps2sdk/common/external_deps
git clone -q --depth 1 -b "${FATFS_BRANCH}" https://github.com/fjtrujy/FatFs.git /tmp/ps2sdk/common/external_deps/fatfs

FFCONF=/tmp/ps2sdk/common/external_deps/fatfs/source/include/ffconf.h
echo "[PATCH] ${FFCONF}: FF_LFN_UNICODE 0 -> 2 (UTF-8)"
sed -i 's/^\(#define FF_LFN_UNICODE[[:space:]]*\)0/\12/' "${FFCONF}"
if ! grep -q '^#define FF_LFN_UNICODE[[:space:]]*2' "${FFCONF}"; then
    echo "[ERROR] FF_LFN_UNICODE patch failed"; grep -n FF_LFN_UNICODE "${FFCONF}"; exit 1
fi
grep -n 'FF_LFN_UNICODE\|FF_CODE_PAGE\|FF_USE_LFN\|FF_FS_EXFAT' "${FFCONF}" | grep define

echo "[BUILD] host srxfixup tool"
make -C /tmp/ps2sdk/tools/srxfixup -j"$(nproc)" >/tmp/srxfixup.log 2>&1 || { cat /tmp/srxfixup.log; exit 1; }

echo "[BUILD] bdmfs_fatfs module"
make -C /tmp/ps2sdk/iop/fs/bdmfs_fatfs -j"$(nproc)" >/tmp/fatfs.log 2>&1 || { cat /tmp/fatfs.log; exit 1; }

IRX=/tmp/ps2sdk/iop/fs/bdmfs_fatfs/irx/bdmfs_fatfs.irx
if [ ! -s "${IRX}" ]; then
    IRX=$(find /tmp/ps2sdk/iop/fs/bdmfs_fatfs -name 'bdmfs_fatfs.irx' -type f | head -n 1)
fi
if [ -z "${IRX}" ] || [ ! -s "${IRX}" ]; then
    echo "[ERROR] bdmfs_fatfs.irx missing"; cat /tmp/fatfs.log; exit 1
fi

cp "${IRX}" /src/irx/bdmfs_fatfs.irx
echo "[OK] replaced /src/irx/bdmfs_fatfs.irx"
ls -l /src/irx/bdmfs_fatfs.irx
sha256sum /src/irx/bdmfs_fatfs.irx
