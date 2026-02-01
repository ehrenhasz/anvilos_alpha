

#include "lan966x_main.h"

#define VLANACCESS_CMD_IDLE		0
#define VLANACCESS_CMD_READ		1
#define VLANACCESS_CMD_WRITE		2
#define VLANACCESS_CMD_INIT		3

static int lan966x_vlan_get_status(struct lan966x *lan966x)
{
	return lan_rd(lan966x, ANA_VLANACCESS);
}

static int lan966x_vlan_wait_for_completion(struct lan966x *lan966x)
{
	u32 val;

	return readx_poll_timeout(lan966x_vlan_get_status,
		lan966x, val,
		(val & ANA_VLANACCESS_VLAN_TBL_CMD) ==
		VLANACCESS_CMD_IDLE,
		TABLE_UPDATE_SLEEP_US, TABLE_UPDATE_TIMEOUT_US);
}

static void lan966x_vlan_set_mask(struct lan966x *lan966x, u16 vid)
{
	u16 mask = lan966x->vlan_mask[vid];
	bool cpu_dis;

	cpu_dis = !(mask & BIT(CPU_PORT));

	 
	lan_rmw(ANA_VLANTIDX_VLAN_PGID_CPU_DIS_SET(cpu_dis) |
		ANA_VLANTIDX_V_INDEX_SET(vid),
		ANA_VLANTIDX_VLAN_PGID_CPU_DIS |
		ANA_VLANTIDX_V_INDEX,
		lan966x, ANA_VLANTIDX);

	 
	lan_rmw(ANA_VLAN_PORT_MASK_VLAN_PORT_MASK_SET(mask),
		ANA_VLAN_PORT_MASK_VLAN_PORT_MASK,
		lan966x, ANA_VLAN_PORT_MASK);

	 
	lan_rmw(ANA_VLANACCESS_VLAN_TBL_CMD_SET(VLANACCESS_CMD_WRITE),
		ANA_VLANACCESS_VLAN_TBL_CMD,
		lan966x, ANA_VLANACCESS);

	if (lan966x_vlan_wait_for_completion(lan966x))
		dev_err(lan966x->dev, "Vlan set mask failed\n");
}

static void lan966x_vlan_port_add_vlan_mask(struct lan966x_port *port, u16 vid)
{
	struct lan966x *lan966x = port->lan966x;
	u8 p = port->chip_port;

	lan966x->vlan_mask[vid] |= BIT(p);
	lan966x_vlan_set_mask(lan966x, vid);
}

static void lan966x_vlan_port_del_vlan_mask(struct lan966x_port *port, u16 vid)
{
	struct lan966x *lan966x = port->lan966x;
	u8 p = port->chip_port;

	lan966x->vlan_mask[vid] &= ~BIT(p);
	lan966x_vlan_set_mask(lan966x, vid);
}

static bool lan966x_vlan_port_any_vlan_mask(struct lan966x *lan966x, u16 vid)
{
	return !!(lan966x->vlan_mask[vid] & ~BIT(CPU_PORT));
}

static void lan966x_vlan_cpu_add_vlan_mask(struct lan966x *lan966x, u16 vid)
{
	lan966x->vlan_mask[vid] |= BIT(CPU_PORT);
	lan966x_vlan_set_mask(lan966x, vid);
}

static void lan966x_vlan_cpu_del_vlan_mask(struct lan966x *lan966x, u16 vid)
{
	lan966x->vlan_mask[vid] &= ~BIT(CPU_PORT);
	lan966x_vlan_set_mask(lan966x, vid);
}

static void lan966x_vlan_cpu_add_cpu_vlan_mask(struct lan966x *lan966x, u16 vid)
{
	__set_bit(vid, lan966x->cpu_vlan_mask);
}

static void lan966x_vlan_cpu_del_cpu_vlan_mask(struct lan966x *lan966x, u16 vid)
{
	__clear_bit(vid, lan966x->cpu_vlan_mask);
}

bool lan966x_vlan_cpu_member_cpu_vlan_mask(struct lan966x *lan966x, u16 vid)
{
	return test_bit(vid, lan966x->cpu_vlan_mask);
}

