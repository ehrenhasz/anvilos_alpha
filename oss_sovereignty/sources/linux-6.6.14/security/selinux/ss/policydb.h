




#ifndef _SS_POLICYDB_H_
#define _SS_POLICYDB_H_

#include "symtab.h"
#include "avtab.h"
#include "sidtab.h"
#include "ebitmap.h"
#include "mls_types.h"
#include "context.h"
#include "constraint.h"




struct perm_datum {
	u32 value;		
};


struct common_datum {
	u32 value;			
	struct symtab permissions;	
};


struct class_datum {
	u32 value;			
	char *comkey;			
	struct common_datum *comdatum;	
	struct symtab permissions;	
	struct constraint_node *constraints;	
	struct constraint_node *validatetrans;	

#define DEFAULT_SOURCE         1
#define DEFAULT_TARGET         2
	char default_user;
	char default_role;
	char default_type;

#define DEFAULT_SOURCE_LOW     1
#define DEFAULT_SOURCE_HIGH    2
#define DEFAULT_SOURCE_LOW_HIGH        3
#define DEFAULT_TARGET_LOW     4
#define DEFAULT_TARGET_HIGH    5
#define DEFAULT_TARGET_LOW_HIGH        6
#define DEFAULT_GLBLUB		7
	char default_range;
};


struct role_datum {
	u32 value;			
	u32 bounds;			
	struct ebitmap dominates;	
	struct ebitmap types;		
};

struct role_trans_key {
	u32 role;		
	u32 type;		
	u32 tclass;		
};

struct role_trans_datum {
	u32 new_role;		
};

struct filename_trans_key {
	u32 ttype;		
	u16 tclass;		
	const char *name;	
};

struct filename_trans_datum {
	struct ebitmap stypes;	
	u32 otype;		
	struct filename_trans_datum *next;	
};

struct role_allow {
	u32 role;		
	u32 new_role;		
	struct role_allow *next;
};


struct type_datum {
	u32 value;		
	u32 bounds;		
	unsigned char primary;	
	unsigned char attribute;
};


struct user_datum {
	u32 value;			
	u32 bounds;			
	struct ebitmap roles;		
	struct mls_range range;		
	struct mls_level dfltlevel;	
};



struct level_datum {
	struct mls_level *level;	
	unsigned char isalias;	
};


struct cat_datum {
	u32 value;		
	unsigned char isalias;  
};

struct range_trans {
	u32 source_type;
	u32 target_type;
	u32 target_class;
};


struct cond_bool_datum {
	__u32 value;		
	int state;
};

struct cond_node;


struct type_set {
	struct ebitmap types;
	struct ebitmap negset;
	u32 flags;
};


struct ocontext {
	union {
		char *name;	
		struct {
			u8 protocol;
			u16 low_port;
			u16 high_port;
		} port;		
		struct {
			u32 addr;
			u32 mask;
		} node;		
		struct {
			u32 addr[4];
			u32 mask[4];
		} node6;        
		struct {
			u64 subnet_prefix;
			u16 low_pkey;
			u16 high_pkey;
		} ibpkey;
		struct {
			char *dev_name;
			u8 port;
		} ibendport;
	} u;
	union {
		u32 sclass;  
		u32 behavior;  
	} v;
	struct context context[2];	
	u32 sid[2];	
	struct ocontext *next;
};

struct genfs {
	char *fstype;
	struct ocontext *head;
	struct genfs *next;
};


#define SYM_COMMONS 0
#define SYM_CLASSES 1
#define SYM_ROLES   2
#define SYM_TYPES   3
#define SYM_USERS   4
#define SYM_BOOLS   5
#define SYM_LEVELS  6
#define SYM_CATS    7
#define SYM_NUM     8


#define OCON_ISID	0 
#define OCON_FS		1 
#define OCON_PORT	2 
#define OCON_NETIF	3 
#define OCON_NODE	4 
#define OCON_FSUSE	5 
#define OCON_NODE6	6 
#define OCON_IBPKEY	7 
#define OCON_IBENDPORT	8 
#define OCON_NUM	9


