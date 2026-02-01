 

 

#ifdef __OPTIMIZE__
 #error "The CPU Jitter random number generator must not be compiled with optimizations. See documentation. Use the compiler switch -O0 for compiling jitterentropy.c."
#endif

typedef	unsigned long long	__u64;
typedef	long long		__s64;
typedef	unsigned int		__u32;
typedef unsigned char		u8;
#define NULL    ((void *) 0)

 
struct rand_data {
	 
#define DATA_SIZE_BITS 256
	 
	void *hash_state;		 
	__u64 prev_time;		 
	__u64 last_delta;		 
	__s64 last_delta2;		 
	unsigned int osr;		 
#define JENT_MEMORY_BLOCKS 64
#define JENT_MEMORY_BLOCKSIZE 32
#define JENT_MEMORY_ACCESSLOOPS 128
#define JENT_MEMORY_SIZE (JENT_MEMORY_BLOCKS*JENT_MEMORY_BLOCKSIZE)
	unsigned char *mem;	 
	unsigned int memlocation;  
	unsigned int memblocks;	 
	unsigned int memblocksize;  
	unsigned int memaccessloops;  

	 
	unsigned int rct_count;			 

	 
	 
	 
#define JENT_RCT_CUTOFF		(31 - 1)	 
#define JENT_APT_CUTOFF		325			 
	 
	 
	 
#define JENT_RCT_CUTOFF_PERMANENT	(61 - 1)
#define JENT_APT_CUTOFF_PERMANENT	355
#define JENT_APT_WINDOW_SIZE	512	 
	 
#define JENT_APT_LSB		16
#define JENT_APT_WORD_MASK	(JENT_APT_LSB - 1)
	unsigned int apt_observations;	 
	unsigned int apt_count;		 
	unsigned int apt_base;		 
	unsigned int apt_base_set:1;	 
};

 
#define JENT_DISABLE_MEMORY_ACCESS (1<<2)  

 
#define JENT_ENOTIME		1  
#define JENT_ECOARSETIME	2  
#define JENT_ENOMONOTONIC	3  
#define JENT_EVARVAR		5  
#define JENT_ESTUCK		8  
#define JENT_EHEALTH		9  

 
#define JENT_ENTROPY_SAFETY_FACTOR	64

#include <linux/fips.h>
#include "jitterentropy.h"

 

 
static void jent_apt_reset(struct rand_data *ec, unsigned int delta_masked)
{
	 
	ec->apt_count = 0;
	ec->apt_base = delta_masked;
	ec->apt_observations = 0;
}

 
static void jent_apt_insert(struct rand_data *ec, unsigned int delta_masked)
{
	 
	if (!ec->apt_base_set) {
		ec->apt_base = delta_masked;
		ec->apt_base_set = 1;
		return;
	}

	if (delta_masked == ec->apt_base)
		ec->apt_count++;

	ec->apt_observations++;

	if (ec->apt_observations >= JENT_APT_WINDOW_SIZE)
		jent_apt_reset(ec, delta_masked);
}

 
static int jent_apt_permanent_failure(struct rand_data *ec)
{
	return (ec->apt_count >= JENT_APT_CUTOFF_PERMANENT) ? 1 : 0;
}

static int jent_apt_failure(struct rand_data *ec)
{
	return (ec->apt_count >= JENT_APT_CUTOFF) ? 1 : 0;
}

 

 
static void jent_rct_insert(struct rand_data *ec, int stuck)
{
	if (stuck) {
		ec->rct_count++;
	} else {
		 
		ec->rct_count = 0;
	}
}

static inline __u64 jent_delta(__u64 prev, __u64 next)
{
#define JENT_UINT64_MAX		(__u64)(~((__u64) 0))
	return (prev < next) ? (next - prev) :
			       (JENT_UINT64_MAX - prev + 1 + next);
}

 
static int jent_stuck(struct rand_data *ec, __u64 current_delta)
{
	__u64 delta2 = jent_delta(ec->last_delta, current_delta);
	__u64 delta3 = jent_delta(ec->last_delta2, delta2);

	ec->last_delta = current_delta;
	ec->last_delta2 = delta2;

	 
	jent_apt_insert(ec, current_delta);

	if (!current_delta || !delta2 || !delta3) {
		 
		jent_rct_insert(ec, 1);
		return 1;
	}

	 
	jent_rct_insert(ec, 0);

	return 0;
}

 
static int jent_rct_permanent_failure(struct rand_data *ec)
{
	return (ec->rct_count >= JENT_RCT_CUTOFF_PERMANENT) ? 1 : 0;
}

