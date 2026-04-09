/* ThumbyDOOM — picoflash stub. Phase 1 has no save support;
 * picoflash_sector_program() is a no-op. Phase 4 will hook this
 * to hardware_flash for savegames. */
#pragma once

#include <stdint.h>

#define FLASH_SECTOR_SIZE (1u << 12)

void picoflash_sector_program(uint32_t flash_offs, const uint8_t *data);
