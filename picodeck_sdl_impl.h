/*
    PicoDeck SDL2 Implementation — Internal Header
    Software renderer for C-Dogs SDL on PicoDeck
*/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "picodeck_sdl.h"

/* Forward declare the PicoDeck API type */
struct PicoCalcAPI;

/* ── Pixel formats ───────────────────────────────────────────────
   The shim renders in RGB565 (host byte order — display->drawImageNN
   byte-swaps to the panel's big-endian order itself).  Textures that
   BORROW Pic->Data mirror whatever format that Pic settled on --
   PICODECK_TEXFMT_RGB565 for "final" pics (Task 2) and style pics (Task 3);
   chars/ pics (Task 4, corrected by Amendment B) are decided per pic by a
   load-time pre-pass (pic.c's PicLoadClassifyCharsFormat) and land on
   PICODECK_TEXFMT_LA8 (most of them -- pure luma+alpha is lossless), RGB565
   (a few with real colour but no recognised colour key), or ARGB8888 (a
   few with both real colour AND a colour key, or partial alpha -- gun/hat
   sheets, mostly). Textures the shim OWNS (the window-sized render
   targets) are always RGB565.

   Stage 2D (Task 1): the LA8 and ARGB8888 branches of SDL_RenderCopyEx can
   additionally recolour channel-index alpha values (246-255) through a
   256-entry CharColors LUT set via PicodeckBlitSetCharColors (declared in
   picodeck_charcolors.h -- kept out of this header and out of picodeck_sdl.h to
   avoid an include cycle through blit.h; see that header's comment).
   Dormant -- byte-identical to pre-2D output -- until a caller sets one. */
#define PICODECK_TEXFMT_ARGB8888  0   /* 4 bytes/px, borrowed Pic->Data  */
#define PICODECK_TEXFMT_RGB565    1   /* 2 bytes/px, shim-owned OR borrowed */
#define PICODECK_TEXFMT_LA8       2   /* 2 bytes/px, borrowed Pic->Data (chars/):
                                      low byte L, high byte A (0 = transparent,
                                      else a channel index or 255 = literal) */

/* RGB565 has no alpha channel.  This sentinel (pure magenta: r=31,
   g=0, b=31) marks a transparent pixel.  It decodes to exactly the
   (r,g,b,a) = (0,0,0,0) that an ARGB alpha-0 pixel produced, so the
   blended path and the opaque path both behave as they did in 32-bit. */
#define PICODECK_RGB565_CKEY  ((uint16_t)0xF81F)

/* Pack 8-bit channels into host-order RGB565, nudging away from the
   colour key so an opaque magenta never reads back as transparent. */
static inline uint16_t picodeck_pack565(uint32_t r, uint32_t g, uint32_t b) {
    uint16_t p = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
    if (p == PICODECK_RGB565_CKEY) p = (uint16_t)(PICODECK_RGB565_CKEY - 1);
    return p;
}

/* ARGB8888 -> RGB565.  Alpha is thresholded at 128: the design measured
   99.76% of source pixels as fully transparent or fully opaque, and the
   one buffer the shim converts wholesale (g->buf) is provably binary —
   BlitClearBuf memsets it to 0 and BlitFillBuf writes opaque colours,
   and nothing else writes it. */
static inline uint16_t picodeck_argb_to_565(uint32_t argb) {
    if (((argb >> 24) & 0xFF) < 128) return PICODECK_RGB565_CKEY;
    return picodeck_pack565((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
}

/* RGB565 -> 8-bit channels, replicating high bits into the low ones so
   full-scale stays full-scale (31 -> 255, not 248). */
static inline void picodeck_unpack565(uint16_t p, uint32_t *r, uint32_t *g,
                                   uint32_t *b) {
    const uint32_t r5 = (p >> 11) & 0x1Fu;
    const uint32_t g6 = (p >> 5) & 0x3Fu;
    const uint32_t b5 = p & 0x1Fu;
    *r = (r5 << 3) | (r5 >> 2);
    *g = (g6 << 2) | (g6 >> 4);
    *b = (b5 << 3) | (b5 >> 2);
}

/* ── Internal texture structure ──────────────────────────────── */
typedef struct PicodeckTexture {
    int w, h, pitch;       /* pitch in bytes: w*4 ARGB8888, w*2 RGB565 */
    void *pixels;          /* PICODECK_TEXFMT_* selects how to read these */
    uint8_t fmt;           /* PICODECK_TEXFMT_ARGB8888 | PICODECK_TEXFMT_RGB565 */
    int access;            /* SDL_TEXTUREACCESS_* */
    uint8_t r_mod, g_mod, b_mod;  /* color mod */
    uint8_t a_mod;         /* alpha mod */
    int blend_mode;
    bool owns_pixels;      /* free pixels on destroy? */
    bool locked;
} PicodeckTexture;

/* ── Internal renderer structure ─────────────────────────────── */
typedef struct PicodeckRenderer {
    uint16_t *framebuf;        /* default render target (RGB565, host order) */
    int fb_w, fb_h;            /* framebuffer dimensions */
    uint8_t draw_r, draw_g, draw_b, draw_a;
    int draw_blend_mode;
    PicodeckTexture *render_target;  /* non-NULL = rendering to texture */
    int logical_w, logical_h;
} PicodeckRenderer;

/* ── Event queue ─────────────────────────────────────────────── */
#define PICODECK_EVENT_QUEUE_SIZE 32

/* ── Initialize the PicoDeck SDL layer ──────────────────────────── */
void picodeck_sdl_init(const struct PicoCalcAPI *api);

/* ── Access to global renderer (for SDL_RenderPresent) ───────── */
extern const struct PicoCalcAPI *g_picodeck_api;
