

#ifndef _FS_FUSE_I_H
#define _FS_FUSE_I_H

#ifndef pr_fmt
# define pr_fmt(fmt) "fuse: " fmt
#endif

#include <linux/fuse.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/backing-dev.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/rbtree.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/kref.h>
#include <linux/xattr.h>
#include <linux/pid_namespace.h>
#include <linux/refcount.h>
#include <linux/user_namespace.h>


#define FUSE_DEFAULT_MAX_PAGES_PER_REQ 32


#define FUSE_MAX_MAX_PAGES 256


#define FUSE_NOWRITE INT_MIN


#define FUSE_NAME_MAX 1024


#define FUSE_CTL_NUM_DENTRIES 5


extern struct list_head fuse_conn_list;


extern struct mutex fuse_mutex;


extern unsigned max_user_bgreq;
extern unsigned max_user_congthresh;


struct fuse_forget_link {
	struct fuse_forget_one forget_one;
	struct fuse_forget_link *next;
};


struct fuse_submount_lookup {
	
	refcount_t count;

	
	u64 nodeid;

	
	struct fuse_forget_link *forget;
};


struct fuse_inode {
	
	struct inode inode;

	
	u64 nodeid;

	
	u64 nlookup;

	
	struct fuse_forget_link *forget;

	
	u64 i_time;

	
	u32 inval_mask;

	
	umode_t orig_i_mode;

	
	struct timespec64 i_btime;

	
	u64 orig_ino;

	
	u64 attr_version;

	union {
		
		struct {
			
			struct list_head write_files;

			
			struct list_head queued_writes;

			
			int writectr;

			
			wait_queue_head_t page_waitq;

			
			struct rb_root writepages;
		};

		
		struct {
			
			bool cached;

			
			loff_t size;

			
			loff_t pos;

			
			u64 version;

			
			struct timespec64 mtime;

			
			u64 iversion;

			
			spinlock_t lock;
		} rdc;
	};

	
	unsigned long state;

	
	struct mutex mutex;

	
	spinlock_t lock;

#ifdef CONFIG_FUSE_DAX
	
	struct fuse_inode_dax *dax;
#endif
	
	struct fuse_submount_lookup *submount_lookup;
};


enum {
	
	FUSE_I_ADVISE_RDPLUS,
	
	FUSE_I_INIT_RDPLUS,
	
	FUSE_I_SIZE_UNSTABLE,
	
	FUSE_I_BAD,
	
	FUSE_I_BTIME,
};

struct fuse_conn;
struct fuse_mount;
struct fuse_release_args;


struct fuse_file {
	
	struct fuse_mount *fm;

	
	struct fuse_release_args *release_args;

	
	u64 kh;

	
	u64 fh;

	
	u64 nodeid;

	
	refcount_t count;

	
	u32 open_flags;

	
	struct list_head write_entry;

	
	struct {
		
		struct mutex lock;

		
		loff_t pos;

		
		loff_t cache_off;

		
		u64 version;

	} readdir;

	
	struct rb_node polled_node;

	
	wait_queue_head_t poll_wait;

	
	bool flock:1;
};


struct fuse_in_arg {
	unsigned size;
	const void *value;
};


struct fuse_arg {
	unsigned size;
	void *value;
};


struct fuse_page_desc {
	unsigned int length;
	unsigned int offset;
};

struct fuse_args {
	uint64_t nodeid;
	uint32_t opcode;
	uint8_t in_numargs;
	uint8_t out_numargs;
	uint8_t ext_idx;
	bool force:1;
	bool noreply:1;
	bool nocreds:1;
	bool in_pages:1;
	bool out_pages:1;
	bool user_pages:1;
	bool out_argvar:1;
	bool page_zeroing:1;
	bool page_replace:1;
	bool may_block:1;
	bool is_ext:1;
	struct fuse_in_arg in_args[3];
	struct fuse_arg out_args[2];
	void (*end)(struct fuse_mount *fm, struct fuse_args *args, int error);
};

struct fuse_args_pages {
	struct fuse_args args;
	struct page **pages;
	struct fuse_page_desc *descs;
	unsigned int num_pages;
};

#define FUSE_ARGS(args) struct fuse_args args = {}


