


#ifndef _LM_H_
#define _LM_H_

#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/types.h>

#include <linux/pds/pds_common.h>
#include <linux/pds/pds_adminq.h>

struct pds_vfio_lm_file {
	struct file *filep;
	struct mutex lock;	
	u64 size;		
	u64 alloc_size;		
	void *page_mem;		
	struct page **pages;	
	unsigned long long npages;
	struct sg_table sg_table;	
	struct pds_lm_sg_elem *sgl;	
	dma_addr_t sgl_addr;
	u16 num_sge;
	struct scatterlist *last_offset_sg;	
	unsigned int sg_last_entry;
	unsigned long last_offset;
};

struct pds_vfio_pci_device;

struct file *
pds_vfio_step_device_state_locked(struct pds_vfio_pci_device *pds_vfio,
				  enum vfio_device_mig_state next);

void pds_vfio_put_save_file(struct pds_vfio_pci_device *pds_vfio);
void pds_vfio_put_restore_file(struct pds_vfio_pci_device *pds_vfio);

#endif 