struct policydb {
	int mls_enabled;

	
	struct symtab symtab[SYM_NUM];
#define p_commons symtab[SYM_COMMONS]
#define p_classes symtab[SYM_CLASSES]
#define p_roles symtab[SYM_ROLES]
#define p_types symtab[SYM_TYPES]
#define p_users symtab[SYM_USERS]
#define p_bools symtab[SYM_BOOLS]
#define p_levels symtab[SYM_LEVELS]
#define p_cats symtab[SYM_CATS]

	
	char		**sym_val_to_name[SYM_NUM];

	
	struct class_datum **class_val_to_struct;
	struct role_datum **role_val_to_struct;
	struct user_datum **user_val_to_struct;
	struct type_datum **type_val_to_struct;

	
	struct avtab te_avtab;

	
	struct hashtab role_tr;

	
	
	struct ebitmap filename_trans_ttypes;
	
	struct hashtab filename_trans;
	
	u32 compat_filename_trans_count;

	
	struct cond_bool_datum **bool_val_to_struct;
	
	struct avtab te_cond_avtab;
	
	struct cond_node *cond_list;
	u32 cond_list_len;

	
	struct role_allow *role_allow;

	
	struct ocontext *ocontexts[OCON_NUM];

	
	struct genfs *genfs;

	
	struct hashtab range_tr;

	
	struct ebitmap *type_attr_map_array;

	struct ebitmap policycaps;

	struct ebitmap permissive_map;

	
	size_t len;

	unsigned int policyvers;

	unsigned int reject_unknown : 1;
	unsigned int allow_unknown : 1;

	u16 process_class;
	u32 process_trans_perms;
} __randomize_layout;

extern void policydb_destroy(struct policydb *p);
extern int policydb_load_isids(struct policydb *p, struct sidtab *s);
extern int policydb_context_isvalid(struct policydb *p, struct context *c);
extern int policydb_class_isvalid(struct policydb *p, unsigned int class);
extern int policydb_type_isvalid(struct policydb *p, unsigned int type);
extern int policydb_role_isvalid(struct policydb *p, unsigned int role);
extern int policydb_read(struct policydb *p, void *fp);
extern int policydb_write(struct policydb *p, void *fp);

extern struct filename_trans_datum *policydb_filenametr_search(
	struct policydb *p, struct filename_trans_key *key);

extern struct mls_range *policydb_rangetr_search(
	struct policydb *p, struct range_trans *key);

extern struct role_trans_datum *policydb_roletr_search(
	struct policydb *p, struct role_trans_key *key);

#define POLICYDB_CONFIG_MLS    1


#define REJECT_UNKNOWN	0x00000002
#define ALLOW_UNKNOWN	0x00000004

#define OBJECT_R "object_r"
#define OBJECT_R_VAL 1

#define POLICYDB_MAGIC SELINUX_MAGIC
#define POLICYDB_STRING "SE Linux"

struct policy_file {
	char *data;
	size_t len;
};

struct policy_data {
	struct policydb *p;
	void *fp;
};

static inline int next_entry(void *buf, struct policy_file *fp, size_t bytes)
{
	if (bytes > fp->len)
		return -EINVAL;

	memcpy(buf, fp->data, bytes);
	fp->data += bytes;
	fp->len -= bytes;
	return 0;
}

static inline int put_entry(const void *buf, size_t bytes, size_t num, struct policy_file *fp)
{
	size_t len;

	if (unlikely(check_mul_overflow(bytes, num, &len)))
		return -EINVAL;

	if (len > fp->len)
		return -EINVAL;
	memcpy(fp->data, buf, len);
	fp->data += len;
	fp->len -= len;

	return 0;
}

static inline char *sym_name(struct policydb *p, unsigned int sym_num, unsigned int element_nr)
{
	return p->sym_val_to_name[sym_num][element_nr];
}

extern u16 string_to_security_class(struct policydb *p, const char *name);
extern u32 string_to_av_perm(struct policydb *p, u16 tclass, const char *name);

#endif	

