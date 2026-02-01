
 
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/llist.h>


 
bool llist_add_batch(struct llist_node *new_first, struct llist_node *new_last,
		     struct llist_head *head)
{
	struct llist_node *first = READ_ONCE(head->first);

	do {
		new_last->next = first;
	} while (!try_cmpxchg(&head->first, &first, new_first));

	return !first;
}
EXPORT_SYMBOL_GPL(llist_add_batch);

 
struct llist_node *llist_del_first(struct llist_head *head)
{
	struct llist_node *entry, *next;

	entry = smp_load_acquire(&head->first);
	do {
		if (entry == NULL)
			return NULL;
		next = READ_ONCE(entry->next);
	} while (!try_cmpxchg(&head->first, &entry, next));

	return entry;
}
EXPORT_SYMBOL_GPL(llist_del_first);

 
struct llist_node *llist_reverse_order(struct llist_node *head)
{
	struct llist_node *new_head = NULL;

	while (head) {
		struct llist_node *tmp = head;
		head = head->next;
		tmp->next = new_head;
		new_head = tmp;
	}

	return new_head;
}
EXPORT_SYMBOL_GPL(llist_reverse_order);
