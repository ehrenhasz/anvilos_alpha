#ifndef _BCACHE_BSET_H
#define _BCACHE_BSET_H
#include <linux/kernel.h>
#include <linux/types.h>
#include "bcache_ondisk.h"
#include "util.h"  
struct btree_keys;
struct btree_iter;
struct btree_iter_set;
struct bkey_float;
#define MAX_BSETS		4U
struct bset_tree {
	unsigned int		size;
	unsigned int		extra;
	struct bkey		end;
	struct bkey_float	*tree;
	uint8_t			*prev;
	struct bset		*data;
};
struct btree_keys_ops {
	bool		(*sort_cmp)(struct btree_iter_set l,
				    struct btree_iter_set r);
	struct bkey	*(*sort_fixup)(struct btree_iter *iter,
				       struct bkey *tmp);
	bool		(*insert_fixup)(struct btree_keys *b,
					struct bkey *insert,
					struct btree_iter *iter,
					struct bkey *replace_key);
	bool		(*key_invalid)(struct btree_keys *bk,
				       const struct bkey *k);
	bool		(*key_bad)(struct btree_keys *bk,
				   const struct bkey *k);
	bool		(*key_merge)(struct btree_keys *bk,
				     struct bkey *l, struct bkey *r);
	void		(*key_to_text)(char *buf,
				       size_t size,
				       const struct bkey *k);
	void		(*key_dump)(struct btree_keys *keys,
				    const struct bkey *k);
	bool		is_extents;
};
struct btree_keys {
	const struct btree_keys_ops	*ops;
	uint8_t			page_order;
	uint8_t			nsets;
	unsigned int		last_set_unwritten:1;
	bool			*expensive_debug_checks;
	struct bset_tree	set[MAX_BSETS];
};
static inline struct bset_tree *bset_tree_last(struct btree_keys *b)
{
	return b->set + b->nsets;
}
static inline bool bset_written(struct btree_keys *b, struct bset_tree *t)
{
	return t <= b->set + b->nsets - b->last_set_unwritten;
}
static inline bool bkey_written(struct btree_keys *b, struct bkey *k)
{
	return !b->last_set_unwritten || k < b->set[b->nsets].data->start;
}
static inline unsigned int bset_byte_offset(struct btree_keys *b,
					    struct bset *i)
{
	return ((size_t) i) - ((size_t) b->set->data);
}
static inline unsigned int bset_sector_offset(struct btree_keys *b,
					      struct bset *i)
{
	return bset_byte_offset(b, i) >> 9;
}
#define __set_bytes(i, k)	(sizeof(*(i)) + (k) * sizeof(uint64_t))
#define set_bytes(i)		__set_bytes(i, i->keys)
#define __set_blocks(i, k, block_bytes)				\
	DIV_ROUND_UP(__set_bytes(i, k), block_bytes)
#define set_blocks(i, block_bytes)				\
	__set_blocks(i, (i)->keys, block_bytes)
static inline size_t bch_btree_keys_u64s_remaining(struct btree_keys *b)
{
	struct bset_tree *t = bset_tree_last(b);
	BUG_ON((PAGE_SIZE << b->page_order) <
	       (bset_byte_offset(b, t->data) + set_bytes(t->data)));
	if (!b->last_set_unwritten)
		return 0;
	return ((PAGE_SIZE << b->page_order) -
		(bset_byte_offset(b, t->data) + set_bytes(t->data))) /
		sizeof(u64);
}
static inline struct bset *bset_next_set(struct btree_keys *b,
					 unsigned int block_bytes)
{
	struct bset *i = bset_tree_last(b)->data;
	return ((void *) i) + roundup(set_bytes(i), block_bytes);
}
void bch_btree_keys_free(struct btree_keys *b);
int bch_btree_keys_alloc(struct btree_keys *b, unsigned int page_order,
			 gfp_t gfp);
void bch_btree_keys_init(struct btree_keys *b, const struct btree_keys_ops *ops,
			 bool *expensive_debug_checks);
void bch_bset_init_next(struct btree_keys *b, struct bset *i, uint64_t magic);
void bch_bset_build_written_tree(struct btree_keys *b);
void bch_bset_fix_invalidated_key(struct btree_keys *b, struct bkey *k);
bool bch_bkey_try_merge(struct btree_keys *b, struct bkey *l, struct bkey *r);
void bch_bset_insert(struct btree_keys *b, struct bkey *where,
		     struct bkey *insert);
