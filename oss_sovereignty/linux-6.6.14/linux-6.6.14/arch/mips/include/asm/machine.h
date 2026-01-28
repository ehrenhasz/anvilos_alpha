#ifndef __MIPS_ASM_MACHINE_H__
#define __MIPS_ASM_MACHINE_H__
#include <linux/libfdt.h>
#include <linux/of.h>
struct mips_machine {
	const struct of_device_id *matches;
	const void *fdt;
	bool (*detect)(void);
	const void *(*fixup_fdt)(const void *fdt, const void *match_data);
	unsigned int (*measure_hpt_freq)(void);
};
extern long __mips_machines_start;
extern long __mips_machines_end;
#define MIPS_MACHINE(name)						\
	static const struct mips_machine __mips_mach_##name		\
		__used __section(".mips.machines.init")
#define for_each_mips_machine(mach)					\
	for ((mach) = (struct mips_machine *)&__mips_machines_start;	\
	     (mach) < (struct mips_machine *)&__mips_machines_end;	\
	     (mach)++)
static inline const struct of_device_id *
mips_machine_is_compatible(const struct mips_machine *mach, const void *fdt)
{
	const struct of_device_id *match;
	if (!mach->matches)
		return NULL;
	for (match = mach->matches; match->compatible[0]; match++) {
		if (fdt_node_check_compatible(fdt, 0, match->compatible) == 0)
			return match;
	}
	return NULL;
}
struct mips_fdt_fixup {
	int (*apply)(void *fdt);
	const char *description;
};
extern int __init apply_mips_fdt_fixups(void *fdt_out, size_t fdt_out_size,
					const void *fdt_in,
					const struct mips_fdt_fixup *fixups);
#endif  
