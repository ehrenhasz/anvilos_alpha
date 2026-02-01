 
 

#ifndef	__HMM_H__
#define	__HMM_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "hmm_common.h"
#include "hmm/hmm_bo.h"
#include "ia_css_types.h"

#define mmgr_NULL              ((ia_css_ptr)0)
#define mmgr_EXCEPTION         ((ia_css_ptr) - 1)

int hmm_init(void);
void hmm_cleanup(void);

ia_css_ptr hmm_alloc(size_t bytes);
ia_css_ptr hmm_create_from_vmalloc_buf(size_t bytes, void *vmalloc_addr);

void hmm_free(ia_css_ptr ptr);
int hmm_load(ia_css_ptr virt, void *data, unsigned int bytes);
int hmm_store(ia_css_ptr virt, const void *data, unsigned int bytes);
int hmm_set(ia_css_ptr virt, int c, unsigned int bytes);
int hmm_flush(ia_css_ptr virt, unsigned int bytes);

 
phys_addr_t hmm_virt_to_phys(ia_css_ptr virt);

 
void *hmm_vmap(ia_css_ptr virt, bool cached);
void hmm_vunmap(ia_css_ptr virt);

 
void hmm_flush_vmap(ia_css_ptr virt);

 
int hmm_mmap(struct vm_area_struct *vma, ia_css_ptr virt);

extern struct hmm_bo_device bo_device;

#endif
