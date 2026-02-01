
 

#include <linux/netdevice.h>
#include <net/dsa.h>
#include <linux/if_bridge.h>

#include "qca8k.h"

#define MIB_DESC(_s, _o, _n)	\
	{			\
		.size = (_s),	\
		.offset = (_o),	\
		.name = (_n),	\
	}

const struct qca8k_mib_desc ar8327_mib[] = {
	MIB_DESC(1, 0x00, "RxBroad"),
	MIB_DESC(1, 0x04, "RxPause"),
	MIB_DESC(1, 0x08, "RxMulti"),
	MIB_DESC(1, 0x0c, "RxFcsErr"),
	MIB_DESC(1, 0x10, "RxAlignErr"),
	MIB_DESC(1, 0x14, "RxRunt"),
	MIB_DESC(1, 0x18, "RxFragment"),
	MIB_DESC(1, 0x1c, "Rx64Byte"),
	MIB_DESC(1, 0x20, "Rx128Byte"),
	MIB_DESC(1, 0x24, "Rx256Byte"),
	MIB_DESC(1, 0x28, "Rx512Byte"),
	MIB_DESC(1, 0x2c, "Rx1024Byte"),
	MIB_DESC(1, 0x30, "Rx1518Byte"),
	MIB_DESC(1, 0x34, "RxMaxByte"),
	MIB_DESC(1, 0x38, "RxTooLong"),
	MIB_DESC(2, 0x3c, "RxGoodByte"),
	MIB_DESC(2, 0x44, "RxBadByte"),
	MIB_DESC(1, 0x4c, "RxOverFlow"),
	MIB_DESC(1, 0x50, "Filtered"),
	MIB_DESC(1, 0x54, "TxBroad"),
	MIB_DESC(1, 0x58, "TxPause"),
	MIB_DESC(1, 0x5c, "TxMulti"),
	MIB_DESC(1, 0x60, "TxUnderRun"),
	MIB_DESC(1, 0x64, "Tx64Byte"),
	MIB_DESC(1, 0x68, "Tx128Byte"),
	MIB_DESC(1, 0x6c, "Tx256Byte"),
	MIB_DESC(1, 0x70, "Tx512Byte"),
	MIB_DESC(1, 0x74, "Tx1024Byte"),
	MIB_DESC(1, 0x78, "Tx1518Byte"),
	MIB_DESC(1, 0x7c, "TxMaxByte"),
	MIB_DESC(1, 0x80, "TxOverSize"),
	MIB_DESC(2, 0x84, "TxByte"),
	MIB_DESC(1, 0x8c, "TxCollision"),
	MIB_DESC(1, 0x90, "TxAbortCol"),
	MIB_DESC(1, 0x94, "TxMultiCol"),
	MIB_DESC(1, 0x98, "TxSingleCol"),
	MIB_DESC(1, 0x9c, "TxExcDefer"),
	MIB_DESC(1, 0xa0, "TxDefer"),
	MIB_DESC(1, 0xa4, "TxLateCol"),
	MIB_DESC(1, 0xa8, "RXUnicast"),
	MIB_DESC(1, 0xac, "TXUnicast"),
};

int qca8k_read(struct qca8k_priv *priv, u32 reg, u32 *val)
{
	return regmap_read(priv->regmap, reg, val);
}

int qca8k_write(struct qca8k_priv *priv, u32 reg, u32 val)
{
	return regmap_write(priv->regmap, reg, val);
}

int qca8k_rmw(struct qca8k_priv *priv, u32 reg, u32 mask, u32 write_val)
{
	return regmap_update_bits(priv->regmap, reg, mask, write_val);
}

static const struct regmap_range qca8k_readable_ranges[] = {
	regmap_reg_range(0x0000, 0x00e4),  
	regmap_reg_range(0x0100, 0x0168),  
	regmap_reg_range(0x0200, 0x0270),  
	regmap_reg_range(0x0400, 0x0454),  
	regmap_reg_range(0x0600, 0x0718),  
	regmap_reg_range(0x0800, 0x0b70),  
	regmap_reg_range(0x0c00, 0x0c80),  
	regmap_reg_range(0x0e00, 0x0e98),  
	regmap_reg_range(0x1000, 0x10ac),  
	regmap_reg_range(0x1100, 0x11ac),  
	regmap_reg_range(0x1200, 0x12ac),  
	regmap_reg_range(0x1300, 0x13ac),  
	regmap_reg_range(0x1400, 0x14ac),  
	regmap_reg_range(0x1500, 0x15ac),  
	regmap_reg_range(0x1600, 0x16ac),  
};

const struct regmap_access_table qca8k_readable_table = {
	.yes_ranges = qca8k_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(qca8k_readable_ranges),
};

static int qca8k_busy_wait(struct qca8k_priv *priv, u32 reg, u32 mask)
{
	u32 val;

	return regmap_read_poll_timeout(priv->regmap, reg, val, !(val & mask), 0,
				       QCA8K_BUSY_WAIT_TIMEOUT * USEC_PER_MSEC);
}

