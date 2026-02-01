 

#ifndef USNIC_UIOM_INTERVAL_TREE_H_
#define USNIC_UIOM_INTERVAL_TREE_H_

#include <linux/rbtree.h>

struct usnic_uiom_interval_node {
	struct rb_node			rb;
	struct list_head		link;
	unsigned long			start;
	unsigned long			last;
	unsigned long			__subtree_last;
	unsigned int			ref_cnt;
	int				flags;
};

extern void
usnic_uiom_interval_tree_insert(struct usnic_uiom_interval_node *node,
					struct rb_root_cached *root);
extern void
usnic_uiom_interval_tree_remove(struct usnic_uiom_interval_node *node,
					struct rb_root_cached *root);
extern struct usnic_uiom_interval_node *
usnic_uiom_interval_tree_iter_first(struct rb_root_cached *root,
					unsigned long start,
					unsigned long last);
extern struct usnic_uiom_interval_node *
usnic_uiom_interval_tree_iter_next(struct usnic_uiom_interval_node *node,
			unsigned long start, unsigned long last);
 
int usnic_uiom_insert_interval(struct rb_root_cached *root,
				unsigned long start, unsigned long last,
				int flags);
 
void usnic_uiom_remove_interval(struct rb_root_cached *root,
				unsigned long start, unsigned long last,
				struct list_head *removed);
 
int usnic_uiom_get_intervals_diff(unsigned long start,
					unsigned long last, int flags,
					int flag_mask,
					struct rb_root_cached *root,
					struct list_head *diff_set);
 
void usnic_uiom_put_interval_set(struct list_head *intervals);
#endif  
