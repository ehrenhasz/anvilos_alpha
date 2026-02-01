 
 

#ifndef __FM_H
#define __FM_H

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

 
 
#define FM_FD_CMD_FCO                   0x80000000
#define FM_FD_CMD_RPD                   0x40000000   
#define FM_FD_CMD_UPD			0x20000000   
#define FM_FD_CMD_DTC                   0x10000000   

 
#define FM_FD_ERR_UNSUPPORTED_FORMAT    0x04000000
 
#define FM_FD_ERR_LENGTH                0x02000000
#define FM_FD_ERR_DMA                   0x01000000   

 
#define FM_FD_IPR                       0x00000001
 
#define FM_FD_ERR_IPR_NCSP              (0x00100000 | FM_FD_IPR)
 
#define FM_FD_ERR_IPR                   (0x00200000 | FM_FD_IPR)
 
#define FM_FD_ERR_IPR_TO                (0x00300000 | FM_FD_IPR)
 
#define FM_FD_ERR_IPRE                  (FM_FD_ERR_IPR & ~FM_FD_IPR)

 
#define FM_FD_ERR_PHYSICAL              0x00080000
 
#define FM_FD_ERR_SIZE                  0x00040000
 
#define FM_FD_ERR_CLS_DISCARD           0x00020000
 
#define FM_FD_ERR_EXTRACTION            0x00008000
 
#define FM_FD_ERR_NO_SCHEME             0x00004000
 
#define FM_FD_ERR_KEYSIZE_OVERFLOW      0x00002000
 
#define FM_FD_ERR_COLOR_RED             0x00000800
 
#define FM_FD_ERR_COLOR_YELLOW          0x00000400
 
#define FM_FD_ERR_PRS_TIMEOUT           0x00000080
 
#define FM_FD_ERR_PRS_ILL_INSTRUCT      0x00000040
 
#define FM_FD_ERR_PRS_HDR_ERR           0x00000020
 
#define FM_FD_ERR_BLOCK_LIMIT_EXCEEDED  0x00000008

 
#define FM_FD_RX_STATUS_ERR_NON_FM      0x00400000

 
#define FMAN_BMI_FIFO_UNITS		0x100
#define OFFSET_UNITS			16

 
#define BM_MAX_NUM_OF_POOLS		64  
#define FMAN_PORT_MAX_EXT_POOLS_NUM	8   

struct fman;  

 
enum fman_port_type {
	FMAN_PORT_TYPE_TX = 0,	 
	FMAN_PORT_TYPE_RX,	 
};

struct fman_rev_info {
	u8 major;			 
	u8 minor;			 
};

enum fman_exceptions {
	FMAN_EX_DMA_BUS_ERROR = 0,	 
	FMAN_EX_DMA_READ_ECC,		 
	FMAN_EX_DMA_SYSTEM_WRITE_ECC,	 
	FMAN_EX_DMA_FM_WRITE_ECC,	 
	FMAN_EX_DMA_SINGLE_PORT_ECC,	 
	FMAN_EX_FPM_STALL_ON_TASKS,	 
	FMAN_EX_FPM_SINGLE_ECC,		 
	FMAN_EX_FPM_DOUBLE_ECC,		 
	FMAN_EX_QMI_SINGLE_ECC,	 
	FMAN_EX_QMI_DOUBLE_ECC,	 
	FMAN_EX_QMI_DEQ_FROM_UNKNOWN_PORTID, 
	FMAN_EX_BMI_LIST_RAM_ECC,	 
	FMAN_EX_BMI_STORAGE_PROFILE_ECC, 
	FMAN_EX_BMI_STATISTICS_RAM_ECC, 
	FMAN_EX_BMI_DISPATCH_RAM_ECC,	 
	FMAN_EX_IRAM_ECC,		 
	FMAN_EX_MURAM_ECC		 
};

 
struct fman_prs_result {
	u8 lpid;		 
	u8 shimr;		 
	__be16 l2r;		 
	__be16 l3r;		 
	u8 l4r;		 
	u8 cplan;		 
	__be16 nxthdr;		 
	__be16 cksum;		 
	 
	__be16 flags_frag_off;
	 
	u8 route_type;
	 
	u8 rhp_ip_valid;
	u8 shim_off[2];		 
	u8 ip_pid_off;		 
	u8 eth_off;		 
	u8 llc_snap_off;	 
	u8 vlan_off[2];		 
	u8 etype_off;		 
	u8 pppoe_off;		 
	u8 mpls_off[2];		 
	u8 ip_off[2];		 
	u8 gre_off;		 
	u8 l4_off;		 
	u8 nxthdr_off;		 
};

 
struct fman_buffer_prefix_content {
	 
	u16 priv_data_size;
	 
	bool pass_prs_result;
	 
	bool pass_time_stamp;
	 
	bool pass_hash_result;
	 
	u16 data_align;
};

 
struct fman_ext_pool_params {
	u8 id;		     
	u16 size;		     
};

 
struct fman_ext_pools {
	u8 num_of_pools_used;  
	struct fman_ext_pool_params ext_buf_pool[FMAN_PORT_MAX_EXT_POOLS_NUM];
					 
};

 
struct fman_buf_pool_depletion {
	 
	bool pools_grp_mode_enable;
	 
	u8 num_of_pools;
	 
