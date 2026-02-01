 

#ifndef __T4VF_COMMON_H__
#define __T4VF_COMMON_H__

#include "../cxgb4/t4_hw.h"
#include "../cxgb4/t4fw_api.h"

#define CHELSIO_CHIP_CODE(version, revision) (((version) << 4) | (revision))
#define CHELSIO_CHIP_VERSION(code) (((code) >> 4) & 0xf)
#define CHELSIO_CHIP_RELEASE(code) ((code) & 0xf)

 
#define CHELSIO_T4		0x4
#define CHELSIO_T5		0x5
#define CHELSIO_T6		0x6

enum chip_type {
	T4_A1 = CHELSIO_CHIP_CODE(CHELSIO_T4, 1),
	T4_A2 = CHELSIO_CHIP_CODE(CHELSIO_T4, 2),
	T4_FIRST_REV	= T4_A1,
	T4_LAST_REV	= T4_A2,

	T5_A0 = CHELSIO_CHIP_CODE(CHELSIO_T5, 0),
	T5_A1 = CHELSIO_CHIP_CODE(CHELSIO_T5, 1),
	T5_FIRST_REV	= T5_A0,
	T5_LAST_REV	= T5_A1,
};

 
#define FW_LEN16(fw_struct) FW_CMD_LEN16_V(sizeof(fw_struct) / 16)

 
struct t4vf_port_stats {
	 
	u64 tx_bcast_bytes;		 
	u64 tx_bcast_frames;
	u64 tx_mcast_bytes;		 
	u64 tx_mcast_frames;
	u64 tx_ucast_bytes;		 
	u64 tx_ucast_frames;
	u64 tx_drop_frames;		 
	u64 tx_offload_bytes;		 
	u64 tx_offload_frames;

	 
	u64 rx_bcast_bytes;		 
	u64 rx_bcast_frames;
	u64 rx_mcast_bytes;		 
	u64 rx_mcast_frames;
	u64 rx_ucast_bytes;
	u64 rx_ucast_frames;		 

	u64 rx_err_frames;		 
};

 
typedef u16 fw_port_cap16_t;     
typedef u32 fw_port_cap32_t;     

enum fw_caps {
	FW_CAPS_UNKNOWN	= 0,	 
	FW_CAPS16	= 1,	 
	FW_CAPS32	= 2,	 
};

enum cc_pause {
	PAUSE_RX	= 1 << 0,
	PAUSE_TX	= 1 << 1,
	PAUSE_AUTONEG	= 1 << 2
};

enum cc_fec {
	FEC_AUTO	= 1 << 0,	 
	FEC_RS		= 1 << 1,	 
	FEC_BASER_RS	= 1 << 2,	 
};

struct link_config {
	fw_port_cap32_t pcaps;		 
	fw_port_cap32_t	acaps;		 
	fw_port_cap32_t	lpacaps;	 

	fw_port_cap32_t	speed_caps;	 
	u32		speed;		 

	enum cc_pause	requested_fc;	 
	enum cc_pause	fc;		 
	enum cc_pause   advertised_fc;   

	enum cc_fec	auto_fec;	 
	enum cc_fec	requested_fec;	 
	enum cc_fec	fec;		 

	unsigned char	autoneg;	 

