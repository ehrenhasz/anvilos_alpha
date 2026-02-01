 
 

#ifndef _NE_MISC_DEV_H_
#define _NE_MISC_DEV_H_

#include <linux/cpumask.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/wait.h>

#include "ne_pci_dev.h"

 
struct ne_mem_region {
	struct list_head	mem_region_list_entry;
	u64			memory_size;
	unsigned long		nr_pages;
	struct page		**pages;
	u64			userspace_addr;
};

 
struct ne_enclave {
	struct mutex		enclave_info_mutex;
	struct list_head	enclave_list_entry;
	wait_queue_head_t	eventq;
	bool			has_event;
	u64			max_mem_regions;
	struct list_head	mem_regions_list;
	u64			mem_size;
	struct mm_struct	*mm;
	unsigned int		nr_mem_regions;
	unsigned int		nr_parent_vm_cores;
	unsigned int		nr_threads_per_core;
	unsigned int		nr_vcpus;
	int			numa_node;
	u64			slot_uid;
	u16			state;
	cpumask_var_t		*threads_per_core;
	cpumask_var_t		vcpu_ids;
};

 
enum ne_state {
	NE_STATE_INIT		= 0,
	NE_STATE_RUNNING	= 2,
	NE_STATE_STOPPED	= U16_MAX,
};

 
struct ne_devs {
	struct miscdevice	*ne_misc_dev;
	struct ne_pci_dev	*ne_pci_dev;
};

 
extern struct ne_devs ne_devs;

#endif  
