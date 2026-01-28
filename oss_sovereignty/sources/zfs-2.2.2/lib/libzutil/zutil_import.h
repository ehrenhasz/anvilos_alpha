

#ifndef _LIBZUTIL_ZUTIL_IMPORT_H_
#define	_LIBZUTIL_ZUTIL_IMPORT_H_

#define	IMPORT_ORDER_PREFERRED_1	1
#define	IMPORT_ORDER_PREFERRED_2	2
#define	IMPORT_ORDER_SCAN_OFFSET	10
#define	IMPORT_ORDER_DEFAULT		100

int label_paths(libpc_handle_t *hdl, nvlist_t *label, const char **path,
    const char **devid);
int zpool_find_import_blkid(libpc_handle_t *hdl, pthread_mutex_t *lock,
    avl_tree_t **slice_cache);

void * zutil_alloc(libpc_handle_t *hdl, size_t size);
char *zutil_strdup(libpc_handle_t *hdl, const char *str);

typedef struct rdsk_node {
	char *rn_name;			
	int rn_order;			
	int rn_num_labels;		
	uint64_t rn_vdev_guid;		
	libpc_handle_t *rn_hdl;
	nvlist_t *rn_config;		
	avl_tree_t *rn_avl;
	avl_node_t rn_node;
	pthread_mutex_t *rn_lock;
	boolean_t rn_labelpaths;
} rdsk_node_t;

int slice_cache_compare(const void *, const void *);

void zpool_open_func(void *);

#endif 
