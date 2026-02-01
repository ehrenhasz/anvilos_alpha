
 
#include <linux/dsa/ocelot.h>
#include <linux/if_bridge.h>
#include <linux/iopoll.h>
#include <linux/phy/phy.h>
#include <net/pkt_sched.h>
#include <soc/mscc/ocelot_hsio.h>
#include <soc/mscc/ocelot_vcap.h>
#include "ocelot.h"
#include "ocelot_vcap.h"

#define TABLE_UPDATE_SLEEP_US	10
#define TABLE_UPDATE_TIMEOUT_US	100000
#define MEM_INIT_SLEEP_US	1000
#define MEM_INIT_TIMEOUT_US	100000

#define OCELOT_RSV_VLAN_RANGE_START 4000

struct ocelot_mact_entry {
	u8 mac[ETH_ALEN];
	u16 vid;
	enum macaccess_entry_type type;
};

 
static inline u32 ocelot_mact_read_macaccess(struct ocelot *ocelot)
{
	return ocelot_read(ocelot, ANA_TABLES_MACACCESS);
}

 
static inline int ocelot_mact_wait_for_completion(struct ocelot *ocelot)
{
	u32 val;

	return readx_poll_timeout(ocelot_mact_read_macaccess,
		ocelot, val,
		(val & ANA_TABLES_MACACCESS_MAC_TABLE_CMD_M) ==
		MACACCESS_CMD_IDLE,
		TABLE_UPDATE_SLEEP_US, TABLE_UPDATE_TIMEOUT_US);
}

 
static void ocelot_mact_select(struct ocelot *ocelot,
			       const unsigned char mac[ETH_ALEN],
			       unsigned int vid)
{
	u32 macl = 0, mach = 0;

	 
	mach |= vid    << 16;
	mach |= mac[0] << 8;
	mach |= mac[1] << 0;
	macl |= mac[2] << 24;
	macl |= mac[3] << 16;
	macl |= mac[4] << 8;
	macl |= mac[5] << 0;

	ocelot_write(ocelot, macl, ANA_TABLES_MACLDATA);
	ocelot_write(ocelot, mach, ANA_TABLES_MACHDATA);

}

static int __ocelot_mact_learn(struct ocelot *ocelot, int port,
			       const unsigned char mac[ETH_ALEN],
			       unsigned int vid, enum macaccess_entry_type type)
{
	u32 cmd = ANA_TABLES_MACACCESS_VALID |
		ANA_TABLES_MACACCESS_DEST_IDX(port) |
		ANA_TABLES_MACACCESS_ENTRYTYPE(type) |
		ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_LEARN);
	unsigned int mc_ports;
	int err;

	 
	if (type == ENTRYTYPE_MACv4)
		mc_ports = (mac[1] << 8) | mac[2];
	else if (type == ENTRYTYPE_MACv6)
		mc_ports = (mac[0] << 8) | mac[1];
	else
		mc_ports = 0;

	if (mc_ports & BIT(ocelot->num_phys_ports))
		cmd |= ANA_TABLES_MACACCESS_MAC_CPU_COPY;

	ocelot_mact_select(ocelot, mac, vid);

	 
	ocelot_write(ocelot, cmd, ANA_TABLES_MACACCESS);

	err = ocelot_mact_wait_for_completion(ocelot);

	return err;
}

int ocelot_mact_learn(struct ocelot *ocelot, int port,
		      const unsigned char mac[ETH_ALEN],
		      unsigned int vid, enum macaccess_entry_type type)
{
	int ret;

	mutex_lock(&ocelot->mact_lock);
	ret = __ocelot_mact_learn(ocelot, port, mac, vid, type);
	mutex_unlock(&ocelot->mact_lock);

	return ret;
}
EXPORT_SYMBOL(ocelot_mact_learn);

int ocelot_mact_forget(struct ocelot *ocelot,
		       const unsigned char mac[ETH_ALEN], unsigned int vid)
{
	int err;

	mutex_lock(&ocelot->mact_lock);

	ocelot_mact_select(ocelot, mac, vid);

	 
	ocelot_write(ocelot,
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_FORGET),
		     ANA_TABLES_MACACCESS);

	err = ocelot_mact_wait_for_completion(ocelot);

	mutex_unlock(&ocelot->mact_lock);

	return err;
}
EXPORT_SYMBOL(ocelot_mact_forget);

int ocelot_mact_lookup(struct ocelot *ocelot, int *dst_idx,
		       const unsigned char mac[ETH_ALEN],
		       unsigned int vid, enum macaccess_entry_type *type)
{
	int val;

	mutex_lock(&ocelot->mact_lock);

	ocelot_mact_select(ocelot, mac, vid);

	 
	ocelot_write(ocelot, ANA_TABLES_MACACCESS_VALID |
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_READ),
		     ANA_TABLES_MACACCESS);

	if (ocelot_mact_wait_for_completion(ocelot)) {
		mutex_unlock(&ocelot->mact_lock);
		return -ETIMEDOUT;
	}

	 
	val = ocelot_read(ocelot, ANA_TABLES_MACACCESS);

	mutex_unlock(&ocelot->mact_lock);

	if (!(val & ANA_TABLES_MACACCESS_VALID))
		return -ENOENT;

	*dst_idx = ANA_TABLES_MACACCESS_DEST_IDX_X(val);
	*type = ANA_TABLES_MACACCESS_ENTRYTYPE_X(val);

	return 0;
}
EXPORT_SYMBOL(ocelot_mact_lookup);

int ocelot_mact_learn_streamdata(struct ocelot *ocelot, int dst_idx,
				 const unsigned char mac[ETH_ALEN],
				 unsigned int vid,
				 enum macaccess_entry_type type,
				 int sfid, int ssid)
{
	int ret;

	mutex_lock(&ocelot->mact_lock);

	ocelot_write(ocelot,
		     (sfid < 0 ? 0 : ANA_TABLES_STREAMDATA_SFID_VALID) |
		     ANA_TABLES_STREAMDATA_SFID(sfid) |
		     (ssid < 0 ? 0 : ANA_TABLES_STREAMDATA_SSID_VALID) |
		     ANA_TABLES_STREAMDATA_SSID(ssid),
		     ANA_TABLES_STREAMDATA);

	ret = __ocelot_mact_learn(ocelot, dst_idx, mac, vid, type);

	mutex_unlock(&ocelot->mact_lock);

	return ret;
}
EXPORT_SYMBOL(ocelot_mact_learn_streamdata);

static void ocelot_mact_init(struct ocelot *ocelot)
{
	 
	ocelot_rmw(ocelot, 0,
		   ANA_AGENCTRL_LEARN_CPU_COPY | ANA_AGENCTRL_IGNORE_DMAC_FLAGS
		   | ANA_AGENCTRL_LEARN_FWD_KILL
		   | ANA_AGENCTRL_LEARN_IGNORE_VLAN,
		   ANA_AGENCTRL);

	 
	ocelot_write(ocelot, MACACCESS_CMD_INIT, ANA_TABLES_MACACCESS);
}

void ocelot_pll5_init(struct ocelot *ocelot)
{
	 
	regmap_write(ocelot->targets[HSIO], HSIO_PLL5G_CFG4,
		     HSIO_PLL5G_CFG4_IB_CTRL(0x7600) |
		     HSIO_PLL5G_CFG4_IB_BIAS_CTRL(0x8));
	regmap_write(ocelot->targets[HSIO], HSIO_PLL5G_CFG0,
		     HSIO_PLL5G_CFG0_CORE_CLK_DIV(0x11) |
		     HSIO_PLL5G_CFG0_CPU_CLK_DIV(2) |
		     HSIO_PLL5G_CFG0_ENA_BIAS |
		     HSIO_PLL5G_CFG0_ENA_VCO_BUF |
		     HSIO_PLL5G_CFG0_ENA_CP1 |
		     HSIO_PLL5G_CFG0_SELCPI(2) |
		     HSIO_PLL5G_CFG0_LOOP_BW_RES(0xe) |
		     HSIO_PLL5G_CFG0_SELBGV820(4) |
		     HSIO_PLL5G_CFG0_DIV4 |
		     HSIO_PLL5G_CFG0_ENA_CLKTREE |
		     HSIO_PLL5G_CFG0_ENA_LANE);
	regmap_write(ocelot->targets[HSIO], HSIO_PLL5G_CFG2,
		     HSIO_PLL5G_CFG2_EN_RESET_FRQ_DET |
		     HSIO_PLL5G_CFG2_EN_RESET_OVERRUN |
		     HSIO_PLL5G_CFG2_GAIN_TEST(0x8) |
		     HSIO_PLL5G_CFG2_ENA_AMPCTRL |
		     HSIO_PLL5G_CFG2_PWD_AMPCTRL_N |
		     HSIO_PLL5G_CFG2_AMPC_SEL(0x10));
}
EXPORT_SYMBOL(ocelot_pll5_init);

static void ocelot_vcap_enable(struct ocelot *ocelot, int port)
{
	ocelot_write_gix(ocelot, ANA_PORT_VCAP_S2_CFG_S2_ENA |
			 ANA_PORT_VCAP_S2_CFG_S2_IP6_CFG(0xa),
			 ANA_PORT_VCAP_S2_CFG, port);

	ocelot_write_gix(ocelot, ANA_PORT_VCAP_CFG_S1_ENA,
			 ANA_PORT_VCAP_CFG, port);

	ocelot_rmw_gix(ocelot, REW_PORT_CFG_ES0_EN,
		       REW_PORT_CFG_ES0_EN,
		       REW_PORT_CFG, port);
}

static int ocelot_single_vlan_aware_bridge(struct ocelot *ocelot,
					   struct netlink_ext_ack *extack)
{
	struct net_device *bridge = NULL;
	int port;

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		struct ocelot_port *ocelot_port = ocelot->ports[port];

		if (!ocelot_port || !ocelot_port->bridge ||
		    !br_vlan_enabled(ocelot_port->bridge))
			continue;

		if (!bridge) {
			bridge = ocelot_port->bridge;
			continue;
		}

		if (bridge == ocelot_port->bridge)
			continue;

		NL_SET_ERR_MSG_MOD(extack,
				   "Only one VLAN-aware bridge is supported");
		return -EBUSY;
	}

	return 0;
}

static inline u32 ocelot_vlant_read_vlanaccess(struct ocelot *ocelot)
{
	return ocelot_read(ocelot, ANA_TABLES_VLANACCESS);
}

static inline int ocelot_vlant_wait_for_completion(struct ocelot *ocelot)
{
	u32 val;

	return readx_poll_timeout(ocelot_vlant_read_vlanaccess,
		ocelot,
		val,
		(val & ANA_TABLES_VLANACCESS_VLAN_TBL_CMD_M) ==
		ANA_TABLES_VLANACCESS_CMD_IDLE,
		TABLE_UPDATE_SLEEP_US, TABLE_UPDATE_TIMEOUT_US);
}

static int ocelot_vlant_set_mask(struct ocelot *ocelot, u16 vid, u32 mask)
{
	 
	ocelot_write(ocelot, ANA_TABLES_VLANTIDX_V_INDEX(vid),
		     ANA_TABLES_VLANTIDX);
	 
	ocelot_write(ocelot, ANA_TABLES_VLANACCESS_VLAN_PORT_MASK(mask) |
			     ANA_TABLES_VLANACCESS_CMD_WRITE,
		     ANA_TABLES_VLANACCESS);

	return ocelot_vlant_wait_for_completion(ocelot);
}

static int ocelot_port_num_untagged_vlans(struct ocelot *ocelot, int port)
{
	struct ocelot_bridge_vlan *vlan;
	int num_untagged = 0;

	list_for_each_entry(vlan, &ocelot->vlans, list) {
		if (!(vlan->portmask & BIT(port)))
			continue;

		 
		if (vlan->vid >= OCELOT_RSV_VLAN_RANGE_START)
			continue;

		if (vlan->untagged & BIT(port))
			num_untagged++;
	}

	return num_untagged;
}

static int ocelot_port_num_tagged_vlans(struct ocelot *ocelot, int port)
{
	struct ocelot_bridge_vlan *vlan;
	int num_tagged = 0;

	list_for_each_entry(vlan, &ocelot->vlans, list) {
		if (!(vlan->portmask & BIT(port)))
			continue;

		if (!(vlan->untagged & BIT(port)))
			num_tagged++;
	}

	return num_tagged;
}

 
static bool ocelot_port_uses_native_vlan(struct ocelot *ocelot, int port)
{
	return ocelot_port_num_tagged_vlans(ocelot, port) &&
	       ocelot_port_num_untagged_vlans(ocelot, port) == 1;
}

