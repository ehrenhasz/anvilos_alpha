 
 

#ifndef _HNS_DSAF_MAC_H
#define _HNS_DSAF_MAC_H

#include <linux/if_vlan.h>
#include <linux/kernel.h>
#include <linux/phy.h>
#include <linux/regmap.h>
#include "hns_dsaf_main.h"

struct dsaf_device;

#define MAC_GMAC_SUPPORTED \
	(SUPPORTED_10baseT_Half \
	| SUPPORTED_10baseT_Full \
	| SUPPORTED_100baseT_Half \
	| SUPPORTED_100baseT_Full \
	| SUPPORTED_Autoneg)

#define MAC_DEFAULT_MTU	(ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN + ETH_DATA_LEN)
#define MAC_MAX_MTU		9600
#define MAC_MAX_MTU_V2		9728
#define MAC_MIN_MTU		68
#define MAC_MAX_MTU_DBG		MAC_DEFAULT_MTU

#define MAC_DEFAULT_PAUSE_TIME 0xffff

#define MAC_GMAC_IDX 0
#define MAC_XGMAC_IDX 1

#define ETH_STATIC_REG	 1
#define ETH_DUMP_REG	 5
 
#define MAC_IS_BROADCAST(p)	((*(p) == 0xff) && (*((p) + 1) == 0xff) && \
		(*((p) + 2) == 0xff) &&  (*((p) + 3) == 0xff)  && \
		(*((p) + 4) == 0xff) && (*((p) + 5) == 0xff))

 
#define MAC_IS_L3_MULTICAST(p) ((*((p) + 0) == 0x01) && \
			(*((p) + 1) == 0x00)   && \
			(*((p) + 2) == 0x5e))

 
#define MAC_IS_ALL_ZEROS(p)   ((*(p) == 0) && (*((p) + 1) == 0) && \
	(*((p) + 2) == 0) && (*((p) + 3) == 0) && \
	(*((p) + 4) == 0) && (*((p) + 5) == 0))

 
#define MAC_IS_MULTICAST(p)	((*((u8 *)((p) + 0)) & 0x01) ? (1) : (0))

struct mac_priv {
	void *mac;
};

 
enum mac_speed {
	MAC_SPEED_10	= 10,	    
	MAC_SPEED_100	= 100,	   
	MAC_SPEED_1000  = 1000,	  
	MAC_SPEED_10000 = 10000	  
};

 
enum mac_intf {
	MAC_IF_NONE  = 0x00000000,    
	MAC_IF_MII   = 0x00010000,    
	MAC_IF_RMII  = 0x00020000,    
	MAC_IF_SMII  = 0x00030000,    
	MAC_IF_GMII  = 0x00040000,    
	MAC_IF_RGMII = 0x00050000,    
	MAC_IF_TBI   = 0x00060000,    
	MAC_IF_RTBI  = 0x00070000,    
	MAC_IF_SGMII = 0x00080000,    
	MAC_IF_XGMII = 0x00090000,    
	MAC_IF_QSGMII = 0x000a0000	 
};

 
enum mac_mode {
	 
	MAC_MODE_INVALID	 = 0,
	 
	MAC_MODE_MII_10	  = (MAC_IF_MII   | MAC_SPEED_10),
	 
	MAC_MODE_MII_100	 = (MAC_IF_MII   | MAC_SPEED_100),
	 
	MAC_MODE_RMII_10	 = (MAC_IF_RMII  | MAC_SPEED_10),
	 
	MAC_MODE_RMII_100	= (MAC_IF_RMII  | MAC_SPEED_100),
	 
	MAC_MODE_SMII_10	 = (MAC_IF_SMII  | MAC_SPEED_10),
	 
	MAC_MODE_SMII_100	= (MAC_IF_SMII  | MAC_SPEED_100),
	 
	MAC_MODE_GMII_1000   = (MAC_IF_GMII  | MAC_SPEED_1000),
	 
	MAC_MODE_RGMII_10	= (MAC_IF_RGMII | MAC_SPEED_10),
	 
	MAC_MODE_RGMII_100   = (MAC_IF_RGMII | MAC_SPEED_100),
	 
	MAC_MODE_RGMII_1000  = (MAC_IF_RGMII | MAC_SPEED_1000),
	 
	MAC_MODE_TBI_1000	= (MAC_IF_TBI   | MAC_SPEED_1000),
	 
	MAC_MODE_RTBI_1000   = (MAC_IF_RTBI  | MAC_SPEED_1000),
	 
	MAC_MODE_SGMII_10	= (MAC_IF_SGMII | MAC_SPEED_10),
	 
