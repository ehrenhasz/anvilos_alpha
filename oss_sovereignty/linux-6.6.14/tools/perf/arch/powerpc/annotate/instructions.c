
#include <linux/compiler.h>

static struct ins_ops *powerpc__associate_instruction_ops(struct arch *arch, const char *name)
{
	int i;
	struct ins_ops *ops;

	 
	if (name[0] != 'b'             ||
	    !strncmp(name, "bcd", 3)   ||
	    !strncmp(name, "brinc", 5) ||
	    !strncmp(name, "bper", 4))
		return NULL;

	ops = &jump_ops;

	i = strlen(name) - 1;
	if (i < 0)
		return NULL;

	 
	if (name[i] == '+' || name[i] == '-')
		i--;

	if (name[i] == 'l' || (name[i] == 'a' && name[i-1] == 'l')) {
		 
		if (strcmp(name, "bnl") && strcmp(name, "bnl+") &&
		    strcmp(name, "bnl-") && strcmp(name, "bnla") &&
		    strcmp(name, "bnla+") && strcmp(name, "bnla-"))
			ops = &call_ops;
	}
	if (name[i] == 'r' && name[i-1] == 'l')
		 
		ops = &ret_ops;

	arch__associate_ins_ops(arch, name, ops);
	return ops;
}

static int powerpc__annotate_init(struct arch *arch, char *cpuid __maybe_unused)
{
	if (!arch->initialized) {
		arch->initialized = true;
		arch->associate_instruction_ops = powerpc__associate_instruction_ops;
		arch->objdump.comment_char      = '#';
	}

	return 0;
}
