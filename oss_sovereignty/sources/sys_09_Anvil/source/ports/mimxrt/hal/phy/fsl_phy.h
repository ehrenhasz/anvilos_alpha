
#ifndef _FSL_PHY_H_
#define _FSL_PHY_H_

#include "fsl_mdio.h"




#define PHY_BASICCONTROL_REG        0x00U 
#define PHY_BASICSTATUS_REG         0x01U 
#define PHY_ID1_REG                 0x02U 
#define PHY_ID2_REG                 0x03U 
#define PHY_AUTONEG_ADVERTISE_REG   0x04U 
#define PHY_AUTONEG_LINKPARTNER_REG 0x05U 
#define PHY_AUTONEG_EXPANSION_REG   0x06U 
#define PHY_1000BASET_CONTROL_REG   0x09U 
#define PHY_MMD_ACCESS_CONTROL_REG  0x0DU 
#define PHY_MMD_ACCESS_DATA_REG     0x0EU 


#define PHY_BCTL_SPEED1_MASK          0x0040U 
#define PHY_BCTL_ISOLATE_MASK         0x0400U 
#define PHY_BCTL_DUPLEX_MASK          0x0100U 
#define PHY_BCTL_RESTART_AUTONEG_MASK 0x0200U 
#define PHY_BCTL_AUTONEG_MASK         0x1000U 
#define PHY_BCTL_SPEED0_MASK          0x2000U 
#define PHY_BCTL_LOOP_MASK            0x4000U 
#define PHY_BCTL_RESET_MASK           0x8000U 


#define PHY_BSTATUS_LINKSTATUS_MASK  0x0004U 
#define PHY_BSTATUS_AUTONEGABLE_MASK 0x0008U 
#define PHY_BSTATUS_SPEEDUPLX_MASK   0x001CU 
#define PHY_BSTATUS_AUTONEGCOMP_MASK 0x0020U 


#define PHY_100BaseT4_ABILITY_MASK    0x200U 
#define PHY_100BASETX_FULLDUPLEX_MASK 0x100U 
#define PHY_100BASETX_HALFDUPLEX_MASK 0x080U 
#define PHY_10BASETX_FULLDUPLEX_MASK  0x040U 
#define PHY_10BASETX_HALFDUPLEX_MASK  0x020U 
#define PHY_IEEE802_3_SELECTOR_MASK   0x001U 


#define PHY_1000BASET_FULLDUPLEX_MASK 0x200U 
#define PHY_1000BASET_HALFDUPLEX_MASK 0x100U 


typedef struct _phy_handle phy_handle_t;

typedef enum _phy_speed
{
    kPHY_Speed10M = 0U, 
    kPHY_Speed100M,     
    kPHY_Speed1000M     
} phy_speed_t;


typedef enum _phy_duplex
{
    kPHY_HalfDuplex = 0U, 
    kPHY_FullDuplex       
} phy_duplex_t;


typedef enum _phy_loop
{
    kPHY_LocalLoop = 0U, 
    kPHY_RemoteLoop,     
    kPHY_ExternalLoop,   
} phy_loop_t;


typedef enum _phy_mmd_access_mode
{
    kPHY_MMDAccessNoPostIncrement = (1U << 14), 
    kPHY_MMDAccessRdWrPostIncrement =
        (2U << 14),                             
    kPHY_MMDAccessWrPostIncrement = (3U << 14), 
} phy_mmd_access_mode_t;


typedef struct _phy_config
{
    uint32_t phyAddr;    
    phy_speed_t speed;   
    phy_duplex_t duplex; 
    bool autoNeg;        
    bool enableEEE;      
} phy_config_t;


typedef struct _phy_operations
{
    status_t (*phyInit)(phy_handle_t *handle, const phy_config_t *config);
    status_t (*phyWrite)(phy_handle_t *handle, uint32_t phyReg, uint32_t data);
    status_t (*phyRead)(phy_handle_t *handle, uint32_t phyReg, uint32_t *dataPtr);
    status_t (*getAutoNegoStatus)(phy_handle_t *handle, bool *status);
    status_t (*getLinkStatus)(phy_handle_t *handle, bool *status);
    status_t (*getLinkSpeedDuplex)(phy_handle_t *handle, phy_speed_t *speed, phy_duplex_t *duplex);
    status_t (*setLinkSpeedDuplex)(phy_handle_t *handle, phy_speed_t speed, phy_duplex_t duplex);
    status_t (*enableLoopback)(phy_handle_t *handle, phy_loop_t mode, phy_speed_t speed, bool enable);
} phy_operations_t;



struct _phy_handle
{
    uint32_t phyAddr;            
    mdio_handle_t *mdioHandle;   
    const phy_operations_t *ops; 
};



#if defined(__cplusplus)
extern "C" {
#endif




static inline status_t PHY_Init(phy_handle_t *handle, const phy_config_t *config) {
    return handle->ops->phyInit(handle, config);
}

static inline status_t PHY_Write(phy_handle_t *handle, uint32_t phyReg, uint32_t data) {
    return handle->ops->phyWrite(handle, phyReg, data);
}


static inline status_t PHY_Read(phy_handle_t *handle, uint32_t phyReg, uint32_t *dataPtr) {
    return handle->ops->phyRead(handle, phyReg, dataPtr);
}


static inline status_t PHY_GetAutoNegotiationStatus(phy_handle_t *handle, bool *status) {
    return handle->ops->getAutoNegoStatus(handle, status);
}


static inline status_t PHY_GetLinkStatus(phy_handle_t *handle, bool *status) {
    return handle->ops->getLinkStatus(handle, status);
}


static inline status_t PHY_GetLinkSpeedDuplex(phy_handle_t *handle, phy_speed_t *speed, phy_duplex_t *duplex) {
    return handle->ops->getLinkSpeedDuplex(handle, speed, duplex);
}


static inline status_t PHY_SetLinkSpeedDuplex(phy_handle_t *handle, phy_speed_t speed, phy_duplex_t duplex) {
    return handle->ops->setLinkSpeedDuplex(handle, speed, duplex);
}


static inline status_t PHY_EnableLoopback(phy_handle_t *handle, phy_loop_t mode, phy_speed_t speed, bool enable) {
    return handle->ops->enableLoopback(handle, mode, speed, enable);
}



#if defined(__cplusplus)
}
#endif


#endif
