#ifndef __CSIO_HW_H__
#define __CSIO_HW_H__
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/compiler.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/mempool.h>
#include <linux/io.h>
#include <linux/spinlock_types.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_transport_fc.h>
#include "t4_hw.h"
#include "csio_hw_chip.h"
#include "csio_wr.h"
#include "csio_mb.h"
#include "csio_scsi.h"
#include "csio_defs.h"
#include "t4_regs.h"
#include "t4_msg.h"
#define	FW_HOSTERROR			255
#define CSIO_HW_NAME		"Chelsio FCoE Adapter"
#define CSIO_MAX_PFN		8
#define CSIO_MAX_PPORTS		4
#define CSIO_MAX_LUN		0xFFFF
#define CSIO_MAX_QUEUE		2048
#define CSIO_MAX_CMD_PER_LUN	32
#define CSIO_MAX_DDP_BUF_SIZE	(1024 * 1024)
#define CSIO_MAX_SECTOR_SIZE	128
#define CSIO_MIN_T6_FW		0x01102D00   
#define CSIO_EXTRA_MSI_IQS	2	 
#define CSIO_EXTRA_VECS		2	 
#define CSIO_MAX_SCSI_CPU	128
#define CSIO_MAX_SCSI_QSETS	(CSIO_MAX_SCSI_CPU * CSIO_MAX_PPORTS)
#define CSIO_MAX_MSIX_VECS	(CSIO_MAX_SCSI_QSETS + CSIO_EXTRA_VECS)
enum {
	CSIO_INTR_WRSIZE = 128,
	CSIO_INTR_IQSIZE = ((CSIO_MAX_MSIX_VECS + 1) * CSIO_INTR_WRSIZE),
	CSIO_FWEVT_WRSIZE = 128,
	CSIO_FWEVT_IQLEN = 128,
	CSIO_FWEVT_FLBUFS = 64,
	CSIO_FWEVT_IQSIZE = (CSIO_FWEVT_WRSIZE * CSIO_FWEVT_IQLEN),
	CSIO_HW_NIQ = 1,
	CSIO_HW_NFLQ = 1,
	CSIO_HW_NEQ = 1,
	CSIO_HW_NINTXQ = 1,
};
struct csio_msix_entries {
	void		*dev_id;	 
	char		desc[24];	 
};
struct csio_scsi_qset {
	int		iq_idx;		 
	int		eq_idx;		 
	uint32_t	intr_idx;	 
};
struct csio_scsi_cpu_info {
	int16_t	max_cpus;
};
extern int csio_dbg_level;
extern unsigned int csio_port_mask;
extern int csio_msi;
#define CSIO_VENDOR_ID				0x1425
#define CSIO_ASIC_DEVID_PROTO_MASK		0xFF00
#define CSIO_ASIC_DEVID_TYPE_MASK		0x00FF
#define CSIO_GLBL_INTR_MASK	(CIM_F | MPS_F | PL_F | PCIE_F | MC_F | \
				 EDC0_F | EDC1_F | LE_F | TP_F | MA_F | \
				 PM_TX_F | PM_RX_F | ULP_RX_F | \
				 CPL_SWITCH_F | SGE_F | ULP_TX_F | SF_F)
