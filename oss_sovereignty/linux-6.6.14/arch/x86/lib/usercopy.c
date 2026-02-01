 

#include <linux/uaccess.h>
#include <linux/export.h>
#include <linux/instrumented.h>

#include <asm/tlbflush.h>

 
unsigned long
copy_from_user_nmi(void *to, const void __user *from, unsigned long n)
{
	unsigned long ret;

	if (!__access_ok(from, n))
		return n;

	if (!nmi_uaccess_okay())
		return n;

	 
	pagefault_disable();
	instrument_copy_from_user_before(to, from, n);
	ret = raw_copy_from_user(to, from, n);
	instrument_copy_from_user_after(to, from, n, ret);
	pagefault_enable();

	return ret;
}
EXPORT_SYMBOL_GPL(copy_from_user_nmi);
