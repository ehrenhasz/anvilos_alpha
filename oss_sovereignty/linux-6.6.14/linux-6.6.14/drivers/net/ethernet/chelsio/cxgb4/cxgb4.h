#ifndef __CXGB4_H__
#define __CXGB4_H__
#include "t4_hw.h"
#include <linux/bitops.h>
#include <linux/cache.h>
#include <linux/ethtool.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/rhashtable.h>
#include <linux/etherdevice.h>
#include <linux/net_tstamp.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/ptp_classify.h>
#include <linux/crash_dump.h>
#include <linux/thermal.h>
#include <asm/io.h>
#include "t4_chip_type.h"
#include "cxgb4_uld.h"
#include "t4fw_api.h"
#define CH_WARN(adap, fmt, ...) dev_warn(adap->pdev_dev, fmt, ## __VA_ARGS__)
extern struct list_head adapter_list;
extern struct list_head uld_list;
extern struct mutex uld_mutex;
#define ETHTXQ_STOP_THRES \
	(1 + DIV_ROUND_UP((3 * MAX_SKB_FRAGS) / 2 + (MAX_SKB_FRAGS & 1), 8))
#define FW_PARAM_DEV(param) \
	(FW_PARAMS_MNEM_V(FW_PARAMS_MNEM_DEV) | \
	 FW_PARAMS_PARAM_X_V(FW_PARAMS_PARAM_DEV_##param))
#define FW_PARAM_PFVF(param) \
	(FW_PARAMS_MNEM_V(FW_PARAMS_MNEM_PFVF) | \
	 FW_PARAMS_PARAM_X_V(FW_PARAMS_PARAM_PFVF_##param) |  \
	 FW_PARAMS_PARAM_Y_V(0) | \
	 FW_PARAMS_PARAM_Z_V(0))
enum {
	MAX_NPORTS	= 4,      
	SERNUM_LEN	= 24,     
	ID_LEN		= 16,     
	PN_LEN		= 16,     
	MACADDR_LEN	= 12,     
};
enum {
	T4_REGMAP_SIZE = (160 * 1024),
	T5_REGMAP_SIZE = (332 * 1024),
};
enum {
	MEM_EDC0,
	MEM_EDC1,
	MEM_MC,
	MEM_MC0 = MEM_MC,
	MEM_MC1,
	MEM_HMA,
};
enum {
	MEMWIN0_APERTURE = 2048,
	MEMWIN0_BASE     = 0x1b800,
	MEMWIN1_APERTURE = 32768,
	MEMWIN1_BASE     = 0x28000,
	MEMWIN1_BASE_T5  = 0x52000,
	MEMWIN2_APERTURE = 65536,
	MEMWIN2_BASE     = 0x30000,
	MEMWIN2_APERTURE_T5 = 131072,
	MEMWIN2_BASE_T5  = 0x60000,
};
enum dev_master {
	MASTER_CANT,
	MASTER_MAY,
	MASTER_MUST
};
enum dev_state {
	DEV_STATE_UNINIT,
	DEV_STATE_INIT,
	DEV_STATE_ERR
};
enum cc_pause {
	PAUSE_RX      = 1 << 0,
	PAUSE_TX      = 1 << 1,
	PAUSE_AUTONEG = 1 << 2
};
enum cc_fec {
	FEC_AUTO      = 1 << 0,	  
	FEC_RS        = 1 << 1,   
	FEC_BASER_RS  = 1 << 2    
};
enum {
	CXGB4_ETHTOOL_FLASH_FW = 1,
	CXGB4_ETHTOOL_FLASH_PHY = 2,
	CXGB4_ETHTOOL_FLASH_BOOT = 3,
	CXGB4_ETHTOOL_FLASH_BOOTCFG = 4
};
enum cxgb4_netdev_tls_ops {
	CXGB4_TLSDEV_OPS  = 1,
	CXGB4_XFRMDEV_OPS
};
struct cxgb4_bootcfg_data {
	__le16 signature;
	__u8 reserved[2];
};
struct cxgb4_pcir_data {
	__le32 signature;	 
	__le16 vendor_id;	 
	__le16 device_id;	 
	__u8 vital_product[2];	 
	__u8 length[2];		 
	__u8 revision;		 
	__u8 class_code[3];	 
	__u8 image_length[2];	 
	__u8 code_revision[2];	 
	__u8 code_type;
	__u8 indicator;
	__u8 reserved[2];
};
struct cxgb4_pci_exp_rom_header {
	__le16 signature;	 
	__u8 reserved[22];	 
	__le16 pcir_offset;	 
};
struct legacy_pci_rom_hdr {
	__u8 signature[2];	 
	__u8 size512;		 
	__u8 initentry_point[4];
	__u8 cksum;		 
	__u8 reserved[16];	 
	__le16 pcir_offset;	 
};
#define CXGB4_HDR_CODE1 0x00
#define CXGB4_HDR_CODE2 0x03
#define CXGB4_HDR_INDI 0x80
enum {
	BOOT_CFG_SIG = 0x4243,
	BOOT_SIZE_INC = 512,
	BOOT_SIGNATURE = 0xaa55,
	BOOT_MIN_SIZE = sizeof(struct cxgb4_pci_exp_rom_header),
	BOOT_MAX_SIZE = 1024 * BOOT_SIZE_INC,
	PCIR_SIGNATURE = 0x52494350
};
struct port_stats {
	u64 tx_octets;             
	u64 tx_frames;             
	u64 tx_bcast_frames;       
	u64 tx_mcast_frames;       
	u64 tx_ucast_frames;       
	u64 tx_error_frames;       
	u64 tx_frames_64;          
	u64 tx_frames_65_127;
	u64 tx_frames_128_255;
	u64 tx_frames_256_511;
	u64 tx_frames_512_1023;
	u64 tx_frames_1024_1518;
	u64 tx_frames_1519_max;
	u64 tx_drop;               
	u64 tx_pause;              
	u64 tx_ppp0;               
	u64 tx_ppp1;               
	u64 tx_ppp2;               
	u64 tx_ppp3;               
	u64 tx_ppp4;               
	u64 tx_ppp5;               
	u64 tx_ppp6;               
	u64 tx_ppp7;               
	u64 rx_octets;             
	u64 rx_frames;             
	u64 rx_bcast_frames;       
	u64 rx_mcast_frames;       
	u64 rx_ucast_frames;       
	u64 rx_too_long;           
	u64 rx_jabber;             
	u64 rx_fcs_err;            
	u64 rx_len_err;            
	u64 rx_symbol_err;         
	u64 rx_runt;               
	u64 rx_frames_64;          
	u64 rx_frames_65_127;
	u64 rx_frames_128_255;
	u64 rx_frames_256_511;
	u64 rx_frames_512_1023;
	u64 rx_frames_1024_1518;
	u64 rx_frames_1519_max;
	u64 rx_pause;              
	u64 rx_ppp0;               
	u64 rx_ppp1;               
	u64 rx_ppp2;               
	u64 rx_ppp3;               
	u64 rx_ppp4;               
	u64 rx_ppp5;               
	u64 rx_ppp6;               
	u64 rx_ppp7;               
	u64 rx_ovflow0;            
	u64 rx_ovflow1;            
	u64 rx_ovflow2;            
	u64 rx_ovflow3;            
	u64 rx_trunc0;             
	u64 rx_trunc1;             
	u64 rx_trunc2;             
	u64 rx_trunc3;             
};
struct lb_port_stats {
	u64 octets;
	u64 frames;
	u64 bcast_frames;
	u64 mcast_frames;
	u64 ucast_frames;
	u64 error_frames;
	u64 frames_64;
	u64 frames_65_127;
	u64 frames_128_255;
	u64 frames_256_511;
	u64 frames_512_1023;
	u64 frames_1024_1518;
	u64 frames_1519_max;
	u64 drop;
	u64 ovflow0;
	u64 ovflow1;
	u64 ovflow2;
	u64 ovflow3;
	u64 trunc0;
	u64 trunc1;
	u64 trunc2;
	u64 trunc3;
};
struct tp_tcp_stats {
	u32 tcp_out_rsts;
	u64 tcp_in_segs;
	u64 tcp_out_segs;
	u64 tcp_retrans_segs;
};
struct tp_usm_stats {
	u32 frames;
	u32 drops;
	u64 octets;
};
struct tp_fcoe_stats {
	u32 frames_ddp;
	u32 frames_drop;
	u64 octets_ddp;
};
struct tp_err_stats {
	u32 mac_in_errs[4];
	u32 hdr_in_errs[4];
	u32 tcp_in_errs[4];
	u32 tnl_cong_drops[4];
	u32 ofld_chan_drops[4];
	u32 tnl_tx_drops[4];
	u32 ofld_vlan_drops[4];
	u32 tcp6_in_errs[4];
	u32 ofld_no_neigh;
	u32 ofld_cong_defer;
};
struct tp_cpl_stats {
	u32 req[4];
	u32 rsp[4];
};
struct tp_rdma_stats {
	u32 rqe_dfr_pkt;
	u32 rqe_dfr_mod;
};
struct sge_params {
	u32 hps;			 
	u32 eq_qpp;			 
	u32 iq_qpp;			 
};
struct tp_params {
	unsigned int tre;             
	unsigned int la_mask;         
	unsigned short tx_modq_map;   
	uint32_t dack_re;             
	unsigned short tx_modq[NCHAN];	 
	u32 vlan_pri_map;                
	u32 filter_mask;
	u32 ingress_config;              
	int rx_pkt_encap;
	int fcoe_shift;
	int port_shift;
	int vnic_shift;
	int vlan_shift;
	int tos_shift;
	int protocol_shift;
	int ethertype_shift;
	int macmatch_shift;
	int matchtype_shift;
	int frag_shift;
	u64 hash_filter_mask;
};
struct vpd_params {
	unsigned int cclk;
	u8 sn[SERNUM_LEN + 1];
	u8 id[ID_LEN + 1];
	u8 pn[PN_LEN + 1];
	u8 na[MACADDR_LEN + 1];
};
struct pf_resources {
	unsigned int nvi;		 
	unsigned int neq;		 
	unsigned int nethctrl;		 
	unsigned int niqflint;		 
	unsigned int niq;		 
	unsigned int tc;		 
	unsigned int pmask;		 
	unsigned int nexactf;		 
	unsigned int r_caps;		 
	unsigned int wx_caps;		 
};
struct pci_params {
	unsigned char speed;
	unsigned char width;
};
struct devlog_params {
	u32 memtype;                     
	u32 start;                       
	u32 size;                        
};
struct arch_specific_params {
	u8 nchan;
	u8 pm_stats_cnt;
	u8 cng_ch_bits_log;		 
	u16 mps_rplc_size;
	u16 vfcount;
	u32 sge_fl_db;
	u16 mps_tcam_size;
};
struct adapter_params {
	struct sge_params sge;
	struct tp_params  tp;
	struct vpd_params vpd;
	struct pf_resources pfres;
	struct pci_params pci;
	struct devlog_params devlog;
	enum pcie_memwin drv_memwin;
	unsigned int cim_la_size;
	unsigned int sf_size;              
	unsigned int sf_nsec;              
	unsigned int fw_vers;		   
	unsigned int bs_vers;		   
	unsigned int tp_vers;		   
	unsigned int er_vers;		   
	unsigned int scfg_vers;		   
	unsigned int vpd_vers;		   
	u8 api_vers[7];
	unsigned short mtus[NMTUS];
	unsigned short a_wnd[NCCTRL_WIN];
	unsigned short b_wnd[NCCTRL_WIN];
	unsigned char nports;              
	unsigned char portvec;
	enum chip_type chip;                
	struct arch_specific_params arch;   
	unsigned char offload;
	unsigned char crypto;		 
	unsigned char ethofld;		 
	unsigned char bypass;
	unsigned char hash_filter;
	unsigned int ofldq_wr_cred;
	bool ulptx_memwrite_dsgl;           
	unsigned int nsched_cls;           
	unsigned int max_ordird_qp;        
	unsigned int max_ird_adapter;      
	bool fr_nsmr_tpte_wr_support;	   
	u8 fw_caps_support;		 
	bool filter2_wr_support;	 
	unsigned int viid_smt_extn_support:1;  
	u8 mps_bg_map[MAX_NPORTS];	 
	bool write_w_imm_support;        
	bool write_cmpl_support;         
};
struct sge_idma_monitor_state {
	unsigned int idma_1s_thresh;	 
	unsigned int idma_stalled[2];	 
	unsigned int idma_state[2];	 
	unsigned int idma_qid[2];	 
	unsigned int idma_warn[2];	 
};
struct mbox_cmd {
	u64 cmd[MBOX_LEN / 8];		 
	u64 timestamp;			 
	u32 seqno;			 
	s16 access;			 
	s16 execute;			 
};
struct mbox_cmd_log {
	unsigned int size;		 
	unsigned int cursor;		 
	u32 seqno;			 
};
static inline struct mbox_cmd *mbox_cmd_log_entry(struct mbox_cmd_log *log,
						  unsigned int entry_idx)
{
	return &((struct mbox_cmd *)&(log)[1])[entry_idx];
}
#define FW_VERSION(chip) ( \
		FW_HDR_FW_VER_MAJOR_G(chip##FW_VERSION_MAJOR) | \
		FW_HDR_FW_VER_MINOR_G(chip##FW_VERSION_MINOR) | \
		FW_HDR_FW_VER_MICRO_G(chip##FW_VERSION_MICRO) | \
		FW_HDR_FW_VER_BUILD_G(chip##FW_VERSION_BUILD))
#define FW_INTFVER(chip, intf) (FW_HDR_INTFVER_##intf)
struct cxgb4_ethtool_lb_test {
	struct completion completion;
	int result;
	int loopback;
};
struct fw_info {
	u8 chip;
	char *fs_name;
	char *fw_mod_name;
	struct fw_hdr fw_hdr;
};
struct trace_params {
	u32 data[TRACE_LEN / 4];
	u32 mask[TRACE_LEN / 4];
	unsigned short snap_len;
	unsigned short min_len;
	unsigned char skip_ofst;
	unsigned char skip_len;
	unsigned char invert;
	unsigned char port;
};
struct cxgb4_fw_data {
	__be32 signature;
	__u8 reserved[4];
};
typedef u16 fw_port_cap16_t;	 
typedef u32 fw_port_cap32_t;	 
enum fw_caps {
	FW_CAPS_UNKNOWN	= 0,	 
	FW_CAPS16	= 1,	 
	FW_CAPS32	= 2,	 
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
	enum cc_pause  advertised_fc;     
	enum cc_fec    requested_fec;	  
	enum cc_fec    fec;		  
	unsigned char  autoneg;           
	unsigned char  link_ok;           
	unsigned char  link_down_rc;      
	bool new_module;		  
	bool redo_l1cfg;		  
};
#define FW_LEN16(fw_struct) FW_CMD_LEN16_V(sizeof(fw_struct) / 16)
enum {
	MAX_ETH_QSETS = 32,            
	MAX_OFLD_QSETS = 16,           
	MAX_CTRL_QUEUES = NCHAN,       
};
enum {
	MAX_TXQ_ENTRIES      = 16384,
	MAX_CTRL_TXQ_ENTRIES = 1024,
	MAX_RSPQ_ENTRIES     = 16384,
	MAX_RX_BUFFERS       = 16384,
	MIN_TXQ_ENTRIES      = 32,
	MIN_CTRL_TXQ_ENTRIES = 32,
	MIN_RSPQ_ENTRIES     = 128,
	MIN_FL_ENTRIES       = 16
};
enum {
	MAX_TXQ_DESC_SIZE      = 64,
	MAX_RXQ_DESC_SIZE      = 128,
	MAX_FL_DESC_SIZE       = 8,
	MAX_CTRL_TXQ_DESC_SIZE = 64,
};
enum {
	INGQ_EXTRAS = 2,         
	MAX_INGQ = MAX_ETH_QSETS + INGQ_EXTRAS,
};
enum {
	PRIV_FLAG_PORT_TX_VM_BIT,
};
#define PRIV_FLAG_PORT_TX_VM		BIT(PRIV_FLAG_PORT_TX_VM_BIT)
#define PRIV_FLAGS_ADAP			0
#define PRIV_FLAGS_PORT			PRIV_FLAG_PORT_TX_VM
struct adapter;
struct sge_rspq;
#include "cxgb4_dcb.h"
#ifdef CONFIG_CHELSIO_T4_FCOE
#include "cxgb4_fcoe.h"
#endif  
struct port_info {
	struct adapter *adapter;
	u16    viid;
	int    xact_addr_filt;         
	u16    rss_size;               
	s8     mdio_addr;
	enum fw_port_type port_type;
	u8     mod_type;
	u8     port_id;
	u8     tx_chan;
	u8     lport;                  
	u8     nqsets;                 
	u8     first_qset;             
	u8     rss_mode;
	struct link_config link_cfg;
	u16   *rss;
	struct port_stats stats_base;
#ifdef CONFIG_CHELSIO_T4_DCB
	struct port_dcb_info dcb;      
#endif
#ifdef CONFIG_CHELSIO_T4_FCOE
	struct cxgb_fcoe fcoe;
#endif  
	bool rxtstamp;   
	struct hwtstamp_config tstamp_config;
	bool ptp_enable;
	struct sched_table *sched_tbl;
	u32 eth_flags;
	u8 vin;
	u8 vivld;
	u8 smt_idx;
	u8 rx_cchan;
	bool tc_block_shared;
	u16 viid_mirror;
	u16 nmirrorqsets;
	u32 vi_mirror_count;
	struct mutex vi_mirror_mutex;  
	struct cxgb4_ethtool_lb_test ethtool_lb;
};
struct dentry;
struct work_struct;
enum {                                  
	CXGB4_FULL_INIT_DONE		= (1 << 0),
	CXGB4_DEV_ENABLED		= (1 << 1),
	CXGB4_USING_MSI			= (1 << 2),
	CXGB4_USING_MSIX		= (1 << 3),
	CXGB4_FW_OK			= (1 << 4),
	CXGB4_RSS_TNLALLLOOKUP		= (1 << 5),
	CXGB4_USING_SOFT_PARAMS		= (1 << 6),
	CXGB4_MASTER_PF			= (1 << 7),
	CXGB4_FW_OFLD_CONN		= (1 << 9),
	CXGB4_ROOT_NO_RELAXED_ORDERING	= (1 << 10),
	CXGB4_SHUTTING_DOWN		= (1 << 11),
	CXGB4_SGE_DBQ_TIMER		= (1 << 12),
};
enum {
	ULP_CRYPTO_LOOKASIDE = 1 << 0,
	ULP_CRYPTO_IPSEC_INLINE = 1 << 1,
	ULP_CRYPTO_KTLS_INLINE  = 1 << 3,
};
#define CXGB4_MIRROR_RXQ_DEFAULT_DESC_NUM 1024
#define CXGB4_MIRROR_RXQ_DEFAULT_DESC_SIZE 64
#define CXGB4_MIRROR_RXQ_DEFAULT_INTR_USEC 5
#define CXGB4_MIRROR_RXQ_DEFAULT_PKT_CNT 8
#define CXGB4_MIRROR_FLQ_DEFAULT_DESC_NUM 72
struct rx_sw_desc;
struct sge_fl {                      
	unsigned int avail;          
	unsigned int pend_cred;      
	unsigned int cidx;           
	unsigned int pidx;           
	unsigned long alloc_failed;  
	unsigned long large_alloc_failed;
	unsigned long mapping_err;   
	unsigned long low;           
	unsigned long starving;
	unsigned int cntxt_id;       
	unsigned int size;           
	struct rx_sw_desc *sdesc;    
	__be64 *desc;                
	dma_addr_t addr;             
	void __iomem *bar2_addr;     
	unsigned int bar2_qid;       
};
struct pkt_gl {
	u64 sgetstamp;		     
	struct page_frag frags[MAX_SKB_FRAGS];
	void *va;                          
	unsigned int nfrags;               
	unsigned int tot_len;              
};
typedef int (*rspq_handler_t)(struct sge_rspq *q, const __be64 *rsp,
			      const struct pkt_gl *gl);
typedef void (*rspq_flush_handler_t)(struct sge_rspq *q);
struct t4_lro_mgr {
#define MAX_LRO_SESSIONS		64
	u8 lro_session_cnt;          
	unsigned long lro_pkts;      
	unsigned long lro_merged;    
	struct sk_buff_head lroq;    
};
struct sge_rspq {                    
	struct napi_struct napi;
	const __be64 *cur_desc;      
	unsigned int cidx;           
	u8 gen;                      
	u8 intr_params;              
	u8 next_intr_params;         
	u8 adaptive_rx;
	u8 pktcnt_idx;               
	u8 uld;                      
	u8 idx;                      
	int offset;                  
	u16 cntxt_id;                
	u16 abs_id;                  
	__be64 *desc;                
	dma_addr_t phys_addr;        
	void __iomem *bar2_addr;     
	unsigned int bar2_qid;       
	unsigned int iqe_len;        
	unsigned int size;           
	struct adapter *adap;
	struct net_device *netdev;   
	rspq_handler_t handler;
	rspq_flush_handler_t flush_handler;
	struct t4_lro_mgr lro_mgr;
};
struct sge_eth_stats {               
	unsigned long pkts;          
	unsigned long lro_pkts;      
	unsigned long lro_merged;    
	unsigned long rx_cso;        
	unsigned long vlan_ex;       
	unsigned long rx_drops;      
	unsigned long bad_rx_pkts;   
};
struct sge_eth_rxq {                 
	struct sge_rspq rspq;
	struct sge_fl fl;
	struct sge_eth_stats stats;
	struct msix_info *msix;
} ____cacheline_aligned_in_smp;
struct sge_ofld_stats {              
	unsigned long pkts;          
	unsigned long imm;           
	unsigned long an;            
	unsigned long nomem;         
};
struct sge_ofld_rxq {                
	struct sge_rspq rspq;
	struct sge_fl fl;
	struct sge_ofld_stats stats;
	struct msix_info *msix;
} ____cacheline_aligned_in_smp;
struct tx_desc {
	__be64 flit[8];
};
struct ulptx_sgl;
struct tx_sw_desc {
	struct sk_buff *skb;  
	dma_addr_t addr[MAX_SKB_FRAGS + 1];  
};
struct sge_txq {
	unsigned int  in_use;        
	unsigned int  q_type;	     
	unsigned int  size;          
	unsigned int  cidx;          
	unsigned int  pidx;          
	unsigned long stops;         
	unsigned long restarts;      
	unsigned int  cntxt_id;      
	struct tx_desc *desc;        
	struct tx_sw_desc *sdesc;    
	struct sge_qstat *stat;      
	dma_addr_t    phys_addr;     
	spinlock_t db_lock;
	int db_disabled;
	unsigned short db_pidx;
	unsigned short db_pidx_inc;
	void __iomem *bar2_addr;     
	unsigned int bar2_qid;       
};
struct sge_eth_txq {                 
	struct sge_txq q;
	struct netdev_queue *txq;    
#ifdef CONFIG_CHELSIO_T4_DCB
	u8 dcb_prio;		     
#endif
	u8 dbqt;                     
	unsigned int dbqtimerix;     
	unsigned long tso;           
	unsigned long uso;           
	unsigned long tx_cso;        
	unsigned long vlan_ins;      
	unsigned long mapping_err;   
} ____cacheline_aligned_in_smp;
struct sge_uld_txq {                
	struct sge_txq q;
	struct adapter *adap;
	struct sk_buff_head sendq;   
	struct tasklet_struct qresume_tsk;  
	bool service_ofldq_running;  
	u8 full;                     
	unsigned long mapping_err;   
} ____cacheline_aligned_in_smp;
struct sge_ctrl_txq {                
	struct sge_txq q;
	struct adapter *adap;
	struct sk_buff_head sendq;   
	struct tasklet_struct qresume_tsk;  
	u8 full;                     
} ____cacheline_aligned_in_smp;
struct sge_uld_rxq_info {
	char name[IFNAMSIZ];	 
	struct sge_ofld_rxq *uldrxq;  
	u16 *rspq_id;		 
	u16 nrxq;		 
	u16 nciq;		 
	u8 uld;			 
};
struct sge_uld_txq_info {
	struct sge_uld_txq *uldtxq;  
	atomic_t users;		 
	u16 ntxq;		 
};
struct cxgb4_uld_list {
	struct cxgb4_uld_info uld_info;
	struct list_head list_node;
	enum cxgb4_uld uld_type;
};
enum sge_eosw_state {
	CXGB4_EO_STATE_CLOSED = 0,  
	CXGB4_EO_STATE_FLOWC_OPEN_SEND,  
	CXGB4_EO_STATE_FLOWC_OPEN_REPLY,  
	CXGB4_EO_STATE_ACTIVE,  
	CXGB4_EO_STATE_FLOWC_CLOSE_SEND,  
	CXGB4_EO_STATE_FLOWC_CLOSE_REPLY,  
};
struct sge_eosw_txq {
	spinlock_t lock;  
	enum sge_eosw_state state;  
	struct tx_sw_desc *desc;  
	u32 ndesc;  
	u32 pidx;  
	u32 last_pidx;  
	u32 cidx;  
	u32 last_cidx;  
	u32 flowc_idx;  
	u32 inuse;  
	u32 cred;  
	u32 ncompl;  
	u32 last_compl;  
	u32 eotid;  
	u32 hwtid;  
	u32 hwqid;  
	struct net_device *netdev;  
	struct tasklet_struct qresume_tsk;  
	struct completion completion;  
};
struct sge_eohw_txq {
	spinlock_t lock;  
	struct sge_txq q;  
	struct adapter *adap;  
	unsigned long tso;  
	unsigned long uso;  
	unsigned long tx_cso;  
	unsigned long vlan_ins;  
	unsigned long mapping_err;  
};
struct sge {
	struct sge_eth_txq ethtxq[MAX_ETH_QSETS];
	struct sge_eth_txq ptptxq;
	struct sge_ctrl_txq ctrlq[MAX_CTRL_QUEUES];
	struct sge_eth_rxq ethrxq[MAX_ETH_QSETS];
	struct sge_rspq fw_evtq ____cacheline_aligned_in_smp;
	struct sge_uld_rxq_info **uld_rxq_info;
	struct sge_uld_txq_info **uld_txq_info;
	struct sge_rspq intrq ____cacheline_aligned_in_smp;
	spinlock_t intrq_lock;
	struct sge_eohw_txq *eohw_txq;
	struct sge_ofld_rxq *eohw_rxq;
	struct sge_eth_rxq *mirror_rxq[NCHAN];
	u16 max_ethqsets;            
	u16 ethqsets;                
	u16 ethtxq_rover;            
	u16 ofldqsets;               
	u16 nqs_per_uld;	     
	u16 eoqsets;                 
	u16 mirrorqsets;             
	u16 timer_val[SGE_NTIMERS];
	u8 counter_val[SGE_NCOUNTERS];
	u16 dbqtimer_tick;
	u16 dbqtimer_val[SGE_NDBQTIMERS];
	u32 fl_pg_order;             
	u32 stat_len;                
	u32 pktshift;                
	u32 fl_align;                
	u32 fl_starve_thres;         
	struct sge_idma_monitor_state idma_monitor;
	unsigned int egr_start;
	unsigned int egr_sz;
	unsigned int ingr_start;
	unsigned int ingr_sz;
	void **egr_map;     
	struct sge_rspq **ingr_map;  
	unsigned long *starving_fl;
	unsigned long *txq_maperr;
	unsigned long *blocked_fl;
	struct timer_list rx_timer;  
	struct timer_list tx_timer;  
	int fwevtq_msix_idx;  
	int nd_msix_idx;  
};
#define for_each_ethrxq(sge, i) for (i = 0; i < (sge)->ethqsets; i++)
#define for_each_ofldtxq(sge, i) for (i = 0; i < (sge)->ofldqsets; i++)
struct l2t_data;
#ifdef CONFIG_PCI_IOV
#define NUM_OF_PF_WITH_SRIOV 4
#endif
struct doorbell_stats {
	u32 db_drop;
	u32 db_empty;
	u32 db_full;
};
struct hash_mac_addr {
	struct list_head list;
	u8 addr[ETH_ALEN];
	unsigned int iface_mac;
};
struct msix_bmap {
	unsigned long *msix_bmap;
	unsigned int mapsize;
	spinlock_t lock;  
};
struct msix_info {
	unsigned short vec;
	char desc[IFNAMSIZ + 10];
	unsigned int idx;
	cpumask_var_t aff_mask;
};
struct vf_info {
	unsigned char vf_mac_addr[ETH_ALEN];
	unsigned int tx_rate;
	bool pf_set_mac;
	u16 vlan;
	int link_state;
};
enum {
	HMA_DMA_MAPPED_FLAG = 1
};
struct hma_data {
	unsigned char flags;
	struct sg_table *sgt;
	dma_addr_t *phy_addr;	 
};
struct mbox_list {
	struct list_head list;
};
#if IS_ENABLED(CONFIG_THERMAL)
struct ch_thermal {
	struct thermal_zone_device *tzdev;
};
#endif
struct mps_entries_ref {
	struct list_head list;
	u8 addr[ETH_ALEN];
	u8 mask[ETH_ALEN];
	u16 idx;
	refcount_t refcnt;
};
struct cxgb4_ethtool_filter_info {
	u32 *loc_array;  
	unsigned long *bmap;  
	u32 in_use;  
};
struct cxgb4_ethtool_filter {
	u32 nentries;  
	struct cxgb4_ethtool_filter_info *port;  
};
struct adapter {
	void __iomem *regs;
	void __iomem *bar2;
	u32 t4_bar0;
	struct pci_dev *pdev;
	struct device *pdev_dev;
	const char *name;
	unsigned int mbox;
	unsigned int pf;
	unsigned int flags;
	unsigned int adap_idx;
	enum chip_type chip;
	u32 eth_flags;
	int msg_enable;
	__be16 vxlan_port;
	__be16 geneve_port;
	struct adapter_params params;
	struct cxgb4_virt_res vres;
	unsigned int swintr;
	struct msix_info *msix_info;
	struct msix_bmap msix_bmap;
	struct doorbell_stats db_stats;
	struct sge sge;
	struct net_device *port[MAX_NPORTS];
	u8 chan_map[NCHAN];                    
	struct vf_info *vfinfo;
	u8 num_vfs;
	u32 filter_mode;
	unsigned int l2t_start;
	unsigned int l2t_end;
	struct l2t_data *l2t;
	unsigned int clipt_start;
	unsigned int clipt_end;
	struct clip_tbl *clipt;
	unsigned int rawf_start;
	unsigned int rawf_cnt;
	struct smt_data *smt;
	struct cxgb4_uld_info *uld;
	void *uld_handle[CXGB4_ULD_MAX];
	unsigned int num_uld;
	unsigned int num_ofld_uld;
	struct list_head list_node;
	struct list_head rcu_node;
	struct list_head mac_hlist;  
	struct list_head mps_ref;
	spinlock_t mps_ref_lock;  
	void *iscsi_ppm;
	struct tid_info tids;
	void **tid_release_head;
	spinlock_t tid_release_lock;
	struct workqueue_struct *workq;
	struct work_struct tid_release_task;
	struct work_struct db_full_task;
	struct work_struct db_drop_task;
	struct work_struct fatal_err_notify_task;
	bool tid_release_task_busy;
	spinlock_t mbox_lock;
	struct mbox_list mlist;
#define T4_OS_LOG_MBOX_CMDS 256
	struct mbox_cmd_log *mbox_log;
	struct mutex uld_mutex;
	struct dentry *debugfs_root;
	bool use_bd;      
	bool trace_rss;	 
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_clock_info;
	struct sk_buff *ptp_tx_skb;
	spinlock_t ptp_lock;
	spinlock_t stats_lock;
	spinlock_t win0_lock ____cacheline_aligned_in_smp;
	struct cxgb4_tc_u32_table *tc_u32;
	struct chcr_ktls chcr_ktls;
	struct chcr_stats_debug chcr_stats;
#if IS_ENABLED(CONFIG_CHELSIO_TLS_DEVICE)
	struct ch_ktls_stats_debug ch_ktls_stats;
#endif
#if IS_ENABLED(CONFIG_CHELSIO_IPSEC_INLINE)
	struct ch_ipsec_stats_debug ch_ipsec_stats;
#endif
	bool tc_flower_initialized;
	struct rhashtable flower_tbl;
	struct rhashtable_params flower_ht_params;
	struct timer_list flower_stats_timer;
	struct work_struct flower_stats_work;
	struct ethtool_dump eth_dump;
	struct hma_data hma;
	struct srq_data *srq;
	struct vmcoredd_data vmcoredd;
#if IS_ENABLED(CONFIG_THERMAL)
	struct ch_thermal ch_thermal;
#endif
	struct cxgb4_tc_mqprio *tc_mqprio;
	struct cxgb4_tc_matchall *tc_matchall;
	struct cxgb4_ethtool_filter *ethtool_filters;
};
struct ch_sched_params {
	u8   type;                      
	union {
		struct {
			u8   level;     
			u8   mode;      
			u8   rateunit;  
			u8   ratemode;  
			u8   channel;   
			u8   class;     
			u32  minrate;   
			u32  maxrate;   
			u16  weight;    
			u16  pktsize;   
			u16  burstsize;   
		} params;
	} u;
};
enum {
	SCHED_CLASS_TYPE_PACKET = 0,     
};
enum {
	SCHED_CLASS_LEVEL_CL_RL = 0,     
	SCHED_CLASS_LEVEL_CH_RL = 2,     
};
enum {
	SCHED_CLASS_MODE_CLASS = 0,      
	SCHED_CLASS_MODE_FLOW,           
};
enum {
	SCHED_CLASS_RATEUNIT_BITS = 0,   
};
enum {
	SCHED_CLASS_RATEMODE_ABS = 1,    
};
struct ch_sched_queue {
	s8   queue;     
	s8   class;     
};
struct ch_sched_flowc {
	s32 tid;    
	s8  class;  
};
#define ETHTYPE_BITWIDTH 16
#define FRAG_BITWIDTH 1
#define MACIDX_BITWIDTH 9
#define FCOE_BITWIDTH 1
#define IPORT_BITWIDTH 3
#define MATCHTYPE_BITWIDTH 3
#define PROTO_BITWIDTH 8
#define TOS_BITWIDTH 8
#define PF_BITWIDTH 8
#define VF_BITWIDTH 8
#define IVLAN_BITWIDTH 16
#define OVLAN_BITWIDTH 16
#define ENCAP_VNI_BITWIDTH 24
struct ch_filter_tuple {
	uint32_t ethtype:ETHTYPE_BITWIDTH;       
	uint32_t frag:FRAG_BITWIDTH;             
	uint32_t ivlan_vld:1;                    
	uint32_t ovlan_vld:1;                    
	uint32_t pfvf_vld:1;                     
	uint32_t encap_vld:1;			 
	uint32_t macidx:MACIDX_BITWIDTH;         
	uint32_t fcoe:FCOE_BITWIDTH;             
	uint32_t iport:IPORT_BITWIDTH;           
	uint32_t matchtype:MATCHTYPE_BITWIDTH;   
	uint32_t proto:PROTO_BITWIDTH;           
	uint32_t tos:TOS_BITWIDTH;               
	uint32_t pf:PF_BITWIDTH;                 
	uint32_t vf:VF_BITWIDTH;                 
	uint32_t ivlan:IVLAN_BITWIDTH;           
	uint32_t ovlan:OVLAN_BITWIDTH;           
	uint32_t vni:ENCAP_VNI_BITWIDTH;	 
	uint8_t lip[16];         
	uint8_t fip[16];         
	uint16_t lport;          
	uint16_t fport;          
};
struct ch_filter_specification {
	uint32_t hitcnts:1;      
	uint32_t prio:1;         
	uint32_t type:1;         
	u32 hash:1;		 
	uint32_t action:2;       
	uint32_t rpttid:1;       
	uint32_t dirsteer:1;     
	uint32_t iq:10;          
	uint32_t maskhash:1;     
	uint32_t dirsteerhash:1; 
	uint32_t eport:2;        
	uint32_t newdmac:1;      
	uint32_t newsmac:1;      
	uint32_t newvlan:2;      
	uint32_t nat_mode:3;     
	uint8_t dmac[ETH_ALEN];  
	uint8_t smac[ETH_ALEN];  
	uint16_t vlan;           
	u8 nat_lip[16];		 
	u8 nat_fip[16];		 
	u16 nat_lport;		 
	u16 nat_fport;		 
	u32 tc_prio;		 
	u64 tc_cookie;		 
	u8 rsvd[12];
	struct ch_filter_tuple val;
	struct ch_filter_tuple mask;
};
enum {
	FILTER_PASS = 0,         
	FILTER_DROP,
	FILTER_SWITCH
};
enum {
	VLAN_NOCHANGE = 0,       
	VLAN_REMOVE,
	VLAN_INSERT,
	VLAN_REWRITE
};
enum {
	NAT_MODE_NONE = 0,	 
	NAT_MODE_DIP,		 
	NAT_MODE_DIP_DP,	 
	NAT_MODE_DIP_DP_SIP,	 
	NAT_MODE_DIP_DP_SP,	 
	NAT_MODE_SIP_SP,	 
	NAT_MODE_DIP_SIP_SP,	 
	NAT_MODE_ALL		 
};
#define CXGB4_FILTER_TYPE_MAX 2
struct filter_entry {
	u32 valid:1;             
	u32 locked:1;            
	u32 pending:1;           
	struct filter_ctx *ctx;  
	struct l2t_entry *l2t;   
	struct smt_entry *smt;   
	struct net_device *dev;  
	u32 tid;                 
	struct ch_filter_specification fs;
};
static inline int is_offload(const struct adapter *adap)
{
	return adap->params.offload;
}
static inline int is_hashfilter(const struct adapter *adap)
{
	return adap->params.hash_filter;
}
static inline int is_pci_uld(const struct adapter *adap)
{
	return adap->params.crypto;
}
static inline int is_uld(const struct adapter *adap)
{
	return (adap->params.offload || adap->params.crypto);
}
static inline int is_ethofld(const struct adapter *adap)
{
	return adap->params.ethofld;
}
static inline u32 t4_read_reg(struct adapter *adap, u32 reg_addr)
{
	return readl(adap->regs + reg_addr);
}
static inline void t4_write_reg(struct adapter *adap, u32 reg_addr, u32 val)
{
	writel(val, adap->regs + reg_addr);
}
#ifndef readq
static inline u64 readq(const volatile void __iomem *addr)
{
	return readl(addr) + ((u64)readl(addr + 4) << 32);
}
static inline void writeq(u64 val, volatile void __iomem *addr)
{
	writel(val, addr);
	writel(val >> 32, addr + 4);
}
#endif
static inline u64 t4_read_reg64(struct adapter *adap, u32 reg_addr)
{
	return readq(adap->regs + reg_addr);
}
static inline void t4_write_reg64(struct adapter *adap, u32 reg_addr, u64 val)
{
	writeq(val, adap->regs + reg_addr);
}
static inline void t4_set_hw_addr(struct adapter *adapter, int port_idx,
				  u8 hw_addr[])
{
	eth_hw_addr_set(adapter->port[port_idx], hw_addr);
	ether_addr_copy(adapter->port[port_idx]->perm_addr, hw_addr);
}
static inline struct port_info *netdev2pinfo(const struct net_device *dev)
{
	return netdev_priv(dev);
}
static inline struct port_info *adap2pinfo(struct adapter *adap, int idx)
{
	return netdev_priv(adap->port[idx]);
}
static inline struct adapter *netdev2adap(const struct net_device *dev)
{
	return netdev2pinfo(dev)->adapter;
}
static inline unsigned int mk_adap_vers(struct adapter *ap)
{
	return CHELSIO_CHIP_VERSION(ap->params.chip) |
		(CHELSIO_CHIP_RELEASE(ap->params.chip) << 10) | (1 << 16);
}
static inline unsigned int qtimer_val(const struct adapter *adap,
				      const struct sge_rspq *q)
{
	unsigned int idx = q->intr_params >> 1;
	return idx < SGE_NTIMERS ? adap->sge.timer_val[idx] : 0;
}
extern char cxgb4_driver_name[];
void t4_os_portmod_changed(struct adapter *adap, int port_id);
void t4_os_link_changed(struct adapter *adap, int port_id, int link_stat);
void t4_free_sge_resources(struct adapter *adap);
void t4_free_ofld_rxqs(struct adapter *adap, int n, struct sge_ofld_rxq *q);
irq_handler_t t4_intr_handler(struct adapter *adap);
netdev_tx_t t4_start_xmit(struct sk_buff *skb, struct net_device *dev);
int cxgb4_selftest_lb_pkt(struct net_device *netdev);
int t4_ethrx_handler(struct sge_rspq *q, const __be64 *rsp,
		     const struct pkt_gl *gl);
int t4_mgmt_tx(struct adapter *adap, struct sk_buff *skb);
int t4_ofld_send(struct adapter *adap, struct sk_buff *skb);
int t4_sge_alloc_rxq(struct adapter *adap, struct sge_rspq *iq, bool fwevtq,
		     struct net_device *dev, int intr_idx,
		     struct sge_fl *fl, rspq_handler_t hnd,
		     rspq_flush_handler_t flush_handler, int cong);
int t4_sge_alloc_eth_txq(struct adapter *adap, struct sge_eth_txq *txq,
			 struct net_device *dev, struct netdev_queue *netdevq,
			 unsigned int iqid, u8 dbqt);
int t4_sge_alloc_ctrl_txq(struct adapter *adap, struct sge_ctrl_txq *txq,
			  struct net_device *dev, unsigned int iqid,
			  unsigned int cmplqid);
int t4_sge_mod_ctrl_txq(struct adapter *adap, unsigned int eqid,
			unsigned int cmplqid);
int t4_sge_alloc_uld_txq(struct adapter *adap, struct sge_uld_txq *txq,
			 struct net_device *dev, unsigned int iqid,
			 unsigned int uld_type);
int t4_sge_alloc_ethofld_txq(struct adapter *adap, struct sge_eohw_txq *txq,
			     struct net_device *dev, u32 iqid);
void t4_sge_free_ethofld_txq(struct adapter *adap, struct sge_eohw_txq *txq);
irqreturn_t t4_sge_intr_msix(int irq, void *cookie);
int t4_sge_init(struct adapter *adap);
void t4_sge_start(struct adapter *adap);
void t4_sge_stop(struct adapter *adap);
int t4_sge_eth_txq_egress_update(struct adapter *adap, struct sge_eth_txq *q,
				 int maxreclaim);
void cxgb4_set_ethtool_ops(struct net_device *netdev);
int cxgb4_write_rss(const struct port_info *pi, const u16 *queues);
enum cpl_tx_tnl_lso_type cxgb_encap_offload_supported(struct sk_buff *skb);
extern int dbfifo_int_thresh;
#define for_each_port(adapter, iter) \
	for (iter = 0; iter < (adapter)->params.nports; ++iter)
static inline int is_bypass(struct adapter *adap)
{
	return adap->params.bypass;
}
static inline int is_bypass_device(int device)
{
	switch (device) {
	case 0x440b:
	case 0x440c:
		return 1;
	default:
		return 0;
	}
}
static inline int is_10gbt_device(int device)
{
	switch (device) {
	case 0x4409:
	case 0x4486:
		return 1;
	default:
		return 0;
	}
}
static inline unsigned int core_ticks_per_usec(const struct adapter *adap)
{
	return adap->params.vpd.cclk / 1000;
}
static inline unsigned int us_to_core_ticks(const struct adapter *adap,
					    unsigned int us)
{
	return (us * adap->params.vpd.cclk) / 1000;
}
static inline unsigned int core_ticks_to_us(const struct adapter *adapter,
					    unsigned int ticks)
{
	return ((ticks * 1000 + adapter->params.vpd.cclk/2) /
		adapter->params.vpd.cclk);
}
static inline unsigned int dack_ticks_to_usec(const struct adapter *adap,
					      unsigned int ticks)
{
	return (ticks << adap->params.tp.dack_re) / core_ticks_per_usec(adap);
}
void t4_set_reg_field(struct adapter *adap, unsigned int addr, u32 mask,
		      u32 val);
int t4_wr_mbox_meat_timeout(struct adapter *adap, int mbox, const void *cmd,
			    int size, void *rpl, bool sleep_ok, int timeout);
int t4_wr_mbox_meat(struct adapter *adap, int mbox, const void *cmd, int size,
		    void *rpl, bool sleep_ok);
static inline int t4_wr_mbox_timeout(struct adapter *adap, int mbox,
				     const void *cmd, int size, void *rpl,
				     int timeout)
{
	return t4_wr_mbox_meat_timeout(adap, mbox, cmd, size, rpl, true,
				       timeout);
}
static inline int t4_wr_mbox(struct adapter *adap, int mbox, const void *cmd,
			     int size, void *rpl)
{
	return t4_wr_mbox_meat(adap, mbox, cmd, size, rpl, true);
}
static inline int t4_wr_mbox_ns(struct adapter *adap, int mbox, const void *cmd,
				int size, void *rpl)
{
	return t4_wr_mbox_meat(adap, mbox, cmd, size, rpl, false);
}
static inline int hash_mac_addr(const u8 *addr)
{
	u32 a = ((u32)addr[0] << 16) | ((u32)addr[1] << 8) | addr[2];
	u32 b = ((u32)addr[3] << 16) | ((u32)addr[4] << 8) | addr[5];
	a ^= b;
	a ^= (a >> 12);
	a ^= (a >> 6);
	return a & 0x3f;
}
int cxgb4_set_rspq_intr_params(struct sge_rspq *q, unsigned int us,
			       unsigned int cnt);
static inline void init_rspq(struct adapter *adap, struct sge_rspq *q,
			     unsigned int us, unsigned int cnt,
			     unsigned int size, unsigned int iqe_size)
{
	q->adap = adap;
	cxgb4_set_rspq_intr_params(q, us, cnt);
	q->iqe_len = iqe_size;
	q->size = size;
}
static inline bool t4_is_inserted_mod_type(unsigned int fw_mod_type)
{
	return (fw_mod_type != FW_PORT_MOD_TYPE_NONE &&
		fw_mod_type != FW_PORT_MOD_TYPE_NOTSUPPORTED &&
		fw_mod_type != FW_PORT_MOD_TYPE_UNKNOWN &&
		fw_mod_type != FW_PORT_MOD_TYPE_ERROR);
}
void t4_write_indirect(struct adapter *adap, unsigned int addr_reg,
		       unsigned int data_reg, const u32 *vals,
		       unsigned int nregs, unsigned int start_idx);
void t4_read_indirect(struct adapter *adap, unsigned int addr_reg,
		      unsigned int data_reg, u32 *vals, unsigned int nregs,
		      unsigned int start_idx);
void t4_hw_pci_read_cfg4(struct adapter *adapter, int reg, u32 *val);
struct fw_filter_wr;
void t4_intr_enable(struct adapter *adapter);
void t4_intr_disable(struct adapter *adapter);
int t4_slow_intr_handler(struct adapter *adapter);
int t4_wait_dev_ready(void __iomem *regs);
fw_port_cap32_t t4_link_acaps(struct adapter *adapter, unsigned int port,
			      struct link_config *lc);
int t4_link_l1cfg_core(struct adapter *adap, unsigned int mbox,
		       unsigned int port, struct link_config *lc,
		       u8 sleep_ok, int timeout);
static inline int t4_link_l1cfg(struct adapter *adapter, unsigned int mbox,
				unsigned int port, struct link_config *lc)
{
	return t4_link_l1cfg_core(adapter, mbox, port, lc,
				  true, FW_CMD_MAX_TIMEOUT);
}
static inline int t4_link_l1cfg_ns(struct adapter *adapter, unsigned int mbox,
				   unsigned int port, struct link_config *lc)
{
	return t4_link_l1cfg_core(adapter, mbox, port, lc,
				  false, FW_CMD_MAX_TIMEOUT);
}
int t4_restart_aneg(struct adapter *adap, unsigned int mbox, unsigned int port);
u32 t4_read_pcie_cfg4(struct adapter *adap, int reg);
u32 t4_get_util_window(struct adapter *adap);
void t4_setup_memwin(struct adapter *adap, u32 memwin_base, u32 window);
int t4_memory_rw_init(struct adapter *adap, int win, int mtype, u32 *mem_off,
		      u32 *mem_base, u32 *mem_aperture);
void t4_memory_update_win(struct adapter *adap, int win, u32 addr);
void t4_memory_rw_residual(struct adapter *adap, u32 off, u32 addr, u8 *buf,
			   int dir);
#define T4_MEMORY_WRITE	0
#define T4_MEMORY_READ	1
int t4_memory_rw(struct adapter *adap, int win, int mtype, u32 addr, u32 len,
		 void *buf, int dir);
static inline int t4_memory_write(struct adapter *adap, int mtype, u32 addr,
				  u32 len, __be32 *buf)
{
	return t4_memory_rw(adap, 0, mtype, addr, len, buf, 0);
}
unsigned int t4_get_regs_len(struct adapter *adapter);
void t4_get_regs(struct adapter *adap, void *buf, size_t buf_size);
int t4_eeprom_ptov(unsigned int phys_addr, unsigned int fn, unsigned int sz);
int t4_seeprom_wp(struct adapter *adapter, bool enable);
int t4_get_raw_vpd_params(struct adapter *adapter, struct vpd_params *p);
int t4_get_vpd_params(struct adapter *adapter, struct vpd_params *p);
int t4_get_pfres(struct adapter *adapter);
int t4_read_flash(struct adapter *adapter, unsigned int addr,
		  unsigned int nwords, u32 *data, int byte_oriented);
int t4_load_fw(struct adapter *adapter, const u8 *fw_data, unsigned int size);
int t4_load_phy_fw(struct adapter *adap, int win,
		   int (*phy_fw_version)(const u8 *, size_t),
		   const u8 *phy_fw_data, size_t phy_fw_size);
int t4_phy_fw_ver(struct adapter *adap, int *phy_fw_ver);
int t4_fwcache(struct adapter *adap, enum fw_params_param_dev_fwcache op);
int t4_fw_upgrade(struct adapter *adap, unsigned int mbox,
		  const u8 *fw_data, unsigned int size, int force);
int t4_fl_pkt_align(struct adapter *adap);
unsigned int t4_flash_cfg_addr(struct adapter *adapter);
int t4_check_fw_version(struct adapter *adap);
int t4_load_cfg(struct adapter *adapter, const u8 *cfg_data, unsigned int size);
int t4_get_fw_version(struct adapter *adapter, u32 *vers);
int t4_get_bs_version(struct adapter *adapter, u32 *vers);
int t4_get_tp_version(struct adapter *adapter, u32 *vers);
int t4_get_exprom_version(struct adapter *adapter, u32 *vers);
int t4_get_scfg_version(struct adapter *adapter, u32 *vers);
int t4_get_vpd_version(struct adapter *adapter, u32 *vers);
int t4_get_version_info(struct adapter *adapter);
void t4_dump_version_info(struct adapter *adapter);
int t4_prep_fw(struct adapter *adap, struct fw_info *fw_info,
	       const u8 *fw_data, unsigned int fw_size,
	       struct fw_hdr *card_fw, enum dev_state state, int *reset);
int t4_prep_adapter(struct adapter *adapter);
int t4_shutdown_adapter(struct adapter *adapter);
enum t4_bar2_qtype { T4_BAR2_QTYPE_EGRESS, T4_BAR2_QTYPE_INGRESS };
int t4_bar2_sge_qregs(struct adapter *adapter,
		      unsigned int qid,
		      enum t4_bar2_qtype qtype,
		      int user,
		      u64 *pbar2_qoffset,
		      unsigned int *pbar2_qid);
unsigned int qtimer_val(const struct adapter *adap,
			const struct sge_rspq *q);
int t4_init_devlog_params(struct adapter *adapter);
int t4_init_sge_params(struct adapter *adapter);
int t4_init_tp_params(struct adapter *adap, bool sleep_ok);
int t4_filter_field_shift(const struct adapter *adap, int filter_sel);
int t4_init_rss_mode(struct adapter *adap, int mbox);
int t4_init_portinfo(struct port_info *pi, int mbox,
		     int port, int pf, int vf, u8 mac[]);
int t4_port_init(struct adapter *adap, int mbox, int pf, int vf);
int t4_init_port_mirror(struct port_info *pi, u8 mbox, u8 port, u8 pf, u8 vf,
			u16 *mirror_viid);
void t4_fatal_err(struct adapter *adapter);
unsigned int t4_chip_rss_size(struct adapter *adapter);
int t4_config_rss_range(struct adapter *adapter, int mbox, unsigned int viid,
			int start, int n, const u16 *rspq, unsigned int nrspq);
int t4_config_glbl_rss(struct adapter *adapter, int mbox, unsigned int mode,
		       unsigned int flags);
int t4_config_vi_rss(struct adapter *adapter, int mbox, unsigned int viid,
		     unsigned int flags, unsigned int defq);
int t4_read_rss(struct adapter *adapter, u16 *entries);
void t4_read_rss_key(struct adapter *adapter, u32 *key, bool sleep_ok);
void t4_write_rss_key(struct adapter *adap, const u32 *key, int idx,
		      bool sleep_ok);
void t4_read_rss_pf_config(struct adapter *adapter, unsigned int index,
			   u32 *valp, bool sleep_ok);
void t4_read_rss_vf_config(struct adapter *adapter, unsigned int index,
			   u32 *vfl, u32 *vfh, bool sleep_ok);
u32 t4_read_rss_pf_map(struct adapter *adapter, bool sleep_ok);
u32 t4_read_rss_pf_mask(struct adapter *adapter, bool sleep_ok);
unsigned int t4_get_mps_bg_map(struct adapter *adapter, int pidx);
unsigned int t4_get_tp_ch_map(struct adapter *adapter, int pidx);
void t4_pmtx_get_stats(struct adapter *adap, u32 cnt[], u64 cycles[]);
void t4_pmrx_get_stats(struct adapter *adap, u32 cnt[], u64 cycles[]);
int t4_read_cim_ibq(struct adapter *adap, unsigned int qid, u32 *data,
		    size_t n);
int t4_read_cim_obq(struct adapter *adap, unsigned int qid, u32 *data,
		    size_t n);
int t4_cim_read(struct adapter *adap, unsigned int addr, unsigned int n,
		unsigned int *valp);
int t4_cim_write(struct adapter *adap, unsigned int addr, unsigned int n,
		 const unsigned int *valp);
int t4_cim_read_la(struct adapter *adap, u32 *la_buf, unsigned int *wrptr);
void t4_cim_read_pif_la(struct adapter *adap, u32 *pif_req, u32 *pif_rsp,
			unsigned int *pif_req_wrptr,
			unsigned int *pif_rsp_wrptr);
void t4_cim_read_ma_la(struct adapter *adap, u32 *ma_req, u32 *ma_rsp);
void t4_read_cimq_cfg(struct adapter *adap, u16 *base, u16 *size, u16 *thres);
const char *t4_get_port_type_description(enum fw_port_type port_type);
void t4_get_port_stats(struct adapter *adap, int idx, struct port_stats *p);
void t4_get_port_stats_offset(struct adapter *adap, int idx,
			      struct port_stats *stats,
			      struct port_stats *offset);
void t4_get_lb_stats(struct adapter *adap, int idx, struct lb_port_stats *p);
void t4_read_mtu_tbl(struct adapter *adap, u16 *mtus, u8 *mtu_log);
void t4_read_cong_tbl(struct adapter *adap, u16 incr[NMTUS][NCCTRL_WIN]);
void t4_tp_wr_bits_indirect(struct adapter *adap, unsigned int addr,
			    unsigned int mask, unsigned int val);
void t4_tp_read_la(struct adapter *adap, u64 *la_buf, unsigned int *wrptr);
void t4_tp_get_err_stats(struct adapter *adap, struct tp_err_stats *st,
			 bool sleep_ok);
void t4_tp_get_cpl_stats(struct adapter *adap, struct tp_cpl_stats *st,
			 bool sleep_ok);
void t4_tp_get_rdma_stats(struct adapter *adap, struct tp_rdma_stats *st,
			  bool sleep_ok);
void t4_get_usm_stats(struct adapter *adap, struct tp_usm_stats *st,
		      bool sleep_ok);
void t4_tp_get_tcp_stats(struct adapter *adap, struct tp_tcp_stats *v4,
			 struct tp_tcp_stats *v6, bool sleep_ok);
void t4_get_fcoe_stats(struct adapter *adap, unsigned int idx,
		       struct tp_fcoe_stats *st, bool sleep_ok);
void t4_load_mtus(struct adapter *adap, const unsigned short *mtus,
		  const unsigned short *alpha, const unsigned short *beta);
void t4_ulprx_read_la(struct adapter *adap, u32 *la_buf);
void t4_get_chan_txrate(struct adapter *adap, u64 *nic_rate, u64 *ofld_rate);
void t4_mk_filtdelwr(unsigned int ftid, struct fw_filter_wr *wr, int qid);
void t4_wol_magic_enable(struct adapter *adap, unsigned int port,
			 const u8 *addr);
int t4_wol_pat_enable(struct adapter *adap, unsigned int port, unsigned int map,
		      u64 mask0, u64 mask1, unsigned int crc, bool enable);
int t4_fw_hello(struct adapter *adap, unsigned int mbox, unsigned int evt_mbox,
		enum dev_master master, enum dev_state *state);
int t4_fw_bye(struct adapter *adap, unsigned int mbox);
int t4_early_init(struct adapter *adap, unsigned int mbox);
int t4_fw_reset(struct adapter *adap, unsigned int mbox, int reset);
int t4_fixup_host_params(struct adapter *adap, unsigned int page_size,
			  unsigned int cache_line_size);
int t4_fw_initialize(struct adapter *adap, unsigned int mbox);
int t4_query_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int nparams, const u32 *params,
		    u32 *val);
int t4_query_params_ns(struct adapter *adap, unsigned int mbox, unsigned int pf,
		       unsigned int vf, unsigned int nparams, const u32 *params,
		       u32 *val);
int t4_query_params_rw(struct adapter *adap, unsigned int mbox, unsigned int pf,
		       unsigned int vf, unsigned int nparams, const u32 *params,
		       u32 *val, int rw, bool sleep_ok);
int t4_set_params_timeout(struct adapter *adap, unsigned int mbox,
			  unsigned int pf, unsigned int vf,
			  unsigned int nparams, const u32 *params,
			  const u32 *val, int timeout);
int t4_set_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		  unsigned int vf, unsigned int nparams, const u32 *params,
		  const u32 *val);
int t4_cfg_pfvf(struct adapter *adap, unsigned int mbox, unsigned int pf,
		unsigned int vf, unsigned int txq, unsigned int txq_eth_ctrl,
		unsigned int rxqi, unsigned int rxq, unsigned int tc,
		unsigned int vi, unsigned int cmask, unsigned int pmask,
		unsigned int nexact, unsigned int rcaps, unsigned int wxcaps);
int t4_alloc_vi(struct adapter *adap, unsigned int mbox, unsigned int port,
		unsigned int pf, unsigned int vf, unsigned int nmac, u8 *mac,
		unsigned int *rss_size, u8 *vivld, u8 *vin);
int t4_free_vi(struct adapter *adap, unsigned int mbox,
	       unsigned int pf, unsigned int vf,
	       unsigned int viid);
int t4_set_rxmode(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  unsigned int viid_mirror, int mtu, int promisc, int all_multi,
		  int bcast, int vlanex, bool sleep_ok);
int t4_free_raw_mac_filt(struct adapter *adap, unsigned int viid,
			 const u8 *addr, const u8 *mask, unsigned int idx,
			 u8 lookup_type, u8 port_id, bool sleep_ok);
int t4_free_encap_mac_filt(struct adapter *adap, unsigned int viid, int idx,
			   bool sleep_ok);
int t4_alloc_encap_mac_filt(struct adapter *adap, unsigned int viid,
			    const u8 *addr, const u8 *mask, unsigned int vni,
			    unsigned int vni_mask, u8 dip_hit, u8 lookup_type,
			    bool sleep_ok);
int t4_alloc_raw_mac_filt(struct adapter *adap, unsigned int viid,
			  const u8 *addr, const u8 *mask, unsigned int idx,
			  u8 lookup_type, u8 port_id, bool sleep_ok);
int t4_alloc_mac_filt(struct adapter *adap, unsigned int mbox,
		      unsigned int viid, bool free, unsigned int naddr,
		      const u8 **addr, u16 *idx, u64 *hash, bool sleep_ok);
int t4_free_mac_filt(struct adapter *adap, unsigned int mbox,
		     unsigned int viid, unsigned int naddr,
		     const u8 **addr, bool sleep_ok);
int t4_change_mac(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int idx, const u8 *addr, bool persist, u8 *smt_idx);
int t4_set_addr_hash(struct adapter *adap, unsigned int mbox, unsigned int viid,
		     bool ucast, u64 vec, bool sleep_ok);
int t4_enable_vi_params(struct adapter *adap, unsigned int mbox,
			unsigned int viid, bool rx_en, bool tx_en, bool dcb_en);
int t4_enable_pi_params(struct adapter *adap, unsigned int mbox,
			struct port_info *pi,
			bool rx_en, bool tx_en, bool dcb_en);
int t4_enable_vi(struct adapter *adap, unsigned int mbox, unsigned int viid,
		 bool rx_en, bool tx_en);
int t4_identify_port(struct adapter *adap, unsigned int mbox, unsigned int viid,
		     unsigned int nblinks);
int t4_mdio_rd(struct adapter *adap, unsigned int mbox, unsigned int phy_addr,
	       unsigned int mmd, unsigned int reg, u16 *valp);
int t4_mdio_wr(struct adapter *adap, unsigned int mbox, unsigned int phy_addr,
	       unsigned int mmd, unsigned int reg, u16 val);
int t4_iq_stop(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int iqtype, unsigned int iqid,
	       unsigned int fl0id, unsigned int fl1id);
int t4_iq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int iqtype, unsigned int iqid,
	       unsigned int fl0id, unsigned int fl1id);
int t4_eth_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		   unsigned int vf, unsigned int eqid);
int t4_ctrl_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid);
int t4_ofld_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid);
int t4_sge_ctxt_flush(struct adapter *adap, unsigned int mbox, int ctxt_type);
int t4_read_sge_dbqtimers(struct adapter *adap, unsigned int ndbqtimers,
			  u16 *dbqtimers);
void t4_handle_get_port_info(struct port_info *pi, const __be64 *rpl);
int t4_update_port_info(struct port_info *pi);
int t4_get_link_params(struct port_info *pi, unsigned int *link_okp,
		       unsigned int *speedp, unsigned int *mtup);
int t4_handle_fw_rpl(struct adapter *adap, const __be64 *rpl);
void t4_db_full(struct adapter *adapter);
void t4_db_dropped(struct adapter *adapter);
int t4_set_trace_filter(struct adapter *adapter, const struct trace_params *tp,
			int filter_index, int enable);
void t4_get_trace_filter(struct adapter *adapter, struct trace_params *tp,
			 int filter_index, int *enabled);
int t4_fwaddrspace_write(struct adapter *adap, unsigned int mbox,
			 u32 addr, u32 val);
void t4_read_pace_tbl(struct adapter *adap, unsigned int pace_vals[NTX_SCHED]);
void t4_get_tx_sched(struct adapter *adap, unsigned int sched,
		     unsigned int *kbps, unsigned int *ipg, bool sleep_ok);
int t4_sge_ctxt_rd(struct adapter *adap, unsigned int mbox, unsigned int cid,
		   enum ctxt_type ctype, u32 *data);
int t4_sge_ctxt_rd_bd(struct adapter *adap, unsigned int cid,
		      enum ctxt_type ctype, u32 *data);
int t4_sched_params(struct adapter *adapter, u8 type, u8 level, u8 mode,
		    u8 rateunit, u8 ratemode, u8 channel, u8 class,
		    u32 minrate, u32 maxrate, u16 weight, u16 pktsize,
		    u16 burstsize);
void t4_sge_decode_idma_state(struct adapter *adapter, int state);
void t4_idma_monitor_init(struct adapter *adapter,
			  struct sge_idma_monitor_state *idma);
void t4_idma_monitor(struct adapter *adapter,
		     struct sge_idma_monitor_state *idma,
		     int hz, int ticks);
int t4_set_vf_mac_acl(struct adapter *adapter, unsigned int vf,
		      unsigned int naddr, u8 *addr);
void t4_tp_pio_read(struct adapter *adap, u32 *buff, u32 nregs,
		    u32 start_index, bool sleep_ok);
void t4_tp_tm_pio_read(struct adapter *adap, u32 *buff, u32 nregs,
		       u32 start_index, bool sleep_ok);
void t4_tp_mib_read(struct adapter *adap, u32 *buff, u32 nregs,
		    u32 start_index, bool sleep_ok);
void t4_uld_mem_free(struct adapter *adap);
int t4_uld_mem_alloc(struct adapter *adap);
void t4_uld_clean_up(struct adapter *adap);
void t4_register_netevent_notifier(void);
int t4_i2c_rd(struct adapter *adap, unsigned int mbox, int port,
	      unsigned int devid, unsigned int offset,
	      unsigned int len, u8 *buf);
int t4_load_boot(struct adapter *adap, u8 *boot_data,
		 unsigned int boot_addr, unsigned int size);
int t4_load_bootcfg(struct adapter *adap,
		    const u8 *cfg_data, unsigned int size);
void free_rspq_fl(struct adapter *adap, struct sge_rspq *rq, struct sge_fl *fl);
void free_tx_desc(struct adapter *adap, struct sge_txq *q,
		  unsigned int n, bool unmap);
void cxgb4_eosw_txq_free_desc(struct adapter *adap, struct sge_eosw_txq *txq,
			      u32 ndesc);
int cxgb4_ethofld_send_flowc(struct net_device *dev, u32 eotid, u32 tc);
void cxgb4_ethofld_restart(struct tasklet_struct *t);
int cxgb4_ethofld_rx_handler(struct sge_rspq *q, const __be64 *rsp,
			     const struct pkt_gl *si);
void free_txq(struct adapter *adap, struct sge_txq *q);
void cxgb4_reclaim_completed_tx(struct adapter *adap,
				struct sge_txq *q, bool unmap);
int cxgb4_map_skb(struct device *dev, const struct sk_buff *skb,
		  dma_addr_t *addr);
void cxgb4_inline_tx_skb(const struct sk_buff *skb, const struct sge_txq *q,
			 void *pos);
void cxgb4_write_sgl(const struct sk_buff *skb, struct sge_txq *q,
		     struct ulptx_sgl *sgl, u64 *end, unsigned int start,
		     const dma_addr_t *addr);
void cxgb4_write_partial_sgl(const struct sk_buff *skb, struct sge_txq *q,
			     struct ulptx_sgl *sgl, u64 *end,
			     const dma_addr_t *addr, u32 start, u32 send_len);
void cxgb4_ring_tx_db(struct adapter *adap, struct sge_txq *q, int n);
int t4_set_vlan_acl(struct adapter *adap, unsigned int mbox, unsigned int vf,
		    u16 vlan);
int cxgb4_dcb_enabled(const struct net_device *dev);
int cxgb4_thermal_init(struct adapter *adap);
int cxgb4_thermal_remove(struct adapter *adap);
int cxgb4_set_msix_aff(struct adapter *adap, unsigned short vec,
		       cpumask_var_t *aff_mask, int idx);
void cxgb4_clear_msix_aff(unsigned short vec, cpumask_var_t aff_mask);
int cxgb4_change_mac(struct port_info *pi, unsigned int viid,
		     int *tcam_idx, const u8 *addr,
		     bool persistent, u8 *smt_idx);
int cxgb4_alloc_mac_filt(struct adapter *adap, unsigned int viid,
			 bool free, unsigned int naddr,
			 const u8 **addr, u16 *idx,
			 u64 *hash, bool sleep_ok);
int cxgb4_free_mac_filt(struct adapter *adap, unsigned int viid,
			unsigned int naddr, const u8 **addr, bool sleep_ok);
int cxgb4_init_mps_ref_entries(struct adapter *adap);
void cxgb4_free_mps_ref_entries(struct adapter *adap);
int cxgb4_alloc_encap_mac_filt(struct adapter *adap, unsigned int viid,
			       const u8 *addr, const u8 *mask,
			       unsigned int vni, unsigned int vni_mask,
			       u8 dip_hit, u8 lookup_type, bool sleep_ok);
int cxgb4_free_encap_mac_filt(struct adapter *adap, unsigned int viid,
			      int idx, bool sleep_ok);
int cxgb4_free_raw_mac_filt(struct adapter *adap,
			    unsigned int viid,
			    const u8 *addr,
			    const u8 *mask,
			    unsigned int idx,
			    u8 lookup_type,
			    u8 port_id,
			    bool sleep_ok);
int cxgb4_alloc_raw_mac_filt(struct adapter *adap,
			     unsigned int viid,
			     const u8 *addr,
			     const u8 *mask,
			     unsigned int idx,
			     u8 lookup_type,
			     u8 port_id,
			     bool sleep_ok);
int cxgb4_update_mac_filt(struct port_info *pi, unsigned int viid,
			  int *tcam_idx, const u8 *addr,
			  bool persistent, u8 *smt_idx);
int cxgb4_get_msix_idx_from_bmap(struct adapter *adap);
void cxgb4_free_msix_idx_in_bmap(struct adapter *adap, u32 msix_idx);
void cxgb4_enable_rx(struct adapter *adap, struct sge_rspq *q);
void cxgb4_quiesce_rx(struct sge_rspq *q);
int cxgb4_port_mirror_alloc(struct net_device *dev);
void cxgb4_port_mirror_free(struct net_device *dev);
#if IS_ENABLED(CONFIG_CHELSIO_TLS_DEVICE)
int cxgb4_set_ktls_feature(struct adapter *adap, bool enable);
#endif
#endif  
