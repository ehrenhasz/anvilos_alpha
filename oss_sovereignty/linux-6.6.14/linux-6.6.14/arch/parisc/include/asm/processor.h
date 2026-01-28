#ifndef __ASM_PARISC_PROCESSOR_H
#define __ASM_PARISC_PROCESSOR_H
#ifndef __ASSEMBLY__
#include <linux/threads.h>
#include <linux/irqreturn.h>
#include <asm/assembly.h>
#include <asm/prefetch.h>
#include <asm/hardware.h>
#include <asm/pdc.h>
#include <asm/ptrace.h>
#include <asm/types.h>
#include <asm/percpu.h>
#endif  
#define HAVE_ARCH_PICK_MMAP_LAYOUT
#define TASK_SIZE_OF(tsk)       ((tsk)->thread.task_size)
#define TASK_SIZE	        TASK_SIZE_OF(current)
#define TASK_UNMAPPED_BASE      (current->thread.map_base)
#define DEFAULT_TASK_SIZE32	(0xFFF00000UL)
#define DEFAULT_MAP_BASE32	(0x40000000UL)
#ifdef CONFIG_64BIT
#define DEFAULT_TASK_SIZE       (MAX_ADDRESS-0xf000000)
#define DEFAULT_MAP_BASE        (0x200000000UL)
#else
#define DEFAULT_TASK_SIZE	DEFAULT_TASK_SIZE32
#define DEFAULT_MAP_BASE	DEFAULT_MAP_BASE32
#endif
#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	DEFAULT_TASK_SIZE
#ifndef __ASSEMBLY__
struct rlimit;
unsigned long mmap_upper_limit(struct rlimit *rlim_stack);
unsigned long calc_max_stack_size(unsigned long stack_max);
struct system_cpuinfo_parisc {
	unsigned int	cpu_count;
	unsigned int	cpu_hz;
	unsigned int	hversion;
	unsigned int	sversion;
	enum cpu_type	cpu_type;
	struct {
		struct pdc_model model;
		unsigned long versions;
		unsigned long cpuid;
		unsigned long capabilities;
		char   sys_model_name[81];  
	} pdc;
	const char	*cpu_name;	 
	const char	*family_name;	 
};
struct cpuinfo_parisc {
	unsigned long it_value;      
	unsigned long irq_count;     
	unsigned long cpuid;         
	unsigned long hpa;           
	unsigned long txn_addr;      
#ifdef CONFIG_SMP
	unsigned long pending_ipi;   
#endif
	unsigned long bh_count;      
	unsigned long fp_rev;
	unsigned long fp_model;
	unsigned long cpu_num;       
	unsigned long cpu_loc;       
	unsigned int state;
	struct parisc_device *dev;
};
extern struct system_cpuinfo_parisc boot_cpu_data;
DECLARE_PER_CPU(struct cpuinfo_parisc, cpu_data);
extern int time_keeper_id;		 
#define CPU_HVERSION ((boot_cpu_data.hversion >> 4) & 0x0FFF)
struct thread_struct {
	struct pt_regs regs;
	unsigned long  task_size;
	unsigned long  map_base;
	unsigned long  flags;
}; 
#define task_pt_regs(tsk) ((struct pt_regs *)&((tsk)->thread.regs))
#define PARISC_UAC_NOPRINT	(1UL << 0)	 
#define PARISC_UAC_SIGBUS	(1UL << 1)
#define PARISC_KERNEL_DEATH	(1UL << 31)	 
#define PARISC_UAC_SHIFT	0
#define PARISC_UAC_MASK		(PARISC_UAC_NOPRINT|PARISC_UAC_SIGBUS)
#define SET_UNALIGN_CTL(task,value)                                       \
        ({                                                                \
        (task)->thread.flags = (((task)->thread.flags & ~PARISC_UAC_MASK) \
                                | (((value) << PARISC_UAC_SHIFT) &        \
                                   PARISC_UAC_MASK));                     \
        0;                                                                \
        })
#define GET_UNALIGN_CTL(task,addr)                                        \
        ({                                                                \
        put_user(((task)->thread.flags & PARISC_UAC_MASK)                 \
                 >> PARISC_UAC_SHIFT, (int __user *) (addr));             \
        })
