/*
 * ThumbyDOOM — WHD WAD loader.
 *
 * The pre-processed WHD blob is linked into flash as
 * `doom1_whd_data` (see port/doom1_whd.S). This file wraps it in a
 * wad_file_t so Doom's W_Init* machinery can consume it directly.
 *
 * Modeled on vendor/rp2040-doom/src/w_file_memory.c but without the
 * TINY_WAD_ADDR indirection — we use the linker symbol directly so
 * the WHD moves with firmware placement and no UF2 merging is needed.
 */

#include "config.h"
#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"
#include "w_wad.h"

extern const uint8_t doom1_whd_data[];

extern const wad_file_class_t memory_wad_file;

static const wad_file_t thumby_wad = {
    .file_class = &memory_wad_file,
    .length     = 0,
    .mapped     = doom1_whd_data,
    .path       = "doom1.whd",
};

static wad_file_t *W_Thumby_OpenFile(const char *path)
{
    const uint8_t *p = doom1_whd_data;
    if (p[0] != 'I' || p[1] != 'W' || p[2] != 'H' || p[3] != 'D') {
        I_Error("WHD blob missing — bad magic %02x%02x%02x%02x\n",
                p[0], p[1], p[2], p[3]);
    }
    return (wad_file_t *)&thumby_wad;
}

static void W_Thumby_CloseFile(wad_file_t *wad) { }

static size_t W_Thumby_Read(wad_file_t *wad, unsigned int offset,
                            void *buffer, size_t buffer_len)
{
    memcpy(buffer, wad->mapped + offset, buffer_len);
    return buffer_len;
}

const wad_file_class_t memory_wad_file =
{
    W_Thumby_OpenFile,
    W_Thumby_CloseFile,
    W_Thumby_Read,
};
