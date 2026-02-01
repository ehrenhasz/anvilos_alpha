
 

#include <linux/module.h>
#include <uapi/linux/module.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/rculist.h>
#include <linux/math.h>

#include "internal.h"

 

 
static LIST_HEAD(dup_failed_modules);

 

atomic_long_t total_mod_size;
atomic_long_t total_text_size;
atomic_long_t invalid_kread_bytes;
atomic_long_t invalid_decompress_bytes;
static atomic_long_t invalid_becoming_bytes;
static atomic_long_t invalid_mod_bytes;
atomic_t modcount;
atomic_t failed_kreads;
atomic_t failed_decompress;
static atomic_t failed_becoming;
static atomic_t failed_load_modules;

static const char *mod_fail_to_str(struct mod_fail_load *mod_fail)
{
	if (test_bit(FAIL_DUP_MOD_BECOMING, &mod_fail->dup_fail_mask) &&
	    test_bit(FAIL_DUP_MOD_LOAD, &mod_fail->dup_fail_mask))
		return "Becoming & Load";
	if (test_bit(FAIL_DUP_MOD_BECOMING, &mod_fail->dup_fail_mask))
		return "Becoming";
	if (test_bit(FAIL_DUP_MOD_LOAD, &mod_fail->dup_fail_mask))
		return "Load";
	return "Bug-on-stats";
}

void mod_stat_bump_invalid(struct load_info *info, int flags)
{
	atomic_long_add(info->len * 2, &invalid_mod_bytes);
	atomic_inc(&failed_load_modules);
#if defined(CONFIG_MODULE_DECOMPRESS)
	if (flags & MODULE_INIT_COMPRESSED_FILE)
		atomic_long_add(info->compressed_len, &invalid_mod_bytes);
#endif
}

void mod_stat_bump_becoming(struct load_info *info, int flags)
{
	atomic_inc(&failed_becoming);
	atomic_long_add(info->len, &invalid_becoming_bytes);
#if defined(CONFIG_MODULE_DECOMPRESS)
	if (flags & MODULE_INIT_COMPRESSED_FILE)
		atomic_long_add(info->compressed_len, &invalid_becoming_bytes);
#endif
}

