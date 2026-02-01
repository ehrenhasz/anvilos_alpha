
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/phylink.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>
#include <linux/pcs/pcs-xpcs.h>
#include <linux/netdev_features.h>
#include <linux/netdevice.h>
#include <linux/if_bridge.h>
#include <linux/if_ether.h>
#include <linux/dsa/8021q.h>
#include "sja1105.h"
#include "sja1105_tas.h"

#define SJA1105_UNKNOWN_MULTICAST	0x010000000000ull

 
static int sja1105_hw_reset(struct device *dev, unsigned int pulse_len,
			    unsigned int startup_delay)
{
	struct gpio_desc *gpio;

	gpio = gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(gpio))
		return PTR_ERR(gpio);

	if (!gpio)
		return 0;

	gpiod_set_value_cansleep(gpio, 1);
	 
	msleep(pulse_len);
	gpiod_set_value_cansleep(gpio, 0);
	 
	msleep(startup_delay);

	gpiod_put(gpio);

	return 0;
}

static void
sja1105_port_allow_traffic(struct sja1105_l2_forwarding_entry *l2_fwd,
			   int from, int to, bool allow)
{
	if (allow)
		l2_fwd[from].reach_port |= BIT(to);
	else
		l2_fwd[from].reach_port &= ~BIT(to);
}

static bool sja1105_can_forward(struct sja1105_l2_forwarding_entry *l2_fwd,
				int from, int to)
{
	return !!(l2_fwd[from].reach_port & BIT(to));
}

static int sja1105_is_vlan_configured(struct sja1105_private *priv, u16 vid)
{
	struct sja1105_vlan_lookup_entry *vlan;
	int count, i;

	vlan = priv->static_config.tables[BLK_IDX_VLAN_LOOKUP].entries;
	count = priv->static_config.tables[BLK_IDX_VLAN_LOOKUP].entry_count;

	for (i = 0; i < count; i++)
		if (vlan[i].vlanid == vid)
			return i;

	 
	return -1;
}

static int sja1105_drop_untagged(struct dsa_switch *ds, int port, bool drop)
{
	struct sja1105_private *priv = ds->priv;
	struct sja1105_mac_config_entry *mac;

	mac = priv->static_config.tables[BLK_IDX_MAC_CONFIG].entries;

	if (mac[port].drpuntag == drop)
		return 0;

	mac[port].drpuntag = drop;

	return sja1105_dynamic_config_write(priv, BLK_IDX_MAC_CONFIG, port,
					    &mac[port], true);
}

static int sja1105_pvid_apply(struct sja1105_private *priv, int port, u16 pvid)
{
	struct sja1105_mac_config_entry *mac;

	mac = priv->static_config.tables[BLK_IDX_MAC_CONFIG].entries;

	if (mac[port].vlanid == pvid)
		return 0;

	mac[port].vlanid = pvid;

	return sja1105_dynamic_config_write(priv, BLK_IDX_MAC_CONFIG, port,
					    &mac[port], true);
}

static int sja1105_commit_pvid(struct dsa_switch *ds, int port)
{
	struct dsa_port *dp = dsa_to_port(ds, port);
	struct net_device *br = dsa_port_bridge_dev_get(dp);
	struct sja1105_private *priv = ds->priv;
	struct sja1105_vlan_lookup_entry *vlan;
	bool drop_untagged = false;
	int match, rc;
	u16 pvid;

	if (br && br_vlan_enabled(br))
		pvid = priv->bridge_pvid[port];
	else
		pvid = priv->tag_8021q_pvid[port];

	rc = sja1105_pvid_apply(priv, port, pvid);
	if (rc)
		return rc;

	 
	if (pvid == priv->bridge_pvid[port]) {
		vlan = priv->static_config.tables[BLK_IDX_VLAN_LOOKUP].entries;

		match = sja1105_is_vlan_configured(priv, pvid);

		if (match < 0 || !(vlan[match].vmemb_port & BIT(port)))
			drop_untagged = true;
	}

	if (dsa_is_cpu_port(ds, port) || dsa_is_dsa_port(ds, port))
		drop_untagged = true;

	return sja1105_drop_untagged(ds, port, drop_untagged);
}

static int sja1105_init_mac_settings(struct sja1105_private *priv)
{
	struct sja1105_mac_config_entry default_mac = {
		 
		.top  = {0x3F, 0x7F, 0xBF, 0xFF, 0x13F, 0x17F, 0x1BF, 0x1FF},
		.base = {0x0, 0x40, 0x80, 0xC0, 0x100, 0x140, 0x180, 0x1C0},
		.enabled = {true, true, true, true, true, true, true, true},
		 
		.ifg = 0,
		 
		.speed = priv->info->port_speed[SJA1105_SPEED_AUTO],
		 
		.tp_delin = 0,
		.tp_delout = 0,
		 
		.maxage = 0xFF,
		 
		.vlanprio = 0,
		.vlanid = 1,
		.ing_mirr = false,
		.egr_mirr = false,
		 
		.drpnona664 = false,
		 
		.drpdtag = false,
		 
		.drpuntag = false,
		 
		.retag = false,
		 
		.dyn_learn = false,
		.egress = false,
		.ingress = false,
	};
	struct sja1105_mac_config_entry *mac;
	struct dsa_switch *ds = priv->ds;
	struct sja1105_table *table;
	struct dsa_port *dp;

	table = &priv->static_config.tables[BLK_IDX_MAC_CONFIG];

	 
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = table->ops->max_entry_count;

	mac = table->entries;

	list_for_each_entry(dp, &ds->dst->ports, list) {
		if (dp->ds != ds)
			continue;

		mac[dp->index] = default_mac;

		 
		if (dsa_port_is_dsa(dp))
			dp->learning = true;

		 
		if (dsa_port_is_cpu(dp) || dsa_port_is_dsa(dp))
			mac[dp->index].drpuntag = true;
	}

	return 0;
}

static int sja1105_init_mii_settings(struct sja1105_private *priv)
{
	struct device *dev = &priv->spidev->dev;
	struct sja1105_xmii_params_entry *mii;
	struct dsa_switch *ds = priv->ds;
	struct sja1105_table *table;
	int i;

	table = &priv->static_config.tables[BLK_IDX_XMII_PARAMS];

	 
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	 
	table->entry_count = table->ops->max_entry_count;

	mii = table->entries;

	for (i = 0; i < ds->num_ports; i++) {
		sja1105_mii_role_t role = XMII_MAC;

		if (dsa_is_unused_port(priv->ds, i))
			continue;

		switch (priv->phy_mode[i]) {
		case PHY_INTERFACE_MODE_INTERNAL:
			if (priv->info->internal_phy[i] == SJA1105_NO_PHY)
				goto unsupported;

			mii->xmii_mode[i] = XMII_MODE_MII;
			if (priv->info->internal_phy[i] == SJA1105_PHY_BASE_TX)
				mii->special[i] = true;

			break;
		case PHY_INTERFACE_MODE_REVMII:
			role = XMII_PHY;
			fallthrough;
		case PHY_INTERFACE_MODE_MII:
			if (!priv->info->supports_mii[i])
				goto unsupported;

			mii->xmii_mode[i] = XMII_MODE_MII;
			break;
		case PHY_INTERFACE_MODE_REVRMII:
			role = XMII_PHY;
			fallthrough;
		case PHY_INTERFACE_MODE_RMII:
			if (!priv->info->supports_rmii[i])
				goto unsupported;

			mii->xmii_mode[i] = XMII_MODE_RMII;
			break;
		case PHY_INTERFACE_MODE_RGMII:
		case PHY_INTERFACE_MODE_RGMII_ID:
		case PHY_INTERFACE_MODE_RGMII_RXID:
		case PHY_INTERFACE_MODE_RGMII_TXID:
			if (!priv->info->supports_rgmii[i])
				goto unsupported;

			mii->xmii_mode[i] = XMII_MODE_RGMII;
			break;
		case PHY_INTERFACE_MODE_SGMII:
			if (!priv->info->supports_sgmii[i])
				goto unsupported;

			mii->xmii_mode[i] = XMII_MODE_SGMII;
			mii->special[i] = true;
			break;
		case PHY_INTERFACE_MODE_2500BASEX:
			if (!priv->info->supports_2500basex[i])
				goto unsupported;

			mii->xmii_mode[i] = XMII_MODE_SGMII;
			mii->special[i] = true;
			break;
unsupported:
		default:
			dev_err(dev, "Unsupported PHY mode %s on port %d!\n",
				phy_modes(priv->phy_mode[i]), i);
			return -EINVAL;
		}

		mii->phy_mac[i] = role;
	}
	return 0;
}

