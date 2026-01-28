





#ifndef _FSL_PHYKSZ8081_H_
#define _FSL_PHYKSZ8081_H_

#include "fsl_phy.h"






#define FSL_PHY_DRIVER_VERSION (MAKE_VERSION(2, 0, 0))


extern const phy_operations_t phyksz8081_ops;



#if defined(__cplusplus)
extern "C" {
#endif




status_t PHY_KSZ8081_Init(phy_handle_t *handle, const phy_config_t *config);


status_t PHY_KSZ8081_Write(phy_handle_t *handle, uint32_t phyReg, uint32_t data);


status_t PHY_KSZ8081_Read(phy_handle_t *handle, uint32_t phyReg, uint32_t *dataPtr);


status_t PHY_KSZ8081_GetAutoNegotiationStatus(phy_handle_t *handle, bool *status);


status_t PHY_KSZ8081_GetLinkStatus(phy_handle_t *handle, bool *status);


status_t PHY_KSZ8081_GetLinkSpeedDuplex(phy_handle_t *handle, phy_speed_t *speed, phy_duplex_t *duplex);


status_t PHY_KSZ8081_SetLinkSpeedDuplex(phy_handle_t *handle, phy_speed_t speed, phy_duplex_t duplex);


status_t PHY_KSZ8081_EnableLoopback(phy_handle_t *handle, phy_loop_t mode, phy_speed_t speed, bool enable);



#if defined(__cplusplus)
}
#endif



#endif 
