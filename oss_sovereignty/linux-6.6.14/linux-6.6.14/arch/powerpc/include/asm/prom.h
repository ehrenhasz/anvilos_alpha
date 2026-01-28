#ifndef _POWERPC_PROM_H
#define _POWERPC_PROM_H
#ifdef __KERNEL__
#include <linux/types.h>
#include <asm/firmware.h>
struct device_node;
struct property;
#define OF_DT_BEGIN_NODE	0x1		 
#define OF_DT_END_NODE		0x2		 
#define OF_DT_PROP		0x3		 
#define OF_DT_NOP		0x4		 
#define OF_DT_END		0x9
#define OF_DT_VERSION		0x10
struct boot_param_header {
	__be32	magic;			 
	__be32	totalsize;		 
	__be32	off_dt_struct;		 
	__be32	off_dt_strings;		 
	__be32	off_mem_rsvmap;		 
	__be32	version;		 
	__be32	last_comp_version;	 
	__be32	boot_cpuid_phys;	 
	__be32	dt_strings_size;	 
	__be32	dt_struct_size;		 
};
void of_parse_dma_window(struct device_node *dn, const __be32 *dma_window,
			 unsigned long *busno, unsigned long *phys,
			 unsigned long *size);
extern void of_instantiate_rtc(void);
extern int of_get_ibm_chip_id(struct device_node *np);
struct of_drc_info {
	char *drc_type;
	char *drc_name_prefix;
	u32 drc_index_start;
	u32 drc_name_suffix_start;
	u32 num_sequential_elems;
	u32 sequential_inc;
	u32 drc_power_domain;
	u32 last_drc_index;
};
extern int of_read_drc_info_cell(struct property **prop,
			const __be32 **curval, struct of_drc_info *data);
extern unsigned int boot_cpu_node_count;
#define OV_IGNORE		0x80	 
#define OV_CESSATION_POLICY	0x40	 
#define OV1_PPC_2_00		0x80	 
#define OV1_PPC_2_01		0x40	 
#define OV1_PPC_2_02		0x20	 
#define OV1_PPC_2_03		0x10	 
#define OV1_PPC_2_04		0x08	 
#define OV1_PPC_2_05		0x04	 
#define OV1_PPC_2_06		0x02	 
#define OV1_PPC_2_07		0x01	 
#define OV1_PPC_3_00		0x80	 
#define OV1_PPC_3_1			0x40	 
#define OV2_REAL_MODE		0x20	 
#define OV3_FP			0x80	 
#define OV3_VMX			0x40	 
#define OV3_DFP			0x20	 
#define OV4_MIN_ENT_CAP		0x01	 
#define OV5_FEAT(x)	((x) & 0xff)
#define OV5_INDX(x)	((x) >> 8)
#define OV5_LPAR		0x0280	 
#define OV5_SPLPAR		0x0240	 
#define OV5_DRCONF_MEMORY	0x0220
#define OV5_LARGE_PAGES		0x0210	 
#define OV5_DONATE_DEDICATE_CPU	0x0202	 
#define OV5_MSI			0x0201	 
#define OV5_CMO			0x0480	 
#define OV5_XCMO		0x0440	 
#define OV5_FORM1_AFFINITY	0x0580	 
#define OV5_PRRN		0x0540	 
#define OV5_FORM2_AFFINITY	0x0520	 
#define OV5_HP_EVT		0x0604	 
#define OV5_RESIZE_HPT		0x0601	 
#define OV5_PFO_HW_RNG		0x1180	 
#define OV5_PFO_HW_842		0x1140	 
#define OV5_PFO_HW_ENCR		0x1120	 
#define OV5_SUB_PROCESSORS	0x1501	 
#define OV5_DRMEM_V2		0x1680	 
#define OV5_XIVE_SUPPORT	0x17C0	 
#define OV5_XIVE_LEGACY		0x1700	 
#define OV5_XIVE_EXPLOIT	0x1740	 
#define OV5_XIVE_EITHER		0x1780	 
#define OV5_MMU_SUPPORT		0x18C0	 
#define OV5_MMU_HASH		0x1800	 
#define OV5_MMU_RADIX		0x1840	 
#define OV5_MMU_EITHER		0x1880	 
#define OV5_MMU_DYNAMIC		0x18C0	 
#define OV5_NMMU		0x1820	 
#define OV5_HASH_SEG_TBL	0x1980	 
#define OV5_HASH_GTSE		0x1940	 
#define OV5_RADIX_GTSE		0x1A40	 
#define OV5_DRC_INFO		0x1640	 
#define OV6_LINUX		0x02	 
#endif  
#endif  
