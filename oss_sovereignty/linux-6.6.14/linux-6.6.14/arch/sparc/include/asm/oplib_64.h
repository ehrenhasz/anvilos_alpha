#ifndef __SPARC64_OPLIB_H
#define __SPARC64_OPLIB_H
#include <asm/openprom.h>
extern char prom_version[];
extern phandle prom_root_node;
extern int prom_stdout;
extern phandle prom_chosen_node;
extern const char prom_peer_name[];
extern const char prom_compatible_name[];
extern const char prom_root_compatible[];
extern const char prom_cpu_compatible[];
extern const char prom_finddev_name[];
extern const char prom_chosen_path[];
extern const char prom_cpu_path[];
extern const char prom_getprop_name[];
extern const char prom_mmu_name[];
extern const char prom_callmethod_name[];
extern const char prom_translate_name[];
extern const char prom_map_name[];
extern const char prom_unmap_name[];
extern int prom_mmu_ihandle_cache;
extern unsigned int prom_boot_mapped_pc;
extern unsigned int prom_boot_mapping_mode;
extern unsigned long prom_boot_mapping_phys_high, prom_boot_mapping_phys_low;
struct linux_mlist_p1275 {
	struct linux_mlist_p1275 *theres_more;
	unsigned long start_adr;
	unsigned long num_bytes;
};
struct linux_mem_p1275 {
	struct linux_mlist_p1275 **p1275_totphys;
	struct linux_mlist_p1275 **p1275_prommap;
	struct linux_mlist_p1275 **p1275_available;  
};
void prom_init(void *cif_handler);
void prom_init_report(void);
char *prom_getbootargs(void);
void prom_reboot(const char *boot_command);
void prom_feval(const char *forth_string);
void prom_cmdline(void);
void prom_halt(void) __attribute__ ((noreturn));
void prom_halt_power_off(void) __attribute__ ((noreturn));
unsigned char prom_get_idprom(char *idp_buffer, int idpbuf_size);
void prom_console_write_buf(const char *buf, int len);
__printf(1, 2) void prom_printf(const char *fmt, ...);
void prom_write(const char *buf, unsigned int len);
#ifdef CONFIG_SMP
void prom_startcpu(int cpunode, unsigned long pc, unsigned long arg);
void prom_startcpu_cpuid(int cpuid, unsigned long pc, unsigned long arg);
void prom_stopcpu_cpuid(int cpuid);
void prom_stopself(void);
void prom_idleself(void);
void prom_resumecpu(int cpunode);
#endif
void prom_sleepself(void);
int prom_sleepsystem(void);
int prom_wakeupsystem(void);
int prom_getunumber(int syndrome_code,
		    unsigned long phys_addr,
		    char *buf, int buflen);
int prom_retain(const char *name, unsigned long size,
		unsigned long align, unsigned long *paddr);
long prom_itlb_load(unsigned long index,
		    unsigned long tte_data,
		    unsigned long vaddr);
long prom_dtlb_load(unsigned long index,
		    unsigned long tte_data,
		    unsigned long vaddr);
#define PROM_MAP_WRITE	0x0001  
#define PROM_MAP_READ	0x0002  
#define PROM_MAP_EXEC	0x0004  
#define PROM_MAP_LOCKED	0x0010  
#define PROM_MAP_CACHED	0x0020  
#define PROM_MAP_SE	0x0040  
#define PROM_MAP_GLOB	0x0080  
#define PROM_MAP_IE	0x0100  
#define PROM_MAP_DEFAULT (PROM_MAP_WRITE | PROM_MAP_READ | PROM_MAP_EXEC | PROM_MAP_CACHED)
int prom_map(int mode, unsigned long size,
	     unsigned long vaddr, unsigned long paddr);
void prom_unmap(unsigned long size, unsigned long vaddr);
phandle prom_getchild(phandle parent_node);
phandle prom_getsibling(phandle node);
int prom_getproplen(phandle thisnode, const char *property);
int prom_getproperty(phandle thisnode, const char *property,
		     char *prop_buffer, int propbuf_size);
int prom_getint(phandle node, const char *property);
int prom_getintdefault(phandle node, const char *property, int defval);
int prom_getbool(phandle node, const char *prop);
void prom_getstring(phandle node, const char *prop, char *buf,
		    int bufsize);
int prom_nodematch(phandle thisnode, const char *name);
phandle prom_searchsiblings(phandle node_start, const char *name);
char *prom_firstprop(phandle node, char *buffer);
char *prom_nextprop(phandle node, const char *prev_property, char *buf);
int prom_node_has_property(phandle node, const char *property);
phandle prom_finddevice(const char *name);
int prom_setprop(phandle node, const char *prop_name, char *prop_value,
		 int value_size);
phandle prom_inst2pkg(int);
void prom_sun4v_guest_soft_state(void);
int prom_ihandle2path(int handle, char *buffer, int bufsize);
void p1275_cmd_direct(unsigned long *);
#endif  
