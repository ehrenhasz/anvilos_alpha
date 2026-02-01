
 

#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/dm-dirty-log.h>
#include <linux/device-mapper.h>
#include <linux/dm-log-userspace.h>
#include <linux/module.h>
#include <linux/workqueue.h>

#include "dm-log-userspace-transfer.h"

#define DM_LOG_USERSPACE_VSN "1.3.0"

#define FLUSH_ENTRY_POOL_SIZE 16

struct dm_dirty_log_flush_entry {
	int type;
	region_t region;
	struct list_head list;
};

 
#define MAX_FLUSH_GROUP_COUNT 32

struct log_c {
	struct dm_target *ti;
	struct dm_dev *log_dev;

	char *usr_argv_str;
	uint32_t usr_argc;

	uint32_t region_size;
	region_t region_count;
	uint64_t luid;
	char uuid[DM_UUID_LEN];

	 
	spinlock_t flush_lock;
	struct list_head mark_list;
	struct list_head clear_list;

	 
	uint64_t in_sync_hint;

	 
	struct workqueue_struct *dmlog_wq;
	struct delayed_work flush_log_work;
	atomic_t sched_flush;

	 
	uint32_t integrated_flush;

	mempool_t flush_entry_pool;
};

static struct kmem_cache *_flush_entry_cache;

static int userspace_do_request(struct log_c *lc, const char *uuid,
				int request_type, char *data, size_t data_size,
				char *rdata, size_t *rdata_size)
{
	int r;

	 
retry:
	r = dm_consult_userspace(uuid, lc->luid, request_type, data,
				 data_size, rdata, rdata_size);

	if (r != -ESRCH)
		return r;

	DMERR(" Userspace log server not found.");
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(2*HZ);
		DMWARN("Attempting to contact userspace log server...");
		r = dm_consult_userspace(uuid, lc->luid, DM_ULOG_CTR,
					 lc->usr_argv_str,
					 strlen(lc->usr_argv_str) + 1,
					 NULL, NULL);
		if (!r)
			break;
	}
	DMINFO("Reconnected to userspace log server... DM_ULOG_CTR complete");
	r = dm_consult_userspace(uuid, lc->luid, DM_ULOG_RESUME, NULL,
				 0, NULL, NULL);
	if (!r)
		goto retry;

	DMERR("Error trying to resume userspace log: %d", r);

	return -ESRCH;
}

static int build_constructor_string(struct dm_target *ti,
				    unsigned int argc, char **argv,
				    char **ctr_str)
{
	int i, str_size;
	char *str = NULL;

	*ctr_str = NULL;

	 
	for (i = 0, str_size = 0; i < argc; i++)
		str_size += strlen(argv[i]) + 1;  

	str_size += 20;  

	str = kzalloc(str_size, GFP_KERNEL);
	if (!str) {
		DMWARN("Unable to allocate memory for constructor string");
		return -ENOMEM;
	}

	str_size = sprintf(str, "%llu", (unsigned long long)ti->len);
	for (i = 0; i < argc; i++)
		str_size += sprintf(str + str_size, " %s", argv[i]);

	*ctr_str = str;
	return str_size;
}

