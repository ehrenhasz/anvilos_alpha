 

#ifndef BTRFS_MISC_H
#define BTRFS_MISC_H

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/math64.h>
#include <linux/rbtree.h>

 
#define ENUM_BIT(name)                                  \
	__ ## name ## _BIT,                             \
	name = (1U << __ ## name ## _BIT),              \
	__ ## name ## _SEQ = __ ## name ## _BIT

static inline void cond_wake_up(struct wait_queue_head *wq)
{
	 
	if (wq_has_sleeper(wq))
		wake_up(wq);
}

static inline void cond_wake_up_nomb(struct wait_queue_head *wq)
{
	 
	if (waitqueue_active(wq))
		wake_up(wq);
}

static inline u64 mult_perc(u64 num, u32 percent)
{
	return div_u64(num * percent, 100);
}
 
static inline bool is_power_of_two_u64(u64 n)
{
	return n != 0 && (n & (n - 1)) == 0;
}

static inline bool has_single_bit_set(u64 n)
{
	return is_power_of_two_u64(n);
}

 
struct rb_simple_node {
	struct rb_node rb_node;
	u64 bytenr;
};

static inline struct rb_node *rb_simple_search(struct rb_root *root, u64 bytenr)
{
	struct rb_node *node = root->rb_node;
	struct rb_simple_node *entry;

	while (node) {
		entry = rb_entry(node, struct rb_simple_node, rb_node);

		if (bytenr < entry->bytenr)
			node = node->rb_left;
		else if (bytenr > entry->bytenr)
			node = node->rb_right;
		else
			return node;
	}
	return NULL;
}

 
static inline struct rb_node *rb_simple_search_first(struct rb_root *root,
						     u64 bytenr)
{
	struct rb_node *node = root->rb_node, *ret = NULL;
	struct rb_simple_node *entry, *ret_entry = NULL;

	while (node) {
		entry = rb_entry(node, struct rb_simple_node, rb_node);

		if (bytenr < entry->bytenr) {
			if (!ret || entry->bytenr < ret_entry->bytenr) {
				ret = node;
				ret_entry = entry;
			}

			node = node->rb_left;
		} else if (bytenr > entry->bytenr) {
			node = node->rb_right;
		} else {
			return node;
		}
	}

	return ret;
}

static inline struct rb_node *rb_simple_insert(struct rb_root *root, u64 bytenr,
					       struct rb_node *node)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct rb_simple_node *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct rb_simple_node, rb_node);

		if (bytenr < entry->bytenr)
			p = &(*p)->rb_left;
		else if (bytenr > entry->bytenr)
			p = &(*p)->rb_right;
		else
			return parent;
	}

	rb_link_node(node, parent, p);
	rb_insert_color(node, root);
	return NULL;
}

static inline bool bitmap_test_range_all_set(const unsigned long *addr,
					     unsigned long start,
					     unsigned long nbits)
{
	unsigned long found_zero;

	found_zero = find_next_zero_bit(addr, start + nbits, start);
	return (found_zero == start + nbits);
}

static inline bool bitmap_test_range_all_zero(const unsigned long *addr,
					      unsigned long start,
					      unsigned long nbits)
{
	unsigned long found_set;

	found_set = find_next_bit(addr, start + nbits, start);
	return (found_set == start + nbits);
}

#endif