static struct ocelot_bridge_vlan *
ocelot_port_find_native_vlan(struct ocelot *ocelot, int port)
{
	struct ocelot_bridge_vlan *vlan;

	list_for_each_entry(vlan, &ocelot->vlans, list)
		if (vlan->portmask & BIT(port) && vlan->untagged & BIT(port))
			return vlan;

	return NULL;
}

 
static void ocelot_port_manage_port_tag(struct ocelot *ocelot, int port)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	enum ocelot_port_tag_config tag_cfg;
	bool uses_native_vlan = false;

	if (ocelot_port->vlan_aware) {
		uses_native_vlan = ocelot_port_uses_native_vlan(ocelot, port);

		if (uses_native_vlan)
			tag_cfg = OCELOT_PORT_TAG_NATIVE;
		else if (ocelot_port_num_untagged_vlans(ocelot, port))
			tag_cfg = OCELOT_PORT_TAG_DISABLED;
		else
			tag_cfg = OCELOT_PORT_TAG_TRUNK;
	} else {
		tag_cfg = OCELOT_PORT_TAG_DISABLED;
	}

	ocelot_rmw_gix(ocelot, REW_TAG_CFG_TAG_CFG(tag_cfg),
		       REW_TAG_CFG_TAG_CFG_M,
		       REW_TAG_CFG, port);

	if (uses_native_vlan) {
		struct ocelot_bridge_vlan *native_vlan;

		 
		native_vlan = ocelot_port_find_native_vlan(ocelot, port);

		ocelot_rmw_gix(ocelot,
			       REW_PORT_VLAN_CFG_PORT_VID(native_vlan->vid),
			       REW_PORT_VLAN_CFG_PORT_VID_M,
			       REW_PORT_VLAN_CFG, port);
	}
}

int ocelot_bridge_num_find(struct ocelot *ocelot,
			   const struct net_device *bridge)
{
	int port;

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		struct ocelot_port *ocelot_port = ocelot->ports[port];

		if (ocelot_port && ocelot_port->bridge == bridge)
			return ocelot_port->bridge_num;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(ocelot_bridge_num_find);

static u16 ocelot_vlan_unaware_pvid(struct ocelot *ocelot,
				    const struct net_device *bridge)
{
	int bridge_num;

	 
	if (!bridge)
		return 0;

	bridge_num = ocelot_bridge_num_find(ocelot, bridge);
	if (WARN_ON(bridge_num < 0))
		return 0;

	 
	return VLAN_N_VID - bridge_num - 1;
}

 
static void ocelot_port_set_pvid(struct ocelot *ocelot, int port,
				 const struct ocelot_bridge_vlan *pvid_vlan)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	u16 pvid = ocelot_vlan_unaware_pvid(ocelot, ocelot_port->bridge);
	u32 val = 0;

	ocelot_port->pvid_vlan = pvid_vlan;

	if (ocelot_port->vlan_aware && pvid_vlan)
		pvid = pvid_vlan->vid;

	ocelot_rmw_gix(ocelot,
		       ANA_PORT_VLAN_CFG_VLAN_VID(pvid),
		       ANA_PORT_VLAN_CFG_VLAN_VID_M,
		       ANA_PORT_VLAN_CFG, port);

	 
	if (!pvid_vlan && ocelot_port->vlan_aware)
		val = ANA_PORT_DROP_CFG_DROP_PRIO_S_TAGGED_ENA |
		      ANA_PORT_DROP_CFG_DROP_PRIO_C_TAGGED_ENA;

	ocelot_rmw_gix(ocelot, val,
		       ANA_PORT_DROP_CFG_DROP_PRIO_S_TAGGED_ENA |
		       ANA_PORT_DROP_CFG_DROP_PRIO_C_TAGGED_ENA,
		       ANA_PORT_DROP_CFG, port);
}

static struct ocelot_bridge_vlan *ocelot_bridge_vlan_find(struct ocelot *ocelot,
							  u16 vid)
{
	struct ocelot_bridge_vlan *vlan;

	list_for_each_entry(vlan, &ocelot->vlans, list)
		if (vlan->vid == vid)
			return vlan;

	return NULL;
}

static int ocelot_vlan_member_add(struct ocelot *ocelot, int port, u16 vid,
				  bool untagged)
{
	struct ocelot_bridge_vlan *vlan = ocelot_bridge_vlan_find(ocelot, vid);
	unsigned long portmask;
	int err;

	if (vlan) {
		portmask = vlan->portmask | BIT(port);

		err = ocelot_vlant_set_mask(ocelot, vid, portmask);
		if (err)
			return err;

		vlan->portmask = portmask;
		 
		if (untagged)
			vlan->untagged |= BIT(port);
		else
			vlan->untagged &= ~BIT(port);

		return 0;
	}

	vlan = kzalloc(sizeof(*vlan), GFP_KERNEL);
	if (!vlan)
		return -ENOMEM;

	portmask = BIT(port);

	err = ocelot_vlant_set_mask(ocelot, vid, portmask);
	if (err) {
		kfree(vlan);
		return err;
	}

	vlan->vid = vid;
	vlan->portmask = portmask;
	if (untagged)
		vlan->untagged = BIT(port);
	INIT_LIST_HEAD(&vlan->list);
	list_add_tail(&vlan->list, &ocelot->vlans);

	return 0;
}

static int ocelot_vlan_member_del(struct ocelot *ocelot, int port, u16 vid)
{
	struct ocelot_bridge_vlan *vlan = ocelot_bridge_vlan_find(ocelot, vid);
	unsigned long portmask;
	int err;

	if (!vlan)
		return 0;

	portmask = vlan->portmask & ~BIT(port);

	err = ocelot_vlant_set_mask(ocelot, vid, portmask);
	if (err)
		return err;

	vlan->portmask = portmask;
	if (vlan->portmask)
		return 0;

	list_del(&vlan->list);
	kfree(vlan);

	return 0;
}

static int ocelot_add_vlan_unaware_pvid(struct ocelot *ocelot, int port,
					const struct net_device *bridge)
{
	u16 vid = ocelot_vlan_unaware_pvid(ocelot, bridge);

	return ocelot_vlan_member_add(ocelot, port, vid, true);
}

static int ocelot_del_vlan_unaware_pvid(struct ocelot *ocelot, int port,
					const struct net_device *bridge)
{
	u16 vid = ocelot_vlan_unaware_pvid(ocelot, bridge);

	return ocelot_vlan_member_del(ocelot, port, vid);
}

int ocelot_port_vlan_filtering(struct ocelot *ocelot, int port,
			       bool vlan_aware, struct netlink_ext_ack *extack)
{
	struct ocelot_vcap_block *block = &ocelot->block[VCAP_IS1];
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	struct ocelot_vcap_filter *filter;
	int err = 0;
	u32 val;

	list_for_each_entry(filter, &block->rules, list) {
		if (filter->ingress_port_mask & BIT(port) &&
		    filter->action.vid_replace_ena) {
			NL_SET_ERR_MSG_MOD(extack,
					   "Cannot change VLAN state with vlan modify rules active");
			return -EBUSY;
		}
	}

	err = ocelot_single_vlan_aware_bridge(ocelot, extack);
	if (err)
		return err;

	if (vlan_aware)
		err = ocelot_del_vlan_unaware_pvid(ocelot, port,
						   ocelot_port->bridge);
	else if (ocelot_port->bridge)
		err = ocelot_add_vlan_unaware_pvid(ocelot, port,
						   ocelot_port->bridge);
	if (err)
		return err;

	ocelot_port->vlan_aware = vlan_aware;

	if (vlan_aware)
		val = ANA_PORT_VLAN_CFG_VLAN_AWARE_ENA |
		      ANA_PORT_VLAN_CFG_VLAN_POP_CNT(1);
	else
		val = 0;
	ocelot_rmw_gix(ocelot, val,
		       ANA_PORT_VLAN_CFG_VLAN_AWARE_ENA |
		       ANA_PORT_VLAN_CFG_VLAN_POP_CNT_M,
		       ANA_PORT_VLAN_CFG, port);

	ocelot_port_set_pvid(ocelot, port, ocelot_port->pvid_vlan);
	ocelot_port_manage_port_tag(ocelot, port);

	return 0;
}
EXPORT_SYMBOL(ocelot_port_vlan_filtering);

int ocelot_vlan_prepare(struct ocelot *ocelot, int port, u16 vid, bool pvid,
			bool untagged, struct netlink_ext_ack *extack)
{
	if (untagged) {
		 
		if (ocelot_port_uses_native_vlan(ocelot, port)) {
			NL_SET_ERR_MSG_MOD(extack,
					   "Port with egress-tagged VLANs cannot have more than one egress-untagged (native) VLAN");
			return -EBUSY;
		}
	} else {
		 
		if (ocelot_port_num_untagged_vlans(ocelot, port) > 1) {
			NL_SET_ERR_MSG_MOD(extack,
					   "Port with more than one egress-untagged VLAN cannot have egress-tagged VLANs");
			return -EBUSY;
		}
	}

	if (vid > OCELOT_RSV_VLAN_RANGE_START) {
		NL_SET_ERR_MSG_MOD(extack,
				   "VLAN range 4000-4095 reserved for VLAN-unaware bridging");
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL(ocelot_vlan_prepare);

int ocelot_vlan_add(struct ocelot *ocelot, int port, u16 vid, bool pvid,
		    bool untagged)
{
	int err;

	 
	if (!vid)
		return 0;

	err = ocelot_vlan_member_add(ocelot, port, vid, untagged);
	if (err)
		return err;

	 
	if (pvid)
		ocelot_port_set_pvid(ocelot, port,
				     ocelot_bridge_vlan_find(ocelot, vid));

	 
	ocelot_port_manage_port_tag(ocelot, port);

	return 0;
}
EXPORT_SYMBOL(ocelot_vlan_add);

int ocelot_vlan_del(struct ocelot *ocelot, int port, u16 vid)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	bool del_pvid = false;
	int err;

	if (!vid)
		return 0;

	if (ocelot_port->pvid_vlan && ocelot_port->pvid_vlan->vid == vid)
		del_pvid = true;

	err = ocelot_vlan_member_del(ocelot, port, vid);
	if (err)
		return err;

	 
	if (del_pvid)
		ocelot_port_set_pvid(ocelot, port, NULL);

	 
	ocelot_port_manage_port_tag(ocelot, port);

	return 0;
}
EXPORT_SYMBOL(ocelot_vlan_del);

static void ocelot_vlan_init(struct ocelot *ocelot)
{
	unsigned long all_ports = GENMASK(ocelot->num_phys_ports - 1, 0);
	u16 port, vid;

	 
	ocelot_write(ocelot, ANA_TABLES_VLANACCESS_CMD_INIT,
		     ANA_TABLES_VLANACCESS);
	ocelot_vlant_wait_for_completion(ocelot);

	 
	for (vid = 1; vid < VLAN_N_VID; vid++)
		ocelot_vlant_set_mask(ocelot, vid, 0);

	 
	ocelot_vlant_set_mask(ocelot, OCELOT_STANDALONE_PVID, all_ports);

	 
	ocelot_write(ocelot, all_ports, ANA_VLANMASK);

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		ocelot_write_gix(ocelot, 0, REW_PORT_VLAN_CFG, port);
		ocelot_write_gix(ocelot, 0, REW_TAG_CFG, port);
	}
}

static u32 ocelot_read_eq_avail(struct ocelot *ocelot, int port)
{
	return ocelot_read_rix(ocelot, QSYS_SW_STATUS, port);
}

static int ocelot_port_flush(struct ocelot *ocelot, int port)
{
	unsigned int pause_ena;
	int err, val;

	 
	ocelot_rmw_rix(ocelot, QSYS_PORT_MODE_DEQUEUE_DIS,
		       QSYS_PORT_MODE_DEQUEUE_DIS,
		       QSYS_PORT_MODE, port);

	 
	ocelot_fields_read(ocelot, port, SYS_PAUSE_CFG_PAUSE_ENA, &pause_ena);
	ocelot_fields_write(ocelot, port, SYS_PAUSE_CFG_PAUSE_ENA, 0);

	 
	ocelot_fields_write(ocelot, port,
			    QSYS_SWITCH_PORT_MODE_TX_PFC_ENA, 0);

	 
	usleep_range(8000, 10000);

	 
	ocelot_rmw_rix(ocelot, 0, SYS_FRONT_PORT_MODE_HDX_MODE,
		       SYS_FRONT_PORT_MODE, port);

	 
	ocelot_rmw_gix(ocelot, REW_PORT_CFG_FLUSH_ENA, REW_PORT_CFG_FLUSH_ENA,
		       REW_PORT_CFG, port);

	 
	ocelot_rmw_rix(ocelot, 0, QSYS_PORT_MODE_DEQUEUE_DIS, QSYS_PORT_MODE,
		       port);

	 
	err = read_poll_timeout(ocelot_read_eq_avail, val, !val,
				100, 2000000, false, ocelot, port);

	 
	ocelot_rmw_gix(ocelot, 0, REW_PORT_CFG_FLUSH_ENA, REW_PORT_CFG, port);

	 
	ocelot_fields_write(ocelot, port, SYS_PAUSE_CFG_PAUSE_ENA, pause_ena);

	return err;
}

