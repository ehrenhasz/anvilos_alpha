
 
#include "lkdtm.h"
#include <asm/page.h>

static int called_count;

 
static noinline void lkdtm_increment_void(int *counter)
{
	(*counter)++;
}

 
static noinline int lkdtm_increment_int(int *counter)
{
	(*counter)++;

	return *counter;
}

 
static noinline void lkdtm_indirect_call(void (*func)(int *))
{
	func(&called_count);
}

 
static void lkdtm_CFI_FORWARD_PROTO(void)
{
	 
	pr_info("Calling matched prototype ...\n");
	lkdtm_indirect_call(lkdtm_increment_void);

	pr_info("Calling mismatched prototype ...\n");
	lkdtm_indirect_call((void *)lkdtm_increment_int);

	pr_err("FAIL: survived mismatched prototype function call!\n");
	pr_expected_config(CONFIG_CFI_CLANG);
}

 
#ifdef CONFIG_ARM64_PTR_AUTH_KERNEL
# ifdef CONFIG_ARM64_BTI_KERNEL
#  define __no_pac             "branch-protection=bti"
# else
#  ifdef CONFIG_CC_HAS_BRANCH_PROT_PAC_RET
#   define __no_pac            "branch-protection=none"
#  else
#   define __no_pac            "sign-return-address=none"
#  endif
# endif
# define __no_ret_protection   __noscs __attribute__((__target__(__no_pac)))
#else
# define __no_ret_protection   __noscs
#endif

#define no_pac_addr(addr)      \
	((__force __typeof__(addr))((uintptr_t)(addr) | PAGE_OFFSET))

 
static noinline __no_ret_protection
void set_return_addr_unchecked(unsigned long *expected, unsigned long *addr)
{
	 
	unsigned long * volatile *ret_addr = (unsigned long **)__builtin_frame_address(0) + 1;

	 
	if (no_pac_addr(*ret_addr) == expected)
		*ret_addr = (addr);
	else
		 
		pr_warn("Eek: return address mismatch! %px != %px\n",
			*ret_addr, addr);
}

static noinline
void set_return_addr(unsigned long *expected, unsigned long *addr)
{
	 
	unsigned long * volatile *ret_addr = (unsigned long **)__builtin_frame_address(0) + 1;

	 
	if (no_pac_addr(*ret_addr) == expected)
		*ret_addr = (addr);
	else
		 
		pr_warn("Eek: return address mismatch! %px != %px\n",
			*ret_addr, addr);
}

static volatile int force_check;

static void lkdtm_CFI_BACKWARD(void)
{
	 
	void *labels[] = { NULL, &&normal, &&redirected, &&check_normal, &&check_redirected };

	pr_info("Attempting unchecked stack return address redirection ...\n");

	 
	if (force_check) {
		 
		set_return_addr_unchecked(NULL, NULL);
		set_return_addr(NULL, NULL);
		if (force_check)
			goto *labels[1];
		if (force_check)
			goto *labels[2];
		if (force_check)
			goto *labels[3];
		if (force_check)
			goto *labels[4];
		return;
	}

	 
	switch (force_check) {
	case 0:
		set_return_addr_unchecked(&&normal, &&redirected);
		fallthrough;
	case 1:
normal:
		 
		if (!force_check) {
			pr_err("FAIL: stack return address manipulation failed!\n");
			 
			return;
		}
		break;
	default:
redirected:
		pr_info("ok: redirected stack return address.\n");
		break;
	}

	pr_info("Attempting checked stack return address redirection ...\n");

	switch (force_check) {
	case 0:
		set_return_addr(&&check_normal, &&check_redirected);
		fallthrough;
	case 1:
check_normal:
		 
		if (!force_check) {
			pr_info("ok: control flow unchanged.\n");
			return;
		}

check_redirected:
		pr_err("FAIL: stack return address was redirected!\n");
		break;
	}

	if (IS_ENABLED(CONFIG_ARM64_PTR_AUTH_KERNEL)) {
		pr_expected_config(CONFIG_ARM64_PTR_AUTH_KERNEL);
		return;
	}
	if (IS_ENABLED(CONFIG_SHADOW_CALL_STACK)) {
		pr_expected_config(CONFIG_SHADOW_CALL_STACK);
		return;
	}
	pr_warn("This is probably expected, since this %s was built *without* %s=y nor %s=y\n",
		lkdtm_kernel_info,
		"CONFIG_ARM64_PTR_AUTH_KERNEL", "CONFIG_SHADOW_CALL_STACK");
}

static struct crashtype crashtypes[] = {
	CRASHTYPE(CFI_FORWARD_PROTO),
	CRASHTYPE(CFI_BACKWARD),
};

struct crashtype_category cfi_crashtypes = {
	.crashtypes = crashtypes,
	.len	    = ARRAY_SIZE(crashtypes),
};
