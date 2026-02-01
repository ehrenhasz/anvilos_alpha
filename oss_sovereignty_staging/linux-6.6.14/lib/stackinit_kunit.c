
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kunit/test.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

 
#define MAX_VAR_SIZE	128
static u8 check_buf[MAX_VAR_SIZE];

 
#define VAR_BUFFER	 32

 
static volatile u8 forced_mask = 0xff;

 
static void *fill_start, *target_start;
static size_t fill_size, target_size;

static bool stackinit_range_contains(char *haystack_start, size_t haystack_size,
				     char *needle_start, size_t needle_size)
{
	if (needle_start >= haystack_start &&
	    needle_start + needle_size <= haystack_start + haystack_size)
		return true;
	return false;
}

 
#define WANT_SUCCESS				0
#define XFAIL					1

#define DO_NOTHING_TYPE_SCALAR(var_type)	var_type
#define DO_NOTHING_TYPE_STRING(var_type)	void
#define DO_NOTHING_TYPE_STRUCT(var_type)	void

#define DO_NOTHING_RETURN_SCALAR(ptr)		*(ptr)
#define DO_NOTHING_RETURN_STRING(ptr)		 
#define DO_NOTHING_RETURN_STRUCT(ptr)		 

#define DO_NOTHING_CALL_SCALAR(var, name)			\
		(var) = do_nothing_ ## name(&(var))
#define DO_NOTHING_CALL_STRING(var, name)			\
		do_nothing_ ## name(var)
#define DO_NOTHING_CALL_STRUCT(var, name)			\
		do_nothing_ ## name(&(var))

#define FETCH_ARG_SCALAR(var)		&var
#define FETCH_ARG_STRING(var)		var
#define FETCH_ARG_STRUCT(var)		&var

#define FILL_SIZE_STRING		16

#define INIT_CLONE_SCALAR		 
#define INIT_CLONE_STRING		[FILL_SIZE_STRING]
#define INIT_CLONE_STRUCT		 

#define ZERO_CLONE_SCALAR(zero)		memset(&(zero), 0x00, sizeof(zero))
#define ZERO_CLONE_STRING(zero)		memset(&(zero), 0x00, sizeof(zero))
 
#define ZERO_CLONE_STRUCT(zero)				\
	do {						\
		memset(&(zero), 0xFF, sizeof(zero));	\
		zero.one = 0;				\
		zero.two = 0;				\
		zero.three = 0;				\
		zero.four = 0;				\
	} while (0)

#define INIT_SCALAR_none(var_type)	 
#define INIT_SCALAR_zero(var_type)	= 0

#define INIT_STRING_none(var_type)	[FILL_SIZE_STRING]  
#define INIT_STRING_zero(var_type)	[FILL_SIZE_STRING] = { }

#define INIT_STRUCT_none(var_type)	 
#define INIT_STRUCT_zero(var_type)	= { }


#define __static_partial		{ .two = 0, }
#define __static_all			{ .one = 0,			\
					  .two = 0,			\
					  .three = 0,			\
					  .four = 0,			\
					}
#define __dynamic_partial		{ .two = arg->two, }
#define __dynamic_all			{ .one = arg->one,		\
					  .two = arg->two,		\
					  .three = arg->three,		\
					  .four = arg->four,		\
					}
#define __runtime_partial		var.two = 0
#define __runtime_all			var.one = 0;			\
					var.two = 0;			\
					var.three = 0;			\
					var.four = 0

#define INIT_STRUCT_static_partial(var_type)				\
					= __static_partial
#define INIT_STRUCT_static_all(var_type)				\
					= __static_all
#define INIT_STRUCT_dynamic_partial(var_type)				\
					= __dynamic_partial
#define INIT_STRUCT_dynamic_all(var_type)				\
					= __dynamic_all
#define INIT_STRUCT_runtime_partial(var_type)				\
					; __runtime_partial
#define INIT_STRUCT_runtime_all(var_type)				\
					; __runtime_all

#define INIT_STRUCT_assigned_static_partial(var_type)			\
					; var = (var_type)__static_partial
#define INIT_STRUCT_assigned_static_all(var_type)			\
					; var = (var_type)__static_all
#define INIT_STRUCT_assigned_dynamic_partial(var_type)			\
					; var = (var_type)__dynamic_partial
#define INIT_STRUCT_assigned_dynamic_all(var_type)			\
					; var = (var_type)__dynamic_all

#define INIT_STRUCT_assigned_copy(var_type)				\
					; var = *(arg)

 
#define DEFINE_TEST_DRIVER(name, var_type, which, xfail)	\
 			\