int ocelot_port_configure_serdes(struct ocelot *ocelot, int port,
				 struct device_node *portnp)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	struct device *dev = ocelot->dev;
	int err;

	 
	if (ocelot_port->phy_mode == PHY_INTERFACE_MODE_QSGMII)
		ocelot_port_rmwl(ocelot_port, 0,
				 DEV_CLOCK_CFG_MAC_TX_RST |
				 DEV_CLOCK_CFG_MAC_RX_RST,
				 DEV_CLOCK_CFG);

	if (ocelot_port->phy_mode != PHY_INTERFACE_MODE_INTERNAL) {
		struct phy *serdes = of_phy_get(portnp, NULL);

		if (IS_ERR(serdes)) {
			err = PTR_ERR(serdes);
			dev_err_probe(dev, err,
				      "missing SerDes phys for port %d\n",
				      port);
			return err;
		}

		err = phy_set_mode_ext(serdes, PHY_MODE_ETHERNET,
				       ocelot_port->phy_mode);
		of_phy_put(serdes);
		if (err) {
			dev_err(dev, "Could not SerDes mode on port %d: %pe\n",
				port, ERR_PTR(err));
			return err;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ocelot_port_configure_serdes);

void ocelot_phylink_mac_config(struct ocelot *ocelot, int port,
			       unsigned int link_an_mode,
			       const struct phylink_link_state *state)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];

	 
	ocelot_port_writel(ocelot_port, DEV_PORT_MISC_HDX_FAST_DIS,
			   DEV_PORT_MISC);

	 
	ocelot_port_writel(ocelot_port, PCS1G_MODE_CFG_SGMII_MODE_ENA,
			   PCS1G_MODE_CFG);
	ocelot_port_writel(ocelot_port, PCS1G_SD_CFG_SD_SEL, PCS1G_SD_CFG);

	 
	ocelot_port_writel(ocelot_port, PCS1G_CFG_PCS_ENA, PCS1G_CFG);

	 
	ocelot_port_writel(ocelot_port, 0, PCS1G_ANEG_CFG);

	 
	ocelot_port_writel(ocelot_port, 0, PCS1G_LB_CFG);
}
EXPORT_SYMBOL_GPL(ocelot_phylink_mac_config);

void ocelot_phylink_mac_link_down(struct ocelot *ocelot, int port,
				  unsigned int link_an_mode,
				  phy_interface_t interface,
				  unsigned long quirks)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	int err;

	ocelot_port->speed = SPEED_UNKNOWN;

	ocelot_port_rmwl(ocelot_port, 0, DEV_MAC_ENA_CFG_RX_ENA,
			 DEV_MAC_ENA_CFG);

	if (ocelot->ops->cut_through_fwd) {
		mutex_lock(&ocelot->fwd_domain_lock);
		ocelot->ops->cut_through_fwd(ocelot);
		mutex_unlock(&ocelot->fwd_domain_lock);
	}

	ocelot_fields_write(ocelot, port, QSYS_SWITCH_PORT_MODE_PORT_ENA, 0);

	err = ocelot_port_flush(ocelot, port);
	if (err)
		dev_err(ocelot->dev, "failed to flush port %d: %d\n",
			port, err);

	 
	if (interface != PHY_INTERFACE_MODE_QSGMII ||
	    !(quirks & OCELOT_QUIRK_QSGMII_PORTS_MUST_BE_UP))
		ocelot_port_rmwl(ocelot_port,
				 DEV_CLOCK_CFG_MAC_TX_RST |
				 DEV_CLOCK_CFG_MAC_RX_RST,
				 DEV_CLOCK_CFG_MAC_TX_RST |
				 DEV_CLOCK_CFG_MAC_RX_RST,
				 DEV_CLOCK_CFG);
}
EXPORT_SYMBOL_GPL(ocelot_phylink_mac_link_down);

void ocelot_phylink_mac_link_up(struct ocelot *ocelot, int port,
				struct phy_device *phydev,
				unsigned int link_an_mode,
				phy_interface_t interface,
				int speed, int duplex,
				bool tx_pause, bool rx_pause,
				unsigned long quirks)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	int mac_speed, mode = 0;
	u32 mac_fc_cfg;

	ocelot_port->speed = speed;

	 
	if ((quirks & OCELOT_QUIRK_PCS_PERFORMS_RATE_ADAPTATION) ||
	    speed == SPEED_1000) {
		mac_speed = OCELOT_SPEED_1000;
		mode = DEV_MAC_MODE_CFG_GIGA_MODE_ENA;
	} else if (speed == SPEED_2500) {
		mac_speed = OCELOT_SPEED_2500;
		mode = DEV_MAC_MODE_CFG_GIGA_MODE_ENA;
	} else if (speed == SPEED_100) {
		mac_speed = OCELOT_SPEED_100;
	} else {
		mac_speed = OCELOT_SPEED_10;
	}

	if (duplex == DUPLEX_FULL)
		mode |= DEV_MAC_MODE_CFG_FDX_ENA;

	ocelot_port_writel(ocelot_port, mode, DEV_MAC_MODE_CFG);

	 
	ocelot_port_writel(ocelot_port, DEV_CLOCK_CFG_LINK_SPEED(mac_speed),
			   DEV_CLOCK_CFG);

	switch (speed) {
	case SPEED_10:
		mac_fc_cfg = SYS_MAC_FC_CFG_FC_LINK_SPEED(OCELOT_SPEED_10);
		break;
	case SPEED_100:
		mac_fc_cfg = SYS_MAC_FC_CFG_FC_LINK_SPEED(OCELOT_SPEED_100);
		break;
	case SPEED_1000:
	case SPEED_2500:
		mac_fc_cfg = SYS_MAC_FC_CFG_FC_LINK_SPEED(OCELOT_SPEED_1000);
		break;
	default:
		dev_err(ocelot->dev, "Unsupported speed on port %d: %d\n",
			port, speed);
		return;
	}

	if (rx_pause)
		mac_fc_cfg |= SYS_MAC_FC_CFG_RX_FC_ENA;

	if (tx_pause)
		mac_fc_cfg |= SYS_MAC_FC_CFG_TX_FC_ENA |
			      SYS_MAC_FC_CFG_PAUSE_VAL_CFG(0xffff) |
			      SYS_MAC_FC_CFG_FC_LATENCY_CFG(0x7) |
			      SYS_MAC_FC_CFG_ZERO_PAUSE_ENA;

	 
	ocelot_write_rix(ocelot, mac_fc_cfg, SYS_MAC_FC_CFG, port);

	ocelot_write_rix(ocelot, 0, ANA_POL_FLOWC, port);

	 
	if (port != ocelot->npi)
		ocelot_fields_write(ocelot, port, SYS_PAUSE_CFG_PAUSE_ENA,
				    tx_pause);

	 
	ocelot_port_writel(ocelot_port, DEV_MAC_ENA_CFG_RX_ENA |
			   DEV_MAC_ENA_CFG_TX_ENA, DEV_MAC_ENA_CFG);

	 
	if (ocelot->ops->cut_through_fwd) {
		mutex_lock(&ocelot->fwd_domain_lock);
		 
		ocelot_port_update_active_preemptible_tcs(ocelot, port);
		mutex_unlock(&ocelot->fwd_domain_lock);
	}

	 
	ocelot_fields_write(ocelot, port,
			    QSYS_SWITCH_PORT_MODE_PORT_ENA, 1);
}
EXPORT_SYMBOL_GPL(ocelot_phylink_mac_link_up);

static int ocelot_rx_frame_word(struct ocelot *ocelot, u8 grp, bool ifh,
				u32 *rval)
{
	u32 bytes_valid, val;

	val = ocelot_read_rix(ocelot, QS_XTR_RD, grp);
	if (val == XTR_NOT_READY) {
		if (ifh)
			return -EIO;

		do {
			val = ocelot_read_rix(ocelot, QS_XTR_RD, grp);
		} while (val == XTR_NOT_READY);
	}

	switch (val) {
	case XTR_ABORT:
		return -EIO;
	case XTR_EOF_0:
	case XTR_EOF_1:
	case XTR_EOF_2:
	case XTR_EOF_3:
	case XTR_PRUNED:
		bytes_valid = XTR_VALID_BYTES(val);
		val = ocelot_read_rix(ocelot, QS_XTR_RD, grp);
		if (val == XTR_ESCAPE)
			*rval = ocelot_read_rix(ocelot, QS_XTR_RD, grp);
		else
			*rval = val;

		return bytes_valid;
	case XTR_ESCAPE:
		*rval = ocelot_read_rix(ocelot, QS_XTR_RD, grp);

		return 4;
	default:
		*rval = val;

		return 4;
	}
}

static int ocelot_xtr_poll_xfh(struct ocelot *ocelot, int grp, u32 *xfh)
{
	int i, err = 0;

	for (i = 0; i < OCELOT_TAG_LEN / 4; i++) {
		err = ocelot_rx_frame_word(ocelot, grp, true, &xfh[i]);
		if (err != 4)
			return (err < 0) ? err : -EIO;
	}

	return 0;
}

