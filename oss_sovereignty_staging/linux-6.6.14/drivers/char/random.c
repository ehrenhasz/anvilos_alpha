
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/nodemask.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/percpu.h>
#include <linux/ptrace.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/ratelimit.h>
#include <linux/syscalls.h>
#include <linux/completion.h>
#include <linux/uuid.h>
#include <linux/uaccess.h>
#include <linux/suspend.h>
#include <linux/siphash.h>
#include <linux/sched/isolation.h>
#include <crypto/chacha.h>
#include <crypto/blake2s.h>
#include <asm/archrandom.h>
#include <asm/processor.h>
#include <asm/irq.h>
#include <asm/irq_regs.h>
#include <asm/io.h>

 

 
static enum {
	CRNG_EMPTY = 0,  
	CRNG_EARLY = 1,  
	CRNG_READY = 2   
} crng_init __read_mostly = CRNG_EMPTY;
static DEFINE_STATIC_KEY_FALSE(crng_is_ready);
#define crng_ready() (static_branch_likely(&crng_is_ready) || crng_init >= CRNG_READY)
 
static DECLARE_WAIT_QUEUE_HEAD(crng_init_wait);
static struct fasync_struct *fasync;
static ATOMIC_NOTIFIER_HEAD(random_ready_notifier);

 
static struct ratelimit_state urandom_warning =
	RATELIMIT_STATE_INIT_FLAGS("urandom_warning", HZ, 3, RATELIMIT_MSG_ON_RELEASE);
static int ratelimit_disable __read_mostly =
	IS_ENABLED(CONFIG_WARN_ALL_UNSEEDED_RANDOM);
module_param_named(ratelimit_disable, ratelimit_disable, int, 0644);
MODULE_PARM_DESC(ratelimit_disable, "Disable random ratelimit suppression");

 
bool rng_is_initialized(void)
{
	return crng_ready();
}
EXPORT_SYMBOL(rng_is_initialized);

static void __cold crng_set_ready(struct work_struct *work)
{
	static_branch_enable(&crng_is_ready);
}

 
static void try_to_generate_entropy(void);

 
int wait_for_random_bytes(void)
{
	while (!crng_ready()) {
		int ret;

		try_to_generate_entropy();
		ret = wait_event_interruptible_timeout(crng_init_wait, crng_ready(), HZ);
		if (ret)
			return ret > 0 ? 0 : ret;
	}
	return 0;
}
EXPORT_SYMBOL(wait_for_random_bytes);

 
int __cold execute_with_initialized_rng(struct notifier_block *nb)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&random_ready_notifier.lock, flags);
	if (crng_ready())
		nb->notifier_call(nb, 0, NULL);
	else
		ret = raw_notifier_chain_register((struct raw_notifier_head *)&random_ready_notifier.head, nb);
	spin_unlock_irqrestore(&random_ready_notifier.lock, flags);
	return ret;
}

#define warn_unseeded_randomness() \
	if (IS_ENABLED(CONFIG_WARN_ALL_UNSEEDED_RANDOM) && !crng_ready()) \
		printk_deferred(KERN_NOTICE "random: %s called from %pS with crng_init=%d\n", \
				__func__, (void *)_RET_IP_, crng_init)


 

enum {
	CRNG_RESEED_START_INTERVAL = HZ,
	CRNG_RESEED_INTERVAL = 60 * HZ
};

static struct {
	u8 key[CHACHA_KEY_SIZE] __aligned(__alignof__(long));
	unsigned long generation;
	spinlock_t lock;
} base_crng = {
	.lock = __SPIN_LOCK_UNLOCKED(base_crng.lock)
};

struct crng {
	u8 key[CHACHA_KEY_SIZE];
	unsigned long generation;
	local_lock_t lock;
};

