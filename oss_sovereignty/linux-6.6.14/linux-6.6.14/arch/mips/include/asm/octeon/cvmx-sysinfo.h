#ifndef __CVMX_SYSINFO_H__
#define __CVMX_SYSINFO_H__
#include "cvmx-coremask.h"
#define OCTEON_SERIAL_LEN 20
struct cvmx_sysinfo {
	uint64_t system_dram_size;
	uint64_t phy_mem_desc_addr;
	uint64_t stack_top;
	uint64_t heap_base;
	uint32_t stack_size;
	uint32_t heap_size;
	struct cvmx_coremask core_mask;
	uint32_t init_core;
	uint64_t exception_base_addr;
	uint32_t cpu_clock_hz;
	uint32_t dram_data_rate_hz;
	uint16_t board_type;
	uint8_t board_rev_major;
	uint8_t board_rev_minor;
	uint8_t mac_addr_base[6];
	uint8_t mac_addr_count;
	char board_serial_number[OCTEON_SERIAL_LEN];
	uint64_t compact_flash_common_base_addr;
	uint64_t compact_flash_attribute_base_addr;
	uint64_t led_display_base_addr;
	uint32_t dfa_ref_clock_hz;
	uint32_t bootloader_config_flags;
	uint8_t console_uart_num;
};
extern struct cvmx_sysinfo *cvmx_sysinfo_get(void);
#endif  
