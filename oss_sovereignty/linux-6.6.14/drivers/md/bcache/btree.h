#ifndef _BCACHE_BTREE_H
#define _BCACHE_BTREE_H
#include "bset.h"
#include "debug.h"
struct btree_write {
	atomic_t		*journal;
	int			prio_blocked;
};
struct btree {
	struct hlist_node	hash;
	BKEY_PADDED(key);
	unsigned long		seq;
	struct rw_semaphore	lock;
	struct cache_set	*c;
	struct btree		*parent;
	struct mutex		write_lock;
	unsigned long		flags;
	uint16_t		written;	 
	uint8_t			level;
	struct btree_keys	keys;
	struct closure		io;
	struct semaphore	io_mutex;
	struct list_head	list;
	struct delayed_work	work;
	struct btree_write	writes[2];
	struct bio		*bio;
};
#define BTREE_FLAG(flag)						\
static inline bool btree_node_ ## flag(struct btree *b)			\
{	return test_bit(BTREE_NODE_ ## flag, &b->flags); }		\
									\
static inline void set_btree_node_ ## flag(struct btree *b)		\
{	set_bit(BTREE_NODE_ ## flag, &b->flags); }
enum btree_flags {
	BTREE_NODE_io_error,
	BTREE_NODE_dirty,
	BTREE_NODE_write_idx,
	BTREE_NODE_journal_flush,
};
BTREE_FLAG(io_error);
BTREE_FLAG(dirty);
BTREE_FLAG(write_idx);
BTREE_FLAG(journal_flush);
static inline struct btree_write *btree_current_write(struct btree *b)
{
	return b->writes + btree_node_write_idx(b);
}
static inline struct btree_write *btree_prev_write(struct btree *b)
{
	return b->writes + (btree_node_write_idx(b) ^ 1);
}
static inline struct bset *btree_bset_first(struct btree *b)
{
	return b->keys.set->data;
}
static inline struct bset *btree_bset_last(struct btree *b)
{
	return bset_tree_last(&b->keys)->data;
}
static inline unsigned int bset_block_offset(struct btree *b, struct bset *i)
{
	return bset_sector_offset(&b->keys, i) >> b->c->block_bits;
}
static inline void set_gc_sectors(struct cache_set *c)
{
	atomic_set(&c->sectors_to_gc, c->cache->sb.bucket_size * c->nbuckets / 16);
}
void bkey_put(struct cache_set *c, struct bkey *k);
#define for_each_cached_btree(b, c, iter)				\
	for (iter = 0;							\
	     iter < ARRAY_SIZE((c)->bucket_hash);			\
	     iter++)							\
		hlist_for_each_entry_rcu((b), (c)->bucket_hash + iter, hash)
struct btree_op {
	wait_queue_entry_t		wait;
	short			lock;
	unsigned int		insert_collision:1;
};
struct btree_check_state;
struct btree_check_info {
	struct btree_check_state	*state;
	struct task_struct		*thread;
	int				result;
};
#define BCH_BTR_CHKTHREAD_MAX	12
struct btree_check_state {
	struct cache_set		*c;
	int				total_threads;
	int				key_idx;
	spinlock_t			idx_lock;
	atomic_t			started;
	atomic_t			enough;
	wait_queue_head_t		wait;
	struct btree_check_info		infos[BCH_BTR_CHKTHREAD_MAX];
};
static inline void bch_btree_op_init(struct btree_op *op, int write_lock_level)
{
	memset(op, 0, sizeof(struct btree_op));
	init_wait(&op->wait);
	op->lock = write_lock_level;
}
static inline void rw_lock(bool w, struct btree *b, int level)
{
	w ? down_write(&b->lock)
	  : down_read(&b->lock);
	if (w)
		b->seq++;
}
static inline void rw_unlock(bool w, struct btree *b)
{
	if (w)
		b->seq++;
	(w ? up_write : up_read)(&b->lock);
}
void bch_btree_node_read_done(struct btree *b);
void __bch_btree_node_write(struct btree *b, struct closure *parent);
void bch_btree_node_write(struct btree *b, struct closure *parent);
void bch_btree_set_root(struct btree *b);
struct btree *__bch_btree_node_alloc(struct cache_set *c, struct btree_op *op,
				     int level, bool wait,
				     struct btree *parent);
struct btree *bch_btree_node_get(struct cache_set *c, struct btree_op *op,
				 struct bkey *k, int level, bool write,
				 struct btree *parent);
int bch_btree_insert_check_key(struct btree *b, struct btree_op *op,
			       struct bkey *check_key);
int bch_btree_insert(struct cache_set *c, struct keylist *keys,
		     atomic_t *journal_ref, struct bkey *replace_key);
int bch_gc_thread_start(struct cache_set *c);
void bch_initial_gc_finish(struct cache_set *c);
void bch_moving_gc(struct cache_set *c);
int bch_btree_check(struct cache_set *c);
void bch_initial_mark_key(struct cache_set *c, int level, struct bkey *k);
void bch_cannibalize_unlock(struct cache_set *c);
static inline void wake_up_gc(struct cache_set *c)
{
	wake_up(&c->gc_wait);
}
static inline void force_wake_up_gc(struct cache_set *c)
{
	atomic_set(&c->sectors_to_gc, -1);
	wake_up_gc(c);
}
#define bcache_btree(fn, key, b, op, ...)				\
({									\
	int _r, l = (b)->level - 1;					\
	bool _w = l <= (op)->lock;					\
	struct btree *_child = bch_btree_node_get((b)->c, op, key, l,	\
						  _w, b);		\
	if (!IS_ERR(_child)) {						\
		_r = bch_btree_ ## fn(_child, op, ##__VA_ARGS__);	\
		rw_unlock(_w, _child);					\
	} else								\
		_r = PTR_ERR(_child);					\
	_r;								\
})
#define bcache_btree_root(fn, c, op, ...)				\
({									\
	int _r = -EINTR;						\
	do {								\
		struct btree *_b = (c)->root;				\
		bool _w = insert_lock(op, _b);				\
		rw_lock(_w, _b, _b->level);				\
		if (_b == (c)->root &&					\
		    _w == insert_lock(op, _b)) {			\
			_r = bch_btree_ ## fn(_b, op, ##__VA_ARGS__);	\
		}							\
		rw_unlock(_w, _b);					\
		bch_cannibalize_unlock(c);                              \
		if (_r == -EINTR)                                       \
			schedule();                                     \
	} while (_r == -EINTR);                                         \
									\
	finish_wait(&(c)->btree_cache_wait, &(op)->wait);               \
	_r;                                                             \
})
#define MAP_DONE	0
#define MAP_CONTINUE	1
#define MAP_ALL_NODES	0
#define MAP_LEAF_NODES	1
#define MAP_END_KEY	1
typedef int (btree_map_nodes_fn)(struct btree_op *b_op, struct btree *b);
int __bch_btree_map_nodes(struct btree_op *op, struct cache_set *c,
			  struct bkey *from, btree_map_nodes_fn *fn, int flags);
static inline int bch_btree_map_nodes(struct btree_op *op, struct cache_set *c,
				      struct bkey *from, btree_map_nodes_fn *fn)
{
	return __bch_btree_map_nodes(op, c, from, fn, MAP_ALL_NODES);
}
static inline int bch_btree_map_leaf_nodes(struct btree_op *op,
					   struct cache_set *c,
					   struct bkey *from,
					   btree_map_nodes_fn *fn)
{
	return __bch_btree_map_nodes(op, c, from, fn, MAP_LEAF_NODES);
}
typedef int (btree_map_keys_fn)(struct btree_op *op, struct btree *b,
				struct bkey *k);
int bch_btree_map_keys(struct btree_op *op, struct cache_set *c,
		       struct bkey *from, btree_map_keys_fn *fn, int flags);
int bch_btree_map_keys_recurse(struct btree *b, struct btree_op *op,
			       struct bkey *from, btree_map_keys_fn *fn,
			       int flags);
typedef bool (keybuf_pred_fn)(struct keybuf *buf, struct bkey *k);
void bch_keybuf_init(struct keybuf *buf);
void bch_refill_keybuf(struct cache_set *c, struct keybuf *buf,
		       struct bkey *end, keybuf_pred_fn *pred);
bool bch_keybuf_check_overlapping(struct keybuf *buf, struct bkey *start,
				  struct bkey *end);
void bch_keybuf_del(struct keybuf *buf, struct keybuf_key *w);
struct keybuf_key *bch_keybuf_next(struct keybuf *buf);
struct keybuf_key *bch_keybuf_next_rescan(struct cache_set *c,
					  struct keybuf *buf,
					  struct bkey *end,
					  keybuf_pred_fn *pred);
void bch_update_bucket_in_use(struct cache_set *c, struct gc_stat *stats);
#endif