static DEFINE_PER_CPU(struct crng, crngs) = {
	.generation = ULONG_MAX,
	.lock = INIT_LOCAL_LOCK(crngs.lock),
};

 
static unsigned int crng_reseed_interval(void)
{
	static bool early_boot = true;

	if (unlikely(READ_ONCE(early_boot))) {
		time64_t uptime = ktime_get_seconds();
		if (uptime >= CRNG_RESEED_INTERVAL / HZ * 2)
			WRITE_ONCE(early_boot, false);
		else
			return max_t(unsigned int, CRNG_RESEED_START_INTERVAL,
				     (unsigned int)uptime / 2 * HZ);
	}
	return CRNG_RESEED_INTERVAL;
}

 
static void extract_entropy(void *buf, size_t len);

 
static void crng_reseed(struct work_struct *work)
{
	static DECLARE_DELAYED_WORK(next_reseed, crng_reseed);
	unsigned long flags;
	unsigned long next_gen;
	u8 key[CHACHA_KEY_SIZE];

	 
	if (likely(system_unbound_wq))
		queue_delayed_work(system_unbound_wq, &next_reseed, crng_reseed_interval());

	extract_entropy(key, sizeof(key));

	 
	spin_lock_irqsave(&base_crng.lock, flags);
	memcpy(base_crng.key, key, sizeof(base_crng.key));
	next_gen = base_crng.generation + 1;
	if (next_gen == ULONG_MAX)
		++next_gen;
	WRITE_ONCE(base_crng.generation, next_gen);
	if (!static_branch_likely(&crng_is_ready))
		crng_init = CRNG_READY;
	spin_unlock_irqrestore(&base_crng.lock, flags);
	memzero_explicit(key, sizeof(key));
}

 
static void crng_fast_key_erasure(u8 key[CHACHA_KEY_SIZE],
				  u32 chacha_state[CHACHA_STATE_WORDS],
				  u8 *random_data, size_t random_data_len)
{
	u8 first_block[CHACHA_BLOCK_SIZE];

	BUG_ON(random_data_len > 32);

	chacha_init_consts(chacha_state);
	memcpy(&chacha_state[4], key, CHACHA_KEY_SIZE);
	memset(&chacha_state[12], 0, sizeof(u32) * 4);
	chacha20_block(chacha_state, first_block);

	memcpy(key, first_block, CHACHA_KEY_SIZE);
	memcpy(random_data, first_block + CHACHA_KEY_SIZE, random_data_len);
	memzero_explicit(first_block, sizeof(first_block));
}

 
static void crng_make_state(u32 chacha_state[CHACHA_STATE_WORDS],
			    u8 *random_data, size_t random_data_len)
{
	unsigned long flags;
	struct crng *crng;

	BUG_ON(random_data_len > 32);

	 
	if (!crng_ready()) {
		bool ready;

		spin_lock_irqsave(&base_crng.lock, flags);
		ready = crng_ready();
		if (!ready) {
			if (crng_init == CRNG_EMPTY)
				extract_entropy(base_crng.key, sizeof(base_crng.key));
			crng_fast_key_erasure(base_crng.key, chacha_state,
					      random_data, random_data_len);
		}
		spin_unlock_irqrestore(&base_crng.lock, flags);
		if (!ready)
			return;
	}

	local_lock_irqsave(&crngs.lock, flags);
	crng = raw_cpu_ptr(&crngs);

	 
	if (unlikely(crng->generation != READ_ONCE(base_crng.generation))) {
		spin_lock(&base_crng.lock);
		crng_fast_key_erasure(base_crng.key, chacha_state,
				      crng->key, sizeof(crng->key));
		crng->generation = base_crng.generation;
		spin_unlock(&base_crng.lock);
	}

	 
	crng_fast_key_erasure(crng->key, chacha_state, random_data, random_data_len);
	local_unlock_irqrestore(&crngs.lock, flags);
}

static void _get_random_bytes(void *buf, size_t len)
{
	u32 chacha_state[CHACHA_STATE_WORDS];
	u8 tmp[CHACHA_BLOCK_SIZE];
	size_t first_block_len;

	if (!len)
		return;

	first_block_len = min_t(size_t, 32, len);
	crng_make_state(chacha_state, buf, first_block_len);
	len -= first_block_len;
	buf += first_block_len;

	while (len) {
		if (len < CHACHA_BLOCK_SIZE) {
			chacha20_block(chacha_state, tmp);
			memcpy(buf, tmp, len);
			memzero_explicit(tmp, sizeof(tmp));
			break;
		}

		chacha20_block(chacha_state, buf);
		if (unlikely(chacha_state[12] == 0))
			++chacha_state[13];
		len -= CHACHA_BLOCK_SIZE;
		buf += CHACHA_BLOCK_SIZE;
	}

	memzero_explicit(chacha_state, sizeof(chacha_state));
}

 
void get_random_bytes(void *buf, size_t len)
{
	warn_unseeded_randomness();
	_get_random_bytes(buf, len);
}
EXPORT_SYMBOL(get_random_bytes);

static ssize_t get_random_bytes_user(struct iov_iter *iter)
{
	u32 chacha_state[CHACHA_STATE_WORDS];
	u8 block[CHACHA_BLOCK_SIZE];
	size_t ret = 0, copied;

	if (unlikely(!iov_iter_count(iter)))
		return 0;

	 
	crng_make_state(chacha_state, (u8 *)&chacha_state[4], CHACHA_KEY_SIZE);
	 
	if (iov_iter_count(iter) <= CHACHA_KEY_SIZE) {
		ret = copy_to_iter(&chacha_state[4], CHACHA_KEY_SIZE, iter);
		goto out_zero_chacha;
	}

	for (;;) {
		chacha20_block(chacha_state, block);
		if (unlikely(chacha_state[12] == 0))
			++chacha_state[13];

		copied = copy_to_iter(block, sizeof(block), iter);
		ret += copied;
		if (!iov_iter_count(iter) || copied != sizeof(block))
			break;

		BUILD_BUG_ON(PAGE_SIZE % sizeof(block) != 0);
		if (ret % PAGE_SIZE == 0) {
			if (signal_pending(current))
				break;
			cond_resched();
		}
	}

	memzero_explicit(block, sizeof(block));
out_zero_chacha:
	memzero_explicit(chacha_state, sizeof(chacha_state));
	return ret ? ret : -EFAULT;
}

 