static int qca8k_fdb_read(struct qca8k_priv *priv, struct qca8k_fdb *fdb)
{
	u32 reg[QCA8K_ATU_TABLE_SIZE];
	int ret;

	 
	ret = regmap_bulk_read(priv->regmap, QCA8K_REG_ATU_DATA0, reg,
			       QCA8K_ATU_TABLE_SIZE);
	if (ret)
		return ret;

	 
	fdb->vid = FIELD_GET(QCA8K_ATU_VID_MASK, reg[2]);
	 
	fdb->aging = FIELD_GET(QCA8K_ATU_STATUS_MASK, reg[2]);
	 
	fdb->port_mask = FIELD_GET(QCA8K_ATU_PORT_MASK, reg[1]);
	 
	fdb->mac[0] = FIELD_GET(QCA8K_ATU_ADDR0_MASK, reg[1]);
	fdb->mac[1] = FIELD_GET(QCA8K_ATU_ADDR1_MASK, reg[1]);
	fdb->mac[2] = FIELD_GET(QCA8K_ATU_ADDR2_MASK, reg[0]);
	fdb->mac[3] = FIELD_GET(QCA8K_ATU_ADDR3_MASK, reg[0]);
	fdb->mac[4] = FIELD_GET(QCA8K_ATU_ADDR4_MASK, reg[0]);
	fdb->mac[5] = FIELD_GET(QCA8K_ATU_ADDR5_MASK, reg[0]);

	return 0;
}

static void qca8k_fdb_write(struct qca8k_priv *priv, u16 vid, u8 port_mask,
			    const u8 *mac, u8 aging)
{
	u32 reg[QCA8K_ATU_TABLE_SIZE] = { 0 };

	 
	reg[2] = FIELD_PREP(QCA8K_ATU_VID_MASK, vid);
	 
	reg[2] |= FIELD_PREP(QCA8K_ATU_STATUS_MASK, aging);
	 
	reg[1] = FIELD_PREP(QCA8K_ATU_PORT_MASK, port_mask);
	 
	reg[1] |= FIELD_PREP(QCA8K_ATU_ADDR0_MASK, mac[0]);
	reg[1] |= FIELD_PREP(QCA8K_ATU_ADDR1_MASK, mac[1]);
	reg[0] |= FIELD_PREP(QCA8K_ATU_ADDR2_MASK, mac[2]);
	reg[0] |= FIELD_PREP(QCA8K_ATU_ADDR3_MASK, mac[3]);
	reg[0] |= FIELD_PREP(QCA8K_ATU_ADDR4_MASK, mac[4]);
	reg[0] |= FIELD_PREP(QCA8K_ATU_ADDR5_MASK, mac[5]);

	 
	regmap_bulk_write(priv->regmap, QCA8K_REG_ATU_DATA0, reg,
			  QCA8K_ATU_TABLE_SIZE);
}

static int qca8k_fdb_access(struct qca8k_priv *priv, enum qca8k_fdb_cmd cmd,
			    int port)
{
	u32 reg;
	int ret;

	 
	reg = QCA8K_ATU_FUNC_BUSY;
	reg |= cmd;
	if (port >= 0) {
		reg |= QCA8K_ATU_FUNC_PORT_EN;
		reg |= FIELD_PREP(QCA8K_ATU_FUNC_PORT_MASK, port);
	}

	 
	ret = qca8k_write(priv, QCA8K_REG_ATU_FUNC, reg);
	if (ret)
		return ret;

	 
	ret = qca8k_busy_wait(priv, QCA8K_REG_ATU_FUNC, QCA8K_ATU_FUNC_BUSY);
	if (ret)
		return ret;

	 
	if (cmd == QCA8K_FDB_LOAD) {
		ret = qca8k_read(priv, QCA8K_REG_ATU_FUNC, &reg);
		if (ret < 0)
			return ret;
		if (reg & QCA8K_ATU_FUNC_FULL)
			return -1;
	}

	return 0;
}

static int qca8k_fdb_next(struct qca8k_priv *priv, struct qca8k_fdb *fdb,
			  int port)
{
	int ret;

	qca8k_fdb_write(priv, fdb->vid, fdb->port_mask, fdb->mac, fdb->aging);
	ret = qca8k_fdb_access(priv, QCA8K_FDB_NEXT, port);
	if (ret < 0)
		return ret;

	return qca8k_fdb_read(priv, fdb);
}

static int qca8k_fdb_add(struct qca8k_priv *priv, const u8 *mac,
			 u16 port_mask, u16 vid, u8 aging)
{
	int ret;

	mutex_lock(&priv->reg_mutex);
	qca8k_fdb_write(priv, vid, port_mask, mac, aging);
	ret = qca8k_fdb_access(priv, QCA8K_FDB_LOAD, -1);
	mutex_unlock(&priv->reg_mutex);

	return ret;
}

static int qca8k_fdb_del(struct qca8k_priv *priv, const u8 *mac,
			 u16 port_mask, u16 vid)
{
	int ret;

	mutex_lock(&priv->reg_mutex);
	qca8k_fdb_write(priv, vid, port_mask, mac, 0);
	ret = qca8k_fdb_access(priv, QCA8K_FDB_PURGE, -1);
	mutex_unlock(&priv->reg_mutex);

	return ret;
}

void qca8k_fdb_flush(struct qca8k_priv *priv)
{
	mutex_lock(&priv->reg_mutex);
	qca8k_fdb_access(priv, QCA8K_FDB_FLUSH, -1);
	mutex_unlock(&priv->reg_mutex);
}