enum {
	CSIO_SGE_DBFIFO_INT_THRESH	= 10,
	CSIO_SGE_RX_DMA_OFFSET		= 2,
	CSIO_SGE_FLBUF_SIZE1		= 65536,
	CSIO_SGE_FLBUF_SIZE2		= 1536,
	CSIO_SGE_FLBUF_SIZE3		= 9024,
	CSIO_SGE_FLBUF_SIZE4		= 9216,
	CSIO_SGE_FLBUF_SIZE5		= 2048,
	CSIO_SGE_FLBUF_SIZE6		= 128,
	CSIO_SGE_FLBUF_SIZE7		= 8192,
	CSIO_SGE_FLBUF_SIZE8		= 16384,
	CSIO_SGE_TIMER_VAL_0		= 5,
	CSIO_SGE_TIMER_VAL_1		= 10,
	CSIO_SGE_TIMER_VAL_2		= 20,
	CSIO_SGE_TIMER_VAL_3		= 50,
	CSIO_SGE_TIMER_VAL_4		= 100,
	CSIO_SGE_TIMER_VAL_5		= 200,
	CSIO_SGE_INT_CNT_VAL_0		= 1,
	CSIO_SGE_INT_CNT_VAL_1		= 4,
	CSIO_SGE_INT_CNT_VAL_2		= 8,
	CSIO_SGE_INT_CNT_VAL_3		= 16,
};
enum csio_evt {
	CSIO_EVT_FW  = 0,	 
	CSIO_EVT_MBX,		 
	CSIO_EVT_SCN,		 
	CSIO_EVT_DEV_LOSS,	 
	CSIO_EVT_MAX,		 
};
#define CSIO_EVT_MSG_SIZE	512
#define CSIO_EVTQ_SIZE		512
struct csio_evt_msg {
	struct list_head	list;	 
	enum csio_evt		type;
	uint8_t			data[CSIO_EVT_MSG_SIZE];
};
enum {
	SERNUM_LEN     = 16,     
	EC_LEN         = 16,     
	ID_LEN         = 16,     
};
enum {
	SF_SIZE = SF_SEC_SIZE * 16,    
};
enum {
	SF_ATTEMPTS = 10,              
	SF_PROG_PAGE    = 2,           
	SF_WR_DISABLE   = 4,           
	SF_RD_STATUS    = 5,           
	SF_WR_ENABLE    = 6,           
	SF_RD_DATA_FAST = 0xb,         
	SF_RD_ID	= 0x9f,	       
	SF_ERASE_SECTOR = 0xd8,        
};
enum {
	CSIO_MGMT_EQ_WRSIZE = 512,
	CSIO_MGMT_IQ_WRSIZE = 128,
	CSIO_MGMT_EQLEN = 64,
	CSIO_MGMT_IQLEN = 64,
};
#define CSIO_MGMT_EQSIZE	(CSIO_MGMT_EQLEN * CSIO_MGMT_EQ_WRSIZE)
#define CSIO_MGMT_IQSIZE	(CSIO_MGMT_IQLEN * CSIO_MGMT_IQ_WRSIZE)
struct csio_mgmtm_stats {
	uint32_t	n_abort_req;		 
	uint32_t	n_abort_rsp;		 
	uint32_t	n_close_req;		 
	uint32_t	n_close_rsp;		 
	uint32_t	n_err;			 
	uint32_t	n_drop;			 
	uint32_t	n_active;		 
	uint32_t	n_cbfn;			 
};
struct csio_mgmtm {
	struct	csio_hw		*hw;		 
	int			eq_idx;		 
	int			iq_idx;		 
	int			msi_vec;	 
	struct list_head	active_q;	 
	struct list_head	abort_q;	 
	struct list_head	cbfn_q;		 
	struct list_head	mgmt_req_freelist;  
	struct timer_list	mgmt_timer;	 
	struct csio_mgmtm_stats stats;		 
};
struct csio_adap_desc {
	char model_no[16];
	char description[32];
};
struct pci_params {
	uint16_t   vendor_id;
	uint16_t   device_id;
	int        vpd_cap_addr;
	uint16_t   speed;
	uint8_t    width;
};
struct csio_hw_params {
	uint32_t		sf_size;		 
	uint32_t		sf_nsec;		 
	struct pci_params	pci;
	uint32_t		log_level;		 
};
struct csio_vpd {
	uint32_t cclk;
	uint8_t ec[EC_LEN + 1];
	uint8_t sn[SERNUM_LEN + 1];
	uint8_t id[ID_LEN + 1];
};
typedef u16 fw_port_cap16_t;     
typedef u32 fw_port_cap32_t;     
enum fw_caps {
	FW_CAPS_UNKNOWN = 0,     
	FW_CAPS16       = 1,     
	FW_CAPS32       = 2,     
};
enum cc_pause {
	PAUSE_RX      = 1 << 0,
	PAUSE_TX      = 1 << 1,
	PAUSE_AUTONEG = 1 << 2
};
enum cc_fec {
	FEC_AUTO	= 1 << 0,   
	FEC_RS		= 1 << 1,   
	FEC_BASER_RS	= 1 << 2    
};
struct link_config {
	fw_port_cap32_t pcaps;		 
	fw_port_cap32_t def_acaps;	 
	fw_port_cap32_t acaps;		 
	fw_port_cap32_t lpacaps;	 
	fw_port_cap32_t speed_caps;	 
	unsigned int   speed;		 
	enum cc_pause  requested_fc;	 
	enum cc_pause  fc;		 
	enum cc_fec    requested_fec;	 
	enum cc_fec    fec;		 
	unsigned char  autoneg;		 
	unsigned char  link_ok;		 
	unsigned char  link_down_rc;	 
};
#define FW_LEN16(fw_struct) FW_CMD_LEN16_V(sizeof(fw_struct) / 16)
#define ADVERT_MASK (FW_PORT_CAP32_SPEED_V(FW_PORT_CAP32_SPEED_M) | \
		     FW_PORT_CAP32_ANEG)
