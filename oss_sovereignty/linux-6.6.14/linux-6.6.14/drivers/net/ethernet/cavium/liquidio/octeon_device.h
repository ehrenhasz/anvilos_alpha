#ifndef _OCTEON_DEVICE_H_
#define  _OCTEON_DEVICE_H_
#include <linux/interrupt.h>
#include <net/devlink.h>
#define  OCTEON_CN68XX_PCIID          0x91177d
#define  OCTEON_CN66XX_PCIID          0x92177d
#define  OCTEON_CN23XX_PCIID_PF       0x9702177d
#define  OCTEON_CN68XX                0x0091
#define  OCTEON_CN66XX                0x0092
#define  OCTEON_CN23XX_PF_VID         0x9702
#define  OCTEON_CN23XX_VF_VID         0x9712
#define  OCTEON_CN23XX_REV_1_0        0x00
#define  OCTEON_CN23XX_REV_1_1        0x01
#define  OCTEON_CN23XX_REV_2_0        0x80
#define	 OCTEON_CN2350_10GB_SUBSYS_ID_1	0X3177d
#define	 OCTEON_CN2350_10GB_SUBSYS_ID_2	0X4177d
#define	 OCTEON_CN2360_10GB_SUBSYS_ID	0X5177d
#define	 OCTEON_CN2350_25GB_SUBSYS_ID	0X7177d
#define	 OCTEON_CN2360_25GB_SUBSYS_ID	0X6177d
enum octeon_pci_swap_mode {
	OCTEON_PCI_PASSTHROUGH = 0,
	OCTEON_PCI_64BIT_SWAP = 1,
	OCTEON_PCI_32BIT_BYTE_SWAP = 2,
	OCTEON_PCI_32BIT_LW_SWAP = 3
};
enum lio_fw_state {
	FW_IS_PRELOADED = 0,
	FW_NEEDS_TO_BE_LOADED = 1,
	FW_IS_BEING_LOADED = 2,
	FW_HAS_BEEN_LOADED = 3,
};
enum {
	OCTEON_CONFIG_TYPE_DEFAULT = 0,
	NUM_OCTEON_CONFS,
};
#define  OCTEON_INPUT_INTR    (1)
#define  OCTEON_OUTPUT_INTR   (2)
#define  OCTEON_MBOX_INTR     (4)
#define  OCTEON_ALL_INTR      0xff
#define    PCI_BAR1_ENABLE_CA            1
#define    PCI_BAR1_ENDIAN_MODE          OCTEON_PCI_64BIT_SWAP
#define    PCI_BAR1_ENTRY_VALID          1
#define    PCI_BAR1_MASK                 ((PCI_BAR1_ENABLE_CA << 3)   \
					    | (PCI_BAR1_ENDIAN_MODE << 1) \
					    | PCI_BAR1_ENTRY_VALID)
#define    OCT_DEV_BEGIN_STATE            0x0
#define    OCT_DEV_PCI_ENABLE_DONE        0x1
#define    OCT_DEV_PCI_MAP_DONE           0x2
#define    OCT_DEV_DISPATCH_INIT_DONE     0x3
#define    OCT_DEV_INSTR_QUEUE_INIT_DONE  0x4
#define    OCT_DEV_SC_BUFF_POOL_INIT_DONE 0x5
#define    OCT_DEV_RESP_LIST_INIT_DONE    0x6
#define    OCT_DEV_DROQ_INIT_DONE         0x7
#define    OCT_DEV_MBOX_SETUP_DONE        0x8
#define    OCT_DEV_MSIX_ALLOC_VECTOR_DONE 0x9
#define    OCT_DEV_INTR_SET_DONE          0xa
#define    OCT_DEV_IO_QUEUES_DONE         0xb
#define    OCT_DEV_CONSOLE_INIT_DONE      0xc
#define    OCT_DEV_HOST_OK                0xd
#define    OCT_DEV_CORE_OK                0xe
#define    OCT_DEV_RUNNING                0xf
#define    OCT_DEV_IN_RESET               0x10
#define    OCT_DEV_STATE_INVALID          0x11
#define    OCT_DEV_STATES                 OCT_DEV_STATE_INVALID
#define	   OCT_DEV_INTR_DMA0_FORCE	  0x01
#define	   OCT_DEV_INTR_DMA1_FORCE	  0x02
#define	   OCT_DEV_INTR_PKT_DATA	  0x04
#define LIO_RESET_SECS (3)
struct octeon_dispatch {
	struct list_head list;
	u16 opcode;
	octeon_dispatch_fn_t dispatch_fn;
	void *arg;
};
struct octeon_dispatch_list {
	spinlock_t lock;
	u32 count;
	struct octeon_dispatch *dlist;
};
#define OCT_MEM_REGIONS     3
struct octeon_mmio {
	u64 start;
	u32 len;
	u32 mapped_len;
	u8 __iomem *hw_addr;
	u32 done;
};
#define   MAX_OCTEON_MAPS    32
struct octeon_io_enable {
	u64 iq;
	u64 oq;
	u64 iq64B;
};
struct octeon_reg_list {
	u32 __iomem *pci_win_wr_addr_hi;
	u32 __iomem *pci_win_wr_addr_lo;
	u64 __iomem *pci_win_wr_addr;
	u32 __iomem *pci_win_rd_addr_hi;
	u32 __iomem *pci_win_rd_addr_lo;
	u64 __iomem *pci_win_rd_addr;
	u32 __iomem *pci_win_wr_data_hi;
	u32 __iomem *pci_win_wr_data_lo;
	u64 __iomem *pci_win_wr_data;
	u32 __iomem *pci_win_rd_data_hi;
	u32 __iomem *pci_win_rd_data_lo;
	u64 __iomem *pci_win_rd_data;
};
#define OCTEON_CONSOLE_MAX_READ_BYTES 512
typedef int (*octeon_console_print_fn)(struct octeon_device *oct,
				       u32 num, char *pre, char *suf);
