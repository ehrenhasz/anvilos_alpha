
#include <linux/interval_tree.h>
#include <linux/interval_tree_generic.h>
#include <linux/compiler.h>
#include <linux/export.h>

#define START(node) ((node)->start)
#define LAST(node)  ((node)->last)

INTERVAL_TREE_DEFINE(struct interval_tree_node, rb,
		     unsigned long, __subtree_last,
		     START, LAST,, interval_tree)

EXPORT_SYMBOL_GPL(interval_tree_insert);
EXPORT_SYMBOL_GPL(interval_tree_remove);
EXPORT_SYMBOL_GPL(interval_tree_iter_first);
EXPORT_SYMBOL_GPL(interval_tree_iter_next);

#ifdef CONFIG_INTERVAL_TREE_SPAN_ITER
 
static void
interval_tree_span_iter_next_gap(struct interval_tree_span_iter *state)
{
	struct interval_tree_node *cur = state->nodes[1];

	state->nodes[0] = cur;
	do {
		if (cur->last > state->nodes[0]->last)
			state->nodes[0] = cur;
		cur = interval_tree_iter_next(cur, state->first_index,
					      state->last_index);
	} while (cur && (state->nodes[0]->last >= cur->start ||
			 state->nodes[0]->last + 1 == cur->start));
	state->nodes[1] = cur;
}

void interval_tree_span_iter_first(struct interval_tree_span_iter *iter,
				   struct rb_root_cached *itree,
				   unsigned long first_index,
				   unsigned long last_index)
{
	iter->first_index = first_index;
	iter->last_index = last_index;
	iter->nodes[0] = NULL;
	iter->nodes[1] =
		interval_tree_iter_first(itree, first_index, last_index);
	if (!iter->nodes[1]) {
		 
		iter->start_hole = first_index;
		iter->last_hole = last_index;
		iter->is_hole = 1;
		return;
	}
	if (iter->nodes[1]->start > first_index) {
		 
		iter->start_hole = first_index;
		iter->last_hole = iter->nodes[1]->start - 1;
		iter->is_hole = 1;
		interval_tree_span_iter_next_gap(iter);
		return;
	}

	 
	iter->start_used = first_index;
	iter->is_hole = 0;
	interval_tree_span_iter_next_gap(iter);
	iter->last_used = iter->nodes[0]->last;
	if (iter->last_used >= last_index) {
		iter->last_used = last_index;
		iter->nodes[0] = NULL;
		iter->nodes[1] = NULL;
	}
}
EXPORT_SYMBOL_GPL(interval_tree_span_iter_first);

void interval_tree_span_iter_next(struct interval_tree_span_iter *iter)
{
	if (!iter->nodes[0] && !iter->nodes[1]) {
		iter->is_hole = -1;
		return;
	}

	if (iter->is_hole) {
		iter->start_used = iter->last_hole + 1;
		iter->last_used = iter->nodes[0]->last;
		if (iter->last_used >= iter->last_index) {
			iter->last_used = iter->last_index;
			iter->nodes[0] = NULL;
			iter->nodes[1] = NULL;
		}
		iter->is_hole = 0;
		return;
	}

	if (!iter->nodes[1]) {
		 
		iter->start_hole = iter->nodes[0]->last + 1;
		iter->last_hole = iter->last_index;
		iter->nodes[0] = NULL;
		iter->is_hole = 1;
		return;
	}

	 
	iter->start_hole = iter->nodes[0]->last + 1;
	iter->last_hole = iter->nodes[1]->start - 1;
	iter->is_hole = 1;
	interval_tree_span_iter_next_gap(iter);
}
EXPORT_SYMBOL_GPL(interval_tree_span_iter_next);

 
void interval_tree_span_iter_advance(struct interval_tree_span_iter *iter,
				     struct rb_root_cached *itree,
				     unsigned long new_index)
{
	if (iter->is_hole == -1)
		return;

	iter->first_index = new_index;
	if (new_index > iter->last_index) {
		iter->is_hole = -1;
		return;
	}

	 
	if (iter->start_hole <= new_index && new_index <= iter->last_hole) {
		iter->start_hole = new_index;
		return;
	}
	if (new_index == iter->last_hole + 1)
		interval_tree_span_iter_next(iter);
	else
		interval_tree_span_iter_first(iter, itree, new_index,
					      iter->last_index);
}
EXPORT_SYMBOL_GPL(interval_tree_span_iter_advance);
#endif