static u16 lan966x_vlan_port_get_pvid(struct lan966x_port *port)
{
	struct lan966x *lan966x = port->lan966x;

	if (!(lan966x->bridge_mask & BIT(port->chip_port)))
		return HOST_PVID;

	return port->vlan_aware ? port->pvid : UNAWARE_PVID;
}

int lan966x_vlan_port_set_vid(struct lan966x_port *port, u16 vid,
			      bool pvid, bool untagged)
{
	struct lan966x *lan966x = port->lan966x;

	 
	if (untagged && port->vid != vid) {
		if (port->vid) {
			dev_err(lan966x->dev,
				"Port already has a native VLAN: %d\n",
				port->vid);
			return -EBUSY;
		}
		port->vid = vid;
	}

	 
	if (pvid)
		port->pvid = vid;

	return 0;
}

static void lan966x_vlan_port_remove_vid(struct lan966x_port *port, u16 vid)
{
	if (port->pvid == vid)
		port->pvid = 0;

	if (port->vid == vid)
		port->vid = 0;
}

void lan966x_vlan_port_set_vlan_aware(struct lan966x_port *port,
				      bool vlan_aware)
{
	port->vlan_aware = vlan_aware;
}

void lan966x_vlan_port_apply(struct lan966x_port *port)
{
	struct lan966x *lan966x = port->lan966x;
	u16 pvid;
	u32 val;

	pvid = lan966x_vlan_port_get_pvid(port);

	 
	 
	val = ANA_VLAN_CFG_VLAN_VID_SET(pvid);
	if (port->vlan_aware)
		val |= ANA_VLAN_CFG_VLAN_AWARE_ENA_SET(1) |
		       ANA_VLAN_CFG_VLAN_POP_CNT_SET(1);

	lan_rmw(val,
		ANA_VLAN_CFG_VLAN_VID | ANA_VLAN_CFG_VLAN_AWARE_ENA |
		ANA_VLAN_CFG_VLAN_POP_CNT,
		lan966x, ANA_VLAN_CFG(port->chip_port));

	lan_rmw(DEV_MAC_TAGS_CFG_VLAN_AWR_ENA_SET(port->vlan_aware) |
		DEV_MAC_TAGS_CFG_VLAN_DBL_AWR_ENA_SET(port->vlan_aware),
		DEV_MAC_TAGS_CFG_VLAN_AWR_ENA |
		DEV_MAC_TAGS_CFG_VLAN_DBL_AWR_ENA,
		lan966x, DEV_MAC_TAGS_CFG(port->chip_port));

	 
	val = ANA_DROP_CFG_DROP_MC_SMAC_ENA_SET(1);
	if (port->vlan_aware && !pvid)
		 
		val |= ANA_DROP_CFG_DROP_UNTAGGED_ENA_SET(1) |
		       ANA_DROP_CFG_DROP_PRIO_S_TAGGED_ENA_SET(1) |
		       ANA_DROP_CFG_DROP_PRIO_C_TAGGED_ENA_SET(1);

	lan_wr(val, lan966x, ANA_DROP_CFG(port->chip_port));

	 
	val = REW_TAG_CFG_TAG_TPID_CFG_SET(0);
	if (port->vlan_aware) {
		if (port->vid)
			 
			val |= REW_TAG_CFG_TAG_CFG_SET(1);
		else
			val |= REW_TAG_CFG_TAG_CFG_SET(3);
	}

	 
	lan_rmw(val,
		REW_TAG_CFG_TAG_TPID_CFG | REW_TAG_CFG_TAG_CFG,
		lan966x, REW_TAG_CFG(port->chip_port));

	 
	lan_rmw(REW_PORT_VLAN_CFG_PORT_TPID_SET(ETH_P_8021Q) |
		REW_PORT_VLAN_CFG_PORT_VID_SET(port->vid),
		REW_PORT_VLAN_CFG_PORT_TPID |
		REW_PORT_VLAN_CFG_PORT_VID,
		lan966x, REW_PORT_VLAN_CFG(port->chip_port));
}