#define DEFINE_BATCHED_ENTROPY(type)						\
struct batch_ ##type {								\
	 									\
	type entropy[CHACHA_BLOCK_SIZE * 3 / (2 * sizeof(type))];		\
	local_lock_t lock;							\
	unsigned long generation;						\
	unsigned int position;							\
};										\
										\
static DEFINE_PER_CPU(struct batch_ ##type, batched_entropy_ ##type) = {	\
	.lock = INIT_LOCAL_LOCK(batched_entropy_ ##type.lock),			\
	.position = UINT_MAX							\
};										\
										\
type get_random_ ##type(void)							\
{										\
	type ret;								\
	unsigned long flags;							\
	struct batch_ ##type *batch;						\
	unsigned long next_gen;							\
										\
	warn_unseeded_randomness();						\
										\
	if  (!crng_ready()) {							\
		_get_random_bytes(&ret, sizeof(ret));				\
		return ret;							\
	}									\
										\
	local_lock_irqsave(&batched_entropy_ ##type.lock, flags);		\
	batch = raw_cpu_ptr(&batched_entropy_##type);				\
										\
	next_gen = READ_ONCE(base_crng.generation);				\
	if (batch->position >= ARRAY_SIZE(batch->entropy) ||			\
	    next_gen != batch->generation) {					\
		_get_random_bytes(batch->entropy, sizeof(batch->entropy));	\
		batch->position = 0;						\
		batch->generation = next_gen;					\
	}									\
										\
	ret = batch->entropy[batch->position];					\
	batch->entropy[batch->position] = 0;					\
	++batch->position;							\
	local_unlock_irqrestore(&batched_entropy_ ##type.lock, flags);		\
	return ret;								\
}										\
EXPORT_SYMBOL(get_random_ ##type);

DEFINE_BATCHED_ENTROPY(u8)
DEFINE_BATCHED_ENTROPY(u16)
DEFINE_BATCHED_ENTROPY(u32)
DEFINE_BATCHED_ENTROPY(u64)

u32 __get_random_u32_below(u32 ceil)
{
	 
	u32 rand = get_random_u32();
	u64 mult;

	 
	if (unlikely(!ceil))
		return rand;

	mult = (u64)ceil * rand;
	if (unlikely((u32)mult < ceil)) {
		u32 bound = -ceil % ceil;
		while (unlikely((u32)mult < bound))
			mult = (u64)ceil * get_random_u32();
	}
	return mult >> 32;
}
EXPORT_SYMBOL(__get_random_u32_below);

#ifdef CONFIG_SMP
 
int __cold random_prepare_cpu(unsigned int cpu)
{
	 
	per_cpu_ptr(&crngs, cpu)->generation = ULONG_MAX;
	per_cpu_ptr(&batched_entropy_u8, cpu)->position = UINT_MAX;
	per_cpu_ptr(&batched_entropy_u16, cpu)->position = UINT_MAX;
	per_cpu_ptr(&batched_entropy_u32, cpu)->position = UINT_MAX;
	per_cpu_ptr(&batched_entropy_u64, cpu)->position = UINT_MAX;
	return 0;
}
#endif


 

enum {
	POOL_BITS = BLAKE2S_HASH_SIZE * 8,
	POOL_READY_BITS = POOL_BITS,  
	POOL_EARLY_BITS = POOL_READY_BITS / 2  
};

static struct {
	struct blake2s_state hash;
	spinlock_t lock;
	unsigned int init_bits;
} input_pool = {
	.hash.h = { BLAKE2S_IV0 ^ (0x01010000 | BLAKE2S_HASH_SIZE),
		    BLAKE2S_IV1, BLAKE2S_IV2, BLAKE2S_IV3, BLAKE2S_IV4,
		    BLAKE2S_IV5, BLAKE2S_IV6, BLAKE2S_IV7 },
	.hash.outlen = BLAKE2S_HASH_SIZE,
	.lock = __SPIN_LOCK_UNLOCKED(input_pool.lock),
};

static void _mix_pool_bytes(const void *buf, size_t len)
{
	blake2s_update(&input_pool.hash, buf, len);
}

 
static void mix_pool_bytes(const void *buf, size_t len)
{
	unsigned long flags;

	spin_lock_irqsave(&input_pool.lock, flags);
	_mix_pool_bytes(buf, len);
	spin_unlock_irqrestore(&input_pool.lock, flags);
}

 
static void extract_entropy(void *buf, size_t len)
{
	unsigned long flags;
	u8 seed[BLAKE2S_HASH_SIZE], next_key[BLAKE2S_HASH_SIZE];
	struct {
		unsigned long rdseed[32 / sizeof(long)];
		size_t counter;
	} block;
	size_t i, longs;

	for (i = 0; i < ARRAY_SIZE(block.rdseed);) {
		longs = arch_get_random_seed_longs(&block.rdseed[i], ARRAY_SIZE(block.rdseed) - i);
		if (longs) {
			i += longs;
			continue;
		}
		longs = arch_get_random_longs(&block.rdseed[i], ARRAY_SIZE(block.rdseed) - i);
		if (longs) {
			i += longs;
			continue;
		}
		block.rdseed[i++] = random_get_entropy();
	}

	spin_lock_irqsave(&input_pool.lock, flags);

	 
	blake2s_final(&input_pool.hash, seed);

	 
	block.counter = 0;
	blake2s(next_key, (u8 *)&block, seed, sizeof(next_key), sizeof(block), sizeof(seed));
	blake2s_init_key(&input_pool.hash, BLAKE2S_HASH_SIZE, next_key, sizeof(next_key));

	spin_unlock_irqrestore(&input_pool.lock, flags);
	memzero_explicit(next_key, sizeof(next_key));

	while (len) {
		i = min_t(size_t, len, BLAKE2S_HASH_SIZE);
		 
		++block.counter;
		blake2s(buf, (u8 *)&block, seed, i, sizeof(block), sizeof(seed));
		len -= i;
		buf += i;
	}

	memzero_explicit(seed, sizeof(seed));
	memzero_explicit(&block, sizeof(block));
}

#define credit_init_bits(bits) if (!crng_ready()) _credit_init_bits(bits)

static void __cold _credit_init_bits(size_t bits)
{
	static struct execute_work set_ready;
	unsigned int new, orig, add;
	unsigned long flags;

	if (!bits)
		return;

	add = min_t(size_t, bits, POOL_BITS);

	orig = READ_ONCE(input_pool.init_bits);
	do {
		new = min_t(unsigned int, POOL_BITS, orig + add);
	} while (!try_cmpxchg(&input_pool.init_bits, &orig, new));

	if (orig < POOL_READY_BITS && new >= POOL_READY_BITS) {
		crng_reseed(NULL);  
		if (static_key_initialized)
			execute_in_process_context(crng_set_ready, &set_ready);
		atomic_notifier_call_chain(&random_ready_notifier, 0, NULL);
		wake_up_interruptible(&crng_init_wait);
		kill_fasync(&fasync, SIGIO, POLL_IN);
		pr_notice("crng init done\n");
		if (urandom_warning.missed)
			pr_notice("%d urandom warning(s) missed due to ratelimiting\n",
				  urandom_warning.missed);
	} else if (orig < POOL_EARLY_BITS && new >= POOL_EARLY_BITS) {
		spin_lock_irqsave(&base_crng.lock, flags);
		 
		if (crng_init == CRNG_EMPTY) {
			extract_entropy(base_crng.key, sizeof(base_crng.key));
			crng_init = CRNG_EARLY;
		}
		spin_unlock_irqrestore(&base_crng.lock, flags);
	}
}


 

static bool trust_cpu __initdata = true;
static bool trust_bootloader __initdata = true;
static int __init parse_trust_cpu(char *arg)
{
	return kstrtobool(arg, &trust_cpu);
}
static int __init parse_trust_bootloader(char *arg)
{
	return kstrtobool(arg, &trust_bootloader);
}
early_param("random.trust_cpu", parse_trust_cpu);
early_param("random.trust_bootloader", parse_trust_bootloader);

static int random_pm_notification(struct notifier_block *nb, unsigned long action, void *data)
{
	unsigned long flags, entropy = random_get_entropy();

	 
	ktime_t stamps[] = { ktime_get(), ktime_get_boottime(), ktime_get_real() };

	spin_lock_irqsave(&input_pool.lock, flags);
	_mix_pool_bytes(&action, sizeof(action));
	_mix_pool_bytes(stamps, sizeof(stamps));
	_mix_pool_bytes(&entropy, sizeof(entropy));
	spin_unlock_irqrestore(&input_pool.lock, flags);

	if (crng_ready() && (action == PM_RESTORE_PREPARE ||
	    (action == PM_POST_SUSPEND && !IS_ENABLED(CONFIG_PM_AUTOSLEEP) &&
	     !IS_ENABLED(CONFIG_PM_USERSPACE_AUTOSLEEP)))) {
		crng_reseed(NULL);
		pr_notice("crng reseeded on system resumption\n");
	}
	return 0;
}

static struct notifier_block pm_notifier = { .notifier_call = random_pm_notification };

 
void __init random_init_early(const char *command_line)
{
	unsigned long entropy[BLAKE2S_BLOCK_SIZE / sizeof(long)];
	size_t i, longs, arch_bits;

#if defined(LATENT_ENTROPY_PLUGIN)
	static const u8 compiletime_seed[BLAKE2S_BLOCK_SIZE] __initconst __latent_entropy;
	_mix_pool_bytes(compiletime_seed, sizeof(compiletime_seed));
#endif

	for (i = 0, arch_bits = sizeof(entropy) * 8; i < ARRAY_SIZE(entropy);) {
		longs = arch_get_random_seed_longs(entropy, ARRAY_SIZE(entropy) - i);
		if (longs) {
			_mix_pool_bytes(entropy, sizeof(*entropy) * longs);
			i += longs;
			continue;
		}
		longs = arch_get_random_longs(entropy, ARRAY_SIZE(entropy) - i);
		if (longs) {
			_mix_pool_bytes(entropy, sizeof(*entropy) * longs);
			i += longs;
			continue;
		}
		arch_bits -= sizeof(*entropy) * 8;
		++i;
	}

	_mix_pool_bytes(init_utsname(), sizeof(*(init_utsname())));
	_mix_pool_bytes(command_line, strlen(command_line));

	 
	if (crng_ready())
		crng_reseed(NULL);
	else if (trust_cpu)
		_credit_init_bits(arch_bits);
}

 
void __init random_init(void)
{
	unsigned long entropy = random_get_entropy();
	ktime_t now = ktime_get_real();

	_mix_pool_bytes(&now, sizeof(now));
	_mix_pool_bytes(&entropy, sizeof(entropy));
	add_latent_entropy();

	 
	if (!static_branch_likely(&crng_is_ready) && crng_init >= CRNG_READY)
		crng_set_ready(NULL);

	 
	if (crng_ready())
		crng_reseed(NULL);

	WARN_ON(register_pm_notifier(&pm_notifier));

	WARN(!entropy, "Missing cycle counter and fallback timer; RNG "
		       "entropy collection will consequently suffer.");
}

 
void add_device_randomness(const void *buf, size_t len)
{
	unsigned long entropy = random_get_entropy();
	unsigned long flags;

	spin_lock_irqsave(&input_pool.lock, flags);
	_mix_pool_bytes(&entropy, sizeof(entropy));
	_mix_pool_bytes(buf, len);
	spin_unlock_irqrestore(&input_pool.lock, flags);
}
EXPORT_SYMBOL(add_device_randomness);

 
void add_hwgenerator_randomness(const void *buf, size_t len, size_t entropy, bool sleep_after)
{
	mix_pool_bytes(buf, len);
	credit_init_bits(entropy);

	 
	if (sleep_after && !kthread_should_stop() && (crng_ready() || !entropy))
		schedule_timeout_interruptible(crng_reseed_interval());
}
EXPORT_SYMBOL_GPL(add_hwgenerator_randomness);

 
void __init add_bootloader_randomness(const void *buf, size_t len)
{
	mix_pool_bytes(buf, len);
	if (trust_bootloader)
		credit_init_bits(len * 8);
}

#if IS_ENABLED(CONFIG_VMGENID)
static BLOCKING_NOTIFIER_HEAD(vmfork_chain);

 
void __cold add_vmfork_randomness(const void *unique_vm_id, size_t len)
{
	add_device_randomness(unique_vm_id, len);
	if (crng_ready()) {
		crng_reseed(NULL);
		pr_notice("crng reseeded due to virtual machine fork\n");
	}
	blocking_notifier_call_chain(&vmfork_chain, 0, NULL);
}
#if IS_MODULE(CONFIG_VMGENID)
EXPORT_SYMBOL_GPL(add_vmfork_randomness);
#endif

int __cold register_random_vmfork_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vmfork_chain, nb);
}
EXPORT_SYMBOL_GPL(register_random_vmfork_notifier);

int __cold unregister_random_vmfork_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vmfork_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_random_vmfork_notifier);
#endif

struct fast_pool {
	unsigned long pool[4];
	unsigned long last;
	unsigned int count;
	struct timer_list mix;
};

static void mix_interrupt_randomness(struct timer_list *work);

static DEFINE_PER_CPU(struct fast_pool, irq_randomness) = {
#ifdef CONFIG_64BIT
#define FASTMIX_PERM SIPHASH_PERMUTATION
	.pool = { SIPHASH_CONST_0, SIPHASH_CONST_1, SIPHASH_CONST_2, SIPHASH_CONST_3 },
#else
#define FASTMIX_PERM HSIPHASH_PERMUTATION
	.pool = { HSIPHASH_CONST_0, HSIPHASH_CONST_1, HSIPHASH_CONST_2, HSIPHASH_CONST_3 },
#endif
	.mix = __TIMER_INITIALIZER(mix_interrupt_randomness, 0)
};

 
static void fast_mix(unsigned long s[4], unsigned long v1, unsigned long v2)
{
	s[3] ^= v1;
	FASTMIX_PERM(s[0], s[1], s[2], s[3]);
	s[0] ^= v1;
	s[3] ^= v2;
	FASTMIX_PERM(s[0], s[1], s[2], s[3]);
	s[0] ^= v2;
}

#ifdef CONFIG_SMP
 
int __cold random_online_cpu(unsigned int cpu)
{
	 
	per_cpu_ptr(&irq_randomness, cpu)->count = 0;
	return 0;
}
#endif

static void mix_interrupt_randomness(struct timer_list *work)
{
	struct fast_pool *fast_pool = container_of(work, struct fast_pool, mix);
	 
	unsigned long pool[2];
	unsigned int count;

	 
	local_irq_disable();
	if (fast_pool != this_cpu_ptr(&irq_randomness)) {
		local_irq_enable();
		return;
	}

	 
	memcpy(pool, fast_pool->pool, sizeof(pool));
	count = fast_pool->count;
	fast_pool->count = 0;
	fast_pool->last = jiffies;
	local_irq_enable();

	mix_pool_bytes(pool, sizeof(pool));
	credit_init_bits(clamp_t(unsigned int, (count & U16_MAX) / 64, 1, sizeof(pool) * 8));

	memzero_explicit(pool, sizeof(pool));
}

void add_interrupt_randomness(int irq)
{
	enum { MIX_INFLIGHT = 1U << 31 };
	unsigned long entropy = random_get_entropy();
	struct fast_pool *fast_pool = this_cpu_ptr(&irq_randomness);
	struct pt_regs *regs = get_irq_regs();
	unsigned int new_count;

	fast_mix(fast_pool->pool, entropy,
		 (regs ? instruction_pointer(regs) : _RET_IP_) ^ swab(irq));
	new_count = ++fast_pool->count;

	if (new_count & MIX_INFLIGHT)
		return;

	if (new_count < 1024 && !time_is_before_jiffies(fast_pool->last + HZ))
		return;

	fast_pool->count |= MIX_INFLIGHT;
	if (!timer_pending(&fast_pool->mix)) {
		fast_pool->mix.expires = jiffies;
		add_timer_on(&fast_pool->mix, raw_smp_processor_id());
	}
}
EXPORT_SYMBOL_GPL(add_interrupt_randomness);

 
struct timer_rand_state {
	unsigned long last_time;
	long last_delta, last_delta2;
};

 
static void add_timer_randomness(struct timer_rand_state *state, unsigned int num)
{
	unsigned long entropy = random_get_entropy(), now = jiffies, flags;
	long delta, delta2, delta3;
	unsigned int bits;

	 
	if (in_hardirq()) {
		fast_mix(this_cpu_ptr(&irq_randomness)->pool, entropy, num);
	} else {
		spin_lock_irqsave(&input_pool.lock, flags);
		_mix_pool_bytes(&entropy, sizeof(entropy));
		_mix_pool_bytes(&num, sizeof(num));
		spin_unlock_irqrestore(&input_pool.lock, flags);
	}

	if (crng_ready())
		return;

	 
	delta = now - READ_ONCE(state->last_time);
	WRITE_ONCE(state->last_time, now);

	delta2 = delta - READ_ONCE(state->last_delta);
	WRITE_ONCE(state->last_delta, delta);

	delta3 = delta2 - READ_ONCE(state->last_delta2);
	WRITE_ONCE(state->last_delta2, delta2);

	if (delta < 0)
		delta = -delta;
	if (delta2 < 0)
		delta2 = -delta2;
	if (delta3 < 0)
		delta3 = -delta3;
	if (delta > delta2)
		delta = delta2;
	if (delta > delta3)
		delta = delta3;

	 
	bits = min(fls(delta >> 1), 11);

	 
	if (in_hardirq())
		this_cpu_ptr(&irq_randomness)->count += max(1u, bits * 64) - 1;
	else
		_credit_init_bits(bits);
}

void add_input_randomness(unsigned int type, unsigned int code, unsigned int value)
{
	static unsigned char last_value;
	static struct timer_rand_state input_timer_state = { INITIAL_JIFFIES };

	 
	if (value == last_value)
		return;

	last_value = value;
	add_timer_randomness(&input_timer_state,
			     (type << 4) ^ code ^ (code >> 4) ^ value);
}
EXPORT_SYMBOL_GPL(add_input_randomness);

#ifdef CONFIG_BLOCK
void add_disk_randomness(struct gendisk *disk)
{
	if (!disk || !disk->random)
		return;
	 
	add_timer_randomness(disk->random, 0x100 + disk_devt(disk));
}
EXPORT_SYMBOL_GPL(add_disk_randomness);

void __cold rand_initialize_disk(struct gendisk *disk)
{
	struct timer_rand_state *state;

	 
	state = kzalloc(sizeof(struct timer_rand_state), GFP_KERNEL);
	if (state) {
		state->last_time = INITIAL_JIFFIES;
		disk->random = state;
	}
}
#endif

struct entropy_timer_state {
	unsigned long entropy;
	struct timer_list timer;
	atomic_t samples;
	unsigned int samples_per_bit;
};

 
static void __cold entropy_timer(struct timer_list *timer)
{
	struct entropy_timer_state *state = container_of(timer, struct entropy_timer_state, timer);
	unsigned long entropy = random_get_entropy();

	mix_pool_bytes(&entropy, sizeof(entropy));
	if (atomic_inc_return(&state->samples) % state->samples_per_bit == 0)
		credit_init_bits(1);
}

 
static void __cold try_to_generate_entropy(void)
{
	enum { NUM_TRIAL_SAMPLES = 8192, MAX_SAMPLES_PER_BIT = HZ / 15 };
	u8 stack_bytes[sizeof(struct entropy_timer_state) + SMP_CACHE_BYTES - 1];
	struct entropy_timer_state *stack = PTR_ALIGN((void *)stack_bytes, SMP_CACHE_BYTES);
	unsigned int i, num_different = 0;
	unsigned long last = random_get_entropy();
	int cpu = -1;

	for (i = 0; i < NUM_TRIAL_SAMPLES - 1; ++i) {
		stack->entropy = random_get_entropy();
		if (stack->entropy != last)
			++num_different;
		last = stack->entropy;
	}
	stack->samples_per_bit = DIV_ROUND_UP(NUM_TRIAL_SAMPLES, num_different + 1);
	if (stack->samples_per_bit > MAX_SAMPLES_PER_BIT)
		return;

	atomic_set(&stack->samples, 0);
	timer_setup_on_stack(&stack->timer, entropy_timer, 0);
	while (!crng_ready() && !signal_pending(current)) {
		 
		if (!timer_pending(&stack->timer) && try_to_del_timer_sync(&stack->timer) >= 0) {
			struct cpumask timer_cpus;
			unsigned int num_cpus;

			 
			preempt_disable();

			 
			cpumask_and(&timer_cpus, housekeeping_cpumask(HK_TYPE_TIMER), cpu_online_mask);
			num_cpus = cpumask_weight(&timer_cpus);
			 
			if (unlikely(num_cpus == 0)) {
				timer_cpus = *cpu_online_mask;
				num_cpus = cpumask_weight(&timer_cpus);
			}

			 
			do {
				cpu = cpumask_next(cpu, &timer_cpus);
				if (cpu >= nr_cpu_ids)
					cpu = cpumask_first(&timer_cpus);
			} while (cpu == smp_processor_id() && num_cpus > 1);

			 
			stack->timer.expires = jiffies;

			add_timer_on(&stack->timer, cpu);

			preempt_enable();
		}
		mix_pool_bytes(&stack->entropy, sizeof(stack->entropy));
		schedule();
		stack->entropy = random_get_entropy();
	}
	mix_pool_bytes(&stack->entropy, sizeof(stack->entropy));

	del_timer_sync(&stack->timer);
	destroy_timer_on_stack(&stack->timer);
}


 

SYSCALL_DEFINE3(getrandom, char __user *, ubuf, size_t, len, unsigned int, flags)
{
	struct iov_iter iter;
	struct iovec iov;
	int ret;

	if (flags & ~(GRND_NONBLOCK | GRND_RANDOM | GRND_INSECURE))
		return -EINVAL;

	 
	if ((flags & (GRND_INSECURE | GRND_RANDOM)) == (GRND_INSECURE | GRND_RANDOM))
		return -EINVAL;

	if (!crng_ready() && !(flags & GRND_INSECURE)) {
		if (flags & GRND_NONBLOCK)
			return -EAGAIN;
		ret = wait_for_random_bytes();
		if (unlikely(ret))
			return ret;
	}

	ret = import_single_range(ITER_DEST, ubuf, len, &iov, &iter);
	if (unlikely(ret))
		return ret;
	return get_random_bytes_user(&iter);
}

static __poll_t random_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &crng_init_wait, wait);
	return crng_ready() ? EPOLLIN | EPOLLRDNORM : EPOLLOUT | EPOLLWRNORM;
}