static int qca8k_fdb_search_and_insert(struct qca8k_priv *priv, u8 port_mask,
				       const u8 *mac, u16 vid, u8 aging)
{
	struct qca8k_fdb fdb = { 0 };
	int ret;

	mutex_lock(&priv->reg_mutex);

	qca8k_fdb_write(priv, vid, 0, mac, 0);
	ret = qca8k_fdb_access(priv, QCA8K_FDB_SEARCH, -1);
	if (ret < 0)
		goto exit;

	ret = qca8k_fdb_read(priv, &fdb);
	if (ret < 0)
		goto exit;

	 
	if (fdb.aging) {
		ret = qca8k_fdb_access(priv, QCA8K_FDB_PURGE, -1);
		if (ret)
			goto exit;
	} else {
		fdb.aging = aging;
	}

	 
	fdb.port_mask |= port_mask;

	qca8k_fdb_write(priv, vid, fdb.port_mask, mac, fdb.aging);
	ret = qca8k_fdb_access(priv, QCA8K_FDB_LOAD, -1);

exit:
	mutex_unlock(&priv->reg_mutex);
	return ret;
}

static int qca8k_fdb_search_and_del(struct qca8k_priv *priv, u8 port_mask,
				    const u8 *mac, u16 vid)
{
	struct qca8k_fdb fdb = { 0 };
	int ret;

	mutex_lock(&priv->reg_mutex);

	qca8k_fdb_write(priv, vid, 0, mac, 0);
	ret = qca8k_fdb_access(priv, QCA8K_FDB_SEARCH, -1);
	if (ret < 0)
		goto exit;

	ret = qca8k_fdb_read(priv, &fdb);
	if (ret < 0)
		goto exit;

	 
	if (!fdb.aging) {
		ret = -EINVAL;
		goto exit;
	}

	ret = qca8k_fdb_access(priv, QCA8K_FDB_PURGE, -1);
	if (ret)
		goto exit;

	 
	if (fdb.port_mask == port_mask)
		goto exit;

	 
	fdb.port_mask &= ~port_mask;

	qca8k_fdb_write(priv, vid, fdb.port_mask, mac, fdb.aging);
	ret = qca8k_fdb_access(priv, QCA8K_FDB_LOAD, -1);

exit:
	mutex_unlock(&priv->reg_mutex);
	return ret;
}

static int qca8k_vlan_access(struct qca8k_priv *priv,
			     enum qca8k_vlan_cmd cmd, u16 vid)
{
	u32 reg;
	int ret;

	 
	reg = QCA8K_VTU_FUNC1_BUSY;
	reg |= cmd;
	reg |= FIELD_PREP(QCA8K_VTU_FUNC1_VID_MASK, vid);

	 
	ret = qca8k_write(priv, QCA8K_REG_VTU_FUNC1, reg);
	if (ret)
		return ret;

	 
	ret = qca8k_busy_wait(priv, QCA8K_REG_VTU_FUNC1, QCA8K_VTU_FUNC1_BUSY);
	if (ret)
		return ret;

	 
	if (cmd == QCA8K_VLAN_LOAD) {
		ret = qca8k_read(priv, QCA8K_REG_VTU_FUNC1, &reg);
		if (ret < 0)
			return ret;
		if (reg & QCA8K_VTU_FUNC1_FULL)
			return -ENOMEM;
	}

	return 0;
}

static int qca8k_vlan_add(struct qca8k_priv *priv, u8 port, u16 vid,
			  bool untagged)
{
	u32 reg;
	int ret;

	 
	if (vid == 0)
		return 0;

	mutex_lock(&priv->reg_mutex);
	ret = qca8k_vlan_access(priv, QCA8K_VLAN_READ, vid);
	if (ret < 0)
		goto out;

	ret = qca8k_read(priv, QCA8K_REG_VTU_FUNC0, &reg);
	if (ret < 0)
		goto out;
	reg |= QCA8K_VTU_FUNC0_VALID | QCA8K_VTU_FUNC0_IVL_EN;
	reg &= ~QCA8K_VTU_FUNC0_EG_MODE_PORT_MASK(port);
	if (untagged)
		reg |= QCA8K_VTU_FUNC0_EG_MODE_PORT_UNTAG(port);
	else
		reg |= QCA8K_VTU_FUNC0_EG_MODE_PORT_TAG(port);

	ret = qca8k_write(priv, QCA8K_REG_VTU_FUNC0, reg);
	if (ret)
		goto out;
	ret = qca8k_vlan_access(priv, QCA8K_VLAN_LOAD, vid);

out:
	mutex_unlock(&priv->reg_mutex);

	return ret;
}

