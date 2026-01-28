
#ifndef __PERF_INTLIST_H
#define __PERF_INTLIST_H

#include <linux/rbtree.h>
#include <stdbool.h>

#include "rblist.h"

struct int_node {
	struct rb_node rb_node;
	unsigned long i;
	void *priv;
};

struct intlist {
	struct rblist rblist;
};

struct intlist *intlist__new(const char *slist);
void intlist__delete(struct intlist *ilist);

void intlist__remove(struct intlist *ilist, struct int_node *in);
int intlist__add(struct intlist *ilist, unsigned long i);

struct int_node *intlist__entry(const struct intlist *ilist, unsigned int idx);
struct int_node *intlist__find(struct intlist *ilist, unsigned long i);
struct int_node *intlist__findnew(struct intlist *ilist, unsigned long i);

static inline bool intlist__has_entry(struct intlist *ilist, unsigned long i)
{
	return intlist__find(ilist, i) != NULL;
}

static inline bool intlist__empty(const struct intlist *ilist)
{
	return rblist__empty(&ilist->rblist);
}

static inline unsigned int intlist__nr_entries(const struct intlist *ilist)
{
	return rblist__nr_entries(&ilist->rblist);
}


static inline struct int_node *intlist__first(struct intlist *ilist)
{
	struct rb_node *rn = rb_first_cached(&ilist->rblist.entries);
	return rn ? rb_entry(rn, struct int_node, rb_node) : NULL;
}
static inline struct int_node *intlist__next(struct int_node *in)
{
	struct rb_node *rn;
	if (!in)
		return NULL;
	rn = rb_next(&in->rb_node);
	return rn ? rb_entry(rn, struct int_node, rb_node) : NULL;
}


#define intlist__for_each_entry(pos, ilist)	\
	for (pos = intlist__first(ilist); pos; pos = intlist__next(pos))


#define intlist__for_each_entry_safe(pos, n, ilist)	\
	for (pos = intlist__first(ilist), n = intlist__next(pos); pos;\
	     pos = n, n = intlist__next(n))
#endif 
