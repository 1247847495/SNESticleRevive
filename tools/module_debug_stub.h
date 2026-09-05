/* PC-only stub of the PS2SDK IOP header module_debug.h.
   The ps2sdk copy of FatFs R0.16 (common/external_deps/fatfs) pulls this
   header in for M_DEBUG logging and uses the IOP-only `u64` typedef in its
   mount_volume() debug patch. On the PC harness both macros are no-ops
   (macro arguments are not evaluated, so winsect_u32[] is never referenced)
   and u64 maps to unsigned long long. */
#ifndef MODULE_DEBUG_STUB_H
#define MODULE_DEBUG_STUB_H

#ifndef _FFTEST_U64_DEFINED
#define _FFTEST_U64_DEFINED
typedef unsigned long long u64;
#endif

#define M_DEBUG(format, args...)   do { } while (0)
#define DEBUG_U64_2XU32(val)       do { } while (0)

#endif /* MODULE_DEBUG_STUB_H */