unsigned int bch_btree_insert_key(struct btree_keys *b, struct bkey *k,
			      struct bkey *replace_key);
enum {
	BTREE_INSERT_STATUS_NO_INSERT = 0,
	BTREE_INSERT_STATUS_INSERT,
	BTREE_INSERT_STATUS_BACK_MERGE,
	BTREE_INSERT_STATUS_OVERWROTE,
	BTREE_INSERT_STATUS_FRONT_MERGE,
};
struct btree_iter {
	size_t size, used;
#ifdef CONFIG_BCACHE_DEBUG
	struct btree_keys *b;
#endif
	struct btree_iter_set {
		struct bkey *k, *end;
	} data[MAX_BSETS];
};
typedef bool (*ptr_filter_fn)(struct btree_keys *b, const struct bkey *k);
struct bkey *bch_btree_iter_next(struct btree_iter *iter);
struct bkey *bch_btree_iter_next_filter(struct btree_iter *iter,
					struct btree_keys *b,
					ptr_filter_fn fn);
void bch_btree_iter_push(struct btree_iter *iter, struct bkey *k,
			 struct bkey *end);
struct bkey *bch_btree_iter_init(struct btree_keys *b,
				 struct btree_iter *iter,
				 struct bkey *search);
struct bkey *__bch_bset_search(struct btree_keys *b, struct bset_tree *t,
			       const struct bkey *search);
static inline struct bkey *bch_bset_search(struct btree_keys *b,
					   struct bset_tree *t,
					   const struct bkey *search)
{
	return search ? __bch_bset_search(b, t, search) : t->data->start;
}
#define for_each_key_filter(b, k, iter, filter)				\
	for (bch_btree_iter_init((b), (iter), NULL);			\
	     ((k) = bch_btree_iter_next_filter((iter), (b), filter));)
#define for_each_key(b, k, iter)					\
	for (bch_btree_iter_init((b), (iter), NULL);			\
	     ((k) = bch_btree_iter_next(iter));)
struct bset_sort_state {
	mempool_t		pool;
	unsigned int		page_order;
	unsigned int		crit_factor;
	struct time_stats	time;
};
void bch_bset_sort_state_free(struct bset_sort_state *state);
int bch_bset_sort_state_init(struct bset_sort_state *state,
			     unsigned int page_order);
void bch_btree_sort_lazy(struct btree_keys *b, struct bset_sort_state *state);
void bch_btree_sort_into(struct btree_keys *b, struct btree_keys *new,
			 struct bset_sort_state *state);
void bch_btree_sort_and_fix_extents(struct btree_keys *b,
				    struct btree_iter *iter,
				    struct bset_sort_state *state);
void bch_btree_sort_partial(struct btree_keys *b, unsigned int start,
			    struct bset_sort_state *state);
static inline void bch_btree_sort(struct btree_keys *b,
				  struct bset_sort_state *state)
{
	bch_btree_sort_partial(b, 0, state);
}
struct bset_stats {
	size_t sets_written, sets_unwritten;
	size_t bytes_written, bytes_unwritten;
	size_t floats, failed;
};
void bch_btree_keys_stats(struct btree_keys *b, struct bset_stats *state);
#define bset_bkey_last(i)	bkey_idx((struct bkey *) (i)->d, \
					 (unsigned int)(i)->keys)
static inline struct bkey *bset_bkey_idx(struct bset *i, unsigned int idx)
{
	return bkey_idx(i->start, idx);
}
static inline void bkey_init(struct bkey *k)
{
	*k = ZERO_KEY;
}
static __always_inline int64_t bkey_cmp(const struct bkey *l,
					const struct bkey *r)
{
	return unlikely(KEY_INODE(l) != KEY_INODE(r))
		? (int64_t) KEY_INODE(l) - (int64_t) KEY_INODE(r)
		: (int64_t) KEY_OFFSET(l) - (int64_t) KEY_OFFSET(r);
}
void bch_bkey_copy_single_ptr(struct bkey *dest, const struct bkey *src,
			      unsigned int i);
