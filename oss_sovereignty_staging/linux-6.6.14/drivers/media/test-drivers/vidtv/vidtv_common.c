
 
#define pr_fmt(fmt) KBUILD_MODNAME ":%s, %d: " fmt, __func__, __LINE__

#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/string.h>
#include <linux/types.h>

#include "vidtv_common.h"

 
u32 vidtv_memcpy(void *to,
		 size_t to_offset,
		 size_t to_size,
		 const void *from,
		 size_t len)
{
	if (unlikely(to_offset + len > to_size)) {
		pr_err_ratelimited("overflow detected, skipping. Try increasing the buffer size. Needed %zu, had %zu\n",
				   to_offset + len,
				   to_size);
		return 0;
	}

	memcpy(to + to_offset, from, len);
	return len;
}

 
u32 vidtv_memset(void *to,
		 size_t to_offset,
		 size_t to_size,
		 const int c,
		 size_t len)
{
	if (unlikely(to_offset + len > to_size)) {
		pr_err_ratelimited("overflow detected, skipping. Try increasing the buffer size. Needed %zu, had %zu\n",
				   to_offset + len,
				   to_size);
		return 0;
	}

	memset(to + to_offset, c, len);
	return len;
}