static int jent_rct_failure(struct rand_data *ec)
{
	return (ec->rct_count >= JENT_RCT_CUTOFF) ? 1 : 0;
}

 
static int jent_health_failure(struct rand_data *ec)
{
	return jent_rct_failure(ec) | jent_apt_failure(ec);
}

static int jent_permanent_health_failure(struct rand_data *ec)
{
	return jent_rct_permanent_failure(ec) | jent_apt_permanent_failure(ec);
}

 

 
static __u64 jent_loop_shuffle(unsigned int bits, unsigned int min)
{
	__u64 time = 0;
	__u64 shuffle = 0;
	unsigned int i = 0;
	unsigned int mask = (1<<bits) - 1;

	jent_get_nstime(&time);

	 
	for (i = 0; ((DATA_SIZE_BITS + bits - 1) / bits) > i; i++) {
		shuffle ^= time & mask;
		time = time >> bits;
	}

	 
	return (shuffle + (1<<min));
}

 
static int jent_condition_data(struct rand_data *ec, __u64 time, int stuck)
{
#define SHA3_HASH_LOOP (1<<3)
	struct {
		int rct_count;
		unsigned int apt_observations;
		unsigned int apt_count;
		unsigned int apt_base;
	} addtl = {
		ec->rct_count,
		ec->apt_observations,
		ec->apt_count,
		ec->apt_base
	};

	return jent_hash_time(ec->hash_state, time, (u8 *)&addtl, sizeof(addtl),
			      SHA3_HASH_LOOP, stuck);
}

 
static void jent_memaccess(struct rand_data *ec, __u64 loop_cnt)
{
	unsigned int wrap = 0;
	__u64 i = 0;
#define MAX_ACC_LOOP_BIT 7
#define MIN_ACC_LOOP_BIT 0
	__u64 acc_loop_cnt =
		jent_loop_shuffle(MAX_ACC_LOOP_BIT, MIN_ACC_LOOP_BIT);

	if (NULL == ec || NULL == ec->mem)
		return;
	wrap = ec->memblocksize * ec->memblocks;

	 
	if (loop_cnt)
		acc_loop_cnt = loop_cnt;

	for (i = 0; i < (ec->memaccessloops + acc_loop_cnt); i++) {
		unsigned char *tmpval = ec->mem + ec->memlocation;
		 
		*tmpval = (*tmpval + 1) & 0xff;
		 
		ec->memlocation = ec->memlocation + ec->memblocksize - 1;
		ec->memlocation = ec->memlocation % wrap;
	}
}

 
 
static int jent_measure_jitter(struct rand_data *ec)
{
	__u64 time = 0;
	__u64 current_delta = 0;
	int stuck;

	 
	jent_memaccess(ec, 0);

	 
	jent_get_nstime(&time);
	current_delta = jent_delta(ec->prev_time, time);
	ec->prev_time = time;

	 
	stuck = jent_stuck(ec, current_delta);

	 
	if (jent_condition_data(ec, current_delta, stuck))
		stuck = 1;

	return stuck;
}

 
static void jent_gen_entropy(struct rand_data *ec)
{
	unsigned int k = 0, safety_factor = 0;

	if (fips_enabled)
		safety_factor = JENT_ENTROPY_SAFETY_FACTOR;

	 
	jent_measure_jitter(ec);

	while (!jent_health_failure(ec)) {
		 
		if (jent_measure_jitter(ec))
			continue;

		 
		if (++k >= ((DATA_SIZE_BITS + safety_factor) * ec->osr))
			break;
	}
}

 
int jent_read_entropy(struct rand_data *ec, unsigned char *data,
		      unsigned int len)
{
	unsigned char *p = data;

	if (!ec)
		return -1;

	while (len > 0) {
		unsigned int tocopy;

		jent_gen_entropy(ec);

		if (jent_permanent_health_failure(ec)) {
			 
			return -3;
		} else if (jent_health_failure(ec)) {
			 
			if (jent_entropy_init(ec->hash_state))
				return -3;

			return -2;
		}

		if ((DATA_SIZE_BITS / 8) < len)
			tocopy = (DATA_SIZE_BITS / 8);
		else
			tocopy = len;
		if (jent_read_random_block(ec->hash_state, p, tocopy))
			return -1;

		len -= tocopy;
		p += tocopy;
	}

	return 0;
}

 

