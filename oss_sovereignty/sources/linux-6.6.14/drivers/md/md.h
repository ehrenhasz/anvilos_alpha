


#ifndef _MD_MD_H
#define _MD_MD_H

#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/badblocks.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "md-cluster.h"

#define MaxSector (~(sector_t)0)


#define	MD_FAILFAST	(REQ_FAILFAST_DEV | REQ_FAILFAST_TRANSPORT)


struct serial_in_rdev {
	struct rb_root_cached serial_rb;
	spinlock_t serial_lock;
	wait_queue_head_t serial_io_wait;
};


struct md_rdev {
	struct list_head same_set;	

	sector_t sectors;		
	struct mddev *mddev;		
	int last_events;		

	
	struct block_device *meta_bdev;
	struct block_device *bdev;	

	struct page	*sb_page, *bb_page;
	int		sb_loaded;
	__u64		sb_events;
	sector_t	data_offset;	
	sector_t	new_data_offset;
	sector_t	sb_start;	
	int		sb_size;	
	int		preferred_minor;	

	struct kobject	kobj;

	

	unsigned long	flags;	
	wait_queue_head_t blocked_wait;

	int desc_nr;			
	int raid_disk;			
	int new_raid_disk;		
	int saved_raid_disk;		
	union {
		sector_t recovery_offset;
		sector_t journal_tail;	
	};

	atomic_t	nr_pending;	
	atomic_t	read_errors;	
	time64_t	last_read_error;	
	atomic_t	corrected_errors; 

	struct serial_in_rdev *serial;  

	struct kernfs_node *sysfs_state; 
	
	struct kernfs_node *sysfs_unack_badblocks;
	
	struct kernfs_node *sysfs_badblocks;
	struct badblocks badblocks;

	struct {
		short offset;	
		unsigned int size;	
		sector_t sector;	
	} ppl;
};
enum flag_bits {
	Faulty,			
	In_sync,		
	Bitmap_sync,		
	WriteMostly,		
	AutoDetected,		
	Blocked,		
	WriteErrorSeen,		
	FaultRecorded,		
	BlockedBadBlocks,	
	WantReplacement,	
	Replacement,		
	Candidate,		
	Journal,		
	ClusterRemove,
	RemoveSynchronized,	
	ExternalBbl,            
	FailFast,		
	LastDev,		
	CollisionCheck,		
	Holder,			
};

static inline int is_badblock(struct md_rdev *rdev, sector_t s, int sectors,
			      sector_t *first_bad, int *bad_sectors)
{
	if (unlikely(rdev->badblocks.count)) {
		int rv = badblocks_check(&rdev->badblocks, rdev->data_offset + s,
					sectors,
					first_bad, bad_sectors);
		if (rv)
			*first_bad -= rdev->data_offset;
		return rv;
	}
	return 0;
}
extern int rdev_set_badblocks(struct md_rdev *rdev, sector_t s, int sectors,
			      int is_new);
extern int rdev_clear_badblocks(struct md_rdev *rdev, sector_t s, int sectors,
				int is_new);
struct md_cluster_info;


enum mddev_flags {
	MD_ARRAY_FIRST_USE,
	MD_CLOSING,
	MD_JOURNAL_CLEAN,
	MD_HAS_JOURNAL,
	MD_CLUSTER_RESYNC_LOCKED,
	MD_FAILFAST_SUPPORTED,
	MD_HAS_PPL,
	MD_HAS_MULTIPLE_PPLS,
	MD_ALLOW_SB_UPDATE,
	MD_UPDATING_SB,
	MD_NOT_READY,
	MD_BROKEN,
	MD_DELETED,
};

enum mddev_sb_flags {
	MD_SB_CHANGE_DEVS,		
	MD_SB_CHANGE_CLEAN,	
	MD_SB_CHANGE_PENDING,	
	MD_SB_NEED_REWRITE,	
};

#define NR_SERIAL_INFOS		8

