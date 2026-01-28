
#ifndef _FSL_MDIO_H_
#define _FSL_MDIO_H_

#include "fsl_common.h"




#if defined(MDIO_TIMEOUT_COUNT_NUMBER) && MDIO_TIMEOUT_COUNT_NUMBER
#define MDIO_TIMEOUT_COUNT MDIO_TIMEOUT_COUNT_NUMBER
#endif


enum _mdio_status
{
    kStatus_PHY_SMIVisitTimeout = MAKE_STATUS(kStatusGroup_PHY, 0), 
};

typedef struct _mdio_operations mdio_operations_t;


typedef struct _mdio_resource
{
    void *base;           
    uint32_t csrClock_Hz; 
} mdio_resource_t;


typedef struct _mdio_handle
{
    mdio_resource_t resource;
    const mdio_operations_t *ops;
} mdio_handle_t;


struct _mdio_operations
{
    void (*mdioInit)(mdio_handle_t *handle); 
    status_t (*mdioWrite)(mdio_handle_t *handle,
        uint32_t phyAddr,
        uint32_t devAddr,
        uint32_t data);                   
    status_t (*mdioRead)(mdio_handle_t *handle,
        uint32_t phyAddr,
        uint32_t devAddr,
        uint32_t *dataPtr);                  
    status_t (*mdioWriteExt)(mdio_handle_t *handle,
        uint32_t phyAddr,
        uint32_t devAddr,
        uint32_t data);                      
    status_t (*mdioReadExt)(mdio_handle_t *handle,
        uint32_t phyAddr,
        uint32_t devAddr,
        uint32_t *dataPtr);                     
};



#if defined(__cplusplus)
extern "C" {
#endif



static inline void MDIO_Init(mdio_handle_t *handle) {
    handle->ops->mdioInit(handle);
}


static inline status_t MDIO_Write(mdio_handle_t *handle, uint32_t phyAddr, uint32_t devAddr, uint32_t data) {
    return handle->ops->mdioWrite(handle, phyAddr, devAddr, data);
}


static inline status_t MDIO_Read(mdio_handle_t *handle, uint32_t phyAddr, uint32_t devAddr, uint32_t *dataPtr) {
    return handle->ops->mdioRead(handle, phyAddr, devAddr, dataPtr);
}



#if defined(__cplusplus)
}
#endif

#endif
