#ifndef __CARD_BASE_H__
#define __CARD_BASE_H__
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/stringify.h>
#include <linux/pci.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/genwqe/genwqe_card.h>
#include "genwqe_driver.h"
#define GENWQE_MSI_IRQS			4   
#define GENWQE_MAX_VFS			15  
#define GENWQE_MAX_FUNCS		16  
#define GENWQE_CARD_NO_MAX		(16 * GENWQE_MAX_FUNCS)
#define GENWQE_DDCB_MAX			32  
#define GENWQE_POLLING_ENABLED		0   
#define GENWQE_DDCB_SOFTWARE_TIMEOUT	10  
#define GENWQE_KILL_TIMEOUT		8   
#define GENWQE_VF_JOBTIMEOUT_MSEC	250   
#define GENWQE_PF_JOBTIMEOUT_MSEC	8000  
#define GENWQE_HEALTH_CHECK_INTERVAL	4  
extern const struct attribute_group *genwqe_attribute_groups[];
#define PCI_DEVICE_GENWQE		0x044b  
#define PCI_SUBSYSTEM_ID_GENWQE5	0x035f  
#define PCI_SUBSYSTEM_ID_GENWQE5_NEW	0x044b  
#define PCI_CLASSCODE_GENWQE5		0x1200  
#define PCI_SUBVENDOR_ID_IBM_SRIOV	0x0000
#define PCI_SUBSYSTEM_ID_GENWQE5_SRIOV	0x0000  
#define PCI_CLASSCODE_GENWQE5_SRIOV	0x1200  
#define	GENWQE_SLU_ARCH_REQ		2  
struct genwqe_reg {
	u32 addr;
	u32 idx;
	u64 val;
};
enum genwqe_dbg_type {
	GENWQE_DBG_UNIT0 = 0,   
	GENWQE_DBG_UNIT1 = 1,
	GENWQE_DBG_UNIT2 = 2,
	GENWQE_DBG_UNIT3 = 3,
	GENWQE_DBG_UNIT4 = 4,
	GENWQE_DBG_UNIT5 = 5,
	GENWQE_DBG_UNIT6 = 6,
	GENWQE_DBG_UNIT7 = 7,
	GENWQE_DBG_REGS  = 8,
	GENWQE_DBG_DMA   = 9,
	GENWQE_DBG_UNITS = 10,  
};
#define GENWQE_INJECT_HARDWARE_FAILURE	0x00000001  
#define GENWQE_INJECT_BUS_RESET_FAILURE 0x00000002  
#define GENWQE_INJECT_GFIR_FATAL	0x00000004  
#define GENWQE_INJECT_GFIR_INFO		0x00000008  
enum dma_mapping_type {
	GENWQE_MAPPING_RAW = 0,		 
	GENWQE_MAPPING_SGL_TEMP,	 
	GENWQE_MAPPING_SGL_PINNED,	 
};
struct dma_mapping {
	enum dma_mapping_type type;
	void *u_vaddr;			 
	void *k_vaddr;			 
	dma_addr_t dma_addr;		 
	struct page **page_list;	 
	dma_addr_t *dma_list;		 
	unsigned int nr_pages;		 
	unsigned int size;		 
	struct list_head card_list;	 
	struct list_head pin_list;	 
	int write;			 
};
static inline void genwqe_mapping_init(struct dma_mapping *m,
				       enum dma_mapping_type type)
{
	memset(m, 0, sizeof(*m));
	m->type = type;
	m->write = 1;  
}
struct ddcb_queue {
	int ddcb_max;			 
	int ddcb_next;			 
	int ddcb_act;			 
	u16 ddcb_seq;			 
	unsigned int ddcbs_in_flight;	 
	unsigned int ddcbs_completed;
	unsigned int ddcbs_max_in_flight;
	unsigned int return_on_busy;     
	unsigned int wait_on_busy;
	dma_addr_t ddcb_daddr;		 
	struct ddcb *ddcb_vaddr;	 
	struct ddcb_requ **ddcb_req;	 
	wait_queue_head_t *ddcb_waitqs;  
	spinlock_t ddcb_lock;		 
	wait_queue_head_t busy_waitq;    
	u32 IO_QUEUE_CONFIG;
	u32 IO_QUEUE_STATUS;
	u32 IO_QUEUE_SEGMENT;
	u32 IO_QUEUE_INITSQN;
	u32 IO_QUEUE_WRAP;
	u32 IO_QUEUE_OFFSET;
	u32 IO_QUEUE_WTIME;
	u32 IO_QUEUE_ERRCNTS;
	u32 IO_QUEUE_LRW;
};
#define GENWQE_FFDC_REGS	(3 + (8 * (2 + 2 * 64)))
struct genwqe_ffdc {
	unsigned int entries;
	struct genwqe_reg *regs;
};
struct genwqe_dev {
	enum genwqe_card_state card_state;
	spinlock_t print_lock;
	int card_idx;			 
	u64 flags;			 
	struct genwqe_ffdc ffdc[GENWQE_DBG_UNITS];
	struct task_struct *card_thread;
	wait_queue_head_t queue_waitq;
	struct ddcb_queue queue;	 
	unsigned int irqs_processed;
	struct task_struct *health_thread;
	wait_queue_head_t health_waitq;
	int use_platform_recovery;	 
	dev_t  devnum_genwqe;		 
	const struct class *class_genwqe;	 
	struct device *dev;		 
	struct cdev cdev_genwqe;	 
	struct dentry *debugfs_root;	 
	struct dentry *debugfs_genwqe;	 
	struct pci_dev *pci_dev;	 
	void __iomem *mmio;		 
	unsigned long mmio_len;
	int num_vfs;
	u32 vf_jobtimeout_msec[GENWQE_MAX_VFS];
	int is_privileged;		 
	u64 slu_unitcfg;
	u64 app_unitcfg;
	u64 softreset;
	u64 err_inject;
	u64 last_gfir;
	char app_name[5];
	spinlock_t file_lock;		 
	struct list_head file_list;	 
	int ddcb_software_timeout;	 
	int skip_recovery;		 
	int kill_timeout;		 
};
enum genwqe_requ_state {
	GENWQE_REQU_NEW      = 0,
	GENWQE_REQU_ENQUEUED = 1,
	GENWQE_REQU_TAPPED   = 2,
	GENWQE_REQU_FINISHED = 3,
	GENWQE_REQU_STATE_MAX,
};
struct genwqe_sgl {
	dma_addr_t sgl_dma_addr;
	struct sg_entry *sgl;
	size_t sgl_size;	 
	void __user *user_addr;  
	size_t user_size;        
	int write;
	unsigned long nr_pages;
	unsigned long fpage_offs;
	size_t fpage_size;
	size_t lpage_size;
	void *fpage;
	dma_addr_t fpage_dma_addr;
	void *lpage;
	dma_addr_t lpage_dma_addr;
};
int genwqe_alloc_sync_sgl(struct genwqe_dev *cd, struct genwqe_sgl *sgl,
			  void __user *user_addr, size_t user_size, int write);