static noinline void test_ ## name (struct kunit *test)		\
{								\
	var_type zero INIT_CLONE_ ## which;			\
	int ignored;						\
	u8 sum = 0, i;						\
								\
	 	\
	BUILD_BUG_ON(sizeof(zero) > MAX_VAR_SIZE);		\
								\
	 	\
	ZERO_CLONE_ ## which(zero);				\
	 	\
	memset(check_buf, 0x00, sizeof(check_buf));		\
	 				\
	ignored = leaf_ ##name((unsigned long)&ignored, 1,	\
				FETCH_ARG_ ## which(zero));	\
	 		\
	for (sum = 0, i = 0; i < target_size; i++)		\
		sum += (check_buf[i] != 0xFF);			\
	KUNIT_ASSERT_EQ_MSG(test, sum, 0,			\
			    "leaf fill was not 0xFF!?\n");	\
	 	\
	memset(check_buf, 0x00, sizeof(check_buf));		\
	 		\
	ignored = leaf_ ##name((unsigned long)&ignored, 0,	\
				FETCH_ARG_ ## which(zero));	\
								\
	 	\
	KUNIT_ASSERT_TRUE_MSG(test,				\
		stackinit_range_contains(fill_start, fill_size,	\
			    target_start, target_size),		\
		"stack fill missed target!? "			\
		"(fill %zu wide, target offset by %d)\n",	\
		fill_size,					\
		(int)((ssize_t)(uintptr_t)fill_start -		\
		      (ssize_t)(uintptr_t)target_start));	\
								\
	 	\
	for (sum = 0, i = 0; i < target_size; i++)		\
		sum += (check_buf[i] == 0xFF);			\
								\
	if (sum != 0 && xfail)					\
		kunit_skip(test,				\
			   "XFAIL uninit bytes: %d\n",		\
			   sum);				\
	KUNIT_ASSERT_EQ_MSG(test, sum, 0,			\
		"uninit bytes: %d\n", sum);			\
}
#define DEFINE_TEST(name, var_type, which, init_level, xfail)	\
 \
static noinline DO_NOTHING_TYPE_ ## which(var_type)		\
do_nothing_ ## name(var_type *ptr)				\
{								\
	 	\
	if ((unsigned long)ptr > 0x2)				\
		return DO_NOTHING_RETURN_ ## which(ptr);	\
	else							\
		return DO_NOTHING_RETURN_ ## which(ptr + 1);	\
}								\
static noinline int leaf_ ## name(unsigned long sp, bool fill,	\
				  var_type *arg)		\
{								\
	char buf[VAR_BUFFER];					\
	var_type var						\
		INIT_ ## which ## _ ## init_level(var_type);	\
								\
	target_start = &var;					\
	target_size = sizeof(var);				\
	 							\
	memset(buf, (char)(sp & 0xff), sizeof(buf));		\
	 				\
	if (fill) {						\
		fill_start = &var;				\
		fill_size = sizeof(var);			\
		memset(fill_start,				\
		       (char)((sp & 0xff) | forced_mask),	\
		       fill_size);				\
	}							\
								\
	 		\
	DO_NOTHING_CALL_ ## which(var, name);			\
								\
	 					\
	memcpy(check_buf, target_start, target_size);		\
								\
	return (int)buf[0] | (int)buf[sizeof(buf) - 1];		\
}								\
DEFINE_TEST_DRIVER(name, var_type, which, xfail)

 
struct test_packed {
	unsigned long one;
	unsigned long two;
	unsigned long three;
	unsigned long four;
};

 
struct test_small_hole {
	size_t one;
	char two;
	 
	int three;
	unsigned long four;
};

 
struct test_big_hole {
	u8 one;
	u8 two;
	u8 three;
	 
	u8 four __aligned(64);
} __aligned(64);

struct test_trailing_hole {
	char *one;
	char *two;
	char *three;
	char four;
	 
};

 
struct test_user {
	u8 one;
	unsigned long two;
	char __user *three;
	unsigned long four;
};

#define ALWAYS_PASS	WANT_SUCCESS
#define ALWAYS_FAIL	XFAIL

#ifdef CONFIG_INIT_STACK_NONE
# define USER_PASS	XFAIL
# define BYREF_PASS	XFAIL
# define STRONG_PASS	XFAIL
#elif defined(CONFIG_GCC_PLUGIN_STRUCTLEAK_USER)
# define USER_PASS	WANT_SUCCESS
# define BYREF_PASS	XFAIL
# define STRONG_PASS	XFAIL
#elif defined(CONFIG_GCC_PLUGIN_STRUCTLEAK_BYREF)
# define USER_PASS	WANT_SUCCESS
# define BYREF_PASS	WANT_SUCCESS
# define STRONG_PASS	XFAIL
#else
# define USER_PASS	WANT_SUCCESS
# define BYREF_PASS	WANT_SUCCESS
# define STRONG_PASS	WANT_SUCCESS
#endif

#define DEFINE_SCALAR_TEST(name, init, xfail)			\
		DEFINE_TEST(name ## _ ## init, name, SCALAR,	\
			    init, xfail)

#define DEFINE_SCALAR_TESTS(init, xfail)			\
		DEFINE_SCALAR_TEST(u8, init, xfail);		\
		DEFINE_SCALAR_TEST(u16, init, xfail);		\
		DEFINE_SCALAR_TEST(u32, init, xfail);		\
		DEFINE_SCALAR_TEST(u64, init, xfail);		\
		DEFINE_TEST(char_array_ ## init, unsigned char,	\
			    STRING, init, xfail)