static int qca8k_vlan_del(struct qca8k_priv *priv, u8 port, u16 vid)
{
	u32 reg, mask;
	int ret, i;
	bool del;

	mutex_lock(&priv->reg_mutex);
	ret = qca8k_vlan_access(priv, QCA8K_VLAN_READ, vid);
	if (ret < 0)
		goto out;

	ret = qca8k_read(priv, QCA8K_REG_VTU_FUNC0, &reg);
	if (ret < 0)
		goto out;
	reg &= ~QCA8K_VTU_FUNC0_EG_MODE_PORT_MASK(port);
	reg |= QCA8K_VTU_FUNC0_EG_MODE_PORT_NOT(port);

	 
	del = true;
	for (i = 0; i < QCA8K_NUM_PORTS; i++) {
		mask = QCA8K_VTU_FUNC0_EG_MODE_PORT_NOT(i);

		if ((reg & mask) != mask) {
			del = false;
			break;
		}
	}

	if (del) {
		ret = qca8k_vlan_access(priv, QCA8K_VLAN_PURGE, vid);
	} else {
		ret = qca8k_write(priv, QCA8K_REG_VTU_FUNC0, reg);
		if (ret)
			goto out;
		ret = qca8k_vlan_access(priv, QCA8K_VLAN_LOAD, vid);
	}

out:
	mutex_unlock(&priv->reg_mutex);

	return ret;
}

int qca8k_mib_init(struct qca8k_priv *priv)
{
	int ret;

	mutex_lock(&priv->reg_mutex);
	ret = regmap_update_bits(priv->regmap, QCA8K_REG_MIB,
				 QCA8K_MIB_FUNC | QCA8K_MIB_BUSY,
				 FIELD_PREP(QCA8K_MIB_FUNC, QCA8K_MIB_FLUSH) |
				 QCA8K_MIB_BUSY);
	if (ret)
		goto exit;

	ret = qca8k_busy_wait(priv, QCA8K_REG_MIB, QCA8K_MIB_BUSY);
	if (ret)
		goto exit;

	ret = regmap_set_bits(priv->regmap, QCA8K_REG_MIB, QCA8K_MIB_CPU_KEEP);
	if (ret)
		goto exit;

	ret = qca8k_write(priv, QCA8K_REG_MODULE_EN, QCA8K_MODULE_EN_MIB);

exit:
	mutex_unlock(&priv->reg_mutex);
	return ret;
}

void qca8k_port_set_status(struct qca8k_priv *priv, int port, int enable)
{
	u32 mask = QCA8K_PORT_STATUS_TXMAC | QCA8K_PORT_STATUS_RXMAC;

	 
	if (port > 0 && port < 6)
		mask |= QCA8K_PORT_STATUS_LINK_AUTO;

	if (enable)
		regmap_set_bits(priv->regmap, QCA8K_REG_PORT_STATUS(port), mask);
	else
		regmap_clear_bits(priv->regmap, QCA8K_REG_PORT_STATUS(port), mask);
}

void qca8k_get_strings(struct dsa_switch *ds, int port, u32 stringset,
		       uint8_t *data)
{
	struct qca8k_priv *priv = ds->priv;
	int i;

	if (stringset != ETH_SS_STATS)
		return;

	for (i = 0; i < priv->info->mib_count; i++)
		strncpy(data + i * ETH_GSTRING_LEN, ar8327_mib[i].name,
			ETH_GSTRING_LEN);
}

void qca8k_get_ethtool_stats(struct dsa_switch *ds, int port,
			     uint64_t *data)
{
	struct qca8k_priv *priv = ds->priv;
	const struct qca8k_mib_desc *mib;
	u32 reg, i, val;
	u32 hi = 0;
	int ret;

	if (priv->mgmt_master && priv->info->ops->autocast_mib &&
	    priv->info->ops->autocast_mib(ds, port, data) > 0)
		return;

	for (i = 0; i < priv->info->mib_count; i++) {
		mib = &ar8327_mib[i];
		reg = QCA8K_PORT_MIB_COUNTER(port) + mib->offset;

		ret = qca8k_read(priv, reg, &val);
		if (ret < 0)
			continue;

		if (mib->size == 2) {
			ret = qca8k_read(priv, reg + 4, &hi);
			if (ret < 0)
				continue;
		}

		data[i] = val;
		if (mib->size == 2)
			data[i] |= (u64)hi << 32;
	}
}

int qca8k_get_sset_count(struct dsa_switch *ds, int port, int sset)
{
	struct qca8k_priv *priv = ds->priv;

	if (sset != ETH_SS_STATS)
		return 0;

	return priv->info->mib_count;
}

int qca8k_set_mac_eee(struct dsa_switch *ds, int port,
		      struct ethtool_eee *eee)
{
	u32 lpi_en = QCA8K_REG_EEE_CTRL_LPI_EN(port);
	struct qca8k_priv *priv = ds->priv;
	u32 reg;
	int ret;

	mutex_lock(&priv->reg_mutex);
	ret = qca8k_read(priv, QCA8K_REG_EEE_CTRL, &reg);
	if (ret < 0)
		goto exit;

	if (eee->eee_enabled)
		reg |= lpi_en;
	else
		reg &= ~lpi_en;
	ret = qca8k_write(priv, QCA8K_REG_EEE_CTRL, reg);

exit:
	mutex_unlock(&priv->reg_mutex);
	return ret;
}

int qca8k_get_mac_eee(struct dsa_switch *ds, int port,
		      struct ethtool_eee *e)
{
	 
	return 0;
}

static int qca8k_port_configure_learning(struct dsa_switch *ds, int port,
					 bool learning)
{
	struct qca8k_priv *priv = ds->priv;

	if (learning)
		return regmap_set_bits(priv->regmap,
				       QCA8K_PORT_LOOKUP_CTRL(port),
				       QCA8K_PORT_LOOKUP_LEARN);
	else
		return regmap_clear_bits(priv->regmap,
					 QCA8K_PORT_LOOKUP_CTRL(port),
					 QCA8K_PORT_LOOKUP_LEARN);
}