int genwqe_setup_sgl(struct genwqe_dev *cd, struct genwqe_sgl *sgl,
		     dma_addr_t *dma_list);
int genwqe_free_sync_sgl(struct genwqe_dev *cd, struct genwqe_sgl *sgl);
struct ddcb_requ {
	enum genwqe_requ_state req_state;  
	int num;			   
	struct ddcb_queue *queue;	   
	struct dma_mapping  dma_mappings[DDCB_FIXUPS];
	struct genwqe_sgl sgls[DDCB_FIXUPS];
	struct genwqe_ddcb_cmd cmd;	 
	struct genwqe_debug_data debug_data;
};
struct genwqe_file {
	struct genwqe_dev *cd;
	struct genwqe_driver *client;
	struct file *filp;
	struct fasync_struct *async_queue;
	struct pid *opener;
	struct list_head list;		 
	spinlock_t map_lock;		 
	struct list_head map_list;	 
	spinlock_t pin_lock;		 
	struct list_head pin_list;	 
};
int  genwqe_setup_service_layer(struct genwqe_dev *cd);  
int  genwqe_finish_queue(struct genwqe_dev *cd);
int  genwqe_release_service_layer(struct genwqe_dev *cd);
static inline int genwqe_get_slu_id(struct genwqe_dev *cd)
{
	return (int)((cd->slu_unitcfg >> 32) & 0xff);
}
int  genwqe_ddcbs_in_flight(struct genwqe_dev *cd);
u8   genwqe_card_type(struct genwqe_dev *cd);
int  genwqe_card_reset(struct genwqe_dev *cd);
int  genwqe_set_interrupt_capability(struct genwqe_dev *cd, int count);
void genwqe_reset_interrupt_capability(struct genwqe_dev *cd);
int  genwqe_device_create(struct genwqe_dev *cd);
int  genwqe_device_remove(struct genwqe_dev *cd);
void genwqe_init_debugfs(struct genwqe_dev *cd);
void genqwe_exit_debugfs(struct genwqe_dev *cd);
int  genwqe_read_softreset(struct genwqe_dev *cd);
int  genwqe_recovery_on_fatal_gfir_required(struct genwqe_dev *cd);
int  genwqe_flash_readback_fails(struct genwqe_dev *cd);
int genwqe_write_vreg(struct genwqe_dev *cd, u32 reg, u64 val, int func);
u64 genwqe_read_vreg(struct genwqe_dev *cd, u32 reg, int func);
int  genwqe_ffdc_buff_size(struct genwqe_dev *cd, int unit_id);
int  genwqe_ffdc_buff_read(struct genwqe_dev *cd, int unit_id,
			   struct genwqe_reg *regs, unsigned int max_regs);
