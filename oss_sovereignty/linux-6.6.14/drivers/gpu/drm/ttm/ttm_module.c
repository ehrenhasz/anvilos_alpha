 
 
 
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pgtable.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <drm/drm_sysfs.h>
#include <drm/ttm/ttm_caching.h>

#include "ttm_module.h"

 

 
pgprot_t ttm_prot_from_caching(enum ttm_caching caching, pgprot_t tmp)
{
	 
	if (caching == ttm_cached)
		return tmp;

#if defined(__i386__) || defined(__x86_64__)
	if (caching == ttm_write_combined)
		tmp = pgprot_writecombine(tmp);
#ifndef CONFIG_UML
	else if (boot_cpu_data.x86 > 3)
		tmp = pgprot_noncached(tmp);
#endif  
#endif  
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__) || \
	defined(__powerpc__) || defined(__mips__) || defined(__loongarch__)
	if (caching == ttm_write_combined)
		tmp = pgprot_writecombine(tmp);
	else
		tmp = pgprot_noncached(tmp);
#endif
#if defined(__sparc__)
	tmp = pgprot_noncached(tmp);
#endif
	return tmp;
}

MODULE_AUTHOR("Thomas Hellstrom, Jerome Glisse");
MODULE_DESCRIPTION("TTM memory manager subsystem (for DRM device)");
MODULE_LICENSE("GPL and additional rights");