void qca8k_port_stp_state_set(struct dsa_switch *ds, int port, u8 state)
{
	struct dsa_port *dp = dsa_to_port(ds, port);
	struct qca8k_priv *priv = ds->priv;
	bool learning = false;
	u32 stp_state;

	switch (state) {
	case BR_STATE_DISABLED:
		stp_state = QCA8K_PORT_LOOKUP_STATE_DISABLED;
		break;
	case BR_STATE_BLOCKING:
		stp_state = QCA8K_PORT_LOOKUP_STATE_BLOCKING;
		break;
	case BR_STATE_LISTENING:
		stp_state = QCA8K_PORT_LOOKUP_STATE_LISTENING;
		break;
	case BR_STATE_LEARNING:
		stp_state = QCA8K_PORT_LOOKUP_STATE_LEARNING;
		learning = dp->learning;
		break;
	case BR_STATE_FORWARDING:
		learning = dp->learning;
		fallthrough;
	default:
		stp_state = QCA8K_PORT_LOOKUP_STATE_FORWARD;
		break;
	}

	qca8k_rmw(priv, QCA8K_PORT_LOOKUP_CTRL(port),
		  QCA8K_PORT_LOOKUP_STATE_MASK, stp_state);

	qca8k_port_configure_learning(ds, port, learning);
}

int qca8k_port_pre_bridge_flags(struct dsa_switch *ds, int port,
				struct switchdev_brport_flags flags,
				struct netlink_ext_ack *extack)
{
	if (flags.mask & ~BR_LEARNING)
		return -EINVAL;

	return 0;
}

int qca8k_port_bridge_flags(struct dsa_switch *ds, int port,
			    struct switchdev_brport_flags flags,
			    struct netlink_ext_ack *extack)
{
	int ret;

	if (flags.mask & BR_LEARNING) {
		ret = qca8k_port_configure_learning(ds, port,
						    flags.val & BR_LEARNING);
		if (ret)
			return ret;
	}

	return 0;
}

int qca8k_port_bridge_join(struct dsa_switch *ds, int port,
			   struct dsa_bridge bridge,
			   bool *tx_fwd_offload,
			   struct netlink_ext_ack *extack)
{
	struct qca8k_priv *priv = ds->priv;
	int port_mask, cpu_port;
	int i, ret;

	cpu_port = dsa_to_port(ds, port)->cpu_dp->index;
	port_mask = BIT(cpu_port);

	for (i = 0; i < QCA8K_NUM_PORTS; i++) {
		if (dsa_is_cpu_port(ds, i))
			continue;
		if (!dsa_port_offloads_bridge(dsa_to_port(ds, i), &bridge))
			continue;
		 
		ret = regmap_set_bits(priv->regmap,
				      QCA8K_PORT_LOOKUP_CTRL(i),
				      BIT(port));
		if (ret)
			return ret;
		if (i != port)
			port_mask |= BIT(i);
	}

	 
	ret = qca8k_rmw(priv, QCA8K_PORT_LOOKUP_CTRL(port),
			QCA8K_PORT_LOOKUP_MEMBER, port_mask);

	return ret;
}

void qca8k_port_bridge_leave(struct dsa_switch *ds, int port,
			     struct dsa_bridge bridge)
{
	struct qca8k_priv *priv = ds->priv;
	int cpu_port, i;

	cpu_port = dsa_to_port(ds, port)->cpu_dp->index;

	for (i = 0; i < QCA8K_NUM_PORTS; i++) {
		if (dsa_is_cpu_port(ds, i))
			continue;
		if (!dsa_port_offloads_bridge(dsa_to_port(ds, i), &bridge))
			continue;
		 
		regmap_clear_bits(priv->regmap,
				  QCA8K_PORT_LOOKUP_CTRL(i),
				  BIT(port));
	}

	 
	qca8k_rmw(priv, QCA8K_PORT_LOOKUP_CTRL(port),
		  QCA8K_PORT_LOOKUP_MEMBER, BIT(cpu_port));
}

void qca8k_port_fast_age(struct dsa_switch *ds, int port)
{
	struct qca8k_priv *priv = ds->priv;

	mutex_lock(&priv->reg_mutex);
	qca8k_fdb_access(priv, QCA8K_FDB_FLUSH_PORT, port);
	mutex_unlock(&priv->reg_mutex);
}

int qca8k_set_ageing_time(struct dsa_switch *ds, unsigned int msecs)
{
	struct qca8k_priv *priv = ds->priv;
	unsigned int secs = msecs / 1000;
	u32 val;

	 
	val = secs / 7;

	 
	if (!val)
		val = 1;

	return regmap_update_bits(priv->regmap, QCA8K_REG_ATU_CTRL,
				  QCA8K_ATU_AGE_TIME_MASK,
				  QCA8K_ATU_AGE_TIME(val));
}

int qca8k_port_enable(struct dsa_switch *ds, int port,
		      struct phy_device *phy)
{
	struct qca8k_priv *priv = ds->priv;

	qca8k_port_set_status(priv, port, 1);
	priv->port_enabled_map |= BIT(port);

	if (dsa_is_user_port(ds, port))
		phy_support_asym_pause(phy);

