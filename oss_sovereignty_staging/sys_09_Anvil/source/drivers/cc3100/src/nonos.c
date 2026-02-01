 



 
 
 
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"

#ifndef SL_PLATFORM_MULTI_THREADED

#include "nonos.h"

#ifndef SL_TINY_EXT
#define NONOS_MAX_SPAWN_ENTRIES     5
#else
#define NONOS_MAX_SPAWN_ENTRIES     1
#endif


typedef struct
{
    _SlSpawnEntryFunc_t 		pEntry;
    void* 						pValue;
}_SlNonOsSpawnEntry_t;

typedef struct
{
    _SlNonOsSpawnEntry_t	SpawnEntries[NONOS_MAX_SPAWN_ENTRIES];
}_SlNonOsCB_t;

_SlNonOsCB_t g__SlNonOsCB;


_SlNonOsRetVal_t _SlNonOsSemSet(_SlNonOsSemObj_t* pSemObj , _SlNonOsSemObj_t Value)
{
    *pSemObj = Value;
    return NONOS_RET_OK;
}

_SlNonOsRetVal_t _SlNonOsSemGet(_SlNonOsSemObj_t* pSyncObj, _SlNonOsSemObj_t WaitValue, _SlNonOsSemObj_t SetValue, _SlNonOsTime_t Timeout)
{
#ifdef _SlSyncWaitLoopCallback
    _SlNonOsTime_t timeOutRequest = Timeout; 
#endif
    while (Timeout>0)
    {
        if (WaitValue == *pSyncObj)
        {
            *pSyncObj = SetValue;
            break;
        }
        if (Timeout != NONOS_WAIT_FOREVER)
        {		
            Timeout--;
        }
        _SlNonOsMainLoopTask();
#ifdef _SlSyncWaitLoopCallback
        if( (__NON_OS_SYNC_OBJ_SIGNAL_VALUE == WaitValue) && (timeOutRequest != NONOS_NO_WAIT) )
        {
            if (WaitValue == *pSyncObj)
            {
                *pSyncObj = SetValue;
                break;
            }
            _SlSyncWaitLoopCallback();
        }
#endif
    }

    if (0 == Timeout)
    {
        return NONOS_RET_ERR;
    }
    else
    {
        return NONOS_RET_OK;
    }
}


_SlNonOsRetVal_t _SlNonOsSpawn(_SlSpawnEntryFunc_t pEntry , void* pValue , _u32 flags)
{
	 _i8 i = 0;
    
#ifndef SL_TINY_EXT 	
	for (i=0 ; i<NONOS_MAX_SPAWN_ENTRIES ; i++)
#endif     
	{
		_SlNonOsSpawnEntry_t* pE = &g__SlNonOsCB.SpawnEntries[i];
	
		if (NULL == pE->pEntry)
		{
			pE->pValue = pValue;
			pE->pEntry = pEntry;
#ifndef SL_TINY_EXT 	                        
			break;
#endif                        
		}
	}
        
        
        return NONOS_RET_OK;
}


_SlNonOsRetVal_t _SlNonOsMainLoopTask(void)
{
	_i8 i=0;


#ifndef SL_TINY_EXT
	for (i=0 ; i<NONOS_MAX_SPAWN_ENTRIES ; i++)
#endif
	{
		_SlNonOsSpawnEntry_t* pE = &g__SlNonOsCB.SpawnEntries[i];
		_SlSpawnEntryFunc_t 		pF = pE->pEntry;
		
		if (NULL != pF)
		{
            if(RxIrqCnt != (g_pCB)->RxDoneCnt)
            {
                pF(0);  
            }
            
			pE->pEntry = NULL;
			pE->pValue = NULL;
		}
	}
        
        return NONOS_RET_OK;
}

    
#endif  
