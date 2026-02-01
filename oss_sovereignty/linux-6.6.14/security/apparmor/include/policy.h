 
 

#ifndef __AA_POLICY_H
#define __AA_POLICY_H

#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/kref.h>
#include <linux/rhashtable.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/socket.h>

#include "apparmor.h"
#include "audit.h"
#include "capability.h"
#include "domain.h"
#include "file.h"
#include "lib.h"
#include "label.h"
#include "net.h"
#include "perms.h"
#include "resource.h"


struct aa_ns;

extern int unprivileged_userns_apparmor_policy;

extern const char *const aa_profile_mode_names[];
#define APPARMOR_MODE_NAMES_MAX_INDEX 4

#define PROFILE_MODE(_profile, _mode)		\
	((aa_g_profile_mode == (_mode)) ||	\
	 ((_profile)->mode == (_mode)))

#define COMPLAIN_MODE(_profile)	PROFILE_MODE((_profile), APPARMOR_COMPLAIN)

#define USER_MODE(_profile)	PROFILE_MODE((_profile), APPARMOR_USER)

#define KILL_MODE(_profile) PROFILE_MODE((_profile), APPARMOR_KILL)

#define PROFILE_IS_HAT(_profile) ((_profile)->label.flags & FLAG_HAT)

#define CHECK_DEBUG1(_profile) ((_profile)->label.flags & FLAG_DEBUG1)

#define CHECK_DEBUG2(_profile) ((_profile)->label.flags & FLAG_DEBUG2)

#define profile_is_stale(_profile) (label_is_stale(&(_profile)->label))

#define on_list_rcu(X) (!list_empty(X) && (X)->prev != LIST_POISON2)

 
enum profile_mode {
	APPARMOR_ENFORCE,	 
	APPARMOR_COMPLAIN,	 
	APPARMOR_KILL,		 
	APPARMOR_UNCONFINED,	 
	APPARMOR_USER,		 
};


 
struct aa_policydb {
	struct aa_dfa *dfa;
	struct {
		struct aa_perms *perms;
		u32 size;
	};
	struct aa_str_table trans;
	aa_state_t start[AA_CLASS_LAST + 1];
};

static inline void aa_destroy_policydb(struct aa_policydb *policy)
{
	aa_put_dfa(policy->dfa);
	if (policy->perms)
		kvfree(policy->perms);
	aa_free_str_table(&policy->trans);

}

static inline struct aa_perms *aa_lookup_perms(struct aa_policydb *policy,
					       aa_state_t state)
{
	unsigned int index = ACCEPT_TABLE(policy->dfa)[state];

	if (!(policy->perms))
		return &default_perms;

	return &(policy->perms[index]);
}


 
struct aa_data {
	char *key;
	u32 size;
	char *data;
	struct rhash_head head;
};

 
struct aa_ruleset {
	struct list_head list;

	int size;

	 
	struct aa_policydb policy;
	struct aa_policydb file;
	struct aa_caps caps;

	struct aa_rlimit rlimits;

	int secmark_count;
	struct aa_secmark *secmark;
};

 
struct aa_attachment {
	const char *xmatch_str;
	struct aa_policydb xmatch;
	unsigned int xmatch_len;
	int xattr_count;
	char **xattrs;
};

 
struct aa_profile {
	struct aa_policy base;
	struct aa_profile __rcu *parent;

	struct aa_ns *ns;
	const char *rename;

	enum audit_mode audit;
	long mode;
	u32 path_flags;
	const char *disconnected;

	struct aa_attachment attach;
	struct list_head rules;

	struct aa_loaddata *rawdata;
	unsigned char *hash;
	char *dirname;
	struct dentry *dents[AAFS_PROF_SIZEOF];
	struct rhashtable *data;
	struct aa_label label;
};

extern enum profile_mode aa_g_profile_mode;

#define AA_MAY_LOAD_POLICY	AA_MAY_APPEND
#define AA_MAY_REPLACE_POLICY	AA_MAY_WRITE
#define AA_MAY_REMOVE_POLICY	AA_MAY_DELETE

#define profiles_ns(P) ((P)->ns)
#define name_is_shared(A, B) ((A)->hname && (A)->hname == (B)->hname)

void aa_add_profile(struct aa_policy *common, struct aa_profile *profile);


