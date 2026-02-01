 
 

#ifndef	_LIBUUTIL_IMPL_H
#define	_LIBUUTIL_IMPL_H



#include <libuutil.h>
#include <pthread.h>

#include <sys/avl_impl.h>
#include <sys/byteorder.h>

#ifdef	__cplusplus
extern "C" {
#endif

void uu_set_error(uint_t);


__attribute__((format(printf, 1, 2), __noreturn__))
void uu_panic(const char *format, ...);


 
typedef struct uu_list_node_impl {
	struct uu_list_node_impl *uln_next;
	struct uu_list_node_impl *uln_prev;
} uu_list_node_impl_t;

struct uu_list_walk {
	uu_list_walk_t	*ulw_next;
	uu_list_walk_t	*ulw_prev;

	uu_list_t	*ulw_list;
	int8_t		ulw_dir;
	uint8_t		ulw_robust;
	uu_list_node_impl_t *ulw_next_result;
};

struct uu_list {
	uu_list_t	*ul_next;
	uu_list_t	*ul_prev;

	uu_list_pool_t	*ul_pool;
	void		*ul_parent;
	size_t		ul_offset;
	size_t		ul_numnodes;
	uint8_t		ul_debug;
	uint8_t		ul_sorted;
	uint8_t		ul_index;	 

	uu_list_node_impl_t ul_null_node;
	uu_list_walk_t	ul_null_walk;	 
};

#define	UU_LIST_POOL_MAXNAME	64

struct uu_list_pool {
	uu_list_pool_t	*ulp_next;
	uu_list_pool_t	*ulp_prev;

	char		ulp_name[UU_LIST_POOL_MAXNAME];
	size_t		ulp_nodeoffset;
	size_t		ulp_objsize;
	uu_compare_fn_t	*ulp_cmp;
	uint8_t		ulp_debug;
	uint8_t		ulp_last_index;
	pthread_mutex_t	ulp_lock;		 
	uu_list_t	ulp_null_list;
};

 
typedef struct avl_node		uu_avl_node_impl_t;

struct uu_avl_walk {
	uu_avl_walk_t	*uaw_next;
	uu_avl_walk_t	*uaw_prev;

	uu_avl_t	*uaw_avl;
	void		*uaw_next_result;
	int8_t		uaw_dir;
	uint8_t		uaw_robust;
};

struct uu_avl {
	uu_avl_t	*ua_next;
	uu_avl_t	*ua_prev;

	uu_avl_pool_t	*ua_pool;
	void		*ua_parent;
	uint8_t		ua_debug;
	uint8_t		ua_index;	 

	struct avl_tree	ua_tree;
	uu_avl_walk_t	ua_null_walk;
};

#define	UU_AVL_POOL_MAXNAME	64

struct uu_avl_pool {
	uu_avl_pool_t	*uap_next;
	uu_avl_pool_t	*uap_prev;

	char		uap_name[UU_AVL_POOL_MAXNAME];
	size_t		uap_nodeoffset;
	size_t		uap_objsize;
	uu_compare_fn_t	*uap_cmp;
	uint8_t		uap_debug;
	uint8_t		uap_last_index;
	pthread_mutex_t	uap_lock;		 
	uu_avl_t	uap_null_avl;
};

 
void uu_avl_lockup(void);
void uu_avl_release(void);

void uu_list_lockup(void);
void uu_list_release(void);

#ifdef	__cplusplus
}
#endif

#endif	 
