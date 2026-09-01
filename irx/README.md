# Pinned SIO2 and storage IRX modules

These binaries are embedded in every SNESticle Revive build. The complete
SIO2 transport, memory-card and input group is kept together so an SDK update
cannot silently combine incompatible driver revisions. This also covers MMCE
and MX4SIO, which are not exported by every historical PS2SDK installation.

The five core PS2SDK modules and MX4SIO were rebuilt from commit `e228ff7b`;
each result was verified byte-identical to the official PS2DEV release.

The complete USB/BDM group is pinned for the same reason. In particular,
`usbd_mini.irx` is the pre-rewrite FreeUsbd implementation restored for OPL
and other BDM loaders, rather than the newer full `usbd.irx` that regressed on
some real USB devices. The three BDM/FatFs modules were taken together from
PS2SDK commit `f08e889f` and verified byte-identical to its official PS2DEV
release. `bdmfs_fatfs.irx` was built with FAT16, FAT32 and exFAT enabled.

`smbman.irx` is pinned as well. It registers the `smb:` iomanX filesystem and
returns explicit regular-file/directory mode bits, avoiding the unreliable
HostFS metadata that made every entry look like a folder in some emulators.
It is loaded on demand only after the user opens `smb:`; normal boot never
initializes DEV9 or waits for a network.

The **ATA-Assault** pair exposes the internal PS2 ATA HDD through the modern
BDM stack, so a FAT/exFAT/MBR/GPT-formatted internal drive shows up as
`massN:` exactly like a USB mass-storage device. This mirrors the latest
OpenPS2Loader BDM ATA layout. `usbd.irx` below is the ATA-Assault *bundled*
BDM+BDMFS image (masqueraded as a USBD slot driver for external-loader
projects); we keep a verified copy pinned for reference/regression testing
but do **not** embed it, because the emulator already ships the equivalent
standalone modules (`bdm.irx` + `bdmfs_fatfs.irx`) plus the real FreeUsbd
mini USB host controller. Only the ATA transport half, `usbhdfsd.irx`, is
embedded and loaded lazily after the user enables the toggle and opens a
`massN:` drive; it then reuses the already-running BDM core to register the
internal HDD as an additional mass block device.

