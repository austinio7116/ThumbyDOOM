#ifndef _HW_FLASH_H
#define _HW_FLASH_H
#include <stdint.h>
#define FLASH_SECTOR_SIZE (1u << 12)
#define XIP_BASE 0x10000000u
static inline void flash_range_program(uint32_t off, const uint8_t *d, size_t l) { (void)off; (void)d; (void)l; }
static inline void flash_range_erase(uint32_t off, size_t l) { (void)off; (void)l; }
#endif