struct rand_data *jent_entropy_collector_alloc(unsigned int osr,
					       unsigned int flags,
					       void *hash_state)
{
	struct rand_data *entropy_collector;

	entropy_collector = jent_zalloc(sizeof(struct rand_data));
	if (!entropy_collector)
		return NULL;

	if (!(flags & JENT_DISABLE_MEMORY_ACCESS)) {
		 
		entropy_collector->mem = jent_zalloc(JENT_MEMORY_SIZE);
		if (!entropy_collector->mem) {
			jent_zfree(entropy_collector);
			return NULL;
		}
		entropy_collector->memblocksize = JENT_MEMORY_BLOCKSIZE;
		entropy_collector->memblocks = JENT_MEMORY_BLOCKS;
		entropy_collector->memaccessloops = JENT_MEMORY_ACCESSLOOPS;
	}

	 
	if (osr == 0)
		osr = 1;  
	entropy_collector->osr = osr;

	entropy_collector->hash_state = hash_state;

	 
	jent_gen_entropy(entropy_collector);

	return entropy_collector;
}

void jent_entropy_collector_free(struct rand_data *entropy_collector)
{
	jent_zfree(entropy_collector->mem);
	entropy_collector->mem = NULL;
	jent_zfree(entropy_collector);
}

int jent_entropy_init(void *hash_state)
{
	int i;
	__u64 delta_sum = 0;
	__u64 old_delta = 0;
	unsigned int nonstuck = 0;
	int time_backwards = 0;
	int count_mod = 0;
	int count_stuck = 0;
	struct rand_data ec = { 0 };

	 
	ec.osr = 1;
	ec.hash_state = hash_state;

	 

	 
	 
#define TESTLOOPCOUNT 1024
#define CLEARCACHE 100
	for (i = 0; (TESTLOOPCOUNT + CLEARCACHE) > i; i++) {
		__u64 time = 0;
		__u64 time2 = 0;
		__u64 delta = 0;
		unsigned int lowdelta = 0;
		int stuck;

		 
		jent_get_nstime(&time);
		ec.prev_time = time;
		jent_condition_data(&ec, time, 0);
		jent_get_nstime(&time2);

		 
		if (!time || !time2)
			return JENT_ENOTIME;
		delta = jent_delta(time, time2);
		 
		if (!delta)
			return JENT_ECOARSETIME;

		stuck = jent_stuck(&ec, delta);

		 
		if (i < CLEARCACHE)
			continue;

		if (stuck)
			count_stuck++;
		else {
			nonstuck++;

			 
			if ((nonstuck % JENT_APT_WINDOW_SIZE) == 0) {
				jent_apt_reset(&ec,
					       delta & JENT_APT_WORD_MASK);
			}
		}

		 
		if (jent_health_failure(&ec))
			return JENT_EHEALTH;

		 
		if (!(time2 > time))
			time_backwards++;

		 
		lowdelta = time2 - time;
		if (!(lowdelta % 100))
			count_mod++;

		 
		if (delta > old_delta)
			delta_sum += (delta - old_delta);
		else
			delta_sum += (old_delta - delta);
		old_delta = delta;
	}

	 
	if (time_backwards > 3)
		return JENT_ENOMONOTONIC;

	 
	if ((delta_sum) <= 1)
		return JENT_EVARVAR;

	 
	if ((TESTLOOPCOUNT/10 * 9) < count_mod)
		return JENT_ECOARSETIME;

	 
	if ((TESTLOOPCOUNT/10 * 9) < count_stuck)
		return JENT_ESTUCK;

	return 0;
}