| File | Size | SHA-256 | Upstream source / license |
|---|---:|---|---|
| `sio2man.irx` | 5,241 bytes | `44748d1c67b22132c026dd05bb06314bcbb5318a3f12835fd388f4e2b3126986` | [`ps2dev/ps2sdk` commit `e228ff7b`](https://github.com/ps2dev/ps2sdk/tree/e228ff7b61a12ad1192a49338754534362a26e58/iop/sio/sio2man), Academic Free License 2.0 |
| `mcman.irx` | 72,101 bytes | `5bb7d332523add2a834374998e5dd6268c9b8a05dbff3346bff334e7d2023dd7` | [`ps2dev/ps2sdk` commit `e228ff7b`](https://github.com/ps2dev/ps2sdk/tree/e228ff7b61a12ad1192a49338754534362a26e58/iop/memorycard/mcman), Academic Free License 2.0 |
| `mcserv.irx` | 8,197 bytes | `9f1b2ee6eb5f7c1f56ce225100824d85bc615eda3dac4f6be00b5f9f6d3c8924` | [`ps2dev/ps2sdk` commit `e228ff7b`](https://github.com/ps2dev/ps2sdk/tree/e228ff7b61a12ad1192a49338754534362a26e58/iop/memorycard/mcserv), Academic Free License 2.0 |
| `padman.irx` | 36,741 bytes | `463fcb30cc4192dce7b4a0ffb8b24b47b3cb0057908c58e4542edebb91e6898e` | [`ps2dev/ps2sdk` commit `e228ff7b`](https://github.com/ps2dev/ps2sdk/tree/e228ff7b61a12ad1192a49338754534362a26e58/iop/input/padman), Academic Free License 2.0 |
| `mtapman.irx` | 7,781 bytes | `dd8e131cb1911d5649452814e880089e94e10606109355774b8d1c7cbf044bdc` | [`ps2dev/ps2sdk` commit `e228ff7b`](https://github.com/ps2dev/ps2sdk/tree/e228ff7b61a12ad1192a49338754534362a26e58/iop/sio/mtapman), Academic Free License 2.0 |
| `mmceman.irx` | 17,033 bytes | `e6f3695dc8cbc3c63de567f292100b8ebea9fe08e3a9440da40f7b4c508d9df7` | [`ps2-mmce/mmceman` commit `db3e93f0`](https://github.com/ps2-mmce/mmceman/tree/db3e93f0fdbcf882f88da110cbd9b7db188ec17a), MIT. Rebuilt from that commit and verified byte-identical to the current [`wLaunchELF_R3Z` reference binary](https://github.com/saildot4k/wLaunchELF_R3Z/blob/6f35bccab2eb1fce4a039b4edb9406bca96ef733/iop/__precompiled/mmceman.irx). |
| `mx4sio_bd.irx` | 11,841 bytes | `761972f0154e9fcf4fde2b7feb69a923bee53f8592fcc5553027be32f1c4a991` | [`ps2dev/ps2sdk` commit `e228ff7b`](https://github.com/ps2dev/ps2sdk/tree/e228ff7b61a12ad1192a49338754534362a26e58/iop/sio/mx4sio_bd), Academic Free License 2.0. Rebuilt from that commit and verified byte-identical to the official PS2DEV release binary (MX4SIO v1.2). |
| `cdfs_stream.irx` | 11,969 bytes | `f0a14edceb4876130508b0c18ba7c254ccbefa284858d87bc4baebc2ca78cdef` | Streaming fork of [`ps2dev/ps2sdk` commit `f08e889f`](https://github.com/ps2dev/ps2sdk/tree/f08e889fef8ab361f863c44ebe78212ced2839ca/iop/cdvd/cdfs), Academic Free License 2.0. Removes the 256-entry ceiling and adds bounded non-blocking CDVD waits, abort-on-timeout and strict I/O error propagation. Modified source and build notes are in `src/modules/cdfs_stream/`. |
| `usbd_mini.irx` | 21,877 bytes | `04e34ef54c5e2f12c299db01f93dd7fce940df944d1c1dcfa1a696fd7fdf24ca` | FreeUsbd restored by [`ps2dev/ps2sdk` commit `af80575`](https://github.com/ps2dev/ps2sdk/tree/af80575ff01e5bc61662cfb0aa756b9189e113e9/iop/usb/usbd_mini), based on the last pre-rewrite tree (`2dc6b32f`) with USBD 1.2 compatibility stubs; Academic Free License 2.0. Rebuilt from that exact commit. |
| `bdm.irx` | 10,745 bytes | `1059a8edc4cc9971ac6b462dd4420756163b87a5c017ddac36395e309d15e6c4` | [`ps2dev/ps2sdk` commit `f08e889f`](https://github.com/ps2dev/ps2sdk/tree/f08e889fef8ab361f863c44ebe78212ced2839ca/iop/fs/bdm), Academic Free License 2.0. |
| `bdmfs_fatfs.irx` | 36,877 bytes | `e76412c235d9ac5636e202ea860dddab22b2e394c27e6428724b29bdb7f8de76` | [`ps2dev/ps2sdk` commit `f08e889f`](https://github.com/ps2dev/ps2sdk/tree/f08e889fef8ab361f863c44ebe78212ced2839ca/iop/fs/bdmfs_fatfs), FatFs R0.15 + Academic Free License 2.0; FAT16/FAT32/exFAT enabled. **Rebuilt locally with `FF_LFN_UNICODE=2`** so long filenames are handed to the EE as UTF-8 (enables CJK ROM names); everything else matches the official build. |
| `usbmass_bd.irx` | 12,681 bytes | `fcfb583298cda02064b38146de465df0dc708d5b364f553700138384bc74276b` | [`ps2dev/ps2sdk` commit `f08e889f`](https://github.com/ps2dev/ps2sdk/tree/f08e889fef8ab361f863c44ebe78212ced2839ca/iop/usb/usbmass_bd), Academic Free License 2.0. |
| `smbman.irx` | 35,053 bytes | `af5e32b142cb505f4a49fc5f48ae45887edc720656e5fd039cd024abb0d7c20c` | [`ps2dev/ps2sdk` commit `f08e889f`](https://github.com/ps2dev/ps2sdk/tree/f08e889fef8ab361f863c44ebe78212ced2839ca/iop/network/smbman), Academic Free License 3.0. SMB1/NT1 client, loaded only for the read-only ROM browser. |
| `usbd.irx` (ATA-Assault BDM+BDMFS bundle, **reference only, not embedded**) | 42,749 bytes | `4E1D39365854747117A08F8A8D843937614590EA072CF2B70F6E4C2EBBF3C6EE` | [`saildot4k/ATA-Assault` release `latest`](https://github.com/saildot4k/ATA-Assault/releases/tag/latest), originally forked from PS2SDK BDM/BDMFS (Academic Free License 2.0). Masqueraded USBD-slot bundle: standalone BDM + BDMFS (FatFs with FAT16/FAT32/exFAT, MBR/GPT). Kept here for driver comparison; not embedded because we use the real FreeUsbd mini plus the separate verified `bdm.irx` + `bdmfs_fatfs.irx` instead. |
| `usbhdfsd.irx` (ATA-Assault DEV9 + ATAD¡úBDM transport, **embedded**) | 21,837 bytes | `1CC865CD09997BD708E9E102C0F0F1F17F352B7844B282FCFA6A00F38609DB9C` | [`saildot4k/ATA-Assault` release `latest`](https://github.com/saildot4k/ATA-Assault/releases/tag/latest), originally forked from PS2SDK DEV9/ATAD sources (Academic Free License 2.0). Starts DEV9 and connects the internal ATA (PATA) HDD to the running BDM core as a `massN:` block device, enabling FAT16/FAT32/exFAT reads and writes on the internal expansion-bay HDD. Loaded lazily and only when the explicit "ATA HDD (FAT/exFAT)" toggle is on. Mutually exclusive with the legacy APA `hdd0:` driver stack at runtime because both own the ATA hardware. |

Every path remains overridable from the Make command line for driver
development. A normal build fails clearly if any pinned file is missing.