#define DEFINE_STRUCT_TEST(name, init, xfail)			\
		DEFINE_TEST(name ## _ ## init,			\
			    struct test_ ## name, STRUCT, init, \
			    xfail)

#define DEFINE_STRUCT_TESTS(init, xfail)			\
		DEFINE_STRUCT_TEST(small_hole, init, xfail);	\
		DEFINE_STRUCT_TEST(big_hole, init, xfail);	\
		DEFINE_STRUCT_TEST(trailing_hole, init, xfail);	\
		DEFINE_STRUCT_TEST(packed, init, xfail)

#define DEFINE_STRUCT_INITIALIZER_TESTS(base, xfail)		\
		DEFINE_STRUCT_TESTS(base ## _ ## partial,	\
				    xfail);			\
		DEFINE_STRUCT_TESTS(base ## _ ## all, xfail)

 
DEFINE_SCALAR_TESTS(zero, ALWAYS_PASS);
DEFINE_STRUCT_TESTS(zero, ALWAYS_PASS);
 
DEFINE_STRUCT_INITIALIZER_TESTS(static, STRONG_PASS);
DEFINE_STRUCT_INITIALIZER_TESTS(dynamic, STRONG_PASS);
DEFINE_STRUCT_INITIALIZER_TESTS(runtime, STRONG_PASS);
DEFINE_STRUCT_INITIALIZER_TESTS(assigned_static, STRONG_PASS);
DEFINE_STRUCT_INITIALIZER_TESTS(assigned_dynamic, STRONG_PASS);
DEFINE_STRUCT_TESTS(assigned_copy, ALWAYS_FAIL);
 
DEFINE_SCALAR_TESTS(none, STRONG_PASS);
DEFINE_STRUCT_TESTS(none, BYREF_PASS);
 
DEFINE_TEST(user, struct test_user, STRUCT, none, USER_PASS);

 
static int noinline __leaf_switch_none(int path, bool fill)
{
	switch (path) {
		 
		uint64_t var[10];

	case 1:
		target_start = &var;
		target_size = sizeof(var);
		if (fill) {
			fill_start = &var;
			fill_size = sizeof(var);

			memset(fill_start, forced_mask | 0x55, fill_size);
		}
		memcpy(check_buf, target_start, target_size);
		break;
	case 2:
		target_start = &var;
		target_size = sizeof(var);
		if (fill) {
			fill_start = &var;
			fill_size = sizeof(var);

			memset(fill_start, forced_mask | 0xaa, fill_size);
		}
		memcpy(check_buf, target_start, target_size);
		break;
	default:
		var[1] = 5;
		return var[1] & forced_mask;
	}
	return 0;
}

static noinline int leaf_switch_1_none(unsigned long sp, bool fill,
					      uint64_t *arg)
{
	return __leaf_switch_none(1, fill);
}

static noinline int leaf_switch_2_none(unsigned long sp, bool fill,
					      uint64_t *arg)
{
	return __leaf_switch_none(2, fill);
}

 
DEFINE_TEST_DRIVER(switch_1_none, uint64_t, SCALAR, ALWAYS_FAIL);
DEFINE_TEST_DRIVER(switch_2_none, uint64_t, SCALAR, ALWAYS_FAIL);

#define KUNIT_test_scalars(init)			\
		KUNIT_CASE(test_u8_ ## init),		\
		KUNIT_CASE(test_u16_ ## init),		\
		KUNIT_CASE(test_u32_ ## init),		\
		KUNIT_CASE(test_u64_ ## init),		\
		KUNIT_CASE(test_char_array_ ## init)

#define KUNIT_test_structs(init)			\
		KUNIT_CASE(test_small_hole_ ## init),	\
		KUNIT_CASE(test_big_hole_ ## init),	\
		KUNIT_CASE(test_trailing_hole_ ## init),\
		KUNIT_CASE(test_packed_ ## init)	\

static struct kunit_case stackinit_test_cases[] = {
	 
	KUNIT_test_scalars(zero),
	KUNIT_test_structs(zero),
	 
	KUNIT_test_structs(dynamic_partial),
	KUNIT_test_structs(assigned_dynamic_partial),
	 
	KUNIT_test_structs(static_partial),
	KUNIT_test_structs(static_all),
	KUNIT_test_structs(dynamic_all),
	KUNIT_test_structs(runtime_partial),
	KUNIT_test_structs(runtime_all),
	KUNIT_test_structs(assigned_static_partial),
	KUNIT_test_structs(assigned_static_all),
	KUNIT_test_structs(assigned_dynamic_all),
	 
	KUNIT_test_structs(assigned_copy),
	 
	KUNIT_test_scalars(none),
	KUNIT_CASE(test_switch_1_none),
	KUNIT_CASE(test_switch_2_none),
	 
	KUNIT_test_structs(none),
	 
	KUNIT_CASE(test_user),
	{}
};

static struct kunit_suite stackinit_test_suite = {
	.name = "stackinit",
	.test_cases = stackinit_test_cases,
};

kunit_test_suites(&stackinit_test_suite);

MODULE_LICENSE("GPL");
