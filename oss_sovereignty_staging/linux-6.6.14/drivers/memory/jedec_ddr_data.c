
 

#include <linux/export.h>

#include "jedec_ddr.h"

 
const struct lpddr2_addressing
	lpddr2_jedec_addressing_table[NUM_DDR_ADDR_TABLE_ENTRIES] = {
	{B4, T_REFI_15_6, T_RFC_90},  
	{B4, T_REFI_15_6, T_RFC_90},  
	{B4, T_REFI_7_8,  T_RFC_90},  
	{B4, T_REFI_7_8,  T_RFC_90},  
	{B8, T_REFI_7_8, T_RFC_130},  
	{B8, T_REFI_3_9, T_RFC_130},  
	{B8, T_REFI_3_9, T_RFC_130},  
	{B8, T_REFI_3_9, T_RFC_210},  
	{B4, T_REFI_7_8, T_RFC_130},  
	{B4, T_REFI_3_9, T_RFC_130},  
};
EXPORT_SYMBOL_GPL(lpddr2_jedec_addressing_table);

 
const struct lpddr2_timings
	lpddr2_jedec_timings[NUM_DDR_TIMING_TABLE_ENTRIES] = {
	 
	[0] = {
		.max_freq	= 200000000,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 10000,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tDQSCK_max_derated = 6000,
	},
	 
	[1] = {
		.max_freq	= 266666666,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 7500,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tDQSCK_max_derated = 6000,
	},
	 
	[2] = {
		.max_freq	= 400000000,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 7500,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tDQSCK_max_derated = 6000,
	},
	 
	[3] = {
		.max_freq	= 533333333,
		.min_freq	= 10000000,
		.tRPab		= 21000,
		.tRCD		= 18000,
		.tWR		= 15000,
		.tRAS_min	= 42000,
		.tRRD		= 10000,
		.tWTR		= 7500,
		.tXP		= 7500,
		.tRTP		= 7500,
		.tCKESR		= 15000,
		.tDQSCK_max	= 5500,
		.tFAW		= 50000,
		.tZQCS		= 90000,
		.tZQCL		= 360000,
		.tZQinit	= 1000000,
		.tRAS_max_ns	= 70000,
		.tDQSCK_max_derated = 5620,
	},
};
EXPORT_SYMBOL_GPL(lpddr2_jedec_timings);

const struct lpddr2_min_tck lpddr2_jedec_min_tck = {
	.tRPab		= 3,
	.tRCD		= 3,
	.tWR		= 3,
	.tRASmin	= 3,
	.tRRD		= 2,
	.tWTR		= 2,
	.tXP		= 2,
	.tRTP		= 2,
	.tCKE		= 3,
	.tCKESR		= 3,
	.tFAW		= 8
};
EXPORT_SYMBOL_GPL(lpddr2_jedec_min_tck);

const char *lpddr2_jedec_manufacturer(unsigned int manufacturer_id)
{
	switch (manufacturer_id) {
	case LPDDR2_MANID_SAMSUNG:
		return "Samsung";
	case LPDDR2_MANID_QIMONDA:
		return "Qimonda";
	case LPDDR2_MANID_ELPIDA:
		return "Elpida";
	case LPDDR2_MANID_ETRON:
		return "Etron";
	case LPDDR2_MANID_NANYA:
		return "Nanya";
	case LPDDR2_MANID_HYNIX:
		return "Hynix";
	case LPDDR2_MANID_MOSEL:
		return "Mosel";
	case LPDDR2_MANID_WINBOND:
		return "Winbond";
	case LPDDR2_MANID_ESMT:
		return "ESMT";
	case LPDDR2_MANID_SPANSION:
		return "Spansion";
	case LPDDR2_MANID_SST:
		return "SST";
	case LPDDR2_MANID_ZMOS:
		return "ZMOS";
	case LPDDR2_MANID_INTEL:
		return "Intel";
	case LPDDR2_MANID_NUMONYX:
		return "Numonyx";
	case LPDDR2_MANID_MICRON:
		return "Micron";
	default:
		break;
	}

	return "invalid";
}
EXPORT_SYMBOL_GPL(lpddr2_jedec_manufacturer);