void ocelot_ptp_rx_timestamp(struct ocelot *ocelot, struct sk_buff *skb,
			     u64 timestamp)
{
	struct skb_shared_hwtstamps *shhwtstamps;
	u64 tod_in_ns, full_ts_in_ns;
	struct timespec64 ts;

	ocelot_ptp_gettime64(&ocelot->ptp_info, &ts);

	tod_in_ns = ktime_set(ts.tv_sec, ts.tv_nsec);
	if ((tod_in_ns & 0xffffffff) < timestamp)
		full_ts_in_ns = (((tod_in_ns >> 32) - 1) << 32) |
				timestamp;
	else
		full_ts_in_ns = (tod_in_ns & GENMASK_ULL(63, 32)) |
				timestamp;

	shhwtstamps = skb_hwtstamps(skb);
	memset(shhwtstamps, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamps->hwtstamp = full_ts_in_ns;
}
EXPORT_SYMBOL(ocelot_ptp_rx_timestamp);

int ocelot_xtr_poll_frame(struct ocelot *ocelot, int grp, struct sk_buff **nskb)
{
	u64 timestamp, src_port, len;
	u32 xfh[OCELOT_TAG_LEN / 4];
	struct net_device *dev;
	struct sk_buff *skb;
	int sz, buf_len;
	u32 val, *buf;
	int err;

	err = ocelot_xtr_poll_xfh(ocelot, grp, xfh);
	if (err)
		return err;

	ocelot_xfh_get_src_port(xfh, &src_port);
	ocelot_xfh_get_len(xfh, &len);
	ocelot_xfh_get_rew_val(xfh, &timestamp);

	if (WARN_ON(src_port >= ocelot->num_phys_ports))
		return -EINVAL;

	dev = ocelot->ops->port_to_netdev(ocelot, src_port);
	if (!dev)
		return -EINVAL;

	skb = netdev_alloc_skb(dev, len);
	if (unlikely(!skb)) {
		netdev_err(dev, "Unable to allocate sk_buff\n");
		return -ENOMEM;
	}

	buf_len = len - ETH_FCS_LEN;
	buf = (u32 *)skb_put(skb, buf_len);

	len = 0;
	do {
		sz = ocelot_rx_frame_word(ocelot, grp, false, &val);
		if (sz < 0) {
			err = sz;
			goto out_free_skb;
		}
		*buf++ = val;
		len += sz;
	} while (len < buf_len);

	 
	sz = ocelot_rx_frame_word(ocelot, grp, false, &val);
	if (sz < 0) {
		err = sz;
		goto out_free_skb;
	}

	 
	len -= ETH_FCS_LEN - sz;

	if (unlikely(dev->features & NETIF_F_RXFCS)) {
		buf = (u32 *)skb_put(skb, ETH_FCS_LEN);
		*buf = val;
	}

	if (ocelot->ptp)
		ocelot_ptp_rx_timestamp(ocelot, skb, timestamp);

	 
	if (ocelot->ports[src_port]->bridge)
		skb->offload_fwd_mark = 1;

	skb->protocol = eth_type_trans(skb, dev);

	*nskb = skb;

	return 0;

out_free_skb:
	kfree_skb(skb);
	return err;
}
EXPORT_SYMBOL(ocelot_xtr_poll_frame);

bool ocelot_can_inject(struct ocelot *ocelot, int grp)
{
	u32 val = ocelot_read(ocelot, QS_INJ_STATUS);

	if (!(val & QS_INJ_STATUS_FIFO_RDY(BIT(grp))))
		return false;
	if (val & QS_INJ_STATUS_WMARK_REACHED(BIT(grp)))
		return false;

	return true;
}
EXPORT_SYMBOL(ocelot_can_inject);

void ocelot_ifh_port_set(void *ifh, int port, u32 rew_op, u32 vlan_tag)
{
	ocelot_ifh_set_bypass(ifh, 1);
	ocelot_ifh_set_dest(ifh, BIT_ULL(port));
	ocelot_ifh_set_tag_type(ifh, IFH_TAG_TYPE_C);
	if (vlan_tag)
		ocelot_ifh_set_vlan_tci(ifh, vlan_tag);
	if (rew_op)
		ocelot_ifh_set_rew_op(ifh, rew_op);
}
EXPORT_SYMBOL(ocelot_ifh_port_set);

void ocelot_port_inject_frame(struct ocelot *ocelot, int port, int grp,
			      u32 rew_op, struct sk_buff *skb)
{
	u32 ifh[OCELOT_TAG_LEN / 4] = {0};
	unsigned int i, count, last;

	ocelot_write_rix(ocelot, QS_INJ_CTRL_GAP_SIZE(1) |
			 QS_INJ_CTRL_SOF, QS_INJ_CTRL, grp);

	ocelot_ifh_port_set(ifh, port, rew_op, skb_vlan_tag_get(skb));

	for (i = 0; i < OCELOT_TAG_LEN / 4; i++)
		ocelot_write_rix(ocelot, ifh[i], QS_INJ_WR, grp);

	count = DIV_ROUND_UP(skb->len, 4);
	last = skb->len % 4;
	for (i = 0; i < count; i++)
		ocelot_write_rix(ocelot, ((u32 *)skb->data)[i], QS_INJ_WR, grp);

	 
	while (i < (OCELOT_BUFFER_CELL_SZ / 4)) {
		ocelot_write_rix(ocelot, 0, QS_INJ_WR, grp);
		i++;
	}

	 
	ocelot_write_rix(ocelot, QS_INJ_CTRL_GAP_SIZE(1) |
			 QS_INJ_CTRL_VLD_BYTES(skb->len < OCELOT_BUFFER_CELL_SZ ? 0 : last) |
			 QS_INJ_CTRL_EOF,
			 QS_INJ_CTRL, grp);

	 
	ocelot_write_rix(ocelot, 0, QS_INJ_WR, grp);
	skb_tx_timestamp(skb);

	skb->dev->stats.tx_packets++;
	skb->dev->stats.tx_bytes += skb->len;
}
EXPORT_SYMBOL(ocelot_port_inject_frame);

void ocelot_drain_cpu_queue(struct ocelot *ocelot, int grp)
{
	while (ocelot_read(ocelot, QS_XTR_DATA_PRESENT) & BIT(grp))
		ocelot_read_rix(ocelot, QS_XTR_RD, grp);
}
EXPORT_SYMBOL(ocelot_drain_cpu_queue);

int ocelot_fdb_add(struct ocelot *ocelot, int port, const unsigned char *addr,
		   u16 vid, const struct net_device *bridge)
{
	if (!vid)
		vid = ocelot_vlan_unaware_pvid(ocelot, bridge);

	return ocelot_mact_learn(ocelot, port, addr, vid, ENTRYTYPE_LOCKED);
}
EXPORT_SYMBOL(ocelot_fdb_add);

int ocelot_fdb_del(struct ocelot *ocelot, int port, const unsigned char *addr,
		   u16 vid, const struct net_device *bridge)
{
	if (!vid)
		vid = ocelot_vlan_unaware_pvid(ocelot, bridge);

	return ocelot_mact_forget(ocelot, addr, vid);
}
EXPORT_SYMBOL(ocelot_fdb_del);

 
static int ocelot_mact_read(struct ocelot *ocelot, int port, int row, int col,
			    struct ocelot_mact_entry *entry)
{
	u32 val, dst, macl, mach;
	char mac[ETH_ALEN];

	 
	ocelot_field_write(ocelot, ANA_TABLES_MACTINDX_M_INDEX, row);
	ocelot_field_write(ocelot, ANA_TABLES_MACTINDX_BUCKET, col);

	 
	ocelot_write(ocelot,
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_READ),
		     ANA_TABLES_MACACCESS);

	if (ocelot_mact_wait_for_completion(ocelot))
		return -ETIMEDOUT;

	 
	val = ocelot_read(ocelot, ANA_TABLES_MACACCESS);
	if (!(val & ANA_TABLES_MACACCESS_VALID))
		return -EINVAL;

	 
	dst = (val & ANA_TABLES_MACACCESS_DEST_IDX_M) >> 3;
	if (dst != port)
		return -EINVAL;

	 
	macl = ocelot_read(ocelot, ANA_TABLES_MACLDATA);
	mach = ocelot_read(ocelot, ANA_TABLES_MACHDATA);

	mac[0] = (mach >> 8)  & 0xff;
	mac[1] = (mach >> 0)  & 0xff;
	mac[2] = (macl >> 24) & 0xff;
	mac[3] = (macl >> 16) & 0xff;
	mac[4] = (macl >> 8)  & 0xff;
	mac[5] = (macl >> 0)  & 0xff;

	entry->vid = (mach >> 16) & 0xfff;
	ether_addr_copy(entry->mac, mac);

	return 0;
}

int ocelot_mact_flush(struct ocelot *ocelot, int port)
{
	int err;

	mutex_lock(&ocelot->mact_lock);

	 
	ocelot_write(ocelot, ANA_ANAGEFIL_PID_EN | ANA_ANAGEFIL_PID_VAL(port),
		     ANA_ANAGEFIL);

	 
	ocelot_write(ocelot,
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_AGE),
		     ANA_TABLES_MACACCESS);

	err = ocelot_mact_wait_for_completion(ocelot);
	if (err) {
		mutex_unlock(&ocelot->mact_lock);
		return err;
	}

	 
	ocelot_write(ocelot,
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_AGE),
		     ANA_TABLES_MACACCESS);

	err = ocelot_mact_wait_for_completion(ocelot);

	 
	ocelot_write(ocelot, 0, ANA_ANAGEFIL);

	mutex_unlock(&ocelot->mact_lock);

	return err;
}
EXPORT_SYMBOL_GPL(ocelot_mact_flush);

int ocelot_fdb_dump(struct ocelot *ocelot, int port,
		    dsa_fdb_dump_cb_t *cb, void *data)
{
	int err = 0;
	int i, j;

	 
	mutex_lock(&ocelot->mact_lock);

	 
	for (i = 0; i < ocelot->num_mact_rows; i++) {
		for (j = 0; j < 4; j++) {
			struct ocelot_mact_entry entry;
			bool is_static;

			err = ocelot_mact_read(ocelot, port, i, j, &entry);
			 
			if (err == -EINVAL)
				continue;
			else if (err)
				break;

			is_static = (entry.type == ENTRYTYPE_LOCKED);

			 
			if (entry.vid > OCELOT_RSV_VLAN_RANGE_START)
				entry.vid = 0;

			err = cb(entry.mac, entry.vid, is_static, data);
			if (err)
				break;
		}
	}

	mutex_unlock(&ocelot->mact_lock);

	return err;
}
EXPORT_SYMBOL(ocelot_fdb_dump);

int ocelot_trap_add(struct ocelot *ocelot, int port,
		    unsigned long cookie, bool take_ts,
		    void (*populate)(struct ocelot_vcap_filter *f))
{
	struct ocelot_vcap_block *block_vcap_is2;
	struct ocelot_vcap_filter *trap;
	bool new = false;
	int err;

	block_vcap_is2 = &ocelot->block[VCAP_IS2];

	trap = ocelot_vcap_block_find_filter_by_id(block_vcap_is2, cookie,
						   false);
	if (!trap) {
		trap = kzalloc(sizeof(*trap), GFP_KERNEL);
		if (!trap)
			return -ENOMEM;

		populate(trap);
		trap->prio = 1;
		trap->id.cookie = cookie;
		trap->id.tc_offload = false;
		trap->block_id = VCAP_IS2;
		trap->type = OCELOT_VCAP_FILTER_OFFLOAD;
		trap->lookup = 0;
		trap->action.cpu_copy_ena = true;
		trap->action.mask_mode = OCELOT_MASK_MODE_PERMIT_DENY;
		trap->action.port_mask = 0;
		trap->take_ts = take_ts;
		trap->is_trap = true;
		new = true;
	}

	trap->ingress_port_mask |= BIT(port);

	if (new)
		err = ocelot_vcap_filter_add(ocelot, trap, NULL);
	else
		err = ocelot_vcap_filter_replace(ocelot, trap);
	if (err) {
		trap->ingress_port_mask &= ~BIT(port);
		if (!trap->ingress_port_mask)
			kfree(trap);
		return err;
	}

	return 0;
}

int ocelot_trap_del(struct ocelot *ocelot, int port, unsigned long cookie)
{
	struct ocelot_vcap_block *block_vcap_is2;
	struct ocelot_vcap_filter *trap;

	block_vcap_is2 = &ocelot->block[VCAP_IS2];

	trap = ocelot_vcap_block_find_filter_by_id(block_vcap_is2, cookie,
						   false);
	if (!trap)
		return 0;

	trap->ingress_port_mask &= ~BIT(port);
	if (!trap->ingress_port_mask)
		return ocelot_vcap_filter_del(ocelot, trap);

	return ocelot_vcap_filter_replace(ocelot, trap);
}

static u32 ocelot_get_bond_mask(struct ocelot *ocelot, struct net_device *bond)
{
	u32 mask = 0;
	int port;

	lockdep_assert_held(&ocelot->fwd_domain_lock);

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		struct ocelot_port *ocelot_port = ocelot->ports[port];

		if (!ocelot_port)
			continue;

		if (ocelot_port->bond == bond)
			mask |= BIT(port);
	}

	return mask;
}

 
int ocelot_bond_get_id(struct ocelot *ocelot, struct net_device *bond)
{
	int bond_mask = ocelot_get_bond_mask(ocelot, bond);

	if (!bond_mask)
		return -ENOENT;

	return __ffs(bond_mask);
}
EXPORT_SYMBOL_GPL(ocelot_bond_get_id);

 
static u32 ocelot_dsa_8021q_cpu_assigned_ports(struct ocelot *ocelot,
					       struct ocelot_port *cpu)
{
	u32 mask = 0;
	int port;

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		struct ocelot_port *ocelot_port = ocelot->ports[port];

		if (!ocelot_port)
			continue;

		if (ocelot_port->dsa_8021q_cpu == cpu)
			mask |= BIT(port);
	}

	if (cpu->bond)
		mask &= ~ocelot_get_bond_mask(ocelot, cpu->bond);

	return mask;
}

 
u32 ocelot_port_assigned_dsa_8021q_cpu_mask(struct ocelot *ocelot, int port)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	struct ocelot_port *cpu_port = ocelot_port->dsa_8021q_cpu;

	if (!cpu_port)
		return 0;

	if (cpu_port->bond)
		return ocelot_get_bond_mask(ocelot, cpu_port->bond);

	return BIT(cpu_port->index);
}
EXPORT_SYMBOL_GPL(ocelot_port_assigned_dsa_8021q_cpu_mask);

u32 ocelot_get_bridge_fwd_mask(struct ocelot *ocelot, int src_port)
{
	struct ocelot_port *ocelot_port = ocelot->ports[src_port];
	const struct net_device *bridge;
	u32 mask = 0;
	int port;

	if (!ocelot_port || ocelot_port->stp_state != BR_STATE_FORWARDING)
		return 0;

	bridge = ocelot_port->bridge;
	if (!bridge)
		return 0;

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		ocelot_port = ocelot->ports[port];

		if (!ocelot_port)
			continue;

		if (ocelot_port->stp_state == BR_STATE_FORWARDING &&
		    ocelot_port->bridge == bridge)
			mask |= BIT(port);
	}

	return mask;
}
EXPORT_SYMBOL_GPL(ocelot_get_bridge_fwd_mask);

