
 

#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/spinlock.h>
#include <linux/page-flags.h>
#include <asm/bug.h>
#include "misc.h"
#include "ctree.h"
#include "extent_io.h"
#include "locking.h"
#include "accessors.h"

 
#ifdef CONFIG_DEBUG_LOCK_ALLOC
#if BTRFS_MAX_LEVEL != 8
#error
#endif

#define DEFINE_LEVEL(stem, level)					\
	.names[level] = "btrfs-" stem "-0" #level,

#define DEFINE_NAME(stem)						\
	DEFINE_LEVEL(stem, 0)						\
	DEFINE_LEVEL(stem, 1)						\
	DEFINE_LEVEL(stem, 2)						\
	DEFINE_LEVEL(stem, 3)						\
	DEFINE_LEVEL(stem, 4)						\
	DEFINE_LEVEL(stem, 5)						\
	DEFINE_LEVEL(stem, 6)						\
	DEFINE_LEVEL(stem, 7)

static struct btrfs_lockdep_keyset {
	u64			id;		 
	 
	char			names[BTRFS_MAX_LEVEL][24];
	struct lock_class_key	keys[BTRFS_MAX_LEVEL];
} btrfs_lockdep_keysets[] = {
	{ .id = BTRFS_ROOT_TREE_OBJECTID,	DEFINE_NAME("root")	},
	{ .id = BTRFS_EXTENT_TREE_OBJECTID,	DEFINE_NAME("extent")	},
	{ .id = BTRFS_CHUNK_TREE_OBJECTID,	DEFINE_NAME("chunk")	},
	{ .id = BTRFS_DEV_TREE_OBJECTID,	DEFINE_NAME("dev")	},
	{ .id = BTRFS_CSUM_TREE_OBJECTID,	DEFINE_NAME("csum")	},
	{ .id = BTRFS_QUOTA_TREE_OBJECTID,	DEFINE_NAME("quota")	},
	{ .id = BTRFS_TREE_LOG_OBJECTID,	DEFINE_NAME("log")	},
	{ .id = BTRFS_TREE_RELOC_OBJECTID,	DEFINE_NAME("treloc")	},
	{ .id = BTRFS_DATA_RELOC_TREE_OBJECTID,	DEFINE_NAME("dreloc")	},
	{ .id = BTRFS_UUID_TREE_OBJECTID,	DEFINE_NAME("uuid")	},
	{ .id = BTRFS_FREE_SPACE_TREE_OBJECTID,	DEFINE_NAME("free-space") },
	{ .id = BTRFS_BLOCK_GROUP_TREE_OBJECTID, DEFINE_NAME("block-group") },
	{ .id = 0,				DEFINE_NAME("tree")	},
};

#undef DEFINE_LEVEL
#undef DEFINE_NAME

void btrfs_set_buffer_lockdep_class(u64 objectid, struct extent_buffer *eb, int level)
{
	struct btrfs_lockdep_keyset *ks;

	BUG_ON(level >= ARRAY_SIZE(ks->keys));

	 
	for (ks = btrfs_lockdep_keysets; ks->id; ks++)
		if (ks->id == objectid)
			break;

	lockdep_set_class_and_name(&eb->lock, &ks->keys[level], ks->names[level]);
}

void btrfs_maybe_reset_lockdep_class(struct btrfs_root *root, struct extent_buffer *eb)
{
	if (test_bit(BTRFS_ROOT_RESET_LOCKDEP_CLASS, &root->state))
		btrfs_set_buffer_lockdep_class(root->root_key.objectid,
					       eb, btrfs_header_level(eb));
}

#endif

 

 
void __btrfs_tree_read_lock(struct extent_buffer *eb, enum btrfs_lock_nesting nest)
{
	u64 start_ns = 0;

	if (trace_btrfs_tree_read_lock_enabled())
		start_ns = ktime_get_ns();

	down_read_nested(&eb->lock, nest);
	trace_btrfs_tree_read_lock(eb, start_ns);
}

