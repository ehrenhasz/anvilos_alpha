 
 

#include "includes.h"

#include <sys/types.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "ssherr.h"
#include "sshbuf.h"

#define NUM_FUZZ_TESTS (1 << 18)

void sshbuf_fuzz_tests(void);

void
sshbuf_fuzz_tests(void)
{
	struct sshbuf *p1;
	u_char *dp;
	size_t sz, sz2, i, ntests = NUM_FUZZ_TESTS;
	u_int32_t r;
	int ret;

	if (test_is_fast())
		ntests >>= 2;
	if (test_is_slow())
		ntests <<= 2;

	 
	TEST_START("fuzz alloc/dealloc");
	p1 = sshbuf_new();
	ASSERT_INT_EQ(sshbuf_set_max_size(p1, 16 * 1024), 0);
	ASSERT_PTR_NE(p1, NULL);
	ASSERT_PTR_NE(sshbuf_ptr(p1), NULL);
	ASSERT_MEM_ZERO_NE(sshbuf_ptr(p1), sshbuf_len(p1));
	for (i = 0; i < ntests; i++) {
		r = arc4random_uniform(10);
		if (r == 0) {
			 
			r = arc4random_uniform(10);
 fuzz_reserve:
			sz = sshbuf_avail(p1);
			sz2 = sshbuf_len(p1);
			ret = sshbuf_reserve(p1, r, &dp);
			if (ret < 0) {
				ASSERT_PTR_EQ(dp, NULL);
				ASSERT_SIZE_T_LT(sz, r);
				ASSERT_SIZE_T_EQ(sshbuf_avail(p1), sz);
				ASSERT_SIZE_T_EQ(sshbuf_len(p1), sz2);
			} else {
				ASSERT_PTR_NE(dp, NULL);
				ASSERT_SIZE_T_GE(sz, r);
				ASSERT_SIZE_T_EQ(sshbuf_avail(p1), sz - r);
				ASSERT_SIZE_T_EQ(sshbuf_len(p1), sz2 + r);
				memset(dp, arc4random_uniform(255) + 1, r);
			}
		} else if (r < 3) {
			 
			r = arc4random_uniform(8 * 1024);
			goto fuzz_reserve;
		} else if (r == 3) {
			 
			r = arc4random_uniform(10);
 fuzz_consume:
			sz = sshbuf_avail(p1);
			sz2 = sshbuf_len(p1);
			 
			ret = ((arc4random() & 1) ?
			    sshbuf_consume : sshbuf_consume_end)(p1, r);
			if (ret < 0) {
				ASSERT_SIZE_T_LT(sz2, r);
				ASSERT_SIZE_T_EQ(sshbuf_avail(p1), sz);
				ASSERT_SIZE_T_EQ(sshbuf_len(p1), sz2);
			} else {
				ASSERT_SIZE_T_GE(sz2, r);
				ASSERT_SIZE_T_EQ(sshbuf_avail(p1), sz + r);
				ASSERT_SIZE_T_EQ(sshbuf_len(p1), sz2 - r);
			}
		} else if (r < 8) {
			 
			r = arc4random_uniform(2 * 1024);
			goto fuzz_consume;
		} else if (r == 8) {
			 
			r = arc4random_uniform(16 * 1024);
			sz = sshbuf_max_size(p1);
			if (sshbuf_set_max_size(p1, r) < 0)
				ASSERT_SIZE_T_EQ(sshbuf_max_size(p1), sz);
			else
				ASSERT_SIZE_T_EQ(sshbuf_max_size(p1), r);
		} else {
			if (arc4random_uniform(8192) == 0) {
				 
				ASSERT_PTR_NE(sshbuf_ptr(p1), NULL);
				ASSERT_MEM_ZERO_NE(sshbuf_ptr(p1), sshbuf_len(p1));
				sshbuf_free(p1);
				p1 = sshbuf_new();
				ASSERT_PTR_NE(p1, NULL);
				ASSERT_INT_EQ(sshbuf_set_max_size(p1,
				    16 * 1024), 0);
			} else {
				 
				 
				arc4random_buf(&r, sizeof(r));
				while (r < SSHBUF_SIZE_MAX / 2) {
					r <<= 1;
					r |= arc4random() & 1;
				}
				goto fuzz_reserve;
			}
		}
		ASSERT_PTR_NE(sshbuf_ptr(p1), NULL);
		ASSERT_SIZE_T_LE(sshbuf_max_size(p1), 16 * 1024);
	}
	ASSERT_PTR_NE(sshbuf_ptr(p1), NULL);
	ASSERT_MEM_ZERO_NE(sshbuf_ptr(p1), sshbuf_len(p1));
	sshbuf_free(p1);
	TEST_DONE();
}
