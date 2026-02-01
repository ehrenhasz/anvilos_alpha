
 

#include <linux/bug.h>
#include <linux/timerqueue.h>
#include <linux/rbtree.h>
#include <linux/export.h>

#define __node_2_tq(_n) \
	rb_entry((_n), struct timerqueue_node, node)

static inline bool __timerqueue_less(struct rb_node *a, const struct rb_node *b)
{
	return __node_2_tq(a)->expires < __node_2_tq(b)->expires;
}

 
bool timerqueue_add(struct timerqueue_head *head, struct timerqueue_node *node)
{
	 
	WARN_ON_ONCE(!RB_EMPTY_NODE(&node->node));

	return rb_add_cached(&node->node, &head->rb_root, __timerqueue_less);
}
EXPORT_SYMBOL_GPL(timerqueue_add);

 
bool timerqueue_del(struct timerqueue_head *head, struct timerqueue_node *node)
{
	WARN_ON_ONCE(RB_EMPTY_NODE(&node->node));

	rb_erase_cached(&node->node, &head->rb_root);
	RB_CLEAR_NODE(&node->node);

	return !RB_EMPTY_ROOT(&head->rb_root.rb_root);
}
EXPORT_SYMBOL_GPL(timerqueue_del);

 
struct timerqueue_node *timerqueue_iterate_next(struct timerqueue_node *node)
{
	struct rb_node *next;

	if (!node)
		return NULL;
	next = rb_next(&node->node);
	if (!next)
		return NULL;
	return container_of(next, struct timerqueue_node, node);
}
EXPORT_SYMBOL_GPL(timerqueue_iterate_next);