static void do_flush(struct work_struct *work)
{
	int r;
	struct log_c *lc = container_of(work, struct log_c, flush_log_work.work);

	atomic_set(&lc->sched_flush, 0);

	r = userspace_do_request(lc, lc->uuid, DM_ULOG_FLUSH, NULL, 0, NULL, NULL);

	if (r)
		dm_table_event(lc->ti->table);
}

 
static int userspace_ctr(struct dm_dirty_log *log, struct dm_target *ti,
			 unsigned int argc, char **argv)
{
	int r = 0;
	int str_size;
	char *ctr_str = NULL;
	struct log_c *lc = NULL;
	uint64_t rdata;
	size_t rdata_size = sizeof(rdata);
	char *devices_rdata = NULL;
	size_t devices_rdata_size = DM_NAME_LEN;

	if (argc < 3) {
		DMWARN("Too few arguments to userspace dirty log");
		return -EINVAL;
	}

	lc = kzalloc(sizeof(*lc), GFP_KERNEL);
	if (!lc) {
		DMWARN("Unable to allocate userspace log context.");
		return -ENOMEM;
	}

	 
	lc->luid = (unsigned long)lc;

	lc->ti = ti;

	if (strlen(argv[0]) > (DM_UUID_LEN - 1)) {
		DMWARN("UUID argument too long.");
		kfree(lc);
		return -EINVAL;
	}

	lc->usr_argc = argc;

	strncpy(lc->uuid, argv[0], DM_UUID_LEN);
	argc--;
	argv++;
	spin_lock_init(&lc->flush_lock);
	INIT_LIST_HEAD(&lc->mark_list);
	INIT_LIST_HEAD(&lc->clear_list);

	if (!strcasecmp(argv[0], "integrated_flush")) {
		lc->integrated_flush = 1;
		argc--;
		argv++;
	}

	str_size = build_constructor_string(ti, argc, argv, &ctr_str);
	if (str_size < 0) {
		kfree(lc);
		return str_size;
	}

	devices_rdata = kzalloc(devices_rdata_size, GFP_KERNEL);
	if (!devices_rdata) {
		DMERR("Failed to allocate memory for device information");
		r = -ENOMEM;
		goto out;
	}

	r = mempool_init_slab_pool(&lc->flush_entry_pool, FLUSH_ENTRY_POOL_SIZE,
				   _flush_entry_cache);
	if (r) {
		DMERR("Failed to create flush_entry_pool");
		goto out;
	}

	 
	r = dm_consult_userspace(lc->uuid, lc->luid, DM_ULOG_CTR,
				 ctr_str, str_size,
				 devices_rdata, &devices_rdata_size);

	if (r < 0) {
		if (r == -ESRCH)
			DMERR("Userspace log server not found");
		else
			DMERR("Userspace log server failed to create log");
		goto out;
	}

	 
	rdata_size = sizeof(rdata);
	r = dm_consult_userspace(lc->uuid, lc->luid, DM_ULOG_GET_REGION_SIZE,
				 NULL, 0, (char *)&rdata, &rdata_size);

	if (r) {
		DMERR("Failed to get region size of dirty log");
		goto out;
	}

	lc->region_size = (uint32_t)rdata;
	lc->region_count = dm_sector_div_up(ti->len, lc->region_size);

	if (devices_rdata_size) {
		if (devices_rdata[devices_rdata_size - 1] != '\0') {
			DMERR("DM_ULOG_CTR device return string not properly terminated");
			r = -EINVAL;
			goto out;
		}
		r = dm_get_device(ti, devices_rdata,
				  dm_table_get_mode(ti->table), &lc->log_dev);
		if (r)
			DMERR("Failed to register %s with device-mapper",
			      devices_rdata);
	}

	if (lc->integrated_flush) {
		lc->dmlog_wq = alloc_workqueue("dmlogd", WQ_MEM_RECLAIM, 0);
		if (!lc->dmlog_wq) {
			DMERR("couldn't start dmlogd");
			r = -ENOMEM;
			goto out;
		}

		INIT_DELAYED_WORK(&lc->flush_log_work, do_flush);
		atomic_set(&lc->sched_flush, 0);
	}

out:
	kfree(devices_rdata);
	if (r) {
		mempool_exit(&lc->flush_entry_pool);
		kfree(lc);
		kfree(ctr_str);
	} else {
		lc->usr_argv_str = ctr_str;
		log->context = lc;
	}

	return r;
}

static void userspace_dtr(struct dm_dirty_log *log)
{
	struct log_c *lc = log->context;

	if (lc->integrated_flush) {
		 
		if (atomic_read(&lc->sched_flush))
			flush_delayed_work(&lc->flush_log_work);

		destroy_workqueue(lc->dmlog_wq);
	}

	(void) dm_consult_userspace(lc->uuid, lc->luid, DM_ULOG_DTR,
				    NULL, 0, NULL, NULL);

	if (lc->log_dev)
		dm_put_device(lc->ti, lc->log_dev);

	mempool_exit(&lc->flush_entry_pool);

	kfree(lc->usr_argv_str);
	kfree(lc);
}

static int userspace_presuspend(struct dm_dirty_log *log)
{
	int r;
	struct log_c *lc = log->context;

	r = dm_consult_userspace(lc->uuid, lc->luid, DM_ULOG_PRESUSPEND,
				 NULL, 0, NULL, NULL);

	return r;
}