void btrfs_tree_read_lock(struct extent_buffer *eb)
{
	__btrfs_tree_read_lock(eb, BTRFS_NESTING_NORMAL);
}

 
int btrfs_try_tree_read_lock(struct extent_buffer *eb)
{
	if (down_read_trylock(&eb->lock)) {
		trace_btrfs_try_tree_read_lock(eb);
		return 1;
	}
	return 0;
}

 
int btrfs_try_tree_write_lock(struct extent_buffer *eb)
{
	if (down_write_trylock(&eb->lock)) {
		eb->lock_owner = current->pid;
		trace_btrfs_try_tree_write_lock(eb);
		return 1;
	}
	return 0;
}

 
void btrfs_tree_read_unlock(struct extent_buffer *eb)
{
	trace_btrfs_tree_read_unlock(eb);
	up_read(&eb->lock);
}

 
void __btrfs_tree_lock(struct extent_buffer *eb, enum btrfs_lock_nesting nest)
	__acquires(&eb->lock)
{
	u64 start_ns = 0;

	if (trace_btrfs_tree_lock_enabled())
		start_ns = ktime_get_ns();

	down_write_nested(&eb->lock, nest);
	eb->lock_owner = current->pid;
	trace_btrfs_tree_lock(eb, start_ns);
}

void btrfs_tree_lock(struct extent_buffer *eb)
{
	__btrfs_tree_lock(eb, BTRFS_NESTING_NORMAL);
}

 
void btrfs_tree_unlock(struct extent_buffer *eb)
{
	trace_btrfs_tree_unlock(eb);
	eb->lock_owner = 0;
	up_write(&eb->lock);
}

 
void btrfs_unlock_up_safe(struct btrfs_path *path, int level)
{
	int i;

	if (path->keep_locks)
		return;

	for (i = level; i < BTRFS_MAX_LEVEL; i++) {
		if (!path->nodes[i])
			continue;
		if (!path->locks[i])
			continue;
		btrfs_tree_unlock_rw(path->nodes[i], path->locks[i]);
		path->locks[i] = 0;
	}
}

 
struct extent_buffer *btrfs_lock_root_node(struct btrfs_root *root)
{
	struct extent_buffer *eb;

	while (1) {
		eb = btrfs_root_node(root);

		btrfs_maybe_reset_lockdep_class(root, eb);
		btrfs_tree_lock(eb);
		if (eb == root->node)
			break;
		btrfs_tree_unlock(eb);
		free_extent_buffer(eb);
	}
	return eb;
}

 
struct extent_buffer *btrfs_read_lock_root_node(struct btrfs_root *root)
{
	struct extent_buffer *eb;

	while (1) {
		eb = btrfs_root_node(root);

		btrfs_maybe_reset_lockdep_class(root, eb);
		btrfs_tree_read_lock(eb);
		if (eb == root->node)
			break;
		btrfs_tree_read_unlock(eb);
		free_extent_buffer(eb);
	}
	return eb;
}

 
struct extent_buffer *btrfs_try_read_lock_root_node(struct btrfs_root *root)
{
	struct extent_buffer *eb;

	while (1) {
		eb = btrfs_root_node(root);
		if (!btrfs_try_tree_read_lock(eb)) {
			free_extent_buffer(eb);
			return ERR_PTR(-EAGAIN);
		}
		if (eb == root->node)
			break;
		btrfs_tree_read_unlock(eb);
		free_extent_buffer(eb);
	}
	return eb;
}

 

void btrfs_drew_lock_init(struct btrfs_drew_lock *lock)
{
	atomic_set(&lock->readers, 0);
	atomic_set(&lock->writers, 0);
	init_waitqueue_head(&lock->pending_readers);
	init_waitqueue_head(&lock->pending_writers);
}

 
bool btrfs_drew_try_write_lock(struct btrfs_drew_lock *lock)
{
	if (atomic_read(&lock->readers))
		return false;

	atomic_inc(&lock->writers);

	 
	smp_mb__after_atomic();
	if (atomic_read(&lock->readers)) {
		btrfs_drew_write_unlock(lock);
		return false;
	}

	return true;
}

void btrfs_drew_write_lock(struct btrfs_drew_lock *lock)
{
	while (true) {
		if (btrfs_drew_try_write_lock(lock))
			return;
		wait_event(lock->pending_writers, !atomic_read(&lock->readers));
	}
}

void btrfs_drew_write_unlock(struct btrfs_drew_lock *lock)
{
	atomic_dec(&lock->writers);
	cond_wake_up(&lock->pending_readers);
}

void btrfs_drew_read_lock(struct btrfs_drew_lock *lock)
{
	atomic_inc(&lock->readers);

	 
	smp_mb__after_atomic();

	wait_event(lock->pending_readers, atomic_read(&lock->writers) == 0);
}

void btrfs_drew_read_unlock(struct btrfs_drew_lock *lock)
{
	 
	if (atomic_dec_and_test(&lock->readers))
		wake_up(&lock->pending_writers);
}