	return 0;
}

void qca8k_port_disable(struct dsa_switch *ds, int port)
{
	struct qca8k_priv *priv = ds->priv;

	qca8k_port_set_status(priv, port, 0);
	priv->port_enabled_map &= ~BIT(port);
}

int qca8k_port_change_mtu(struct dsa_switch *ds, int port, int new_mtu)
{
	struct qca8k_priv *priv = ds->priv;
	int ret;

	 
	if (!dsa_is_cpu_port(ds, port))
		return 0;

	 
	if (priv->port_enabled_map & BIT(0))
		qca8k_port_set_status(priv, 0, 0);

	if (priv->port_enabled_map & BIT(6))
		qca8k_port_set_status(priv, 6, 0);

	 
	ret = qca8k_write(priv, QCA8K_MAX_FRAME_SIZE, new_mtu +
			  ETH_HLEN + ETH_FCS_LEN);

	if (priv->port_enabled_map & BIT(0))
		qca8k_port_set_status(priv, 0, 1);

	if (priv->port_enabled_map & BIT(6))
		qca8k_port_set_status(priv, 6, 1);

	return ret;
}

int qca8k_port_max_mtu(struct dsa_switch *ds, int port)
{
	return QCA8K_MAX_MTU;
}

int qca8k_port_fdb_insert(struct qca8k_priv *priv, const u8 *addr,
			  u16 port_mask, u16 vid)
{
	 
	if (!vid)
		vid = QCA8K_PORT_VID_DEF;

	return qca8k_fdb_add(priv, addr, port_mask, vid,
			     QCA8K_ATU_STATUS_STATIC);
}

int qca8k_port_fdb_add(struct dsa_switch *ds, int port,
		       const unsigned char *addr, u16 vid,
		       struct dsa_db db)
{
	struct qca8k_priv *priv = ds->priv;
	u16 port_mask = BIT(port);

	return qca8k_port_fdb_insert(priv, addr, port_mask, vid);
}

int qca8k_port_fdb_del(struct dsa_switch *ds, int port,
		       const unsigned char *addr, u16 vid,
		       struct dsa_db db)
{
	struct qca8k_priv *priv = ds->priv;
	u16 port_mask = BIT(port);

	if (!vid)
		vid = QCA8K_PORT_VID_DEF;

	return qca8k_fdb_del(priv, addr, port_mask, vid);
}

int qca8k_port_fdb_dump(struct dsa_switch *ds, int port,
			dsa_fdb_dump_cb_t *cb, void *data)
{
	struct qca8k_priv *priv = ds->priv;
	struct qca8k_fdb _fdb = { 0 };
	int cnt = QCA8K_NUM_FDB_RECORDS;
	bool is_static;
	int ret = 0;

	mutex_lock(&priv->reg_mutex);
	while (cnt-- && !qca8k_fdb_next(priv, &_fdb, port)) {
		if (!_fdb.aging)
			break;
		is_static = (_fdb.aging == QCA8K_ATU_STATUS_STATIC);
		ret = cb(_fdb.mac, _fdb.vid, is_static, data);
		if (ret)
			break;
	}
	mutex_unlock(&priv->reg_mutex);

	return 0;
}

int qca8k_port_mdb_add(struct dsa_switch *ds, int port,
		       const struct switchdev_obj_port_mdb *mdb,
		       struct dsa_db db)
{
	struct qca8k_priv *priv = ds->priv;
	const u8 *addr = mdb->addr;
	u16 vid = mdb->vid;

	if (!vid)
		vid = QCA8K_PORT_VID_DEF;

	return qca8k_fdb_search_and_insert(priv, BIT(port), addr, vid,
					   QCA8K_ATU_STATUS_STATIC);
}

int qca8k_port_mdb_del(struct dsa_switch *ds, int port,
		       const struct switchdev_obj_port_mdb *mdb,
		       struct dsa_db db)
{
	struct qca8k_priv *priv = ds->priv;
	const u8 *addr = mdb->addr;
	u16 vid = mdb->vid;

	if (!vid)
		vid = QCA8K_PORT_VID_DEF;

	return qca8k_fdb_search_and_del(priv, BIT(port), addr, vid);
}

int qca8k_port_mirror_add(struct dsa_switch *ds, int port,
			  struct dsa_mall_mirror_tc_entry *mirror,
			  bool ingress, struct netlink_ext_ack *extack)
{
	struct qca8k_priv *priv = ds->priv;
	int monitor_port, ret;
	u32 reg, val;

	 
	if ((ingress ? priv->mirror_rx : priv->mirror_tx) & BIT(port))
		return -EEXIST;

	ret = regmap_read(priv->regmap, QCA8K_REG_GLOBAL_FW_CTRL0, &val);
	if (ret)
		return ret;

	 
	monitor_port = FIELD_GET(QCA8K_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM, val);
	if (monitor_port != 0xF && monitor_port != mirror->to_local_port)
		return -EEXIST;

	 
	val = FIELD_PREP(QCA8K_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM,
			 mirror->to_local_port);
	ret = regmap_update_bits(priv->regmap, QCA8K_REG_GLOBAL_FW_CTRL0,
				 QCA8K_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM, val);
	if (ret)
		return ret;

	if (ingress) {
		reg = QCA8K_PORT_LOOKUP_CTRL(port);
		val = QCA8K_PORT_LOOKUP_ING_MIRROR_EN;
	} else {
		reg = QCA8K_REG_PORT_HOL_CTRL1(port);
		val = QCA8K_PORT_HOL_CTRL1_EG_MIRROR_EN;
	}

	ret = regmap_update_bits(priv->regmap, reg, val, val);
	if (ret)
		return ret;

	 
	if (ingress)
		priv->mirror_rx |= BIT(port);
	else
		priv->mirror_tx |= BIT(port);

	return 0;
}