static void ocelot_apply_bridge_fwd_mask(struct ocelot *ocelot, bool joining)
{
	int port;

	lockdep_assert_held(&ocelot->fwd_domain_lock);

	 
	if (joining && ocelot->ops->cut_through_fwd)
		ocelot->ops->cut_through_fwd(ocelot);

	 
	for (port = 0; port < ocelot->num_phys_ports; port++) {
		struct ocelot_port *ocelot_port = ocelot->ports[port];
		unsigned long mask;

		if (!ocelot_port) {
			 
			mask = 0;
		} else if (ocelot_port->is_dsa_8021q_cpu) {
			 
			mask = ocelot_dsa_8021q_cpu_assigned_ports(ocelot,
								   ocelot_port);
		} else if (ocelot_port->bridge) {
			struct net_device *bond = ocelot_port->bond;

			mask = ocelot_get_bridge_fwd_mask(ocelot, port);
			mask &= ~BIT(port);

			mask |= ocelot_port_assigned_dsa_8021q_cpu_mask(ocelot,
									port);

			if (bond)
				mask &= ~ocelot_get_bond_mask(ocelot, bond);
		} else {
			 
			mask = ocelot_port_assigned_dsa_8021q_cpu_mask(ocelot,
								       port);
		}

		ocelot_write_rix(ocelot, mask, ANA_PGID_PGID, PGID_SRC + port);
	}

	 
	if (!joining && ocelot->ops->cut_through_fwd)
		ocelot->ops->cut_through_fwd(ocelot);
}

 
static void ocelot_update_pgid_cpu(struct ocelot *ocelot)
{
	int pgid_cpu = 0;
	int port;

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		struct ocelot_port *ocelot_port = ocelot->ports[port];

		if (!ocelot_port || !ocelot_port->is_dsa_8021q_cpu)
			continue;

		pgid_cpu |= BIT(port);
	}

	if (!pgid_cpu)
		pgid_cpu = BIT(ocelot->num_phys_ports);

	ocelot_write_rix(ocelot, pgid_cpu, ANA_PGID_PGID, PGID_CPU);
}

void ocelot_port_setup_dsa_8021q_cpu(struct ocelot *ocelot, int cpu)
{
	struct ocelot_port *cpu_port = ocelot->ports[cpu];
	u16 vid;

	mutex_lock(&ocelot->fwd_domain_lock);

	cpu_port->is_dsa_8021q_cpu = true;

	for (vid = OCELOT_RSV_VLAN_RANGE_START; vid < VLAN_N_VID; vid++)
		ocelot_vlan_member_add(ocelot, cpu, vid, true);

	ocelot_update_pgid_cpu(ocelot);

	mutex_unlock(&ocelot->fwd_domain_lock);
}
EXPORT_SYMBOL_GPL(ocelot_port_setup_dsa_8021q_cpu);

void ocelot_port_teardown_dsa_8021q_cpu(struct ocelot *ocelot, int cpu)
{
	struct ocelot_port *cpu_port = ocelot->ports[cpu];
	u16 vid;

	mutex_lock(&ocelot->fwd_domain_lock);

	cpu_port->is_dsa_8021q_cpu = false;

	for (vid = OCELOT_RSV_VLAN_RANGE_START; vid < VLAN_N_VID; vid++)
		ocelot_vlan_member_del(ocelot, cpu_port->index, vid);

	ocelot_update_pgid_cpu(ocelot);

	mutex_unlock(&ocelot->fwd_domain_lock);
}
EXPORT_SYMBOL_GPL(ocelot_port_teardown_dsa_8021q_cpu);

void ocelot_port_assign_dsa_8021q_cpu(struct ocelot *ocelot, int port,
				      int cpu)
{
	struct ocelot_port *cpu_port = ocelot->ports[cpu];

	mutex_lock(&ocelot->fwd_domain_lock);

	ocelot->ports[port]->dsa_8021q_cpu = cpu_port;
	ocelot_apply_bridge_fwd_mask(ocelot, true);

	mutex_unlock(&ocelot->fwd_domain_lock);
}
EXPORT_SYMBOL_GPL(ocelot_port_assign_dsa_8021q_cpu);

void ocelot_port_unassign_dsa_8021q_cpu(struct ocelot *ocelot, int port)
{
	mutex_lock(&ocelot->fwd_domain_lock);

	ocelot->ports[port]->dsa_8021q_cpu = NULL;
	ocelot_apply_bridge_fwd_mask(ocelot, true);

	mutex_unlock(&ocelot->fwd_domain_lock);
}
EXPORT_SYMBOL_GPL(ocelot_port_unassign_dsa_8021q_cpu);

void ocelot_bridge_stp_state_set(struct ocelot *ocelot, int port, u8 state)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	u32 learn_ena = 0;

	mutex_lock(&ocelot->fwd_domain_lock);

	ocelot_port->stp_state = state;

	if ((state == BR_STATE_LEARNING || state == BR_STATE_FORWARDING) &&
	    ocelot_port->learn_ena)
		learn_ena = ANA_PORT_PORT_CFG_LEARN_ENA;

	ocelot_rmw_gix(ocelot, learn_ena, ANA_PORT_PORT_CFG_LEARN_ENA,
		       ANA_PORT_PORT_CFG, port);

	ocelot_apply_bridge_fwd_mask(ocelot, state == BR_STATE_FORWARDING);

	mutex_unlock(&ocelot->fwd_domain_lock);
}
EXPORT_SYMBOL(ocelot_bridge_stp_state_set);

void ocelot_set_ageing_time(struct ocelot *ocelot, unsigned int msecs)
{
	unsigned int age_period = ANA_AUTOAGE_AGE_PERIOD(msecs / 2000);

	 
	if (!age_period)
		age_period = 1;

	ocelot_rmw(ocelot, age_period, ANA_AUTOAGE_AGE_PERIOD_M, ANA_AUTOAGE);
}
EXPORT_SYMBOL(ocelot_set_ageing_time);

static struct ocelot_multicast *ocelot_multicast_get(struct ocelot *ocelot,
						     const unsigned char *addr,
						     u16 vid)
{
	struct ocelot_multicast *mc;

	list_for_each_entry(mc, &ocelot->multicast, list) {
		if (ether_addr_equal(mc->addr, addr) && mc->vid == vid)
			return mc;
	}

	return NULL;
}

static enum macaccess_entry_type ocelot_classify_mdb(const unsigned char *addr)
{
	if (addr[0] == 0x01 && addr[1] == 0x00 && addr[2] == 0x5e)
		return ENTRYTYPE_MACv4;
	if (addr[0] == 0x33 && addr[1] == 0x33)
		return ENTRYTYPE_MACv6;
	return ENTRYTYPE_LOCKED;
}

static struct ocelot_pgid *ocelot_pgid_alloc(struct ocelot *ocelot, int index,
					     unsigned long ports)
{
	struct ocelot_pgid *pgid;

	pgid = kzalloc(sizeof(*pgid), GFP_KERNEL);
	if (!pgid)
		return ERR_PTR(-ENOMEM);

	pgid->ports = ports;
	pgid->index = index;
	refcount_set(&pgid->refcount, 1);
	list_add_tail(&pgid->list, &ocelot->pgids);

	return pgid;
}

static void ocelot_pgid_free(struct ocelot *ocelot, struct ocelot_pgid *pgid)
{
	if (!refcount_dec_and_test(&pgid->refcount))
		return;

	list_del(&pgid->list);
	kfree(pgid);
}

static struct ocelot_pgid *ocelot_mdb_get_pgid(struct ocelot *ocelot,
					       const struct ocelot_multicast *mc)
{
	struct ocelot_pgid *pgid;
	int index;

	 
	if (mc->entry_type == ENTRYTYPE_MACv4 ||
	    mc->entry_type == ENTRYTYPE_MACv6)
		return ocelot_pgid_alloc(ocelot, 0, mc->ports);

	list_for_each_entry(pgid, &ocelot->pgids, list) {
		 
		if (pgid->index && pgid->ports == mc->ports) {
			refcount_inc(&pgid->refcount);
			return pgid;
		}
	}

	 
	for_each_nonreserved_multicast_dest_pgid(ocelot, index) {
		bool used = false;

		list_for_each_entry(pgid, &ocelot->pgids, list) {
			if (pgid->index == index) {
				used = true;
				break;
			}
		}

		if (!used)
			return ocelot_pgid_alloc(ocelot, index, mc->ports);
	}

	return ERR_PTR(-ENOSPC);
}

static void ocelot_encode_ports_to_mdb(unsigned char *addr,
				       struct ocelot_multicast *mc)
{
	ether_addr_copy(addr, mc->addr);

	if (mc->entry_type == ENTRYTYPE_MACv4) {
		addr[0] = 0;
		addr[1] = mc->ports >> 8;
		addr[2] = mc->ports & 0xff;
	} else if (mc->entry_type == ENTRYTYPE_MACv6) {
		addr[0] = mc->ports >> 8;
		addr[1] = mc->ports & 0xff;
	}
}

int ocelot_port_mdb_add(struct ocelot *ocelot, int port,
			const struct switchdev_obj_port_mdb *mdb,
			const struct net_device *bridge)
{
	unsigned char addr[ETH_ALEN];
	struct ocelot_multicast *mc;
	struct ocelot_pgid *pgid;
	u16 vid = mdb->vid;

	if (!vid)
		vid = ocelot_vlan_unaware_pvid(ocelot, bridge);

	mc = ocelot_multicast_get(ocelot, mdb->addr, vid);
	if (!mc) {
		 
		mc = devm_kzalloc(ocelot->dev, sizeof(*mc), GFP_KERNEL);
		if (!mc)
			return -ENOMEM;

		mc->entry_type = ocelot_classify_mdb(mdb->addr);
		ether_addr_copy(mc->addr, mdb->addr);
		mc->vid = vid;

		list_add_tail(&mc->list, &ocelot->multicast);
	} else {
		 
		ocelot_pgid_free(ocelot, mc->pgid);
		ocelot_encode_ports_to_mdb(addr, mc);
		ocelot_mact_forget(ocelot, addr, vid);
	}

	mc->ports |= BIT(port);

	pgid = ocelot_mdb_get_pgid(ocelot, mc);
	if (IS_ERR(pgid)) {
		dev_err(ocelot->dev,
			"Cannot allocate PGID for mdb %pM vid %d\n",
			mc->addr, mc->vid);
		devm_kfree(ocelot->dev, mc);
		return PTR_ERR(pgid);
	}
	mc->pgid = pgid;

	ocelot_encode_ports_to_mdb(addr, mc);

	if (mc->entry_type != ENTRYTYPE_MACv4 &&
	    mc->entry_type != ENTRYTYPE_MACv6)
		ocelot_write_rix(ocelot, pgid->ports, ANA_PGID_PGID,
				 pgid->index);

	return ocelot_mact_learn(ocelot, pgid->index, addr, vid,
				 mc->entry_type);
}
EXPORT_SYMBOL(ocelot_port_mdb_add);

int ocelot_port_mdb_del(struct ocelot *ocelot, int port,
			const struct switchdev_obj_port_mdb *mdb,
			const struct net_device *bridge)
{
	unsigned char addr[ETH_ALEN];
	struct ocelot_multicast *mc;
	struct ocelot_pgid *pgid;
	u16 vid = mdb->vid;

	if (!vid)
		vid = ocelot_vlan_unaware_pvid(ocelot, bridge);

	mc = ocelot_multicast_get(ocelot, mdb->addr, vid);
	if (!mc)
		return -ENOENT;

	ocelot_encode_ports_to_mdb(addr, mc);
	ocelot_mact_forget(ocelot, addr, vid);

	ocelot_pgid_free(ocelot, mc->pgid);
	mc->ports &= ~BIT(port);
	if (!mc->ports) {
		list_del(&mc->list);
		devm_kfree(ocelot->dev, mc);
		return 0;
	}

	 
	pgid = ocelot_mdb_get_pgid(ocelot, mc);
	if (IS_ERR(pgid))
		return PTR_ERR(pgid);
	mc->pgid = pgid;

	ocelot_encode_ports_to_mdb(addr, mc);

	if (mc->entry_type != ENTRYTYPE_MACv4 &&
	    mc->entry_type != ENTRYTYPE_MACv6)
		ocelot_write_rix(ocelot, pgid->ports, ANA_PGID_PGID,
				 pgid->index);

	return ocelot_mact_learn(ocelot, pgid->index, addr, vid,
				 mc->entry_type);
}
EXPORT_SYMBOL(ocelot_port_mdb_del);

int ocelot_port_bridge_join(struct ocelot *ocelot, int port,
			    struct net_device *bridge, int bridge_num,
			    struct netlink_ext_ack *extack)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	int err;

	err = ocelot_single_vlan_aware_bridge(ocelot, extack);
	if (err)
		return err;

	mutex_lock(&ocelot->fwd_domain_lock);

	ocelot_port->bridge = bridge;
	ocelot_port->bridge_num = bridge_num;

	ocelot_apply_bridge_fwd_mask(ocelot, true);

	mutex_unlock(&ocelot->fwd_domain_lock);

	if (br_vlan_enabled(bridge))
		return 0;

	return ocelot_add_vlan_unaware_pvid(ocelot, port, bridge);
}
EXPORT_SYMBOL(ocelot_port_bridge_join);

