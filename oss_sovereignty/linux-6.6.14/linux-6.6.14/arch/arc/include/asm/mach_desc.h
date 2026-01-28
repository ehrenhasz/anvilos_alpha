#ifndef _ASM_ARC_MACH_DESC_H_
#define _ASM_ARC_MACH_DESC_H_
struct machine_desc {
	const char		*name;
	const char		**dt_compat;
	void			(*init_early)(void);
	void			(*init_per_cpu)(unsigned int);
	void			(*init_machine)(void);
	void			(*init_late)(void);
};
extern const struct machine_desc *machine_desc;
extern const struct machine_desc __arch_info_begin[], __arch_info_end[];
#define MACHINE_START(_type, _name)			\
static const struct machine_desc __mach_desc_##_type	\
__used __section(".arch.info.init") = {			\
	.name		= _name,
#define MACHINE_END				\
};
extern const struct machine_desc *setup_machine_fdt(void *dt);
#endif
