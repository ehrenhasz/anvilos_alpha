 
 

 

#ifndef _SYS_SPACE_REFTREE_H
#define	_SYS_SPACE_REFTREE_H

#include <sys/range_tree.h>
#include <sys/avl.h>
#ifdef	__cplusplus
extern "C" {
#endif

typedef struct space_ref {
	avl_node_t	sr_node;	 
	uint64_t	sr_offset;	 
	int64_t		sr_refcnt;	 
} space_ref_t;

void space_reftree_create(avl_tree_t *t);
void space_reftree_destroy(avl_tree_t *t);
void space_reftree_add_seg(avl_tree_t *t, uint64_t start, uint64_t end,
    int64_t refcnt);
void space_reftree_add_map(avl_tree_t *t, range_tree_t *rt, int64_t refcnt);
void space_reftree_generate_map(avl_tree_t *t, range_tree_t *rt,
    int64_t minref);

#ifdef	__cplusplus
}
#endif

#endif	 