	bool pools_to_consider[BM_MAX_NUM_OF_POOLS];
	 
	bool single_pool_mode_enable;
	 
	bool pools_to_consider_for_single_mode[BM_MAX_NUM_OF_POOLS];
};

 
enum fman_event_modules {
	FMAN_MOD_MAC = 0,		 
	FMAN_MOD_FMAN_CTRL,	 
	FMAN_MOD_DUMMY_LAST
};

 
enum fman_intr_type {
	FMAN_INTR_TYPE_ERR,
	FMAN_INTR_TYPE_NORMAL
};

 
enum fman_inter_module_event {
	FMAN_EV_ERR_MAC0 = 0,	 
	FMAN_EV_ERR_MAC1,		 
	FMAN_EV_ERR_MAC2,		 
	FMAN_EV_ERR_MAC3,		 
	FMAN_EV_ERR_MAC4,		 
	FMAN_EV_ERR_MAC5,		 
	FMAN_EV_ERR_MAC6,		 
	FMAN_EV_ERR_MAC7,		 
	FMAN_EV_ERR_MAC8,		 
	FMAN_EV_ERR_MAC9,		 
	FMAN_EV_MAC0,		 
	FMAN_EV_MAC1,		 
	FMAN_EV_MAC2,		 
	FMAN_EV_MAC3,		 
	FMAN_EV_MAC4,		 
	FMAN_EV_MAC5,		 
	FMAN_EV_MAC6,		 
	FMAN_EV_MAC7,		 
	FMAN_EV_MAC8,		 
	FMAN_EV_MAC9,		 
	FMAN_EV_FMAN_CTRL_0,	 
	FMAN_EV_FMAN_CTRL_1,	 
	FMAN_EV_FMAN_CTRL_2,	 
	FMAN_EV_FMAN_CTRL_3,	 
	FMAN_EV_CNT
};

struct fman_intr_src {
	void (*isr_cb)(void *src_arg);
	void *src_handle;
};

 
typedef irqreturn_t (fman_exceptions_cb)(struct fman *fman,
					 enum fman_exceptions exception);
 
typedef irqreturn_t (fman_bus_error_cb)(struct fman *fman, u8 port_id,
					u64 addr, u8 tnum, u16 liodn);

 
struct fman_dts_params {
	void __iomem *base_addr;                 
	struct resource *res;                    
	u8 id;                                   

	int err_irq;                             

	u16 clk_freq;                            

	u32 qman_channel_base;                   
	u32 num_of_qman_channels;                

	struct resource muram_res;               
};

struct fman {
	struct device *dev;
	void __iomem *base_addr;
	struct fman_intr_src intr_mng[FMAN_EV_CNT];

	struct fman_fpm_regs __iomem *fpm_regs;
	struct fman_bmi_regs __iomem *bmi_regs;
	struct fman_qmi_regs __iomem *qmi_regs;
	struct fman_dma_regs __iomem *dma_regs;
	struct fman_hwp_regs __iomem *hwp_regs;
	struct fman_kg_regs __iomem *kg_regs;
	fman_exceptions_cb *exception_cb;
	fman_bus_error_cb *bus_error_cb;
	 
	spinlock_t spinlock;
	struct fman_state_struct *state;

	struct fman_cfg *cfg;
	struct muram_info *muram;
	struct fman_keygen *keygen;
	 
	unsigned long cam_offset;
	size_t cam_size;
	 
	unsigned long fifo_offset;
	size_t fifo_size;

	u32 liodn_base[64];
	u32 liodn_offset[64];

	struct fman_dts_params dts_params;
};

 
struct fman_port_init_params {
	u8 port_id;			 
	enum fman_port_type port_type;	 
	u16 port_speed;			 
	u16 liodn_offset;		 
	u8 num_of_tasks;		 
	u8 num_of_extra_tasks;		 
	u8 num_of_open_dmas;		 
	u8 num_of_extra_open_dmas;	 
	u32 size_of_fifo;		 
	u32 extra_size_of_fifo;		 
	u8 deq_pipeline_depth;		 
	u16 max_frame_length;		 
	u16 liodn_base;
	 
};

void fman_get_revision(struct fman *fman, struct fman_rev_info *rev_info);

void fman_register_intr(struct fman *fman, enum fman_event_modules mod,
			u8 mod_id, enum fman_intr_type intr_type,
			void (*f_isr)(void *h_src_arg), void *h_src_arg);

void fman_unregister_intr(struct fman *fman, enum fman_event_modules mod,
			  u8 mod_id, enum fman_intr_type intr_type);

int fman_set_port_params(struct fman *fman,
			 struct fman_port_init_params *port_params);

int fman_reset_mac(struct fman *fman, u8 mac_id);

u16 fman_get_clock_freq(struct fman *fman);

u32 fman_get_bmi_max_fifo_size(struct fman *fman);

int fman_set_mac_max_frame(struct fman *fman, u8 mac_id, u16 mfl);

u32 fman_get_qman_channel_id(struct fman *fman, u32 port_id);

struct resource *fman_get_mem_region(struct fman *fman);

u16 fman_get_max_frm(void);

int fman_get_rx_extra_headroom(void);

#ifdef CONFIG_DPAA_ERRATUM_A050385
bool fman_has_errata_a050385(void);
#endif

struct fman *fman_bind(struct device *dev);

#endif  
