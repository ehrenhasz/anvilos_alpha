


#ifndef __AA_NAMESPACE_H
#define __AA_NAMESPACE_H

#include <linux/kref.h>

#include "apparmor.h"
#include "apparmorfs.h"
#include "label.h"
#include "policy.h"



struct aa_ns_acct {
	int max_size;
	int max_count;
	int size;
	int count;
};


struct aa_ns {
	struct aa_policy base;
	struct aa_ns *parent;
	struct mutex lock;
	struct aa_ns_acct acct;
	struct aa_profile *unconfined;
	struct list_head sub_ns;
	atomic_t uniq_null;
	long uniq_id;
	int level;
	long revision;
	wait_queue_head_t wait;

	struct aa_labelset labels;
	struct list_head rawdata_list;

	struct dentry *dents[AAFS_NS_SIZEOF];
};

extern struct aa_label *kernel_t;
extern struct aa_ns *root_ns;

extern const char *aa_hidden_ns_name;

#define ns_unconfined(NS) (&(NS)->unconfined->label)

bool aa_ns_visible(struct aa_ns *curr, struct aa_ns *view, bool subns);
const char *aa_ns_name(struct aa_ns *parent, struct aa_ns *child, bool subns);
void aa_free_ns(struct aa_ns *ns);
int aa_alloc_root_ns(void);
void aa_free_root_ns(void);
void aa_free_ns_kref(struct kref *kref);

struct aa_ns *aa_find_ns(struct aa_ns *root, const char *name);
struct aa_ns *aa_findn_ns(struct aa_ns *root, const char *name, size_t n);
struct aa_ns *__aa_lookupn_ns(struct aa_ns *view, const char *hname, size_t n);
struct aa_ns *aa_lookupn_ns(struct aa_ns *view, const char *name, size_t n);
struct aa_ns *__aa_find_or_create_ns(struct aa_ns *parent, const char *name,
				     struct dentry *dir);
struct aa_ns *aa_prepare_ns(struct aa_ns *root, const char *name);
void __aa_remove_ns(struct aa_ns *ns);

static inline struct aa_profile *aa_deref_parent(struct aa_profile *p)
{
	return rcu_dereference_protected(p->parent,
					 mutex_is_locked(&p->ns->lock));
}


static inline struct aa_ns *aa_get_ns(struct aa_ns *ns)
{
	if (ns)
		aa_get_profile(ns->unconfined);

	return ns;
}


static inline void aa_put_ns(struct aa_ns *ns)
{
	if (ns)
		aa_put_profile(ns->unconfined);
}


static inline struct aa_ns *__aa_findn_ns(struct list_head *head,
					  const char *name, size_t n)
{
	return (struct aa_ns *)__policy_strn_find(head, name, n);
}

static inline struct aa_ns *__aa_find_ns(struct list_head *head,
					 const char *name)
{
	return __aa_findn_ns(head, name, strlen(name));
}

static inline struct aa_ns *__aa_lookup_ns(struct aa_ns *base,
					   const char *hname)
{
	return __aa_lookupn_ns(base, hname, strlen(hname));
}

static inline struct aa_ns *aa_lookup_ns(struct aa_ns *view, const char *name)
{
	return aa_lookupn_ns(view, name, strlen(name));
}

#endif 
