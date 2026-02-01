
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include "debug.h"

 
void __ntfs_warning(const char *function, const struct super_block *sb,
		const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int flen = 0;

#ifndef DEBUG
	if (!printk_ratelimit())
		return;
#endif
	if (function)
		flen = strlen(function);
	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	if (sb)
		pr_warn("(device %s): %s(): %pV\n",
			sb->s_id, flen ? function : "", &vaf);
	else
		pr_warn("%s(): %pV\n", flen ? function : "", &vaf);
	va_end(args);
}

 
void __ntfs_error(const char *function, const struct super_block *sb,
		const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int flen = 0;

#ifndef DEBUG
	if (!printk_ratelimit())
		return;
#endif
	if (function)
		flen = strlen(function);
	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	if (sb)
		pr_err("(device %s): %s(): %pV\n",
		       sb->s_id, flen ? function : "", &vaf);
	else
		pr_err("%s(): %pV\n", flen ? function : "", &vaf);
	va_end(args);
}

#ifdef DEBUG

 
int debug_msgs = 0;

void __ntfs_debug(const char *file, int line, const char *function,
		const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int flen = 0;

	if (!debug_msgs)
		return;
	if (function)
		flen = strlen(function);
	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	pr_debug("(%s, %d): %s(): %pV", file, line, flen ? function : "", &vaf);
	va_end(args);
}

 
void ntfs_debug_dump_runlist(const runlist_element *rl)
{
	int i;
	const char *lcn_str[5] = { "LCN_HOLE         ", "LCN_RL_NOT_MAPPED",
				   "LCN_ENOENT       ", "LCN_unknown      " };

	if (!debug_msgs)
		return;
	pr_debug("Dumping runlist (values in hex):\n");
	if (!rl) {
		pr_debug("Run list not present.\n");
		return;
	}
	pr_debug("VCN              LCN               Run length\n");
	for (i = 0; ; i++) {
		LCN lcn = (rl + i)->lcn;

		if (lcn < (LCN)0) {
			int index = -lcn - 1;

			if (index > -LCN_ENOENT - 1)
				index = 3;
			pr_debug("%-16Lx %s %-16Lx%s\n",
					(long long)(rl + i)->vcn, lcn_str[index],
					(long long)(rl + i)->length,
					(rl + i)->length ? "" :
						" (runlist end)");
		} else
			pr_debug("%-16Lx %-16Lx  %-16Lx%s\n",
					(long long)(rl + i)->vcn,
					(long long)(rl + i)->lcn,
					(long long)(rl + i)->length,
					(rl + i)->length ? "" :
						" (runlist end)");
		if (!(rl + i)->length)
			break;
	}
}

#endif
