
#ifndef MICROPY_INCLUDED_MIMXRT_HAL_FLEXSPI_NOR_FLASH_H
#define MICROPY_INCLUDED_MIMXRT_HAL_FLEXSPI_NOR_FLASH_H

#include "fsl_flexspi.h"
#include "mpconfigboard.h"
#include BOARD_FLASH_CONFIG_HEADER_H

#if defined MICROPY_HW_FLASH_INTERNAL
#define BOARD_FLEX_SPI FLEXSPI2
#define BOARD_FLEX_SPI_ADDR_BASE FlexSPI2_AMBA_BASE
#elif defined MIMXRT117x_SERIES
#define BOARD_FLEX_SPI FLEXSPI1
#define BOARD_FLEX_SPI_ADDR_BASE FlexSPI1_AMBA_BASE
#else
#define BOARD_FLEX_SPI FLEXSPI
#define BOARD_FLEX_SPI_ADDR_BASE FlexSPI_AMBA_BASE
#endif


extern flexspi_nor_config_t qspiflash_config;

status_t flexspi_nor_get_vendor_id(FLEXSPI_Type *base, uint8_t *vendorId);
status_t flexspi_nor_init(void);
void flexspi_nor_update_lut(void);
status_t flexspi_nor_flash_erase_sector(FLEXSPI_Type *base, uint32_t address);
status_t flexspi_nor_flash_erase_block(FLEXSPI_Type *base, uint32_t address);
status_t flexspi_nor_flash_page_program(FLEXSPI_Type *base, uint32_t address, const uint32_t *src, uint32_t size);

#endif 