struct fuse_io_priv {
	struct kref refcnt;
	int async;
	spinlock_t lock;
	unsigned reqs;
	ssize_t bytes;
	size_t size;
	__u64 offset;
	bool write;
	bool should_dirty;
	int err;
	struct kiocb *iocb;
	struct completion *done;
	bool blocking;
};

#define FUSE_IO_PRIV_SYNC(i) \
{					\
	.refcnt = KREF_INIT(1),		\
	.async = 0,			\
	.iocb = i,			\
}


enum fuse_req_flag {
	FR_ISREPLY,
	FR_FORCE,
	FR_BACKGROUND,
	FR_WAITING,
	FR_ABORTED,
	FR_INTERRUPTED,
	FR_LOCKED,
	FR_PENDING,
	FR_SENT,
	FR_FINISHED,
	FR_PRIVATE,
	FR_ASYNC,
};


struct fuse_req {
	
	struct list_head list;

	
	struct list_head intr_entry;

	
	struct fuse_args *args;

	
	refcount_t count;

	
	unsigned long flags;

	
	struct {
		struct fuse_in_header h;
	} in;

	
	struct {
		struct fuse_out_header h;
	} out;

	
	wait_queue_head_t waitq;

#if IS_ENABLED(CONFIG_VIRTIO_FS)
	
	void *argbuf;
#endif

	
	struct fuse_mount *fm;
};

struct fuse_iqueue;


struct fuse_iqueue_ops {
	
	void (*wake_forget_and_unlock)(struct fuse_iqueue *fiq)
		__releases(fiq->lock);

	
	void (*wake_interrupt_and_unlock)(struct fuse_iqueue *fiq)
		__releases(fiq->lock);

	
	void (*wake_pending_and_unlock)(struct fuse_iqueue *fiq)
		__releases(fiq->lock);

	
	void (*release)(struct fuse_iqueue *fiq);
};


extern const struct fuse_iqueue_ops fuse_dev_fiq_ops;

struct fuse_iqueue {
	
	unsigned connected;

	
	spinlock_t lock;

	
	wait_queue_head_t waitq;

	
	u64 reqctr;

	
	struct list_head pending;

	
	struct list_head interrupts;

	
	struct fuse_forget_link forget_list_head;
	struct fuse_forget_link *forget_list_tail;

	
	int forget_batch;

	
	struct fasync_struct *fasync;

	
	const struct fuse_iqueue_ops *ops;

	
	void *priv;
};

#define FUSE_PQ_HASH_BITS 8
#define FUSE_PQ_HASH_SIZE (1 << FUSE_PQ_HASH_BITS)

struct fuse_pqueue {
	
	unsigned connected;

	
	spinlock_t lock;

	
	struct list_head *processing;

	
	struct list_head io;
};


struct fuse_dev {
	
	struct fuse_conn *fc;

	
	struct fuse_pqueue pq;

	
	struct list_head entry;
};

enum fuse_dax_mode {
	FUSE_DAX_INODE_DEFAULT,	
	FUSE_DAX_ALWAYS,	
	FUSE_DAX_NEVER,		
	FUSE_DAX_INODE_USER,	
};

static inline bool fuse_is_inode_dax_mode(enum fuse_dax_mode mode)
{
	return mode == FUSE_DAX_INODE_DEFAULT || mode == FUSE_DAX_INODE_USER;
}

struct fuse_fs_context {
	int fd;
	struct file *file;
	unsigned int rootmode;
	kuid_t user_id;
	kgid_t group_id;
	bool is_bdev:1;
	bool fd_present:1;
	bool rootmode_present:1;
	bool user_id_present:1;
	bool group_id_present:1;
	bool default_permissions:1;
	bool allow_other:1;
	bool destroy:1;
	bool no_control:1;
	bool no_force_umount:1;
	bool legacy_opts_show:1;
	enum fuse_dax_mode dax_mode;
	unsigned int max_read;
	unsigned int blksize;
	const char *subtype;

	
	struct dax_device *dax_dev;

	
	void **fudptr;
};

struct fuse_sync_bucket {
	
	atomic_t count;
	wait_queue_head_t waitq;
	struct rcu_head rcu;
};


struct fuse_conn {
	
