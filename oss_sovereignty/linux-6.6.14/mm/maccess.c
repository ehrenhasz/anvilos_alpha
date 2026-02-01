
 
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <asm/tlb.h>

bool __weak copy_from_kernel_nofault_allowed(const void *unsafe_src,
		size_t size)
{
	return true;
}

#define copy_from_kernel_nofault_loop(dst, src, len, type, err_label)	\
	while (len >= sizeof(type)) {					\
		__get_kernel_nofault(dst, src, type, err_label);		\
		dst += sizeof(type);					\
		src += sizeof(type);					\
		len -= sizeof(type);					\
	}

long copy_from_kernel_nofault(void *dst, const void *src, size_t size)
{
	unsigned long align = 0;

	if (!IS_ENABLED(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS))
		align = (unsigned long)dst | (unsigned long)src;

	if (!copy_from_kernel_nofault_allowed(src, size))
		return -ERANGE;

	pagefault_disable();
	if (!(align & 7))
		copy_from_kernel_nofault_loop(dst, src, size, u64, Efault);
	if (!(align & 3))
		copy_from_kernel_nofault_loop(dst, src, size, u32, Efault);
	if (!(align & 1))
		copy_from_kernel_nofault_loop(dst, src, size, u16, Efault);
	copy_from_kernel_nofault_loop(dst, src, size, u8, Efault);
	pagefault_enable();
	return 0;
Efault:
	pagefault_enable();
	return -EFAULT;
}
EXPORT_SYMBOL_GPL(copy_from_kernel_nofault);

#define copy_to_kernel_nofault_loop(dst, src, len, type, err_label)	\
	while (len >= sizeof(type)) {					\
		__put_kernel_nofault(dst, src, type, err_label);		\
		dst += sizeof(type);					\
		src += sizeof(type);					\
		len -= sizeof(type);					\
	}

long copy_to_kernel_nofault(void *dst, const void *src, size_t size)
{
	unsigned long align = 0;

	if (!IS_ENABLED(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS))
		align = (unsigned long)dst | (unsigned long)src;

	pagefault_disable();
	if (!(align & 7))
		copy_to_kernel_nofault_loop(dst, src, size, u64, Efault);
	if (!(align & 3))
		copy_to_kernel_nofault_loop(dst, src, size, u32, Efault);
	if (!(align & 1))
		copy_to_kernel_nofault_loop(dst, src, size, u16, Efault);
	copy_to_kernel_nofault_loop(dst, src, size, u8, Efault);
	pagefault_enable();
	return 0;
Efault:
	pagefault_enable();
	return -EFAULT;
}

long strncpy_from_kernel_nofault(char *dst, const void *unsafe_addr, long count)
{
	const void *src = unsafe_addr;

	if (unlikely(count <= 0))
		return 0;
	if (!copy_from_kernel_nofault_allowed(unsafe_addr, count))
		return -ERANGE;

	pagefault_disable();
	do {
		__get_kernel_nofault(dst, src, u8, Efault);
		dst++;
		src++;
	} while (dst[-1] && src - unsafe_addr < count);
	pagefault_enable();

	dst[-1] = '\0';
	return src - unsafe_addr;
Efault:
	pagefault_enable();
	dst[0] = '\0';
	return -EFAULT;
}

 
long copy_from_user_nofault(void *dst, const void __user *src, size_t size)
{
	long ret = -EFAULT;

	if (!__access_ok(src, size))
		return ret;

	if (!nmi_uaccess_okay())
		return ret;

	pagefault_disable();
	ret = __copy_from_user_inatomic(dst, src, size);
	pagefault_enable();

	if (ret)
		return -EFAULT;
	return 0;
}
EXPORT_SYMBOL_GPL(copy_from_user_nofault);

 
long copy_to_user_nofault(void __user *dst, const void *src, size_t size)
{
	long ret = -EFAULT;

	if (access_ok(dst, size)) {
		pagefault_disable();
		ret = __copy_to_user_inatomic(dst, src, size);
		pagefault_enable();
	}

	if (ret)
		return -EFAULT;
	return 0;
}
EXPORT_SYMBOL_GPL(copy_to_user_nofault);

 
long strncpy_from_user_nofault(char *dst, const void __user *unsafe_addr,
			      long count)
{
	long ret;

	if (unlikely(count <= 0))
		return 0;

	pagefault_disable();
	ret = strncpy_from_user(dst, unsafe_addr, count);
	pagefault_enable();

	if (ret >= count) {
		ret = count;
		dst[ret - 1] = '\0';
	} else if (ret > 0) {
		ret++;
	}

	return ret;
}

 
long strnlen_user_nofault(const void __user *unsafe_addr, long count)
{
	int ret;

	pagefault_disable();
	ret = strnlen_user(unsafe_addr, count);
	pagefault_enable();

	return ret;
}

void __copy_overflow(int size, unsigned long count)
{
	WARN(1, "Buffer overflow detected (%d < %lu)!\n", size, count);
}
EXPORT_SYMBOL(__copy_overflow);
