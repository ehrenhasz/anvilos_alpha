

#ifndef MICROPY_INCLUDED_MIMXRT_FLASH_H
#define MICROPY_INCLUDED_MIMXRT_FLASH_H

#include BOARD_FLASH_OPS_HEADER_H

#define SECTOR_SIZE_BYTES (qspiflash_config.sectorSize)
#define PAGE_SIZE_BYTES (qspiflash_config.pageSize)
#define BLOCK_SIZE_BYTES (qspiflash_config.blockSize)

#define SECTOR_SIZE_BYTES (qspiflash_config.sectorSize)
#define PAGE_SIZE_BYTES (qspiflash_config.pageSize)

#ifndef MICROPY_HW_FLASH_STORAGE_BYTES
#define MICROPY_HW_FLASH_STORAGE_BYTES (((uint32_t)&__vfs_end) - ((uint32_t)&__vfs_start))
#endif

#ifndef MICROPY_HW_FLASH_STORAGE_BASE
#define MICROPY_HW_FLASH_STORAGE_BASE (((uint32_t)&__vfs_start) - ((uint32_t)&__flash_start))
#endif


extern uint8_t __vfs_start;
extern uint8_t __vfs_end;
extern uint8_t __flash_start;

void flash_init(void);
status_t flash_erase_sector(uint32_t erase_addr);
status_t flash_erase_block(uint32_t erase_addr);
void flash_read_block(uint32_t src_addr, uint8_t *dest, uint32_t length);
status_t flash_write_block(uint32_t dest_addr, const uint8_t *src, uint32_t length);

#endif 
