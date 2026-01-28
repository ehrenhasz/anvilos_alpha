#ifndef __MACH_CORE_H
#define __MACH_CORE_H
#define SOCFPGA_RSTMGR_CTRL	0x04
#define SOCFPGA_RSTMGR_MODMPURST	0x10
#define SOCFPGA_RSTMGR_MODPERRST	0x14
#define SOCFPGA_RSTMGR_BRGMODRST	0x1c
#define SOCFPGA_A10_RSTMGR_CTRL		0xC
#define SOCFPGA_A10_RSTMGR_MODMPURST	0x20
#define RSTMGR_CTRL_SWCOLDRSTREQ	0x1	 
#define RSTMGR_CTRL_SWWARMRSTREQ	0x2	 
#define RSTMGR_MPUMODRST_CPU1		0x2      
void socfpga_init_l2_ecc(void);
void socfpga_init_ocram_ecc(void);
void socfpga_init_arria10_l2_ecc(void);
void socfpga_init_arria10_ocram_ecc(void);
extern void __iomem *sys_manager_base_addr;
extern void __iomem *rst_manager_base_addr;
extern void __iomem *sdr_ctl_base_addr;
u32 socfpga_sdram_self_refresh(u32 sdr_base);
extern unsigned int socfpga_sdram_self_refresh_sz;
extern char secondary_trampoline[], secondary_trampoline_end[];
extern unsigned long socfpga_cpu1start_addr;
#define SOCFPGA_SCU_VIRT_BASE   0xfee00000
#endif
