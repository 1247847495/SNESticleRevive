/* hdpack.h: HD texture pack support for the snes9x2010 renderer.
 *
 * The architecture follows Mesen's NES HD packs (HdNesPack /
 * HdPackLoader / hires.txt), adapted to the SNES PPU and this
 * renderer: tile draws are intercepted through the GFX.DrawTile* /
 * GFX.DrawClippedTile* function pointers, the pixels each 8x8 tile
 * actually contributed to the main screen are recovered from the
 * depth buffer, and a post-frame compositor emits an integer-scaled
 * RGB565 frame in which recognized tiles are replaced by PNG art.
 *
 * A pack lives in "<rom directory>/<rom name>.hdpack/hires.txt":
 *
 *   <ver>1
 *   <scale>N                 integer 1..4
 *   <img>sheet.png           images are indexed in order of appearance;
                            PNG, WebP and DDS files are supported,
                            selected by extension
 *   <tile>img,bpp,DATA,PAL,x,y
 *        img  = image index
 *        bpp  = 2, 4 or 8
 *        DATA = 128 hex chars: the 8x8 tile as 64 palette-index bytes,
 *               row-major, unflipped
 *        PAL  = 8 hex chars: CRC32 of the tile's palette block as
 *               drawn (2^bpp RGB565 values, little-endian), or the
 *               word "default" to match the tile under any palette
 *        x,y  = top-left corner of the scale*8 x scale*8 replacement
 *               inside the image
 *
 * Setting the environment variable SNES_HD_RECORD=1 records every
 * unique tile/palette drawn into a new pack skeleton (sheet PNG at
 * native resolution plus matching hires.txt) so that artists can
 * upscale the sheet in place and raise <scale>. */
#ifndef _HDPACK_H_
#define _HDPACK_H_

#include "port.h"

/* Load/unload; rom_path is the full content path. Returns 1 when a
 * pack (or the recorder) is active afterwards. */
#ifdef PS2
/* AURORA_SNES9X2010_V2_PS2LEAN_20260824 -- compile-time dead HD Pack API on standalone PS2. */
static inline int S9xHdPackInit(const char *rom_path) { (void)rom_path; return 0; }
static inline void S9xHdPackDeinit(void) {}
static inline int S9xHdPackActive(void) { return 0; }
static inline int S9xHdPackRecording(void) { return 0; }
static inline uint32_t S9xHdPackScale(void) { return 1; }
static inline void S9xHdPackFrameBegin(void) {}
static inline void S9xHdPackWrapRenderers(int normal1x1) { (void)normal1x1; }
static inline uint16_t *S9xHdPackComposite(int width, int height,
      int *out_width, int *out_height, int *out_pitch)
{
    (void)width; (void)height; (void)out_width; (void)out_height; (void)out_pitch;
    return (uint16_t *)0;
}
#else
int  S9xHdPackInit (const char *rom_path);
void S9xHdPackDeinit (void);
int  S9xHdPackActive (void);
int  S9xHdPackRecording (void);
uint32_t S9xHdPackScale (void);
void S9xHdPackFrameBegin (void);
void S9xHdPackWrapRenderers (int normal1x1);
uint16_t *S9xHdPackComposite (int width, int height,
      int *out_width, int *out_height, int *out_pitch);
#endif

#endif
