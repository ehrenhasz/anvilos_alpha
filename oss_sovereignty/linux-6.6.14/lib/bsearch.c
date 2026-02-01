
 

#include <linux/export.h>
#include <linux/bsearch.h>
#include <linux/kprobes.h>

 
void *bsearch(const void *key, const void *base, size_t num, size_t size, cmp_func_t cmp)
{
	return __inline_bsearch(key, base, num, size, cmp);
}
EXPORT_SYMBOL(bsearch);
NOKPROBE_SYMBOL(bsearch);