static int userspace_postsuspend(struct dm_dirty_log *log)
{
	int r;
	struct log_c *lc = log->context;

	 
	if (lc->integrated_flush && atomic_read(&lc->sched_flush))
		flush_delayed_work(&lc->flush_log_work);

	r = dm_consult_userspace(lc->uuid, lc->luid, DM_ULOG_POSTSUSPEND,
				 NULL, 0, NULL, NULL);

	return r;
}

static int userspace_resume(struct dm_dirty_log *log)
{
	int r;
	struct log_c *lc = log->context;

	lc->in_sync_hint = 0;
	r = dm_consult_userspace(lc->uuid, lc->luid, DM_ULOG_RESUME,
				 NULL, 0, NULL, NULL);

	return r;
}

static uint32_t userspace_get_region_size(struct dm_dirty_log *log)
{
	struct log_c *lc = log->context;

	return lc->region_size;
}

 
static int userspace_is_clean(struct dm_dirty_log *log, region_t region)
{
	int r;
	uint64_t region64 = (uint64_t)region;
	int64_t is_clean;
	size_t rdata_size;
	struct log_c *lc = log->context;

	rdata_size = sizeof(is_clean);
	r = userspace_do_request(lc, lc->uuid, DM_ULOG_IS_CLEAN,
				 (char *)&region64, sizeof(region64),
				 (char *)&is_clean, &rdata_size);

	return (r) ? 0 : (int)is_clean;
}

 
static int userspace_in_sync(struct dm_dirty_log *log, region_t region,
			     int can_block)
{
	int r;
	uint64_t region64 = region;
	int64_t in_sync;
	size_t rdata_size;
	struct log_c *lc = log->context;

	 
	if (!can_block)
		return -EWOULDBLOCK;

	rdata_size = sizeof(in_sync);
	r = userspace_do_request(lc, lc->uuid, DM_ULOG_IN_SYNC,
				 (char *)&region64, sizeof(region64),
				 (char *)&in_sync, &rdata_size);
	return (r) ? 0 : (int)in_sync;
}

static int flush_one_by_one(struct log_c *lc, struct list_head *flush_list)
{
	int r = 0;
	struct dm_dirty_log_flush_entry *fe;

	list_for_each_entry(fe, flush_list, list) {
		r = userspace_do_request(lc, lc->uuid, fe->type,
					 (char *)&fe->region,
					 sizeof(fe->region),
					 NULL, NULL);
		if (r)
			break;
	}

	return r;
}

