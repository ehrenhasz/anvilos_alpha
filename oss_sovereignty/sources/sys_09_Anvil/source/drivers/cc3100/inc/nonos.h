

#ifndef __NONOS_H__
#define	__NONOS_H__

#ifdef	__cplusplus
extern "C" {
#endif


#ifndef SL_PLATFORM_MULTI_THREADED









#define NONOS_WAIT_FOREVER   							0xFF
#define NONOS_NO_WAIT        							0x00

#define NONOS_RET_OK                            (0)
#define NONOS_RET_ERR                           (0xFF)
#define OSI_OK  NONOS_RET_OK

#define __NON_OS_SYNC_OBJ_CLEAR_VALUE				0x11
#define __NON_OS_SYNC_OBJ_SIGNAL_VALUE				0x22
#define __NON_OS_LOCK_OBJ_UNLOCK_VALUE				0x33
#define __NON_OS_LOCK_OBJ_LOCK_VALUE				0x44


typedef _i8 _SlNonOsRetVal_t;


typedef _u8 _SlNonOsTime_t;


typedef _u8 _SlNonOsSemObj_t;


#define _SlTime_t       _SlNonOsTime_t

#define _SlSyncObj_t    _SlNonOsSemObj_t

#define _SlLockObj_t    _SlNonOsSemObj_t

#define SL_OS_WAIT_FOREVER     NONOS_WAIT_FOREVER

#define SL_OS_RET_CODE_OK       NONOS_RET_OK       

#define SL_OS_NO_WAIT           NONOS_NO_WAIT






#define _SlNonOsSyncObjCreate(pSyncObj)			_SlNonOsSemSet(pSyncObj,__NON_OS_SYNC_OBJ_CLEAR_VALUE)


#define _SlNonOsSyncObjDelete(pSyncObj)			_SlNonOsSemSet(pSyncObj,0)


#define _SlNonOsSyncObjSignal(pSyncObj)			_SlNonOsSemSet(pSyncObj,__NON_OS_SYNC_OBJ_SIGNAL_VALUE)


#define _SlNonOsSyncObjWait(pSyncObj , Timeout)	_SlNonOsSemGet(pSyncObj,__NON_OS_SYNC_OBJ_SIGNAL_VALUE,__NON_OS_SYNC_OBJ_CLEAR_VALUE,Timeout)


#define _SlNonOsSyncObjClear(pSyncObj)			_SlNonOsSemSet(pSyncObj,__NON_OS_SYNC_OBJ_CLEAR_VALUE)


#define _SlNonOsLockObjCreate(pLockObj)			_SlNonOsSemSet(pLockObj,__NON_OS_LOCK_OBJ_UNLOCK_VALUE)


#define _SlNonOsLockObjDelete(pLockObj)			_SlNonOsSemSet(pLockObj,0)


#define _SlNonOsLockObjLock(pLockObj , Timeout)	_SlNonOsSemGet(pLockObj,__NON_OS_LOCK_OBJ_UNLOCK_VALUE,__NON_OS_LOCK_OBJ_LOCK_VALUE,Timeout)


#define _SlNonOsLockObjUnlock(pLockObj)			_SlNonOsSemSet(pLockObj,__NON_OS_LOCK_OBJ_UNLOCK_VALUE)



_SlNonOsRetVal_t _SlNonOsSpawn(_SlSpawnEntryFunc_t pEntry , void* pValue , _u32 flags);



_SlNonOsRetVal_t _SlNonOsMainLoopTask(void);

extern _SlNonOsRetVal_t _SlNonOsSemGet(_SlNonOsSemObj_t* pSyncObj, _SlNonOsSemObj_t WaitValue, _SlNonOsSemObj_t SetValue, _SlNonOsTime_t Timeout);
extern _SlNonOsRetVal_t _SlNonOsSemSet(_SlNonOsSemObj_t* pSemObj , _SlNonOsSemObj_t Value);
extern _SlNonOsRetVal_t _SlNonOsSpawn(_SlSpawnEntryFunc_t pEntry , void* pValue , _u32 flags);
  
#if (defined(_SlSyncWaitLoopCallback))
extern void _SlSyncWaitLoopCallback(void);
#endif




#undef sl_SyncObjCreate
#define sl_SyncObjCreate(pSyncObj,pName)           _SlNonOsSemSet(pSyncObj,__NON_OS_SYNC_OBJ_CLEAR_VALUE)

#undef sl_SyncObjDelete
#define sl_SyncObjDelete(pSyncObj)                  _SlNonOsSemSet(pSyncObj,0)

#undef sl_SyncObjSignal
#define sl_SyncObjSignal(pSyncObj)                  _SlNonOsSemSet(pSyncObj,__NON_OS_SYNC_OBJ_SIGNAL_VALUE)

#undef sl_SyncObjSignalFromIRQ
#define sl_SyncObjSignalFromIRQ(pSyncObj)           _SlNonOsSemSet(pSyncObj,__NON_OS_SYNC_OBJ_SIGNAL_VALUE)

#undef sl_SyncObjWait
#define sl_SyncObjWait(pSyncObj,Timeout)            _SlNonOsSemGet(pSyncObj,__NON_OS_SYNC_OBJ_SIGNAL_VALUE,__NON_OS_SYNC_OBJ_CLEAR_VALUE,Timeout)

#undef sl_LockObjCreate
#define sl_LockObjCreate(pLockObj,pName)            _SlNonOsSemSet(pLockObj,__NON_OS_LOCK_OBJ_UNLOCK_VALUE)

#undef sl_LockObjDelete
#define sl_LockObjDelete(pLockObj)                  _SlNonOsSemSet(pLockObj,0)

#undef sl_LockObjLock
#define sl_LockObjLock(pLockObj,Timeout)            _SlNonOsSemGet(pLockObj,__NON_OS_LOCK_OBJ_UNLOCK_VALUE,__NON_OS_LOCK_OBJ_LOCK_VALUE,Timeout)

#undef sl_LockObjUnlock
#define sl_LockObjUnlock(pLockObj)                  _SlNonOsSemSet(pLockObj,__NON_OS_LOCK_OBJ_UNLOCK_VALUE)

#undef sl_Spawn
#define sl_Spawn(pEntry,pValue,flags)               _SlNonOsSpawn(pEntry,pValue,flags)

#undef _SlTaskEntry
#define _SlTaskEntry                                _SlNonOsMainLoopTask

#endif 

#ifdef  __cplusplus
}
#endif 

#endif