void lan966x_vlan_port_add_vlan(struct lan966x_port *port,
				u16 vid,
				bool pvid,
				bool untagged)
{
	struct lan966x *lan966x = port->lan966x;

	 
	if (lan966x_vlan_cpu_member_cpu_vlan_mask(lan966x, vid)) {
		lan966x_vlan_cpu_add_vlan_mask(lan966x, vid);
		lan966x_fdb_write_entries(lan966x, vid);
		lan966x_mdb_write_entries(lan966x, vid);
	}

	lan966x_vlan_port_set_vid(port, vid, pvid, untagged);
	lan966x_vlan_port_add_vlan_mask(port, vid);
	lan966x_vlan_port_apply(port);
}

void lan966x_vlan_port_del_vlan(struct lan966x_port *port, u16 vid)
{
	struct lan966x *lan966x = port->lan966x;

	lan966x_vlan_port_remove_vid(port, vid);
	lan966x_vlan_port_del_vlan_mask(port, vid);
	lan966x_vlan_port_apply(port);

	 
	if (!lan966x_vlan_port_any_vlan_mask(lan966x, vid)) {
		lan966x_vlan_cpu_del_vlan_mask(lan966x, vid);
		lan966x_fdb_erase_entries(lan966x, vid);
		lan966x_mdb_erase_entries(lan966x, vid);
	}
}

void lan966x_vlan_cpu_add_vlan(struct lan966x *lan966x, u16 vid)
{
	 
	if (lan966x_vlan_port_any_vlan_mask(lan966x, vid)) {
		lan966x_vlan_cpu_add_vlan_mask(lan966x, vid);
		lan966x_mdb_write_entries(lan966x, vid);
	}

	lan966x_vlan_cpu_add_cpu_vlan_mask(lan966x, vid);
	lan966x_fdb_write_entries(lan966x, vid);
}

void lan966x_vlan_cpu_del_vlan(struct lan966x *lan966x, u16 vid)
{
	 
	lan966x_vlan_cpu_del_cpu_vlan_mask(lan966x, vid);
	lan966x_vlan_cpu_del_vlan_mask(lan966x, vid);
	lan966x_fdb_erase_entries(lan966x, vid);
	lan966x_mdb_erase_entries(lan966x, vid);
}

void lan966x_vlan_init(struct lan966x *lan966x)
{
	u16 port, vid;

	 
	lan_rmw(ANA_VLANACCESS_VLAN_TBL_CMD_SET(VLANACCESS_CMD_INIT),
		ANA_VLANACCESS_VLAN_TBL_CMD,
		lan966x, ANA_VLANACCESS);
	lan966x_vlan_wait_for_completion(lan966x);

	for (vid = 1; vid < VLAN_N_VID; vid++) {
		lan966x->vlan_mask[vid] = 0;
		lan966x_vlan_set_mask(lan966x, vid);
	}

	 
	lan966x->vlan_mask[HOST_PVID] =
		GENMASK(lan966x->num_phys_ports - 1, 0) | BIT(CPU_PORT);
	lan966x_vlan_set_mask(lan966x, HOST_PVID);

	lan966x->vlan_mask[UNAWARE_PVID] =
		GENMASK(lan966x->num_phys_ports - 1, 0) | BIT(CPU_PORT);
	lan966x_vlan_set_mask(lan966x, UNAWARE_PVID);

	lan966x_vlan_cpu_add_cpu_vlan_mask(lan966x, UNAWARE_PVID);

	 
	lan_wr(ANA_VLAN_CFG_VLAN_VID_SET(0) |
	       ANA_VLAN_CFG_VLAN_AWARE_ENA_SET(1) |
	       ANA_VLAN_CFG_VLAN_POP_CNT_SET(1),
	       lan966x, ANA_VLAN_CFG(CPU_PORT));

	 
	lan_wr(GENMASK(lan966x->num_phys_ports, 0),
	       lan966x, ANA_VLANMASK);

	for (port = 0; port < lan966x->num_phys_ports; port++) {
		lan_wr(0, lan966x, REW_PORT_VLAN_CFG(port));
		lan_wr(0, lan966x, REW_TAG_CFG(port));
	}
}
