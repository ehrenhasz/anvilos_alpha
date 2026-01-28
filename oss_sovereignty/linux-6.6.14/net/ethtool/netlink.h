#ifndef _NET_ETHTOOL_NETLINK_H
#define _NET_ETHTOOL_NETLINK_H
#include <linux/ethtool_netlink.h>
#include <linux/netdevice.h>
#include <net/genetlink.h>
#include <net/sock.h>
struct ethnl_req_info;
int ethnl_parse_header_dev_get(struct ethnl_req_info *req_info,
			       const struct nlattr *nest, struct net *net,
			       struct netlink_ext_ack *extack,
			       bool require_dev);
int ethnl_fill_reply_header(struct sk_buff *skb, struct net_device *dev,
			    u16 attrtype);
struct sk_buff *ethnl_reply_init(size_t payload, struct net_device *dev, u8 cmd,
				 u16 hdr_attrtype, struct genl_info *info,
				 void **ehdrp);
void *ethnl_dump_put(struct sk_buff *skb, struct netlink_callback *cb, u8 cmd);
void *ethnl_bcastmsg_put(struct sk_buff *skb, u8 cmd);
int ethnl_multicast(struct sk_buff *skb, struct net_device *dev);
static inline int ethnl_strz_size(const char *s)
{
	return nla_total_size(strnlen(s, ETH_GSTRING_LEN) + 1);
}
static inline int ethnl_put_strz(struct sk_buff *skb, u16 attrtype,
				 const char *s)
{
	unsigned int len = strnlen(s, ETH_GSTRING_LEN);
	struct nlattr *attr;
	attr = nla_reserve(skb, attrtype, len + 1);
	if (!attr)
		return -EMSGSIZE;
	memcpy(nla_data(attr), s, len);
	((char *)nla_data(attr))[len] = '\0';
	return 0;
}
static inline void ethnl_update_u32(u32 *dst, const struct nlattr *attr,
				    bool *mod)
{
	u32 val;
	if (!attr)
		return;
	val = nla_get_u32(attr);
	if (*dst == val)
		return;
	*dst = val;
	*mod = true;
}
static inline void ethnl_update_u8(u8 *dst, const struct nlattr *attr,
				   bool *mod)
{
	u8 val;
	if (!attr)
		return;
	val = nla_get_u8(attr);
	if (*dst == val)
		return;
	*dst = val;
	*mod = true;
}
static inline void ethnl_update_bool32(u32 *dst, const struct nlattr *attr,
				       bool *mod)
{
	u8 val;
	if (!attr)
		return;
	val = !!nla_get_u8(attr);
	if (!!*dst == val)
		return;
	*dst = val;
	*mod = true;
}
static inline void ethnl_update_bool(bool *dst, const struct nlattr *attr,
				     bool *mod)
{
	u8 val;
	if (!attr)
		return;
	val = !!nla_get_u8(attr);
	if (!!*dst == val)
		return;
	*dst = val;
	*mod = true;
}
static inline void ethnl_update_binary(void *dst, unsigned int len,
				       const struct nlattr *attr, bool *mod)
{
	if (!attr)
		return;
	if (nla_len(attr) < len)
		len = nla_len(attr);
	if (!memcmp(dst, nla_data(attr), len))
		return;
	memcpy(dst, nla_data(attr), len);
	*mod = true;
}
static inline void ethnl_update_bitfield32(u32 *dst, const struct nlattr *attr,
					   bool *mod)
{
	struct nla_bitfield32 change;
	u32 newval;
	if (!attr)
		return;
	change = nla_get_bitfield32(attr);
	newval = (*dst & ~change.selector) | (change.value & change.selector);
	if (*dst == newval)
		return;
	*dst = newval;
	*mod = true;
}
static inline unsigned int ethnl_reply_header_size(void)
{
	return nla_total_size(nla_total_size(sizeof(u32)) +
			      nla_total_size(IFNAMSIZ));
}
struct ethnl_req_info {
	struct net_device	*dev;
	netdevice_tracker	dev_tracker;
	u32			flags;
};
static inline void ethnl_parse_header_dev_put(struct ethnl_req_info *req_info)
{
	netdev_put(req_info->dev, &req_info->dev_tracker);
}
struct ethnl_reply_data {
	struct net_device		*dev;
};
int ethnl_ops_begin(struct net_device *dev);
void ethnl_ops_complete(struct net_device *dev);
struct ethnl_request_ops {
	u8			request_cmd;
	u8			reply_cmd;
	u16			hdr_attr;
	unsigned int		req_info_size;
	unsigned int		reply_data_size;
	bool			allow_nodev_do;
	u8			set_ntf_cmd;
	int (*parse_request)(struct ethnl_req_info *req_info,
			     struct nlattr **tb,
			     struct netlink_ext_ack *extack);
	int (*prepare_data)(const struct ethnl_req_info *req_info,
			    struct ethnl_reply_data *reply_data,
			    const struct genl_info *info);
	int (*reply_size)(const struct ethnl_req_info *req_info,
			  const struct ethnl_reply_data *reply_data);
	int (*fill_reply)(struct sk_buff *skb,
			  const struct ethnl_req_info *req_info,
			  const struct ethnl_reply_data *reply_data);
	void (*cleanup_data)(struct ethnl_reply_data *reply_data);
	int (*set_validate)(struct ethnl_req_info *req_info,
			    struct genl_info *info);
	int (*set)(struct ethnl_req_info *req_info,
		   struct genl_info *info);
};
extern const struct ethnl_request_ops ethnl_strset_request_ops;
extern const struct ethnl_request_ops ethnl_linkinfo_request_ops;
extern const struct ethnl_request_ops ethnl_linkmodes_request_ops;
extern const struct ethnl_request_ops ethnl_linkstate_request_ops;
extern const struct ethnl_request_ops ethnl_debug_request_ops;
extern const struct ethnl_request_ops ethnl_wol_request_ops;
extern const struct ethnl_request_ops ethnl_features_request_ops;
extern const struct ethnl_request_ops ethnl_privflags_request_ops;
extern const struct ethnl_request_ops ethnl_rings_request_ops;
extern const struct ethnl_request_ops ethnl_channels_request_ops;
extern const struct ethnl_request_ops ethnl_coalesce_request_ops;
extern const struct ethnl_request_ops ethnl_pause_request_ops;
extern const struct ethnl_request_ops ethnl_eee_request_ops;
extern const struct ethnl_request_ops ethnl_tsinfo_request_ops;
extern const struct ethnl_request_ops ethnl_fec_request_ops;
extern const struct ethnl_request_ops ethnl_module_eeprom_request_ops;
extern const struct ethnl_request_ops ethnl_stats_request_ops;
extern const struct ethnl_request_ops ethnl_phc_vclocks_request_ops;
extern const struct ethnl_request_ops ethnl_module_request_ops;
extern const struct ethnl_request_ops ethnl_pse_request_ops;
extern const struct ethnl_request_ops ethnl_rss_request_ops;
extern const struct ethnl_request_ops ethnl_plca_cfg_request_ops;
extern const struct ethnl_request_ops ethnl_plca_status_request_ops;
extern const struct ethnl_request_ops ethnl_mm_request_ops;
extern const struct nla_policy ethnl_header_policy[ETHTOOL_A_HEADER_FLAGS + 1];
extern const struct nla_policy ethnl_header_policy_stats[ETHTOOL_A_HEADER_FLAGS + 1];
extern const struct nla_policy ethnl_strset_get_policy[ETHTOOL_A_STRSET_COUNTS_ONLY + 1];
extern const struct nla_policy ethnl_linkinfo_get_policy[ETHTOOL_A_LINKINFO_HEADER + 1];
extern const struct nla_policy ethnl_linkinfo_set_policy[ETHTOOL_A_LINKINFO_TP_MDIX_CTRL + 1];
extern const struct nla_policy ethnl_linkmodes_get_policy[ETHTOOL_A_LINKMODES_HEADER + 1];
extern const struct nla_policy ethnl_linkmodes_set_policy[ETHTOOL_A_LINKMODES_LANES + 1];
extern const struct nla_policy ethnl_linkstate_get_policy[ETHTOOL_A_LINKSTATE_HEADER + 1];
extern const struct nla_policy ethnl_debug_get_policy[ETHTOOL_A_DEBUG_HEADER + 1];
extern const struct nla_policy ethnl_debug_set_policy[ETHTOOL_A_DEBUG_MSGMASK + 1];
extern const struct nla_policy ethnl_wol_get_policy[ETHTOOL_A_WOL_HEADER + 1];
extern const struct nla_policy ethnl_wol_set_policy[ETHTOOL_A_WOL_SOPASS + 1];
extern const struct nla_policy ethnl_features_get_policy[ETHTOOL_A_FEATURES_HEADER + 1];
extern const struct nla_policy ethnl_features_set_policy[ETHTOOL_A_FEATURES_WANTED + 1];
extern const struct nla_policy ethnl_privflags_get_policy[ETHTOOL_A_PRIVFLAGS_HEADER + 1];
extern const struct nla_policy ethnl_privflags_set_policy[ETHTOOL_A_PRIVFLAGS_FLAGS + 1];
extern const struct nla_policy ethnl_rings_get_policy[ETHTOOL_A_RINGS_HEADER + 1];
extern const struct nla_policy ethnl_rings_set_policy[ETHTOOL_A_RINGS_TX_PUSH_BUF_LEN_MAX + 1];
extern const struct nla_policy ethnl_channels_get_policy[ETHTOOL_A_CHANNELS_HEADER + 1];
extern const struct nla_policy ethnl_channels_set_policy[ETHTOOL_A_CHANNELS_COMBINED_COUNT + 1];
extern const struct nla_policy ethnl_coalesce_get_policy[ETHTOOL_A_COALESCE_HEADER + 1];
extern const struct nla_policy ethnl_coalesce_set_policy[ETHTOOL_A_COALESCE_MAX + 1];
extern const struct nla_policy ethnl_pause_get_policy[ETHTOOL_A_PAUSE_STATS_SRC + 1];
extern const struct nla_policy ethnl_pause_set_policy[ETHTOOL_A_PAUSE_TX + 1];
extern const struct nla_policy ethnl_eee_get_policy[ETHTOOL_A_EEE_HEADER + 1];
extern const struct nla_policy ethnl_eee_set_policy[ETHTOOL_A_EEE_TX_LPI_TIMER + 1];
extern const struct nla_policy ethnl_tsinfo_get_policy[ETHTOOL_A_TSINFO_HEADER + 1];
extern const struct nla_policy ethnl_cable_test_act_policy[ETHTOOL_A_CABLE_TEST_HEADER + 1];
extern const struct nla_policy ethnl_cable_test_tdr_act_policy[ETHTOOL_A_CABLE_TEST_TDR_CFG + 1];
extern const struct nla_policy ethnl_tunnel_info_get_policy[ETHTOOL_A_TUNNEL_INFO_HEADER + 1];
extern const struct nla_policy ethnl_fec_get_policy[ETHTOOL_A_FEC_HEADER + 1];
extern const struct nla_policy ethnl_fec_set_policy[ETHTOOL_A_FEC_AUTO + 1];
extern const struct nla_policy ethnl_module_eeprom_get_policy[ETHTOOL_A_MODULE_EEPROM_I2C_ADDRESS + 1];
extern const struct nla_policy ethnl_stats_get_policy[ETHTOOL_A_STATS_SRC + 1];
extern const struct nla_policy ethnl_phc_vclocks_get_policy[ETHTOOL_A_PHC_VCLOCKS_HEADER + 1];
extern const struct nla_policy ethnl_module_get_policy[ETHTOOL_A_MODULE_HEADER + 1];
extern const struct nla_policy ethnl_module_set_policy[ETHTOOL_A_MODULE_POWER_MODE_POLICY + 1];
extern const struct nla_policy ethnl_pse_get_policy[ETHTOOL_A_PSE_HEADER + 1];
extern const struct nla_policy ethnl_pse_set_policy[ETHTOOL_A_PSE_MAX + 1];
extern const struct nla_policy ethnl_rss_get_policy[ETHTOOL_A_RSS_CONTEXT + 1];
extern const struct nla_policy ethnl_plca_get_cfg_policy[ETHTOOL_A_PLCA_HEADER + 1];
extern const struct nla_policy ethnl_plca_set_cfg_policy[ETHTOOL_A_PLCA_MAX + 1];
extern const struct nla_policy ethnl_plca_get_status_policy[ETHTOOL_A_PLCA_HEADER + 1];
extern const struct nla_policy ethnl_mm_get_policy[ETHTOOL_A_MM_HEADER + 1];
extern const struct nla_policy ethnl_mm_set_policy[ETHTOOL_A_MM_MAX + 1];
int ethnl_set_features(struct sk_buff *skb, struct genl_info *info);
int ethnl_act_cable_test(struct sk_buff *skb, struct genl_info *info);
int ethnl_act_cable_test_tdr(struct sk_buff *skb, struct genl_info *info);
int ethnl_tunnel_info_doit(struct sk_buff *skb, struct genl_info *info);
int ethnl_tunnel_info_start(struct netlink_callback *cb);
int ethnl_tunnel_info_dumpit(struct sk_buff *skb, struct netlink_callback *cb);
extern const char stats_std_names[__ETHTOOL_STATS_CNT][ETH_GSTRING_LEN];
extern const char stats_eth_phy_names[__ETHTOOL_A_STATS_ETH_PHY_CNT][ETH_GSTRING_LEN];
extern const char stats_eth_mac_names[__ETHTOOL_A_STATS_ETH_MAC_CNT][ETH_GSTRING_LEN];
extern const char stats_eth_ctrl_names[__ETHTOOL_A_STATS_ETH_CTRL_CNT][ETH_GSTRING_LEN];
extern const char stats_rmon_names[__ETHTOOL_A_STATS_RMON_CNT][ETH_GSTRING_LEN];
#endif  