#define AUTONEG_DISABLE	0x00
#define AUTONEG_ENABLE	0x01
struct csio_pport {
	uint16_t	pcap;
	uint16_t	acap;
	uint8_t		portid;
	uint8_t		link_status;
	uint16_t	link_speed;
	uint8_t		mac[6];
	uint8_t		mod_type;
	uint8_t		rsvd1;
	uint8_t		rsvd2;
	uint8_t		rsvd3;
	struct link_config link_cfg;
};
struct csio_fcoe_res_info {
	uint16_t	e_d_tov;
	uint16_t	r_a_tov_seq;
	uint16_t	r_a_tov_els;
	uint16_t	r_r_tov;
	uint32_t	max_xchgs;
	uint32_t	max_ssns;
	uint32_t	used_xchgs;
	uint32_t	used_ssns;
	uint32_t	max_fcfs;
	uint32_t	max_vnps;
	uint32_t	used_fcfs;
	uint32_t	used_vnps;
};
enum csio_hw_ev {
	CSIO_HWE_CFG = (uint32_t)1,  
	CSIO_HWE_INIT,	          
	CSIO_HWE_INIT_DONE,       
	CSIO_HWE_FATAL,		  
	CSIO_HWE_PCIERR_DETECTED, 
	CSIO_HWE_PCIERR_SLOT_RESET,  
	CSIO_HWE_PCIERR_RESUME,   
	CSIO_HWE_QUIESCED,	  
	CSIO_HWE_HBA_RESET,       
	CSIO_HWE_HBA_RESET_DONE,  
	CSIO_HWE_FW_DLOAD,        
	CSIO_HWE_PCI_REMOVE,      
	CSIO_HWE_SUSPEND,         
	CSIO_HWE_RESUME,          
	CSIO_HWE_MAX,		  
};
struct csio_hw_stats {
	uint32_t	n_evt_activeq;	 
	uint32_t	n_evt_freeq;	 
	uint32_t	n_evt_drop;	 
	uint32_t	n_evt_unexp;	 
	uint32_t	n_pcich_offline; 
	uint32_t	n_lnlkup_miss;   
	uint32_t	n_cpl_fw6_msg;	 
	uint32_t	n_cpl_fw6_pld;	 
	uint32_t	n_cpl_unexp;	 
	uint32_t	n_mbint_unexp;	 
	uint32_t	n_plint_unexp;	 
	uint32_t	n_plint_cnt;	 
	uint32_t	n_int_stray;	 
	uint32_t	n_err;		 
	uint32_t	n_err_fatal;	 
	uint32_t	n_err_nomem;	 
	uint32_t	n_err_io;	 
	enum csio_hw_ev	n_evt_sm[CSIO_HWE_MAX];	 
	uint64_t	n_reset_start;   
	uint32_t	rsvd1;
};
#define CSIO_HWF_MASTER			0x00000001	 
#define	CSIO_HWF_HW_INTR_ENABLED	0x00000002	 
#define	CSIO_HWF_FWEVT_PENDING		0x00000004	 
#define	CSIO_HWF_Q_MEM_ALLOCED		0x00000008	 
#define	CSIO_HWF_Q_FW_ALLOCED		0x00000010	 
#define CSIO_HWF_VPD_VALID		0x00000020	 
#define CSIO_HWF_DEVID_CACHED		0X00000040	 
#define	CSIO_HWF_FWEVT_STOP		0x00000080	 
#define CSIO_HWF_USING_SOFT_PARAMS	0x00000100       
#define	CSIO_HWF_HOST_INTR_ENABLED	0x00000200	 
#define CSIO_HWF_ROOT_NO_RELAXED_ORDERING 0x00000400	 
#define csio_is_hw_intr_enabled(__hw)	\
				((__hw)->flags & CSIO_HWF_HW_INTR_ENABLED)
#define csio_is_host_intr_enabled(__hw)	\
				((__hw)->flags & CSIO_HWF_HOST_INTR_ENABLED)
