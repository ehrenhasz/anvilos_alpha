
 
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/ras.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#include <asm/mce.h>

#include "debugfs.h"

 

#undef pr_fmt
#define pr_fmt(fmt) "RAS: " fmt

 
#define DECAY_BITS		2
#define DECAY_MASK		((1ULL << DECAY_BITS) - 1)
#define MAX_ELEMS		(PAGE_SIZE / sizeof(u64))

 
#define CLEAN_ELEMS		(MAX_ELEMS >> DECAY_BITS)

 
#define COUNT_BITS		(PAGE_SHIFT - DECAY_BITS)
#define COUNT_MASK		((1ULL << COUNT_BITS) - 1)
#define FULL_COUNT_MASK		(PAGE_SIZE - 1)

 

#define PFN(e)			((e) >> PAGE_SHIFT)
#define DECAY(e)		(((e) >> COUNT_BITS) & DECAY_MASK)
#define COUNT(e)		((unsigned int)(e) & COUNT_MASK)
#define FULL_COUNT(e)		((e) & (PAGE_SIZE - 1))

static struct ce_array {
	u64 *array;			 
	unsigned int n;			 

	unsigned int decay_count;	 

	u64 pfns_poisoned;		 

	u64 ces_entered;		 

	u64 decays_done;		 

	union {
		struct {
			__u32	disabled : 1,	 
			__resv   : 31;
		};
		__u32 flags;
	};
} ce_arr;

static DEFINE_MUTEX(ce_mutex);
static u64 dfs_pfn;

 
static u64 action_threshold = COUNT_MASK;

 
#define CEC_DECAY_DEFAULT_INTERVAL	24 * 60 * 60	 
#define CEC_DECAY_MIN_INTERVAL		 1 * 60 * 60	 
#define CEC_DECAY_MAX_INTERVAL	   30 *	24 * 60 * 60	 
static struct delayed_work cec_work;
static u64 decay_interval = CEC_DECAY_DEFAULT_INTERVAL;

 
static void do_spring_cleaning(struct ce_array *ca)
{
	int i;

	for (i = 0; i < ca->n; i++) {
		u8 decay = DECAY(ca->array[i]);

		if (!decay)
			continue;

		decay--;

		ca->array[i] &= ~(DECAY_MASK << COUNT_BITS);
		ca->array[i] |= (decay << COUNT_BITS);
	}
	ca->decay_count = 0;
	ca->decays_done++;
}

 
static void cec_mod_work(unsigned long interval)
{
	unsigned long iv;

	iv = interval * HZ;
	mod_delayed_work(system_wq, &cec_work, round_jiffies(iv));
}

static void cec_work_fn(struct work_struct *work)
{
	mutex_lock(&ce_mutex);
	do_spring_cleaning(&ce_arr);
	mutex_unlock(&ce_mutex);

	cec_mod_work(decay_interval);
}

 
static int __find_elem(struct ce_array *ca, u64 pfn, unsigned int *to)
{
	int min = 0, max = ca->n - 1;
	u64 this_pfn;

	while (min <= max) {
		int i = (min + max) >> 1;

		this_pfn = PFN(ca->array[i]);

		if (this_pfn < pfn)
			min = i + 1;
		else if (this_pfn > pfn)
			max = i - 1;
		else if (this_pfn == pfn) {
			if (to)
				*to = i;

			return i;
		}
	}

	 
	if (to)
		*to = min;

	return -ENOKEY;
}

static int find_elem(struct ce_array *ca, u64 pfn, unsigned int *to)
{
	WARN_ON(!to);

	if (!ca->n) {
		*to = 0;
		return -ENOKEY;
	}
	return __find_elem(ca, pfn, to);
}

static void del_elem(struct ce_array *ca, int idx)
{
	 
	if (ca->n - (idx + 1))
		memmove((void *)&ca->array[idx],
			(void *)&ca->array[idx + 1],
			(ca->n - (idx + 1)) * sizeof(u64));

	ca->n--;
}