	MAC_MODE_SGMII_100   = (MAC_IF_SGMII | MAC_SPEED_100),
	 
	MAC_MODE_SGMII_1000  = (MAC_IF_SGMII | MAC_SPEED_1000),
	 
	MAC_MODE_XGMII_10000 = (MAC_IF_XGMII | MAC_SPEED_10000),
	 
	MAC_MODE_QSGMII_1000 = (MAC_IF_QSGMII | MAC_SPEED_1000)
};

 
enum mac_commom_mode {
	MAC_COMM_MODE_NONE	  = 0,  
	MAC_COMM_MODE_RX		= 1,  
	MAC_COMM_MODE_TX		= 2,  
	MAC_COMM_MODE_RX_AND_TX = 3   
};

 
struct mac_statistics {
	u64  stat_pkts64;  
	u64  stat_pkts65to127;  
	u64  stat_pkts128to255;  
	u64  stat_pkts256to511;  
	u64  stat_pkts512to1023; 
	u64  stat_pkts1024to1518;  
	u64  stat_pkts1519to1522;  
	 
	 
	u64  stat_fragments;
	 
	u64  stat_jabbers;
	 
	 
	u64  stat_drop_events;
	 
	 
	u64  stat_crc_align_errors;
	 
	 
	u64  stat_undersize_pkts;
	u64  stat_oversize_pkts;   

	u64  stat_rx_pause;		    
	u64  stat_tx_pause;		    

	u64  in_octets;		 
	u64  in_pkts;		 
	u64  in_mcast_pkts;	 
	u64  in_bcast_pkts;	 
				 
				 
	u64  in_discards;
	u64  in_errors;		 
				 
				 
				 
				 
	u64  out_octets;  
	u64  out_pkts;	 
	u64  out_mcast_pkts;  
	u64  out_bcast_pkts;  
	 
	 
	u64  out_discards;
	u64  out_errors;	 
			 
			 
			 
};

 
struct mac_params {
	char addr[ETH_ALEN];
	u8 __iomem *vaddr;  
	struct device *dev;
	u8 mac_id;
	 
	enum mac_mode mac_mode;
};

struct mac_info {
	u16 speed; 
		 
		 
		 
	u8 duplex;		 
	u8 auto_neg;	 
	enum hnae_loop loop_mode;
	u8 tx_pause_en;
	u8 tx_pause_time;
	u8 rx_pause_en;
	u8 pad_and_crc_en;
	u8 promiscuous_en;
	u8 port_en;	  
};

struct mac_entry_idx {
	u8 addr[ETH_ALEN];
	u16 vlan_id:12;
	u16 valid:1;
	u16 qos:3;
};

struct mac_hw_stats {
	u64 rx_good_pkts;	 
	u64 rx_good_bytes;
	u64 rx_total_pkts;	 
	u64 rx_total_bytes;	 
	u64 rx_bad_bytes;	 
	u64 rx_uc_pkts;
	u64 rx_mc_pkts;
	u64 rx_bc_pkts;
	u64 rx_fragment_err;	 
	u64 rx_undersize;	 
	u64 rx_under_min;
	u64 rx_minto64;		 
	u64 rx_64bytes;
	u64 rx_65to127;
	u64 rx_128to255;
	u64 rx_256to511;
	u64 rx_512to1023;
	u64 rx_1024to1518;
	u64 rx_1519tomax;
	u64 rx_1519tomax_good;	 
	u64 rx_oversize;
	u64 rx_jabber_err;
	u64 rx_fcs_err;
	u64 rx_vlan_pkts;	 
	u64 rx_data_err;	 
	u64 rx_align_err;	 
	u64 rx_long_err;	 
	u64 rx_pfc_tc0;
	u64 rx_pfc_tc1;		 
	u64 rx_pfc_tc2;		 
	u64 rx_pfc_tc3;		 
	u64 rx_pfc_tc4;		 
	u64 rx_pfc_tc5;		 
	u64 rx_pfc_tc6;		 
	u64 rx_pfc_tc7;		 
	u64 rx_unknown_ctrl;
	u64 rx_filter_pkts;	 
	u64 rx_filter_bytes;	 
	u64 rx_fifo_overrun_err; 
	u64 rx_len_err;		 
	u64 rx_comma_err;	 
	u64 rx_symbol_err;	 
	u64 tx_good_to_sw;	 
	u64 tx_bad_to_sw;	 
	u64 rx_1731_pkts;	 