#define csio_is_hw_master(__hw)		((__hw)->flags & CSIO_HWF_MASTER)
#define csio_is_valid_vpd(__hw)		((__hw)->flags & CSIO_HWF_VPD_VALID)
#define csio_is_dev_id_cached(__hw)	((__hw)->flags & CSIO_HWF_DEVID_CACHED)
#define csio_valid_vpd_copied(__hw)	((__hw)->flags |= CSIO_HWF_VPD_VALID)
#define csio_dev_id_cached(__hw)	((__hw)->flags |= CSIO_HWF_DEVID_CACHED)
enum csio_intr_mode {
	CSIO_IM_NONE = 0,
	CSIO_IM_INTX = 1,
	CSIO_IM_MSI  = 2,
	CSIO_IM_MSIX = 3,
};
struct csio_hw {
	struct csio_sm		sm;			 
	spinlock_t		lock;			 
	struct csio_scsim	scsim;			 
	struct csio_wrm		wrm;			 
	struct pci_dev		*pdev;			 
	void __iomem		*regstart;		 
	uint32_t		num_sqsets;		 
	uint32_t		num_scsi_msix_cpus;	 
	struct csio_scsi_qset	sqset[CSIO_MAX_PPORTS][CSIO_MAX_SCSI_CPU];
	struct csio_scsi_cpu_info scsi_cpu_info[CSIO_MAX_PPORTS];
	uint32_t		evtflag;		 
	uint32_t		flags;			 
	struct csio_mgmtm	mgmtm;			 
	struct csio_mbm		mbm;			 
	uint32_t		num_lns;		 
	struct csio_lnode	*rln;			 
	struct list_head	sln_head;		 
	int			intr_iq_idx;		 
	int			fwevt_iq_idx;		 
	struct work_struct	evtq_work;		 
	struct list_head	evt_free_q;		 
	struct list_head	evt_active_q;		 
	char			name[32];
	char			hw_ver[16];
	char			model_desc[32];
	char			drv_version[32];
	char			fwrev_str[32];
	uint32_t		optrom_ver;
	uint32_t		fwrev;
	uint32_t		tp_vers;
	char			chip_ver;
	uint16_t		chip_id;		 
	enum csio_dev_state	fw_state;
	struct csio_vpd		vpd;
	uint8_t			pfn;			 
	uint32_t		port_vec;		 
	uint8_t			num_pports;		 
	uint8_t			rst_retries;		 
	uint8_t			cur_evt;		 
	uint8_t			prev_evt;		 
	uint32_t		dev_num;		 
	struct csio_pport	pport[CSIO_MAX_PPORTS];	 
	struct csio_hw_params	params;			 
	struct dma_pool		*scsi_dma_pool;		 
	mempool_t		*mb_mempool;		 
	mempool_t		*rnode_mempool;		 
	enum csio_intr_mode	intr_mode;		 
	uint32_t		fwevt_intr_idx;		 
	uint32_t		nondata_intr_idx;	 
	uint8_t			cfg_neq;		 
	uint8_t			cfg_niq;		 
	struct csio_fcoe_res_info  fres_info;		 
	struct csio_hw_chip_ops	*chip_ops;		 
	struct csio_msix_entries msix_entries[CSIO_MAX_MSIX_VECS];
	struct dentry		*debugfs_root;		 
	struct csio_hw_stats	stats;			 
};
#define csio_reg(_b, _r)		((_b) + (_r))
#define	csio_rd_reg8(_h, _r)		readb(csio_reg((_h)->regstart, (_r)))
#define	csio_rd_reg16(_h, _r)		readw(csio_reg((_h)->regstart, (_r)))
#define	csio_rd_reg32(_h, _r)		readl(csio_reg((_h)->regstart, (_r)))
#define	csio_rd_reg64(_h, _r)		readq(csio_reg((_h)->regstart, (_r)))
#define	csio_wr_reg8(_h, _v, _r)	writeb((_v), \
						csio_reg((_h)->regstart, (_r)))
#define	csio_wr_reg16(_h, _v, _r)	writew((_v), \
						csio_reg((_h)->regstart, (_r)))
#define	csio_wr_reg32(_h, _v, _r)	writel((_v), \
						csio_reg((_h)->regstart, (_r)))
#define	csio_wr_reg64(_h, _v, _r)	writeq((_v), \
						csio_reg((_h)->regstart, (_r)))
