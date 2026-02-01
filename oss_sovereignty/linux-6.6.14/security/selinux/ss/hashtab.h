 
 
#ifndef _SS_HASHTAB_H_
#define _SS_HASHTAB_H_

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>

#define HASHTAB_MAX_NODES	U32_MAX

struct hashtab_key_params {
	u32 (*hash)(const void *key);	 
	int (*cmp)(const void *key1, const void *key2);
					 
};

struct hashtab_node {
	void *key;
	void *datum;
	struct hashtab_node *next;
};

struct hashtab {
	struct hashtab_node **htable;	 
	u32 size;			 
	u32 nel;			 
};

struct hashtab_info {
	u32 slots_used;
	u32 max_chain_len;
};

 
int hashtab_init(struct hashtab *h, u32 nel_hint);

int __hashtab_insert(struct hashtab *h, struct hashtab_node **dst,
		     void *key, void *datum);

 
static inline int hashtab_insert(struct hashtab *h, void *key, void *datum,
				 struct hashtab_key_params key_params)
{
	u32 hvalue;
	struct hashtab_node *prev, *cur;

	cond_resched();

	if (!h->size || h->nel == HASHTAB_MAX_NODES)
		return -EINVAL;

	hvalue = key_params.hash(key) & (h->size - 1);
	prev = NULL;
	cur = h->htable[hvalue];
	while (cur) {
		int cmp = key_params.cmp(key, cur->key);

		if (cmp == 0)
			return -EEXIST;
		if (cmp < 0)
			break;
		prev = cur;
		cur = cur->next;
	}

	return __hashtab_insert(h, prev ? &prev->next : &h->htable[hvalue],
				key, datum);
}

 
static inline void *hashtab_search(struct hashtab *h, const void *key,
				   struct hashtab_key_params key_params)
{
	u32 hvalue;
	struct hashtab_node *cur;

	if (!h->size)
		return NULL;

	hvalue = key_params.hash(key) & (h->size - 1);
	cur = h->htable[hvalue];
	while (cur) {
		int cmp = key_params.cmp(key, cur->key);

		if (cmp == 0)
			return cur->datum;
		if (cmp < 0)
			break;
		cur = cur->next;
	}
	return NULL;
}

 
void hashtab_destroy(struct hashtab *h);

 
int hashtab_map(struct hashtab *h,
		int (*apply)(void *k, void *d, void *args),
		void *args);

int hashtab_duplicate(struct hashtab *new, struct hashtab *orig,
		int (*copy)(struct hashtab_node *new,
			struct hashtab_node *orig, void *args),
		int (*destroy)(void *k, void *d, void *args),
		void *args);

#ifdef CONFIG_SECURITY_SELINUX_DEBUG
 
void hashtab_stat(struct hashtab *h, struct hashtab_info *info);
#else
static inline void hashtab_stat(struct hashtab *h, struct hashtab_info *info)
{
}
#endif

#endif	 
