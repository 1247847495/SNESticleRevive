#!/bin/bash
set -e
export PATH=/usr/local/ps2dev/iop/bin:/usr/local/ps2dev/bin:/usr/local/ps2dev/ps2sdk/bin:/usr/bin:/bin:$PATH
cd /tmp/ps2sdk/iop/dev9/atad
make clean >/dev/null 2>&1 || true
# Build with the f08e889f tree itself (PS2SDKSRC), whose bdm.h has the
# `path` field -> devices appear as ata0:/ata1: via bdmfs_fatfs typed drivers.
make ATA_ENABLE_BDM=1 IOP_BIN=atad_bd.irx PS2SDKSRC=/tmp/ps2sdk PS2SDK=/usr/local/ps2dev/ps2sdk
ls -la atad_bd.irx
echo "--- sha256 ---"
sha256sum atad_bd.irx
echo "--- strings check ---"
strings atad_bd.irx | grep -E 'ata|BDM|Driver' | head -8
