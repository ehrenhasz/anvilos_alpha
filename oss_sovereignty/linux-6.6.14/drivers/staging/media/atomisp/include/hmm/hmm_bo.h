 
 

#ifndef	__HMM_BO_H__
#define	__HMM_BO_H__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include "mmu/isp_mmu.h"
#include "hmm/hmm_common.h"
#include "ia_css_types.h"

#define	check_bodev_null_return(bdev, exp)	\
		check_null_return(bdev, exp, \
			"NULL hmm_bo_device.\n")

#define	check_bodev_null_return_void(bdev)	\
		check_null_return_void(bdev, \
			"NULL hmm_bo_device.\n")

#define	check_bo_status_yes_goto(bo, _status, label) \
	var_not_equal_goto((bo->status & (_status)), (_status), \
			label, \
			"HMM buffer status not contain %s.\n", \
			#_status)

#define	check_bo_status_no_goto(bo, _status, label) \
	var_equal_goto((bo->status & (_status)), (_status), \
			label, \
			"HMM buffer status contains %s.\n", \
			#_status)

#define rbtree_node_to_hmm_bo(root_node)	\
	container_of((root_node), struct hmm_buffer_object, node)

#define	list_to_hmm_bo(list_ptr)	\
	list_entry((list_ptr), struct hmm_buffer_object, list)

#define	kref_to_hmm_bo(kref_ptr)	\
	list_entry((kref_ptr), struct hmm_buffer_object, kref)

#define	check_bo_null_return(bo, exp)	\
	check_null_return(bo, exp, "NULL hmm buffer object.\n")

#define	check_bo_null_return_void(bo)	\
	check_null_return_void(bo, "NULL hmm buffer object.\n")

#define	ISP_VM_START	0x0
#define	ISP_VM_SIZE	(0x7FFFFFFF)	 
#define	ISP_PTR_NULL	NULL

#define	HMM_BO_DEVICE_INITED	0x1

enum hmm_bo_type {
	HMM_BO_PRIVATE,
	HMM_BO_VMALLOC,
	HMM_BO_LAST,
};

#define	HMM_BO_MASK		0x1
#define	HMM_BO_FREE		0x0
#define	HMM_BO_ALLOCED	0x1
#define	HMM_BO_PAGE_ALLOCED	0x2
#define	HMM_BO_BINDED		0x4
#define	HMM_BO_MMAPED		0x8
#define	HMM_BO_VMAPED		0x10
#define	HMM_BO_VMAPED_CACHED	0x20
#define	HMM_BO_ACTIVE		0x1000

struct hmm_bo_device {
	struct isp_mmu		mmu;

	 
	unsigned int start;
	unsigned int pgnr;
	unsigned int size;

	 
	spinlock_t	list_lock;
	int flag;

	 
	struct list_head entire_bo_list;
	 
	struct rb_root allocated_rbtree;
	 
	struct rb_root free_rbtree;
	struct mutex rbtree_mutex;
	struct kmem_cache *bo_cache;
};

struct hmm_buffer_object {
	struct hmm_bo_device	*bdev;
	struct list_head	list;
	struct kref	kref;

	struct page **pages;

	 
	struct mutex		mutex;
	enum hmm_bo_type	type;
	int		mmap_count;
	int		status;
	void		*vmap_addr;  

	struct rb_node	node;
	unsigned int	start;
	unsigned int	end;
	unsigned int	pgnr;
	 
	struct hmm_buffer_object	*prev;
	struct hmm_buffer_object	*next;
};

struct hmm_buffer_object *hmm_bo_alloc(struct hmm_bo_device *bdev,
				       unsigned int pgnr);

void hmm_bo_release(struct hmm_buffer_object *bo);

int hmm_bo_device_init(struct hmm_bo_device *bdev,
		       struct isp_mmu_client *mmu_driver,
		       unsigned int vaddr_start, unsigned int size);

 
void hmm_bo_device_exit(struct hmm_bo_device *bdev);

 
int hmm_bo_device_inited(struct hmm_bo_device *bdev);

 
void hmm_bo_ref(struct hmm_buffer_object *bo);

 
void hmm_bo_unref(struct hmm_buffer_object *bo);

int hmm_bo_allocated(struct hmm_buffer_object *bo);

 
int hmm_bo_alloc_pages(struct hmm_buffer_object *bo,
		       enum hmm_bo_type type,
		       void *vmalloc_addr);
void hmm_bo_free_pages(struct hmm_buffer_object *bo);
int hmm_bo_page_allocated(struct hmm_buffer_object *bo);

 
int hmm_bo_bind(struct hmm_buffer_object *bo);
void hmm_bo_unbind(struct hmm_buffer_object *bo);
int hmm_bo_binded(struct hmm_buffer_object *bo);

 
void *hmm_bo_vmap(struct hmm_buffer_object *bo, bool cached);

 
void hmm_bo_flush_vmap(struct hmm_buffer_object *bo);

 
void hmm_bo_vunmap(struct hmm_buffer_object *bo);

 
int hmm_bo_mmap(struct vm_area_struct *vma,
		struct hmm_buffer_object *bo);

 
struct hmm_buffer_object *hmm_bo_device_search_start(
    struct hmm_bo_device *bdev, ia_css_ptr vaddr);

 
struct hmm_buffer_object *hmm_bo_device_search_in_range(
    struct hmm_bo_device *bdev, ia_css_ptr vaddr);

 
struct hmm_buffer_object *hmm_bo_device_search_vmap_start(
    struct hmm_bo_device *bdev, const void *vaddr);

#endif
