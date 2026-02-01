
 

#include <linux/export.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <sound/core.h>
#include <sound/pcm.h>

 
int copy_to_user_fromio(void __user *dst, const volatile void __iomem *src, size_t count)
{
	struct iov_iter iter;

	if (import_ubuf(ITER_DEST, dst, count, &iter))
		return -EFAULT;
	return copy_to_iter_fromio(&iter, (const void __iomem *)src, count);
}
EXPORT_SYMBOL(copy_to_user_fromio);

 
int copy_to_iter_fromio(struct iov_iter *dst, const void __iomem *src,
			size_t count)
{
#if defined(__i386__) || defined(CONFIG_SPARC32)
	return copy_to_iter((const void __force *)src, count, dst) == count ? 0 : -EFAULT;
#else
	char buf[256];
	while (count) {
		size_t c = count;
		if (c > sizeof(buf))
			c = sizeof(buf);
		memcpy_fromio(buf, (void __iomem *)src, c);
		if (copy_to_iter(buf, c, dst) != c)
			return -EFAULT;
		count -= c;
		src += c;
	}
	return 0;
#endif
}
EXPORT_SYMBOL(copy_to_iter_fromio);

 
int copy_from_user_toio(volatile void __iomem *dst, const void __user *src, size_t count)
{
	struct iov_iter iter;

	if (import_ubuf(ITER_SOURCE, (void __user *)src, count, &iter))
		return -EFAULT;
	return copy_from_iter_toio((void __iomem *)dst, &iter, count);
}
EXPORT_SYMBOL(copy_from_user_toio);

 
int copy_from_iter_toio(void __iomem *dst, struct iov_iter *src, size_t count)
{
#if defined(__i386__) || defined(CONFIG_SPARC32)
	return copy_from_iter((void __force *)dst, count, src) == count ? 0 : -EFAULT;
#else
	char buf[256];
	while (count) {
		size_t c = count;
		if (c > sizeof(buf))
			c = sizeof(buf);
		if (copy_from_iter(buf, c, src) != c)
			return -EFAULT;
		memcpy_toio(dst, buf, c);
		count -= c;
		dst += c;
	}
	return 0;
#endif
}
EXPORT_SYMBOL(copy_from_iter_toio);