int try_add_failed_module(const char *name, enum fail_dup_mod_reason reason)
{
	struct mod_fail_load *mod_fail;

	list_for_each_entry_rcu(mod_fail, &dup_failed_modules, list,
				lockdep_is_held(&module_mutex)) {
		if (!strcmp(mod_fail->name, name)) {
			atomic_long_inc(&mod_fail->count);
			__set_bit(reason, &mod_fail->dup_fail_mask);
			goto out;
		}
	}

	mod_fail = kzalloc(sizeof(*mod_fail), GFP_KERNEL);
	if (!mod_fail)
		return -ENOMEM;
	memcpy(mod_fail->name, name, strlen(name));
	__set_bit(reason, &mod_fail->dup_fail_mask);
	atomic_long_inc(&mod_fail->count);
	list_add_rcu(&mod_fail->list, &dup_failed_modules);
out:
	return 0;
}

 
#define MAX_PREAMBLE 1024
#define MAX_FAILED_MOD_PRINT 112
#define MAX_BYTES_PER_MOD 64
static ssize_t read_file_mod_stats(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct mod_fail_load *mod_fail;
	unsigned int len, size, count_failed = 0;
	char *buf;
	int ret;
	u32 live_mod_count, fkreads, fdecompress, fbecoming, floads;
	unsigned long total_size, text_size, ikread_bytes, ibecoming_bytes,
		idecompress_bytes, imod_bytes, total_virtual_lost;

	live_mod_count = atomic_read(&modcount);
	fkreads = atomic_read(&failed_kreads);
	fdecompress = atomic_read(&failed_decompress);
	fbecoming = atomic_read(&failed_becoming);
	floads = atomic_read(&failed_load_modules);

	total_size = atomic_long_read(&total_mod_size);
	text_size = atomic_long_read(&total_text_size);
	ikread_bytes = atomic_long_read(&invalid_kread_bytes);
	idecompress_bytes = atomic_long_read(&invalid_decompress_bytes);
	ibecoming_bytes = atomic_long_read(&invalid_becoming_bytes);
	imod_bytes = atomic_long_read(&invalid_mod_bytes);

	total_virtual_lost = ikread_bytes + idecompress_bytes + ibecoming_bytes + imod_bytes;

	size = MAX_PREAMBLE + min((unsigned int)(floads + fbecoming),
				  (unsigned int)MAX_FAILED_MOD_PRINT) * MAX_BYTES_PER_MOD;
	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	 
	len = scnprintf(buf, size, "%25s\t%u\n", "Mods ever loaded", live_mod_count);

	len += scnprintf(buf + len, size - len, "%25s\t%u\n", "Mods failed on kread", fkreads);

	len += scnprintf(buf + len, size - len, "%25s\t%u\n", "Mods failed on decompress",
			 fdecompress);
	len += scnprintf(buf + len, size - len, "%25s\t%u\n", "Mods failed on becoming", fbecoming);

	len += scnprintf(buf + len, size - len, "%25s\t%u\n", "Mods failed on load", floads);

	len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Total module size", total_size);
	len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Total mod text size", text_size);

	len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Failed kread bytes", ikread_bytes);

	len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Failed decompress bytes",
			 idecompress_bytes);

	len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Failed becoming bytes", ibecoming_bytes);

	len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Failed kmod bytes", imod_bytes);

	len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Virtual mem wasted bytes", total_virtual_lost);

	if (live_mod_count && total_size) {
		len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Average mod size",
				 DIV_ROUND_UP(total_size, live_mod_count));
	}

	if (live_mod_count && text_size) {
		len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Average mod text size",
				 DIV_ROUND_UP(text_size, live_mod_count));
	}

	 

	WARN_ON_ONCE(ikread_bytes && !fkreads);
	if (fkreads && ikread_bytes) {
		len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Avg fail kread bytes",
				 DIV_ROUND_UP(ikread_bytes, fkreads));
	}

	WARN_ON_ONCE(ibecoming_bytes && !fbecoming);
	if (fbecoming && ibecoming_bytes) {
		len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Avg fail becoming bytes",
				 DIV_ROUND_UP(ibecoming_bytes, fbecoming));
	}

	WARN_ON_ONCE(idecompress_bytes && !fdecompress);
	if (fdecompress && idecompress_bytes) {
		len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Avg fail decomp bytes",
				 DIV_ROUND_UP(idecompress_bytes, fdecompress));
	}

	WARN_ON_ONCE(imod_bytes && !floads);
	if (floads && imod_bytes) {
		len += scnprintf(buf + len, size - len, "%25s\t%lu\n", "Average fail load bytes",
				 DIV_ROUND_UP(imod_bytes, floads));
	}

	 

	 
	WARN_ON_ONCE(len >= MAX_PREAMBLE);

	if (list_empty(&dup_failed_modules))
		goto out;

	len += scnprintf(buf + len, size - len, "Duplicate failed modules:\n");
	len += scnprintf(buf + len, size - len, "%25s\t%15s\t%25s\n",
			 "Module-name", "How-many-times", "Reason");
	mutex_lock(&module_mutex);


	list_for_each_entry_rcu(mod_fail, &dup_failed_modules, list) {
		if (WARN_ON_ONCE(++count_failed >= MAX_FAILED_MOD_PRINT))
			goto out_unlock;
		len += scnprintf(buf + len, size - len, "%25s\t%15lu\t%25s\n", mod_fail->name,
				 atomic_long_read(&mod_fail->count), mod_fail_to_str(mod_fail));
	}
out_unlock:
	mutex_unlock(&module_mutex);
out:
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}
#undef MAX_PREAMBLE
#undef MAX_FAILED_MOD_PRINT
#undef MAX_BYTES_PER_MOD

static const struct file_operations fops_mod_stats = {
	.read = read_file_mod_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

#define mod_debug_add_ulong(name) debugfs_create_ulong(#name, 0400, mod_debugfs_root, (unsigned long *) &name.counter)
#define mod_debug_add_atomic(name) debugfs_create_atomic_t(#name, 0400, mod_debugfs_root, &name)
static int __init module_stats_init(void)
{
	mod_debug_add_ulong(total_mod_size);
	mod_debug_add_ulong(total_text_size);
	mod_debug_add_ulong(invalid_kread_bytes);
	mod_debug_add_ulong(invalid_decompress_bytes);
	mod_debug_add_ulong(invalid_becoming_bytes);
	mod_debug_add_ulong(invalid_mod_bytes);

	mod_debug_add_atomic(modcount);
	mod_debug_add_atomic(failed_kreads);
	mod_debug_add_atomic(failed_decompress);
	mod_debug_add_atomic(failed_becoming);
	mod_debug_add_atomic(failed_load_modules);

	debugfs_create_file("stats", 0400, mod_debugfs_root, mod_debugfs_root, &fops_mod_stats);

	return 0;
}
#undef mod_debug_add_ulong
#undef mod_debug_add_atomic
module_init(module_stats_init);
