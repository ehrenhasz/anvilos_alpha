#ifndef PP_HWMGR_PPT_H
#define PP_HWMGR_PPT_H
#include "hardwaremanager.h"
#include "smumgr.h"
#include "atom-types.h"
struct phm_ppt_v1_clock_voltage_dependency_record {
	uint32_t clk;
	uint8_t  vddInd;
	uint8_t  vddciInd;
	uint8_t  mvddInd;
	uint16_t vdd_offset;
	uint16_t vddc;
	uint16_t vddgfx;
	uint16_t vddci;
	uint16_t mvdd;
	uint8_t  phases;
	uint8_t  cks_enable;
	uint8_t  cks_voffset;
	uint32_t sclk_offset;
};
typedef struct phm_ppt_v1_clock_voltage_dependency_record phm_ppt_v1_clock_voltage_dependency_record;
struct phm_ppt_v1_clock_voltage_dependency_table {
	uint32_t count;                                             
	phm_ppt_v1_clock_voltage_dependency_record entries[];	    
};
typedef struct phm_ppt_v1_clock_voltage_dependency_table phm_ppt_v1_clock_voltage_dependency_table;
struct phm_ppt_v1_mm_clock_voltage_dependency_record {
	uint32_t  dclk;                                               
	uint32_t  vclk;                                               
	uint32_t  eclk;                                               
	uint32_t  aclk;                                               
	uint32_t  samclock;                                           
	uint8_t	vddcInd;
	uint16_t vddgfx_offset;
	uint16_t vddc;
	uint16_t vddgfx;
	uint8_t phases;
};
typedef struct phm_ppt_v1_mm_clock_voltage_dependency_record phm_ppt_v1_mm_clock_voltage_dependency_record;
struct phm_ppt_v1_mm_clock_voltage_dependency_table {
	uint32_t count;													 
	phm_ppt_v1_mm_clock_voltage_dependency_record entries[];		 
};
typedef struct phm_ppt_v1_mm_clock_voltage_dependency_table phm_ppt_v1_mm_clock_voltage_dependency_table;
struct phm_ppt_v1_voltage_lookup_record {
	uint16_t us_calculated;
	uint16_t us_vdd;												 
	uint16_t us_cac_low;
	uint16_t us_cac_mid;
	uint16_t us_cac_high;
};
typedef struct phm_ppt_v1_voltage_lookup_record phm_ppt_v1_voltage_lookup_record;
struct phm_ppt_v1_voltage_lookup_table {
	uint32_t count;
	phm_ppt_v1_voltage_lookup_record entries[];     
};
typedef struct phm_ppt_v1_voltage_lookup_table phm_ppt_v1_voltage_lookup_table;
struct phm_ppt_v1_pcie_record {
	uint8_t gen_speed;
	uint8_t lane_width;
	uint16_t usreserved;
	uint16_t reserved;
	uint32_t pcie_sclk;
};
typedef struct phm_ppt_v1_pcie_record phm_ppt_v1_pcie_record;
struct phm_ppt_v1_pcie_table {
	uint32_t count;                                             
	phm_ppt_v1_pcie_record entries[];			    
};
typedef struct phm_ppt_v1_pcie_table phm_ppt_v1_pcie_table;
struct phm_ppt_v1_gpio_table {
	uint8_t vrhot_triggered_sclk_dpm_index;            
};
typedef struct phm_ppt_v1_gpio_table phm_ppt_v1_gpio_table;
#endif
