


#ifndef _KUNIT_HOOKS_IMPL_H
#define _KUNIT_HOOKS_IMPL_H

#include <kunit/test-bug.h>


void __printf(3, 4) __kunit_fail_current_test_impl(const char *file,
						   int line,
						   const char *fmt, ...);
void *__kunit_get_static_stub_address_impl(struct kunit *test, void *real_fn_addr);


static inline void kunit_install_hooks(void)
{
	
	kunit_hooks.fail_current_test = __kunit_fail_current_test_impl;
	kunit_hooks.get_static_stub_address = __kunit_get_static_stub_address_impl;
}

#endif 
