 



 
 
 
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"
#include "flowcont.h"


 
 
 
void _SlDrvFlowContInit(void)
{
    g_pCB->FlowContCB.TxPoolCnt = FLOW_CONT_MIN;

    OSI_RET_OK_CHECK(sl_LockObjCreate(&g_pCB->FlowContCB.TxLockObj, "TxLockObj"));

    OSI_RET_OK_CHECK(sl_SyncObjCreate(&g_pCB->FlowContCB.TxSyncObj, "TxSyncObj"));
}

 
 
 
void _SlDrvFlowContDeinit(void)
{
    g_pCB->FlowContCB.TxPoolCnt = 0;

    OSI_RET_OK_CHECK(sl_LockObjDelete(&g_pCB->FlowContCB.TxLockObj));

    OSI_RET_OK_CHECK(sl_SyncObjDelete(&g_pCB->FlowContCB.TxSyncObj));
}