void ocelot_port_bridge_leave(struct ocelot *ocelot, int port,
			      struct net_device *bridge)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];

	mutex_lock(&ocelot->fwd_domain_lock);

	if (!br_vlan_enabled(bridge))
		ocelot_del_vlan_unaware_pvid(ocelot, port, bridge);

	ocelot_port->bridge = NULL;
	ocelot_port->bridge_num = -1;

	ocelot_port_set_pvid(ocelot, port, NULL);
	ocelot_port_manage_port_tag(ocelot, port);
	ocelot_apply_bridge_fwd_mask(ocelot, false);

	mutex_unlock(&ocelot->fwd_domain_lock);
}
EXPORT_SYMBOL(ocelot_port_bridge_leave);

static void ocelot_set_aggr_pgids(struct ocelot *ocelot)
{
	unsigned long visited = GENMASK(ocelot->num_phys_ports - 1, 0);
	int i, port, lag;

	 
	for_each_unicast_dest_pgid(ocelot, port)
		ocelot_write_rix(ocelot, BIT(port), ANA_PGID_PGID, port);

	for_each_aggr_pgid(ocelot, i)
		ocelot_write_rix(ocelot, GENMASK(ocelot->num_phys_ports - 1, 0),
				 ANA_PGID_PGID, i);

	 
	for (port = 0; port < ocelot->num_phys_ports; port++) {
		struct ocelot_port *ocelot_port = ocelot->ports[port];

		if (!ocelot_port || !ocelot_port->bond)
			continue;

		visited &= ~BIT(port);
	}

	 
	for (lag = 0; lag < ocelot->num_phys_ports; lag++) {
		struct net_device *bond = ocelot->ports[lag]->bond;
		int num_active_ports = 0;
		unsigned long bond_mask;
		u8 aggr_idx[16];

		if (!bond || (visited & BIT(lag)))
			continue;

		bond_mask = ocelot_get_bond_mask(ocelot, bond);

		for_each_set_bit(port, &bond_mask, ocelot->num_phys_ports) {
			struct ocelot_port *ocelot_port = ocelot->ports[port];

			
			ocelot_write_rix(ocelot, bond_mask,
					 ANA_PGID_PGID, port);

			if (ocelot_port->lag_tx_active)
				aggr_idx[num_active_ports++] = port;
		}

		for_each_aggr_pgid(ocelot, i) {
			u32 ac;

			ac = ocelot_read_rix(ocelot, ANA_PGID_PGID, i);
			ac &= ~bond_mask;
			 
			if (num_active_ports)
				ac |= BIT(aggr_idx[i % num_active_ports]);
			ocelot_write_rix(ocelot, ac, ANA_PGID_PGID, i);
		}

		 
		for (port = lag; port < ocelot->num_phys_ports; port++) {
			struct ocelot_port *ocelot_port = ocelot->ports[port];

			if (!ocelot_port)
				continue;

			if (ocelot_port->bond == bond)
				visited |= BIT(port);
		}
	}
}

 
static void ocelot_setup_logical_port_ids(struct ocelot *ocelot)
{
	int port;

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		struct ocelot_port *ocelot_port = ocelot->ports[port];
		struct net_device *bond;

		if (!ocelot_port)
			continue;

		bond = ocelot_port->bond;
		if (bond) {
			int lag = ocelot_bond_get_id(ocelot, bond);

			ocelot_rmw_gix(ocelot,
				       ANA_PORT_PORT_CFG_PORTID_VAL(lag),
				       ANA_PORT_PORT_CFG_PORTID_VAL_M,
				       ANA_PORT_PORT_CFG, port);
		} else {
			ocelot_rmw_gix(ocelot,
				       ANA_PORT_PORT_CFG_PORTID_VAL(port),
				       ANA_PORT_PORT_CFG_PORTID_VAL_M,
				       ANA_PORT_PORT_CFG, port);
		}
	}
}

static int ocelot_migrate_mc(struct ocelot *ocelot, struct ocelot_multicast *mc,
			     unsigned long from_mask, unsigned long to_mask)
{
	unsigned char addr[ETH_ALEN];
	struct ocelot_pgid *pgid;
	u16 vid = mc->vid;

	dev_dbg(ocelot->dev,
		"Migrating multicast %pM vid %d from port mask 0x%lx to 0x%lx\n",
		mc->addr, mc->vid, from_mask, to_mask);

	 
	ocelot_pgid_free(ocelot, mc->pgid);
	ocelot_encode_ports_to_mdb(addr, mc);
	ocelot_mact_forget(ocelot, addr, vid);

	mc->ports &= ~from_mask;
	mc->ports |= to_mask;

	pgid = ocelot_mdb_get_pgid(ocelot, mc);
	if (IS_ERR(pgid)) {
		dev_err(ocelot->dev,
			"Cannot allocate PGID for mdb %pM vid %d\n",
			mc->addr, mc->vid);
		devm_kfree(ocelot->dev, mc);
		return PTR_ERR(pgid);
	}
	mc->pgid = pgid;

	ocelot_encode_ports_to_mdb(addr, mc);

	if (mc->entry_type != ENTRYTYPE_MACv4 &&
	    mc->entry_type != ENTRYTYPE_MACv6)
		ocelot_write_rix(ocelot, pgid->ports, ANA_PGID_PGID,
				 pgid->index);

	return ocelot_mact_learn(ocelot, pgid->index, addr, vid,
				 mc->entry_type);
}

int ocelot_migrate_mdbs(struct ocelot *ocelot, unsigned long from_mask,
			unsigned long to_mask)
{
	struct ocelot_multicast *mc;
	int err;

	list_for_each_entry(mc, &ocelot->multicast, list) {
		if (!(mc->ports & from_mask))
			continue;

		err = ocelot_migrate_mc(ocelot, mc, from_mask, to_mask);
		if (err)
			return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ocelot_migrate_mdbs);

 
static void ocelot_migrate_lag_fdbs(struct ocelot *ocelot,
				    struct net_device *bond,
				    int lag)
{
	struct ocelot_lag_fdb *fdb;
	int err;

	lockdep_assert_held(&ocelot->fwd_domain_lock);

	list_for_each_entry(fdb, &ocelot->lag_fdbs, list) {
		if (fdb->bond != bond)
			continue;

		err = ocelot_mact_forget(ocelot, fdb->addr, fdb->vid);
		if (err) {
			dev_err(ocelot->dev,
				"failed to delete LAG %s FDB %pM vid %d: %pe\n",
				bond->name, fdb->addr, fdb->vid, ERR_PTR(err));
		}

		err = ocelot_mact_learn(ocelot, lag, fdb->addr, fdb->vid,
					ENTRYTYPE_LOCKED);
		if (err) {
			dev_err(ocelot->dev,
				"failed to migrate LAG %s FDB %pM vid %d: %pe\n",
				bond->name, fdb->addr, fdb->vid, ERR_PTR(err));
		}
	}
}

int ocelot_port_lag_join(struct ocelot *ocelot, int port,
			 struct net_device *bond,
			 struct netdev_lag_upper_info *info,
			 struct netlink_ext_ack *extack)
{
	if (info->tx_type != NETDEV_LAG_TX_TYPE_HASH) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Can only offload LAG using hash TX type");
		return -EOPNOTSUPP;
	}

	mutex_lock(&ocelot->fwd_domain_lock);

	ocelot->ports[port]->bond = bond;

	ocelot_setup_logical_port_ids(ocelot);
	ocelot_apply_bridge_fwd_mask(ocelot, true);
	ocelot_set_aggr_pgids(ocelot);

	mutex_unlock(&ocelot->fwd_domain_lock);

	return 0;
}
EXPORT_SYMBOL(ocelot_port_lag_join);

void ocelot_port_lag_leave(struct ocelot *ocelot, int port,
			   struct net_device *bond)
{
	int old_lag_id, new_lag_id;

	mutex_lock(&ocelot->fwd_domain_lock);

	old_lag_id = ocelot_bond_get_id(ocelot, bond);

	ocelot->ports[port]->bond = NULL;

	ocelot_setup_logical_port_ids(ocelot);
	ocelot_apply_bridge_fwd_mask(ocelot, false);
	ocelot_set_aggr_pgids(ocelot);

	new_lag_id = ocelot_bond_get_id(ocelot, bond);

	if (new_lag_id >= 0 && old_lag_id != new_lag_id)
		ocelot_migrate_lag_fdbs(ocelot, bond, new_lag_id);

	mutex_unlock(&ocelot->fwd_domain_lock);
}
EXPORT_SYMBOL(ocelot_port_lag_leave);

void ocelot_port_lag_change(struct ocelot *ocelot, int port, bool lag_tx_active)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];

	mutex_lock(&ocelot->fwd_domain_lock);

	ocelot_port->lag_tx_active = lag_tx_active;

	 
	ocelot_set_aggr_pgids(ocelot);

	mutex_unlock(&ocelot->fwd_domain_lock);
}
EXPORT_SYMBOL(ocelot_port_lag_change);

int ocelot_lag_fdb_add(struct ocelot *ocelot, struct net_device *bond,
		       const unsigned char *addr, u16 vid,
		       const struct net_device *bridge)
{
	struct ocelot_lag_fdb *fdb;
	int lag, err;

	fdb = kzalloc(sizeof(*fdb), GFP_KERNEL);
	if (!fdb)
		return -ENOMEM;

	mutex_lock(&ocelot->fwd_domain_lock);

	if (!vid)
		vid = ocelot_vlan_unaware_pvid(ocelot, bridge);

	ether_addr_copy(fdb->addr, addr);
	fdb->vid = vid;
	fdb->bond = bond;

	lag = ocelot_bond_get_id(ocelot, bond);

	err = ocelot_mact_learn(ocelot, lag, addr, vid, ENTRYTYPE_LOCKED);
	if (err) {
		mutex_unlock(&ocelot->fwd_domain_lock);
		kfree(fdb);
		return err;
	}

	list_add_tail(&fdb->list, &ocelot->lag_fdbs);
	mutex_unlock(&ocelot->fwd_domain_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(ocelot_lag_fdb_add);

int ocelot_lag_fdb_del(struct ocelot *ocelot, struct net_device *bond,
		       const unsigned char *addr, u16 vid,
		       const struct net_device *bridge)
{
	struct ocelot_lag_fdb *fdb, *tmp;

	mutex_lock(&ocelot->fwd_domain_lock);

	if (!vid)
		vid = ocelot_vlan_unaware_pvid(ocelot, bridge);

	list_for_each_entry_safe(fdb, tmp, &ocelot->lag_fdbs, list) {
		if (!ether_addr_equal(fdb->addr, addr) || fdb->vid != vid ||
		    fdb->bond != bond)
			continue;

		ocelot_mact_forget(ocelot, addr, vid);
		list_del(&fdb->list);
		mutex_unlock(&ocelot->fwd_domain_lock);
		kfree(fdb);

		return 0;
	}

	mutex_unlock(&ocelot->fwd_domain_lock);

	return -ENOENT;
}
EXPORT_SYMBOL_GPL(ocelot_lag_fdb_del);

 
void ocelot_port_set_maxlen(struct ocelot *ocelot, int port, size_t sdu)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	int maxlen = sdu + ETH_HLEN + ETH_FCS_LEN;
	int pause_start, pause_stop;
	int atop, atop_tot;

	if (port == ocelot->npi) {
		maxlen += OCELOT_TAG_LEN;

		if (ocelot->npi_inj_prefix == OCELOT_TAG_PREFIX_SHORT)
			maxlen += OCELOT_SHORT_PREFIX_LEN;
		else if (ocelot->npi_inj_prefix == OCELOT_TAG_PREFIX_LONG)
			maxlen += OCELOT_LONG_PREFIX_LEN;
	}

	ocelot_port_writel(ocelot_port, maxlen, DEV_MAC_MAXLEN_CFG);

	 
	pause_start = 6 * maxlen / OCELOT_BUFFER_CELL_SZ;
	pause_stop = 4 * maxlen / OCELOT_BUFFER_CELL_SZ;
	ocelot_fields_write(ocelot, port, SYS_PAUSE_CFG_PAUSE_START,
			    pause_start);
	ocelot_fields_write(ocelot, port, SYS_PAUSE_CFG_PAUSE_STOP,
			    pause_stop);

	 
	atop_tot = (ocelot->packet_buffer_size - 9 * maxlen) /
		   OCELOT_BUFFER_CELL_SZ;
	atop = (9 * maxlen) / OCELOT_BUFFER_CELL_SZ;
	ocelot_write_rix(ocelot, ocelot->ops->wm_enc(atop), SYS_ATOP, port);
	ocelot_write(ocelot, ocelot->ops->wm_enc(atop_tot), SYS_ATOP_TOT_CFG);
}
EXPORT_SYMBOL(ocelot_port_set_maxlen);

int ocelot_get_max_mtu(struct ocelot *ocelot, int port)
{
	int max_mtu = 65535 - ETH_HLEN - ETH_FCS_LEN;

	if (port == ocelot->npi) {
		max_mtu -= OCELOT_TAG_LEN;

		if (ocelot->npi_inj_prefix == OCELOT_TAG_PREFIX_SHORT)
			max_mtu -= OCELOT_SHORT_PREFIX_LEN;
		else if (ocelot->npi_inj_prefix == OCELOT_TAG_PREFIX_LONG)
			max_mtu -= OCELOT_LONG_PREFIX_LEN;
	}

	return max_mtu;
}
EXPORT_SYMBOL(ocelot_get_max_mtu);

static void ocelot_port_set_learning(struct ocelot *ocelot, int port,
				     bool enabled)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];
	u32 val = 0;

	if (enabled)
		val = ANA_PORT_PORT_CFG_LEARN_ENA;

	ocelot_rmw_gix(ocelot, val, ANA_PORT_PORT_CFG_LEARN_ENA,
		       ANA_PORT_PORT_CFG, port);

	ocelot_port->learn_ena = enabled;
}

