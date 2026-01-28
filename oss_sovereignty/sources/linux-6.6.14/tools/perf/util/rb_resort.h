
#ifndef _PERF_RESORT_RB_H_
#define _PERF_RESORT_RB_H_


#define DEFINE_RESORT_RB(__name, __comp, ...)					\
struct __name##_sorted_entry {							\
	struct rb_node	rb_node;						\
	__VA_ARGS__								\
};										\
static void __name##_sorted__init_entry(struct rb_node *nd,			\
					struct __name##_sorted_entry *entry);	\
										\
static int __name##_sorted__cmp(struct rb_node *nda, struct rb_node *ndb)	\
{										\
	struct __name##_sorted_entry *a, *b;					\
	a = rb_entry(nda, struct __name##_sorted_entry, rb_node);		\
	b = rb_entry(ndb, struct __name##_sorted_entry, rb_node);		\
	return __comp;								\
}										\
										\
struct __name##_sorted {							\
       struct rb_root		    entries;					\
       struct __name##_sorted_entry nd[0];					\
};										\
										\
static void __name##_sorted__insert(struct __name##_sorted *sorted,		\
				      struct rb_node *sorted_nd)		\
{										\
	struct rb_node **p = &sorted->entries.rb_node, *parent = NULL;		\
	while (*p != NULL) {							\
		parent = *p;							\
		if (__name##_sorted__cmp(sorted_nd, parent))			\
			p = &(*p)->rb_left;					\
		else								\
			p = &(*p)->rb_right;					\
	}									\
	rb_link_node(sorted_nd, parent, p);					\
	rb_insert_color(sorted_nd, &sorted->entries);				\
}										\
										\
static void __name##_sorted__sort(struct __name##_sorted *sorted,		\
				    struct rb_root *entries)			\
{										\
	struct rb_node *nd;							\
	unsigned int i = 0;							\
	for (nd = rb_first(entries); nd; nd = rb_next(nd)) {			\
		struct __name##_sorted_entry *snd = &sorted->nd[i++];		\
		__name##_sorted__init_entry(nd, snd);				\
		__name##_sorted__insert(sorted, &snd->rb_node);			\
	}									\
}										\
										\
static struct __name##_sorted *__name##_sorted__new(struct rb_root *entries,	\
						    int nr_entries)		\
{										\
	struct __name##_sorted *sorted;						\
	sorted = malloc(sizeof(*sorted) + sizeof(sorted->nd[0]) * nr_entries);	\
	if (sorted) {								\
		sorted->entries = RB_ROOT;					\
		__name##_sorted__sort(sorted, entries);				\
	}									\
	return sorted;								\
}										\
										\
static void __name##_sorted__delete(struct __name##_sorted *sorted)		\
{										\
	free(sorted);								\
}										\
										\
static void __name##_sorted__init_entry(struct rb_node *nd,			\
					struct __name##_sorted_entry *entry)

#define DECLARE_RESORT_RB(__name)						\
struct __name##_sorted_entry *__name##_entry;					\
struct __name##_sorted *__name = __name##_sorted__new

#define resort_rb__for_each_entry(__nd, __name)					\
	for (__nd = rb_first(&__name->entries);					\
	     __name##_entry = rb_entry(__nd, struct __name##_sorted_entry,	\
				       rb_node), __nd;				\
	     __nd = rb_next(__nd))

#define resort_rb__delete(__name)						\
	__name##_sorted__delete(__name), __name = NULL




#define DECLARE_RESORT_RB_INTLIST(__name, __ilist)				\
	DECLARE_RESORT_RB(__name)(&__ilist->rblist.entries.rb_root,		\
				  __ilist->rblist.nr_entries)


#define DECLARE_RESORT_RB_MACHINE_THREADS(__name, __machine, hash_bucket)    \
 DECLARE_RESORT_RB(__name)(&__machine->threads[hash_bucket].entries.rb_root, \
			   __machine->threads[hash_bucket].nr)

#endif 
