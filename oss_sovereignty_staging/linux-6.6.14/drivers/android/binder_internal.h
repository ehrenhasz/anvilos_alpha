 

#ifndef _LINUX_BINDER_INTERNAL_H
#define _LINUX_BINDER_INTERNAL_H

#include <linux/export.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/uidgid.h>
#include <uapi/linux/android/binderfs.h>
#include "binder_alloc.h"

struct binder_context {
	struct binder_node *binder_context_mgr_node;
	struct mutex context_mgr_node_lock;
	kuid_t binder_context_mgr_uid;
	const char *name;
};

 
struct binder_device {
	struct hlist_node hlist;
	struct miscdevice miscdev;
	struct binder_context context;
	struct inode *binderfs_inode;
	refcount_t ref;
};

 
struct binderfs_mount_opts {
	int max;
	int stats_mode;
};

 
struct binderfs_info {
	struct ipc_namespace *ipc_ns;
	struct dentry *control_dentry;
	kuid_t root_uid;
	kgid_t root_gid;
	struct binderfs_mount_opts mount_opts;
	int device_count;
	struct dentry *proc_log_dir;
};

extern const struct file_operations binder_fops;

extern char *binder_devices_param;

#ifdef CONFIG_ANDROID_BINDERFS
extern bool is_binderfs_device(const struct inode *inode);
extern struct dentry *binderfs_create_file(struct dentry *dir, const char *name,
					   const struct file_operations *fops,
					   void *data);
extern void binderfs_remove_file(struct dentry *dentry);
#else
static inline bool is_binderfs_device(const struct inode *inode)
{
	return false;
}
static inline struct dentry *binderfs_create_file(struct dentry *dir,
					   const char *name,
					   const struct file_operations *fops,
					   void *data)
{
	return NULL;
}
static inline void binderfs_remove_file(struct dentry *dentry) {}
#endif

#ifdef CONFIG_ANDROID_BINDERFS
extern int __init init_binderfs(void);
#else
static inline int __init init_binderfs(void)
{
	return 0;
}
#endif

struct binder_debugfs_entry {
	const char *name;
	umode_t mode;
	const struct file_operations *fops;
	void *data;
};

extern const struct binder_debugfs_entry binder_debugfs_entries[];

#define binder_for_each_debugfs_entry(entry)	\
	for ((entry) = binder_debugfs_entries;	\
	     (entry)->name;			\
	     (entry)++)

enum binder_stat_types {
	BINDER_STAT_PROC,
	BINDER_STAT_THREAD,
	BINDER_STAT_NODE,
	BINDER_STAT_REF,
	BINDER_STAT_DEATH,
	BINDER_STAT_TRANSACTION,
	BINDER_STAT_TRANSACTION_COMPLETE,
	BINDER_STAT_COUNT
};

struct binder_stats {
	atomic_t br[_IOC_NR(BR_TRANSACTION_PENDING_FROZEN) + 1];
	atomic_t bc[_IOC_NR(BC_REPLY_SG) + 1];
	atomic_t obj_created[BINDER_STAT_COUNT];
	atomic_t obj_deleted[BINDER_STAT_COUNT];
};

 
struct binder_work {
	struct list_head entry;

	enum binder_work_type {
		BINDER_WORK_TRANSACTION = 1,
		BINDER_WORK_TRANSACTION_COMPLETE,
		BINDER_WORK_TRANSACTION_PENDING,
		BINDER_WORK_TRANSACTION_ONEWAY_SPAM_SUSPECT,
		BINDER_WORK_RETURN_ERROR,
		BINDER_WORK_NODE,
		BINDER_WORK_DEAD_BINDER,
		BINDER_WORK_DEAD_BINDER_AND_CLEAR,
		BINDER_WORK_CLEAR_DEATH_NOTIFICATION,
	} type;
};

