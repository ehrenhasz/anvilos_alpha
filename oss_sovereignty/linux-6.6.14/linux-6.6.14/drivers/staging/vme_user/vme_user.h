#ifndef _VME_USER_H_
#define _VME_USER_H_
#define VME_USER_BUS_MAX	1
struct vme_master {
	__u32 enable;		 
	__u64 vme_addr;		 
	__u64 size;		 
	__u32 aspace;		 
	__u32 cycle;		 
	__u32 dwidth;		 
#if 0
	char prefetchenable;		 
	int prefetchsize;		 
	char wrpostenable;		 
#endif
} __packed;
#define VME_IOC_MAGIC 0xAE
struct vme_slave {
	__u32 enable;		 
	__u64 vme_addr;		 
	__u64 size;		 
	__u32 aspace;		 
	__u32 cycle;		 
#if 0
	char wrpostenable;		 
	char rmwlock;			 
	char data64bitcapable;		 
#endif
} __packed;
struct vme_irq_id {
	__u8 level;
	__u8 statid;
};
#define VME_GET_SLAVE _IOR(VME_IOC_MAGIC, 1, struct vme_slave)
#define VME_SET_SLAVE _IOW(VME_IOC_MAGIC, 2, struct vme_slave)
#define VME_GET_MASTER _IOR(VME_IOC_MAGIC, 3, struct vme_master)
#define VME_SET_MASTER _IOW(VME_IOC_MAGIC, 4, struct vme_master)
#define VME_IRQ_GEN _IOW(VME_IOC_MAGIC, 5, struct vme_irq_id)
#endif  
