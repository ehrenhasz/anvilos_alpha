#define COMMON_USER_BOOKE	(PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU | \
				 PPC_FEATURE_BOOKE)
static struct cpu_spec cpu_specs[] __initdata = {
	{  
		.pvr_mask		= 0xffffffff,
		.pvr_value		= 0x11a52080,
		.cpu_name		= "476",
		.cpu_features		= CPU_FTRS_47X | CPU_FTR_476_DD2,
		.cpu_user_features	= COMMON_USER_BOOKE | PPC_FEATURE_HAS_FPU,
		.mmu_features		= MMU_FTR_TYPE_47x | MMU_FTR_USE_TLBIVAX_BCAST |
					  MMU_FTR_LOCK_BCAST_INVAL,
		.icache_bsize		= 32,
		.dcache_bsize		= 128,
		.machine_check		= machine_check_47x,
		.platform		= "ppc470",
	},
	{  
		.pvr_mask		= 0xffff0000,
		.pvr_value		= 0x7ff50000,
		.cpu_name		= "476fpe",
		.cpu_features		= CPU_FTRS_47X | CPU_FTR_476_DD2,
		.cpu_user_features	= COMMON_USER_BOOKE | PPC_FEATURE_HAS_FPU,
		.mmu_features		= MMU_FTR_TYPE_47x | MMU_FTR_USE_TLBIVAX_BCAST |
					  MMU_FTR_LOCK_BCAST_INVAL,
		.icache_bsize		= 32,
		.dcache_bsize		= 128,
		.machine_check		= machine_check_47x,
		.platform		= "ppc470",
	},
	{  
		.pvr_mask		= 0xffff0000,
		.pvr_value		= 0x00050000,
		.cpu_name		= "476",
		.cpu_features		= CPU_FTRS_47X,
		.cpu_user_features	= COMMON_USER_BOOKE | PPC_FEATURE_HAS_FPU,
		.mmu_features		= MMU_FTR_TYPE_47x | MMU_FTR_USE_TLBIVAX_BCAST |
					  MMU_FTR_LOCK_BCAST_INVAL,
		.icache_bsize		= 32,
		.dcache_bsize		= 128,
		.machine_check		= machine_check_47x,
		.platform		= "ppc470",
	},
	{  
		.pvr_mask		= 0xffff0000,
		.pvr_value		= 0x11a50000,
		.cpu_name		= "476",
		.cpu_features		= CPU_FTRS_47X,
		.cpu_user_features	= COMMON_USER_BOOKE | PPC_FEATURE_HAS_FPU,
		.mmu_features		= MMU_FTR_TYPE_47x | MMU_FTR_USE_TLBIVAX_BCAST |
					  MMU_FTR_LOCK_BCAST_INVAL,
		.icache_bsize		= 32,
		.dcache_bsize		= 128,
		.machine_check		= machine_check_47x,
		.platform		= "ppc470",
	},
	{	 
		.pvr_mask		= 0x00000000,
		.pvr_value		= 0x00000000,
		.cpu_name		= "(generic 47x PPC)",
		.cpu_features		= CPU_FTRS_47X,
		.cpu_user_features	= COMMON_USER_BOOKE,
		.mmu_features		= MMU_FTR_TYPE_47x,
		.icache_bsize		= 32,
		.dcache_bsize		= 128,
		.machine_check		= machine_check_47x,
		.platform		= "ppc470",
	}
};