static ssize_t write_pool_user(struct iov_iter *iter)
{
	u8 block[BLAKE2S_BLOCK_SIZE];
	ssize_t ret = 0;
	size_t copied;

	if (unlikely(!iov_iter_count(iter)))
		return 0;

	for (;;) {
		copied = copy_from_iter(block, sizeof(block), iter);
		ret += copied;
		mix_pool_bytes(block, copied);
		if (!iov_iter_count(iter) || copied != sizeof(block))
			break;

		BUILD_BUG_ON(PAGE_SIZE % sizeof(block) != 0);
		if (ret % PAGE_SIZE == 0) {
			if (signal_pending(current))
				break;
			cond_resched();
		}
	}

	memzero_explicit(block, sizeof(block));
	return ret ? ret : -EFAULT;
}

static ssize_t random_write_iter(struct kiocb *kiocb, struct iov_iter *iter)
{
	return write_pool_user(iter);
}

static ssize_t urandom_read_iter(struct kiocb *kiocb, struct iov_iter *iter)
{
	static int maxwarn = 10;

	 
	if (!crng_ready())
		try_to_generate_entropy();

	if (!crng_ready()) {
		if (!ratelimit_disable && maxwarn <= 0)
			++urandom_warning.missed;
		else if (ratelimit_disable || __ratelimit(&urandom_warning)) {
			--maxwarn;
			pr_notice("%s: uninitialized urandom read (%zu bytes read)\n",
				  current->comm, iov_iter_count(iter));
		}
	}

	return get_random_bytes_user(iter);
}

