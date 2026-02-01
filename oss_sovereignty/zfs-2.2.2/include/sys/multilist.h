 
 

#ifndef	_SYS_MULTILIST_H
#define	_SYS_MULTILIST_H

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef list_node_t multilist_node_t;
typedef struct multilist multilist_t;
typedef struct multilist_sublist multilist_sublist_t;
typedef unsigned int multilist_sublist_index_func_t(multilist_t *, void *);

struct multilist_sublist {
	 
	kmutex_t	mls_lock;
	 
	list_t		mls_list;
	 
} ____cacheline_aligned;

struct multilist {
	 
	size_t				ml_offset;
	 
	uint64_t			ml_num_sublists;
	 
	multilist_sublist_t		*ml_sublists;
	 
	multilist_sublist_index_func_t	*ml_index_func;
};

void multilist_create(multilist_t *, size_t, size_t,
    multilist_sublist_index_func_t *);
void multilist_destroy(multilist_t *);

void multilist_insert(multilist_t *, void *);
void multilist_remove(multilist_t *, void *);
int  multilist_is_empty(multilist_t *);

unsigned int multilist_get_num_sublists(multilist_t *);
unsigned int multilist_get_random_index(multilist_t *);

multilist_sublist_t *multilist_sublist_lock(multilist_t *, unsigned int);
multilist_sublist_t *multilist_sublist_lock_obj(multilist_t *, void *);
void multilist_sublist_unlock(multilist_sublist_t *);

void multilist_sublist_insert_head(multilist_sublist_t *, void *);
void multilist_sublist_insert_tail(multilist_sublist_t *, void *);
void multilist_sublist_move_forward(multilist_sublist_t *mls, void *obj);
void multilist_sublist_remove(multilist_sublist_t *, void *);
int  multilist_sublist_is_empty(multilist_sublist_t *);
int  multilist_sublist_is_empty_idx(multilist_t *, unsigned int);

void *multilist_sublist_head(multilist_sublist_t *);
void *multilist_sublist_tail(multilist_sublist_t *);
void *multilist_sublist_next(multilist_sublist_t *, void *);
void *multilist_sublist_prev(multilist_sublist_t *, void *);

void multilist_link_init(multilist_node_t *);
int  multilist_link_active(multilist_node_t *);

#ifdef	__cplusplus
}
#endif

#endif  