struct binder_error {
	struct binder_work work;
	uint32_t cmd;
};

 
struct binder_node {
	int debug_id;
	spinlock_t lock;
	struct binder_work work;
	union {
		struct rb_node rb_node;
		struct hlist_node dead_node;
	};
	struct binder_proc *proc;
	struct hlist_head refs;
	int internal_strong_refs;
	int local_weak_refs;
	int local_strong_refs;
	int tmp_refs;
	binder_uintptr_t ptr;
	binder_uintptr_t cookie;
	struct {
		 
		u8 has_strong_ref:1;
		u8 pending_strong_ref:1;
		u8 has_weak_ref:1;
		u8 pending_weak_ref:1;
	};
	struct {
		 
		u8 accept_fds:1;
		u8 txn_security_ctx:1;
		u8 min_priority;
	};
	bool has_async_transaction;
	struct list_head async_todo;
};

struct binder_ref_death {
	 
	struct binder_work work;
	binder_uintptr_t cookie;
};

 
struct binder_ref_data {
	int debug_id;
	uint32_t desc;
	int strong;
	int weak;
};

 
struct binder_ref {
	 
	 
	 
	 
	struct binder_ref_data data;
	struct rb_node rb_node_desc;
	struct rb_node rb_node_node;
	struct hlist_node node_entry;
	struct binder_proc *proc;
	struct binder_node *node;
	struct binder_ref_death *death;
};

 
struct binder_proc {
	struct hlist_node proc_node;
	struct rb_root threads;
	struct rb_root nodes;
	struct rb_root refs_by_desc;
	struct rb_root refs_by_node;
	struct list_head waiting_threads;
	int pid;
	struct task_struct *tsk;
	const struct cred *cred;
	struct hlist_node deferred_work_node;
	int deferred_work;
	int outstanding_txns;
	bool is_dead;
	bool is_frozen;
	bool sync_recv;
	bool async_recv;
	wait_queue_head_t freeze_wait;

	struct list_head todo;
	struct binder_stats stats;
	struct list_head delivered_death;
	int max_threads;
	int requested_threads;
	int requested_threads_started;
	int tmp_ref;
	long default_priority;
	struct dentry *debugfs_entry;
	struct binder_alloc alloc;
	struct binder_context *context;
	spinlock_t inner_lock;
	spinlock_t outer_lock;
	struct dentry *binderfs_entry;
	bool oneway_spam_detection_enabled;
};

 
struct binder_thread {
	struct binder_proc *proc;
	struct rb_node rb_node;
	struct list_head waiting_thread_node;
	int pid;
	int looper;               
	bool looper_need_return;  
	struct binder_transaction *transaction_stack;
	struct list_head todo;
	bool process_todo;
	struct binder_error return_error;
	struct binder_error reply_error;
	struct binder_extended_error ee;
	wait_queue_head_t wait;
	struct binder_stats stats;
	atomic_t tmp_ref;
	bool is_dead;
};

 
struct binder_txn_fd_fixup {
	struct list_head fixup_entry;
	struct file *file;
	size_t offset;
	int target_fd;
};

struct binder_transaction {
	int debug_id;
	struct binder_work work;
	struct binder_thread *from;
	pid_t from_pid;
	pid_t from_tid;
	struct binder_transaction *from_parent;
	struct binder_proc *to_proc;
	struct binder_thread *to_thread;
	struct binder_transaction *to_parent;
	unsigned need_reply:1;
	         

	struct binder_buffer *buffer;
	unsigned int    code;
	unsigned int    flags;
	long    priority;
	long    saved_priority;
	kuid_t  sender_euid;
	ktime_t start_time;
	struct list_head fd_fixups;
	binder_uintptr_t security_ctx;
	 
	spinlock_t lock;
};

 
struct binder_object {
	union {
		struct binder_object_header hdr;
		struct flat_binder_object fbo;
		struct binder_fd_object fdo;
		struct binder_buffer_object bbo;
		struct binder_fd_array_object fdao;
	};
};

#endif  