struct octeon_console {
	u32 active;
	u32 waiting;
	u64 addr;
	u32 buffer_size;
	u64 input_base_addr;
	u64 output_base_addr;
	octeon_console_print_fn print;
	char leftover[OCTEON_CONSOLE_MAX_READ_BYTES];
};
struct octeon_board_info {
	char name[OCT_BOARD_NAME];
	char serial_number[OCT_SERIAL_LEN];
	u64 major;
	u64 minor;
};
struct octeon_fn_list {
	void (*setup_iq_regs)(struct octeon_device *, u32);
	void (*setup_oq_regs)(struct octeon_device *, u32);
	irqreturn_t (*process_interrupt_regs)(void *);
	u64 (*msix_interrupt_handler)(void *);
	int (*setup_mbox)(struct octeon_device *);
	int (*free_mbox)(struct octeon_device *);
	int (*soft_reset)(struct octeon_device *);
	int (*setup_device_regs)(struct octeon_device *);
	void (*bar1_idx_setup)(struct octeon_device *, u64, u32, int);
	void (*bar1_idx_write)(struct octeon_device *, u32, u32);
	u32 (*bar1_idx_read)(struct octeon_device *, u32);
	u32 (*update_iq_read_idx)(struct octeon_instr_queue *);
	void (*enable_oq_pkt_time_intr)(struct octeon_device *, u32);
	void (*disable_oq_pkt_time_intr)(struct octeon_device *, u32);
	void (*enable_interrupt)(struct octeon_device *, u8);
	void (*disable_interrupt)(struct octeon_device *, u8);
	int (*enable_io_queues)(struct octeon_device *);
	void (*disable_io_queues)(struct octeon_device *);
};
#define CVMX_BOOTMEM_NAME_LEN 128
struct cvmx_bootmem_named_block_desc {
	u64 base_addr;
	u64 size;
	char name[CVMX_BOOTMEM_NAME_LEN];
};
struct oct_fw_info {
	u32 max_nic_ports;       
	u32 num_gmx_ports;       
	u64 app_cap_flags;       
	u32 app_mode;
	char   liquidio_firmware_version[32];
	struct {
		u8  maj;
		u8  min;
		u8  rev;
	} ver;
};
#define OCT_FW_VER(maj, min, rev) \
	(((u32)(maj) << 16) | ((u32)(min) << 8) | ((u32)(rev)))