static int flush_by_group(struct log_c *lc, struct list_head *flush_list,
			  int flush_with_payload)
{
	int r = 0;
	int count;
	uint32_t type = 0;
	struct dm_dirty_log_flush_entry *fe, *tmp_fe;
	LIST_HEAD(tmp_list);
	uint64_t group[MAX_FLUSH_GROUP_COUNT];

	 
	while (!list_empty(flush_list)) {
		count = 0;

		list_for_each_entry_safe(fe, tmp_fe, flush_list, list) {
			group[count] = fe->region;
			count++;

			list_move(&fe->list, &tmp_list);

			type = fe->type;
			if (count >= MAX_FLUSH_GROUP_COUNT)
				break;
		}

		if (flush_with_payload) {
			r = userspace_do_request(lc, lc->uuid, DM_ULOG_FLUSH,
						 (char *)(group),
						 count * sizeof(uint64_t),
						 NULL, NULL);
			 
			if (r)
				break;
		} else {
			r = userspace_do_request(lc, lc->uuid, type,
						 (char *)(group),
						 count * sizeof(uint64_t),
						 NULL, NULL);
			if (r) {
				 
				list_splice_init(&tmp_list, flush_list);
				r = flush_one_by_one(lc, flush_list);
				break;
			}
		}
	}

	 
	list_splice_init(&tmp_list, flush_list);

	return r;
}

 
static int userspace_flush(struct dm_dirty_log *log)
{
	int r = 0;
	unsigned long flags;
	struct log_c *lc = log->context;
	LIST_HEAD(mark_list);
	LIST_HEAD(clear_list);
	int mark_list_is_empty;
	int clear_list_is_empty;
	struct dm_dirty_log_flush_entry *fe, *tmp_fe;
	mempool_t *flush_entry_pool = &lc->flush_entry_pool;

	spin_lock_irqsave(&lc->flush_lock, flags);
	list_splice_init(&lc->mark_list, &mark_list);
	list_splice_init(&lc->clear_list, &clear_list);
	spin_unlock_irqrestore(&lc->flush_lock, flags);

	mark_list_is_empty = list_empty(&mark_list);
	clear_list_is_empty = list_empty(&clear_list);

	if (mark_list_is_empty && clear_list_is_empty)
		return 0;

	r = flush_by_group(lc, &clear_list, 0);
	if (r)
		goto out;

	if (!lc->integrated_flush) {
		r = flush_by_group(lc, &mark_list, 0);
		if (r)
			goto out;
		r = userspace_do_request(lc, lc->uuid, DM_ULOG_FLUSH,
					 NULL, 0, NULL, NULL);
		goto out;
	}

	 
	r = flush_by_group(lc, &mark_list, 1);
	if (r)
		goto out;

	if (mark_list_is_empty && !atomic_read(&lc->sched_flush)) {
		 
		queue_delayed_work(lc->dmlog_wq, &lc->flush_log_work, 3 * HZ);
		atomic_set(&lc->sched_flush, 1);
	} else {
		 
		cancel_delayed_work(&lc->flush_log_work);
		atomic_set(&lc->sched_flush, 0);
	}

out:
	 
	list_for_each_entry_safe(fe, tmp_fe, &mark_list, list) {
		list_del(&fe->list);
		mempool_free(fe, flush_entry_pool);
	}
	list_for_each_entry_safe(fe, tmp_fe, &clear_list, list) {
		list_del(&fe->list);
		mempool_free(fe, flush_entry_pool);
	}

	if (r)
		dm_table_event(lc->ti->table);

	return r;
}

 
static void userspace_mark_region(struct dm_dirty_log *log, region_t region)
{
	unsigned long flags;
	struct log_c *lc = log->context;
	struct dm_dirty_log_flush_entry *fe;

	 
	fe = mempool_alloc(&lc->flush_entry_pool, GFP_NOIO);
	BUG_ON(!fe);

	spin_lock_irqsave(&lc->flush_lock, flags);
	fe->type = DM_ULOG_MARK_REGION;
	fe->region = region;
	list_add(&fe->list, &lc->mark_list);
	spin_unlock_irqrestore(&lc->flush_lock, flags);
}

 
static void userspace_clear_region(struct dm_dirty_log *log, region_t region)
{
	unsigned long flags;
	struct log_c *lc = log->context;
	struct dm_dirty_log_flush_entry *fe;

	 
	fe = mempool_alloc(&lc->flush_entry_pool, GFP_ATOMIC);
	if (!fe) {
		DMERR("Failed to allocate memory to clear region.");
		return;
	}

	spin_lock_irqsave(&lc->flush_lock, flags);
	fe->type = DM_ULOG_CLEAR_REGION;
	fe->region = region;
	list_add(&fe->list, &lc->clear_list);
	spin_unlock_irqrestore(&lc->flush_lock, flags);
}

 
static int userspace_get_resync_work(struct dm_dirty_log *log, region_t *region)
{
	int r;
	size_t rdata_size;
	struct log_c *lc = log->context;
	struct {
		int64_t i;  
		region_t r;
	} pkg;

	if (lc->in_sync_hint >= lc->region_count)
		return 0;

	rdata_size = sizeof(pkg);
	r = userspace_do_request(lc, lc->uuid, DM_ULOG_GET_RESYNC_WORK,
				 NULL, 0, (char *)&pkg, &rdata_size);

	*region = pkg.r;
	return (r) ? r : (int)pkg.i;
}

 
static void userspace_set_region_sync(struct dm_dirty_log *log,
				      region_t region, int in_sync)
{
	struct log_c *lc = log->context;
	struct {
		region_t r;
		int64_t i;
	} pkg;

	pkg.r = region;
	pkg.i = (int64_t)in_sync;

	(void) userspace_do_request(lc, lc->uuid, DM_ULOG_SET_REGION_SYNC,
				    (char *)&pkg, sizeof(pkg), NULL, NULL);

	 
}

 
static region_t userspace_get_sync_count(struct dm_dirty_log *log)
{
	int r;
	size_t rdata_size;
	uint64_t sync_count;
	struct log_c *lc = log->context;

	rdata_size = sizeof(sync_count);
	r = userspace_do_request(lc, lc->uuid, DM_ULOG_GET_SYNC_COUNT,
				 NULL, 0, (char *)&sync_count, &rdata_size);

	if (r)
		return 0;

	if (sync_count >= lc->region_count)
		lc->in_sync_hint = lc->region_count;

	return (region_t)sync_count;
}

 
static int userspace_status(struct dm_dirty_log *log, status_type_t status_type,
			    char *result, unsigned int maxlen)
{
	int r = 0;
	char *table_args;
	size_t sz = (size_t)maxlen;
	struct log_c *lc = log->context;

	switch (status_type) {
	case STATUSTYPE_INFO:
		r = userspace_do_request(lc, lc->uuid, DM_ULOG_STATUS_INFO,
					 NULL, 0, result, &sz);

		if (r) {
			sz = 0;
			DMEMIT("%s 1 COM_FAILURE", log->type->name);
		}
		break;
	case STATUSTYPE_TABLE:
		sz = 0;
		table_args = strchr(lc->usr_argv_str, ' ');
		BUG_ON(!table_args);  
		table_args++;

		DMEMIT("%s %u %s ", log->type->name, lc->usr_argc, lc->uuid);
		if (lc->integrated_flush)
			DMEMIT("integrated_flush ");
		DMEMIT("%s ", table_args);
		break;
	case STATUSTYPE_IMA:
		*result = '\0';
		break;
	}
	return (r) ? 0 : (int)sz;
}

 
static int userspace_is_remote_recovering(struct dm_dirty_log *log,
					  region_t region)
{
	int r;
	uint64_t region64 = region;
	struct log_c *lc = log->context;
	static unsigned long limit;
	struct {
		int64_t is_recovering;
		uint64_t in_sync_hint;
	} pkg;
	size_t rdata_size = sizeof(pkg);

	 
	if (region < lc->in_sync_hint)
		return 0;
	else if (time_after(limit, jiffies))
		return 1;

	limit = jiffies + (HZ / 4);
	r = userspace_do_request(lc, lc->uuid, DM_ULOG_IS_REMOTE_RECOVERING,
				 (char *)&region64, sizeof(region64),
				 (char *)&pkg, &rdata_size);
	if (r)
		return 1;

	lc->in_sync_hint = pkg.in_sync_hint;

	return (int)pkg.is_recovering;
}

