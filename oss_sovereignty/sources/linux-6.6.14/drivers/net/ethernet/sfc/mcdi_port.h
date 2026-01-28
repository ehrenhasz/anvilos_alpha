


#ifndef EFX_MCDI_PORT_H
#define EFX_MCDI_PORT_H

#include "net_driver.h"

u32 efx_mcdi_phy_get_caps(struct efx_nic *efx);
bool efx_mcdi_mac_check_fault(struct efx_nic *efx);
int efx_mcdi_port_probe(struct efx_nic *efx);
void efx_mcdi_port_remove(struct efx_nic *efx);

#endif 