struct serial_info {
	struct rb_node node;
	sector_t start;		
	sector_t last;		
	sector_t _subtree_last; 
};


enum {
	
	MD_RESYNC_NONE = 0,
	
	MD_RESYNC_YIELDED = 1,
	
	MD_RESYNC_DELAYED = 2,
	
	MD_RESYNC_ACTIVE = 3,
};

struct mddev {
	void				*private;
	struct md_personality		*pers;
	dev_t				unit;
	int				md_minor;
	struct list_head		disks;
	unsigned long			flags;
	unsigned long			sb_flags;

	int				suspended;
	struct percpu_ref		active_io;
	int				ro;
	int				sysfs_active; 
	struct gendisk			*gendisk;

	struct kobject			kobj;
	int				hold_active;
#define	UNTIL_IOCTL	1
#define	UNTIL_STOP	2

	
	int				major_version,
					minor_version,
					patch_version;
	int				persistent;
	int				external;	
	char				metadata_type[17]; 
	int				chunk_sectors;
	time64_t			ctime, utime;
	int				level, layout;
	char				clevel[16];
	int				raid_disks;
	int				max_disks;
	sector_t			dev_sectors;	
	sector_t			array_sectors; 
	int				external_size; 
	__u64				events;
	
	int				can_decrease_events;

	char				uuid[16];

	
	sector_t			reshape_position;
	int				delta_disks, new_level, new_layout;
	int				new_chunk_sectors;
	int				reshape_backwards;

	struct md_thread __rcu		*thread;	
	struct md_thread __rcu		*sync_thread;	

	
	char				*last_sync_action;
	sector_t			curr_resync;	
	
	sector_t			curr_resync_completed;
	unsigned long			resync_mark;	
	sector_t			resync_mark_cnt;
	sector_t			curr_mark_cnt; 

	sector_t			resync_max_sectors; 

	atomic64_t			resync_mismatches; 

	
	sector_t			suspend_lo;
	sector_t			suspend_hi;
	
	int				sync_speed_min;
	int				sync_speed_max;

	
	int				parallel_resync;

	int				ok_start_degraded;

	unsigned long			recovery;
	
	int				recovery_disabled;

	int				in_sync;	
	
	struct mutex			open_mutex;
	struct mutex			reconfig_mutex;
	atomic_t			active;		
	atomic_t			openers;	

	int				changed;	
	int				degraded;	

	atomic_t			recovery_active; 
	wait_queue_head_t		recovery_wait;
	sector_t			recovery_cp;
	sector_t			resync_min;	
	sector_t			resync_max;	

	struct kernfs_node		*sysfs_state;	
	struct kernfs_node		*sysfs_action;  
	struct kernfs_node		*sysfs_completed;	
	struct kernfs_node		*sysfs_degraded;	
	struct kernfs_node		*sysfs_level;		

	struct work_struct del_work;	

	
	spinlock_t			lock;
	wait_queue_head_t		sb_wait;	
	atomic_t			pending_writes;	

	unsigned int			safemode;	
	unsigned int			safemode_delay;
	struct timer_list		safemode_timer;
	struct percpu_ref		writes_pending;
	int				sync_checkers;	
	struct request_queue		*queue;	

	struct bitmap			*bitmap; 
	struct {
		struct file		*file; 
		loff_t			offset; 
		unsigned long		space; 
		loff_t			default_offset; 
		unsigned long		default_space; 
		struct mutex		mutex;
		unsigned long		chunksize;
		unsigned long		daemon_sleep; 
		unsigned long		max_write_behind; 
		int			external;
		int			nodes; 
		char                    cluster_name[64]; 
	} bitmap_info;

	atomic_t			max_corr_read_errors; 
	struct list_head		all_mddevs;

	const struct attribute_group	*to_remove;