static struct dm_dirty_log_type _userspace_type = {
	.name = "userspace",
	.module = THIS_MODULE,
	.ctr = userspace_ctr,
	.dtr = userspace_dtr,
	.presuspend = userspace_presuspend,
	.postsuspend = userspace_postsuspend,
	.resume = userspace_resume,
	.get_region_size = userspace_get_region_size,
	.is_clean = userspace_is_clean,
	.in_sync = userspace_in_sync,
	.flush = userspace_flush,
	.mark_region = userspace_mark_region,
	.clear_region = userspace_clear_region,
	.get_resync_work = userspace_get_resync_work,
	.set_region_sync = userspace_set_region_sync,
	.get_sync_count = userspace_get_sync_count,
	.status = userspace_status,
	.is_remote_recovering = userspace_is_remote_recovering,
};

static int __init userspace_dirty_log_init(void)
{
	int r = 0;

	_flush_entry_cache = KMEM_CACHE(dm_dirty_log_flush_entry, 0);
	if (!_flush_entry_cache) {
		DMWARN("Unable to create flush_entry_cache: No memory.");
		return -ENOMEM;
	}

	r = dm_ulog_tfr_init();
	if (r) {
		DMWARN("Unable to initialize userspace log communications");
		kmem_cache_destroy(_flush_entry_cache);
		return r;
	}

	r = dm_dirty_log_type_register(&_userspace_type);
	if (r) {
		DMWARN("Couldn't register userspace dirty log type");
		dm_ulog_tfr_exit();
		kmem_cache_destroy(_flush_entry_cache);
		return r;
	}

	DMINFO("version " DM_LOG_USERSPACE_VSN " loaded");
	return 0;
}

static void __exit userspace_dirty_log_exit(void)
{
	dm_dirty_log_type_unregister(&_userspace_type);
	dm_ulog_tfr_exit();
	kmem_cache_destroy(_flush_entry_cache);

	DMINFO("version " DM_LOG_USERSPACE_VSN " unloaded");
}

module_init(userspace_dirty_log_init);
module_exit(userspace_dirty_log_exit);

MODULE_DESCRIPTION(DM_NAME " userspace dirty log link");
MODULE_AUTHOR("Jonathan Brassow <dm-devel@redhat.com>");
MODULE_LICENSE("GPL");
