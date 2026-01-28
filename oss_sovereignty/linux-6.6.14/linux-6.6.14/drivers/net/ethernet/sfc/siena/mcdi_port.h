#ifndef EFX_MCDI_PORT_H
#define EFX_MCDI_PORT_H
#include "net_driver.h"
bool efx_siena_mcdi_mac_check_fault(struct efx_nic *efx);
int efx_siena_mcdi_port_probe(struct efx_nic *efx);
void efx_siena_mcdi_port_remove(struct efx_nic *efx);
#endif  
