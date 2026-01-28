

#ifndef UTIL_LINUX_LIST_H
#define UTIL_LINUX_LIST_H

#include "c.h"


#ifdef __GNUC__
#define _INLINE_ static __inline__
#else                         
#define _INLINE_ static inline
#endif



struct list_head {
	struct list_head *next, *prev;
};

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)


_INLINE_ void __list_add(struct list_head * add,
	struct list_head * prev,
	struct list_head * next)
{
	next->prev = add;
	add->next = next;
	add->prev = prev;
	prev->next = add;
}


_INLINE_ void list_add(struct list_head *add, struct list_head *head)
{
	__list_add(add, head, head->next);
}


_INLINE_ void list_add_tail(struct list_head *add, struct list_head *head)
{
	__list_add(add, head->prev, head);
}


_INLINE_ void __list_del(struct list_head * prev,
				  struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}


_INLINE_ void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}


_INLINE_ void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}


_INLINE_ int list_empty(struct list_head *head)
{
	return head->next == head;
}


_INLINE_ int list_entry_is_last(struct list_head *entry, struct list_head *head)
{
	return head->prev == entry;
}


_INLINE_ int list_entry_is_first(struct list_head *entry, struct list_head *head)
{
	return head->next == entry;
}


_INLINE_ void list_splice(struct list_head *list, struct list_head *head)
{
	struct list_head *first = list->next;

	if (first != list) {
		struct list_head *last = list->prev;
		struct list_head *at = head->next;

		first->prev = head;
		head->next = first;

		last->next = at;
		at->prev = last;
	}
}


#define list_entry(ptr, type, member)	container_of(ptr, type, member)

#define list_first_entry(head, type, member) \
	((head) && (head)->next != (head) ? list_entry((head)->next, type, member) : NULL)

#define list_last_entry(head, type, member) \
	((head) && (head)->prev != (head) ? list_entry((head)->prev, type, member) : NULL)


#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)


#define list_for_each_backwardly(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)


#define list_for_each_safe(pos, pnext, head) \
	for (pos = (head)->next, pnext = pos->next; pos != (head); \
	     pos = pnext, pnext = pos->next)


#define list_free(head, type, member, freefunc)				\
	do {								\
		struct list_head *__p, *__pnext;			\
									\
		list_for_each_safe (__p, __pnext, (head)) {		\
			type *__elt = list_entry(__p, type, member);	\
			list_del(__p);					\
			freefunc(__elt);			\
		}							\
	} while (0)

_INLINE_ size_t list_count_entries(struct list_head *head)
{
	struct list_head *pos;
	size_t ct = 0;

	list_for_each(pos, head)
		ct++;

	return ct;
}

#define MAX_LIST_LENGTH_BITS 20


_INLINE_ struct list_head *merge(int (*cmp)(struct list_head *a,
					  struct list_head *b,
					  void *data),
			       void *data,
			       struct list_head *a, struct list_head *b)
{
	struct list_head head, *tail = &head;

	while (a && b) {
		
		if ((*cmp)(a, b, data) <= 0) {
			tail->next = a;
			a = a->next;
		} else {
			tail->next = b;
			b = b->next;
		}
		tail = tail->next;
	}
	tail->next = a ? a : b;
	return head.next;
}


_INLINE_ void merge_and_restore_back_links(int (*cmp)(struct list_head *a,
						    struct list_head *b,
						    void *data),
					 void *data,
					 struct list_head *head,
					 struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head;

	while (a && b) {
		
		if ((*cmp)(a, b, data) <= 0) {
			tail->next = a;
			a->prev = tail;
			a = a->next;
		} else {
			tail->next = b;
			b->prev = tail;
			b = b->next;
		}
		tail = tail->next;
	}
	tail->next = a ? a : b;

	do {
		
		(*cmp)(tail->next, tail->next, data);

		tail->next->prev = tail;
		tail = tail->next;
	} while (tail->next);

	tail->next = head;
	head->prev = tail;
}



_INLINE_ void list_sort(struct list_head *head,
			int (*cmp)(struct list_head *a,
				   struct list_head *b,
				   void *data),
			void *data)
{
	struct list_head *part[MAX_LIST_LENGTH_BITS+1]; 
	size_t lev;  
	size_t max_lev = 0;
	struct list_head *list;

	if (list_empty(head))
		return;

	memset(part, 0, sizeof(part));

	head->prev->next = NULL;
	list = head->next;

	while (list) {
		struct list_head *cur = list;
		list = list->next;
		cur->next = NULL;

		for (lev = 0; part[lev]; lev++) {
			cur = merge(cmp, data, part[lev], cur);
			part[lev] = NULL;
		}
		if (lev > max_lev) {
			
			if (lev >= ARRAY_SIZE(part) - 1)
				lev--;
			max_lev = lev;
		}
		part[lev] = cur;
	}

	for (lev = 0; lev < max_lev; lev++)
		if (part[lev])
			list = merge(cmp, data, part[lev], list);

	merge_and_restore_back_links(cmp, data, head, part[max_lev], list);
}

#undef _INLINE_

#endif 