int  genwqe_read_ffdc_regs(struct genwqe_dev *cd, struct genwqe_reg *regs,
			   unsigned int max_regs, int all);
int  genwqe_ffdc_dump_dma(struct genwqe_dev *cd,
			  struct genwqe_reg *regs, unsigned int max_regs);
int  genwqe_init_debug_data(struct genwqe_dev *cd,
			    struct genwqe_debug_data *d);
void genwqe_init_crc32(void);
int  genwqe_read_app_id(struct genwqe_dev *cd, char *app_name, int len);
int  genwqe_user_vmap(struct genwqe_dev *cd, struct dma_mapping *m,
		      void *uaddr, unsigned long size);
int  genwqe_user_vunmap(struct genwqe_dev *cd, struct dma_mapping *m);
static inline bool dma_mapping_used(struct dma_mapping *m)
{
	if (!m)
		return false;
	return m->size != 0;
}
int  __genwqe_execute_ddcb(struct genwqe_dev *cd,
			   struct genwqe_ddcb_cmd *cmd, unsigned int f_flags);
int  __genwqe_execute_raw_ddcb(struct genwqe_dev *cd,
			       struct genwqe_ddcb_cmd *cmd,
			       unsigned int f_flags);
int  __genwqe_enqueue_ddcb(struct genwqe_dev *cd,
			   struct ddcb_requ *req,
			   unsigned int f_flags);
int  __genwqe_wait_ddcb(struct genwqe_dev *cd, struct ddcb_requ *req);
int  __genwqe_purge_ddcb(struct genwqe_dev *cd, struct ddcb_requ *req);
int __genwqe_writeq(struct genwqe_dev *cd, u64 byte_offs, u64 val);
u64 __genwqe_readq(struct genwqe_dev *cd, u64 byte_offs);
int __genwqe_writel(struct genwqe_dev *cd, u64 byte_offs, u32 val);
u32 __genwqe_readl(struct genwqe_dev *cd, u64 byte_offs);
void *__genwqe_alloc_consistent(struct genwqe_dev *cd, size_t size,
				 dma_addr_t *dma_handle);
void __genwqe_free_consistent(struct genwqe_dev *cd, size_t size,
			      void *vaddr, dma_addr_t dma_handle);
int  genwqe_base_clock_frequency(struct genwqe_dev *cd);
void genwqe_stop_traps(struct genwqe_dev *cd);
void genwqe_start_traps(struct genwqe_dev *cd);
bool genwqe_need_err_masking(struct genwqe_dev *cd);
static inline int genwqe_is_privileged(struct genwqe_dev *cd)
{
	return cd->is_privileged;
}
#endif	 