struct cavium_wk {
	struct delayed_work work;
	void *ctxptr;
	u64 ctxul;
};
struct cavium_wq {
	struct workqueue_struct *wq;
	struct cavium_wk wk;
};
struct octdev_props {
	int    rx_on;
	int    fec;
	int    fec_boot;
	int    napi_enabled;
	int    gmxport;
	struct net_device *netdev;
};
#define LIO_FLAG_MSIX_ENABLED	0x1
#define MSIX_PO_INT		0x1
#define MSIX_PI_INT		0x2
#define MSIX_MBOX_INT		0x4
struct octeon_pf_vf_hs_word {
#ifdef __LITTLE_ENDIAN_BITFIELD
	u64        pkind : 8;
	u64        core_tics_per_us : 16;
	u64        coproc_tics_per_us : 16;
	u64        app_mode : 8;
	u64 reserved : 16;
#else
	u64 reserved : 16;
	u64        app_mode : 8;
	u64        coproc_tics_per_us : 16;
	u64        core_tics_per_us : 16;
	u64        pkind : 8;
#endif
};
struct octeon_sriov_info {
	u32	rings_per_vf;
	u32	max_vfs;
	u32	num_vfs_alloced;
	u32	num_pf_rings;
	u32	pf_srn;
	u32	trs;
	u32	sriov_enabled;
	struct lio_trusted_vf	trusted_vf;
	struct pci_dev *dpiring_to_vfpcidev_lut[MAX_POSSIBLE_VFS];
	u64	vf_macaddr[MAX_POSSIBLE_VFS];
	u16	vf_vlantci[MAX_POSSIBLE_VFS];
	int	vf_linkstate[MAX_POSSIBLE_VFS];
	bool    vf_spoofchk[MAX_POSSIBLE_VFS];
	u64	vf_drv_loaded_mask;
};
struct octeon_ioq_vector {
	struct octeon_device   *oct_dev;
	int		        iq_index;
	int		        droq_index;
	int			vector;
	struct octeon_mbox     *mbox;
	struct cpumask		affinity_mask;
	u32			ioq_num;
};
struct lio_vf_rep_list {
	int num_vfs;
	struct net_device *ndev[CN23XX_MAX_VFS_PER_PF];
};
struct lio_devlink_priv {
	struct octeon_device *oct;
};
struct octeon_device {
	spinlock_t pci_win_lock;
	spinlock_t mem_access_lock;
	struct pci_dev *pci_dev;
	void *chip;
	u32 ifcount;
	struct octdev_props props[MAX_OCTEON_LINKS];
	u16 chip_id;
	u16 rev_id;
	u32 subsystem_id;
	u16 pf_num;
	u16 vf_num;
	u32 octeon_id;
	u16 pcie_port;
	u16 flags;
#define LIO_FLAG_MSI_ENABLED                  (u32)(1 << 1)
	atomic_t status;
	struct octeon_mmio mmio[OCT_MEM_REGIONS];
	struct octeon_reg_list reg_list;
	struct octeon_fn_list fn_list;
	struct octeon_board_info boardinfo;
	u32 num_iqs;
	struct octeon_sc_buffer_pool	sc_buf_pool;
	struct octeon_instr_queue *instr_queue
		[MAX_POSSIBLE_OCTEON_INSTR_QUEUES];
	struct octeon_response_list response_list[MAX_RESPONSE_LISTS];
	u32 num_oqs;
	struct octeon_droq *droq[MAX_POSSIBLE_OCTEON_OUTPUT_QUEUES];
	struct octeon_io_enable io_qmask;
	struct octeon_dispatch_list dispatch;
	u32 int_status;
	u64 droq_intr;
	u64 bootmem_desc_addr;
	struct cvmx_bootmem_named_block_desc bootmem_named_block_desc;
	u64 console_desc_addr;
	u32 num_consoles;
	struct octeon_console console[MAX_OCTEON_MAPS];
	struct {
		u64 dram_region_base;
		int bar1_index;
	} console_nb_info;
	u64 coproc_clock_rate;
	u32 app_mode;
	struct oct_fw_info fw_info;
	char device_name[32];
	void *app_ctx;
	struct cavium_wq dma_comp_wq;
	spinlock_t cmd_resp_wqlock;
	u32 cmd_resp_state;
	struct cavium_wq check_db_wq[MAX_POSSIBLE_OCTEON_INSTR_QUEUES];
	struct cavium_wk nic_poll_work;
	struct cavium_wk console_poll_work[MAX_OCTEON_MAPS];
	void *priv;
	int num_msix_irqs;
	void *msix_entries;
	void *irq_name_storage;
	struct octeon_sriov_info sriov_info;
	struct octeon_pf_vf_hs_word pfvf_hsword;
	int msix_on;
	struct octeon_mbox  *mbox[MAX_POSSIBLE_VFS];
	struct octeon_ioq_vector    *ioq_vector;
	int rx_pause;
	int tx_pause;
	struct oct_link_stats link_stats;  
	u32 priv_flags;
	void *watchdog_task;
	u32 rx_coalesce_usecs;
	u32 rx_max_coalesced_frames;
	u32 tx_max_coalesced_frames;
	bool cores_crashed;
	struct {
		int bus;
		int dev;
		int func;
	} loc;
	atomic_t *adapter_refcount;  
	atomic_t *adapter_fw_state;  
	bool ptp_enable;
	struct lio_vf_rep_list vf_rep_list;
	struct devlink *devlink;
	enum devlink_eswitch_mode eswitch_mode;
	u8  speed_boot;
	u8  speed_setting;
	u8  no_speed_setting;
	u32    vfstats_poll;
#define LIO_VFSTATS_POLL 10
};
#define  OCT_DRV_ONLINE 1
#define  OCT_DRV_OFFLINE 2
#define  OCTEON_CN6XXX(oct)	({					\
				 typeof(oct) _oct = (oct);		\
				 ((_oct->chip_id == OCTEON_CN66XX) ||	\
				  (_oct->chip_id == OCTEON_CN68XX));	})
