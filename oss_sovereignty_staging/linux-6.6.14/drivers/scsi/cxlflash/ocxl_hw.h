 
 

#define OCXL_MAX_IRQS	4	 

struct ocxlflash_irqs {
	int hwirq;
	u32 virq;
	void __iomem *vtrig;
};

 
struct ocxl_hw_afu {
	struct ocxlflash_context *ocxl_ctx;  
	struct pci_dev *pdev;		 
	struct device *dev;		 
	bool perst_same_image;		 

	struct ocxl_fn_config fcfg;	 
	struct ocxl_afu_config acfg;	 

	int fn_actag_base;		 
	int fn_actag_enabled;		 
	int afu_actag_base;		 
	int afu_actag_enabled;		 

	phys_addr_t ppmmio_phys;	 
	phys_addr_t gmmio_phys;		 
	void __iomem *gmmio_virt;	 

	void *link_token;		 
	struct idr idr;			 
	int max_pasid;			 
	bool is_present;		 
};

enum ocxlflash_ctx_state {
	CLOSED,
	OPENED,
	STARTED
};

struct ocxlflash_context {
	struct ocxl_hw_afu *hw_afu;	 
	struct address_space *mapping;	 
	bool master;			 
	int pe;				 

	phys_addr_t psn_phys;		 
	u64 psn_size;			 

	spinlock_t slock;		 
	wait_queue_head_t wq;		 
	struct mutex state_mutex;	 
	enum ocxlflash_ctx_state state;	 

	struct ocxlflash_irqs *irqs;	 
	int num_irqs;			 
	bool pending_irq;		 
	ulong irq_bitmap;		 

	u64 fault_addr;			 
	u64 fault_dsisr;		 
	bool pending_fault;		 
};
