



#ifndef _SS_AVTAB_H_
#define _SS_AVTAB_H_

#include "security.h"

struct avtab_key {
	u16 source_type;	
	u16 target_type;	
	u16 target_class;	
#define AVTAB_ALLOWED		0x0001
#define AVTAB_AUDITALLOW	0x0002
#define AVTAB_AUDITDENY		0x0004
#define AVTAB_AV		(AVTAB_ALLOWED | AVTAB_AUDITALLOW | AVTAB_AUDITDENY)
#define AVTAB_TRANSITION	0x0010
#define AVTAB_MEMBER		0x0020
#define AVTAB_CHANGE		0x0040
#define AVTAB_TYPE		(AVTAB_TRANSITION | AVTAB_MEMBER | AVTAB_CHANGE)

#define AVTAB_XPERMS_ALLOWED	0x0100
#define AVTAB_XPERMS_AUDITALLOW	0x0200
#define AVTAB_XPERMS_DONTAUDIT	0x0400
#define AVTAB_XPERMS		(AVTAB_XPERMS_ALLOWED | \
				AVTAB_XPERMS_AUDITALLOW | \
				AVTAB_XPERMS_DONTAUDIT)
#define AVTAB_ENABLED_OLD   0x80000000 
#define AVTAB_ENABLED		0x8000 
	u16 specified;	
};


struct avtab_extended_perms {

#define AVTAB_XPERMS_IOCTLFUNCTION	0x01
#define AVTAB_XPERMS_IOCTLDRIVER	0x02
	
	u8 specified; 
	
	u8 driver;
	
	struct extended_perms_data perms;
};

struct avtab_datum {
	union {
		u32 data; 
		struct avtab_extended_perms *xperms;
	} u;
};

struct avtab_node {
	struct avtab_key key;
	struct avtab_datum datum;
	struct avtab_node *next;
};

struct avtab {
	struct avtab_node **htable;
	u32 nel;	
	u32 nslot;      
	u32 mask;       
};

void avtab_init(struct avtab *h);
int avtab_alloc(struct avtab *, u32);
int avtab_alloc_dup(struct avtab *new, const struct avtab *orig);
void avtab_destroy(struct avtab *h);

#ifdef CONFIG_SECURITY_SELINUX_DEBUG
void avtab_hash_eval(struct avtab *h, const char *tag);
#else
static inline void avtab_hash_eval(struct avtab *h, const char *tag)
{
}
#endif

struct policydb;
int avtab_read_item(struct avtab *a, void *fp, struct policydb *pol,
		    int (*insert)(struct avtab *a, const struct avtab_key *k,
				  const struct avtab_datum *d, void *p),
		    void *p);

int avtab_read(struct avtab *a, void *fp, struct policydb *pol);
int avtab_write_item(struct policydb *p, const struct avtab_node *cur, void *fp);
int avtab_write(struct policydb *p, struct avtab *a, void *fp);

struct avtab_node *avtab_insert_nonunique(struct avtab *h,
					  const struct avtab_key *key,
					  const struct avtab_datum *datum);

struct avtab_node *avtab_search_node(struct avtab *h,
				     const struct avtab_key *key);

struct avtab_node *avtab_search_node_next(struct avtab_node *node, u16 specified);

#define MAX_AVTAB_HASH_BITS 16
#define MAX_AVTAB_HASH_BUCKETS (1 << MAX_AVTAB_HASH_BITS)

#endif	

