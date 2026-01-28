


#ifndef EF4_MDIO_10G_H
#define EF4_MDIO_10G_H

#include <linux/mdio.h>



#include "efx.h"

static inline unsigned ef4_mdio_id_rev(u32 id) { return id & 0xf; }
static inline unsigned ef4_mdio_id_model(u32 id) { return (id >> 4) & 0x3f; }
unsigned ef4_mdio_id_oui(u32 id);

static inline int ef4_mdio_read(struct ef4_nic *efx, int devad, int addr)
{
	return efx->mdio.mdio_read(efx->net_dev, efx->mdio.prtad, devad, addr);
}

static inline void
ef4_mdio_write(struct ef4_nic *efx, int devad, int addr, int value)
{
	efx->mdio.mdio_write(efx->net_dev, efx->mdio.prtad, devad, addr, value);
}

static inline u32 ef4_mdio_read_id(struct ef4_nic *efx, int mmd)
{
	u16 id_low = ef4_mdio_read(efx, mmd, MDIO_DEVID2);
	u16 id_hi = ef4_mdio_read(efx, mmd, MDIO_DEVID1);
	return (id_hi << 16) | (id_low);
}

static inline bool ef4_mdio_phyxgxs_lane_sync(struct ef4_nic *efx)
{
	int i, lane_status;
	bool sync;

	for (i = 0; i < 2; ++i)
		lane_status = ef4_mdio_read(efx, MDIO_MMD_PHYXS,
					    MDIO_PHYXS_LNSTAT);

	sync = !!(lane_status & MDIO_PHYXS_LNSTAT_ALIGN);
	if (!sync)
		netif_dbg(efx, hw, efx->net_dev, "XGXS lane status: %x\n",
			  lane_status);
	return sync;
}

const char *ef4_mdio_mmd_name(int mmd);


int ef4_mdio_reset_mmd(struct ef4_nic *efx, int mmd, int spins, int spintime);


int ef4_mdio_check_mmds(struct ef4_nic *efx, unsigned int mmd_mask);


bool ef4_mdio_links_ok(struct ef4_nic *efx, unsigned int mmd_mask);


void ef4_mdio_transmit_disable(struct ef4_nic *efx);


void ef4_mdio_phy_reconfigure(struct ef4_nic *efx);


void ef4_mdio_set_mmds_lpower(struct ef4_nic *efx, int low_power,
			      unsigned int mmd_mask);


int ef4_mdio_set_link_ksettings(struct ef4_nic *efx,
				const struct ethtool_link_ksettings *cmd);


void ef4_mdio_an_reconfigure(struct ef4_nic *efx);


u8 ef4_mdio_get_pause(struct ef4_nic *efx);


int ef4_mdio_wait_reset_mmds(struct ef4_nic *efx, unsigned int mmd_mask);


static inline void
ef4_mdio_set_flag(struct ef4_nic *efx, int devad, int addr,
		  int mask, bool state)
{
	mdio_set_flag(&efx->mdio, efx->mdio.prtad, devad, addr, mask, state);
}


int ef4_mdio_test_alive(struct ef4_nic *efx);

#endif 