void csio_set_reg_field(struct csio_hw *, uint32_t, uint32_t, uint32_t);
static inline uint32_t
csio_core_ticks_to_us(struct csio_hw *hw, uint32_t ticks)
{
	return (ticks * 1000 + hw->vpd.cclk/2) / hw->vpd.cclk;
}
static inline uint32_t
csio_us_to_core_ticks(struct csio_hw *hw, uint32_t us)
{
	return (us * hw->vpd.cclk) / 1000;
}
#define csio_hw_to_wrm(hw)		((struct csio_wrm *)(&(hw)->wrm))
#define csio_hw_to_mbm(hw)		((struct csio_mbm *)(&(hw)->mbm))
#define csio_hw_to_scsim(hw)		((struct csio_scsim *)(&(hw)->scsim))
#define csio_hw_to_mgmtm(hw)		((struct csio_mgmtm *)(&(hw)->mgmtm))
#define CSIO_PCI_BUS(hw)		((hw)->pdev->bus->number)
#define CSIO_PCI_DEV(hw)		(PCI_SLOT((hw)->pdev->devfn))
#define CSIO_PCI_FUNC(hw)		(PCI_FUNC((hw)->pdev->devfn))
#define csio_set_fwevt_intr_idx(_h, _i)		((_h)->fwevt_intr_idx = (_i))
#define csio_get_fwevt_intr_idx(_h)		((_h)->fwevt_intr_idx)
#define csio_set_nondata_intr_idx(_h, _i)	((_h)->nondata_intr_idx = (_i))
#define csio_get_nondata_intr_idx(_h)		((_h)->nondata_intr_idx)
#define CSIO_DEVID(__dev)		((__dev)->dev_num)
#define CSIO_DEVID_LO(__dev)		(CSIO_DEVID((__dev)) & 0xFFFF)
#define CSIO_DEVID_HI(__dev)		((CSIO_DEVID((__dev)) >> 16) & 0xFFFF)
#define csio_info(__hw, __fmt, ...)					\
			dev_info(&(__hw)->pdev->dev, __fmt, ##__VA_ARGS__)
#define csio_fatal(__hw, __fmt, ...)					\
			dev_crit(&(__hw)->pdev->dev, __fmt, ##__VA_ARGS__)
#define csio_err(__hw, __fmt, ...)					\
			dev_err(&(__hw)->pdev->dev, __fmt, ##__VA_ARGS__)
#define csio_warn(__hw, __fmt, ...)					\
			dev_warn(&(__hw)->pdev->dev, __fmt, ##__VA_ARGS__)
#ifdef __CSIO_DEBUG__
#define csio_dbg(__hw, __fmt, ...)					\
			csio_info((__hw), __fmt, ##__VA_ARGS__);
#else
#define csio_dbg(__hw, __fmt, ...)
#endif
int csio_hw_wait_op_done_val(struct csio_hw *, int, uint32_t, int,
			     int, int, uint32_t *);
void csio_hw_tp_wr_bits_indirect(struct csio_hw *, unsigned int,
				 unsigned int, unsigned int);
int csio_mgmt_req_lookup(struct csio_mgmtm *, struct csio_ioreq *);
void csio_hw_intr_disable(struct csio_hw *);
int csio_hw_slow_intr_handler(struct csio_hw *);
int csio_handle_intr_status(struct csio_hw *, unsigned int,
			    const struct intr_info *);
fw_port_cap32_t fwcap_to_fwspeed(fw_port_cap32_t acaps);
fw_port_cap32_t fwcaps16_to_caps32(fw_port_cap16_t caps16);
fw_port_cap16_t fwcaps32_to_caps16(fw_port_cap32_t caps32);
fw_port_cap32_t lstatus_to_fwcap(u32 lstatus);
int csio_hw_start(struct csio_hw *);
int csio_hw_stop(struct csio_hw *);
int csio_hw_reset(struct csio_hw *);
int csio_is_hw_ready(struct csio_hw *);
int csio_is_hw_removing(struct csio_hw *);
int csio_fwevtq_handler(struct csio_hw *);
void csio_evtq_worker(struct work_struct *);
int csio_enqueue_evt(struct csio_hw *, enum csio_evt, void *, uint16_t);
void csio_evtq_flush(struct csio_hw *hw);
int csio_request_irqs(struct csio_hw *);
void csio_intr_enable(struct csio_hw *);
void csio_intr_disable(struct csio_hw *, bool);
void csio_hw_fatal_err(struct csio_hw *);
struct csio_lnode *csio_lnode_alloc(struct csio_hw *);
int csio_config_queues(struct csio_hw *);
int csio_hw_init(struct csio_hw *);
void csio_hw_exit(struct csio_hw *);
#endif  
