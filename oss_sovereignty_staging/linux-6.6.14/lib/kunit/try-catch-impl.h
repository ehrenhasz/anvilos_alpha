 
 

#ifndef _KUNIT_TRY_CATCH_IMPL_H
#define _KUNIT_TRY_CATCH_IMPL_H

#include <kunit/try-catch.h>
#include <linux/types.h>

struct kunit;

static inline void kunit_try_catch_init(struct kunit_try_catch *try_catch,
					struct kunit *test,
					kunit_try_catch_func_t try,
					kunit_try_catch_func_t catch)
{
	try_catch->test = test;
	try_catch->try = try;
	try_catch->catch = catch;
}

#endif  
