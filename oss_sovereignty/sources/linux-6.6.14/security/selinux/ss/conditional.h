


#ifndef _CONDITIONAL_H_
#define _CONDITIONAL_H_

#include "avtab.h"
#include "symtab.h"
#include "policydb.h"
#include "../include/conditional.h"

#define COND_EXPR_MAXDEPTH 10


struct cond_expr_node {
#define COND_BOOL	1 
#define COND_NOT	2 
#define COND_OR		3 
#define COND_AND	4 
#define COND_XOR	5 
#define COND_EQ		6 
#define COND_NEQ	7 
#define COND_LAST	COND_NEQ
	u32 expr_type;
	u32 boolean;
};

struct cond_expr {
	struct cond_expr_node *nodes;
	u32 len;
};


struct cond_av_list {
	struct avtab_node **nodes;
	u32 len;
};


struct cond_node {
	int cur_state;
	struct cond_expr expr;
	struct cond_av_list true_list;
	struct cond_av_list false_list;
};

void cond_policydb_init(struct policydb *p);
void cond_policydb_destroy(struct policydb *p);

int cond_init_bool_indexes(struct policydb *p);
int cond_destroy_bool(void *key, void *datum, void *p);

int cond_index_bool(void *key, void *datum, void *datap);

int cond_read_bool(struct policydb *p, struct symtab *s, void *fp);
int cond_read_list(struct policydb *p, void *fp);
int cond_write_bool(void *key, void *datum, void *ptr);
int cond_write_list(struct policydb *p, void *fp);

void cond_compute_av(struct avtab *ctab, struct avtab_key *key,
		struct av_decision *avd, struct extended_perms *xperms);
void cond_compute_xperms(struct avtab *ctab, struct avtab_key *key,
		struct extended_perms_decision *xpermd);
void evaluate_cond_nodes(struct policydb *p);
void cond_policydb_destroy_dup(struct policydb *p);
int cond_policydb_dup(struct policydb *new, struct policydb *orig);

#endif 
