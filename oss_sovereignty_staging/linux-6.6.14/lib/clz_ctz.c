
 

#include <linux/export.h>
#include <linux/kernel.h>

int __weak __ctzsi2(int val);
int __weak __ctzsi2(int val)
{
	return __ffs(val);
}
EXPORT_SYMBOL(__ctzsi2);

int __weak __clzsi2(int val);
int __weak __clzsi2(int val)
{
	return 32 - fls(val);
}
EXPORT_SYMBOL(__clzsi2);

int __weak __clzdi2(u64 val);
int __weak __clzdi2(u64 val)
{
	return 64 - fls64(val);
}
EXPORT_SYMBOL(__clzdi2);

int __weak __ctzdi2(u64 val);
int __weak __ctzdi2(u64 val)
{
	return __ffs64(val);
}
EXPORT_SYMBOL(__ctzdi2);