static u64 del_lru_elem_unlocked(struct ce_array *ca)
{
	unsigned int min = FULL_COUNT_MASK;
	int i, min_idx = 0;

	for (i = 0; i < ca->n; i++) {
		unsigned int this = FULL_COUNT(ca->array[i]);

		if (min > this) {
			min = this;
			min_idx = i;
		}
	}

	del_elem(ca, min_idx);

	return PFN(ca->array[min_idx]);
}

 
static u64 __maybe_unused del_lru_elem(void)
{
	struct ce_array *ca = &ce_arr;
	u64 pfn;

	if (!ca->n)
		return 0;

	mutex_lock(&ce_mutex);
	pfn = del_lru_elem_unlocked(ca);
	mutex_unlock(&ce_mutex);

	return pfn;
}

static bool sanity_check(struct ce_array *ca)
{
	bool ret = false;
	u64 prev = 0;
	int i;

	for (i = 0; i < ca->n; i++) {
		u64 this = PFN(ca->array[i]);

		if (WARN(prev > this, "prev: 0x%016llx <-> this: 0x%016llx\n", prev, this))
			ret = true;

		prev = this;
	}

	if (!ret)
		return ret;

	pr_info("Sanity check dump:\n{ n: %d\n", ca->n);
	for (i = 0; i < ca->n; i++) {
		u64 this = PFN(ca->array[i]);

		pr_info(" %03d: [%016llx|%03llx]\n", i, this, FULL_COUNT(ca->array[i]));
	}
	pr_info("}\n");

	return ret;
}

 
static int cec_add_elem(u64 pfn)
{
	struct ce_array *ca = &ce_arr;
	int count, err, ret = 0;
	unsigned int to = 0;

	 
	if (!ce_arr.array || ce_arr.disabled)
		return -ENODEV;

	mutex_lock(&ce_mutex);

	ca->ces_entered++;

	 
	if (ca->n == MAX_ELEMS)
		WARN_ON(!del_lru_elem_unlocked(ca));

	err = find_elem(ca, pfn, &to);
	if (err < 0) {
		 
		memmove((void *)&ca->array[to + 1],
			(void *)&ca->array[to],
			(ca->n - to) * sizeof(u64));

		ca->array[to] = pfn << PAGE_SHIFT;
		ca->n++;
	}

	 
	ca->array[to] |= DECAY_MASK << COUNT_BITS;
	ca->array[to]++;

	 
	count = COUNT(ca->array[to]);
	if (count >= action_threshold) {
		u64 pfn = ca->array[to] >> PAGE_SHIFT;

		if (!pfn_valid(pfn)) {
			pr_warn("CEC: Invalid pfn: 0x%llx\n", pfn);
		} else {
			 
			pr_err("Soft-offlining pfn: 0x%llx\n", pfn);
			memory_failure_queue(pfn, MF_SOFT_OFFLINE);
			ca->pfns_poisoned++;
		}

		del_elem(ca, to);

		 
		ret = 1;

		goto unlock;
	}

	ca->decay_count++;

	if (ca->decay_count >= CLEAN_ELEMS)
		do_spring_cleaning(ca);

	WARN_ON_ONCE(sanity_check(ca));

unlock:
	mutex_unlock(&ce_mutex);

	return ret;
}

static int u64_get(void *data, u64 *val)
{
	*val = *(u64 *)data;

	return 0;
}