static int sja1105_init_static_fdb(struct sja1105_private *priv)
{
	struct sja1105_l2_lookup_entry *l2_lookup;
	struct sja1105_table *table;
	int port;

	table = &priv->static_config.tables[BLK_IDX_L2_LOOKUP];

	 
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	if (!priv->info->can_limit_mcast_flood)
		return 0;

	table->entries = kcalloc(1, table->ops->unpacked_entry_size,
				 GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = 1;
	l2_lookup = table->entries;

	 
	l2_lookup[0].macaddr = SJA1105_UNKNOWN_MULTICAST;
	l2_lookup[0].mask_macaddr = SJA1105_UNKNOWN_MULTICAST;
	l2_lookup[0].lockeds = true;
	l2_lookup[0].index = SJA1105_MAX_L2_LOOKUP_COUNT - 1;

	 
	for (port = 0; port < priv->ds->num_ports; port++)
		if (!dsa_is_unused_port(priv->ds, port))
			l2_lookup[0].destports |= BIT(port);

	return 0;
}

static int sja1105_init_l2_lookup_params(struct sja1105_private *priv)
{
	struct sja1105_l2_lookup_params_entry default_l2_lookup_params = {
		 
		.maxage = SJA1105_AGEING_TIME_MS(300000),
		 
		.dyn_tbsz = SJA1105ET_FDB_BIN_SIZE,
		 
		.start_dynspc = 0,
		 
		.poly = 0x97,
		 
		.shared_learn = false,
		 
		.no_enf_hostprt = false,
		 
		.no_mgmt_learn = true,
		 
		.use_static = true,
		 
		.owr_dyn = true,
		.drpnolearn = true,
	};
	struct dsa_switch *ds = priv->ds;
	int port, num_used_ports = 0;
	struct sja1105_table *table;
	u64 max_fdb_entries;

	for (port = 0; port < ds->num_ports; port++)
		if (!dsa_is_unused_port(ds, port))
			num_used_ports++;

	max_fdb_entries = SJA1105_MAX_L2_LOOKUP_COUNT / num_used_ports;

	for (port = 0; port < ds->num_ports; port++) {
		if (dsa_is_unused_port(ds, port))
			continue;

		default_l2_lookup_params.maxaddrp[port] = max_fdb_entries;
	}

	table = &priv->static_config.tables[BLK_IDX_L2_LOOKUP_PARAMS];

	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = table->ops->max_entry_count;

	 
	((struct sja1105_l2_lookup_params_entry *)table->entries)[0] =
				default_l2_lookup_params;

	return 0;
}

 
static int sja1105_init_static_vlan(struct sja1105_private *priv)
{
	struct sja1105_table *table;
	struct sja1105_vlan_lookup_entry pvid = {
		.type_entry = SJA1110_VLAN_D_TAG,
		.ving_mirr = 0,
		.vegr_mirr = 0,
		.vmemb_port = 0,
		.vlan_bc = 0,
		.tag_port = 0,
		.vlanid = SJA1105_DEFAULT_VLAN,
	};
	struct dsa_switch *ds = priv->ds;
	int port;

	table = &priv->static_config.tables[BLK_IDX_VLAN_LOOKUP];

	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kzalloc(table->ops->unpacked_entry_size,
				 GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = 1;

	for (port = 0; port < ds->num_ports; port++) {
		if (dsa_is_unused_port(ds, port))
			continue;

		pvid.vmemb_port |= BIT(port);
		pvid.vlan_bc |= BIT(port);
		pvid.tag_port &= ~BIT(port);

		if (dsa_is_cpu_port(ds, port) || dsa_is_dsa_port(ds, port)) {
			priv->tag_8021q_pvid[port] = SJA1105_DEFAULT_VLAN;
			priv->bridge_pvid[port] = SJA1105_DEFAULT_VLAN;
		}
	}

	((struct sja1105_vlan_lookup_entry *)table->entries)[0] = pvid;
	return 0;
}

static int sja1105_init_l2_forwarding(struct sja1105_private *priv)
{
	struct sja1105_l2_forwarding_entry *l2fwd;
	struct dsa_switch *ds = priv->ds;
	struct dsa_switch_tree *dst;
	struct sja1105_table *table;
	struct dsa_link *dl;
	int port, tc;
	int from, to;

	table = &priv->static_config.tables[BLK_IDX_L2_FORWARDING];

	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = table->ops->max_entry_count;

	l2fwd = table->entries;

	 
	for (port = 0; port < ds->num_ports; port++) {
		if (dsa_is_unused_port(ds, port))
			continue;

		for (tc = 0; tc < SJA1105_NUM_TC; tc++)
			l2fwd[port].vlan_pmap[tc] = tc;
	}

	 
	for (from = 0; from < ds->num_ports; from++) {
		if (!dsa_is_user_port(ds, from))
			continue;

		for (to = 0; to < ds->num_ports; to++) {
			if (!dsa_is_cpu_port(ds, to) &&
			    !dsa_is_dsa_port(ds, to))
				continue;

			l2fwd[from].bc_domain |= BIT(to);
			l2fwd[from].fl_domain |= BIT(to);

			sja1105_port_allow_traffic(l2fwd, from, to, true);
		}
	}

	 
	for (from = 0; from < ds->num_ports; from++) {
		if (!dsa_is_cpu_port(ds, from) && !dsa_is_dsa_port(ds, from))
			continue;

		for (to = 0; to < ds->num_ports; to++) {
			if (dsa_is_unused_port(ds, to))
				continue;

			if (from == to)
				continue;

			l2fwd[from].bc_domain |= BIT(to);
			l2fwd[from].fl_domain |= BIT(to);

			sja1105_port_allow_traffic(l2fwd, from, to, true);
		}
	}

	 
	dst = ds->dst;

	list_for_each_entry(dl, &dst->rtable, list) {
		if (dl->dp->ds != ds || dl->link_dp->cpu_dp == dl->dp->cpu_dp)
			continue;

		from = dl->dp->index;
		to = dsa_upstream_port(ds, from);

		dev_warn(ds->dev,
			 "H topology detected, cutting RX from DSA link %d to CPU port %d to prevent TX packet loops\n",
			 from, to);

		sja1105_port_allow_traffic(l2fwd, from, to, false);

		l2fwd[from].bc_domain &= ~BIT(to);
		l2fwd[from].fl_domain &= ~BIT(to);
	}

	 
	for (port = 0; port < ds->num_ports; port++) {
		if (dsa_is_unused_port(ds, port))
			continue;

		priv->ucast_egress_floods |= BIT(port);
		priv->bcast_egress_floods |= BIT(port);
	}

	 
	for (tc = 0; tc < SJA1105_NUM_TC; tc++) {
		for (port = 0; port < ds->num_ports; port++) {
			if (dsa_is_unused_port(ds, port))
				continue;

			l2fwd[ds->num_ports + tc].vlan_pmap[port] = tc;
		}

		l2fwd[ds->num_ports + tc].type_egrpcp2outputq = true;
	}

	return 0;
}

static int sja1110_init_pcp_remapping(struct sja1105_private *priv)
{
	struct sja1110_pcp_remapping_entry *pcp_remap;
	struct dsa_switch *ds = priv->ds;
	struct sja1105_table *table;
	int port, tc;

	table = &priv->static_config.tables[BLK_IDX_PCP_REMAPPING];

	 
	if (!table->ops->max_entry_count)
		return 0;

	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = table->ops->max_entry_count;

	pcp_remap = table->entries;

	 
	for (port = 0; port < ds->num_ports; port++) {
		if (dsa_is_unused_port(ds, port))
			continue;

		for (tc = 0; tc < SJA1105_NUM_TC; tc++)
			pcp_remap[port].egrpcp[tc] = tc;
	}

	return 0;
}

static int sja1105_init_l2_forwarding_params(struct sja1105_private *priv)
{
	struct sja1105_l2_forwarding_params_entry *l2fwd_params;
	struct sja1105_table *table;

	table = &priv->static_config.tables[BLK_IDX_L2_FORWARDING_PARAMS];

	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = table->ops->max_entry_count;

	 
	l2fwd_params = table->entries;

	 
	l2fwd_params->max_dynp = 0;
	 
	l2fwd_params->part_spc[0] = priv->info->max_frame_mem;

	return 0;
}

void sja1105_frame_memory_partitioning(struct sja1105_private *priv)
{
	struct sja1105_l2_forwarding_params_entry *l2_fwd_params;
	struct sja1105_vl_forwarding_params_entry *vl_fwd_params;
	struct sja1105_table *table;

	table = &priv->static_config.tables[BLK_IDX_L2_FORWARDING_PARAMS];
	l2_fwd_params = table->entries;
	l2_fwd_params->part_spc[0] = SJA1105_MAX_FRAME_MEMORY;

	 
	if (!priv->static_config.tables[BLK_IDX_VL_FORWARDING].entry_count)
		return;

	table = &priv->static_config.tables[BLK_IDX_VL_FORWARDING_PARAMS];
	vl_fwd_params = table->entries;

	l2_fwd_params->part_spc[0] -= SJA1105_VL_FRAME_MEMORY;
	vl_fwd_params->partspc[0] = SJA1105_VL_FRAME_MEMORY;
}

 
static void sja1110_select_tdmaconfigidx(struct sja1105_private *priv)
{
	struct sja1105_general_params_entry *general_params;
	struct sja1105_table *table;
	bool port_1_is_base_tx;
	bool port_3_is_2500;
	bool port_4_is_2500;
	u64 tdmaconfigidx;

	if (priv->info->device_id != SJA1110_DEVICE_ID)
		return;

	table = &priv->static_config.tables[BLK_IDX_GENERAL_PARAMS];
	general_params = table->entries;

	 
	port_1_is_base_tx = priv->phy_mode[1] == PHY_INTERFACE_MODE_INTERNAL;
	port_3_is_2500 = priv->phy_mode[3] == PHY_INTERFACE_MODE_2500BASEX;
	port_4_is_2500 = priv->phy_mode[4] == PHY_INTERFACE_MODE_2500BASEX;

	if (port_1_is_base_tx)
		 
		tdmaconfigidx = 5;
	else if (port_3_is_2500 && port_4_is_2500)
		 
		tdmaconfigidx = 1;
	else if (port_3_is_2500)
		 
		tdmaconfigidx = 3;
	else if (port_4_is_2500)
		 
		tdmaconfigidx = 2;
	else
		 
		tdmaconfigidx = 14;

	general_params->tdmaconfigidx = tdmaconfigidx;
}

static int sja1105_init_topology(struct sja1105_private *priv,
				 struct sja1105_general_params_entry *general_params)
{
	struct dsa_switch *ds = priv->ds;
	int port;

	 
	general_params->host_port = ds->num_ports;

	 
	if (!priv->info->multiple_cascade_ports)
		general_params->casc_port = ds->num_ports;

	for (port = 0; port < ds->num_ports; port++) {
		bool is_upstream = dsa_is_upstream_port(ds, port);
		bool is_dsa_link = dsa_is_dsa_port(ds, port);

		 
		if (is_upstream) {
			if (general_params->host_port == ds->num_ports) {
				general_params->host_port = port;
			} else {
				dev_err(ds->dev,
					"Port %llu is already a host port, configuring %d as one too is not supported\n",
					general_params->host_port, port);
				return -EINVAL;
			}
		}

		 
		if (is_dsa_link && !is_upstream) {
			if (priv->info->multiple_cascade_ports) {
				general_params->casc_port |= BIT(port);
			} else if (general_params->casc_port == ds->num_ports) {
				general_params->casc_port = port;
			} else {
				dev_err(ds->dev,
					"Port %llu is already a cascade port, configuring %d as one too is not supported\n",
					general_params->casc_port, port);
				return -EINVAL;
			}
		}
	}

	if (general_params->host_port == ds->num_ports) {
		dev_err(ds->dev, "No host port configured\n");
		return -EINVAL;
	}

	return 0;
}

static int sja1105_init_general_params(struct sja1105_private *priv)
{
	struct sja1105_general_params_entry default_general_params = {
		 
		.mirr_ptacu = true,
		.switchid = priv->ds->index,
		 
		.hostprio = 7,
		.mac_fltres1 = SJA1105_LINKLOCAL_FILTER_A,
		.mac_flt1    = SJA1105_LINKLOCAL_FILTER_A_MASK,
		.incl_srcpt1 = true,
		.send_meta1  = true,
		.mac_fltres0 = SJA1105_LINKLOCAL_FILTER_B,
		.mac_flt0    = SJA1105_LINKLOCAL_FILTER_B_MASK,
		.incl_srcpt0 = true,
		.send_meta0  = true,
		 
		.mirr_port = priv->ds->num_ports,
		 
		.vllupformat = SJA1105_VL_FORMAT_PSFP,
		.vlmarker = 0,
		.vlmask = 0,
		 
		.ignore2stf = 0,
		 
		.tpid = ETH_P_SJA1105,
		.tpid2 = ETH_P_SJA1105,
		 
		.tte_en = true,
		 
		.header_type = ETH_P_SJA1110,
	};
	struct sja1105_general_params_entry *general_params;
	struct sja1105_table *table;
	int rc;

	rc = sja1105_init_topology(priv, &default_general_params);
	if (rc)
		return rc;

	table = &priv->static_config.tables[BLK_IDX_GENERAL_PARAMS];

	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = table->ops->max_entry_count;

	general_params = table->entries;

	 
	general_params[0] = default_general_params;

	sja1110_select_tdmaconfigidx(priv);

	return 0;
}

static int sja1105_init_avb_params(struct sja1105_private *priv)
{
	struct sja1105_avb_params_entry *avb;
	struct sja1105_table *table;

	table = &priv->static_config.tables[BLK_IDX_AVB_PARAMS];

	 
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = table->ops->max_entry_count;

	avb = table->entries;

	 
	avb->destmeta = SJA1105_META_DMAC;
	avb->srcmeta  = SJA1105_META_SMAC;
	 
	avb->cas_master = false;

	return 0;
}

 
#define SJA1105_RATE_MBPS(speed) (((speed) * 64000) / 1000)

static int sja1105_init_l2_policing(struct sja1105_private *priv)
{
	struct sja1105_l2_policing_entry *policing;
	struct dsa_switch *ds = priv->ds;
	struct sja1105_table *table;
	int port, tc;

	table = &priv->static_config.tables[BLK_IDX_L2_POLICING];

	 
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	table->entries = kcalloc(table->ops->max_entry_count,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;

	table->entry_count = table->ops->max_entry_count;

	policing = table->entries;

	 
	for (port = 0; port < ds->num_ports; port++) {
		int mcast = (ds->num_ports * (SJA1105_NUM_TC + 1)) + port;
		int bcast = (ds->num_ports * SJA1105_NUM_TC) + port;

		for (tc = 0; tc < SJA1105_NUM_TC; tc++)
			policing[port * SJA1105_NUM_TC + tc].sharindx = port;

		policing[bcast].sharindx = port;
		 
		if (mcast < table->ops->max_entry_count)
			policing[mcast].sharindx = port;
	}

	 
	for (port = 0; port < ds->num_ports; port++) {
		int mtu = VLAN_ETH_FRAME_LEN + ETH_FCS_LEN;

		if (dsa_is_cpu_port(ds, port) || dsa_is_dsa_port(ds, port))
			mtu += VLAN_HLEN;

		policing[port].smax = 65535;  
		policing[port].rate = SJA1105_RATE_MBPS(1000);
		policing[port].maxlen = mtu;
		policing[port].partition = 0;
	}

	return 0;
}

static int sja1105_static_config_load(struct sja1105_private *priv)
{
	int rc;

	sja1105_static_config_free(&priv->static_config);
	rc = sja1105_static_config_init(&priv->static_config,
					priv->info->static_ops,
					priv->info->device_id);
	if (rc)
		return rc;

	 
	rc = sja1105_init_mac_settings(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_mii_settings(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_static_fdb(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_static_vlan(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_l2_lookup_params(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_l2_forwarding(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_l2_forwarding_params(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_l2_policing(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_general_params(priv);
	if (rc < 0)
		return rc;
	rc = sja1105_init_avb_params(priv);
	if (rc < 0)
		return rc;
	rc = sja1110_init_pcp_remapping(priv);
	if (rc < 0)
		return rc;

	 
	return sja1105_static_config_upload(priv);
}

 
static int sja1105_parse_rgmii_delays(struct sja1105_private *priv, int port,
				      struct device_node *port_dn)
{
	phy_interface_t phy_mode = priv->phy_mode[port];
	struct device *dev = &priv->spidev->dev;
	int rx_delay = -1, tx_delay = -1;

	if (!phy_interface_mode_is_rgmii(phy_mode))
		return 0;

	of_property_read_u32(port_dn, "rx-internal-delay-ps", &rx_delay);
	of_property_read_u32(port_dn, "tx-internal-delay-ps", &tx_delay);

	if (rx_delay == -1 && tx_delay == -1 && priv->fixed_link[port]) {
		dev_warn(dev,
			 "Port %d interpreting RGMII delay settings based on \"phy-mode\" property, "
			 "please update device tree to specify \"rx-internal-delay-ps\" and "
			 "\"tx-internal-delay-ps\"",
			 port);

		if (phy_mode == PHY_INTERFACE_MODE_RGMII_RXID ||
		    phy_mode == PHY_INTERFACE_MODE_RGMII_ID)
			rx_delay = 2000;

		if (phy_mode == PHY_INTERFACE_MODE_RGMII_TXID ||
		    phy_mode == PHY_INTERFACE_MODE_RGMII_ID)
			tx_delay = 2000;
	}

	if (rx_delay < 0)
		rx_delay = 0;
	if (tx_delay < 0)
		tx_delay = 0;

	if ((rx_delay || tx_delay) && !priv->info->setup_rgmii_delay) {
		dev_err(dev, "Chip cannot apply RGMII delays\n");
		return -EINVAL;
	}

	if ((rx_delay && rx_delay < SJA1105_RGMII_DELAY_MIN_PS) ||
	    (tx_delay && tx_delay < SJA1105_RGMII_DELAY_MIN_PS) ||
	    (rx_delay > SJA1105_RGMII_DELAY_MAX_PS) ||
	    (tx_delay > SJA1105_RGMII_DELAY_MAX_PS)) {
		dev_err(dev,
			"port %d RGMII delay values out of range, must be between %d and %d ps\n",
			port, SJA1105_RGMII_DELAY_MIN_PS, SJA1105_RGMII_DELAY_MAX_PS);
		return -ERANGE;
	}

	priv->rgmii_rx_delay_ps[port] = rx_delay;
	priv->rgmii_tx_delay_ps[port] = tx_delay;

	return 0;
}

static int sja1105_parse_ports_node(struct sja1105_private *priv,
				    struct device_node *ports_node)
{
	struct device *dev = &priv->spidev->dev;
	struct device_node *child;

	for_each_available_child_of_node(ports_node, child) {
		struct device_node *phy_node;
		phy_interface_t phy_mode;
		u32 index;
		int err;

		 
		if (of_property_read_u32(child, "reg", &index) < 0) {
			dev_err(dev, "Port number not defined in device tree "
				"(property \"reg\")\n");
			of_node_put(child);
			return -ENODEV;
		}

		 
		err = of_get_phy_mode(child, &phy_mode);
		if (err) {
			dev_err(dev, "Failed to read phy-mode or "
				"phy-interface-type property for port %d\n",
				index);
			of_node_put(child);
			return -ENODEV;
		}

		phy_node = of_parse_phandle(child, "phy-handle", 0);
		if (!phy_node) {
			if (!of_phy_is_fixed_link(child)) {
				dev_err(dev, "phy-handle or fixed-link "
					"properties missing!\n");
				of_node_put(child);
				return -ENODEV;
			}
			 
			priv->fixed_link[index] = true;
		} else {
			of_node_put(phy_node);
		}

		priv->phy_mode[index] = phy_mode;

		err = sja1105_parse_rgmii_delays(priv, index, child);
		if (err) {
			of_node_put(child);
			return err;
		}
	}

	return 0;
}

static int sja1105_parse_dt(struct sja1105_private *priv)
{
	struct device *dev = &priv->spidev->dev;
	struct device_node *switch_node = dev->of_node;
	struct device_node *ports_node;
	int rc;

	ports_node = of_get_child_by_name(switch_node, "ports");
	if (!ports_node)
		ports_node = of_get_child_by_name(switch_node, "ethernet-ports");
	if (!ports_node) {
		dev_err(dev, "Incorrect bindings: absent \"ports\" node\n");
		return -ENODEV;
	}

	rc = sja1105_parse_ports_node(priv, ports_node);
	of_node_put(ports_node);

	return rc;
}

 
static int sja1105_port_speed_to_ethtool(struct sja1105_private *priv,
					 u64 speed)
{
	if (speed == priv->info->port_speed[SJA1105_SPEED_10MBPS])
		return SPEED_10;
	if (speed == priv->info->port_speed[SJA1105_SPEED_100MBPS])
		return SPEED_100;
	if (speed == priv->info->port_speed[SJA1105_SPEED_1000MBPS])
		return SPEED_1000;
	if (speed == priv->info->port_speed[SJA1105_SPEED_2500MBPS])
		return SPEED_2500;
	return SPEED_UNKNOWN;
}

 
static int sja1105_adjust_port_config(struct sja1105_private *priv, int port,
				      int speed_mbps)
{
	struct sja1105_mac_config_entry *mac;
	struct device *dev = priv->ds->dev;
	u64 speed;
	int rc;

	 
	mac = priv->static_config.tables[BLK_IDX_MAC_CONFIG].entries;

	switch (speed_mbps) {
	case SPEED_UNKNOWN:
		 
		speed = priv->info->port_speed[SJA1105_SPEED_AUTO];
		break;
	case SPEED_10:
		speed = priv->info->port_speed[SJA1105_SPEED_10MBPS];
		break;
	case SPEED_100:
		speed = priv->info->port_speed[SJA1105_SPEED_100MBPS];
		break;
	case SPEED_1000:
		speed = priv->info->port_speed[SJA1105_SPEED_1000MBPS];
		break;
	case SPEED_2500:
		speed = priv->info->port_speed[SJA1105_SPEED_2500MBPS];
		break;
	default:
		dev_err(dev, "Invalid speed %iMbps\n", speed_mbps);
		return -EINVAL;
	}

	 
	if (priv->phy_mode[port] == PHY_INTERFACE_MODE_SGMII)
		mac[port].speed = priv->info->port_speed[SJA1105_SPEED_1000MBPS];
	else if (priv->phy_mode[port] == PHY_INTERFACE_MODE_2500BASEX)
		mac[port].speed = priv->info->port_speed[SJA1105_SPEED_2500MBPS];
	else
		mac[port].speed = speed;

	 
	rc = sja1105_dynamic_config_write(priv, BLK_IDX_MAC_CONFIG, port,
					  &mac[port], true);
	if (rc < 0) {
		dev_err(dev, "Failed to write MAC config: %d\n", rc);
		return rc;
	}

	 
	if (!phy_interface_mode_is_rgmii(priv->phy_mode[port]))
		return 0;

	return sja1105_clocking_setup_port(priv, port);
}

static struct phylink_pcs *
sja1105_mac_select_pcs(struct dsa_switch *ds, int port, phy_interface_t iface)
{
	struct sja1105_private *priv = ds->priv;
	struct dw_xpcs *xpcs = priv->xpcs[port];

	if (xpcs)
		return &xpcs->pcs;

	return NULL;
}

static void sja1105_mac_link_down(struct dsa_switch *ds, int port,
				  unsigned int mode,
				  phy_interface_t interface)
{
	sja1105_inhibit_tx(ds->priv, BIT(port), true);
}

static void sja1105_mac_link_up(struct dsa_switch *ds, int port,
				unsigned int mode,
				phy_interface_t interface,
				struct phy_device *phydev,
				int speed, int duplex,
				bool tx_pause, bool rx_pause)
{
	struct sja1105_private *priv = ds->priv;

	sja1105_adjust_port_config(priv, port, speed);

	sja1105_inhibit_tx(priv, BIT(port), false);
}

static void sja1105_phylink_get_caps(struct dsa_switch *ds, int port,
				     struct phylink_config *config)
{
	struct sja1105_private *priv = ds->priv;
	struct sja1105_xmii_params_entry *mii;
	phy_interface_t phy_mode;

	phy_mode = priv->phy_mode[port];
	if (phy_mode == PHY_INTERFACE_MODE_SGMII ||
	    phy_mode == PHY_INTERFACE_MODE_2500BASEX) {
		 
		if (priv->info->supports_sgmii[port])
			__set_bit(PHY_INTERFACE_MODE_SGMII,
				  config->supported_interfaces);

		if (priv->info->supports_2500basex[port])
			__set_bit(PHY_INTERFACE_MODE_2500BASEX,
				  config->supported_interfaces);
	} else {
		 
		__set_bit(phy_mode, config->supported_interfaces);
	}

	 
	config->mac_capabilities = MAC_10FD | MAC_100FD;

	mii = priv->static_config.tables[BLK_IDX_XMII_PARAMS].entries;
	if (mii->xmii_mode[port] == XMII_MODE_RGMII ||
	    mii->xmii_mode[port] == XMII_MODE_SGMII)
		config->mac_capabilities |= MAC_1000FD;

	if (priv->info->supports_2500basex[port])
		config->mac_capabilities |= MAC_2500FD;
}

static int
sja1105_find_static_fdb_entry(struct sja1105_private *priv, int port,
			      const struct sja1105_l2_lookup_entry *requested)
{
	struct sja1105_l2_lookup_entry *l2_lookup;
	struct sja1105_table *table;
	int i;

	table = &priv->static_config.tables[BLK_IDX_L2_LOOKUP];
	l2_lookup = table->entries;

	for (i = 0; i < table->entry_count; i++)
		if (l2_lookup[i].macaddr == requested->macaddr &&
		    l2_lookup[i].vlanid == requested->vlanid &&
		    l2_lookup[i].destports & BIT(port))
			return i;

	return -1;
}

 
static int
sja1105_static_fdb_change(struct sja1105_private *priv, int port,
			  const struct sja1105_l2_lookup_entry *requested,
			  bool keep)
{
	struct sja1105_l2_lookup_entry *l2_lookup;
	struct sja1105_table *table;
	int rc, match;

	table = &priv->static_config.tables[BLK_IDX_L2_LOOKUP];

	match = sja1105_find_static_fdb_entry(priv, port, requested);
	if (match < 0) {
		 
		if (!keep)
			return 0;

		 
		rc = sja1105_table_resize(table, table->entry_count + 1);
		if (rc)
			return rc;

		match = table->entry_count - 1;
	}

	 
	l2_lookup = table->entries;

	 
	if (keep) {
		l2_lookup[match] = *requested;
		return 0;
	}

	 
	l2_lookup[match] = l2_lookup[table->entry_count - 1];
	return sja1105_table_resize(table, table->entry_count - 1);
}

 
static int sja1105et_fdb_index(int bin, int way)
{
	return bin * SJA1105ET_FDB_BIN_SIZE + way;
}

static int sja1105et_is_fdb_entry_in_bin(struct sja1105_private *priv, int bin,
					 const u8 *addr, u16 vid,
					 struct sja1105_l2_lookup_entry *match,
					 int *last_unused)
{
	int way;

	for (way = 0; way < SJA1105ET_FDB_BIN_SIZE; way++) {
		struct sja1105_l2_lookup_entry l2_lookup = {0};
		int index = sja1105et_fdb_index(bin, way);

		 
		if (sja1105_dynamic_config_read(priv, BLK_IDX_L2_LOOKUP,
						index, &l2_lookup)) {
			if (last_unused)
				*last_unused = way;
			continue;
		}

		if (l2_lookup.macaddr == ether_addr_to_u64(addr) &&
		    l2_lookup.vlanid == vid) {
			if (match)
				*match = l2_lookup;
			return way;
		}
	}
	 
	return -1;
}

int sja1105et_fdb_add(struct dsa_switch *ds, int port,
		      const unsigned char *addr, u16 vid)
{
	struct sja1105_l2_lookup_entry l2_lookup = {0}, tmp;
	struct sja1105_private *priv = ds->priv;
	struct device *dev = ds->dev;
	int last_unused = -1;
	int start, end, i;
	int bin, way, rc;

	bin = sja1105et_fdb_hash(priv, addr, vid);

	way = sja1105et_is_fdb_entry_in_bin(priv, bin, addr, vid,
					    &l2_lookup, &last_unused);
	if (way >= 0) {
		 
		if ((l2_lookup.destports & BIT(port)) && l2_lookup.lockeds)
			return 0;
		l2_lookup.destports |= BIT(port);
	} else {
		int index = sja1105et_fdb_index(bin, way);

		 
		l2_lookup.macaddr = ether_addr_to_u64(addr);
		l2_lookup.destports = BIT(port);
		l2_lookup.vlanid = vid;

		if (last_unused >= 0) {
			way = last_unused;
		} else {
			 
			get_random_bytes(&way, sizeof(u8));
			way %= SJA1105ET_FDB_BIN_SIZE;
			dev_warn(dev, "Warning, FDB bin %d full while adding entry for %pM. Evicting entry %u.\n",
				 bin, addr, way);
			 
			sja1105_dynamic_config_write(priv, BLK_IDX_L2_LOOKUP,
						     index, NULL, false);
		}
	}
	l2_lookup.lockeds = true;
	l2_lookup.index = sja1105et_fdb_index(bin, way);

	rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_LOOKUP,
					  l2_lookup.index, &l2_lookup,
					  true);
	if (rc < 0)
		return rc;

	 
	start = sja1105et_fdb_index(bin, 0);
	end = sja1105et_fdb_index(bin, way);

	for (i = start; i < end; i++) {
		rc = sja1105_dynamic_config_read(priv, BLK_IDX_L2_LOOKUP,
						 i, &tmp);
		if (rc == -ENOENT)
			continue;
		if (rc)
			return rc;

		if (tmp.macaddr != ether_addr_to_u64(addr) || tmp.vlanid != vid)
			continue;

		rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_LOOKUP,
						  i, NULL, false);
		if (rc)
			return rc;

		break;
	}

	return sja1105_static_fdb_change(priv, port, &l2_lookup, true);
}

int sja1105et_fdb_del(struct dsa_switch *ds, int port,
		      const unsigned char *addr, u16 vid)
{
	struct sja1105_l2_lookup_entry l2_lookup = {0};
	struct sja1105_private *priv = ds->priv;
	int index, bin, way, rc;
	bool keep;

	bin = sja1105et_fdb_hash(priv, addr, vid);
	way = sja1105et_is_fdb_entry_in_bin(priv, bin, addr, vid,
					    &l2_lookup, NULL);
	if (way < 0)
		return 0;
	index = sja1105et_fdb_index(bin, way);

	 
	l2_lookup.destports &= ~BIT(port);

	if (l2_lookup.destports)
		keep = true;
	else
		keep = false;

	rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_LOOKUP,
					  index, &l2_lookup, keep);
	if (rc < 0)
		return rc;

	return sja1105_static_fdb_change(priv, port, &l2_lookup, keep);
}

int sja1105pqrs_fdb_add(struct dsa_switch *ds, int port,
			const unsigned char *addr, u16 vid)
{
	struct sja1105_l2_lookup_entry l2_lookup = {0}, tmp;
	struct sja1105_private *priv = ds->priv;
	int rc, i;

	 
	l2_lookup.macaddr = ether_addr_to_u64(addr);
	l2_lookup.vlanid = vid;
	l2_lookup.mask_macaddr = GENMASK_ULL(ETH_ALEN * 8 - 1, 0);
	l2_lookup.mask_vlanid = VLAN_VID_MASK;
	l2_lookup.destports = BIT(port);

	tmp = l2_lookup;

	rc = sja1105_dynamic_config_read(priv, BLK_IDX_L2_LOOKUP,
					 SJA1105_SEARCH, &tmp);
	if (rc == 0 && tmp.index != SJA1105_MAX_L2_LOOKUP_COUNT - 1) {
		 
		if ((tmp.destports & BIT(port)) && tmp.lockeds)
			return 0;

		l2_lookup = tmp;

		 
		l2_lookup.destports |= BIT(port);
		goto skip_finding_an_index;
	}

	 
	for (i = 0; i < SJA1105_MAX_L2_LOOKUP_COUNT; i++) {
		rc = sja1105_dynamic_config_read(priv, BLK_IDX_L2_LOOKUP,
						 i, NULL);
		if (rc < 0)
			break;
	}
	if (i == SJA1105_MAX_L2_LOOKUP_COUNT) {
		dev_err(ds->dev, "FDB is full, cannot add entry.\n");
		return -EINVAL;
	}
	l2_lookup.index = i;

skip_finding_an_index:
	l2_lookup.lockeds = true;

	rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_LOOKUP,
					  l2_lookup.index, &l2_lookup,
					  true);
	if (rc < 0)
		return rc;

	 
	tmp = l2_lookup;

	rc = sja1105_dynamic_config_read(priv, BLK_IDX_L2_LOOKUP,
					 SJA1105_SEARCH, &tmp);
	if (rc < 0) {
		dev_err(ds->dev,
			"port %d failed to read back entry for %pM vid %d: %pe\n",
			port, addr, vid, ERR_PTR(rc));
		return rc;
	}

	if (tmp.index < l2_lookup.index) {
		rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_LOOKUP,
						  tmp.index, NULL, false);
		if (rc < 0)
			return rc;
	}

	return sja1105_static_fdb_change(priv, port, &l2_lookup, true);
}

int sja1105pqrs_fdb_del(struct dsa_switch *ds, int port,
			const unsigned char *addr, u16 vid)
{
	struct sja1105_l2_lookup_entry l2_lookup = {0};
	struct sja1105_private *priv = ds->priv;
	bool keep;
	int rc;

	l2_lookup.macaddr = ether_addr_to_u64(addr);
	l2_lookup.vlanid = vid;
	l2_lookup.mask_macaddr = GENMASK_ULL(ETH_ALEN * 8 - 1, 0);
	l2_lookup.mask_vlanid = VLAN_VID_MASK;
	l2_lookup.destports = BIT(port);

	rc = sja1105_dynamic_config_read(priv, BLK_IDX_L2_LOOKUP,
					 SJA1105_SEARCH, &l2_lookup);
	if (rc < 0)
		return 0;

	l2_lookup.destports &= ~BIT(port);

	 
	if (l2_lookup.destports)
		keep = true;
	else
		keep = false;

	rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_LOOKUP,
					  l2_lookup.index, &l2_lookup, keep);
	if (rc < 0)
		return rc;

	return sja1105_static_fdb_change(priv, port, &l2_lookup, keep);
}

static int sja1105_fdb_add(struct dsa_switch *ds, int port,
			   const unsigned char *addr, u16 vid,
			   struct dsa_db db)
{
	struct sja1105_private *priv = ds->priv;
	int rc;

	if (!vid) {
		switch (db.type) {
		case DSA_DB_PORT:
			vid = dsa_tag_8021q_standalone_vid(db.dp);
			break;
		case DSA_DB_BRIDGE:
			vid = dsa_tag_8021q_bridge_vid(db.bridge.num);
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	mutex_lock(&priv->fdb_lock);
	rc = priv->info->fdb_add_cmd(ds, port, addr, vid);
	mutex_unlock(&priv->fdb_lock);

	return rc;
}

static int __sja1105_fdb_del(struct dsa_switch *ds, int port,
			     const unsigned char *addr, u16 vid,
			     struct dsa_db db)
{
	struct sja1105_private *priv = ds->priv;

	if (!vid) {
		switch (db.type) {
		case DSA_DB_PORT:
			vid = dsa_tag_8021q_standalone_vid(db.dp);
			break;
		case DSA_DB_BRIDGE:
			vid = dsa_tag_8021q_bridge_vid(db.bridge.num);
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	return priv->info->fdb_del_cmd(ds, port, addr, vid);
}

static int sja1105_fdb_del(struct dsa_switch *ds, int port,
			   const unsigned char *addr, u16 vid,
			   struct dsa_db db)
{
	struct sja1105_private *priv = ds->priv;
	int rc;

	mutex_lock(&priv->fdb_lock);
	rc = __sja1105_fdb_del(ds, port, addr, vid, db);
	mutex_unlock(&priv->fdb_lock);

	return rc;
}

static int sja1105_fdb_dump(struct dsa_switch *ds, int port,
			    dsa_fdb_dump_cb_t *cb, void *data)
{
	struct sja1105_private *priv = ds->priv;
	struct device *dev = ds->dev;
	int i;

	for (i = 0; i < SJA1105_MAX_L2_LOOKUP_COUNT; i++) {
		struct sja1105_l2_lookup_entry l2_lookup = {0};
		u8 macaddr[ETH_ALEN];
		int rc;

		rc = sja1105_dynamic_config_read(priv, BLK_IDX_L2_LOOKUP,
						 i, &l2_lookup);
		 
		if (rc == -ENOENT)
			continue;
		if (rc) {
			dev_err(dev, "Failed to dump FDB: %d\n", rc);
			return rc;
		}

		 
		if (!(l2_lookup.destports & BIT(port)))
			continue;

		u64_to_ether_addr(l2_lookup.macaddr, macaddr);

		 
		if (is_multicast_ether_addr(macaddr))
			continue;

		 
		if (vid_is_dsa_8021q(l2_lookup.vlanid))
			l2_lookup.vlanid = 0;
		rc = cb(macaddr, l2_lookup.vlanid, l2_lookup.lockeds, data);
		if (rc)
			return rc;
	}
	return 0;
}

static void sja1105_fast_age(struct dsa_switch *ds, int port)
{
	struct dsa_port *dp = dsa_to_port(ds, port);
	struct sja1105_private *priv = ds->priv;
	struct dsa_db db = {
		.type = DSA_DB_BRIDGE,
		.bridge = {
			.dev = dsa_port_bridge_dev_get(dp),
			.num = dsa_port_bridge_num_get(dp),
		},
	};
	int i;

	mutex_lock(&priv->fdb_lock);

	for (i = 0; i < SJA1105_MAX_L2_LOOKUP_COUNT; i++) {
		struct sja1105_l2_lookup_entry l2_lookup = {0};
		u8 macaddr[ETH_ALEN];
		int rc;

		rc = sja1105_dynamic_config_read(priv, BLK_IDX_L2_LOOKUP,
						 i, &l2_lookup);
		 
		if (rc == -ENOENT)
			continue;
		if (rc) {
			dev_err(ds->dev, "Failed to read FDB: %pe\n",
				ERR_PTR(rc));
			break;
		}

		if (!(l2_lookup.destports & BIT(port)))
			continue;

		 
		if (l2_lookup.lockeds)
			continue;

		u64_to_ether_addr(l2_lookup.macaddr, macaddr);

		rc = __sja1105_fdb_del(ds, port, macaddr, l2_lookup.vlanid, db);
		if (rc) {
			dev_err(ds->dev,
				"Failed to delete FDB entry %pM vid %lld: %pe\n",
				macaddr, l2_lookup.vlanid, ERR_PTR(rc));
			break;
		}
	}

	mutex_unlock(&priv->fdb_lock);
}

static int sja1105_mdb_add(struct dsa_switch *ds, int port,
			   const struct switchdev_obj_port_mdb *mdb,
			   struct dsa_db db)
{
	return sja1105_fdb_add(ds, port, mdb->addr, mdb->vid, db);
}

static int sja1105_mdb_del(struct dsa_switch *ds, int port,
			   const struct switchdev_obj_port_mdb *mdb,
			   struct dsa_db db)
{
	return sja1105_fdb_del(ds, port, mdb->addr, mdb->vid, db);
}

 
static int sja1105_manage_flood_domains(struct sja1105_private *priv)
{
	struct sja1105_l2_forwarding_entry *l2_fwd;
	struct dsa_switch *ds = priv->ds;
	int from, to, rc;

	l2_fwd = priv->static_config.tables[BLK_IDX_L2_FORWARDING].entries;

	for (from = 0; from < ds->num_ports; from++) {
		u64 fl_domain = 0, bc_domain = 0;

		for (to = 0; to < priv->ds->num_ports; to++) {
			if (!sja1105_can_forward(l2_fwd, from, to))
				continue;

			if (priv->ucast_egress_floods & BIT(to))
				fl_domain |= BIT(to);
			if (priv->bcast_egress_floods & BIT(to))
				bc_domain |= BIT(to);
		}

		 
		if (l2_fwd[from].fl_domain == fl_domain &&
		    l2_fwd[from].bc_domain == bc_domain)
			continue;

		l2_fwd[from].fl_domain = fl_domain;
		l2_fwd[from].bc_domain = bc_domain;

		rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_FORWARDING,
						  from, &l2_fwd[from], true);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int sja1105_bridge_member(struct dsa_switch *ds, int port,
				 struct dsa_bridge bridge, bool member)
{
	struct sja1105_l2_forwarding_entry *l2_fwd;
	struct sja1105_private *priv = ds->priv;
	int i, rc;

	l2_fwd = priv->static_config.tables[BLK_IDX_L2_FORWARDING].entries;

	for (i = 0; i < ds->num_ports; i++) {
		 
		if (!dsa_is_user_port(ds, i))
			continue;
		 
		if (i == port)
			continue;
		if (!dsa_port_offloads_bridge(dsa_to_port(ds, i), &bridge))
			continue;
		sja1105_port_allow_traffic(l2_fwd, i, port, member);
		sja1105_port_allow_traffic(l2_fwd, port, i, member);

		rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_FORWARDING,
						  i, &l2_fwd[i], true);
		if (rc < 0)
			return rc;
	}

	rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_FORWARDING,
					  port, &l2_fwd[port], true);
	if (rc)
		return rc;

	rc = sja1105_commit_pvid(ds, port);
	if (rc)
		return rc;

	return sja1105_manage_flood_domains(priv);
}

static void sja1105_bridge_stp_state_set(struct dsa_switch *ds, int port,
					 u8 state)
{
	struct dsa_port *dp = dsa_to_port(ds, port);
	struct sja1105_private *priv = ds->priv;
	struct sja1105_mac_config_entry *mac;

	mac = priv->static_config.tables[BLK_IDX_MAC_CONFIG].entries;

	switch (state) {
	case BR_STATE_DISABLED:
	case BR_STATE_BLOCKING:
		 
		mac[port].ingress   = false;
		mac[port].egress    = false;
		mac[port].dyn_learn = false;
		break;
	case BR_STATE_LISTENING:
		mac[port].ingress   = true;
		mac[port].egress    = false;
		mac[port].dyn_learn = false;
		break;
	case BR_STATE_LEARNING:
		mac[port].ingress   = true;
		mac[port].egress    = false;
		mac[port].dyn_learn = dp->learning;
		break;
	case BR_STATE_FORWARDING:
		mac[port].ingress   = true;
		mac[port].egress    = true;
		mac[port].dyn_learn = dp->learning;
		break;
	default:
		dev_err(ds->dev, "invalid STP state: %d\n", state);
		return;
	}

	sja1105_dynamic_config_write(priv, BLK_IDX_MAC_CONFIG, port,
				     &mac[port], true);
}

static int sja1105_bridge_join(struct dsa_switch *ds, int port,
			       struct dsa_bridge bridge,
			       bool *tx_fwd_offload,
			       struct netlink_ext_ack *extack)
{
	int rc;

	rc = sja1105_bridge_member(ds, port, bridge, true);
	if (rc)
		return rc;

	rc = dsa_tag_8021q_bridge_join(ds, port, bridge);
	if (rc) {
		sja1105_bridge_member(ds, port, bridge, false);
		return rc;
	}

	*tx_fwd_offload = true;

	return 0;
}

static void sja1105_bridge_leave(struct dsa_switch *ds, int port,
				 struct dsa_bridge bridge)
{
	dsa_tag_8021q_bridge_leave(ds, port, bridge);
	sja1105_bridge_member(ds, port, bridge, false);
}

#define BYTES_PER_KBIT (1000LL / 8)
 
#define SJA1110_FIXED_CBS(port, prio) ((((port) - 1) * SJA1105_NUM_TC) + (prio))

static int sja1105_find_cbs_shaper(struct sja1105_private *priv,
				   int port, int prio)
{
	int i;

	if (priv->info->fixed_cbs_mapping) {
		i = SJA1110_FIXED_CBS(port, prio);
		if (i >= 0 && i < priv->info->num_cbs_shapers)
			return i;

		return -1;
	}

	for (i = 0; i < priv->info->num_cbs_shapers; i++)
		if (priv->cbs[i].port == port && priv->cbs[i].prio == prio)
			return i;

	return -1;
}

static int sja1105_find_unused_cbs_shaper(struct sja1105_private *priv)
{
	int i;

	if (priv->info->fixed_cbs_mapping)
		return -1;

	for (i = 0; i < priv->info->num_cbs_shapers; i++)
		if (!priv->cbs[i].idle_slope && !priv->cbs[i].send_slope)
			return i;

	return -1;
}

static int sja1105_delete_cbs_shaper(struct sja1105_private *priv, int port,
				     int prio)
{
	int i;

	for (i = 0; i < priv->info->num_cbs_shapers; i++) {
		struct sja1105_cbs_entry *cbs = &priv->cbs[i];

		if (cbs->port == port && cbs->prio == prio) {
			memset(cbs, 0, sizeof(*cbs));
			return sja1105_dynamic_config_write(priv, BLK_IDX_CBS,
							    i, cbs, true);
		}
	}

	return 0;
}

static int sja1105_setup_tc_cbs(struct dsa_switch *ds, int port,
				struct tc_cbs_qopt_offload *offload)
{
	struct sja1105_private *priv = ds->priv;
	struct sja1105_cbs_entry *cbs;
	s64 port_transmit_rate_kbps;
	int index;

	if (!offload->enable)
		return sja1105_delete_cbs_shaper(priv, port, offload->queue);

	 
	index = sja1105_find_cbs_shaper(priv, port, offload->queue);
	if (index < 0) {
		 
		index = sja1105_find_unused_cbs_shaper(priv);
		if (index < 0)
			return -ENOSPC;
	}

	cbs = &priv->cbs[index];
	cbs->port = port;
	cbs->prio = offload->queue;
	 
	cbs->credit_hi = offload->hicredit;
	cbs->credit_lo = abs(offload->locredit);
	 
	port_transmit_rate_kbps = offload->idleslope - offload->sendslope;
	cbs->idle_slope = div_s64(offload->idleslope * BYTES_PER_KBIT,
				  port_transmit_rate_kbps);
	cbs->send_slope = div_s64(abs(offload->sendslope * BYTES_PER_KBIT),
				  port_transmit_rate_kbps);
	 
	cbs->credit_lo &= GENMASK_ULL(31, 0);
	cbs->send_slope &= GENMASK_ULL(31, 0);

	return sja1105_dynamic_config_write(priv, BLK_IDX_CBS, index, cbs,
					    true);
}

static int sja1105_reload_cbs(struct sja1105_private *priv)
{
	int rc = 0, i;

	 
	if (!priv->cbs)
		return 0;

	for (i = 0; i < priv->info->num_cbs_shapers; i++) {
		struct sja1105_cbs_entry *cbs = &priv->cbs[i];

		if (!cbs->idle_slope && !cbs->send_slope)
			continue;

		rc = sja1105_dynamic_config_write(priv, BLK_IDX_CBS, i, cbs,
						  true);
		if (rc)
			break;
	}

	return rc;
}

static const char * const sja1105_reset_reasons[] = {
	[SJA1105_VLAN_FILTERING] = "VLAN filtering",
	[SJA1105_AGEING_TIME] = "Ageing time",
	[SJA1105_SCHEDULING] = "Time-aware scheduling",
	[SJA1105_BEST_EFFORT_POLICING] = "Best-effort policing",
	[SJA1105_VIRTUAL_LINKS] = "Virtual links",
};

 
int sja1105_static_config_reload(struct sja1105_private *priv,
				 enum sja1105_reset_reason reason)
{
	struct ptp_system_timestamp ptp_sts_before;
	struct ptp_system_timestamp ptp_sts_after;
	int speed_mbps[SJA1105_MAX_NUM_PORTS];
	u16 bmcr[SJA1105_MAX_NUM_PORTS] = {0};
	struct sja1105_mac_config_entry *mac;
	struct dsa_switch *ds = priv->ds;
	s64 t1, t2, t3, t4;
	s64 t12, t34;
	int rc, i;
	s64 now;

	mutex_lock(&priv->fdb_lock);
	mutex_lock(&priv->mgmt_lock);

	mac = priv->static_config.tables[BLK_IDX_MAC_CONFIG].entries;

	 
	for (i = 0; i < ds->num_ports; i++) {
		speed_mbps[i] = sja1105_port_speed_to_ethtool(priv,
							      mac[i].speed);
		mac[i].speed = priv->info->port_speed[SJA1105_SPEED_AUTO];

		if (priv->xpcs[i])
			bmcr[i] = mdiobus_c45_read(priv->mdio_pcs, i,
						   MDIO_MMD_VEND2, MDIO_CTRL1);
	}

	 
	mutex_lock(&priv->ptp_data.lock);

	rc = __sja1105_ptp_gettimex(ds, &now, &ptp_sts_before);
	if (rc < 0) {
		mutex_unlock(&priv->ptp_data.lock);
		goto out;
	}

	 
	rc = sja1105_static_config_upload(priv);
	if (rc < 0) {
		mutex_unlock(&priv->ptp_data.lock);
		goto out;
	}

	rc = __sja1105_ptp_settime(ds, 0, &ptp_sts_after);
	if (rc < 0) {
		mutex_unlock(&priv->ptp_data.lock);
		goto out;
	}

	t1 = timespec64_to_ns(&ptp_sts_before.pre_ts);
	t2 = timespec64_to_ns(&ptp_sts_before.post_ts);
	t3 = timespec64_to_ns(&ptp_sts_after.pre_ts);
	t4 = timespec64_to_ns(&ptp_sts_after.post_ts);
	 
	t12 = t1 + (t2 - t1) / 2;
	 
	t34 = t3 + (t4 - t3) / 2;
	 
	now += (t34 - t12);

	__sja1105_ptp_adjtime(ds, now);

	mutex_unlock(&priv->ptp_data.lock);

	dev_info(priv->ds->dev,
		 "Reset switch and programmed static config. Reason: %s\n",
		 sja1105_reset_reasons[reason]);

	 
	if (priv->info->clocking_setup) {
		rc = priv->info->clocking_setup(priv);
		if (rc < 0)
			goto out;
	}

	for (i = 0; i < ds->num_ports; i++) {
		struct dw_xpcs *xpcs = priv->xpcs[i];
		unsigned int neg_mode;

		rc = sja1105_adjust_port_config(priv, i, speed_mbps[i]);
		if (rc < 0)
			goto out;

		if (!xpcs)
			continue;

		if (bmcr[i] & BMCR_ANENABLE)
			neg_mode = PHYLINK_PCS_NEG_INBAND_ENABLED;
		else
			neg_mode = PHYLINK_PCS_NEG_OUTBAND;

		rc = xpcs_do_config(xpcs, priv->phy_mode[i], NULL, neg_mode);
		if (rc < 0)
			goto out;

		if (neg_mode == PHYLINK_PCS_NEG_OUTBAND) {
			int speed = SPEED_UNKNOWN;

			if (priv->phy_mode[i] == PHY_INTERFACE_MODE_2500BASEX)
				speed = SPEED_2500;
			else if (bmcr[i] & BMCR_SPEED1000)
				speed = SPEED_1000;
			else if (bmcr[i] & BMCR_SPEED100)
				speed = SPEED_100;
			else
				speed = SPEED_10;

			xpcs_link_up(&xpcs->pcs, neg_mode, priv->phy_mode[i],
				     speed, DUPLEX_FULL);
		}
	}

	rc = sja1105_reload_cbs(priv);
	if (rc < 0)
		goto out;
out:
	mutex_unlock(&priv->mgmt_lock);
	mutex_unlock(&priv->fdb_lock);

	return rc;
}

static enum dsa_tag_protocol
sja1105_get_tag_protocol(struct dsa_switch *ds, int port,
			 enum dsa_tag_protocol mp)
{
	struct sja1105_private *priv = ds->priv;

	return priv->info->tag_proto;
}

 
int sja1105_vlan_filtering(struct dsa_switch *ds, int port, bool enabled,
			   struct netlink_ext_ack *extack)
{
	struct sja1105_general_params_entry *general_params;
	struct sja1105_private *priv = ds->priv;
	struct sja1105_table *table;
	struct sja1105_rule *rule;
	u16 tpid, tpid2;
	int rc;

	list_for_each_entry(rule, &priv->flow_block.rules, list) {
		if (rule->type == SJA1105_RULE_VL) {
			NL_SET_ERR_MSG_MOD(extack,
					   "Cannot change VLAN filtering with active VL rules");
			return -EBUSY;
		}
	}

	if (enabled) {
		 
		tpid  = ETH_P_8021Q;
		tpid2 = ETH_P_8021AD;
	} else {
		 
		tpid  = ETH_P_SJA1105;
		tpid2 = ETH_P_SJA1105;
	}

	table = &priv->static_config.tables[BLK_IDX_GENERAL_PARAMS];
	general_params = table->entries;
	 
	general_params->tpid = tpid;
	 
	general_params->tpid2 = tpid2;

	for (port = 0; port < ds->num_ports; port++) {
		if (dsa_is_unused_port(ds, port))
			continue;

		rc = sja1105_commit_pvid(ds, port);
		if (rc)
			return rc;
	}

	rc = sja1105_static_config_reload(priv, SJA1105_VLAN_FILTERING);
	if (rc)
		NL_SET_ERR_MSG_MOD(extack, "Failed to change VLAN Ethertype");

	return rc;
}

static int sja1105_vlan_add(struct sja1105_private *priv, int port, u16 vid,
			    u16 flags, bool allowed_ingress)
{
	struct sja1105_vlan_lookup_entry *vlan;
	struct sja1105_table *table;
	int match, rc;

	table = &priv->static_config.tables[BLK_IDX_VLAN_LOOKUP];

	match = sja1105_is_vlan_configured(priv, vid);
	if (match < 0) {
		rc = sja1105_table_resize(table, table->entry_count + 1);
		if (rc)
			return rc;
		match = table->entry_count - 1;
	}

	 
	vlan = table->entries;

	vlan[match].type_entry = SJA1110_VLAN_D_TAG;
	vlan[match].vlanid = vid;
	vlan[match].vlan_bc |= BIT(port);

	if (allowed_ingress)
		vlan[match].vmemb_port |= BIT(port);
	else
		vlan[match].vmemb_port &= ~BIT(port);

	if (flags & BRIDGE_VLAN_INFO_UNTAGGED)
		vlan[match].tag_port &= ~BIT(port);
	else
		vlan[match].tag_port |= BIT(port);

	return sja1105_dynamic_config_write(priv, BLK_IDX_VLAN_LOOKUP, vid,
					    &vlan[match], true);
}

static int sja1105_vlan_del(struct sja1105_private *priv, int port, u16 vid)
{
	struct sja1105_vlan_lookup_entry *vlan;
	struct sja1105_table *table;
	bool keep = true;
	int match, rc;

	table = &priv->static_config.tables[BLK_IDX_VLAN_LOOKUP];

	match = sja1105_is_vlan_configured(priv, vid);
	 
	if (match < 0)
		return 0;

	 
	vlan = table->entries;

	vlan[match].vlanid = vid;
	vlan[match].vlan_bc &= ~BIT(port);
	vlan[match].vmemb_port &= ~BIT(port);
	 
	vlan[match].tag_port &= ~BIT(port);

	 
	if (!vlan[match].vmemb_port)
		keep = false;

	rc = sja1105_dynamic_config_write(priv, BLK_IDX_VLAN_LOOKUP, vid,
					  &vlan[match], keep);
	if (rc < 0)
		return rc;

	if (!keep)
		return sja1105_table_delete_entry(table, match);

	return 0;
}

static int sja1105_bridge_vlan_add(struct dsa_switch *ds, int port,
				   const struct switchdev_obj_port_vlan *vlan,
				   struct netlink_ext_ack *extack)
{
	struct sja1105_private *priv = ds->priv;
	u16 flags = vlan->flags;
	int rc;

	 
	if (vid_is_dsa_8021q(vlan->vid)) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Range 3072-4095 reserved for dsa_8021q operation");
		return -EBUSY;
	}

	 
	if (dsa_is_cpu_port(ds, port) || dsa_is_dsa_port(ds, port))
		flags = 0;

	rc = sja1105_vlan_add(priv, port, vlan->vid, flags, true);
	if (rc)
		return rc;

	if (vlan->flags & BRIDGE_VLAN_INFO_PVID)
		priv->bridge_pvid[port] = vlan->vid;

	return sja1105_commit_pvid(ds, port);
}

static int sja1105_bridge_vlan_del(struct dsa_switch *ds, int port,
				   const struct switchdev_obj_port_vlan *vlan)
{
	struct sja1105_private *priv = ds->priv;
	int rc;

	rc = sja1105_vlan_del(priv, port, vlan->vid);
	if (rc)
		return rc;

	 
	return sja1105_commit_pvid(ds, port);
}

static int sja1105_dsa_8021q_vlan_add(struct dsa_switch *ds, int port, u16 vid,
				      u16 flags)
{
	struct sja1105_private *priv = ds->priv;
	bool allowed_ingress = true;
	int rc;

	 
	if (dsa_is_user_port(ds, port))
		allowed_ingress = false;

	rc = sja1105_vlan_add(priv, port, vid, flags, allowed_ingress);
	if (rc)
		return rc;

	if (flags & BRIDGE_VLAN_INFO_PVID)
		priv->tag_8021q_pvid[port] = vid;

	return sja1105_commit_pvid(ds, port);
}

static int sja1105_dsa_8021q_vlan_del(struct dsa_switch *ds, int port, u16 vid)
{
	struct sja1105_private *priv = ds->priv;

	return sja1105_vlan_del(priv, port, vid);
}

static int sja1105_prechangeupper(struct dsa_switch *ds, int port,
				  struct netdev_notifier_changeupper_info *info)
{
	struct netlink_ext_ack *extack = info->info.extack;
	struct net_device *upper = info->upper_dev;
	struct dsa_switch_tree *dst = ds->dst;
	struct dsa_port *dp;

	if (is_vlan_dev(upper)) {
		NL_SET_ERR_MSG_MOD(extack, "8021q uppers are not supported");
		return -EBUSY;
	}

	if (netif_is_bridge_master(upper)) {
		list_for_each_entry(dp, &dst->ports, list) {
			struct net_device *br = dsa_port_bridge_dev_get(dp);

			if (br && br != upper && br_vlan_enabled(br)) {
				NL_SET_ERR_MSG_MOD(extack,
						   "Only one VLAN-aware bridge is supported");
				return -EBUSY;
			}
		}
	}

	return 0;
}

static int sja1105_mgmt_xmit(struct dsa_switch *ds, int port, int slot,
			     struct sk_buff *skb, bool takets)
{
	struct sja1105_mgmt_entry mgmt_route = {0};
	struct sja1105_private *priv = ds->priv;
	struct ethhdr *hdr;
	int timeout = 10;
	int rc;

	hdr = eth_hdr(skb);

	mgmt_route.macaddr = ether_addr_to_u64(hdr->h_dest);
	mgmt_route.destports = BIT(port);
	mgmt_route.enfport = 1;
	mgmt_route.tsreg = 0;
	mgmt_route.takets = takets;

	rc = sja1105_dynamic_config_write(priv, BLK_IDX_MGMT_ROUTE,
					  slot, &mgmt_route, true);
	if (rc < 0) {
		kfree_skb(skb);
		return rc;
	}

	 
	dsa_enqueue_skb(skb, dsa_to_port(ds, port)->slave);

	 
	do {
		rc = sja1105_dynamic_config_read(priv, BLK_IDX_MGMT_ROUTE,
						 slot, &mgmt_route);
		if (rc < 0) {
			dev_err_ratelimited(priv->ds->dev,
					    "failed to poll for mgmt route\n");
			continue;
		}

		 
		cpu_relax();
	} while (mgmt_route.enfport && --timeout);

	if (!timeout) {
		 
		sja1105_dynamic_config_write(priv, BLK_IDX_MGMT_ROUTE,
					     slot, &mgmt_route, false);
		dev_err_ratelimited(priv->ds->dev, "xmit timed out\n");
	}

	return NETDEV_TX_OK;
}

#define work_to_xmit_work(w) \
		container_of((w), struct sja1105_deferred_xmit_work, work)

 
static void sja1105_port_deferred_xmit(struct kthread_work *work)
{
	struct sja1105_deferred_xmit_work *xmit_work = work_to_xmit_work(work);
	struct sk_buff *clone, *skb = xmit_work->skb;
	struct dsa_switch *ds = xmit_work->dp->ds;
	struct sja1105_private *priv = ds->priv;
	int port = xmit_work->dp->index;

	clone = SJA1105_SKB_CB(skb)->clone;

	mutex_lock(&priv->mgmt_lock);

	sja1105_mgmt_xmit(ds, port, 0, skb, !!clone);

	 
	if (clone)
		sja1105_ptp_txtstamp_skb(ds, port, clone);

	mutex_unlock(&priv->mgmt_lock);

	kfree(xmit_work);
}

static int sja1105_connect_tag_protocol(struct dsa_switch *ds,
					enum dsa_tag_protocol proto)
{
	struct sja1105_private *priv = ds->priv;
	struct sja1105_tagger_data *tagger_data;

	if (proto != priv->info->tag_proto)
		return -EPROTONOSUPPORT;

	tagger_data = sja1105_tagger_data(ds);
	tagger_data->xmit_work_fn = sja1105_port_deferred_xmit;
	tagger_data->meta_tstamp_handler = sja1110_process_meta_tstamp;

	return 0;
}

 
static int sja1105_set_ageing_time(struct dsa_switch *ds,
				   unsigned int ageing_time)
{
	struct sja1105_l2_lookup_params_entry *l2_lookup_params;
	struct sja1105_private *priv = ds->priv;
	struct sja1105_table *table;
	unsigned int maxage;

	table = &priv->static_config.tables[BLK_IDX_L2_LOOKUP_PARAMS];
	l2_lookup_params = table->entries;

	maxage = SJA1105_AGEING_TIME_MS(ageing_time);

	if (l2_lookup_params->maxage == maxage)
		return 0;

	l2_lookup_params->maxage = maxage;

	return sja1105_static_config_reload(priv, SJA1105_AGEING_TIME);
}

static int sja1105_change_mtu(struct dsa_switch *ds, int port, int new_mtu)
{
	struct sja1105_l2_policing_entry *policing;
	struct sja1105_private *priv = ds->priv;

	new_mtu += VLAN_ETH_HLEN + ETH_FCS_LEN;

	if (dsa_is_cpu_port(ds, port) || dsa_is_dsa_port(ds, port))
		new_mtu += VLAN_HLEN;

	policing = priv->static_config.tables[BLK_IDX_L2_POLICING].entries;

	if (policing[port].maxlen == new_mtu)
		return 0;

	policing[port].maxlen = new_mtu;

	return sja1105_static_config_reload(priv, SJA1105_BEST_EFFORT_POLICING);
}

static int sja1105_get_max_mtu(struct dsa_switch *ds, int port)
{
	return 2043 - VLAN_ETH_HLEN - ETH_FCS_LEN;
}

static int sja1105_port_setup_tc(struct dsa_switch *ds, int port,
				 enum tc_setup_type type,
				 void *type_data)
{
	switch (type) {
	case TC_SETUP_QDISC_TAPRIO:
		return sja1105_setup_tc_taprio(ds, port, type_data);
	case TC_SETUP_QDISC_CBS:
		return sja1105_setup_tc_cbs(ds, port, type_data);
	default:
		return -EOPNOTSUPP;
	}
}

 
static int sja1105_mirror_apply(struct sja1105_private *priv, int from, int to,
				bool ingress, bool enabled)
{
	struct sja1105_general_params_entry *general_params;
	struct sja1105_mac_config_entry *mac;
	struct dsa_switch *ds = priv->ds;
	struct sja1105_table *table;
	bool already_enabled;
	u64 new_mirr_port;
	int rc;

	table = &priv->static_config.tables[BLK_IDX_GENERAL_PARAMS];
	general_params = table->entries;

	mac = priv->static_config.tables[BLK_IDX_MAC_CONFIG].entries;

	already_enabled = (general_params->mirr_port != ds->num_ports);
	if (already_enabled && enabled && general_params->mirr_port != to) {
		dev_err(priv->ds->dev,
			"Delete mirroring rules towards port %llu first\n",
			general_params->mirr_port);
		return -EBUSY;
	}

	new_mirr_port = to;
	if (!enabled) {
		bool keep = false;
		int port;

		 
		for (port = 0; port < ds->num_ports; port++) {
			if (mac[port].ing_mirr || mac[port].egr_mirr) {
				keep = true;
				break;
			}
		}
		 
		if (!keep)
			new_mirr_port = ds->num_ports;
	}
	if (new_mirr_port != general_params->mirr_port) {
		general_params->mirr_port = new_mirr_port;

		rc = sja1105_dynamic_config_write(priv, BLK_IDX_GENERAL_PARAMS,
						  0, general_params, true);
		if (rc < 0)
			return rc;
	}

	if (ingress)
		mac[from].ing_mirr = enabled;
	else
		mac[from].egr_mirr = enabled;

	return sja1105_dynamic_config_write(priv, BLK_IDX_MAC_CONFIG, from,
					    &mac[from], true);
}

static int sja1105_mirror_add(struct dsa_switch *ds, int port,
			      struct dsa_mall_mirror_tc_entry *mirror,
			      bool ingress, struct netlink_ext_ack *extack)
{
	return sja1105_mirror_apply(ds->priv, port, mirror->to_local_port,
				    ingress, true);
}

static void sja1105_mirror_del(struct dsa_switch *ds, int port,
			       struct dsa_mall_mirror_tc_entry *mirror)
{
	sja1105_mirror_apply(ds->priv, port, mirror->to_local_port,
			     mirror->ingress, false);
}

static int sja1105_port_policer_add(struct dsa_switch *ds, int port,
				    struct dsa_mall_policer_tc_entry *policer)
{
	struct sja1105_l2_policing_entry *policing;
	struct sja1105_private *priv = ds->priv;

	policing = priv->static_config.tables[BLK_IDX_L2_POLICING].entries;

	 
	policing[port].rate = div_u64(512 * policer->rate_bytes_per_sec,
				      1000000);
	policing[port].smax = policer->burst;

	return sja1105_static_config_reload(priv, SJA1105_BEST_EFFORT_POLICING);
}

static void sja1105_port_policer_del(struct dsa_switch *ds, int port)
{
	struct sja1105_l2_policing_entry *policing;
	struct sja1105_private *priv = ds->priv;

	policing = priv->static_config.tables[BLK_IDX_L2_POLICING].entries;

	policing[port].rate = SJA1105_RATE_MBPS(1000);
	policing[port].smax = 65535;

	sja1105_static_config_reload(priv, SJA1105_BEST_EFFORT_POLICING);
}

static int sja1105_port_set_learning(struct sja1105_private *priv, int port,
				     bool enabled)
{
	struct sja1105_mac_config_entry *mac;

	mac = priv->static_config.tables[BLK_IDX_MAC_CONFIG].entries;

	mac[port].dyn_learn = enabled;

	return sja1105_dynamic_config_write(priv, BLK_IDX_MAC_CONFIG, port,
					    &mac[port], true);
}

static int sja1105_port_ucast_bcast_flood(struct sja1105_private *priv, int to,
					  struct switchdev_brport_flags flags)
{
	if (flags.mask & BR_FLOOD) {
		if (flags.val & BR_FLOOD)
			priv->ucast_egress_floods |= BIT(to);
		else
			priv->ucast_egress_floods &= ~BIT(to);
	}

	if (flags.mask & BR_BCAST_FLOOD) {
		if (flags.val & BR_BCAST_FLOOD)
			priv->bcast_egress_floods |= BIT(to);
		else
			priv->bcast_egress_floods &= ~BIT(to);
	}

	return sja1105_manage_flood_domains(priv);
}

static int sja1105_port_mcast_flood(struct sja1105_private *priv, int to,
				    struct switchdev_brport_flags flags,
				    struct netlink_ext_ack *extack)
{
	struct sja1105_l2_lookup_entry *l2_lookup;
	struct sja1105_table *table;
	int match, rc;

	mutex_lock(&priv->fdb_lock);

	table = &priv->static_config.tables[BLK_IDX_L2_LOOKUP];
	l2_lookup = table->entries;

	for (match = 0; match < table->entry_count; match++)
		if (l2_lookup[match].macaddr == SJA1105_UNKNOWN_MULTICAST &&
		    l2_lookup[match].mask_macaddr == SJA1105_UNKNOWN_MULTICAST)
			break;

	if (match == table->entry_count) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Could not find FDB entry for unknown multicast");
		rc = -ENOSPC;
		goto out;
	}

	if (flags.val & BR_MCAST_FLOOD)
		l2_lookup[match].destports |= BIT(to);
	else
		l2_lookup[match].destports &= ~BIT(to);

	rc = sja1105_dynamic_config_write(priv, BLK_IDX_L2_LOOKUP,
					  l2_lookup[match].index,
					  &l2_lookup[match], true);
out:
	mutex_unlock(&priv->fdb_lock);

	return rc;
}

static int sja1105_port_pre_bridge_flags(struct dsa_switch *ds, int port,
					 struct switchdev_brport_flags flags,
					 struct netlink_ext_ack *extack)
{
	struct sja1105_private *priv = ds->priv;

	if (flags.mask & ~(BR_LEARNING | BR_FLOOD | BR_MCAST_FLOOD |
			   BR_BCAST_FLOOD))
		return -EINVAL;

	if (flags.mask & (BR_FLOOD | BR_MCAST_FLOOD) &&
	    !priv->info->can_limit_mcast_flood) {
		bool multicast = !!(flags.val & BR_MCAST_FLOOD);
		bool unicast = !!(flags.val & BR_FLOOD);

		if (unicast != multicast) {
			NL_SET_ERR_MSG_MOD(extack,
					   "This chip cannot configure multicast flooding independently of unicast");
			return -EINVAL;
		}
	}

	return 0;
}

static int sja1105_port_bridge_flags(struct dsa_switch *ds, int port,
				     struct switchdev_brport_flags flags,
				     struct netlink_ext_ack *extack)
{
	struct sja1105_private *priv = ds->priv;
	int rc;

	if (flags.mask & BR_LEARNING) {
		bool learn_ena = !!(flags.val & BR_LEARNING);

		rc = sja1105_port_set_learning(priv, port, learn_ena);
		if (rc)
			return rc;
	}

	if (flags.mask & (BR_FLOOD | BR_BCAST_FLOOD)) {
		rc = sja1105_port_ucast_bcast_flood(priv, port, flags);
		if (rc)
			return rc;
	}

	 
	if (flags.mask & BR_MCAST_FLOOD && priv->info->can_limit_mcast_flood) {
		rc = sja1105_port_mcast_flood(priv, port, flags,
					      extack);
		if (rc)
			return rc;
	}

	return 0;
}

 
static int sja1105_setup(struct dsa_switch *ds)
{
	struct sja1105_private *priv = ds->priv;
	int rc;

	if (priv->info->disable_microcontroller) {
		rc = priv->info->disable_microcontroller(priv);
		if (rc < 0) {
			dev_err(ds->dev,
				"Failed to disable microcontroller: %pe\n",
				ERR_PTR(rc));
			return rc;
		}
	}

	 
	rc = sja1105_static_config_load(priv);
	if (rc < 0) {
		dev_err(ds->dev, "Failed to load static config: %d\n", rc);
		return rc;
	}

	 
	if (priv->info->clocking_setup) {
		rc = priv->info->clocking_setup(priv);
		if (rc < 0) {
			dev_err(ds->dev,
				"Failed to configure MII clocking: %pe\n",
				ERR_PTR(rc));
			goto out_static_config_free;
		}
	}

	sja1105_tas_setup(ds);
	sja1105_flower_setup(ds);

	rc = sja1105_ptp_clock_register(ds);
	if (rc < 0) {
		dev_err(ds->dev, "Failed to register PTP clock: %d\n", rc);
		goto out_flower_teardown;
	}

	rc = sja1105_mdiobus_register(ds);
	if (rc < 0) {
		dev_err(ds->dev, "Failed to register MDIO bus: %pe\n",
			ERR_PTR(rc));
		goto out_ptp_clock_unregister;
	}

	rc = sja1105_devlink_setup(ds);
	if (rc < 0)
		goto out_mdiobus_unregister;

	rtnl_lock();
	rc = dsa_tag_8021q_register(ds, htons(ETH_P_8021Q));
	rtnl_unlock();
	if (rc)
		goto out_devlink_teardown;

	 
	ds->vlan_filtering_is_global = true;
	ds->untag_bridge_pvid = true;
	ds->fdb_isolation = true;
	 
	ds->max_num_bridges = 7;

	 
	ds->num_tx_queues = SJA1105_NUM_TC;

	ds->mtu_enforcement_ingress = true;
	ds->assisted_learning_on_cpu_port = true;

	return 0;

out_devlink_teardown:
	sja1105_devlink_teardown(ds);
out_mdiobus_unregister:
	sja1105_mdiobus_unregister(ds);
out_ptp_clock_unregister:
	sja1105_ptp_clock_unregister(ds);
out_flower_teardown:
	sja1105_flower_teardown(ds);
	sja1105_tas_teardown(ds);
out_static_config_free:
	sja1105_static_config_free(&priv->static_config);

	return rc;
}

static void sja1105_teardown(struct dsa_switch *ds)
{
	struct sja1105_private *priv = ds->priv;

	rtnl_lock();
	dsa_tag_8021q_unregister(ds);
	rtnl_unlock();

	sja1105_devlink_teardown(ds);
	sja1105_mdiobus_unregister(ds);
	sja1105_ptp_clock_unregister(ds);
	sja1105_flower_teardown(ds);
	sja1105_tas_teardown(ds);
	sja1105_static_config_free(&priv->static_config);
}

static const struct dsa_switch_ops sja1105_switch_ops = {
	.get_tag_protocol	= sja1105_get_tag_protocol,
	.connect_tag_protocol	= sja1105_connect_tag_protocol,
	.setup			= sja1105_setup,
	.teardown		= sja1105_teardown,
	.set_ageing_time	= sja1105_set_ageing_time,
	.port_change_mtu	= sja1105_change_mtu,
	.port_max_mtu		= sja1105_get_max_mtu,
	.phylink_get_caps	= sja1105_phylink_get_caps,
	.phylink_mac_select_pcs	= sja1105_mac_select_pcs,
	.phylink_mac_link_up	= sja1105_mac_link_up,
	.phylink_mac_link_down	= sja1105_mac_link_down,
	.get_strings		= sja1105_get_strings,
	.get_ethtool_stats	= sja1105_get_ethtool_stats,
	.get_sset_count		= sja1105_get_sset_count,
	.get_ts_info		= sja1105_get_ts_info,
	.port_fdb_dump		= sja1105_fdb_dump,
	.port_fdb_add		= sja1105_fdb_add,
	.port_fdb_del		= sja1105_fdb_del,
	.port_fast_age		= sja1105_fast_age,
	.port_bridge_join	= sja1105_bridge_join,
	.port_bridge_leave	= sja1105_bridge_leave,
	.port_pre_bridge_flags	= sja1105_port_pre_bridge_flags,
	.port_bridge_flags	= sja1105_port_bridge_flags,
	.port_stp_state_set	= sja1105_bridge_stp_state_set,
	.port_vlan_filtering	= sja1105_vlan_filtering,
	.port_vlan_add		= sja1105_bridge_vlan_add,
	.port_vlan_del		= sja1105_bridge_vlan_del,
	.port_mdb_add		= sja1105_mdb_add,
	.port_mdb_del		= sja1105_mdb_del,
	.port_hwtstamp_get	= sja1105_hwtstamp_get,
	.port_hwtstamp_set	= sja1105_hwtstamp_set,
	.port_rxtstamp		= sja1105_port_rxtstamp,
	.port_txtstamp		= sja1105_port_txtstamp,
	.port_setup_tc		= sja1105_port_setup_tc,
	.port_mirror_add	= sja1105_mirror_add,
	.port_mirror_del	= sja1105_mirror_del,
	.port_policer_add	= sja1105_port_policer_add,
	.port_policer_del	= sja1105_port_policer_del,
	.cls_flower_add		= sja1105_cls_flower_add,
	.cls_flower_del		= sja1105_cls_flower_del,
	.cls_flower_stats	= sja1105_cls_flower_stats,
	.devlink_info_get	= sja1105_devlink_info_get,
	.tag_8021q_vlan_add	= sja1105_dsa_8021q_vlan_add,
	.tag_8021q_vlan_del	= sja1105_dsa_8021q_vlan_del,
	.port_prechangeupper	= sja1105_prechangeupper,
};

static const struct of_device_id sja1105_dt_ids[];

static int sja1105_check_device_id(struct sja1105_private *priv)
{
	const struct sja1105_regs *regs = priv->info->regs;
	u8 prod_id[SJA1105_SIZE_DEVICE_ID] = {0};
	struct device *dev = &priv->spidev->dev;
	const struct of_device_id *match;
	u32 device_id;
	u64 part_no;
	int rc;

	rc = sja1105_xfer_u32(priv, SPI_READ, regs->device_id, &device_id,
			      NULL);
	if (rc < 0)
		return rc;

	rc = sja1105_xfer_buf(priv, SPI_READ, regs->prod_id, prod_id,
			      SJA1105_SIZE_DEVICE_ID);
	if (rc < 0)
		return rc;

	sja1105_unpack(prod_id, &part_no, 19, 4, SJA1105_SIZE_DEVICE_ID);

	for (match = sja1105_dt_ids; match->compatible[0]; match++) {
		const struct sja1105_info *info = match->data;

		 
		if (info->device_id != device_id || info->part_no != part_no)
			continue;

		 
		if (priv->info->device_id != device_id ||
		    priv->info->part_no != part_no) {
			dev_warn(dev, "Device tree specifies chip %s but found %s, please fix it!\n",
				 priv->info->name, info->name);
			 
			priv->info = info;
		}

		return 0;
	}

	dev_err(dev, "Unexpected {device ID, part number}: 0x%x 0x%llx\n",
		device_id, part_no);

	return -ENODEV;
}

static int sja1105_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct sja1105_private *priv;
	size_t max_xfer, max_msg;
	struct dsa_switch *ds;
	int rc;

	if (!dev->of_node) {
		dev_err(dev, "No DTS bindings for SJA1105 driver\n");
		return -EINVAL;
	}

	rc = sja1105_hw_reset(dev, 1, 1);
	if (rc)
		return rc;

	priv = devm_kzalloc(dev, sizeof(struct sja1105_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	 
	priv->spidev = spi;
	spi_set_drvdata(spi, priv);

	 
	spi->bits_per_word = 8;
	rc = spi_setup(spi);
	if (rc < 0) {
		dev_err(dev, "Could not init SPI\n");
		return rc;
	}

	 
	max_xfer = spi_max_transfer_size(spi);
	max_msg = spi_max_message_size(spi);

	 
	if (max_msg < SJA1105_SIZE_SPI_MSG_HEADER + 8) {
		dev_err(dev, "SPI master cannot send large enough buffers, aborting\n");
		return -EINVAL;
	}

	priv->max_xfer_len = SJA1105_SIZE_SPI_MSG_MAXLEN;
	if (priv->max_xfer_len > max_xfer)
		priv->max_xfer_len = max_xfer;
	if (priv->max_xfer_len > max_msg - SJA1105_SIZE_SPI_MSG_HEADER)
		priv->max_xfer_len = max_msg - SJA1105_SIZE_SPI_MSG_HEADER;

	priv->info = of_device_get_match_data(dev);

	 
	rc = sja1105_check_device_id(priv);
	if (rc < 0) {
		dev_err(dev, "Device ID check failed: %d\n", rc);
		return rc;
	}

	dev_info(dev, "Probed switch chip: %s\n", priv->info->name);

	ds = devm_kzalloc(dev, sizeof(*ds), GFP_KERNEL);
	if (!ds)
		return -ENOMEM;

	ds->dev = dev;
	ds->num_ports = priv->info->num_ports;
	ds->ops = &sja1105_switch_ops;
	ds->priv = priv;
	priv->ds = ds;

	mutex_init(&priv->ptp_data.lock);
	mutex_init(&priv->dynamic_config_lock);
	mutex_init(&priv->mgmt_lock);
	mutex_init(&priv->fdb_lock);
	spin_lock_init(&priv->ts_id_lock);

	rc = sja1105_parse_dt(priv);
	if (rc < 0) {
		dev_err(ds->dev, "Failed to parse DT: %d\n", rc);
		return rc;
	}

	if (IS_ENABLED(CONFIG_NET_SCH_CBS)) {
		priv->cbs = devm_kcalloc(dev, priv->info->num_cbs_shapers,
					 sizeof(struct sja1105_cbs_entry),
					 GFP_KERNEL);
		if (!priv->cbs)
			return -ENOMEM;
	}

	return dsa_register_switch(priv->ds);
}

static void sja1105_remove(struct spi_device *spi)
{
	struct sja1105_private *priv = spi_get_drvdata(spi);

	if (!priv)
		return;

	dsa_unregister_switch(priv->ds);
}

static void sja1105_shutdown(struct spi_device *spi)
{
	struct sja1105_private *priv = spi_get_drvdata(spi);

	if (!priv)
		return;

	dsa_switch_shutdown(priv->ds);

	spi_set_drvdata(spi, NULL);
}

static const struct of_device_id sja1105_dt_ids[] = {
	{ .compatible = "nxp,sja1105e", .data = &sja1105e_info },
	{ .compatible = "nxp,sja1105t", .data = &sja1105t_info },
	{ .compatible = "nxp,sja1105p", .data = &sja1105p_info },
	{ .compatible = "nxp,sja1105q", .data = &sja1105q_info },
	{ .compatible = "nxp,sja1105r", .data = &sja1105r_info },
	{ .compatible = "nxp,sja1105s", .data = &sja1105s_info },
	{ .compatible = "nxp,sja1110a", .data = &sja1110a_info },
	{ .compatible = "nxp,sja1110b", .data = &sja1110b_info },
	{ .compatible = "nxp,sja1110c", .data = &sja1110c_info },
	{ .compatible = "nxp,sja1110d", .data = &sja1110d_info },
	{   },
};
MODULE_DEVICE_TABLE(of, sja1105_dt_ids);

static const struct spi_device_id sja1105_spi_ids[] = {
	{ "sja1105e" },
	{ "sja1105t" },
	{ "sja1105p" },
	{ "sja1105q" },
	{ "sja1105r" },
	{ "sja1105s" },
	{ "sja1110a" },
	{ "sja1110b" },
	{ "sja1110c" },
	{ "sja1110d" },
	{ },
};
MODULE_DEVICE_TABLE(spi, sja1105_spi_ids);

static struct spi_driver sja1105_driver = {
	.driver = {
		.name  = "sja1105",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sja1105_dt_ids),
	},
	.id_table = sja1105_spi_ids,
	.probe  = sja1105_probe,
	.remove = sja1105_remove,
	.shutdown = sja1105_shutdown,
};

module_spi_driver(sja1105_driver);

MODULE_AUTHOR("Vladimir Oltean <olteanv@gmail.com>");
MODULE_AUTHOR("Georg Waibel <georg.waibel@sensor-technik.de>");
MODULE_DESCRIPTION("SJA1105 Driver");
MODULE_LICENSE("GPL v2");
