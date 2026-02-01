
 

#include "trace/beauty/beauty.h"
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <sys/mount.h>

static size_t mount__scnprintf_flags(unsigned long flags, char *bf, size_t size, bool show_prefix)
{
#include "trace/beauty/generated/mount_flags_array.c"
	static DEFINE_STRARRAY(mount_flags, "MS_");

	return strarray__scnprintf_flags(&strarray__mount_flags, bf, size, show_prefix, flags);
}

unsigned long syscall_arg__mask_val_mount_flags(struct syscall_arg *arg __maybe_unused, unsigned long flags)
{
	
	 
	if ((flags & MS_MGC_MSK) == MS_MGC_VAL)
		flags &= ~MS_MGC_MSK;

	return flags;
}

size_t syscall_arg__scnprintf_mount_flags(char *bf, size_t size, struct syscall_arg *arg)
{
	unsigned long flags = arg->val;

	return mount__scnprintf_flags(flags, bf, size, arg->show_string_prefix);
}
