
#ifndef UTIL_LINUX_LSBLK_H
#define UTIL_LINUX_LSBLK_H

#include <stdint.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <libsmartcols.h>
#include <libmount.h>

#include "c.h"
#include "list.h"
#include "debug.h"

#define LSBLK_DEBUG_INIT	(1 << 1)
#define LSBLK_DEBUG_FILTER	(1 << 2)
#define LSBLK_DEBUG_DEV		(1 << 3)
#define LSBLK_DEBUG_TREE	(1 << 4)
#define LSBLK_DEBUG_DEP		(1 << 5)
#define LSBLK_DEBUG_ALL		0xFFFF

UL_DEBUG_DECLARE_MASK(lsblk);
#define DBG(m, x)       __UL_DBG(lsblk, LSBLK_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(lsblk, LSBLK_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(lsblk)
#include "debugobj.h"

struct lsblk {
	struct libscols_table *table;	
	struct libscols_column *sort_col;

	int sort_id;			
	int tree_id;			

	int dedup_id;


	const char *sysroot;
	int flags;			

	unsigned int all_devices:1;	
	unsigned int bytes:1;		
	unsigned int inverse:1;		
	unsigned int merge:1;           
	unsigned int nodeps:1;		
	unsigned int scsi:1;		
	unsigned int nvme:1;		
	unsigned int virtio:1;		
	unsigned int paths:1;		
	unsigned int sort_hidden:1;	
	unsigned int dedup_hidden :1;	
	unsigned int force_tree_order:1;
	unsigned int noempty:1;		
};

extern struct lsblk *lsblk;     

struct lsblk_devprop {
	
	char *fstype;		
	char *fsversion;	
	char *uuid;		
	char *ptuuid;		
	char *pttype;		
	char *label;		
	char *parttype;		
	char *partuuid;		
	char *partlabel;	
	char *partflags;	
	char *partn;		
	char *wwn;		
	char *serial;		
	char *model;		
	char *idlink;		
	char *revision;		

	
	char *owner;		
	char *group;		
	char *mode;		
};


struct lsblk_devdep {
	struct list_head        ls_childs;	
	struct list_head	ls_parents;	

	struct lsblk_device	*child;
	struct lsblk_device	*parent;
};

struct lsblk_device {
	int	refcount;

	struct list_head	childs;		
	struct list_head	parents;
	struct list_head	ls_roots;	
	struct list_head	ls_devices;	

	struct lsblk_device	*wholedisk;	

	struct libscols_line	*scols_line;

	struct lsblk_devprop	*properties;
	struct stat	st;

	char *name;		
	char *dm_name;		

	char *filename;		
	char *dedupkey;		

	struct path_cxt	*sysfs;

	struct libmnt_fs **fss;	
	size_t nfss;		

	struct statvfs fsstat;	

	int npartitions;	
	int nholders;		
	int nslaves;		
	int maj, min;		

	uint64_t discard_granularity;	

	uint64_t size;		
	int removable;		

	unsigned int	is_mounted : 1,
			is_swap : 1,
			is_printed : 1,
			udev_requested : 1,
			blkid_requested : 1,
			file_requested : 1;
};

#define device_is_partition(_x)		((_x)->wholedisk != NULL)


struct lsblk_devnomap {
	dev_t slave;		
	dev_t holder;		

	struct list_head ls_devnomap;
};



struct lsblk_devtree {
	int	refcount;

	struct list_head	roots;		
	struct list_head	devices;	
	struct list_head	pktcdvd_map;	

	unsigned int	is_inverse : 1,		
			pktcdvd_read : 1;
};



struct lsblk_iter {
	struct list_head        *p;		
	struct list_head        *head;		
	int			direction;	
};

#define LSBLK_ITER_FORWARD	0
#define LSBLK_ITER_BACKWARD	1

#define IS_ITER_FORWARD(_i)	((_i)->direction == LSBLK_ITER_FORWARD)
#define IS_ITER_BACKWARD(_i)	((_i)->direction == LSBLK_ITER_BACKWARD)

#define LSBLK_ITER_INIT(itr, list) \
	do { \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(list)->next : (list)->prev; \
		(itr)->head = (list); \
	} while(0)

#define LSBLK_ITER_ITERATE(itr, res, restype, member) \
	do { \
		res = list_entry((itr)->p, restype, member); \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(itr)->p->next : (itr)->p->prev; \
	} while(0)



extern void lsblk_mnt_init(void);
extern void lsblk_mnt_deinit(void);

extern void lsblk_device_free_filesystems(struct lsblk_device *dev);
extern const char *lsblk_device_get_mountpoint(struct lsblk_device *dev);
extern struct libmnt_fs **lsblk_device_get_filesystems(struct lsblk_device *dev, size_t *n);


extern void lsblk_device_free_properties(struct lsblk_devprop *p);
extern struct lsblk_devprop *lsblk_device_get_properties(struct lsblk_device *dev);
extern void lsblk_properties_deinit(void);

extern const char *lsblk_parttype_code_to_string(const char *code, const char *pttype);


void lsblk_reset_iter(struct lsblk_iter *itr, int direction);
struct lsblk_device *lsblk_new_device(void);
void lsblk_ref_device(struct lsblk_device *dev);
void lsblk_unref_device(struct lsblk_device *dev);
int lsblk_device_new_dependence(struct lsblk_device *parent, struct lsblk_device *child);
int lsblk_device_has_child(struct lsblk_device *dev, struct lsblk_device *child);
int lsblk_device_next_child(struct lsblk_device *dev,
                          struct lsblk_iter *itr,
                          struct lsblk_device **child);

dev_t lsblk_devtree_pktcdvd_get_mate(struct lsblk_devtree *tr, dev_t devno, int is_slave);

int lsblk_device_is_last_parent(struct lsblk_device *dev, struct lsblk_device *parent);
int lsblk_device_next_parent(
                        struct lsblk_device *dev,
                        struct lsblk_iter *itr,
                        struct lsblk_device **parent);

struct lsblk_devtree *lsblk_new_devtree(void);
void lsblk_ref_devtree(struct lsblk_devtree *tr);
void lsblk_unref_devtree(struct lsblk_devtree *tr);
int lsblk_devtree_add_root(struct lsblk_devtree *tr, struct lsblk_device *dev);
int lsblk_devtree_remove_root(struct lsblk_devtree *tr, struct lsblk_device *dev);
int lsblk_devtree_next_root(struct lsblk_devtree *tr,
                            struct lsblk_iter *itr,
                            struct lsblk_device **dev);
int lsblk_devtree_add_device(struct lsblk_devtree *tr, struct lsblk_device *dev);
int lsblk_devtree_next_device(struct lsblk_devtree *tr,
                            struct lsblk_iter *itr,
                            struct lsblk_device **dev);
int lsblk_devtree_has_device(struct lsblk_devtree *tr, struct lsblk_device *dev);
struct lsblk_device *lsblk_devtree_get_device(struct lsblk_devtree *tr, const char *name);
int lsblk_devtree_remove_device(struct lsblk_devtree *tr, struct lsblk_device *dev);
int lsblk_devtree_deduplicate_devices(struct lsblk_devtree *tr);

#endif 