	u64 tx_good_bytes;
	u64 tx_good_pkts;	 
	u64 tx_total_bytes;	 
	u64 tx_total_pkts;	 
	u64 tx_bad_bytes;	 
	u64 tx_bad_pkts;	 
	u64 tx_uc_pkts;
	u64 tx_mc_pkts;
	u64 tx_bc_pkts;
	u64 tx_undersize;	 
	u64 tx_fragment_err;	 
	u64 tx_under_min_pkts;	 
	u64 tx_64bytes;
	u64 tx_65to127;
	u64 tx_128to255;
	u64 tx_256to511;
	u64 tx_512to1023;
	u64 tx_1024to1518;
	u64 tx_1519tomax;
	u64 tx_1519tomax_good;	 
	u64 tx_oversize;	 
	u64 tx_jabber_err;
	u64 tx_underrun_err;	 
	u64 tx_vlan;		 
	u64 tx_crc_err;		 
	u64 tx_pfc_tc0;
	u64 tx_pfc_tc1;		 
	u64 tx_pfc_tc2;		 
	u64 tx_pfc_tc3;		 
	u64 tx_pfc_tc4;		 
	u64 tx_pfc_tc5;		 
	u64 tx_pfc_tc6;		 
	u64 tx_pfc_tc7;		 
	u64 tx_ctrl;		 
	u64 tx_1731_pkts;	 
	u64 tx_1588_pkts;	 
	u64 rx_good_from_sw;	 
	u64 rx_bad_from_sw;	 
};

struct hns_mac_cb {
	struct device *dev;
	struct dsaf_device *dsaf_dev;
	struct mac_priv priv;
	struct fwnode_handle *fw_port;
	u8 __iomem *vaddr;
	u8 __iomem *sys_ctl_vaddr;
	u8 __iomem *serdes_vaddr;
	struct regmap *serdes_ctrl;
	struct regmap *cpld_ctrl;
	char mc_mask[ETH_ALEN];
	u32 cpld_ctrl_reg;
	u32 port_rst_off;
	u32 port_mode_off;
	struct mac_entry_idx addr_entry_idx[DSAF_MAX_VM_NUM];
	u8 sfp_prsnt;
	u8 cpld_led_value;
	u8 mac_id;

	u8 link;
	u8 half_duplex;
	u16 speed;
	u16 max_speed;
	u16 max_frm;
	u16 tx_pause_frm_time;
	u32 if_support;
	u64 txpkt_for_led;
	u64 rxpkt_for_led;
	enum hnae_port_type mac_type;
	enum hnae_media_type media_type;
	phy_interface_t phy_if;
	enum hnae_loop loop_mode;

	struct phy_device *phy_dev;

	struct mac_hw_stats hw_stats;
};

struct mac_driver {
	 
	void (*mac_init)(void *mac_drv);
	 
	void (*mac_free)(void *mac_drv);
	 
	void (*mac_enable)(void *mac_drv, enum mac_commom_mode mode);
	 
	void (*mac_disable)(void *mac_drv, enum mac_commom_mode mode);
	 
	void (*set_mac_addr)(void *mac_drv,	const char *mac_addr);
	 
	int (*adjust_link)(void *mac_drv, enum mac_speed speed,
			   u32 full_duplex);
	 
	bool (*need_adjust_link)(void *mac_drv, enum mac_speed speed,
				 int duplex);
	 
	void (*set_an_mode)(void *mac_drv, u8 enable);
	 
	int (*config_loopback)(void *mac_drv, enum hnae_loop loop_mode,
			       u8 enable);
	 
	void (*config_max_frame_length)(void *mac_drv, u16 newval);
	 
	void (*config_pad_and_crc)(void *mac_drv, u8 newval);
	 
	void (*set_tx_auto_pause_frames)(void *mac_drv, u16 pause_time);
	 
	void (*set_promiscuous)(void *mac_drv, u8 enable);
	void (*mac_pausefrm_cfg)(void *mac_drv, u32 rx_en, u32 tx_en);

	void (*autoneg_stat)(void *mac_drv, u32 *enable);
	int (*set_pause_enable)(void *mac_drv, u32 rx_en, u32 tx_en);
	void (*get_pause_enable)(void *mac_drv, u32 *rx_en, u32 *tx_en);
	void (*get_link_status)(void *mac_drv, u32 *link_stat);
	 
	void (*get_regs)(void *mac_drv, void *data);
	int (*get_regs_count)(void);
	 
	void (*get_strings)(u32 stringset, u8 *data);
	 
	int (*get_sset_count)(int stringset);

	 
	void (*get_ethtool_stats)(void *mac_drv, u64 *data);

	 
	void (*get_info)(void *mac_drv, struct mac_info *mac_info);

	void (*update_stats)(void *mac_drv);
	int (*wait_fifo_clean)(void *mac_drv);