void qca8k_port_mirror_del(struct dsa_switch *ds, int port,
			   struct dsa_mall_mirror_tc_entry *mirror)
{
	struct qca8k_priv *priv = ds->priv;
	u32 reg, val;
	int ret;

	if (mirror->ingress) {
		reg = QCA8K_PORT_LOOKUP_CTRL(port);
		val = QCA8K_PORT_LOOKUP_ING_MIRROR_EN;
	} else {
		reg = QCA8K_REG_PORT_HOL_CTRL1(port);
		val = QCA8K_PORT_HOL_CTRL1_EG_MIRROR_EN;
	}

	ret = regmap_clear_bits(priv->regmap, reg, val);
	if (ret)
		goto err;

	if (mirror->ingress)
		priv->mirror_rx &= ~BIT(port);
	else
		priv->mirror_tx &= ~BIT(port);

	 
	if (!priv->mirror_rx && !priv->mirror_tx) {
		val = FIELD_PREP(QCA8K_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM, 0xF);
		ret = regmap_update_bits(priv->regmap, QCA8K_REG_GLOBAL_FW_CTRL0,
					 QCA8K_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM, val);
		if (ret)
			goto err;
	}
err:
	dev_err(priv->dev, "Failed to del mirror port from %d", port);
}

int qca8k_port_vlan_filtering(struct dsa_switch *ds, int port,
			      bool vlan_filtering,
			      struct netlink_ext_ack *extack)
{
	struct qca8k_priv *priv = ds->priv;
	int ret;

	if (vlan_filtering) {
		ret = qca8k_rmw(priv, QCA8K_PORT_LOOKUP_CTRL(port),
				QCA8K_PORT_LOOKUP_VLAN_MODE_MASK,
				QCA8K_PORT_LOOKUP_VLAN_MODE_SECURE);
	} else {
		ret = qca8k_rmw(priv, QCA8K_PORT_LOOKUP_CTRL(port),
				QCA8K_PORT_LOOKUP_VLAN_MODE_MASK,
				QCA8K_PORT_LOOKUP_VLAN_MODE_NONE);
	}

	return ret;
}

int qca8k_port_vlan_add(struct dsa_switch *ds, int port,
			const struct switchdev_obj_port_vlan *vlan,
			struct netlink_ext_ack *extack)
{
	bool untagged = vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED;
	bool pvid = vlan->flags & BRIDGE_VLAN_INFO_PVID;
	struct qca8k_priv *priv = ds->priv;
	int ret;

	ret = qca8k_vlan_add(priv, port, vlan->vid, untagged);
	if (ret) {
		dev_err(priv->dev, "Failed to add VLAN to port %d (%d)", port, ret);
		return ret;
	}

	if (pvid) {
		ret = qca8k_rmw(priv, QCA8K_EGRESS_VLAN(port),
				QCA8K_EGREES_VLAN_PORT_MASK(port),
				QCA8K_EGREES_VLAN_PORT(port, vlan->vid));
		if (ret)
			return ret;

		ret = qca8k_write(priv, QCA8K_REG_PORT_VLAN_CTRL0(port),
				  QCA8K_PORT_VLAN_CVID(vlan->vid) |
				  QCA8K_PORT_VLAN_SVID(vlan->vid));
	}

	return ret;
}

int qca8k_port_vlan_del(struct dsa_switch *ds, int port,
			const struct switchdev_obj_port_vlan *vlan)
{
	struct qca8k_priv *priv = ds->priv;
	int ret;

	ret = qca8k_vlan_del(priv, port, vlan->vid);
	if (ret)
		dev_err(priv->dev, "Failed to delete VLAN from port %d (%d)", port, ret);

	return ret;
}

static bool qca8k_lag_can_offload(struct dsa_switch *ds,
				  struct dsa_lag lag,
				  struct netdev_lag_upper_info *info,
				  struct netlink_ext_ack *extack)
{
	struct dsa_port *dp;
	int members = 0;

	if (!lag.id)
		return false;

	dsa_lag_foreach_port(dp, ds->dst, &lag)
		 
		members++;

	if (members > QCA8K_NUM_PORTS_FOR_LAG) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Cannot offload more than 4 LAG ports");
		return false;
	}

	if (info->tx_type != NETDEV_LAG_TX_TYPE_HASH) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Can only offload LAG using hash TX type");
		return false;
	}

	if (info->hash_type != NETDEV_LAG_HASH_L2 &&
	    info->hash_type != NETDEV_LAG_HASH_L23) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Can only offload L2 or L2+L3 TX hash");
		return false;
	}

	return true;
}

static int qca8k_lag_setup_hash(struct dsa_switch *ds,
				struct dsa_lag lag,
				struct netdev_lag_upper_info *info)
{
	struct net_device *lag_dev = lag.dev;
	struct qca8k_priv *priv = ds->priv;
	bool unique_lag = true;
	unsigned int i;
	u32 hash = 0;

