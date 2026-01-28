/

typedef unsigned int OsiTime_t;


typedef void * OsiSyncObj_t;



typedef void * OsiLockObj_t;


typedef void (*P_OSI_SPAWN_ENTRY)(void* pValue);

typedef void (*P_OSI_EVENT_HANDLER)(void* pValue);

typedef void (*P_OSI_TASK_ENTRY)(void* pValue);

typedef void (*P_OSI_INTR_ENTRY)(void);

typedef void* OsiTaskHandle;


OsiReturnVal_e osi_InterruptRegister(int iIntrNum,P_OSI_INTR_ENTRY pEntry,unsigned char ucPriority);


void osi_InterruptDeRegister(int iIntrNum);



OsiReturnVal_e osi_SyncObjCreate(OsiSyncObj_t* pSyncObj);



OsiReturnVal_e osi_SyncObjDelete(OsiSyncObj_t* pSyncObj);


OsiReturnVal_e osi_SyncObjSignal(OsiSyncObj_t* pSyncObj);


OsiReturnVal_e osi_SyncObjSignalFromISR(OsiSyncObj_t* pSyncObj);


OsiReturnVal_e osi_SyncObjWait(OsiSyncObj_t* pSyncObj , OsiTime_t Timeout);


OsiReturnVal_e osi_SyncObjClear(OsiSyncObj_t* pSyncObj);


OsiReturnVal_e osi_LockObjCreate(OsiLockObj_t* pLockObj);


#define osi_LockObjDelete            osi_SyncObjDelete


#define osi_LockObjLock             osi_SyncObjWait


#define osi_LockObjUnlock           osi_SyncObjSignal




OsiReturnVal_e osi_TaskCreate(P_OSI_TASK_ENTRY pEntry,const signed char * const pcName,unsigned short usStackDepth,void *pvParameters,unsigned long uxPriority,OsiTaskHandle *pTaskHandle);


void osi_TaskDelete(OsiTaskHandle* pTaskHandle);


OsiReturnVal_e osi_Spawn(P_OSI_SPAWN_ENTRY pEntry , void* pValue , unsigned long flags);



OsiReturnVal_e osi_MsgQCreate(OsiMsgQ_t* 		pMsgQ ,
							  char*				pMsgQName,
							  unsigned long 		MsgSize,
							  unsigned long 		MaxMsgs);


OsiReturnVal_e osi_MsgQDelete(OsiMsgQ_t* pMsgQ);



OsiReturnVal_e osi_MsgQWrite(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout);



OsiReturnVal_e osi_MsgQRead(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout);


void osi_start();


void * mem_Malloc(unsigned long Size);



void mem_Free(void *pMem);



void  mem_set(void *pBuf,int Val,size_t Size);


void  mem_copy(void *pDst, void *pSrc,size_t Size);


void osi_EnterCritical(void);


void osi_ExitCritical(void);


void osi_ContextSave();

void osi_ContextRestore();


void osi_Sleep(unsigned int MilliSecs);


void osi_TaskDisable(void);


void osi_TaskEnable(void);


typedef struct
{
    P_OSI_SPAWN_ENTRY pEntry;
    void* pValue;
}tSimpleLinkSpawnMsg;


extern void* xSimpleLinkSpawnQueue;


OsiReturnVal_e VStartSimpleLinkSpawnTask(unsigned long uxPriority);
void VDeleteSimpleLinkSpawnTask( void );



#ifdef  __cplusplus
}
#endif 

#endif