	spinlock_t lock;

	
	refcount_t count;

	
	atomic_t dev_count;

	struct rcu_head rcu;

	
	kuid_t user_id;

	
	kgid_t group_id;

	
	struct pid_namespace *pid_ns;

	
	struct user_namespace *user_ns;

	
	unsigned max_read;

	
	unsigned max_write;

	
	unsigned int max_pages;

	
	unsigned int max_pages_limit;

	
	struct fuse_iqueue iq;

	
	atomic64_t khctr;

	
	struct rb_root polled_files;

	
	unsigned max_background;

	
	unsigned congestion_threshold;

	
	unsigned num_background;

	
	unsigned active_background;

	
	struct list_head bg_queue;

	
	spinlock_t bg_lock;

	
	int initialized;

	
	int blocked;

	
	wait_queue_head_t blocked_waitq;

	
	unsigned connected;

	
	bool aborted;

	
	unsigned conn_error:1;

	
	unsigned conn_init:1;

	
	unsigned async_read:1;

	
	unsigned abort_err:1;

	
	unsigned atomic_o_trunc:1;

	
	unsigned export_support:1;

	
	unsigned writeback_cache:1;

	
	unsigned parallel_dirops:1;

	
	unsigned handle_killpriv:1;

	
	unsigned cache_symlinks:1;

	
	unsigned int legacy_opts_show:1;

	
	unsigned handle_killpriv_v2:1;

	

	
	unsigned no_open:1;

	
	unsigned no_opendir:1;

	
	unsigned no_fsync:1;

	
	unsigned no_fsyncdir:1;

	
	unsigned no_flush:1;

	
	unsigned no_setxattr:1;

	
	unsigned setxattr_ext:1;

	
	unsigned no_getxattr:1;

	
	unsigned no_listxattr:1;

	
	unsigned no_removexattr:1;

	
	unsigned no_lock:1;

	
	unsigned no_access:1;

	
	unsigned no_create:1;

	
	unsigned no_interrupt:1;

	
	unsigned no_bmap:1;

	
	unsigned no_poll:1;

	
	unsigned big_writes:1;

	
	unsigned dont_mask:1;

	
	unsigned no_flock:1;

	
	unsigned no_fallocate:1;

	
	unsigned no_rename2:1;

	
	unsigned auto_inval_data:1;

	
	unsigned explicit_inval_data:1;

	
	unsigned do_readdirplus:1;

	
	unsigned readdirplus_auto:1;

	
	unsigned async_dio:1;

	
	unsigned no_lseek:1;

	
	unsigned posix_acl:1;

	
	unsigned default_permissions:1;

	
	unsigned allow_other:1;

	
	unsigned no_copy_file_range:1;

	
	unsigned int destroy:1;

	
	unsigned int delete_stale:1;

	
	unsigned int no_control:1;

	
	unsigned int no_force_umount:1;

	
	unsigned int auto_submounts:1;

	
	unsigned int sync_fs:1;

	
	unsigned int init_security:1;

	
	unsigned int create_supp_group:1;

	
	unsigned int inode_dax:1;

	
	unsigned int no_tmpfile:1;

	
	unsigned int direct_io_allow_mmap:1;

	
	unsigned int no_statx:1;

	
	atomic_t num_waiting;

	
	unsigned minor;

	
	struct list_head entry;

	
	dev_t dev;

	
	struct dentry *ctl_dentry[FUSE_CTL_NUM_DENTRIES];

	
	int ctl_ndents;

	
	u32 scramble_key[4];

	
	atomic64_t attr_version;

	
	void (*release)(struct fuse_conn *);

	
	struct rw_semaphore killsb;

	
	struct list_head devices;

#ifdef CONFIG_FUSE_DAX
	
	enum fuse_dax_mode dax_mode;

	
	struct fuse_conn_dax *dax;
#endif

	
	struct list_head mounts;

	
	struct fuse_sync_bucket __rcu *curr_bucket;
};


struct fuse_mount {
	
	struct fuse_conn *fc;

	
	struct super_block *sb;

	
	struct list_head fc_entry;
};

