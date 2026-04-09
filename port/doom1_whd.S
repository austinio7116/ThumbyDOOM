/*
 * Embed the pre-processed doom1.whd blob directly into flash.
 *
 * This file is assembled with -I<path-to-whd-dir> so .incbin can
 * find doom1.whd without absolute paths. The blob is XIP-mapped
 * at runtime; w_file_thumby.c hands its address to Doom's WAD
 * loader via TINY_WAD_ADDR.
 */
    .section .rodata.doom1_whd, "a"
    .align 4
    .global doom1_whd_data
    .global doom1_whd_data_end
    .global doom1_whd_data_size
doom1_whd_data:
    .incbin "doom1.whd"
doom1_whd_data_end:

    .align 4
doom1_whd_data_size:
    .word doom1_whd_data_end - doom1_whd_data
