#ifndef ARC_EMAC_H
#define ARC_EMAC_H
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/clk.h>
#define TXINT_MASK	(1 << 0)	 
#define RXINT_MASK	(1 << 1)	 
#define ERR_MASK	(1 << 2)	 
#define TXCH_MASK	(1 << 3)	 
#define MSER_MASK	(1 << 4)	 
#define RXCR_MASK	(1 << 8)	 
#define RXFR_MASK	(1 << 9)	 
#define RXFL_MASK	(1 << 10)	 
#define MDIO_MASK	(1 << 12)	 
#define TXPL_MASK	(1 << 31)	 
#define EN_MASK		(1 << 0)	 
#define TXRN_MASK	(1 << 3)	 
#define RXRN_MASK	(1 << 4)	 
#define DSBC_MASK	(1 << 8)	 
#define ENFL_MASK	(1 << 10)	 
#define PROM_MASK	(1 << 11)	 
#define OWN_MASK	(1 << 31)	 
#define FIRST_MASK	(1 << 16)	 
#define LAST_MASK	(1 << 17)	 
#define LEN_MASK	0x000007FF	 
#define CRLS		(1 << 21)
#define DEFR		(1 << 22)
#define DROP		(1 << 23)
#define RTRY		(1 << 24)
#define LTCL		(1 << 28)
#define UFLO		(1 << 29)
#define FOR_EMAC	OWN_MASK
#define FOR_CPU		0
enum {
	R_ID = 0,
	R_STATUS,
	R_ENABLE,
	R_CTRL,
	R_POLLRATE,
	R_RXERR,
	R_MISS,
	R_TX_RING,
	R_RX_RING,
	R_ADDRL,
	R_ADDRH,
	R_LAFL,
	R_LAFH,
	R_MDIO,
};
#define TX_TIMEOUT		(400 * HZ / 1000)  
#define ARC_EMAC_NAPI_WEIGHT	40		 
#define EMAC_BUFFER_SIZE	1536		 
struct arc_emac_bd {
	__le32 info;
	dma_addr_t data;
};
#define RX_BD_NUM	128
#define TX_BD_NUM	128
#define RX_RING_SZ	(RX_BD_NUM * sizeof(struct arc_emac_bd))
#define TX_RING_SZ	(TX_BD_NUM * sizeof(struct arc_emac_bd))
struct buffer_state {
	struct sk_buff *skb;
	DEFINE_DMA_UNMAP_ADDR(addr);
	DEFINE_DMA_UNMAP_LEN(len);
};
struct arc_emac_mdio_bus_data {
	struct gpio_desc *reset_gpio;
	int msec;
};
struct arc_emac_priv {
	const char *drv_name;
	void (*set_mac_speed)(void *priv, unsigned int speed);
	struct device *dev;
	struct mii_bus *bus;
	struct arc_emac_mdio_bus_data bus_data;
	void __iomem *regs;
	struct clk *clk;
	struct napi_struct napi;
	struct arc_emac_bd *rxbd;
	struct arc_emac_bd *txbd;
	dma_addr_t rxbd_dma;
	dma_addr_t txbd_dma;
	struct buffer_state rx_buff[RX_BD_NUM];
	struct buffer_state tx_buff[TX_BD_NUM];
	unsigned int txbd_curr;
	unsigned int txbd_dirty;
	unsigned int last_rx_bd;
	unsigned int link;
	unsigned int duplex;
	unsigned int speed;
	unsigned int rx_missed_errors;
};
static inline void arc_reg_set(struct arc_emac_priv *priv, int reg, int value)
{
	iowrite32(value, priv->regs + reg * sizeof(int));
}
static inline unsigned int arc_reg_get(struct arc_emac_priv *priv, int reg)
{
	return ioread32(priv->regs + reg * sizeof(int));
}
static inline void arc_reg_or(struct arc_emac_priv *priv, int reg, int mask)
{
	unsigned int value = arc_reg_get(priv, reg);
	arc_reg_set(priv, reg, value | mask);
}
static inline void arc_reg_clr(struct arc_emac_priv *priv, int reg, int mask)
{
	unsigned int value = arc_reg_get(priv, reg);
	arc_reg_set(priv, reg, value & ~mask);
}
int arc_mdio_probe(struct arc_emac_priv *priv);
int arc_mdio_remove(struct arc_emac_priv *priv);
int arc_emac_probe(struct net_device *ndev, int interface);
void arc_emac_remove(struct net_device *ndev);
#endif  
