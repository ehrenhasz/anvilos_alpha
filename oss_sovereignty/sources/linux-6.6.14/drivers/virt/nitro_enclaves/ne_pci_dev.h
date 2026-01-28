


#ifndef _NE_PCI_DEV_H_
#define _NE_PCI_DEV_H_

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/wait.h>




#define PCI_DEVICE_ID_NE	(0xe4c1)

#define PCI_BAR_NE		(0x03)




#define NE_ENABLE		(0x0000)
#define NE_ENABLE_OFF		(0x00)
#define NE_ENABLE_ON		(0x01)


#define NE_VERSION		(0x0002)
#define NE_VERSION_MAX		(0x0001)


#define NE_COMMAND		(0x0004)


#define NE_EVTCNT		(0x000c)
#define NE_EVTCNT_REPLY_SHIFT	(0)
#define NE_EVTCNT_REPLY_MASK	(0x0000ffff)
#define NE_EVTCNT_REPLY(cnt)	(((cnt) & NE_EVTCNT_REPLY_MASK) >> \
				NE_EVTCNT_REPLY_SHIFT)
#define NE_EVTCNT_EVENT_SHIFT	(16)
#define NE_EVTCNT_EVENT_MASK	(0xffff0000)
#define NE_EVTCNT_EVENT(cnt)	(((cnt) & NE_EVTCNT_EVENT_MASK) >> \
				NE_EVTCNT_EVENT_SHIFT)


#define NE_SEND_DATA		(0x0010)


#define NE_RECV_DATA		(0x0100)




#define NE_SEND_DATA_SIZE	(240)


#define NE_RECV_DATA_SIZE	(240)




#define NE_VEC_REPLY		(0)


#define NE_VEC_EVENT		(1)


enum ne_pci_dev_cmd_type {
	INVALID_CMD		= 0,
	ENCLAVE_START		= 1,
	ENCLAVE_GET_SLOT	= 2,
	ENCLAVE_STOP		= 3,
	SLOT_ALLOC		= 4,
	SLOT_FREE		= 5,
	SLOT_ADD_MEM		= 6,
	SLOT_ADD_VCPU		= 7,
	SLOT_COUNT		= 8,
	NEXT_SLOT		= 9,
	SLOT_INFO		= 10,
	SLOT_ADD_BULK_VCPUS	= 11,
	MAX_CMD,
};




struct enclave_start_req {
	u64	slot_uid;
	u64	enclave_cid;
	u64	flags;
};


struct enclave_get_slot_req {
	u64	enclave_cid;
};


struct enclave_stop_req {
	u64	slot_uid;
};


struct slot_alloc_req {
	u8	unused;
};


struct slot_free_req {
	u64	slot_uid;
};



struct slot_add_mem_req {
	u64	slot_uid;
	u64	paddr;
	u64	size;
};


struct slot_add_vcpu_req {
	u64	slot_uid;
	u32	vcpu_id;
	u8	padding[4];
};


struct slot_count_req {
	u8	unused;
};


struct next_slot_req {
	u64	slot_uid;
};


struct slot_info_req {
	u64	slot_uid;
};


struct slot_add_bulk_vcpus_req {
	u64	slot_uid;
	u64	nr_vcpus;
};


struct ne_pci_dev_cmd_reply {
	s32	rc;
	u8	padding0[4];
	u64	slot_uid;
	u64	enclave_cid;
	u64	slot_count;
	u64	mem_regions;
	u64	mem_size;
	u64	nr_vcpus;
	u64	flags;
	u16	state;
	u8	padding1[6];
};


struct ne_pci_dev {
	atomic_t		cmd_reply_avail;
	wait_queue_head_t	cmd_reply_wait_q;
	struct list_head	enclaves_list;
	struct mutex		enclaves_list_mutex;
	struct workqueue_struct	*event_wq;
	void __iomem		*iomem_base;
	struct work_struct	notify_work;
	struct mutex		pci_dev_mutex;
	struct pci_dev		*pdev;
};


int ne_do_request(struct pci_dev *pdev, enum ne_pci_dev_cmd_type cmd_type,
		  void *cmd_request, size_t cmd_request_size,
		  struct ne_pci_dev_cmd_reply *cmd_reply,
		  size_t cmd_reply_size);


extern struct pci_driver ne_pci_driver;

#endif 
