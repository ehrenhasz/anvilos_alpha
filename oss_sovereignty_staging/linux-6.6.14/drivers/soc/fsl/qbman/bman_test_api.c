 

#include "bman_test.h"

#define NUM_BUFS	93
#define LOOPS		3
#define BMAN_TOKEN_MASK 0x00FFFFFFFFFFLLU

static struct bman_pool *pool;
static struct bm_buffer bufs_in[NUM_BUFS] ____cacheline_aligned;
static struct bm_buffer bufs_out[NUM_BUFS] ____cacheline_aligned;
static int bufs_received;

static void bufs_init(void)
{
	int i;

	for (i = 0; i < NUM_BUFS; i++)
		bm_buffer_set64(&bufs_in[i], 0xfedc01234567LLU * i);
	bufs_received = 0;
}

static inline int bufs_cmp(const struct bm_buffer *a, const struct bm_buffer *b)
{
	if (bman_ip_rev == BMAN_REV20 || bman_ip_rev == BMAN_REV21) {

		 
		if ((bm_buffer_get64(a) & BMAN_TOKEN_MASK) <
		    (bm_buffer_get64(b) & BMAN_TOKEN_MASK))
			return -1;
		if ((bm_buffer_get64(a) & BMAN_TOKEN_MASK) >
		    (bm_buffer_get64(b) & BMAN_TOKEN_MASK))
			return 1;
	} else {
		if (bm_buffer_get64(a) < bm_buffer_get64(b))
			return -1;
		if (bm_buffer_get64(a) > bm_buffer_get64(b))
			return 1;
	}

	return 0;
}

static void bufs_confirm(void)
{
	int i, j;

	for (i = 0; i < NUM_BUFS; i++) {
		int matches = 0;

		for (j = 0; j < NUM_BUFS; j++)
			if (!bufs_cmp(&bufs_in[i], &bufs_out[j]))
				matches++;
		WARN_ON(matches != 1);
	}
}

 
void bman_test_api(void)
{
	int i, loops = LOOPS;

	bufs_init();

	pr_info("%s(): Starting\n", __func__);

	pool = bman_new_pool();
	if (!pool) {
		pr_crit("bman_new_pool() failed\n");
		goto failed;
	}

	 
do_loop:
	i = 0;
	while (i < NUM_BUFS) {
		int num = 8;

		if (i + num > NUM_BUFS)
			num = NUM_BUFS - i;
		if (bman_release(pool, bufs_in + i, num)) {
			pr_crit("bman_release() failed\n");
			goto failed;
		}
		i += num;
	}

	 
	while (i > 0) {
		int tmp, num = 8;

		if (num > i)
			num = i;
		tmp = bman_acquire(pool, bufs_out + i - num, num);
		WARN_ON(tmp != num);
		i -= num;
	}
	i = bman_acquire(pool, NULL, 1);
	WARN_ON(i > 0);

	bufs_confirm();

	if (--loops)
		goto do_loop;

	 
	bman_free_pool(pool);
	pr_info("%s(): Finished\n", __func__);
	return;

failed:
	WARN_ON(1);
}
