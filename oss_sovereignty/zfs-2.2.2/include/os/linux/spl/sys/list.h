 

#ifndef _SPL_LIST_H
#define	_SPL_LIST_H

#include <sys/types.h>
#include <sys/debug.h>
#include <linux/list.h>

 

typedef struct list_head list_node_t;

typedef struct list {
	size_t list_size;
	size_t list_offset;
	list_node_t list_head;
} list_t;

#define	list_d2l(a, obj) ((list_node_t *)(((char *)obj) + (a)->list_offset))
#define	list_object(a, node) ((void *)(((char *)node) - (a)->list_offset))

static inline int
list_is_empty(list_t *list)
{
	return (list_empty(&list->list_head));
}

static inline void
list_link_init(list_node_t *node)
{
	node->next = LIST_POISON1;
	node->prev = LIST_POISON2;
}

static inline void
list_create(list_t *list, size_t size, size_t offset)
{
	list->list_size = size;
	list->list_offset = offset;
	INIT_LIST_HEAD(&list->list_head);
}

static inline void
list_destroy(list_t *list)
{
	list_del(&list->list_head);
}

static inline void
list_insert_head(list_t *list, void *object)
{
	list_add(list_d2l(list, object), &list->list_head);
}

static inline void
list_insert_tail(list_t *list, void *object)
{
	list_add_tail(list_d2l(list, object), &list->list_head);
}

static inline void
list_insert_after(list_t *list, void *object, void *nobject)
{
	if (object == NULL)
		list_insert_head(list, nobject);
	else
		list_add(list_d2l(list, nobject), list_d2l(list, object));
}

static inline void
list_insert_before(list_t *list, void *object, void *nobject)
{
	if (object == NULL)
		list_insert_tail(list, nobject);
	else
		list_add_tail(list_d2l(list, nobject), list_d2l(list, object));
}

static inline void
list_remove(list_t *list, void *object)
{
	list_del(list_d2l(list, object));
}

static inline void *
list_remove_head(list_t *list)
{
	list_node_t *head = list->list_head.next;
	if (head == &list->list_head)
		return (NULL);

	list_del(head);
	return (list_object(list, head));
}

static inline void *
list_remove_tail(list_t *list)
{
	list_node_t *tail = list->list_head.prev;
	if (tail == &list->list_head)
		return (NULL);

	list_del(tail);
	return (list_object(list, tail));
}

static inline void *
list_head(list_t *list)
{
	if (list_is_empty(list))
		return (NULL);

	return (list_object(list, list->list_head.next));
}

static inline void *
list_tail(list_t *list)
{
	if (list_is_empty(list))
		return (NULL);

	return (list_object(list, list->list_head.prev));
}

static inline void *
list_next(list_t *list, void *object)
{
	list_node_t *node = list_d2l(list, object);

	if (node->next != &list->list_head)
		return (list_object(list, node->next));

	return (NULL);
}

static inline void *
list_prev(list_t *list, void *object)
{
	list_node_t *node = list_d2l(list, object);

	if (node->prev != &list->list_head)
		return (list_object(list, node->prev));

	return (NULL);
}

static inline int
list_link_active(list_node_t *node)
{
	EQUIV(node->next == LIST_POISON1, node->prev == LIST_POISON2);
	return (node->next != LIST_POISON1);
}

static inline void
spl_list_move_tail(list_t *dst, list_t *src)
{
	list_splice_init(&src->list_head, dst->list_head.prev);
}

#define	list_move_tail(dst, src)	spl_list_move_tail(dst, src)

static inline void
list_link_replace(list_node_t *old_node, list_node_t *new_node)
{
	new_node->next = old_node->next;
	new_node->prev = old_node->prev;
	old_node->prev->next = new_node;
	old_node->next->prev = new_node;
	list_link_init(old_node);
}

#endif  