static ssize_t random_read_iter(struct kiocb *kiocb, struct iov_iter *iter)
{
	int ret;

	if (!crng_ready() &&
	    ((kiocb->ki_flags & (IOCB_NOWAIT | IOCB_NOIO)) ||
	     (kiocb->ki_filp->f_flags & O_NONBLOCK)))
		return -EAGAIN;

	ret = wait_for_random_bytes();
	if (ret != 0)
		return ret;
	return get_random_bytes_user(iter);
}

static long random_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int __user *p = (int __user *)arg;
	int ent_count;

	switch (cmd) {
	case RNDGETENTCNT:
		 
		if (put_user(input_pool.init_bits, p))
			return -EFAULT;
		return 0;
	case RNDADDTOENTCNT:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (get_user(ent_count, p))
			return -EFAULT;
		if (ent_count < 0)
			return -EINVAL;
		credit_init_bits(ent_count);
		return 0;
	case RNDADDENTROPY: {
		struct iov_iter iter;
		struct iovec iov;
		ssize_t ret;
		int len;

		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (get_user(ent_count, p++))
			return -EFAULT;
		if (ent_count < 0)
			return -EINVAL;
		if (get_user(len, p++))
			return -EFAULT;
		ret = import_single_range(ITER_SOURCE, p, len, &iov, &iter);
		if (unlikely(ret))
			return ret;
		ret = write_pool_user(&iter);
		if (unlikely(ret < 0))
			return ret;
		 
		if (unlikely(ret != len))
			return -EFAULT;
		credit_init_bits(ent_count);
		return 0;
	}
	case RNDZAPENTCNT:
	case RNDCLEARPOOL:
		 
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		return 0;
	case RNDRESEEDCRNG:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if (!crng_ready())
			return -ENODATA;
		crng_reseed(NULL);
		return 0;
	default:
		return -EINVAL;
	}
}

