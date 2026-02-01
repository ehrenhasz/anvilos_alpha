 



 
 
 
#include "simplelink.h"


#if (defined (SL_PLATFORM_MULTI_THREADED)) && (!defined (SL_PLATFORM_EXTERNAL_SPAWN))

#define _SL_MAX_INTERNAL_SPAWN_ENTRIES      10

typedef struct _SlInternalSpawnEntry_t
{
	_SlSpawnEntryFunc_t 		        pEntry;
	void* 						        pValue;
    struct _SlInternalSpawnEntry_t*     pNext;
}_SlInternalSpawnEntry_t;

typedef struct
{
	_SlInternalSpawnEntry_t     SpawnEntries[_SL_MAX_INTERNAL_SPAWN_ENTRIES];
    _SlInternalSpawnEntry_t*    pFree;
    _SlInternalSpawnEntry_t*    pWaitForExe;
    _SlInternalSpawnEntry_t*    pLastInWaitList;
    _SlSyncObj_t                SyncObj;
    _SlLockObj_t                LockObj;
}_SlInternalSpawnCB_t;

_SlInternalSpawnCB_t g_SlInternalSpawnCB;


void _SlInternalSpawnTaskEntry() 
{
    _i16                         i;
    _SlInternalSpawnEntry_t*    pEntry;
    _u8                         LastEntry;

     
    sl_LockObjCreate(&g_SlInternalSpawnCB.LockObj,"SlSpawnProtect");
    sl_LockObjLock(&g_SlInternalSpawnCB.LockObj,SL_OS_NO_WAIT);

     
    sl_SyncObjCreate(&g_SlInternalSpawnCB.SyncObj,"SlSpawnSync");
    sl_SyncObjWait(&g_SlInternalSpawnCB.SyncObj,SL_OS_NO_WAIT);

    g_SlInternalSpawnCB.pFree = &g_SlInternalSpawnCB.SpawnEntries[0];
    g_SlInternalSpawnCB.pWaitForExe = NULL;
    g_SlInternalSpawnCB.pLastInWaitList = NULL;

     
    for (i=0 ; i<_SL_MAX_INTERNAL_SPAWN_ENTRIES - 1 ; i++)
    {
        g_SlInternalSpawnCB.SpawnEntries[i].pNext = &g_SlInternalSpawnCB.SpawnEntries[i+1];
        g_SlInternalSpawnCB.SpawnEntries[i].pEntry = NULL;
    }
    g_SlInternalSpawnCB.SpawnEntries[i].pNext = NULL;

    _SlDrvObjUnLock(&g_SlInternalSpawnCB.LockObj);

     

    while (TRUE)
    {
        sl_SyncObjWait(&g_SlInternalSpawnCB.SyncObj,SL_OS_WAIT_FOREVER);
         
        LastEntry = FALSE;
        do
        {
             
            _SlDrvObjLockWaitForever(&g_SlInternalSpawnCB.LockObj);

            pEntry = g_SlInternalSpawnCB.pWaitForExe;
            if ( NULL == pEntry )
            {
               _SlDrvObjUnLock(&g_SlInternalSpawnCB.LockObj);
               break;
            }
            g_SlInternalSpawnCB.pWaitForExe = pEntry->pNext;
            if (pEntry == g_SlInternalSpawnCB.pLastInWaitList)
            {
                g_SlInternalSpawnCB.pLastInWaitList = NULL;
                LastEntry = TRUE;
            }

            _SlDrvObjUnLock(&g_SlInternalSpawnCB.LockObj);

             
            if (NULL != pEntry)
            {
                pEntry->pEntry(pEntry->pValue);
                 

                _SlDrvObjLockWaitForever(&g_SlInternalSpawnCB.LockObj);
                
                pEntry->pNext = g_SlInternalSpawnCB.pFree;
                g_SlInternalSpawnCB.pFree = pEntry;


                if (NULL != g_SlInternalSpawnCB.pWaitForExe)
                {
                     
                    LastEntry = FALSE;
                }

                _SlDrvObjUnLock(&g_SlInternalSpawnCB.LockObj);

            }

        }while (!LastEntry);
    }
}


_i16 _SlInternalSpawn(_SlSpawnEntryFunc_t pEntry , void* pValue , _u32 flags)
{
    _i16                         Res = 0;
    _SlInternalSpawnEntry_t*    pSpawnEntry;

    if (NULL == pEntry)
    {
        Res = -1;
    }
    else
    {
        _SlDrvObjLockWaitForever(&g_SlInternalSpawnCB.LockObj);

        pSpawnEntry = g_SlInternalSpawnCB.pFree;
        g_SlInternalSpawnCB.pFree = pSpawnEntry->pNext;

        pSpawnEntry->pEntry = pEntry;
        pSpawnEntry->pValue = pValue;
        pSpawnEntry->pNext = NULL;

        if (NULL == g_SlInternalSpawnCB.pWaitForExe)
        {
            g_SlInternalSpawnCB.pWaitForExe = pSpawnEntry;
            g_SlInternalSpawnCB.pLastInWaitList = pSpawnEntry;
        }
        else
        {
            g_SlInternalSpawnCB.pLastInWaitList->pNext = pSpawnEntry;
            g_SlInternalSpawnCB.pLastInWaitList = pSpawnEntry;
        }

        _SlDrvObjUnLock(&g_SlInternalSpawnCB.LockObj);
        
         
        _SlDrvSyncObjSignal(&g_SlInternalSpawnCB.SyncObj);
    }

    return Res;
}





#endif
