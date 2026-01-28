#ifndef _ASM_TRAPS_H
#define _ASM_TRAPS_H
#define MIPS_BE_DISCARD 0		 
#define MIPS_BE_FIXUP	1		 
#define MIPS_BE_FATAL	2		 
extern void (*board_be_init)(void);
void mips_set_be_handler(int (*handler)(struct pt_regs *reg, int is_fixup));
extern void (*board_nmi_handler_setup)(void);
extern void (*board_ejtag_handler_setup)(void);
extern void (*board_bind_eic_interrupt)(int irq, int regset);
extern void (*board_ebase_setup)(void);
extern void (*board_cache_error_setup)(void);
extern int register_nmi_notifier(struct notifier_block *nb);
extern void reserve_exception_space(phys_addr_t addr, unsigned long size);
extern char except_vec_nmi[];
#define VECTORSPACING 0x100	 
#define nmi_notifier(fn, pri)						\
({									\
	static struct notifier_block fn##_nb = {			\
		.notifier_call = fn,					\
		.priority = pri						\
	};								\
									\
	register_nmi_notifier(&fn##_nb);				\
})
#endif  