static int random_fasync(int fd, struct file *filp, int on)
{
	return fasync_helper(fd, filp, on, &fasync);
}

const struct file_operations random_fops = {
	.read_iter = random_read_iter,
	.write_iter = random_write_iter,
	.poll = random_poll,
	.unlocked_ioctl = random_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
	.fasync = random_fasync,
	.llseek = noop_llseek,
	.splice_read = copy_splice_read,
	.splice_write = iter_file_splice_write,
};

const struct file_operations urandom_fops = {
	.read_iter = urandom_read_iter,
	.write_iter = random_write_iter,
	.unlocked_ioctl = random_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
	.fasync = random_fasync,
	.llseek = noop_llseek,
	.splice_read = copy_splice_read,
	.splice_write = iter_file_splice_write,
};


 

#ifdef CONFIG_SYSCTL

#include <linux/sysctl.h>

static int sysctl_random_min_urandom_seed = CRNG_RESEED_INTERVAL / HZ;
static int sysctl_random_write_wakeup_bits = POOL_READY_BITS;
static int sysctl_poolsize = POOL_BITS;
static u8 sysctl_bootid[UUID_SIZE];

 
static int proc_do_uuid(struct ctl_table *table, int write, void *buf,
			size_t *lenp, loff_t *ppos)
{
	u8 tmp_uuid[UUID_SIZE], *uuid;
	char uuid_string[UUID_STRING_LEN + 1];
	struct ctl_table fake_table = {
		.data = uuid_string,
		.maxlen = UUID_STRING_LEN
	};

	if (write)
		return -EPERM;

	uuid = table->data;
	if (!uuid) {
		uuid = tmp_uuid;
		generate_random_uuid(uuid);
	} else {
		static DEFINE_SPINLOCK(bootid_spinlock);

		spin_lock(&bootid_spinlock);
		if (!uuid[8])
			generate_random_uuid(uuid);
		spin_unlock(&bootid_spinlock);
	}

	snprintf(uuid_string, sizeof(uuid_string), "%pU", uuid);
	return proc_dostring(&fake_table, 0, buf, lenp, ppos);
}

 
static int proc_do_rointvec(struct ctl_table *table, int write, void *buf,
			    size_t *lenp, loff_t *ppos)
{
	return write ? 0 : proc_dointvec(table, 0, buf, lenp, ppos);
}

static struct ctl_table random_table[] = {
	{
		.procname	= "poolsize",
		.data		= &sysctl_poolsize,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "entropy_avail",
		.data		= &input_pool.init_bits,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "write_wakeup_threshold",
		.data		= &sysctl_random_write_wakeup_bits,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_do_rointvec,
	},
	{
		.procname	= "urandom_min_reseed_secs",
		.data		= &sysctl_random_min_urandom_seed,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_do_rointvec,
	},
	{
		.procname	= "boot_id",
		.data		= &sysctl_bootid,
		.mode		= 0444,
		.proc_handler	= proc_do_uuid,
	},
	{
		.procname	= "uuid",
		.mode		= 0444,
		.proc_handler	= proc_do_uuid,
	},
	{ }
};

 
static int __init random_sysctls_init(void)
{
	register_sysctl_init("kernel/random", random_table);
	return 0;
}
device_initcall(random_sysctls_init);
#endif
