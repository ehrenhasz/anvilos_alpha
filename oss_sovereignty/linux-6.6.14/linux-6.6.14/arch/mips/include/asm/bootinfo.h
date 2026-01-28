#ifndef _ASM_BOOTINFO_H
#define _ASM_BOOTINFO_H
#include <linux/types.h>
#include <asm/setup.h>
#define  MACH_UNKNOWN		0	 
#define  MACH_DSUNKNOWN		0
#define  MACH_DS23100		1	 
#define  MACH_DS5100		2	 
#define  MACH_DS5000_200	3	 
#define  MACH_DS5000_1XX	4	 
#define  MACH_DS5000_XX		5	 
#define  MACH_DS5000_2X0	6	 
#define  MACH_DS5400		7	 
#define  MACH_DS5500		8	 
#define  MACH_DS5800		9	 
#define  MACH_DS5900		10	 
#define MACH_MIKROTIK_RB532	0	 
#define MACH_MIKROTIK_RB532A	1	 
enum loongson2ef_machine_type {
	MACH_LOONGSON_UNKNOWN,
	MACH_LEMOTE_FL2E,
	MACH_LEMOTE_FL2F,
	MACH_LEMOTE_ML2F7,
	MACH_LEMOTE_YL2F89,
	MACH_DEXXON_GDIUM2F10,
	MACH_LEMOTE_NAS,
	MACH_LEMOTE_LL2F,
	MACH_LOONGSON_END
};
enum ingenic_machine_type {
	MACH_INGENIC_UNKNOWN,
	MACH_INGENIC_JZ4720,
	MACH_INGENIC_JZ4725,
	MACH_INGENIC_JZ4725B,
	MACH_INGENIC_JZ4730,
	MACH_INGENIC_JZ4740,
	MACH_INGENIC_JZ4750,
	MACH_INGENIC_JZ4755,
	MACH_INGENIC_JZ4760,
	MACH_INGENIC_JZ4760B,
	MACH_INGENIC_JZ4770,
	MACH_INGENIC_JZ4775,
	MACH_INGENIC_JZ4780,
	MACH_INGENIC_X1000,
	MACH_INGENIC_X1000E,
	MACH_INGENIC_X1830,
	MACH_INGENIC_X2000,
	MACH_INGENIC_X2000E,
	MACH_INGENIC_X2000H,
	MACH_INGENIC_X2100,
};
extern char *system_type;
const char *get_system_type(void);
extern unsigned long mips_machtype;
extern void detect_memory_region(phys_addr_t start, phys_addr_t sz_min,  phys_addr_t sz_max);
extern void prom_init(void);
extern void prom_free_prom_memory(void);
extern void prom_cleanup(void);
extern void free_init_pages(const char *what,
			    unsigned long begin, unsigned long end);
extern void (*free_init_pages_eva)(void *begin, void *end);
extern char arcs_cmdline[COMMAND_LINE_SIZE];
extern unsigned long fw_arg0, fw_arg1, fw_arg2, fw_arg3;
#ifdef CONFIG_USE_OF
#include <linux/libfdt.h>
#include <linux/of_fdt.h>
extern char __appended_dtb[];
static inline void *get_fdt(void)
{
	if (IS_ENABLED(CONFIG_MIPS_RAW_APPENDED_DTB) ||
	    IS_ENABLED(CONFIG_MIPS_ELF_APPENDED_DTB))
		if (fdt_magic(&__appended_dtb) == FDT_MAGIC)
			return &__appended_dtb;
	if (fw_arg0 == -2)  
		return (void *)fw_arg1;
	if (IS_ENABLED(CONFIG_BUILTIN_DTB))
		if (&__dtb_start != &__dtb_end)
			return &__dtb_start;
	return NULL;
}
#endif
extern void plat_mem_setup(void);
#ifdef CONFIG_SWIOTLB
extern void plat_swiotlb_setup(void);
#else
static inline void plat_swiotlb_setup(void) {}
#endif  
#ifdef CONFIG_USE_OF
extern void *plat_get_fdt(void);
#ifdef CONFIG_RELOCATABLE
void plat_fdt_relocated(void *new_location);
#endif  
#endif  
#endif  
