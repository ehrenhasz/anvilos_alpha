

#ifndef __MICROPY_INCLUDED_LIB_FLASH_H__
#define __MICROPY_INCLUDED_LIB_FLASH_H__

#include "nrfx_nvmc.h"

#if defined(NRF51)
#define FLASH_PAGESIZE (1024)

#elif defined(NRF52_SERIES)
#define FLASH_PAGESIZE (4096)

#elif defined(NRF91_SERIES)
#define FLASH_PAGESIZE (4096)

#else
#error Unknown chip
#endif

#define FLASH_IS_PAGE_ALIGNED(addr) (((uint32_t)(addr) & (FLASH_PAGESIZE - 1)) == 0)

#if BLUETOOTH_SD

typedef enum {
    FLASH_STATE_BUSY,
    FLASH_STATE_SUCCESS,
    FLASH_STATE_ERROR,
} flash_state_t;

void flash_page_erase(uint32_t address);
void flash_write_byte(uint32_t address, uint8_t value);
void flash_write_bytes(uint32_t address, const uint8_t *src, uint32_t num_bytes);
void flash_operation_finished(flash_state_t result);

#else

#define flash_page_erase nrfx_nvmc_page_erase
#define flash_write_byte nrfx_nvmc_byte_write
#define flash_write_bytes nrfx_nvmc_bytes_write

#endif

#endif 
