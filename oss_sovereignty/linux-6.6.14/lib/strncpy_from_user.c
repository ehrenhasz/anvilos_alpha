
#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/fault-inject-usercopy.h>
#include <linux/kasan-checks.h>
#include <linux/thread_info.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>

#include <asm/byteorder.h>
#include <asm/word-at-a-time.h>

#ifdef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
#define IS_UNALIGNED(src, dst)	0
#else
#define IS_UNALIGNED(src, dst)	\
	(((long) dst | (long) src) & (sizeof(long) - 1))
#endif

 
static __always_inline long do_strncpy_from_user(char *dst, const char __user *src,
					unsigned long count, unsigned long max)
{
	const struct word_at_a_time constants = WORD_AT_A_TIME_CONSTANTS;
	unsigned long res = 0;

	if (IS_UNALIGNED(src, dst))
		goto byte_at_a_time;

	while (max >= sizeof(unsigned long)) {
		unsigned long c, data, mask;

		 
		unsafe_get_user(c, (unsigned long __user *)(src+res), byte_at_a_time);

		 
		if (has_zero(c, &data, &constants)) {
			data = prep_zero_mask(c, data, &constants);
			data = create_zero_mask(data);
			mask = zero_bytemask(data);
			*(unsigned long *)(dst+res) = c & mask;
			return res + find_zero(data);
		}

		*(unsigned long *)(dst+res) = c;

		res += sizeof(unsigned long);
		max -= sizeof(unsigned long);
	}

byte_at_a_time:
	while (max) {
		char c;

		unsafe_get_user(c,src+res, efault);
		dst[res] = c;
		if (!c)
			return res;
		res++;
		max--;
	}

	 
	if (res >= count)
		return res;

	 
efault:
	return -EFAULT;
}

 
long strncpy_from_user(char *dst, const char __user *src, long count)
{
	unsigned long max_addr, src_addr;

	might_fault();
	if (should_fail_usercopy())
		return -EFAULT;
	if (unlikely(count <= 0))
		return 0;

	max_addr = TASK_SIZE_MAX;
	src_addr = (unsigned long)untagged_addr(src);
	if (likely(src_addr < max_addr)) {
		unsigned long max = max_addr - src_addr;
		long retval;

		 
		if (max > count)
			max = count;

		kasan_check_write(dst, count);
		check_object_size(dst, count, false);
		if (user_read_access_begin(src, max)) {
			retval = do_strncpy_from_user(dst, src, count, max);
			user_read_access_end();
			return retval;
		}
	}
	return -EFAULT;
}
EXPORT_SYMBOL(strncpy_from_user);
