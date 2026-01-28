
#ifndef MICROPY_INCLUDED_MIMXRT_ETH_H
#define MICROPY_INCLUDED_MIMXRT_ETH_H

typedef struct _eth_t eth_t;
extern eth_t eth_instance0;
extern eth_t eth_instance1;
void eth_init_0(eth_t *self, int mac_idx, const phy_operations_t *phy_ops, int phy_addr, bool phy_clock);

#if defined(ENET_DUAL_PORT)
void eth_init_1(eth_t *self, int mac_idx, const phy_operations_t *phy_ops, int phy_addr, bool phy_clock);
#endif

void eth_set_trace(eth_t *self, uint32_t value);
struct netif *eth_netif(eth_t *self);
int eth_link_status(eth_t *self);
int eth_start(eth_t *self);
int eth_stop(eth_t *self);
void eth_low_power_mode(eth_t *self, bool enable);

enum {
    PHY_KSZ8081 = 0,
    PHY_DP83825,
    PHY_DP83848,
    PHY_LAN8720,
    PHY_RTL8211F,
};

enum {
    PHY_TX_CLK_IN = false,
    PHY_TX_CLK_OUT = true,
};

#endif 