	struct bio_set			bio_set;
	struct bio_set			sync_set; 
	struct bio_set			io_clone_set;

	
	struct bio *flush_bio;
	atomic_t flush_pending;
	ktime_t start_flush, prev_flush_start; 
	struct work_struct flush_work;
	struct work_struct event_work;	
	mempool_t *serial_info_pool;
	void (*sync_super)(struct mddev *mddev, struct md_rdev *rdev);
	struct md_cluster_info		*cluster_info;
	unsigned int			good_device_nr;	
	unsigned int			noio_flag; 

	
	struct list_head		deleting;

	
	struct mutex			sync_mutex;
	
	atomic_t sync_seq;

	bool	has_superblocks:1;
	bool	fail_last_dev:1;
	bool	serialize_policy:1;
};

enum recovery_flags {
	
	MD_RECOVERY_RUNNING,	
	MD_RECOVERY_SYNC,	
	MD_RECOVERY_RECOVER,	
	MD_RECOVERY_INTR,	
	MD_RECOVERY_DONE,	
	MD_RECOVERY_NEEDED,	
	MD_RECOVERY_REQUESTED,	
	MD_RECOVERY_CHECK,	
	MD_RECOVERY_RESHAPE,	
	MD_RECOVERY_FROZEN,	
	MD_RECOVERY_ERROR,	
	MD_RECOVERY_WAIT,	
	MD_RESYNCING_REMOTE,	
};

enum md_ro_state {
	MD_RDWR,
	MD_RDONLY,
	MD_AUTO_READ,
	MD_MAX_STATE
};

static inline bool md_is_rdwr(struct mddev *mddev)
{
	return (mddev->ro == MD_RDWR);
}

static inline bool is_md_suspended(struct mddev *mddev)
{
	return percpu_ref_is_dying(&mddev->active_io);
}

static inline int __must_check mddev_lock(struct mddev *mddev)
{
	return mutex_lock_interruptible(&mddev->reconfig_mutex);
}


static inline void mddev_lock_nointr(struct mddev *mddev)
{
	mutex_lock(&mddev->reconfig_mutex);
}

static inline int mddev_trylock(struct mddev *mddev)
{
	return mutex_trylock(&mddev->reconfig_mutex);
}
extern void mddev_unlock(struct mddev *mddev);

static inline void md_sync_acct(struct block_device *bdev, unsigned long nr_sectors)
{
	atomic_add(nr_sectors, &bdev->bd_disk->sync_io);
}

static inline void md_sync_acct_bio(struct bio *bio, unsigned long nr_sectors)
{
	md_sync_acct(bio->bi_bdev, nr_sectors);
}

struct md_personality
{
	char *name;
	int level;
	struct list_head list;
	struct module *owner;
	bool __must_check (*make_request)(struct mddev *mddev, struct bio *bio);
	
	int (*run)(struct mddev *mddev);
	
	int (*start)(struct mddev *mddev);
	void (*free)(struct mddev *mddev, void *priv);
	void (*status)(struct seq_file *seq, struct mddev *mddev);
	
	void (*error_handler)(struct mddev *mddev, struct md_rdev *rdev);
	int (*hot_add_disk) (struct mddev *mddev, struct md_rdev *rdev);
	int (*hot_remove_disk) (struct mddev *mddev, struct md_rdev *rdev);
	int (*spare_active) (struct mddev *mddev);
	sector_t (*sync_request)(struct mddev *mddev, sector_t sector_nr, int *skipped);
	int (*resize) (struct mddev *mddev, sector_t sectors);
	sector_t (*size) (struct mddev *mddev, sector_t sectors, int raid_disks);
	int (*check_reshape) (struct mddev *mddev);
	int (*start_reshape) (struct mddev *mddev);
	void (*finish_reshape) (struct mddev *mddev);
	void (*update_reshape_pos) (struct mddev *mddev);
	void (*prepare_suspend) (struct mddev *mddev);
	
	void (*quiesce) (struct mddev *mddev, int quiesce);
	
	void *(*takeover) (struct mddev *mddev);
	
	int (*change_consistency_policy)(struct mddev *mddev, const char *buf);
};

struct md_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct mddev *, char *);
	ssize_t (*store)(struct mddev *, const char *, size_t);
};
extern const struct attribute_group md_bitmap_group;

