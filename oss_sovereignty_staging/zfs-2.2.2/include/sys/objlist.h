 
 

#ifndef	_OBJLIST_H
#define	_OBJLIST_H

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/zfs_context.h>

typedef struct objlist_node {
	list_node_t	on_node;
	uint64_t	on_object;
} objlist_node_t;

typedef struct objlist {
	list_t		ol_list;  
	 
	uint64_t	ol_last_lookup;
} objlist_t;

objlist_t *objlist_create(void);
void objlist_destroy(objlist_t *);
boolean_t objlist_exists(objlist_t *, uint64_t);
void objlist_insert(objlist_t *, uint64_t);

#ifdef	__cplusplus
}
#endif

#endif	 
