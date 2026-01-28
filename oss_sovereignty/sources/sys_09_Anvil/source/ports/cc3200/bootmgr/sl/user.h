



#ifndef __USER_H__
#define __USER_H__

#ifdef  __cplusplus
extern "C" {
#endif







#include <string.h>
#include "cc_pal.h"


#define MAX_CONCURRENT_ACTIONS 10


#define CPU_FREQ_IN_MHZ        80





#define SL_INC_ARG_CHECK



#define SL_INC_STD_BSD_API_NAMING



#define SL_INC_EXT_API


#define SL_INC_WLAN_PKG


#define SL_INC_SOCKET_PKG


#define SL_INC_NET_APP_PKG


#define SL_INC_NET_CFG_PKG


#define SL_INC_NVMEM_PKG


#define SL_INC_SOCK_SERVER_SIDE_API


#define SL_INC_SOCK_CLIENT_SIDE_API


#define SL_INC_SOCK_RECV_API


#define SL_INC_SOCK_SEND_API







#ifdef DEBUG
#define sl_DeviceEnablePreamble()       NwpPowerOnPreamble()
#else
#define sl_DeviceEnablePreamble()
#endif


#define sl_DeviceEnable()			NwpPowerOn()


#define sl_DeviceDisable() 			NwpPowerOff()





#define _SlFd_t					Fd_t


#define sl_IfOpen                           spi_Open


#define sl_IfClose                          spi_Close


#define sl_IfRead                           spi_Read


#define sl_IfWrite                          spi_Write


#define sl_IfRegIntHdlr(InterruptHdl , pValue)          NwpRegisterInterruptHandler(InterruptHdl , pValue)




#define sl_IfMaskIntHdlr()								NwpMaskInterrupt()



#define sl_IfUnMaskIntHdlr()								NwpUnMaskInterrupt()











#ifdef SL_PLATFORM_MULTI_THREADED
#include "osi.h"



#define SL_OS_RET_CODE_OK                       ((int)OSI_OK)


#define SL_OS_WAIT_FOREVER                      ((OsiTime_t)OSI_WAIT_FOREVER)


#define SL_OS_NO_WAIT	                        ((OsiTime_t)OSI_NO_WAIT)


#define _SlTime_t				OsiTime_t


typedef OsiSyncObj_t                            _SlSyncObj_t;



#define sl_SyncObjCreate(pSyncObj,pName)            osi_SyncObjCreate(pSyncObj)



#define sl_SyncObjDelete(pSyncObj)                  osi_SyncObjDelete(pSyncObj)



#define sl_SyncObjSignal(pSyncObj)                osi_SyncObjSignal(pSyncObj)


#define sl_SyncObjSignalFromIRQ(pSyncObj)           osi_SyncObjSignalFromISR(pSyncObj)


#define sl_SyncObjWait(pSyncObj,Timeout)            osi_SyncObjWait(pSyncObj,Timeout)


typedef OsiLockObj_t                            _SlLockObj_t;


#define sl_LockObjCreate(pLockObj,pName)            osi_LockObjCreate(pLockObj)


#define sl_LockObjDelete(pLockObj)                  osi_LockObjDelete(pLockObj)


#define sl_LockObjLock(pLockObj,Timeout)           osi_LockObjLock(pLockObj,Timeout)


#define sl_LockObjUnlock(pLockObj)                   osi_LockObjUnlock(pLockObj)

#endif



#ifdef SL_PLATFORM_EXTERNAL_SPAWN
#define sl_Spawn(pEntry,pValue,flags)       osi_Spawn(pEntry,pValue,flags)
#endif






#define SL_MEMORY_MGMT_DYNAMIC 	        1
#define SL_MEMORY_MGMT_STATIC           0

#define SL_MEMORY_MGMT                  SL_MEMORY_MGMT_STATIC

#ifdef SL_MEMORY_MGMT_DYNAMIC
#ifdef SL_PLATFORM_MULTI_THREADED


#define sl_Malloc(Size)                                 mem_Malloc(Size)


#define sl_Free(pMem)                                   mem_Free(pMem)
#else
#include <stdlib.h>

#define sl_Malloc(Size)                                 malloc(Size)


#define sl_Free(pMem)                                   free(pMem)
#endif
#endif







#define sl_GeneralEvtHdlr                   SimpleLinkGeneralEventHandler




#define sl_WlanEvtHdlr                     SimpleLinkWlanEventHandler




#define sl_NetAppEvtHdlr              		SimpleLinkNetAppEventHandler



#define sl_HttpServerCallback   SimpleLinkHttpServerCallback


#define sl_SockEvtHdlr         SimpleLinkSockEventHandler



#define _SL_USER_TYPES
#define _u8         unsigned char
#define _i8         signed char

#define _u16        unsigned short
#define _i16        signed short

#define _u32        unsigned int
#define _i32        signed int
#define _volatile   volatile
#define _const      const






#ifdef  __cplusplus
}
#endif 

#endif 
