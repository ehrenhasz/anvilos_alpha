


#ifndef HCI_H
#define HCI_H



#define DBG(x, ...) pr_devel("%s: " x "\n", __func__, ##__VA_ARGS__)


#define W0_MASK(h, l)  GENMASK((h) - 0,  (l) - 0)
#define W1_MASK(h, l)  GENMASK((h) - 32, (l) - 32)
#define W2_MASK(h, l)  GENMASK((h) - 64, (l) - 64)
#define W3_MASK(h, l)  GENMASK((h) - 96, (l) - 96)


#define W0_BIT_(x)  BIT((x) - 0)
#define W1_BIT_(x)  BIT((x) - 32)
#define W2_BIT_(x)  BIT((x) - 64)
#define W3_BIT_(x)  BIT((x) - 96)


struct hci_cmd_ops;


struct i3c_hci {
	struct i3c_master_controller master;
	void __iomem *base_regs;
	void __iomem *DAT_regs;
	void __iomem *DCT_regs;
	void __iomem *RHS_regs;
	void __iomem *PIO_regs;
	void __iomem *EXTCAPS_regs;
	void __iomem *AUTOCMD_regs;
	void __iomem *DEBUG_regs;
	const struct hci_io_ops *io;
	void *io_data;
	const struct hci_cmd_ops *cmd;
	atomic_t next_cmd_tid;
	u32 caps;
	unsigned int quirks;
	unsigned int DAT_entries;
	unsigned int DAT_entry_size;
	void *DAT_data;
	unsigned int DCT_entries;
	unsigned int DCT_entry_size;
	u8 version_major;
	u8 version_minor;
	u8 revision;
	u32 vendor_mipi_id;
	u32 vendor_version_id;
	u32 vendor_product_id;
	void *vendor_data;
};



struct hci_xfer {
	u32 cmd_desc[4];
	u32 response;
	bool rnw;
	void *data;
	unsigned int data_len;
	unsigned int cmd_tid;
	struct completion *completion;
	union {
		struct {
			
			struct hci_xfer *next_xfer;
			struct hci_xfer *next_data;
			struct hci_xfer *next_resp;
			unsigned int data_left;
			u32 data_word_before_partial;
		};
		struct {
			
			dma_addr_t data_dma;
			int ring_number;
			int ring_entry;
		};
	};
};

static inline struct hci_xfer *hci_alloc_xfer(unsigned int n)
{
	return kcalloc(n, sizeof(struct hci_xfer), GFP_KERNEL);
}

static inline void hci_free_xfer(struct hci_xfer *xfer, unsigned int n)
{
	kfree(xfer);
}



struct hci_io_ops {
	bool (*irq_handler)(struct i3c_hci *hci, unsigned int mask);
	int (*queue_xfer)(struct i3c_hci *hci, struct hci_xfer *xfer, int n);
	bool (*dequeue_xfer)(struct i3c_hci *hci, struct hci_xfer *xfer, int n);
	int (*request_ibi)(struct i3c_hci *hci, struct i3c_dev_desc *dev,
			   const struct i3c_ibi_setup *req);
	void (*free_ibi)(struct i3c_hci *hci, struct i3c_dev_desc *dev);
	void (*recycle_ibi_slot)(struct i3c_hci *hci, struct i3c_dev_desc *dev,
				struct i3c_ibi_slot *slot);
	int (*init)(struct i3c_hci *hci);
	void (*cleanup)(struct i3c_hci *hci);
};

extern const struct hci_io_ops mipi_i3c_hci_pio;
extern const struct hci_io_ops mipi_i3c_hci_dma;



struct i3c_hci_dev_data {
	int dat_idx;
	void *ibi_data;
};



#define HCI_QUIRK_RAW_CCC	BIT(1)	



void mipi_i3c_hci_resume(struct i3c_hci *hci);
void mipi_i3c_hci_pio_reset(struct i3c_hci *hci);
void mipi_i3c_hci_dct_index_reset(struct i3c_hci *hci);

#endif