bool __bch_cut_front(const struct bkey *where, struct bkey *k);
bool __bch_cut_back(const struct bkey *where, struct bkey *k);
static inline bool bch_cut_front(const struct bkey *where, struct bkey *k)
{
	BUG_ON(bkey_cmp(where, k) > 0);
	return __bch_cut_front(where, k);
}
static inline bool bch_cut_back(const struct bkey *where, struct bkey *k)
{
	BUG_ON(bkey_cmp(where, &START_KEY(k)) < 0);
	return __bch_cut_back(where, k);
}
static inline void preceding_key(struct bkey *k, struct bkey **preceding_key_p)
{
	if (KEY_INODE(k) || KEY_OFFSET(k)) {
		(**preceding_key_p) = KEY(KEY_INODE(k), KEY_OFFSET(k), 0);
		if (!(*preceding_key_p)->low)
			(*preceding_key_p)->high--;
		(*preceding_key_p)->low--;
	} else {
		(*preceding_key_p) = NULL;
	}
}
static inline bool bch_ptr_invalid(struct btree_keys *b, const struct bkey *k)
{
	return b->ops->key_invalid(b, k);
}
static inline bool bch_ptr_bad(struct btree_keys *b, const struct bkey *k)
{
	return b->ops->key_bad(b, k);
}
static inline void bch_bkey_to_text(struct btree_keys *b, char *buf,
				    size_t size, const struct bkey *k)
{
	return b->ops->key_to_text(buf, size, k);
}
static inline bool bch_bkey_equal_header(const struct bkey *l,
					 const struct bkey *r)
{
	return (KEY_DIRTY(l) == KEY_DIRTY(r) &&
		KEY_PTRS(l) == KEY_PTRS(r) &&
		KEY_CSUM(l) == KEY_CSUM(r));
}
struct keylist {
	union {
		struct bkey		*keys;
		uint64_t		*keys_p;
	};
	union {
		struct bkey		*top;
		uint64_t		*top_p;
	};
#define KEYLIST_INLINE		16
	uint64_t		inline_keys[KEYLIST_INLINE];
};
static inline void bch_keylist_init(struct keylist *l)
{
	l->top_p = l->keys_p = l->inline_keys;
}
static inline void bch_keylist_init_single(struct keylist *l, struct bkey *k)
{
	l->keys = k;
	l->top = bkey_next(k);
}
static inline void bch_keylist_push(struct keylist *l)
{
	l->top = bkey_next(l->top);
}
static inline void bch_keylist_add(struct keylist *l, struct bkey *k)
{
	bkey_copy(l->top, k);
	bch_keylist_push(l);
}
static inline bool bch_keylist_empty(struct keylist *l)
{
	return l->top == l->keys;
}
static inline void bch_keylist_reset(struct keylist *l)
{
	l->top = l->keys;
}
static inline void bch_keylist_free(struct keylist *l)
{
	if (l->keys_p != l->inline_keys)
		kfree(l->keys_p);
}
static inline size_t bch_keylist_nkeys(struct keylist *l)
{
	return l->top_p - l->keys_p;
}
static inline size_t bch_keylist_bytes(struct keylist *l)
{
	return bch_keylist_nkeys(l) * sizeof(uint64_t);
}
struct bkey *bch_keylist_pop(struct keylist *l);
void bch_keylist_pop_front(struct keylist *l);
int __bch_keylist_realloc(struct keylist *l, unsigned int u64s);
#ifdef CONFIG_BCACHE_DEBUG
int __bch_count_data(struct btree_keys *b);
void __printf(2, 3) __bch_check_keys(struct btree_keys *b,
				     const char *fmt,
				     ...);
void bch_dump_bset(struct btree_keys *b, struct bset *i, unsigned int set);
void bch_dump_bucket(struct btree_keys *b);
#else
static inline int __bch_count_data(struct btree_keys *b) { return -1; }
static inline void __printf(2, 3)
	__bch_check_keys(struct btree_keys *b, const char *fmt, ...) {}
static inline void bch_dump_bucket(struct btree_keys *b) {}
void bch_dump_bset(struct btree_keys *b, struct bset *i, unsigned int set);
#endif
static inline bool btree_keys_expensive_checks(struct btree_keys *b)
{
#ifdef CONFIG_BCACHE_DEBUG
	return *b->expensive_debug_checks;
#else
	return false;
#endif
}
static inline int bch_count_data(struct btree_keys *b)
{
	return btree_keys_expensive_checks(b) ? __bch_count_data(b) : -1;
}
#define bch_check_keys(b, ...)						\
do {									\
	if (btree_keys_expensive_checks(b))				\
		__bch_check_keys(b, __VA_ARGS__);			\
} while (0)
#endif
