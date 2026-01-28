#ifndef _SPARC_SETUP_H
#define _SPARC_SETUP_H
#include <linux/interrupt.h>
#include <uapi/asm/setup.h>
extern char reboot_command[];
#ifdef CONFIG_SPARC32
extern unsigned char boot_cpu_id;
extern unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)];
extern int serial_console;
static inline int con_is_present(void)
{
	return serial_console ? 0 : 1;
}
extern volatile unsigned char *fdc_status;
extern char *pdma_vaddr;
extern unsigned long pdma_size;
extern volatile int doing_pdma;
extern char *pdma_base;
extern unsigned long pdma_areasize;
int sparc_floppy_request_irq(unsigned int irq, irq_handler_t irq_handler);
extern unsigned long cmdline_memory_size;
void __init device_scan(void);
unsigned long safe_compute_effective_address(struct pt_regs *, unsigned int);
#endif
#ifdef CONFIG_SPARC64
void __init start_early_boot(void);
int handle_ldf_stq(u32 insn, struct pt_regs *regs);
void handle_ld_nf(u32 insn, struct pt_regs *regs);
extern atomic_t dcpage_flushes;
extern atomic_t dcpage_flushes_xcall;
extern int sysctl_tsb_ratio;
#ifdef CONFIG_SERIAL_SUNHV
void sunhv_migrate_hvcons_irq(int cpu);
#endif
#endif
void sun_do_break(void);
extern int stop_a_enabled;
extern int scons_pwroff;
#endif  
