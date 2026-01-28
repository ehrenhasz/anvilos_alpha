




#ifndef	_AVL_H
#define	_AVL_H extern __attribute__((visibility("default")))



#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/avl_impl.h>




#define	TREE_ISIGN(a)	(((a) > 0) - ((a) < 0))
#define	TREE_CMP(a, b)	(((a) > (b)) - ((a) < (b)))
#define	TREE_PCMP(a, b)	\
	(((uintptr_t)(a) > (uintptr_t)(b)) - ((uintptr_t)(a) < (uintptr_t)(b)))


typedef struct avl_tree avl_tree_t;


typedef struct avl_node avl_node_t;


typedef uintptr_t avl_index_t;



#define	AVL_BEFORE	(0)
#define	AVL_AFTER	(1)





_AVL_H void avl_create(avl_tree_t *tree,
	int (*compar) (const void *, const void *), size_t size, size_t offset);



_AVL_H void *avl_find(avl_tree_t *tree, const void *node, avl_index_t *where);


_AVL_H void avl_insert(avl_tree_t *tree, void *node, avl_index_t where);


_AVL_H void avl_insert_here(avl_tree_t *tree, void *new_data, void *here,
    int direction);



_AVL_H void *avl_first(avl_tree_t *tree);
_AVL_H void *avl_last(avl_tree_t *tree);



#define	AVL_NEXT(tree, node)	avl_walk(tree, node, AVL_AFTER)
#define	AVL_PREV(tree, node)	avl_walk(tree, node, AVL_BEFORE)



_AVL_H void *avl_nearest(avl_tree_t *tree, avl_index_t where, int direction);



_AVL_H void avl_add(avl_tree_t *tree, void *node);



_AVL_H void avl_remove(avl_tree_t *tree, void *node);


_AVL_H boolean_t avl_update(avl_tree_t *, void *);
_AVL_H boolean_t avl_update_lt(avl_tree_t *, void *);
_AVL_H boolean_t avl_update_gt(avl_tree_t *, void *);


_AVL_H void avl_swap(avl_tree_t *tree1, avl_tree_t *tree2);


_AVL_H ulong_t avl_numnodes(avl_tree_t *tree);


_AVL_H boolean_t avl_is_empty(avl_tree_t *tree);


_AVL_H void *avl_destroy_nodes(avl_tree_t *tree, void **cookie);



_AVL_H void avl_destroy(avl_tree_t *tree);



#ifdef	__cplusplus
}
#endif

#endif	