#define INIT_THREAD { \
	.regs = {	.gr	= { 0, }, \
			.fr	= { 0, }, \
			.sr	= { 0, }, \
			.iasq	= { 0, }, \
			.iaoq	= { 0, }, \
			.cr27	= 0, \
		}, \
	.task_size	= DEFAULT_TASK_SIZE, \
	.map_base	= DEFAULT_MAP_BASE, \
	.flags		= 0 \
	}
struct task_struct;
void show_trace(struct task_struct *task, unsigned long *stack);
typedef unsigned int elf_caddr_t;
#define USER_WIDE_MODE	(!is_32bit_task())
#define start_thread(regs, new_pc, new_sp) do {		\
	elf_addr_t *sp = (elf_addr_t *)new_sp;		\
	__u32 spaceid = (__u32)current->mm->context.space_id;	\
	elf_addr_t pc = (elf_addr_t)new_pc | 3;		\
	elf_caddr_t *argv = (elf_caddr_t *)bprm->exec + 1;	\
							\
	regs->iasq[0] = spaceid;			\
	regs->iasq[1] = spaceid;			\
	regs->iaoq[0] = pc;				\
	regs->iaoq[1] = pc + 4;                         \
	regs->sr[2] = LINUX_GATEWAY_SPACE;              \
	regs->sr[3] = 0xffff;				\
	regs->sr[4] = spaceid;				\
	regs->sr[5] = spaceid;				\
	regs->sr[6] = spaceid;				\
	regs->sr[7] = spaceid;				\
	regs->gr[ 0] = USER_PSW | (USER_WIDE_MODE ? PSW_W : 0); \
	regs->fr[ 0] = 0LL;                            	\
	regs->fr[ 1] = 0LL;                            	\
	regs->fr[ 2] = 0LL;                            	\
	regs->fr[ 3] = 0LL;                            	\
	regs->gr[30] = (((unsigned long)sp + 63) &~ 63) | (USER_WIDE_MODE ? 1 : 0); \
	regs->gr[31] = pc;				\
							\
	get_user(regs->gr[25], (argv - 1));		\
	regs->gr[24] = (long) argv;			\
	regs->gr[23] = 0;				\
} while(0)
struct mm_struct;
extern unsigned long __get_wchan(struct task_struct *p);
#define KSTK_EIP(tsk)	((tsk)->thread.regs.iaoq[0])
#define KSTK_ESP(tsk)	((tsk)->thread.regs.gr[30])
#define cpu_relax()	barrier()
#ifdef CONFIG_PA8X00
extern int _parisc_requires_coherency;
#define parisc_requires_coherency()	_parisc_requires_coherency
#else
#define parisc_requires_coherency()	(0)
#endif
extern int running_on_qemu;
extern void __noreturn toc_intr(struct pt_regs *regs);
extern void toc_handler(void);
extern unsigned int toc_handler_size;
extern unsigned int toc_handler_csum;
extern void do_cpu_irq_mask(struct pt_regs *);
extern irqreturn_t timer_interrupt(int, void *);
extern irqreturn_t ipi_interrupt(int, void *);
extern void start_cpu_itimer(void);
extern void handle_interruption(int, struct pt_regs *);
extern void start_parisc(void);
extern void smp_callin(unsigned long);
extern void sys_rt_sigreturn(struct pt_regs *, int);
extern void do_notify_resume(struct pt_regs *, long);
extern long do_syscall_trace_enter(struct pt_regs *);
extern void do_syscall_trace_exit(struct pt_regs *);
struct seq_file;
extern void early_trap_init(void);
extern void collect_boot_cpu_data(void);
extern void btlb_init_per_cpu(void);
extern int show_cpuinfo (struct seq_file *m, void *v);
extern void processor_init(void);
struct parisc_device;
struct resource;
extern void sba_distributed_lmmio(struct parisc_device *, struct resource *);
extern void sba_directed_lmmio(struct parisc_device *, struct resource *);
extern void lba_set_iregs(struct parisc_device *lba, u32 ibase, u32 imask);
extern void ccio_cujo20_fixup(struct parisc_device *dev, u32 iovp);
#endif  
#endif  