static int pfn_set(void *data, u64 val)
{
	*(u64 *)data = val;

	cec_add_elem(val);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(pfn_ops, u64_get, pfn_set, "0x%llx\n");

static int decay_interval_set(void *data, u64 val)
{
	if (val < CEC_DECAY_MIN_INTERVAL)
		return -EINVAL;

	if (val > CEC_DECAY_MAX_INTERVAL)
		return -EINVAL;

	*(u64 *)data   = val;
	decay_interval = val;

	cec_mod_work(decay_interval);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(decay_interval_ops, u64_get, decay_interval_set, "%lld\n");

static int action_threshold_set(void *data, u64 val)
{
	*(u64 *)data = val;

	if (val > COUNT_MASK)
		val = COUNT_MASK;

	action_threshold = val;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(action_threshold_ops, u64_get, action_threshold_set, "%lld\n");

static const char * const bins[] = { "00", "01", "10", "11" };

static int array_show(struct seq_file *m, void *v)
{
	struct ce_array *ca = &ce_arr;
	int i;

	mutex_lock(&ce_mutex);

	seq_printf(m, "{ n: %d\n", ca->n);
	for (i = 0; i < ca->n; i++) {
		u64 this = PFN(ca->array[i]);

		seq_printf(m, " %3d: [%016llx|%s|%03llx]\n",
			   i, this, bins[DECAY(ca->array[i])], COUNT(ca->array[i]));
	}

	seq_printf(m, "}\n");

	seq_printf(m, "Stats:\nCEs: %llu\nofflined pages: %llu\n",
		   ca->ces_entered, ca->pfns_poisoned);

	seq_printf(m, "Flags: 0x%x\n", ca->flags);

	seq_printf(m, "Decay interval: %lld seconds\n", decay_interval);
	seq_printf(m, "Decays: %lld\n", ca->decays_done);

	seq_printf(m, "Action threshold: %lld\n", action_threshold);

	mutex_unlock(&ce_mutex);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(array);

static int __init create_debugfs_nodes(void)
{
	struct dentry *d, *pfn, *decay, *count, *array;

	d = debugfs_create_dir("cec", ras_debugfs_dir);
	if (!d) {
		pr_warn("Error creating cec debugfs node!\n");
		return -1;
	}

	decay = debugfs_create_file("decay_interval", S_IRUSR | S_IWUSR, d,
				    &decay_interval, &decay_interval_ops);
	if (!decay) {
		pr_warn("Error creating decay_interval debugfs node!\n");
		goto err;
	}

	count = debugfs_create_file("action_threshold", S_IRUSR | S_IWUSR, d,
				    &action_threshold, &action_threshold_ops);
	if (!count) {
		pr_warn("Error creating action_threshold debugfs node!\n");
		goto err;
	}

	if (!IS_ENABLED(CONFIG_RAS_CEC_DEBUG))
		return 0;

	pfn = debugfs_create_file("pfn", S_IRUSR | S_IWUSR, d, &dfs_pfn, &pfn_ops);
	if (!pfn) {
		pr_warn("Error creating pfn debugfs node!\n");
		goto err;
	}

	array = debugfs_create_file("array", S_IRUSR, d, NULL, &array_fops);
	if (!array) {
		pr_warn("Error creating array debugfs node!\n");
		goto err;
	}

	return 0;

err:
	debugfs_remove_recursive(d);

	return 1;
}

static int cec_notifier(struct notifier_block *nb, unsigned long val,
			void *data)
{
	struct mce *m = (struct mce *)data;

	if (!m)
		return NOTIFY_DONE;

	 
	if (mce_is_memory_error(m) &&
	    mce_is_correctable(m)  &&
	    mce_usable_address(m)) {
		if (!cec_add_elem(m->addr >> PAGE_SHIFT)) {
			m->kflags |= MCE_HANDLED_CEC;
			return NOTIFY_OK;
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block cec_nb = {
	.notifier_call	= cec_notifier,
	.priority	= MCE_PRIO_CEC,
};

static int __init cec_init(void)
{
	if (ce_arr.disabled)
		return -ENODEV;

	 
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL)
		action_threshold = 2;

	ce_arr.array = (void *)get_zeroed_page(GFP_KERNEL);
	if (!ce_arr.array) {
		pr_err("Error allocating CE array page!\n");
		return -ENOMEM;
	}

	if (create_debugfs_nodes()) {
		free_page((unsigned long)ce_arr.array);
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&cec_work, cec_work_fn);
	schedule_delayed_work(&cec_work, CEC_DECAY_DEFAULT_INTERVAL);

	mce_register_decode_chain(&cec_nb);

	pr_info("Correctable Errors collector initialized.\n");
	return 0;
}
late_initcall(cec_init);

int __init parse_cec_param(char *str)
{
	if (!str)
		return 0;

	if (*str == '=')
		str++;

	if (!strcmp(str, "cec_disable"))
		ce_arr.disabled = 1;
	else
		return 0;

	return 1;
}