static inline struct kernfs_node *sysfs_get_dirent_safe(struct kernfs_node *sd, char *name)
{
	if (sd)
		return sysfs_get_dirent(sd, name);
	return sd;
}
static inline void sysfs_notify_dirent_safe(struct kernfs_node *sd)
{
	if (sd)
		sysfs_notify_dirent(sd);
}

static inline char * mdname (struct mddev * mddev)
{
	return mddev->gendisk ? mddev->gendisk->disk_name : "mdX";
}

static inline int sysfs_link_rdev(struct mddev *mddev, struct md_rdev *rdev)
{
	char nm[20];
	if (!test_bit(Replacement, &rdev->flags) &&
	    !test_bit(Journal, &rdev->flags) &&
	    mddev->kobj.sd) {
		sprintf(nm, "rd%d", rdev->raid_disk);
		return sysfs_create_link(&mddev->kobj, &rdev->kobj, nm);
	} else
		return 0;
}

static inline void sysfs_unlink_rdev(struct mddev *mddev, struct md_rdev *rdev)
{
	char nm[20];
	if (!test_bit(Replacement, &rdev->flags) &&
	    !test_bit(Journal, &rdev->flags) &&
	    mddev->kobj.sd) {
		sprintf(nm, "rd%d", rdev->raid_disk);
		sysfs_remove_link(&mddev->kobj, nm);
	}
}


#define rdev_for_each_list(rdev, tmp, head)				\
	list_for_each_entry_safe(rdev, tmp, head, same_set)


#define rdev_for_each(rdev, mddev)				\
	list_for_each_entry(rdev, &((mddev)->disks), same_set)

#define rdev_for_each_safe(rdev, tmp, mddev)				\
	list_for_each_entry_safe(rdev, tmp, &((mddev)->disks), same_set)

#define rdev_for_each_rcu(rdev, mddev)				\
	list_for_each_entry_rcu(rdev, &((mddev)->disks), same_set)

struct md_thread {
	void			(*run) (struct md_thread *thread);
	struct mddev		*mddev;
	wait_queue_head_t	wqueue;
	unsigned long		flags;
	struct task_struct	*tsk;
	unsigned long		timeout;
	void			*private;
};

struct md_io_clone {
	struct mddev	*mddev;
	struct bio	*orig_bio;
	unsigned long	start_time;
	struct bio	bio_clone;
};

#define THREAD_WAKEUP  0

static inline void safe_put_page(struct page *p)
{
	if (p) put_page(p);
}

extern int register_md_personality(struct md_personality *p);
extern int unregister_md_personality(struct md_personality *p);
extern int register_md_cluster_operations(struct md_cluster_operations *ops,
		struct module *module);
extern int unregister_md_cluster_operations(void);
extern int md_setup_cluster(struct mddev *mddev, int nodes);
extern void md_cluster_stop(struct mddev *mddev);
extern struct md_thread *md_register_thread(
	void (*run)(struct md_thread *thread),
	struct mddev *mddev,
	const char *name);
extern void md_unregister_thread(struct mddev *mddev, struct md_thread __rcu **threadp);
extern void md_wakeup_thread(struct md_thread __rcu *thread);
extern void md_check_recovery(struct mddev *mddev);
extern void md_reap_sync_thread(struct mddev *mddev);
extern int mddev_init_writes_pending(struct mddev *mddev);
extern bool md_write_start(struct mddev *mddev, struct bio *bi);
extern void md_write_inc(struct mddev *mddev, struct bio *bi);
extern void md_write_end(struct mddev *mddev);
extern void md_done_sync(struct mddev *mddev, int blocks, int ok);
extern void md_error(struct mddev *mddev, struct md_rdev *rdev);
extern void md_finish_reshape(struct mddev *mddev);
void md_submit_discard_bio(struct mddev *mddev, struct md_rdev *rdev,
			struct bio *bio, sector_t start, sector_t size);
