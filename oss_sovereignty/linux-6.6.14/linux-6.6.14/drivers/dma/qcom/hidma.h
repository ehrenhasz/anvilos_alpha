#ifndef QCOM_HIDMA_H
#define QCOM_HIDMA_H
#include <linux/kfifo.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
#define HIDMA_TRE_SIZE			32  
#define HIDMA_TRE_CFG_IDX		0
#define HIDMA_TRE_LEN_IDX		1
#define HIDMA_TRE_SRC_LOW_IDX		2
#define HIDMA_TRE_SRC_HI_IDX		3
#define HIDMA_TRE_DEST_LOW_IDX		4
#define HIDMA_TRE_DEST_HI_IDX		5
enum tre_type {
	HIDMA_TRE_MEMCPY = 3,
	HIDMA_TRE_MEMSET = 4,
};
struct hidma_tre {
	atomic_t allocated;		 
	bool queued;			 
	u16 status;			 
	u32 idx;			 
	u32 dma_sig;			 
	const char *dev_name;		 
	void (*callback)(void *data);	 
	void *data;			 
	struct hidma_lldev *lldev;	 
	u32 tre_local[HIDMA_TRE_SIZE / sizeof(u32) + 1];  
	u32 tre_index;			 
	u32 int_flags;			 
	u8 err_info;			 
	u8 err_code;			 
};
struct hidma_lldev {
	bool msi_support;		 
	bool initialized;		 
	u8 trch_state;			 
	u8 evch_state;			 
	u8 chidx;			 
	u32 nr_tres;			 
	spinlock_t lock;		 
	struct hidma_tre *trepool;	 
	struct device *dev;		 
	void __iomem *trca;		 
	void __iomem *evca;		 
	struct hidma_tre
		**pending_tre_list;	 
	atomic_t pending_tre_count;	 
	void *tre_ring;			 
	dma_addr_t tre_dma;		 
	u32 tre_ring_size;		 
	u32 tre_processed_off;		 
	void *evre_ring;		 
	dma_addr_t evre_dma;		 
	u32 evre_ring_size;		 
	u32 evre_processed_off;		 
	u32 tre_write_offset;            
	struct tasklet_struct task;	 
	DECLARE_KFIFO_PTR(handoff_fifo,
		struct hidma_tre *);     
};
struct hidma_desc {
	struct dma_async_tx_descriptor	desc;
	struct list_head		node;
	u32				tre_ch;
};
struct hidma_chan {
	bool				paused;
	bool				allocated;
	char				dbg_name[16];
	u32				dma_sig;
	dma_cookie_t			last_success;
	struct hidma_dev		*dmadev;
	struct hidma_desc		*running;
	struct dma_chan			chan;
	struct list_head		free;
	struct list_head		prepared;
	struct list_head		queued;
	struct list_head		active;
	struct list_head		completed;
	spinlock_t			lock;
};
struct hidma_dev {
	int				irq;
	int				chidx;
	u32				nr_descriptors;
	int				msi_virqbase;
	struct hidma_lldev		*lldev;
	void				__iomem *dev_trca;
	struct resource			*trca_resource;
	void				__iomem *dev_evca;
	struct resource			*evca_resource;
	spinlock_t			lock;
	struct dma_device		ddev;
	struct dentry			*debugfs;
	struct device_attribute		*chid_attrs;
	struct tasklet_struct		task;
};
int hidma_ll_request(struct hidma_lldev *llhndl, u32 dev_id,
			const char *dev_name,
			void (*callback)(void *data), void *data, u32 *tre_ch);
void hidma_ll_free(struct hidma_lldev *llhndl, u32 tre_ch);
enum dma_status hidma_ll_status(struct hidma_lldev *llhndl, u32 tre_ch);
bool hidma_ll_isenabled(struct hidma_lldev *llhndl);
void hidma_ll_queue_request(struct hidma_lldev *llhndl, u32 tre_ch);
void hidma_ll_start(struct hidma_lldev *llhndl);
int hidma_ll_disable(struct hidma_lldev *lldev);
int hidma_ll_enable(struct hidma_lldev *llhndl);
void hidma_ll_set_transfer_params(struct hidma_lldev *llhndl, u32 tre_ch,
	dma_addr_t src, dma_addr_t dest, u32 len, u32 flags, u32 txntype);
void hidma_ll_setup_irq(struct hidma_lldev *lldev, bool msi);
int hidma_ll_setup(struct hidma_lldev *lldev);
struct hidma_lldev *hidma_ll_init(struct device *dev, u32 max_channels,
			void __iomem *trca, void __iomem *evca,
			u8 chidx);
int hidma_ll_uninit(struct hidma_lldev *llhndl);
irqreturn_t hidma_ll_inthandler(int irq, void *arg);
irqreturn_t hidma_ll_inthandler_msi(int irq, void *arg, int cause);
void hidma_cleanup_pending_tre(struct hidma_lldev *llhndl, u8 err_info,
				u8 err_code);
void hidma_debug_init(struct hidma_dev *dmadev);
void hidma_debug_uninit(struct hidma_dev *dmadev);
#endif