	enum mac_mode mac_mode;
	u8 mac_id;
	struct hns_mac_cb *mac_cb;
	u8 __iomem *io_base;
	unsigned int mac_en_flg; 
	unsigned int virt_dev_num;
	struct device *dev;
};

struct mac_stats_string {
	const char desc[ETH_GSTRING_LEN];
	unsigned long offset;
};

#define MAC_MAKE_MODE(interface, speed) (enum mac_mode)((interface) | (speed))
#define MAC_INTERFACE_FROM_MODE(mode) (enum mac_intf)((mode) & 0xFFFF0000)
#define MAC_SPEED_FROM_MODE(mode) (enum mac_speed)((mode) & 0x0000FFFF)
#define MAC_STATS_FIELD_OFF(field) (offsetof(struct mac_hw_stats, field))

static inline struct mac_driver *hns_mac_get_drv(
	const struct hns_mac_cb *mac_cb)
{
	return (struct mac_driver *)(mac_cb->priv.mac);
}

void *hns_gmac_config(struct hns_mac_cb *mac_cb,
		      struct mac_params *mac_param);
void *hns_xgmac_config(struct hns_mac_cb *mac_cb,
		       struct mac_params *mac_param);

int hns_mac_init(struct dsaf_device *dsaf_dev);
bool hns_mac_need_adjust_link(struct hns_mac_cb *mac_cb, int speed, int duplex);
void hns_mac_get_link_status(struct hns_mac_cb *mac_cb,	u32 *link_status);
int hns_mac_change_vf_addr(struct hns_mac_cb *mac_cb, u32 vmid,
			   const char *addr);
int hns_mac_set_multi(struct hns_mac_cb *mac_cb,
		      u32 port_num, char *addr, bool enable);
int hns_mac_vm_config_bc_en(struct hns_mac_cb *mac_cb, u32 vm, bool enable);
void hns_mac_start(struct hns_mac_cb *mac_cb);
void hns_mac_stop(struct hns_mac_cb *mac_cb);
void hns_mac_uninit(struct dsaf_device *dsaf_dev);
void hns_mac_adjust_link(struct hns_mac_cb *mac_cb, int speed, int duplex);
void hns_mac_reset(struct hns_mac_cb *mac_cb);
void hns_mac_get_autoneg(struct hns_mac_cb *mac_cb, u32 *auto_neg);
void hns_mac_get_pauseparam(struct hns_mac_cb *mac_cb, u32 *rx_en, u32 *tx_en);
int hns_mac_set_autoneg(struct hns_mac_cb *mac_cb, u8 enable);
int hns_mac_set_pauseparam(struct hns_mac_cb *mac_cb, u32 rx_en, u32 tx_en);
int hns_mac_set_mtu(struct hns_mac_cb *mac_cb, u32 new_mtu, u32 buf_size);
int hns_mac_get_port_info(struct hns_mac_cb *mac_cb,
			  u8 *auto_neg, u16 *speed, u8 *duplex);
int hns_mac_config_mac_loopback(struct hns_mac_cb *mac_cb,
				enum hnae_loop loop, int en);
void hns_mac_update_stats(struct hns_mac_cb *mac_cb);
void hns_mac_get_stats(struct hns_mac_cb *mac_cb, u64 *data);
void hns_mac_get_strings(struct hns_mac_cb *mac_cb, int stringset, u8 *data);
int hns_mac_get_sset_count(struct hns_mac_cb *mac_cb, int stringset);
void hns_mac_get_regs(struct hns_mac_cb *mac_cb, void *data);
int hns_mac_get_regs_count(struct hns_mac_cb *mac_cb);
void hns_set_led_opt(struct hns_mac_cb *mac_cb);
int hns_cpld_led_set_id(struct hns_mac_cb *mac_cb,
			enum hnae_led_state status);
void hns_mac_set_promisc(struct hns_mac_cb *mac_cb, u8 en);
int hns_mac_get_inner_port_num(struct hns_mac_cb *mac_cb,
			       u8 vmid, u8 *port_num);
int hns_mac_add_uc_addr(struct hns_mac_cb *mac_cb, u8 vf_id,
			const unsigned char *addr);
int hns_mac_rm_uc_addr(struct hns_mac_cb *mac_cb, u8 vf_id,
		       const unsigned char *addr);
int hns_mac_clr_multicast(struct hns_mac_cb *mac_cb, int vfn);
void hns_mac_enable(struct hns_mac_cb *mac_cb, enum mac_commom_mode mode);
void hns_mac_disable(struct hns_mac_cb *mac_cb, enum mac_commom_mode mode);
int hns_mac_wait_fifo_clean(struct hns_mac_cb *mac_cb);

#endif  