	switch (info->hash_type) {
	case NETDEV_LAG_HASH_L23:
		hash |= QCA8K_TRUNK_HASH_SIP_EN;
		hash |= QCA8K_TRUNK_HASH_DIP_EN;
		fallthrough;
	case NETDEV_LAG_HASH_L2:
		hash |= QCA8K_TRUNK_HASH_SA_EN;
		hash |= QCA8K_TRUNK_HASH_DA_EN;
		break;
	default:  
		return -EOPNOTSUPP;
	}

	 
	dsa_lags_foreach_id(i, ds->dst)
		if (i != lag.id && dsa_lag_by_id(ds->dst, i)) {
			unique_lag = false;
			break;
		}

	 
	if (unique_lag) {
		priv->lag_hash_mode = hash;
	} else if (priv->lag_hash_mode != hash) {
		netdev_err(lag_dev, "Error: Mismatched Hash Mode across different lag is not supported\n");
		return -EOPNOTSUPP;
	}

	return regmap_update_bits(priv->regmap, QCA8K_TRUNK_HASH_EN_CTRL,
				  QCA8K_TRUNK_HASH_MASK, hash);
}

static int qca8k_lag_refresh_portmap(struct dsa_switch *ds, int port,
				     struct dsa_lag lag, bool delete)
{
	struct qca8k_priv *priv = ds->priv;
	int ret, id, i;
	u32 val;

	 
	id = lag.id - 1;

	 
	ret = regmap_read(priv->regmap, QCA8K_REG_GOL_TRUNK_CTRL0, &val);
	if (ret)
		return ret;

	 
	val >>= QCA8K_REG_GOL_TRUNK_SHIFT(id);
	val &= QCA8K_REG_GOL_TRUNK_MEMBER_MASK;
	if (delete)
		val &= ~BIT(port);
	else
		val |= BIT(port);

	 
	ret = regmap_update_bits(priv->regmap, QCA8K_REG_GOL_TRUNK_CTRL0,
				 QCA8K_REG_GOL_TRUNK_MEMBER(id) |
				 QCA8K_REG_GOL_TRUNK_EN(id),
				 !val << QCA8K_REG_GOL_TRUNK_SHIFT(id) |
				 val << QCA8K_REG_GOL_TRUNK_SHIFT(id));

	 
	for (i = 0; i < QCA8K_NUM_PORTS_FOR_LAG; i++) {
		ret = regmap_read(priv->regmap, QCA8K_REG_GOL_TRUNK_CTRL(id), &val);
		if (ret)
			return ret;

		val >>= QCA8K_REG_GOL_TRUNK_ID_MEM_ID_SHIFT(id, i);
		val &= QCA8K_REG_GOL_TRUNK_ID_MEM_ID_MASK;

		if (delete) {
			 
			if (val != QCA8K_REG_GOL_TRUNK_ID_MEM_ID_EN_MASK)
				continue;

			val &= QCA8K_REG_GOL_TRUNK_ID_MEM_ID_PORT_MASK;
			if (val != port)
				continue;
		} else {
			 
			if (val == QCA8K_REG_GOL_TRUNK_ID_MEM_ID_EN_MASK)
				continue;
		}

		 
		break;
	}

	 
	return regmap_update_bits(priv->regmap, QCA8K_REG_GOL_TRUNK_CTRL(id),
				  QCA8K_REG_GOL_TRUNK_ID_MEM_ID_EN(id, i) |
				  QCA8K_REG_GOL_TRUNK_ID_MEM_ID_PORT(id, i),
				  !delete << QCA8K_REG_GOL_TRUNK_ID_MEM_ID_SHIFT(id, i) |
				  port << QCA8K_REG_GOL_TRUNK_ID_MEM_ID_SHIFT(id, i));
}

int qca8k_port_lag_join(struct dsa_switch *ds, int port, struct dsa_lag lag,
			struct netdev_lag_upper_info *info,
			struct netlink_ext_ack *extack)
{
	int ret;

	if (!qca8k_lag_can_offload(ds, lag, info, extack))
		return -EOPNOTSUPP;

	ret = qca8k_lag_setup_hash(ds, lag, info);
	if (ret)
		return ret;

	return qca8k_lag_refresh_portmap(ds, port, lag, false);
}

int qca8k_port_lag_leave(struct dsa_switch *ds, int port,
			 struct dsa_lag lag)
{
	return qca8k_lag_refresh_portmap(ds, port, lag, true);
}

int qca8k_read_switch_id(struct qca8k_priv *priv)
{
	u32 val;
	u8 id;
	int ret;

	if (!priv->info)
		return -ENODEV;

	ret = qca8k_read(priv, QCA8K_REG_MASK_CTRL, &val);
	if (ret < 0)
		return -ENODEV;

	id = QCA8K_MASK_CTRL_DEVICE_ID(val);
	if (id != priv->info->id) {
		dev_err(priv->dev,
			"Switch id detected %x but expected %x",
			id, priv->info->id);
		return -ENODEV;
	}

	priv->switch_id = id;

	 
	priv->switch_revision = QCA8K_MASK_CTRL_REV_ID(val);

	return 0;
}
