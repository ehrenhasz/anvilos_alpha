
#ifndef MICROPY_INCLUDED_STM32_FLASH_H
#define MICROPY_INCLUDED_STM32_FLASH_H

bool flash_is_valid_addr(uint32_t addr);
int32_t flash_get_sector_info(uint32_t addr, uint32_t *start_addr, uint32_t *size);
int flash_erase(uint32_t flash_dest);
int flash_write(uint32_t flash_dest, const uint32_t *src, uint32_t num_word32);

#endif 