#define  OCTEON_CN23XX_PF(oct)        ((oct)->chip_id == OCTEON_CN23XX_PF_VID)
#define  OCTEON_CN23XX_VF(oct)        ((oct)->chip_id == OCTEON_CN23XX_VF_VID)
#define CHIP_CONF(oct, TYPE)             \
	(((struct octeon_ ## TYPE  *)((oct)->chip))->conf)
#define MAX_IO_PENDING_PKT_COUNT 100
void octeon_init_device_list(int conf_type);
void octeon_free_device_mem(struct octeon_device *oct);
struct octeon_device *octeon_allocate_device(u32 pci_id,
					     u32 priv_size);
int octeon_register_device(struct octeon_device *oct,
			   int bus, int dev, int func, int is_pf);
int octeon_deregister_device(struct octeon_device *oct);
int octeon_init_dispatch_list(struct octeon_device *octeon_dev);
void octeon_delete_dispatch_list(struct octeon_device *octeon_dev);
int octeon_core_drv_init(struct octeon_recv_info *recv_info, void *buf);
octeon_dispatch_fn_t
octeon_get_dispatch(struct octeon_device *octeon_dev, u16 opcode,
		    u16 subcode);
struct octeon_device *lio_get_device(u32 octeon_id);
int lio_get_device_id(void *dev);
u64 lio_pci_readq(struct octeon_device *oct, u64 addr);
void lio_pci_writeq(struct octeon_device *oct, u64 val, u64 addr);
#define   octeon_write_csr(oct_dev, reg_off, value) \
		writel(value, (oct_dev)->mmio[0].hw_addr + (reg_off))
#define   octeon_write_csr64(oct_dev, reg_off, val64) \
		writeq(val64, (oct_dev)->mmio[0].hw_addr + (reg_off))
#define   octeon_read_csr(oct_dev, reg_off)         \
		readl((oct_dev)->mmio[0].hw_addr + (reg_off))
#define   octeon_read_csr64(oct_dev, reg_off)         \
		readq((oct_dev)->mmio[0].hw_addr + (reg_off))
int octeon_mem_access_ok(struct octeon_device *oct);
int octeon_wait_for_ddr_init(struct octeon_device *oct,
			     u32 *timeout_in_ms);
int octeon_wait_for_bootloader(struct octeon_device *oct,
			       u32 wait_time_hundredths);
int octeon_init_consoles(struct octeon_device *oct);
int octeon_add_console(struct octeon_device *oct, u32 console_num,
		       char *dbg_enb);
int octeon_console_write(struct octeon_device *oct, u32 console_num,
			 char *buffer, u32 write_request_size, u32 flags);
int octeon_console_write_avail(struct octeon_device *oct, u32 console_num);
int octeon_console_read_avail(struct octeon_device *oct, u32 console_num);
void octeon_remove_consoles(struct octeon_device *oct);
int octeon_console_send_cmd(struct octeon_device *oct, char *cmd_str,
			    u32 wait_hundredths);
int octeon_download_firmware(struct octeon_device *oct, const u8 *data,
			     size_t size);
char *lio_get_state_string(atomic_t *state_ptr);
int octeon_setup_instr_queues(struct octeon_device *oct);
int octeon_setup_output_queues(struct octeon_device *oct);
int octeon_get_tx_qsize(struct octeon_device *oct, u32 q_no);
int octeon_get_rx_qsize(struct octeon_device *oct, u32 q_no);
int octeon_set_io_queues_off(struct octeon_device *oct);
void octeon_set_droq_pkt_op(struct octeon_device *oct, u32 q_no, u32 enable);
void *oct_get_config_info(struct octeon_device *oct, u16 card_type);
struct octeon_config *octeon_get_conf(struct octeon_device *oct);
void octeon_free_ioq_vector(struct octeon_device *oct);
int octeon_allocate_ioq_vector(struct octeon_device  *oct, u32 num_ioqs);
void lio_enable_irq(struct octeon_droq *droq, struct octeon_instr_queue *iq);
enum {
	OCT_PRIV_FLAG_TX_BYTES = 0,  
};
#define OCT_PRIV_FLAG_DEFAULT 0x0
static inline u32 lio_get_priv_flag(struct octeon_device *octdev, u32 flag)
{
	return !!(octdev->priv_flags & (0x1 << flag));
}
static inline void lio_set_priv_flag(struct octeon_device *octdev,
				     u32 flag, u32 val)
{
	if (val)
		octdev->priv_flags |= (0x1 << flag);
	else
		octdev->priv_flags &= ~(0x1 << flag);
}
#endif