static void ocelot_port_set_ucast_flood(struct ocelot *ocelot, int port,
					bool enabled)
{
	u32 val = 0;

	if (enabled)
		val = BIT(port);

	ocelot_rmw_rix(ocelot, val, BIT(port), ANA_PGID_PGID, PGID_UC);
}

static void ocelot_port_set_mcast_flood(struct ocelot *ocelot, int port,
					bool enabled)
{
	u32 val = 0;

	if (enabled)
		val = BIT(port);

	ocelot_rmw_rix(ocelot, val, BIT(port), ANA_PGID_PGID, PGID_MC);
	ocelot_rmw_rix(ocelot, val, BIT(port), ANA_PGID_PGID, PGID_MCIPV4);
	ocelot_rmw_rix(ocelot, val, BIT(port), ANA_PGID_PGID, PGID_MCIPV6);
}

static void ocelot_port_set_bcast_flood(struct ocelot *ocelot, int port,
					bool enabled)
{
	u32 val = 0;

	if (enabled)
		val = BIT(port);

	ocelot_rmw_rix(ocelot, val, BIT(port), ANA_PGID_PGID, PGID_BC);
}

int ocelot_port_pre_bridge_flags(struct ocelot *ocelot, int port,
				 struct switchdev_brport_flags flags)
{
	if (flags.mask & ~(BR_LEARNING | BR_FLOOD | BR_MCAST_FLOOD |
			   BR_BCAST_FLOOD))
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(ocelot_port_pre_bridge_flags);

void ocelot_port_bridge_flags(struct ocelot *ocelot, int port,
			      struct switchdev_brport_flags flags)
{
	if (flags.mask & BR_LEARNING)
		ocelot_port_set_learning(ocelot, port,
					 !!(flags.val & BR_LEARNING));

	if (flags.mask & BR_FLOOD)
		ocelot_port_set_ucast_flood(ocelot, port,
					    !!(flags.val & BR_FLOOD));

	if (flags.mask & BR_MCAST_FLOOD)
		ocelot_port_set_mcast_flood(ocelot, port,
					    !!(flags.val & BR_MCAST_FLOOD));

	if (flags.mask & BR_BCAST_FLOOD)
		ocelot_port_set_bcast_flood(ocelot, port,
					    !!(flags.val & BR_BCAST_FLOOD));
}
EXPORT_SYMBOL(ocelot_port_bridge_flags);

int ocelot_port_get_default_prio(struct ocelot *ocelot, int port)
{
	int val = ocelot_read_gix(ocelot, ANA_PORT_QOS_CFG, port);

	return ANA_PORT_QOS_CFG_QOS_DEFAULT_VAL_X(val);
}
EXPORT_SYMBOL_GPL(ocelot_port_get_default_prio);

int ocelot_port_set_default_prio(struct ocelot *ocelot, int port, u8 prio)
{
	if (prio >= OCELOT_NUM_TC)
		return -ERANGE;

	ocelot_rmw_gix(ocelot,
		       ANA_PORT_QOS_CFG_QOS_DEFAULT_VAL(prio),
		       ANA_PORT_QOS_CFG_QOS_DEFAULT_VAL_M,
		       ANA_PORT_QOS_CFG,
		       port);

	return 0;
}
EXPORT_SYMBOL_GPL(ocelot_port_set_default_prio);

int ocelot_port_get_dscp_prio(struct ocelot *ocelot, int port, u8 dscp)
{
	int qos_cfg = ocelot_read_gix(ocelot, ANA_PORT_QOS_CFG, port);
	int dscp_cfg = ocelot_read_rix(ocelot, ANA_DSCP_CFG, dscp);

	 
	if (!(qos_cfg & ANA_PORT_QOS_CFG_QOS_DSCP_ENA))
		return -EOPNOTSUPP;

	if (qos_cfg & ANA_PORT_QOS_CFG_DSCP_TRANSLATE_ENA) {
		dscp = ANA_DSCP_CFG_DSCP_TRANSLATE_VAL_X(dscp_cfg);
		 
		dscp_cfg = ocelot_read_rix(ocelot, ANA_DSCP_CFG, dscp);
	}

	 
	if (!(dscp_cfg & ANA_DSCP_CFG_DSCP_TRUST_ENA))
		return -EOPNOTSUPP;

	return ANA_DSCP_CFG_QOS_DSCP_VAL_X(dscp_cfg);
}
EXPORT_SYMBOL_GPL(ocelot_port_get_dscp_prio);

int ocelot_port_add_dscp_prio(struct ocelot *ocelot, int port, u8 dscp, u8 prio)
{
	int mask, val;

	if (prio >= OCELOT_NUM_TC)
		return -ERANGE;

	 
	mask = ANA_PORT_QOS_CFG_QOS_DSCP_ENA |
	       ANA_PORT_QOS_CFG_DSCP_TRANSLATE_ENA;

	ocelot_rmw_gix(ocelot, ANA_PORT_QOS_CFG_QOS_DSCP_ENA, mask,
		       ANA_PORT_QOS_CFG, port);

	 
	val = ANA_DSCP_CFG_DSCP_TRUST_ENA | ANA_DSCP_CFG_QOS_DSCP_VAL(prio);

	ocelot_write_rix(ocelot, val, ANA_DSCP_CFG, dscp);

	return 0;
}
EXPORT_SYMBOL_GPL(ocelot_port_add_dscp_prio);

int ocelot_port_del_dscp_prio(struct ocelot *ocelot, int port, u8 dscp, u8 prio)
{
	int dscp_cfg = ocelot_read_rix(ocelot, ANA_DSCP_CFG, dscp);
	int mask, i;

	 
	if (ANA_DSCP_CFG_QOS_DSCP_VAL_X(dscp_cfg) != prio)
		return 0;

	 
	ocelot_write_rix(ocelot, 0, ANA_DSCP_CFG, dscp);

	for (i = 0; i < 64; i++) {
		int dscp_cfg = ocelot_read_rix(ocelot, ANA_DSCP_CFG, i);

		 
		if (dscp_cfg & ANA_DSCP_CFG_DSCP_TRUST_ENA)
			return 0;
	}

	 
	mask = ANA_PORT_QOS_CFG_QOS_DSCP_ENA |
	       ANA_PORT_QOS_CFG_DSCP_TRANSLATE_ENA;

	ocelot_rmw_gix(ocelot, 0, mask, ANA_PORT_QOS_CFG, port);

	return 0;
}
EXPORT_SYMBOL_GPL(ocelot_port_del_dscp_prio);

struct ocelot_mirror *ocelot_mirror_get(struct ocelot *ocelot, int to,
					struct netlink_ext_ack *extack)
{
	struct ocelot_mirror *m = ocelot->mirror;

	if (m) {
		if (m->to != to) {
			NL_SET_ERR_MSG_MOD(extack,
					   "Mirroring already configured towards different egress port");
			return ERR_PTR(-EBUSY);
		}

		refcount_inc(&m->refcount);
		return m;
	}

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return ERR_PTR(-ENOMEM);

	m->to = to;
	refcount_set(&m->refcount, 1);
	ocelot->mirror = m;

	 
	ocelot_write(ocelot, BIT(to), ANA_MIRRORPORTS);

	return m;
}

void ocelot_mirror_put(struct ocelot *ocelot)
{
	struct ocelot_mirror *m = ocelot->mirror;

	if (!refcount_dec_and_test(&m->refcount))
		return;

	ocelot_write(ocelot, 0, ANA_MIRRORPORTS);
	ocelot->mirror = NULL;
	kfree(m);
}

int ocelot_port_mirror_add(struct ocelot *ocelot, int from, int to,
			   bool ingress, struct netlink_ext_ack *extack)
{
	struct ocelot_mirror *m = ocelot_mirror_get(ocelot, to, extack);

	if (IS_ERR(m))
		return PTR_ERR(m);

	if (ingress) {
		ocelot_rmw_gix(ocelot, ANA_PORT_PORT_CFG_SRC_MIRROR_ENA,
			       ANA_PORT_PORT_CFG_SRC_MIRROR_ENA,
			       ANA_PORT_PORT_CFG, from);
	} else {
		ocelot_rmw(ocelot, BIT(from), BIT(from),
			   ANA_EMIRRORPORTS);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ocelot_port_mirror_add);

void ocelot_port_mirror_del(struct ocelot *ocelot, int from, bool ingress)
{
	if (ingress) {
		ocelot_rmw_gix(ocelot, 0, ANA_PORT_PORT_CFG_SRC_MIRROR_ENA,
			       ANA_PORT_PORT_CFG, from);
	} else {
		ocelot_rmw(ocelot, 0, BIT(from), ANA_EMIRRORPORTS);
	}

	ocelot_mirror_put(ocelot);
}
EXPORT_SYMBOL_GPL(ocelot_port_mirror_del);

static void ocelot_port_reset_mqprio(struct ocelot *ocelot, int port)
{
	struct net_device *dev = ocelot->ops->port_to_netdev(ocelot, port);

	netdev_reset_tc(dev);
	ocelot_port_change_fp(ocelot, port, 0);
}

int ocelot_port_mqprio(struct ocelot *ocelot, int port,
		       struct tc_mqprio_qopt_offload *mqprio)
{
	struct net_device *dev = ocelot->ops->port_to_netdev(ocelot, port);
	struct netlink_ext_ack *extack = mqprio->extack;
	struct tc_mqprio_qopt *qopt = &mqprio->qopt;
	int num_tc = qopt->num_tc;
	int tc, err;

	if (!num_tc) {
		ocelot_port_reset_mqprio(ocelot, port);
		return 0;
	}

	err = netdev_set_num_tc(dev, num_tc);
	if (err)
		return err;

	for (tc = 0; tc < num_tc; tc++) {
		if (qopt->count[tc] != 1) {
			NL_SET_ERR_MSG_MOD(extack,
					   "Only one TXQ per TC supported");
			return -EINVAL;
		}

		err = netdev_set_tc_queue(dev, tc, 1, qopt->offset[tc]);
		if (err)
			goto err_reset_tc;
	}

	err = netif_set_real_num_tx_queues(dev, num_tc);
	if (err)
		goto err_reset_tc;

	ocelot_port_change_fp(ocelot, port, mqprio->preemptible_tcs);

	return 0;

err_reset_tc:
	ocelot_port_reset_mqprio(ocelot, port);
	return err;
}
EXPORT_SYMBOL_GPL(ocelot_port_mqprio);

void ocelot_init_port(struct ocelot *ocelot, int port)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];

