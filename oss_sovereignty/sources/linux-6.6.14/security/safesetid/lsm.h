

#ifndef _SAFESETID_H
#define _SAFESETID_H

#include <linux/types.h>
#include <linux/uidgid.h>
#include <linux/hashtable.h>


extern int safesetid_initialized __initdata;

enum sid_policy_type {
	SIDPOL_DEFAULT, 
	SIDPOL_CONSTRAINED, 
	SIDPOL_ALLOWED 
};

typedef union {
	kuid_t uid;
	kgid_t gid;
} kid_t;

enum setid_type {
	UID,
	GID
};


struct setid_rule {
	struct hlist_node next;
	kid_t src_id;
	kid_t dst_id;

	
	enum setid_type type;
};

#define SETID_HASH_BITS 8 


#define INVALID_ID (kid_t){.uid = INVALID_UID}

struct setid_ruleset {
	DECLARE_HASHTABLE(rules, SETID_HASH_BITS);
	char *policy_str;
	struct rcu_head rcu;

	
	enum setid_type type;
};

enum sid_policy_type _setid_policy_lookup(struct setid_ruleset *policy,
		kid_t src, kid_t dst);

extern struct setid_ruleset __rcu *safesetid_setuid_rules;
extern struct setid_ruleset __rcu *safesetid_setgid_rules;

#endif 
