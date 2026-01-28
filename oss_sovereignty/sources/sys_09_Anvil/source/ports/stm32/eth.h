
#ifndef MICROPY_INCLUDED_STM32_ETH_H
#define MICROPY_INCLUDED_STM32_ETH_H

enum {
    ETH_PHY_LAN8742 = 0,
    ETH_PHY_LAN8720,
    ETH_PHY_DP83848,
    ETH_PHY_DP83825
};

typedef struct _eth_t eth_t;
extern eth_t eth_instance;

int eth_init(eth_t *self, int mac_idx, uint32_t phy_addr, int phy_type);
void eth_set_trace(eth_t *self, uint32_t value);
struct netif *eth_netif(eth_t *self);
int eth_link_status(eth_t *self);
int eth_start(eth_t *self);
int eth_stop(eth_t *self);
void eth_low_power_mode(eth_t *self, bool enable);

#endif 
