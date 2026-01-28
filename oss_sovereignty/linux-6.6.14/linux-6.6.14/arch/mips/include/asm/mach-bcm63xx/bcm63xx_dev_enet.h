#ifndef BCM63XX_DEV_ENET_H_
#define BCM63XX_DEV_ENET_H_
#include <linux/if_ether.h>
#include <linux/init.h>
#include <bcm63xx_regs.h>
struct bcm63xx_enet_platform_data {
	char mac_addr[ETH_ALEN];
	int has_phy;
	int use_internal_phy;
	int phy_id;
	int has_phy_interrupt;
	int phy_interrupt;
	int pause_auto;
	int pause_rx;
	int pause_tx;
	int force_speed_100;
	int force_duplex_full;
	int (*mii_config)(struct net_device *dev, int probe,
			  int (*mii_read)(struct net_device *dev,
					  int phy_id, int reg),
			  void (*mii_write)(struct net_device *dev,
					    int phy_id, int reg, int val));
	u32 dma_chan_en_mask;
	u32 dma_chan_int_mask;
	bool dma_has_sram;
	unsigned int dma_chan_width;
	unsigned int dma_desc_shift;
	int rx_chan;
	int tx_chan;
};
#define ENETSW_MAX_PORT	8
#define ENETSW_PORTS_6328 5  
#define ENETSW_PORTS_6368 6  
#define ENETSW_RGMII_PORT0	4
struct bcm63xx_enetsw_port {
	int		used;
	int		phy_id;
	int		bypass_link;
	int		force_speed;
	int		force_duplex_full;
	const char	*name;
};
struct bcm63xx_enetsw_platform_data {
	char mac_addr[ETH_ALEN];
	int num_ports;
	struct bcm63xx_enetsw_port used_ports[ENETSW_MAX_PORT];
	u32 dma_chan_en_mask;
	u32 dma_chan_int_mask;
	unsigned int dma_chan_width;
	bool dma_has_sram;
};
int __init bcm63xx_enet_register(int unit,
				 const struct bcm63xx_enet_platform_data *pd);
int bcm63xx_enetsw_register(const struct bcm63xx_enetsw_platform_data *pd);
enum bcm63xx_regs_enetdmac {
	ENETDMAC_CHANCFG,
	ENETDMAC_IR,
	ENETDMAC_IRMASK,
	ENETDMAC_MAXBURST,
	ENETDMAC_BUFALLOC,
	ENETDMAC_RSTART,
	ENETDMAC_FC,
	ENETDMAC_LEN,
};
static inline unsigned long bcm63xx_enetdmacreg(enum bcm63xx_regs_enetdmac reg)
{
	extern const unsigned long *bcm63xx_regs_enetdmac;
	return bcm63xx_regs_enetdmac[reg];
}
#endif  