	unsigned char	link_ok;	 
	unsigned char	link_down_rc;	 
};

 
static inline bool is_x_10g_port(const struct link_config *lc)
{
	fw_port_cap32_t speeds, high_speeds;

	speeds = FW_PORT_CAP32_SPEED_V(FW_PORT_CAP32_SPEED_G(lc->pcaps));
	high_speeds =
		speeds & ~(FW_PORT_CAP32_SPEED_100M | FW_PORT_CAP32_SPEED_1G);

	return high_speeds != 0;
}

 
struct dev_params {
	u32 fwrev;			 
	u32 tprev;			 
};

 
struct sge_params {
	u32 sge_control;		 
	u32 sge_control2;		 
	u32 sge_host_page_size;		 
	u32 sge_egress_queues_per_page;	 
	u32 sge_ingress_queues_per_page; 
	u32 sge_vf_hps;                  
	u32 sge_vf_eq_qpp;		 
	u32 sge_vf_iq_qpp;		 
	u32 sge_fl_buffer_size[16];	 
	u32 sge_ingress_rx_threshold;	 
	u32 sge_congestion_control;      
	u32 sge_timer_value_0_and_1;	 
	u32 sge_timer_value_2_and_3;
	u32 sge_timer_value_4_and_5;
};

 
struct vpd_params {
	u32 cclk;			 
};

 
struct arch_specific_params {
	u32 sge_fl_db;
	u16 mps_tcam_size;
};

 
struct rss_params {
	unsigned int mode;		 
	union {
	    struct {
		unsigned int synmapen:1;	 
		unsigned int syn4tupenipv6:1;	 
		unsigned int syn2tupenipv6:1;	 
		unsigned int syn4tupenipv4:1;	 
		unsigned int syn2tupenipv4:1;	 
		unsigned int ofdmapen:1;	 
		unsigned int tnlmapen:1;	 
		unsigned int tnlalllookup:1;	 
		unsigned int hashtoeplitz:1;	 
	    } basicvirtual;
	} u;
};

 
union rss_vi_config {
    struct {
	u16 defaultq;			 
	unsigned int ip6fourtupen:1;	 
	unsigned int ip6twotupen:1;	 
	unsigned int ip4fourtupen:1;	 
	unsigned int ip4twotupen:1;	 
	int udpen;			 
    } basicvirtual;
};

 
struct vf_resources {
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

 
struct adapter_params {
	struct dev_params dev;		 
	struct sge_params sge;		 
	struct vpd_params vpd;		 
	struct rss_params rss;		 
	struct vf_resources vfres;	 
	struct arch_specific_params arch;  
	enum chip_type chip;		 
	u8 nports;			 
	u8 fw_caps_support;		 
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

#include "adapter.h"

#ifndef PCI_VENDOR_ID_CHELSIO
# define PCI_VENDOR_ID_CHELSIO 0x1425
#endif

#define for_each_port(adapter, iter) \
	for (iter = 0; iter < (adapter)->params.nports; iter++)

static inline unsigned int core_ticks_per_usec(const struct adapter *adapter)
{
	return adapter->params.vpd.cclk / 1000;
}

static inline unsigned int us_to_core_ticks(const struct adapter *adapter,
					    unsigned int us)
{
	return (us * adapter->params.vpd.cclk) / 1000;
}

static inline unsigned int core_ticks_to_us(const struct adapter *adapter,
					    unsigned int ticks)
{
	return (ticks * 1000) / adapter->params.vpd.cclk;
}

int t4vf_wr_mbox_core(struct adapter *, const void *, int, void *, bool);

static inline int t4vf_wr_mbox(struct adapter *adapter, const void *cmd,
			       int size, void *rpl)
{
	return t4vf_wr_mbox_core(adapter, cmd, size, rpl, true);
}

static inline int t4vf_wr_mbox_ns(struct adapter *adapter, const void *cmd,
				  int size, void *rpl)
{
	return t4vf_wr_mbox_core(adapter, cmd, size, rpl, false);
}

#define CHELSIO_PCI_ID_VER(dev_id)  ((dev_id) >> 12)

static inline int is_t4(enum chip_type chip)
{
	return CHELSIO_CHIP_VERSION(chip) == CHELSIO_T4;
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

int t4vf_wait_dev_ready(struct adapter *);
int t4vf_port_init(struct adapter *, int);

int t4vf_fw_reset(struct adapter *);
int t4vf_set_params(struct adapter *, unsigned int, const u32 *, const u32 *);

int t4vf_fl_pkt_align(struct adapter *adapter);
enum t4_bar2_qtype { T4_BAR2_QTYPE_EGRESS, T4_BAR2_QTYPE_INGRESS };
int t4vf_bar2_sge_qregs(struct adapter *adapter,
			unsigned int qid,
			enum t4_bar2_qtype qtype,
			u64 *pbar2_qoffset,
			unsigned int *pbar2_qid);

unsigned int t4vf_get_pf_from_vf(struct adapter *);
int t4vf_get_sge_params(struct adapter *);
int t4vf_get_vpd_params(struct adapter *);
int t4vf_get_dev_params(struct adapter *);
int t4vf_get_rss_glb_config(struct adapter *);
int t4vf_get_vfres(struct adapter *);

int t4vf_read_rss_vi_config(struct adapter *, unsigned int,
			    union rss_vi_config *);
int t4vf_write_rss_vi_config(struct adapter *, unsigned int,
			     union rss_vi_config *);
int t4vf_config_rss_range(struct adapter *, unsigned int, int, int,
			  const u16 *, int);

int t4vf_alloc_vi(struct adapter *, int);
int t4vf_free_vi(struct adapter *, int);
int t4vf_enable_vi(struct adapter *adapter, unsigned int viid, bool rx_en,
		   bool tx_en);
int t4vf_enable_pi(struct adapter *adapter, struct port_info *pi, bool rx_en,
		   bool tx_en);
int t4vf_identify_port(struct adapter *, unsigned int, unsigned int);

int t4vf_set_rxmode(struct adapter *, unsigned int, int, int, int, int, int,
		    bool);
int t4vf_alloc_mac_filt(struct adapter *, unsigned int, bool, unsigned int,
			const u8 **, u16 *, u64 *, bool);
int t4vf_free_mac_filt(struct adapter *, unsigned int, unsigned int naddr,
		       const u8 **, bool);
int t4vf_change_mac(struct adapter *, unsigned int, int, const u8 *, bool);
int t4vf_set_addr_hash(struct adapter *, unsigned int, bool, u64, bool);
int t4vf_get_port_stats(struct adapter *, int, struct t4vf_port_stats *);

int t4vf_iq_free(struct adapter *, unsigned int, unsigned int, unsigned int,
		 unsigned int);
int t4vf_eth_eq_free(struct adapter *, unsigned int);

int t4vf_update_port_info(struct port_info *pi);
int t4vf_handle_fw_rpl(struct adapter *, const __be64 *);
int t4vf_prep_adapter(struct adapter *);
int t4vf_get_vf_mac_acl(struct adapter *adapter, unsigned int port,
			unsigned int *naddr, u8 *addr);
int t4vf_get_vf_vlan_acl(struct adapter *adapter);

#endif  