static inline struct fuse_mount *get_fuse_mount_super(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct fuse_conn *get_fuse_conn_super(struct super_block *sb)
{
	return get_fuse_mount_super(sb)->fc;
}

static inline struct fuse_mount *get_fuse_mount(struct inode *inode)
{
	return get_fuse_mount_super(inode->i_sb);
}

static inline struct fuse_conn *get_fuse_conn(struct inode *inode)
{
	return get_fuse_mount_super(inode->i_sb)->fc;
}

static inline struct fuse_inode *get_fuse_inode(struct inode *inode)
{
	return container_of(inode, struct fuse_inode, inode);
}

static inline u64 get_node_id(struct inode *inode)
{
	return get_fuse_inode(inode)->nodeid;
}

static inline int invalid_nodeid(u64 nodeid)
{
	return !nodeid || nodeid == FUSE_ROOT_ID;
}

static inline u64 fuse_get_attr_version(struct fuse_conn *fc)
{
	return atomic64_read(&fc->attr_version);
}

static inline bool fuse_stale_inode(const struct inode *inode, int generation,
				    struct fuse_attr *attr)
{
	return inode->i_generation != generation ||
		inode_wrong_type(inode, attr->mode);
}

static inline void fuse_make_bad(struct inode *inode)
{
	remove_inode_hash(inode);
	set_bit(FUSE_I_BAD, &get_fuse_inode(inode)->state);
}

static inline bool fuse_is_bad(struct inode *inode)
{
	return unlikely(test_bit(FUSE_I_BAD, &get_fuse_inode(inode)->state));
}

static inline struct page **fuse_pages_alloc(unsigned int npages, gfp_t flags,
					     struct fuse_page_desc **desc)
{
	struct page **pages;

	pages = kzalloc(npages * (sizeof(struct page *) +
				  sizeof(struct fuse_page_desc)), flags);
	*desc = (void *) (pages + npages);

	return pages;
}

static inline void fuse_page_descs_length_init(struct fuse_page_desc *descs,
					       unsigned int index,
					       unsigned int nr_pages)
{
	int i;

	for (i = index; i < index + nr_pages; i++)
		descs[i].length = PAGE_SIZE - descs[i].offset;
}

static inline void fuse_sync_bucket_dec(struct fuse_sync_bucket *bucket)
{
	
	rcu_read_lock();
	if (atomic_dec_and_test(&bucket->count))
		wake_up(&bucket->waitq);
	rcu_read_unlock();
}


extern const struct file_operations fuse_dev_operations;

extern const struct dentry_operations fuse_dentry_operations;
extern const struct dentry_operations fuse_root_dentry_operations;


struct inode *fuse_iget(struct super_block *sb, u64 nodeid,
			int generation, struct fuse_attr *attr,
			u64 attr_valid, u64 attr_version);

int fuse_lookup_name(struct super_block *sb, u64 nodeid, const struct qstr *name,
		     struct fuse_entry_out *outarg, struct inode **inode);


void fuse_queue_forget(struct fuse_conn *fc, struct fuse_forget_link *forget,
		       u64 nodeid, u64 nlookup);

struct fuse_forget_link *fuse_alloc_forget(void);

struct fuse_forget_link *fuse_dequeue_forget(struct fuse_iqueue *fiq,
					     unsigned int max,
					     unsigned int *countp);


struct fuse_io_args {
	union {
		struct {
			struct fuse_read_in in;
			u64 attr_ver;
		} read;
		struct {
			struct fuse_write_in in;
			struct fuse_write_out out;
			bool page_locked;
		} write;
	};
	struct fuse_args_pages ap;
	struct fuse_io_priv *io;
	struct fuse_file *ff;
};

void fuse_read_args_fill(struct fuse_io_args *ia, struct file *file, loff_t pos,
			 size_t count, int opcode);



int fuse_open_common(struct inode *inode, struct file *file, bool isdir);

struct fuse_file *fuse_file_alloc(struct fuse_mount *fm);
void fuse_file_free(struct fuse_file *ff);
void fuse_finish_open(struct inode *inode, struct file *file);

void fuse_sync_release(struct fuse_inode *fi, struct fuse_file *ff,
		       unsigned int flags);


void fuse_release_common(struct file *file, bool isdir);


int fuse_fsync_common(struct file *file, loff_t start, loff_t end,
		      int datasync, int opcode);


int fuse_notify_poll_wakeup(struct fuse_conn *fc,
			    struct fuse_notify_poll_wakeup_out *outarg);


void fuse_init_file_inode(struct inode *inode, unsigned int flags);


void fuse_init_common(struct inode *inode);


void fuse_init_dir(struct inode *inode);


void fuse_init_symlink(struct inode *inode);


void fuse_change_attributes(struct inode *inode, struct fuse_attr *attr,
			    struct fuse_statx *sx,
			    u64 attr_valid, u64 attr_version);

void fuse_change_attributes_common(struct inode *inode, struct fuse_attr *attr,
				   struct fuse_statx *sx,
				   u64 attr_valid, u32 cache_mask);

u32 fuse_get_cache_mask(struct inode *inode);


int fuse_dev_init(void);


void fuse_dev_cleanup(void);

int fuse_ctl_init(void);
void __exit fuse_ctl_cleanup(void);


ssize_t fuse_simple_request(struct fuse_mount *fm, struct fuse_args *args);
int fuse_simple_background(struct fuse_mount *fm, struct fuse_args *args,
			   gfp_t gfp_flags);


void fuse_request_end(struct fuse_req *req);


void fuse_abort_conn(struct fuse_conn *fc);
void fuse_wait_aborted(struct fuse_conn *fc);




#define FUSE_STATX_MODIFY	(STATX_MTIME | STATX_CTIME | STATX_BLOCKS)


#define FUSE_STATX_MODSIZE	(FUSE_STATX_MODIFY | STATX_SIZE)

void fuse_invalidate_attr(struct inode *inode);
void fuse_invalidate_attr_mask(struct inode *inode, u32 mask);

void fuse_invalidate_entry_cache(struct dentry *entry);

void fuse_invalidate_atime(struct inode *inode);

u64 fuse_time_to_jiffies(u64 sec, u32 nsec);
#define ATTR_TIMEOUT(o) \
	fuse_time_to_jiffies((o)->attr_valid, (o)->attr_valid_nsec)

void fuse_change_entry_timeout(struct dentry *entry, struct fuse_entry_out *o);


struct fuse_conn *fuse_conn_get(struct fuse_conn *fc);


void fuse_conn_init(struct fuse_conn *fc, struct fuse_mount *fm,
		    struct user_namespace *user_ns,
		    const struct fuse_iqueue_ops *fiq_ops, void *fiq_priv);


void fuse_conn_put(struct fuse_conn *fc);

struct fuse_dev *fuse_dev_alloc_install(struct fuse_conn *fc);
struct fuse_dev *fuse_dev_alloc(void);
void fuse_dev_install(struct fuse_dev *fud, struct fuse_conn *fc);
void fuse_dev_free(struct fuse_dev *fud);
void fuse_send_init(struct fuse_mount *fm);


int fuse_fill_super_common(struct super_block *sb, struct fuse_fs_context *ctx);


bool fuse_mount_remove(struct fuse_mount *fm);


int fuse_init_fs_context_submount(struct fs_context *fsc);


void fuse_conn_destroy(struct fuse_mount *fm);


void fuse_mount_destroy(struct fuse_mount *fm);


int fuse_ctl_add_conn(struct fuse_conn *fc);


void fuse_ctl_remove_conn(struct fuse_conn *fc);


int fuse_valid_type(int m);

bool fuse_invalid_attr(struct fuse_attr *attr);


bool fuse_allow_current_process(struct fuse_conn *fc);

u64 fuse_lock_owner_id(struct fuse_conn *fc, fl_owner_t id);

void fuse_flush_time_update(struct inode *inode);
void fuse_update_ctime(struct inode *inode);

int fuse_update_attributes(struct inode *inode, struct file *file, u32 mask);

void fuse_flush_writepages(struct inode *inode);

void fuse_set_nowrite(struct inode *inode);
void fuse_release_nowrite(struct inode *inode);


struct inode *fuse_ilookup(struct fuse_conn *fc, u64 nodeid,
			   struct fuse_mount **fm);


int fuse_reverse_inval_inode(struct fuse_conn *fc, u64 nodeid,
			     loff_t offset, loff_t len);


int fuse_reverse_inval_entry(struct fuse_conn *fc, u64 parent_nodeid,
			     u64 child_nodeid, struct qstr *name, u32 flags);

int fuse_do_open(struct fuse_mount *fm, u64 nodeid, struct file *file,
		 bool isdir);




#define FUSE_DIO_WRITE (1 << 0)


#define FUSE_DIO_CUSE  (1 << 1)

ssize_t fuse_direct_io(struct fuse_io_priv *io, struct iov_iter *iter,
		       loff_t *ppos, int flags);
long fuse_do_ioctl(struct file *file, unsigned int cmd, unsigned long arg,
		   unsigned int flags);
long fuse_ioctl_common(struct file *file, unsigned int cmd,
		       unsigned long arg, unsigned int flags);
__poll_t fuse_file_poll(struct file *file, poll_table *wait);
int fuse_dev_release(struct inode *inode, struct file *file);

bool fuse_write_update_attr(struct inode *inode, loff_t pos, ssize_t written);

int fuse_flush_times(struct inode *inode, struct fuse_file *ff);
int fuse_write_inode(struct inode *inode, struct writeback_control *wbc);

int fuse_do_setattr(struct dentry *dentry, struct iattr *attr,
		    struct file *file);

void fuse_set_initialized(struct fuse_conn *fc);

void fuse_unlock_inode(struct inode *inode, bool locked);
bool fuse_lock_inode(struct inode *inode);

int fuse_setxattr(struct inode *inode, const char *name, const void *value,
		  size_t size, int flags, unsigned int extra_flags);
ssize_t fuse_getxattr(struct inode *inode, const char *name, void *value,
		      size_t size);
ssize_t fuse_listxattr(struct dentry *entry, char *list, size_t size);
int fuse_removexattr(struct inode *inode, const char *name);
extern const struct xattr_handler *fuse_xattr_handlers[];

struct posix_acl;
struct posix_acl *fuse_get_inode_acl(struct inode *inode, int type, bool rcu);
struct posix_acl *fuse_get_acl(struct mnt_idmap *idmap,
			       struct dentry *dentry, int type);
int fuse_set_acl(struct mnt_idmap *, struct dentry *dentry,
		 struct posix_acl *acl, int type);


int fuse_readdir(struct file *file, struct dir_context *ctx);


unsigned int fuse_len_args(unsigned int numargs, struct fuse_arg *args);


u64 fuse_get_unique(struct fuse_iqueue *fiq);
void fuse_free_conn(struct fuse_conn *fc);



#define FUSE_IS_DAX(inode) (IS_ENABLED(CONFIG_FUSE_DAX) && IS_DAX(inode))

ssize_t fuse_dax_read_iter(struct kiocb *iocb, struct iov_iter *to);
ssize_t fuse_dax_write_iter(struct kiocb *iocb, struct iov_iter *from);
int fuse_dax_mmap(struct file *file, struct vm_area_struct *vma);
int fuse_dax_break_layouts(struct inode *inode, u64 dmap_start, u64 dmap_end);
int fuse_dax_conn_alloc(struct fuse_conn *fc, enum fuse_dax_mode mode,
			struct dax_device *dax_dev);
void fuse_dax_conn_free(struct fuse_conn *fc);
bool fuse_dax_inode_alloc(struct super_block *sb, struct fuse_inode *fi);
void fuse_dax_inode_init(struct inode *inode, unsigned int flags);
void fuse_dax_inode_cleanup(struct inode *inode);
void fuse_dax_dontcache(struct inode *inode, unsigned int flags);
bool fuse_dax_check_alignment(struct fuse_conn *fc, unsigned int map_alignment);
void fuse_dax_cancel_work(struct fuse_conn *fc);


long fuse_file_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
long fuse_file_compat_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg);
int fuse_fileattr_get(struct dentry *dentry, struct fileattr *fa);
int fuse_fileattr_set(struct mnt_idmap *idmap,
		      struct dentry *dentry, struct fileattr *fa);



struct fuse_file *fuse_file_open(struct fuse_mount *fm, u64 nodeid,
				 unsigned int open_flags, bool isdir);
void fuse_file_release(struct inode *inode, struct fuse_file *ff,
		       unsigned int open_flags, fl_owner_t id, bool isdir);

#endif 