	skb_queue_head_init(&ocelot_port->tx_skbs);

	 

	 
	ocelot_port_writel(ocelot_port, DEV_MAC_IFG_CFG_TX_IFG(5),
			   DEV_MAC_IFG_CFG);

	 
	ocelot_port_writel(ocelot_port, DEV_MAC_HDX_CFG_LATE_COL_POS(67) |
			   DEV_MAC_HDX_CFG_SEED_LOAD,
			   DEV_MAC_HDX_CFG);
	mdelay(1);
	ocelot_port_writel(ocelot_port, DEV_MAC_HDX_CFG_LATE_COL_POS(67),
			   DEV_MAC_HDX_CFG);

	 
	ocelot_port_set_maxlen(ocelot, port, ETH_DATA_LEN);
	ocelot_port_writel(ocelot_port, DEV_MAC_TAGS_CFG_TAG_ID(ETH_P_8021AD) |
			   DEV_MAC_TAGS_CFG_VLAN_AWR_ENA |
			   DEV_MAC_TAGS_CFG_VLAN_DBL_AWR_ENA |
			   DEV_MAC_TAGS_CFG_VLAN_LEN_AWR_ENA,
			   DEV_MAC_TAGS_CFG);

	 
	ocelot_port_writel(ocelot_port, 0, DEV_MAC_FC_MAC_HIGH_CFG);
	ocelot_port_writel(ocelot_port, 0, DEV_MAC_FC_MAC_LOW_CFG);

	 
	ocelot_fields_write(ocelot, port, SYS_PAUSE_CFG_PAUSE_ENA, 1);

	 
	ocelot_rmw_gix(ocelot, ANA_PORT_DROP_CFG_DROP_MC_SMAC_ENA,
		       ANA_PORT_DROP_CFG_DROP_MC_SMAC_ENA,
		       ANA_PORT_DROP_CFG, port);

	 
	ocelot_rmw_gix(ocelot, REW_PORT_VLAN_CFG_PORT_TPID(ETH_P_8021Q),
		       REW_PORT_VLAN_CFG_PORT_TPID_M,
		       REW_PORT_VLAN_CFG, port);

	 
	ocelot_port_set_learning(ocelot, port, false);

	 
	ocelot_write_gix(ocelot, ANA_PORT_PORT_CFG_LEARNAUTO |
			 ANA_PORT_PORT_CFG_RECV_ENA |
			 ANA_PORT_PORT_CFG_PORTID_VAL(port),
			 ANA_PORT_PORT_CFG, port);

	 
	ocelot_vcap_enable(ocelot, port);
}
EXPORT_SYMBOL(ocelot_init_port);

 
static void ocelot_cpu_port_init(struct ocelot *ocelot)
{
	int cpu = ocelot->num_phys_ports;

	 
	ocelot_write_rix(ocelot, 0, ANA_PGID_PGID, cpu);
	 
	ocelot_write_rix(ocelot, BIT(cpu), ANA_PGID_PGID, PGID_CPU);
	ocelot_write_gix(ocelot, ANA_PORT_PORT_CFG_RECV_ENA |
			 ANA_PORT_PORT_CFG_PORTID_VAL(cpu),
			 ANA_PORT_PORT_CFG, cpu);

	 
	ocelot_fields_write(ocelot, cpu, QSYS_SWITCH_PORT_MODE_PORT_ENA, 1);
	 
	ocelot_fields_write(ocelot, cpu, SYS_PORT_MODE_INCL_XTR_HDR,
			    OCELOT_TAG_PREFIX_NONE);
	ocelot_fields_write(ocelot, cpu, SYS_PORT_MODE_INCL_INJ_HDR,
			    OCELOT_TAG_PREFIX_NONE);

	 
	ocelot_write_gix(ocelot,
			 ANA_PORT_VLAN_CFG_VLAN_VID(OCELOT_STANDALONE_PVID) |
			 ANA_PORT_VLAN_CFG_VLAN_AWARE_ENA |
			 ANA_PORT_VLAN_CFG_VLAN_POP_CNT(1),
			 ANA_PORT_VLAN_CFG, cpu);
}

static void ocelot_detect_features(struct ocelot *ocelot)
{
	int mmgt, eq_ctrl;

	 
	mmgt = ocelot_read(ocelot, SYS_MMGT);
	ocelot->packet_buffer_size = 240 * SYS_MMGT_FREECNT(mmgt);

	eq_ctrl = ocelot_read(ocelot, QSYS_EQ_CTRL);
	ocelot->num_frame_refs = QSYS_MMGT_EQ_CTRL_FP_FREE_CNT(eq_ctrl);
}

static int ocelot_mem_init_status(struct ocelot *ocelot)
{
	unsigned int val;
	int err;

	err = regmap_field_read(ocelot->regfields[SYS_RESET_CFG_MEM_INIT],
				&val);

	return err ?: val;
}

int ocelot_reset(struct ocelot *ocelot)
{
	int err;
	u32 val;

	err = regmap_field_write(ocelot->regfields[SYS_RESET_CFG_MEM_INIT], 1);
	if (err)
		return err;

	err = regmap_field_write(ocelot->regfields[SYS_RESET_CFG_MEM_ENA], 1);
	if (err)
		return err;

	 
	err = readx_poll_timeout(ocelot_mem_init_status, ocelot, val, !val,
				 MEM_INIT_SLEEP_US, MEM_INIT_TIMEOUT_US);
	if (err)
		return err;

	err = regmap_field_write(ocelot->regfields[SYS_RESET_CFG_MEM_ENA], 1);
	if (err)
		return err;

	return regmap_field_write(ocelot->regfields[SYS_RESET_CFG_CORE_ENA], 1);
}
EXPORT_SYMBOL(ocelot_reset);

int ocelot_init(struct ocelot *ocelot)
{
	int i, ret;
	u32 port;

	if (ocelot->ops->reset) {
		ret = ocelot->ops->reset(ocelot);
		if (ret) {
			dev_err(ocelot->dev, "Switch reset failed\n");
			return ret;
		}
	}

	mutex_init(&ocelot->mact_lock);
	mutex_init(&ocelot->fwd_domain_lock);
	spin_lock_init(&ocelot->ptp_clock_lock);
	spin_lock_init(&ocelot->ts_id_lock);

	ocelot->owq = alloc_ordered_workqueue("ocelot-owq", 0);
	if (!ocelot->owq)
		return -ENOMEM;

	ret = ocelot_stats_init(ocelot);
	if (ret)
		goto err_stats_init;

	INIT_LIST_HEAD(&ocelot->multicast);
	INIT_LIST_HEAD(&ocelot->pgids);
	INIT_LIST_HEAD(&ocelot->vlans);
	INIT_LIST_HEAD(&ocelot->lag_fdbs);
	ocelot_detect_features(ocelot);
	ocelot_mact_init(ocelot);
	ocelot_vlan_init(ocelot);
	ocelot_vcap_init(ocelot);
	ocelot_cpu_port_init(ocelot);

	if (ocelot->ops->psfp_init)
		ocelot->ops->psfp_init(ocelot);

	if (ocelot->mm_supported) {
		ret = ocelot_mm_init(ocelot);
		if (ret)
			goto err_mm_init;
	}

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		 
		ocelot_write(ocelot, SYS_STAT_CFG_STAT_VIEW(port) |
				     SYS_STAT_CFG_STAT_CLEAR_SHOT(0x7f),
			     SYS_STAT_CFG);
	}

	 
	ocelot_write(ocelot, ETH_P_8021AD, SYS_VLAN_ETYPE_CFG);

	 
	ocelot_write(ocelot, ANA_AGGR_CFG_AC_SMAC_ENA |
			     ANA_AGGR_CFG_AC_DMAC_ENA |
			     ANA_AGGR_CFG_AC_IP4_SIPDIP_ENA |
			     ANA_AGGR_CFG_AC_IP4_TCPUDP_ENA |
			     ANA_AGGR_CFG_AC_IP6_FLOW_LBL_ENA |
			     ANA_AGGR_CFG_AC_IP6_TCPUDP_ENA,
			     ANA_AGGR_CFG);

	 
	ocelot_write(ocelot,
		     ANA_AUTOAGE_AGE_PERIOD(BR_DEFAULT_AGEING_TIME / 2 / HZ),
		     ANA_AUTOAGE);

	 
	regmap_field_write(ocelot->regfields[ANA_ADVLEARN_VLAN_CHK], 1);

	 
	ocelot_write(ocelot, SYS_FRM_AGING_AGE_TX_ENA |
		     SYS_FRM_AGING_MAX_AGE(307692), SYS_FRM_AGING);

	 
	for (i = 0; i < ocelot->num_flooding_pgids; i++)
		ocelot_write_rix(ocelot, ANA_FLOODING_FLD_MULTICAST(PGID_MC) |
				 ANA_FLOODING_FLD_BROADCAST(PGID_BC) |
				 ANA_FLOODING_FLD_UNICAST(PGID_UC),
				 ANA_FLOODING, i);
	ocelot_write(ocelot, ANA_FLOODING_IPMC_FLD_MC6_DATA(PGID_MCIPV6) |
		     ANA_FLOODING_IPMC_FLD_MC6_CTRL(PGID_MC) |
		     ANA_FLOODING_IPMC_FLD_MC4_DATA(PGID_MCIPV4) |
		     ANA_FLOODING_IPMC_FLD_MC4_CTRL(PGID_MC),
		     ANA_FLOODING_IPMC);

	for (port = 0; port < ocelot->num_phys_ports; port++) {
		 
		ocelot_write_rix(ocelot, BIT(port), ANA_PGID_PGID, port);
		 
		ocelot_write_gix(ocelot,
				 ANA_PORT_CPU_FWD_BPDU_CFG_BPDU_REDIR_ENA(0xffff),
				 ANA_PORT_CPU_FWD_BPDU_CFG,
				 port);
		 
		ocelot_write_rix(ocelot, 0, ANA_PGID_PGID, PGID_SRC + port);
	}

	for_each_nonreserved_multicast_dest_pgid(ocelot, i) {
		u32 val = ANA_PGID_PGID_PGID(GENMASK(ocelot->num_phys_ports - 1, 0));

		ocelot_write_rix(ocelot, val, ANA_PGID_PGID, i);
	}

	ocelot_write_rix(ocelot, 0, ANA_PGID_PGID, PGID_BLACKHOLE);

	 
	ocelot_rmw_rix(ocelot, ANA_PGID_PGID_PGID(BIT(ocelot->num_phys_ports)),
		       ANA_PGID_PGID_PGID(BIT(ocelot->num_phys_ports)),
		       ANA_PGID_PGID, PGID_MC);
	ocelot_rmw_rix(ocelot, ANA_PGID_PGID_PGID(BIT(ocelot->num_phys_ports)),
		       ANA_PGID_PGID_PGID(BIT(ocelot->num_phys_ports)),
		       ANA_PGID_PGID, PGID_BC);
	ocelot_write_rix(ocelot, 0, ANA_PGID_PGID, PGID_MCIPV4);
	ocelot_write_rix(ocelot, 0, ANA_PGID_PGID, PGID_MCIPV6);

	 
	ocelot_write_rix(ocelot, QS_INJ_GRP_CFG_BYTE_SWAP |
			 QS_INJ_GRP_CFG_MODE(1), QS_INJ_GRP_CFG, 0);
	ocelot_write_rix(ocelot, QS_XTR_GRP_CFG_BYTE_SWAP |
			 QS_XTR_GRP_CFG_MODE(1), QS_XTR_GRP_CFG, 0);
	ocelot_write(ocelot, ANA_CPUQ_CFG_CPUQ_MIRROR(2) |
		     ANA_CPUQ_CFG_CPUQ_LRN(2) |
		     ANA_CPUQ_CFG_CPUQ_MAC_COPY(2) |
		     ANA_CPUQ_CFG_CPUQ_SRC_COPY(2) |
		     ANA_CPUQ_CFG_CPUQ_LOCKED_PORTMOVE(2) |
		     ANA_CPUQ_CFG_CPUQ_ALLBRIDGE(6) |
		     ANA_CPUQ_CFG_CPUQ_IPMC_CTRL(6) |
		     ANA_CPUQ_CFG_CPUQ_IGMP(6) |
		     ANA_CPUQ_CFG_CPUQ_MLD(6), ANA_CPUQ_CFG);
	for (i = 0; i < 16; i++)
		ocelot_write_rix(ocelot, ANA_CPUQ_8021_CFG_CPUQ_GARP_VAL(6) |
				 ANA_CPUQ_8021_CFG_CPUQ_BPDU_VAL(6),
				 ANA_CPUQ_8021_CFG, i);

	return 0;

err_mm_init:
	ocelot_stats_deinit(ocelot);
err_stats_init:
	destroy_workqueue(ocelot->owq);
	return ret;
}
EXPORT_SYMBOL(ocelot_init);

void ocelot_deinit(struct ocelot *ocelot)
{
	ocelot_stats_deinit(ocelot);
	destroy_workqueue(ocelot->owq);
}
EXPORT_SYMBOL(ocelot_deinit);

void ocelot_deinit_port(struct ocelot *ocelot, int port)
{
	struct ocelot_port *ocelot_port = ocelot->ports[port];

	skb_queue_purge(&ocelot_port->tx_skbs);
}
EXPORT_SYMBOL(ocelot_deinit_port);

MODULE_LICENSE("Dual MIT/GPL");
