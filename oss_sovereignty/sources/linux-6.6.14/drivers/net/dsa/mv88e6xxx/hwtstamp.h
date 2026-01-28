


#ifndef _MV88E6XXX_HWTSTAMP_H
#define _MV88E6XXX_HWTSTAMP_H

#include "chip.h"



#define MV88E6XXX_PTP_ETHERTYPE	0x00


#define MV88E6XXX_PTP_MSGTYPE			0x01
#define MV88E6XXX_PTP_MSGTYPE_SYNC		0x0001
#define MV88E6XXX_PTP_MSGTYPE_DELAY_REQ		0x0002
#define MV88E6XXX_PTP_MSGTYPE_PDLAY_REQ		0x0004
#define MV88E6XXX_PTP_MSGTYPE_PDLAY_RES		0x0008
#define MV88E6XXX_PTP_MSGTYPE_ALL_EVENT		0x000f


#define MV88E6XXX_PTP_TS_ARRIVAL_PTR	0x02


#define MV88E6165_PTP_CFG			0x05
#define MV88E6165_PTP_CFG_TSPEC_MASK		0xf000
#define MV88E6165_PTP_CFG_DISABLE_TS_OVERWRITE	BIT(1)
#define MV88E6165_PTP_CFG_DISABLE_PTP		BIT(0)


#define MV88E6341_PTP_CFG			0x07
#define MV88E6341_PTP_CFG_UPDATE		0x8000
#define MV88E6341_PTP_CFG_IDX_MASK		0x7f00
#define MV88E6341_PTP_CFG_DATA_MASK		0x00ff
#define MV88E6341_PTP_CFG_MODE_IDX		0x0
#define MV88E6341_PTP_CFG_MODE_TS_AT_PHY	0x00
#define MV88E6341_PTP_CFG_MODE_TS_AT_MAC	0x80


#define MV88E6XXX_PTP_IRQ_STATUS	0x08



#define MV88E6XXX_PORT_PTP_CFG0				0x00
#define MV88E6XXX_PORT_PTP_CFG0_TSPEC_SHIFT		12
#define MV88E6XXX_PORT_PTP_CFG0_TSPEC_MASK		0xf000
#define MV88E6XXX_PORT_PTP_CFG0_TSPEC_1588		0x0000
#define MV88E6XXX_PORT_PTP_CFG0_TSPEC_8021AS		0x1000
#define MV88E6XXX_PORT_PTP_CFG0_DISABLE_TSPEC_MATCH	0x0800
#define MV88E6XXX_PORT_PTP_CFG0_DISABLE_OVERWRITE	0x0002
#define MV88E6XXX_PORT_PTP_CFG0_DISABLE_PTP		0x0001


#define MV88E6XXX_PORT_PTP_CFG1	0x01


#define MV88E6XXX_PORT_PTP_CFG2				0x02
#define MV88E6XXX_PORT_PTP_CFG2_EMBED_ARRIVAL		0x1000
#define MV88E6XXX_PORT_PTP_CFG2_DEP_IRQ_EN		0x0002
#define MV88E6XXX_PORT_PTP_CFG2_ARR_IRQ_EN		0x0001


#define MV88E6XXX_PORT_PTP_LED_CFG	0x03


#define MV88E6XXX_PORT_PTP_ARR0_STS	0x08


#define MV88E6XXX_PORT_PTP_ARR0_TIME_LO	0x09
#define MV88E6XXX_PORT_PTP_ARR0_TIME_HI	0x0a


#define MV88E6XXX_PORT_PTP_ARR0_SEQID	0x0b


#define MV88E6XXX_PORT_PTP_ARR1_STS	0x0c


#define MV88E6XXX_PORT_PTP_ARR1_TIME_LO	0x0d
#define MV88E6XXX_PORT_PTP_ARR1_TIME_HI	0x0e


#define MV88E6XXX_PORT_PTP_ARR1_SEQID	0x0f


#define MV88E6XXX_PORT_PTP_DEP_STS	0x10


#define MV88E6XXX_PORT_PTP_DEP_TIME_LO	0x11
#define MV88E6XXX_PORT_PTP_DEP_TIME_HI	0x12


#define MV88E6XXX_PORT_PTP_DEP_SEQID	0x13


#define MV88E6XXX_PTP_TS_STATUS_MASK		0x0006
#define MV88E6XXX_PTP_TS_STATUS_NORMAL		0x0000
#define MV88E6XXX_PTP_TS_STATUS_OVERWITTEN	0x0002
#define MV88E6XXX_PTP_TS_STATUS_DISCARDED	0x0004
#define MV88E6XXX_PTP_TS_VALID			0x0001

#ifdef CONFIG_NET_DSA_MV88E6XXX_PTP

int mv88e6xxx_port_hwtstamp_set(struct dsa_switch *ds, int port,
				struct ifreq *ifr);
int mv88e6xxx_port_hwtstamp_get(struct dsa_switch *ds, int port,
				struct ifreq *ifr);

bool mv88e6xxx_port_rxtstamp(struct dsa_switch *ds, int port,
			     struct sk_buff *clone, unsigned int type);
void mv88e6xxx_port_txtstamp(struct dsa_switch *ds, int port,
			     struct sk_buff *skb);

int mv88e6xxx_get_ts_info(struct dsa_switch *ds, int port,
			  struct ethtool_ts_info *info);

int mv88e6xxx_hwtstamp_setup(struct mv88e6xxx_chip *chip);
void mv88e6xxx_hwtstamp_free(struct mv88e6xxx_chip *chip);
int mv88e6352_hwtstamp_port_enable(struct mv88e6xxx_chip *chip, int port);
int mv88e6352_hwtstamp_port_disable(struct mv88e6xxx_chip *chip, int port);
int mv88e6165_global_enable(struct mv88e6xxx_chip *chip);
int mv88e6165_global_disable(struct mv88e6xxx_chip *chip);

#else 

static inline int mv88e6xxx_port_hwtstamp_set(struct dsa_switch *ds,
					      int port, struct ifreq *ifr)
{
	return -EOPNOTSUPP;
}

static inline int mv88e6xxx_port_hwtstamp_get(struct dsa_switch *ds,
					      int port, struct ifreq *ifr)
{
	return -EOPNOTSUPP;
}

static inline bool mv88e6xxx_port_rxtstamp(struct dsa_switch *ds, int port,
					   struct sk_buff *clone,
					   unsigned int type)
{
	return false;
}

static inline void mv88e6xxx_port_txtstamp(struct dsa_switch *ds, int port,
					   struct sk_buff *skb)
{
}

static inline int mv88e6xxx_get_ts_info(struct dsa_switch *ds, int port,
					struct ethtool_ts_info *info)
{
	return -EOPNOTSUPP;
}

static inline int mv88e6xxx_hwtstamp_setup(struct mv88e6xxx_chip *chip)
{
	return 0;
}

static inline void mv88e6xxx_hwtstamp_free(struct mv88e6xxx_chip *chip)
{
}

#endif 

#endif 
