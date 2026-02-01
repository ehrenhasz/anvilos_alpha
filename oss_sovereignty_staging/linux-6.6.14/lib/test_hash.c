
 

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/hash.h>
#include <linux/stringhash.h>
#include <kunit/test.h>

 
static u32 __attribute_const__
xorshift(u32 seed)
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

 
static u8 __attribute_const__
mod255(u32 x)
{
	x = (x & 0xffff) + (x >> 16);	 
	x = (x & 0xff) + (x >> 8);	 
	x = (x & 0xff) + (x >> 8);	 
	x = (x & 0xff) + (x >> 8);	 
	return x;
}

 
static void fill_buf(char *buf, size_t len, u32 seed)
{
	size_t i;

	for (i = 0; i < len; i++) {
		seed = xorshift(seed);
		buf[i] = mod255(seed);
	}
}

 
struct test_hash_params {
         
	unsigned long long *h64;
         
	u32 h0;
         
	u32 h1;
         
	u32 h2;
         
	u32 (*hash_or)[33];
};

#ifdef HAVE_ARCH__HASH_32
static void
test_int__hash_32(struct kunit *test, struct test_hash_params *params)
{
	params->hash_or[1][0] |= params->h2 = __hash_32_generic(params->h0);
#if HAVE_ARCH__HASH_32 == 1
	KUNIT_EXPECT_EQ_MSG(test, params->h1, params->h2,
			    "__hash_32(%#x) = %#x != __hash_32_generic() = %#x",
			    params->h0, params->h1, params->h2);
#endif
}
#endif

#ifdef HAVE_ARCH_HASH_64
static void
test_int_hash_64(struct kunit *test, struct test_hash_params *params, u32 const *m, int *k)
{
	params->h2 = hash_64_generic(*params->h64, *k);
#if HAVE_ARCH_HASH_64 == 1
	KUNIT_EXPECT_EQ_MSG(test, params->h1, params->h2,
			    "hash_64(%#llx, %d) = %#x != hash_64_generic() = %#x",
			    *params->h64, *k, params->h1, params->h2);
#else
	KUNIT_EXPECT_LE_MSG(test, params->h1, params->h2,
			    "hash_64_generic(%#llx, %d) = %#x > %#x",
			    *params->h64, *k, params->h1, *m);
#endif
}
#endif

 
static void
test_int_hash(struct kunit *test, unsigned long long h64, u32 hash_or[2][33])
{
	int k;
	struct test_hash_params params = { &h64, (u32)h64, 0, 0, hash_or };

	 
	hash_or[0][0] |= params.h1 = __hash_32(params.h0);
#ifdef HAVE_ARCH__HASH_32
	test_int__hash_32(test, &params);
#endif

	 
	for (k = 1; k <= 32; k++) {
		u32 const m = ((u32)2 << (k-1)) - 1;	 

		 
		hash_or[0][k] |= params.h1 = hash_32(params.h0, k);
		KUNIT_EXPECT_LE_MSG(test, params.h1, m,
				    "hash_32(%#x, %d) = %#x > %#x",
				    params.h0, k, params.h1, m);

		 
		hash_or[1][k] |= params.h1 = hash_64(h64, k);
		KUNIT_EXPECT_LE_MSG(test, params.h1, m,
				    "hash_64(%#llx, %d) = %#x > %#x",
				    h64, k, params.h1, m);
#ifdef HAVE_ARCH_HASH_64
		test_int_hash_64(test, &params, &m, &k);
#endif
	}
}

#define SIZE 256	 

static void test_string_or(struct kunit *test)
{
	char buf[SIZE+1];
	u32 string_or = 0;
	int i, j;

	fill_buf(buf, SIZE, 1);

	 
	for (j = SIZE; j > 0; --j) {
		buf[j] = '\0';

		for (i = 0; i <= j; i++) {
			u32 h0 = full_name_hash(buf+i, buf+i, j-i);

			string_or |= h0;
		}  
	}  

	 
	KUNIT_EXPECT_EQ_MSG(test, string_or, -1u,
			    "OR of all string hash results = %#x != %#x",
			    string_or, -1u);
}

static void test_hash_or(struct kunit *test)
{
	char buf[SIZE+1];
	u32 hash_or[2][33] = { { 0, } };
	unsigned long long h64 = 0;
	int i, j;

	fill_buf(buf, SIZE, 1);

	 
	for (j = SIZE; j > 0; --j) {
		buf[j] = '\0';

		for (i = 0; i <= j; i++) {
			u64 hashlen = hashlen_string(buf+i, buf+i);
			u32 h0 = full_name_hash(buf+i, buf+i, j-i);

			 
			KUNIT_EXPECT_EQ_MSG(test, hashlen_len(hashlen), j-i,
					    "hashlen_string(%d..%d) returned length %u, expected %d",
					    i, j, hashlen_len(hashlen), j-i);
			 
			KUNIT_EXPECT_EQ_MSG(test, hashlen_hash(hashlen), h0,
					    "hashlen_string(%d..%d) = %08x != full_name_hash() = %08x",
					    i, j, hashlen_hash(hashlen), h0);

			h64 = h64 << 32 | h0;	 
			test_int_hash(test, h64, hash_or);
		}  
	}  

	KUNIT_EXPECT_EQ_MSG(test, hash_or[0][0], -1u,
			    "OR of all __hash_32 results = %#x != %#x",
			    hash_or[0][0], -1u);
#ifdef HAVE_ARCH__HASH_32
#if HAVE_ARCH__HASH_32 != 1	 
	KUNIT_EXPECT_EQ_MSG(test, hash_or[1][0], -1u,
			    "OR of all __hash_32_generic results = %#x != %#x",
			    hash_or[1][0], -1u);
#endif
#endif

	 
	for (i = 1; i <= 32; i++) {
		u32 const m = ((u32)2 << (i-1)) - 1;	 

		KUNIT_EXPECT_EQ_MSG(test, hash_or[0][i], m,
				    "OR of all hash_32(%d) results = %#x (%#x expected)",
				    i, hash_or[0][i], m);
		KUNIT_EXPECT_EQ_MSG(test, hash_or[1][i], m,
				    "OR of all hash_64(%d) results = %#x (%#x expected)",
				    i, hash_or[1][i], m);
	}
}

static struct kunit_case hash_test_cases[] __refdata = {
	KUNIT_CASE(test_string_or),
	KUNIT_CASE(test_hash_or),
	{}
};

static struct kunit_suite hash_test_suite = {
	.name = "hash",
	.test_cases = hash_test_cases,
};


kunit_test_suite(hash_test_suite);

MODULE_LICENSE("GPL");
