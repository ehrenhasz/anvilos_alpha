


#ifndef	_AVL_IMPL_H
#define	_AVL_IMPL_H extern __attribute__((visibility("default")))




#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif




#ifndef _LP64

struct avl_node {
	struct avl_node *avl_child[2];	
	struct avl_node *avl_parent;	
	unsigned short avl_child_index;	
	short avl_balance;		
};

#define	AVL_XPARENT(n)		((n)->avl_parent)
#define	AVL_SETPARENT(n, p)	((n)->avl_parent = (p))

#define	AVL_XCHILD(n)		((n)->avl_child_index)
#define	AVL_SETCHILD(n, c)	((n)->avl_child_index = (unsigned short)(c))

#define	AVL_XBALANCE(n)		((n)->avl_balance)
#define	AVL_SETBALANCE(n, b)	((n)->avl_balance = (short)(b))

#else 


struct avl_node {
	struct avl_node *avl_child[2];	
	uintptr_t avl_pcb;		
};


#define	AVL_XPARENT(n)		((struct avl_node *)((n)->avl_pcb & ~7))
#define	AVL_SETPARENT(n, p)						\
	((n)->avl_pcb = (((n)->avl_pcb & 7) | (uintptr_t)(p)))


#define	AVL_XCHILD(n)		(((n)->avl_pcb >> 2) & 1)
#define	AVL_SETCHILD(n, c)						\
	((n)->avl_pcb = (uintptr_t)(((n)->avl_pcb & ~4) | ((c) << 2)))


#define	AVL_XBALANCE(n)		((int)(((n)->avl_pcb & 3) - 1))
#define	AVL_SETBALANCE(n, b)						\
	((n)->avl_pcb = (uintptr_t)((((n)->avl_pcb & ~3) | ((b) + 1))))

#endif 




#define	AVL_NODE2DATA(n, o)	((void *)((uintptr_t)(n) - (o)))
#define	AVL_DATA2NODE(d, o)	((struct avl_node *)((uintptr_t)(d) + (o)))




#define	AVL_INDEX2NODE(x)	((avl_node_t *)((x) & ~1))
#define	AVL_INDEX2CHILD(x)	((x) & 1)
#define	AVL_MKINDEX(n, c)	((avl_index_t)(n) | (c))



struct avl_tree {
	struct avl_node *avl_root;	
	int (*avl_compar)(const void *, const void *);
	size_t avl_offset;		
	ulong_t avl_numnodes;		
#ifndef _KERNEL
	size_t avl_pad;			
#endif
};



_AVL_IMPL_H void *avl_walk(struct avl_tree *, void *, int);

#ifdef	__cplusplus
}
#endif

#endif	
