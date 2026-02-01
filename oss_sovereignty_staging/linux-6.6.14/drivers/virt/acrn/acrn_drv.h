 

#ifndef __ACRN_HSM_DRV_H
#define __ACRN_HSM_DRV_H

#include <linux/acrn.h>
#include <linux/dev_printk.h>
#include <linux/miscdevice.h>
#include <linux/types.h>

#include "hypercall.h"

extern struct miscdevice acrn_dev;

#define ACRN_NAME_LEN		16
#define ACRN_MEM_MAPPING_MAX	256

#define ACRN_MEM_REGION_ADD	0
#define ACRN_MEM_REGION_DEL	2

struct acrn_vm;
struct acrn_ioreq_client;

 
struct vm_memory_region_op {
	u32	type;
	u32	attr;
	u64	user_vm_pa;
	u64	service_vm_pa;
	u64	size;
};

 
struct vm_memory_region_batch {
	u16			   vmid;
	u16			   reserved[3];
	u32			   regions_num;
	u64			   regions_gpa;
	struct vm_memory_region_op regions_op[];
};

 
struct vm_memory_mapping {
	struct page	**pages;
	int		npages;
	void		*service_vm_va;
	u64		user_vm_pa;
	size_t		size;
};

 
struct acrn_ioreq_buffer {
	u64	ioreq_buf;
};

struct acrn_ioreq_range {
	struct list_head	list;
	u32			type;
	u64			start;
	u64			end;
};

#define ACRN_IOREQ_CLIENT_DESTROYING	0U
typedef	int (*ioreq_handler_t)(struct acrn_ioreq_client *client,
			       struct acrn_io_request *req);
 
struct acrn_ioreq_client {
	char			name[ACRN_NAME_LEN];
	struct acrn_vm		*vm;
	struct list_head	list;
	bool			is_default;
	unsigned long		flags;
	struct list_head	range_list;
	rwlock_t		range_lock;
	DECLARE_BITMAP(ioreqs_map, ACRN_IO_REQUEST_MAX);
	ioreq_handler_t		handler;
	struct task_struct	*thread;
	wait_queue_head_t	wq;
	void			*priv;
};

#define ACRN_INVALID_VMID (0xffffU)

#define ACRN_VM_FLAG_DESTROYED		0U
#define ACRN_VM_FLAG_CLEARING_IOREQ	1U
extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;
 
struct acrn_vm {
	struct list_head		list;
	u16				vmid;
	int				vcpu_num;
	unsigned long			flags;
	struct mutex			regions_mapping_lock;
	struct vm_memory_mapping	regions_mapping[ACRN_MEM_MAPPING_MAX];
	int				regions_mapping_count;
	spinlock_t			ioreq_clients_lock;
	struct list_head		ioreq_clients;
	struct acrn_ioreq_client	*default_client;
	struct acrn_io_request_buffer	*ioreq_buf;
	struct page			*ioreq_page;
	u32				pci_conf_addr;
	struct page			*monitor_page;
	struct mutex			ioeventfds_lock;
	struct list_head		ioeventfds;
	struct acrn_ioreq_client	*ioeventfd_client;
	struct mutex			irqfds_lock;
	struct list_head		irqfds;
	struct workqueue_struct		*irqfd_wq;
};

struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
			       struct acrn_vm_creation *vm_param);
int acrn_vm_destroy(struct acrn_vm *vm);
int acrn_mm_region_add(struct acrn_vm *vm, u64 user_gpa, u64 service_gpa,
		       u64 size, u32 mem_type, u32 mem_access_right);
int acrn_mm_region_del(struct acrn_vm *vm, u64 user_gpa, u64 size);
int acrn_vm_memseg_map(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
int acrn_vm_memseg_unmap(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
int acrn_vm_ram_map(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
void acrn_vm_all_ram_unmap(struct acrn_vm *vm);

int acrn_ioreq_init(struct acrn_vm *vm, u64 buf_vma);
void acrn_ioreq_deinit(struct acrn_vm *vm);
int acrn_ioreq_intr_setup(void);
void acrn_ioreq_intr_remove(void);
void acrn_ioreq_request_clear(struct acrn_vm *vm);
int acrn_ioreq_client_wait(struct acrn_ioreq_client *client);
int acrn_ioreq_request_default_complete(struct acrn_vm *vm, u16 vcpu);
struct acrn_ioreq_client *acrn_ioreq_client_create(struct acrn_vm *vm,
						   ioreq_handler_t handler,
						   void *data, bool is_default,
						   const char *name);
void acrn_ioreq_client_destroy(struct acrn_ioreq_client *client);
int acrn_ioreq_range_add(struct acrn_ioreq_client *client,
			 u32 type, u64 start, u64 end);
void acrn_ioreq_range_del(struct acrn_ioreq_client *client,
			  u32 type, u64 start, u64 end);

int acrn_msi_inject(struct acrn_vm *vm, u64 msi_addr, u64 msi_data);

int acrn_ioeventfd_init(struct acrn_vm *vm);
int acrn_ioeventfd_config(struct acrn_vm *vm, struct acrn_ioeventfd *args);
void acrn_ioeventfd_deinit(struct acrn_vm *vm);

int acrn_irqfd_init(struct acrn_vm *vm);
int acrn_irqfd_config(struct acrn_vm *vm, struct acrn_irqfd *args);
void acrn_irqfd_deinit(struct acrn_vm *vm);

#endif  