void md_account_bio(struct mddev *mddev, struct bio **bio);

extern bool __must_check md_flush_request(struct mddev *mddev, struct bio *bio);
extern void md_super_write(struct mddev *mddev, struct md_rdev *rdev,
			   sector_t sector, int size, struct page *page);
extern int md_super_wait(struct mddev *mddev);
extern int sync_page_io(struct md_rdev *rdev, sector_t sector, int size,
		struct page *page, blk_opf_t opf, bool metadata_op);
extern void md_do_sync(struct md_thread *thread);
extern void md_new_event(void);
extern void md_allow_write(struct mddev *mddev);
extern void md_wait_for_blocked_rdev(struct md_rdev *rdev, struct mddev *mddev);
extern void md_set_array_sectors(struct mddev *mddev, sector_t array_sectors);
extern int md_check_no_bitmap(struct mddev *mddev);
extern int md_integrity_register(struct mddev *mddev);
extern int md_integrity_add_rdev(struct md_rdev *rdev, struct mddev *mddev);
extern int strict_strtoul_scaled(const char *cp, unsigned long *res, int scale);

extern void mddev_init(struct mddev *mddev);
struct mddev *md_alloc(dev_t dev, char *name);
void mddev_put(struct mddev *mddev);
extern int md_run(struct mddev *mddev);
extern int md_start(struct mddev *mddev);
extern void md_stop(struct mddev *mddev);
extern void md_stop_writes(struct mddev *mddev);
extern int md_rdev_init(struct md_rdev *rdev);
extern void md_rdev_clear(struct md_rdev *rdev);

extern void md_handle_request(struct mddev *mddev, struct bio *bio);
extern void mddev_suspend(struct mddev *mddev);
extern void mddev_resume(struct mddev *mddev);

extern void md_reload_sb(struct mddev *mddev, int raid_disk);
extern void md_update_sb(struct mddev *mddev, int force);
extern void mddev_create_serial_pool(struct mddev *mddev, struct md_rdev *rdev,
				     bool is_suspend);
extern void mddev_destroy_serial_pool(struct mddev *mddev, struct md_rdev *rdev,
				      bool is_suspend);
struct md_rdev *md_find_rdev_nr_rcu(struct mddev *mddev, int nr);
struct md_rdev *md_find_rdev_rcu(struct mddev *mddev, dev_t dev);

static inline bool is_rdev_broken(struct md_rdev *rdev)
{
	return !disk_live(rdev->bdev->bd_disk);
}

static inline void rdev_dec_pending(struct md_rdev *rdev, struct mddev *mddev)
{
	int faulty = test_bit(Faulty, &rdev->flags);
	if (atomic_dec_and_test(&rdev->nr_pending) && faulty) {
		set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
		md_wakeup_thread(mddev->thread);
	}
}

extern struct md_cluster_operations *md_cluster_ops;
static inline int mddev_is_clustered(struct mddev *mddev)
{
	return mddev->cluster_info && mddev->bitmap_info.nodes > 1;
}


static inline void mddev_clear_unsupported_flags(struct mddev *mddev,
	unsigned long unsupported_flags)
{
	mddev->flags &= ~unsupported_flags;
}

static inline void mddev_check_write_zeroes(struct mddev *mddev, struct bio *bio)
{
	if (bio_op(bio) == REQ_OP_WRITE_ZEROES &&
	    !bio->bi_bdev->bd_disk->queue->limits.max_write_zeroes_sectors)
		mddev->queue->limits.max_write_zeroes_sectors = 0;
}

struct mdu_array_info_s;
struct mdu_disk_info_s;

extern int mdp_major;
extern struct workqueue_struct *md_bitmap_wq;
void md_autostart_arrays(int part);
int md_set_array_info(struct mddev *mddev, struct mdu_array_info_s *info);
int md_add_new_disk(struct mddev *mddev, struct mdu_disk_info_s *info);
int do_md_run(struct mddev *mddev);

extern const struct block_device_operations md_fops;

#endif 
