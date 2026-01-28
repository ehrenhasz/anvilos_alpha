


#ifndef _LINUX_NTFS_DEBUG_H
#define _LINUX_NTFS_DEBUG_H

#include <linux/fs.h>

#include "runlist.h"

#ifdef DEBUG

extern int debug_msgs;

extern __printf(4, 5)
void __ntfs_debug(const char *file, int line, const char *function,
		  const char *format, ...);

#define ntfs_debug(f, a...)						\
	__ntfs_debug(__FILE__, __LINE__, __func__, f, ##a)

extern void ntfs_debug_dump_runlist(const runlist_element *rl);

#else	

#define ntfs_debug(fmt, ...)						\
do {									\
	if (0)								\
		no_printk(fmt, ##__VA_ARGS__);				\
} while (0)

#define ntfs_debug_dump_runlist(rl)	do {} while (0)

#endif	

extern  __printf(3, 4)
void __ntfs_warning(const char *function, const struct super_block *sb,
		    const char *fmt, ...);
#define ntfs_warning(sb, f, a...)	__ntfs_warning(__func__, sb, f, ##a)

extern  __printf(3, 4)
void __ntfs_error(const char *function, const struct super_block *sb,
		  const char *fmt, ...);
#define ntfs_error(sb, f, a...)		__ntfs_error(__func__, sb, f, ##a)

#endif 
