
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/list_sort.h>
#include <linux/list.h>

 
__attribute__((nonnull(2,3,4)))
static struct list_head *merge(void *priv, list_cmp_func_t cmp,
				struct list_head *a, struct list_head *b)
{
	struct list_head *head, **tail = &head;

	for (;;) {
		 
		if (cmp(priv, a, b) <= 0) {
			*tail = a;
			tail = &a->next;
			a = a->next;
			if (!a) {
				*tail = b;
				break;
			}
		} else {
			*tail = b;
			tail = &b->next;
			b = b->next;
			if (!b) {
				*tail = a;
				break;
			}
		}
	}
	return head;
}

 
__attribute__((nonnull(2,3,4,5)))
static void merge_final(void *priv, list_cmp_func_t cmp, struct list_head *head,
			struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head;
	u8 count = 0;

	for (;;) {
		 
		if (cmp(priv, a, b) <= 0) {
			tail->next = a;
			a->prev = tail;
			tail = a;
			a = a->next;
			if (!a)
				break;
		} else {
			tail->next = b;
			b->prev = tail;
			tail = b;
			b = b->next;
			if (!b) {
				b = a;
				break;
			}
		}
	}

	 
	tail->next = b;
	do {
		 
		if (unlikely(!++count))
			cmp(priv, b, b);
		b->prev = tail;
		tail = b;
		b = b->next;
	} while (b);

	 
	tail->next = head;
	head->prev = tail;
}

 
__attribute__((nonnull(2,3)))
void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
	struct list_head *list = head->next, *pending = NULL;
	size_t count = 0;	 

	if (list == head->prev)	 
		return;

	 
	head->prev->next = NULL;

	 
	do {
		size_t bits;
		struct list_head **tail = &pending;

		 
		for (bits = count; bits & 1; bits >>= 1)
			tail = &(*tail)->prev;
		 
		if (likely(bits)) {
			struct list_head *a = *tail, *b = a->prev;

			a = merge(priv, cmp, b, a);
			 
			a->prev = b->prev;
			*tail = a;
		}

		 
		list->prev = pending;
		pending = list;
		list = list->next;
		pending->next = NULL;
		count++;
	} while (list);

	 
	list = pending;
	pending = pending->prev;
	for (;;) {
		struct list_head *next = pending->prev;

		if (!next)
			break;
		list = merge(priv, cmp, pending, list);
		pending = next;
	}
	 
	merge_final(priv, cmp, head, pending, list);
}
EXPORT_SYMBOL(list_sort);
