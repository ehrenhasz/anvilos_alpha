#ifndef BTRFS_LOCKING_H
#define BTRFS_LOCKING_H
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/percpu_counter.h>
#include "extent_io.h"
#define BTRFS_WRITE_LOCK 1
#define BTRFS_READ_LOCK 2
enum btrfs_lock_nesting {
	BTRFS_NESTING_NORMAL,
	BTRFS_NESTING_COW,
	BTRFS_NESTING_LEFT,
	BTRFS_NESTING_RIGHT,
	BTRFS_NESTING_LEFT_COW,
	BTRFS_NESTING_RIGHT_COW,
	BTRFS_NESTING_SPLIT,
	BTRFS_NESTING_NEW_ROOT,
	BTRFS_NESTING_MAX,
};
enum btrfs_lockdep_trans_states {
	BTRFS_LOCKDEP_TRANS_COMMIT_PREP,
	BTRFS_LOCKDEP_TRANS_UNBLOCKED,
	BTRFS_LOCKDEP_TRANS_SUPER_COMMITTED,
	BTRFS_LOCKDEP_TRANS_COMPLETED,
};
#define btrfs_might_wait_for_event(owner, lock)					\
	do {									\
		rwsem_acquire(&owner->lock##_map, 0, 0, _THIS_IP_);		\
		rwsem_release(&owner->lock##_map, _THIS_IP_);			\
	} while (0)
#define btrfs_lockdep_acquire(owner, lock)					\
	rwsem_acquire_read(&owner->lock##_map, 0, 0, _THIS_IP_)
#define btrfs_lockdep_release(owner, lock)					\
	rwsem_release(&owner->lock##_map, _THIS_IP_)
#define btrfs_might_wait_for_state(owner, i)					\
	do {									\
		rwsem_acquire(&owner->btrfs_state_change_map[i], 0, 0, _THIS_IP_); \
		rwsem_release(&owner->btrfs_state_change_map[i], _THIS_IP_);	\
	} while (0)
#define btrfs_trans_state_lockdep_acquire(owner, i)				\
	rwsem_acquire_read(&owner->btrfs_state_change_map[i], 0, 0, _THIS_IP_)
#define btrfs_trans_state_lockdep_release(owner, i)				\
	rwsem_release(&owner->btrfs_state_change_map[i], _THIS_IP_)
#define btrfs_lockdep_init_map(owner, lock)					\
	do {									\
		static struct lock_class_key lock##_key;			\
		lockdep_init_map(&owner->lock##_map, #lock, &lock##_key, 0);	\
	} while (0)
#define btrfs_state_lockdep_init_map(owner, lock, state)			\
	do {									\
		static struct lock_class_key lock##_key;			\
		lockdep_init_map(&owner->btrfs_state_change_map[state], #lock,	\
				 &lock##_key, 0);				\
	} while (0)
static_assert(BTRFS_NESTING_MAX <= MAX_LOCKDEP_SUBCLASSES,
	      "too many lock subclasses defined");
struct btrfs_path;
void __btrfs_tree_lock(struct extent_buffer *eb, enum btrfs_lock_nesting nest);
void btrfs_tree_lock(struct extent_buffer *eb);
void btrfs_tree_unlock(struct extent_buffer *eb);
void __btrfs_tree_read_lock(struct extent_buffer *eb, enum btrfs_lock_nesting nest);
void btrfs_tree_read_lock(struct extent_buffer *eb);
void btrfs_tree_read_unlock(struct extent_buffer *eb);
int btrfs_try_tree_read_lock(struct extent_buffer *eb);
int btrfs_try_tree_write_lock(struct extent_buffer *eb);
struct extent_buffer *btrfs_lock_root_node(struct btrfs_root *root);
struct extent_buffer *btrfs_read_lock_root_node(struct btrfs_root *root);
struct extent_buffer *btrfs_try_read_lock_root_node(struct btrfs_root *root);
#ifdef CONFIG_BTRFS_DEBUG
static inline void btrfs_assert_tree_write_locked(struct extent_buffer *eb)
{
	lockdep_assert_held_write(&eb->lock);
}
#else
static inline void btrfs_assert_tree_write_locked(struct extent_buffer *eb) { }
#endif
void btrfs_unlock_up_safe(struct btrfs_path *path, int level);
static inline void btrfs_tree_unlock_rw(struct extent_buffer *eb, int rw)
{
	if (rw == BTRFS_WRITE_LOCK)
		btrfs_tree_unlock(eb);
	else if (rw == BTRFS_READ_LOCK)
		btrfs_tree_read_unlock(eb);
	else
		BUG();
}
struct btrfs_drew_lock {
	atomic_t readers;
	atomic_t writers;
	wait_queue_head_t pending_writers;
	wait_queue_head_t pending_readers;
};
void btrfs_drew_lock_init(struct btrfs_drew_lock *lock);
void btrfs_drew_write_lock(struct btrfs_drew_lock *lock);
bool btrfs_drew_try_write_lock(struct btrfs_drew_lock *lock);
void btrfs_drew_write_unlock(struct btrfs_drew_lock *lock);
void btrfs_drew_read_lock(struct btrfs_drew_lock *lock);
void btrfs_drew_read_unlock(struct btrfs_drew_lock *lock);
#ifdef CONFIG_DEBUG_LOCK_ALLOC
void btrfs_set_buffer_lockdep_class(u64 objectid, struct extent_buffer *eb, int level);
void btrfs_maybe_reset_lockdep_class(struct btrfs_root *root, struct extent_buffer *eb);
#else
static inline void btrfs_set_buffer_lockdep_class(u64 objectid,
					struct extent_buffer *eb, int level)
{
}
static inline void btrfs_maybe_reset_lockdep_class(struct btrfs_root *root,
						   struct extent_buffer *eb)
{
}
#endif
#endif