void aa_free_proxy_kref(struct kref *kref);
struct aa_ruleset *aa_alloc_ruleset(gfp_t gfp);
struct aa_profile *aa_alloc_profile(const char *name, struct aa_proxy *proxy,
				    gfp_t gfp);
struct aa_profile *aa_alloc_null(struct aa_profile *parent, const char *name,
				 gfp_t gfp);
struct aa_profile *aa_new_learning_profile(struct aa_profile *parent, bool hat,
					   const char *base, gfp_t gfp);
void aa_free_profile(struct aa_profile *profile);
void aa_free_profile_kref(struct kref *kref);
struct aa_profile *aa_find_child(struct aa_profile *parent, const char *name);
struct aa_profile *aa_lookupn_profile(struct aa_ns *ns, const char *hname,
				      size_t n);
struct aa_profile *aa_lookup_profile(struct aa_ns *ns, const char *name);
struct aa_profile *aa_fqlookupn_profile(struct aa_label *base,
					const char *fqname, size_t n);
struct aa_profile *aa_match_profile(struct aa_ns *ns, const char *name);

ssize_t aa_replace_profiles(struct aa_ns *view, struct aa_label *label,
			    u32 mask, struct aa_loaddata *udata);
ssize_t aa_remove_profiles(struct aa_ns *view, struct aa_label *label,
			   char *name, size_t size);
void __aa_profile_list_release(struct list_head *head);

#define PROF_ADD 1
#define PROF_REPLACE 0

#define profile_unconfined(X) ((X)->mode == APPARMOR_UNCONFINED)

 
static inline struct aa_profile *aa_get_newest_profile(struct aa_profile *p)
{
	return labels_profile(aa_get_newest_label(&p->label));
}

static inline aa_state_t RULE_MEDIATES(struct aa_ruleset *rules,
				       unsigned char class)
{
	if (class <= AA_CLASS_LAST)
		return rules->policy.start[class];
	else
		return aa_dfa_match_len(rules->policy.dfa,
					rules->policy.start[0], &class, 1);
}

static inline aa_state_t RULE_MEDIATES_AF(struct aa_ruleset *rules, u16 AF)
{
	aa_state_t state = RULE_MEDIATES(rules, AA_CLASS_NET);
	__be16 be_af = cpu_to_be16(AF);

	if (!state)
		return DFA_NOMATCH;
	return aa_dfa_match_len(rules->policy.dfa, state, (char *) &be_af, 2);
}

static inline aa_state_t ANY_RULE_MEDIATES(struct list_head *head,
					   unsigned char class)
{
	struct aa_ruleset *rule;

	 
	rule = list_first_entry(head, typeof(*rule), list);
	return RULE_MEDIATES(rule, class);
}

 
static inline struct aa_profile *aa_get_profile(struct aa_profile *p)
{
	if (p)
		kref_get(&(p->label.count));

	return p;
}

 
static inline struct aa_profile *aa_get_profile_not0(struct aa_profile *p)
{
	if (p && kref_get_unless_zero(&p->label.count))
		return p;

	return NULL;
}

 
static inline struct aa_profile *aa_get_profile_rcu(struct aa_profile __rcu **p)
{
	struct aa_profile *c;

	rcu_read_lock();
	do {
		c = rcu_dereference(*p);
	} while (c && !kref_get_unless_zero(&c->label.count));
	rcu_read_unlock();

	return c;
}

 
static inline void aa_put_profile(struct aa_profile *p)
{
	if (p)
		kref_put(&p->label.count, aa_label_kref);
}

static inline int AUDIT_MODE(struct aa_profile *profile)
{
	if (aa_g_audit != AUDIT_NORMAL)
		return aa_g_audit;

	return profile->audit;
}

bool aa_policy_view_capable(const struct cred *subj_cred,
			    struct aa_label *label, struct aa_ns *ns);
bool aa_policy_admin_capable(const struct cred *subj_cred,
			     struct aa_label *label, struct aa_ns *ns);
int aa_may_manage_policy(const struct cred *subj_cred,
			 struct aa_label *label, struct aa_ns *ns,
			 u32 mask);
bool aa_current_policy_view_capable(struct aa_ns *ns);
bool aa_current_policy_admin_capable(struct aa_ns *ns);

#endif  
