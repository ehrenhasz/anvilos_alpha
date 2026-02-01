 

 
 
 
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"
#include "flowcont.h"

 
 
 

#define          _SL_PENDING_RX_MSG(pDriverCB)   (RxIrqCnt != (pDriverCB)->RxDoneCnt)

 
#define N2H_SYNC_PATTERN_SEQ_NUM_BITS            ((_u32)0x00000003)  
#define N2H_SYNC_PATTERN_SEQ_NUM_EXISTS          ((_u32)0x00000004)  
#define N2H_SYNC_PATTERN_MASK                    ((_u32)0xFFFFFFF8)  
#define N2H_SYNC_SPI_BUGS_MASK                   ((_u32)0x7FFF7F7F)  
#define BUF_SYNC_SPIM(pBuf)                      ((*(_u32 *)(pBuf)) & N2H_SYNC_SPI_BUGS_MASK)

_u8             _SlDrvProtectAsyncRespSetting(_u8 *pAsyncRsp, _u8 ActionID, _u8 SocketID);
#define N2H_SYNC_SPIM                            (N2H_SYNC_PATTERN    & N2H_SYNC_SPI_BUGS_MASK)
#define N2H_SYNC_SPIM_WITH_SEQ(TxSeqNum)         ((N2H_SYNC_SPIM & N2H_SYNC_PATTERN_MASK) | N2H_SYNC_PATTERN_SEQ_NUM_EXISTS | ((TxSeqNum) & (N2H_SYNC_PATTERN_SEQ_NUM_BITS)))
#define MATCH_WOUT_SEQ_NUM(pBuf)                 ( BUF_SYNC_SPIM(pBuf) ==  N2H_SYNC_SPIM )
#define MATCH_WITH_SEQ_NUM(pBuf, TxSeqNum)       ( BUF_SYNC_SPIM(pBuf) == (N2H_SYNC_SPIM_WITH_SEQ(TxSeqNum)) )
#define N2H_SYNC_PATTERN_MATCH(pBuf, TxSeqNum) \
    ( \
    (  (*((_u32 *)pBuf) & N2H_SYNC_PATTERN_SEQ_NUM_EXISTS) && ( MATCH_WITH_SEQ_NUM(pBuf, TxSeqNum) ) )	|| \
    ( !(*((_u32 *)pBuf) & N2H_SYNC_PATTERN_SEQ_NUM_EXISTS) && ( MATCH_WOUT_SEQ_NUM(pBuf          ) ) )	   \
    )

#define OPCODE(_ptr)          (((_SlResponseHeader_t *)(_ptr))->GenHeader.Opcode)         
#define RSP_PAYLOAD_LEN(_ptr) (((_SlResponseHeader_t *)(_ptr))->GenHeader.Len - _SL_RESP_SPEC_HDR_SIZE)         
#define SD(_ptr)              (((_SocketAddrResponse_u *)(_ptr))->IpV4.sd)
 
#define ACT_DATA_SIZE(_ptr)   (((_SocketAddrResponse_u *)(_ptr))->IpV4.statusOrLen)




 
#if defined (EXT_LIB_REGISTERED_GENERAL_EVENTS)

typedef _SlEventPropogationStatus_e (*general_callback) (SlDeviceEvent_t *);

static const general_callback  general_callbacks[] =
{
#ifdef SlExtLib1GeneralEventHandler
	SlExtLib1GeneralEventHandler,
#endif

#ifdef SlExtLib2GeneralEventHandler
	SlExtLib2GeneralEventHandler,
#endif

#ifdef SlExtLib3GeneralEventHandler
	SlExtLib3GeneralEventHandler,
#endif

#ifdef SlExtLib4GeneralEventHandler
	SlExtLib4GeneralEventHandler,
#endif

#ifdef SlExtLib5GeneralEventHandler
	SlExtLib5GeneralEventHandler,
#endif
};

#undef _SlDrvHandleGeneralEvents

 
void _SlDrvHandleGeneralEvents(SlDeviceEvent_t *slGeneralEvent)
{
    _u8 i;

     
    for ( i = 0 ; i < sizeof(general_callbacks)/sizeof(general_callbacks[0]) ; i++ )
    {
        if (EVENT_PROPAGATION_BLOCK == general_callbacks[i](slGeneralEvent) )
		{
        	 
            return;
		}
    }

 
#ifdef sl_GeneralEvtHdlr
    sl_GeneralEvtHdlr(slGeneralEvent);
#endif

}
#endif



 

#if defined (EXT_LIB_REGISTERED_WLAN_EVENTS)

typedef _SlEventPropogationStatus_e (*wlan_callback) (SlWlanEvent_t *);

static wlan_callback  wlan_callbacks[] =
{
#ifdef SlExtLib1WlanEventHandler
		SlExtLib1WlanEventHandler,
#endif

#ifdef SlExtLib2WlanEventHandler
		SlExtLib2WlanEventHandler,
#endif

#ifdef SlExtLib3WlanEventHandler
		SlExtLib3WlanEventHandler,
#endif

#ifdef SlExtLib4WlanEventHandler
		SlExtLib4WlanEventHandler,
#endif

#ifdef SlExtLib5WlanEventHandler
		SlExtLib5WlanEventHandler,
#endif
};

#undef _SlDrvHandleWlanEvents

 
void _SlDrvHandleWlanEvents(SlWlanEvent_t *slWlanEvent)
{
    _u8 i;

     
    for ( i = 0 ; i < sizeof(wlan_callbacks)/sizeof(wlan_callbacks[0]) ; i++ )
    {
        if ( EVENT_PROPAGATION_BLOCK == wlan_callbacks[i](slWlanEvent) )
		{
        	 
            return;
		}
    }

 
#ifdef sl_WlanEvtHdlr
    sl_WlanEvtHdlr(slWlanEvent);
#endif

}
#endif


 
#if defined (EXT_LIB_REGISTERED_NETAPP_EVENTS)

typedef _SlEventPropogationStatus_e (*netApp_callback) (SlNetAppEvent_t *);

static const netApp_callback  netApp_callbacks[] =
{
#ifdef SlExtLib1NetAppEventHandler
	 SlExtLib1NetAppEventHandler,
#endif

#ifdef SlExtLib2NetAppEventHandler
	 SlExtLib2NetAppEventHandler,
#endif

#ifdef SlExtLib3NetAppEventHandler
	 SlExtLib3NetAppEventHandler,
#endif

#ifdef SlExtLib4NetAppEventHandler
	 SlExtLib4NetAppEventHandler,
#endif

#ifdef SlExtLib5NetAppEventHandler
	 SlExtLib5NetAppEventHandler,
#endif
};

#undef _SlDrvHandleNetAppEvents

 
void _SlDrvHandleNetAppEvents(SlNetAppEvent_t *slNetAppEvent)
{
    _u8 i;

     
    for ( i = 0 ; i < sizeof(netApp_callbacks)/sizeof(netApp_callbacks[0]) ; i++ )
    {
        if (EVENT_PROPAGATION_BLOCK == netApp_callbacks[i](slNetAppEvent) )
		{
        	 
            return;
		}
    }

 
#ifdef sl_NetAppEvtHdlr
    sl_NetAppEvtHdlr(slNetAppEvent);
#endif

}
#endif


 
#if defined (EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS)

typedef _SlEventPropogationStatus_e (*httpServer_callback) (SlHttpServerEvent_t*, SlHttpServerResponse_t*);

static const httpServer_callback  httpServer_callbacks[] =
{
#ifdef SlExtLib1HttpServerEventHandler
		SlExtLib1HttpServerEventHandler,
#endif

#ifdef SlExtLib2HttpServerEventHandler
		SlExtLib2HttpServerEventHandler,
#endif

#ifdef SlExtLib3HttpServerEventHandler
		SlExtLib3HttpServerEventHandler,
#endif

#ifdef SlExtLib4HttpServerEventHandler
		SlExtLib4HttpServerEventHandler,
#endif

#ifdef SlExtLib5HttpServerEventHandler
	  SlExtLib5HttpServerEventHandler,
#endif
};

#undef _SlDrvHandleHttpServerEvents

 
void _SlDrvHandleHttpServerEvents(SlHttpServerEvent_t *slHttpServerEvent, SlHttpServerResponse_t *slHttpServerResponse)
{
    _u8 i;

     
    for ( i = 0 ; i < sizeof(httpServer_callbacks)/sizeof(httpServer_callbacks[0]) ; i++ )
    {
        if ( EVENT_PROPAGATION_BLOCK == httpServer_callbacks[i](slHttpServerEvent, slHttpServerResponse) )
		{
        	 
            return;
		}
    }

 
#ifdef sl_HttpServerCallback
    sl_HttpServerCallback(slHttpServerEvent, slHttpServerResponse);
#endif

}
#endif


 
#if defined (EXT_LIB_REGISTERED_SOCK_EVENTS)

typedef _SlEventPropogationStatus_e (*sock_callback) (SlSockEvent_t *);

static const sock_callback  sock_callbacks[] =
{
#ifdef SlExtLib1SockEventHandler
		SlExtLib1SockEventHandler,
#endif

#ifdef SlExtLib2SockEventHandler
		SlExtLib2SockEventHandler,
#endif

#ifdef SlExtLib3SockEventHandler
		SlExtLib3SockEventHandler,
#endif

#ifdef SlExtLib4SockEventHandler
		SlExtLib4SockEventHandler,
#endif

#ifdef SlExtLib5SockEventHandler
		SlExtLib5SockEventHandler,
#endif
};

 
void _SlDrvHandleSockEvents(SlSockEvent_t *slSockEvent)
{
    _u8 i;

     
    for ( i = 0 ; i < sizeof(sock_callbacks)/sizeof(sock_callbacks[0]) ; i++ )
    {
        if ( EVENT_PROPAGATION_BLOCK == sock_callbacks[i](slSockEvent) )
		{
        	 
            return;
		}
    }

 
#ifdef sl_SockEvtHdlr
    sl_SockEvtHdlr(slSockEvent);
#endif

}

#endif


#if (SL_MEMORY_MGMT != SL_MEMORY_MGMT_DYNAMIC)
typedef struct
{
    _u32 Align;
    _SlDriverCb_t DriverCB;
    _u8 AsyncRespBuf[SL_ASYNC_MAX_MSG_LEN];
}_SlStatMem_t;

_SlStatMem_t g_StatMem;
#endif

_u8 _SlDrvProtectAsyncRespSetting(_u8 *pAsyncRsp, _u8 ActionID, _u8 SocketID)
{
    _u8 ObjIdx;


     
    ObjIdx = _SlDrvWaitForPoolObj(ActionID, SocketID);

    if (MAX_CONCURRENT_ACTIONS != ObjIdx)
    {
        _SlDrvProtectionObjLockWaitForever();
        g_pCB->ObjPool[ObjIdx].pRespArgs = pAsyncRsp;
        _SlDrvProtectionObjUnLock();
    }

    return ObjIdx;
}


 
 
 
const _SlSyncPattern_t g_H2NSyncPattern = H2N_SYNC_PATTERN;
const _SlSyncPattern_t g_H2NCnysPattern = H2N_CNYS_PATTERN;
_volatile _u8           RxIrqCnt;

#ifndef SL_TINY_EXT
const _SlActionLookup_t _SlActionLookupTable[] = 
{
    {ACCEPT_ID, SL_OPCODE_SOCKET_ACCEPTASYNCRESPONSE, (_SlSpawnEntryFunc_t)_sl_HandleAsync_Accept},
    {CONNECT_ID, SL_OPCODE_SOCKET_CONNECTASYNCRESPONSE,(_SlSpawnEntryFunc_t)_sl_HandleAsync_Connect},
    {SELECT_ID, SL_OPCODE_SOCKET_SELECTASYNCRESPONSE,(_SlSpawnEntryFunc_t)_sl_HandleAsync_Select},
    {GETHOSYBYNAME_ID, SL_OPCODE_NETAPP_DNSGETHOSTBYNAMEASYNCRESPONSE,(_SlSpawnEntryFunc_t)_sl_HandleAsync_DnsGetHostByName},
    {GETHOSYBYSERVICE_ID, SL_OPCODE_NETAPP_MDNSGETHOSTBYSERVICEASYNCRESPONSE,(_SlSpawnEntryFunc_t)_sl_HandleAsync_DnsGetHostByService}, 
    {PING_ID, SL_OPCODE_NETAPP_PINGREPORTREQUESTRESPONSE, (_SlSpawnEntryFunc_t)_sl_HandleAsync_PingResponse},
    {START_STOP_ID, SL_OPCODE_DEVICE_STOP_ASYNC_RESPONSE,(_SlSpawnEntryFunc_t)_sl_HandleAsync_Stop}
};
#else
const _SlActionLookup_t _SlActionLookupTable[] = 
{
    {CONNECT_ID, SL_OPCODE_SOCKET_CONNECTASYNCRESPONSE,(_SlSpawnEntryFunc_t)_sl_HandleAsync_Connect},
    {GETHOSYBYNAME_ID, SL_OPCODE_NETAPP_DNSGETHOSTBYNAMEASYNCRESPONSE,(_SlSpawnEntryFunc_t)_sl_HandleAsync_DnsGetHostByName},  
    {START_STOP_ID, SL_OPCODE_DEVICE_STOP_ASYNC_RESPONSE,(_SlSpawnEntryFunc_t)_sl_HandleAsync_Stop}
};
#endif



typedef struct
{
    _u16 opcode;
    _u8  event;
} OpcodeKeyVal_t;

 
const OpcodeKeyVal_t OpcodeTranslateTable[] = 
{
{SL_OPCODE_WLAN_SMART_CONFIG_START_ASYNC_RESPONSE, SL_WLAN_SMART_CONFIG_COMPLETE_EVENT},
{SL_OPCODE_WLAN_SMART_CONFIG_STOP_ASYNC_RESPONSE,SL_WLAN_SMART_CONFIG_STOP_EVENT},
{SL_OPCODE_WLAN_STA_CONNECTED, SL_WLAN_STA_CONNECTED_EVENT},
{SL_OPCODE_WLAN_STA_DISCONNECTED,SL_WLAN_STA_DISCONNECTED_EVENT},
{SL_OPCODE_WLAN_P2P_DEV_FOUND,SL_WLAN_P2P_DEV_FOUND_EVENT},    
{SL_OPCODE_WLAN_P2P_NEG_REQ_RECEIVED, SL_WLAN_P2P_NEG_REQ_RECEIVED_EVENT},
{SL_OPCODE_WLAN_CONNECTION_FAILED, SL_WLAN_CONNECTION_FAILED_EVENT},
{SL_OPCODE_WLAN_WLANASYNCCONNECTEDRESPONSE, SL_WLAN_CONNECT_EVENT},
{SL_OPCODE_WLAN_WLANASYNCDISCONNECTEDRESPONSE, SL_WLAN_DISCONNECT_EVENT},
{SL_OPCODE_NETAPP_IPACQUIRED, SL_NETAPP_IPV4_IPACQUIRED_EVENT},
{SL_OPCODE_NETAPP_IPACQUIRED_V6, SL_NETAPP_IPV6_IPACQUIRED_EVENT},
{SL_OPCODE_NETAPP_IP_LEASED, SL_NETAPP_IP_LEASED_EVENT},
{SL_OPCODE_NETAPP_IP_RELEASED, SL_NETAPP_IP_RELEASED_EVENT},
{SL_OPCODE_SOCKET_TXFAILEDASYNCRESPONSE, SL_SOCKET_TX_FAILED_EVENT},
{SL_OPCODE_SOCKET_SOCKETASYNCEVENT, SL_SOCKET_ASYNC_EVENT}
};



_SlDriverCb_t* g_pCB = NULL;
P_SL_DEV_PING_CALLBACK  pPingCallBackFunc = NULL;
_u8 gFirstCmdMode = 0;

 
 
 
_SlReturnVal_t   _SlDrvMsgRead(void);
_SlReturnVal_t   _SlDrvMsgWrite(_SlCmdCtrl_t  *pCmdCtrl,_SlCmdExt_t  *pCmdExt, _u8 *pTxRxDescBuff);
_SlReturnVal_t   _SlDrvMsgReadCmdCtx(void);
_SlReturnVal_t   _SlDrvMsgReadSpawnCtx(void *pValue);
void             _SlDrvClassifyRxMsg(_SlOpcode_t Opcode );
_SlReturnVal_t   _SlDrvRxHdrRead(_u8 *pBuf, _u8 *pAlignSize);
void             _SlDrvShiftDWord(_u8 *pBuf);
void             _SlDrvDriverCBInit(void);
void             _SlAsyncEventGenericHandler(void);
_u8			     _SlDrvWaitForPoolObj(_u8 ActionID, _u8 SocketID);
void			 _SlDrvReleasePoolObj(_u8 pObj);
void			 _SlRemoveFromList(_u8* ListIndex, _u8 ItemIndex);
_SlReturnVal_t	 _SlFindAndSetActiveObj(_SlOpcode_t  Opcode, _u8 Sd);


 
 
 


 

void _SlDrvDriverCBInit(void)
{
    _u8          Idx =0;

#if (SL_MEMORY_MGMT == SL_MEMORY_MGMT_DYNAMIC)
    g_pCB = sl_Malloc(sizeof(_SlDriverCb_t));
#else
    g_pCB = &(g_StatMem.DriverCB);
#endif
    MALLOC_OK_CHECK(g_pCB);

    
    _SlDrvMemZero(g_pCB, sizeof(_SlDriverCb_t));
    RxIrqCnt = 0;
    OSI_RET_OK_CHECK( sl_SyncObjCreate(&g_pCB->CmdSyncObj, "CmdSyncObj") );
    sl_SyncObjClear(&g_pCB->CmdSyncObj);

    OSI_RET_OK_CHECK( sl_LockObjCreate(&g_pCB->GlobalLockObj, "GlobalLockObj") );
    OSI_RET_OK_CHECK( sl_LockObjCreate(&g_pCB->ProtectionLockObj, "ProtectionLockObj") );

     
    _SlDrvMemZero(&g_pCB->ObjPool[0], MAX_CONCURRENT_ACTIONS*sizeof(_SlPoolObj_t));

     
    g_pCB->FreePoolIdx = 0;

    for (Idx = 0 ; Idx < MAX_CONCURRENT_ACTIONS ; Idx++)
    {
        g_pCB->ObjPool[Idx].NextIndex = Idx + 1;
        g_pCB->ObjPool[Idx].AdditionalData = SL_MAX_SOCKETS;

        OSI_RET_OK_CHECK( sl_SyncObjCreate(&g_pCB->ObjPool[Idx].SyncObj, "SyncObj"));
        sl_SyncObjClear(&g_pCB->ObjPool[Idx].SyncObj);
    }

     g_pCB->ActivePoolIdx = MAX_CONCURRENT_ACTIONS;
     g_pCB->PendingPoolIdx = MAX_CONCURRENT_ACTIONS;

     
    g_pCB->FlowContCB.TxPoolCnt = FLOW_CONT_MIN;
    OSI_RET_OK_CHECK(sl_LockObjCreate(&g_pCB->FlowContCB.TxLockObj, "TxLockObj"));
    OSI_RET_OK_CHECK(sl_SyncObjCreate(&g_pCB->FlowContCB.TxSyncObj, "TxSyncObj"));
    
    gFirstCmdMode = 0;  

}

 
void _SlDrvDriverCBDeinit()
{
    _u8  Idx =0;

     
    g_pCB->FlowContCB.TxPoolCnt = 0;
    OSI_RET_OK_CHECK(sl_LockObjDelete(&g_pCB->FlowContCB.TxLockObj));
    OSI_RET_OK_CHECK(sl_SyncObjDelete(&g_pCB->FlowContCB.TxSyncObj));
    
    OSI_RET_OK_CHECK( sl_SyncObjDelete(&g_pCB->CmdSyncObj) );
    OSI_RET_OK_CHECK( sl_LockObjDelete(&g_pCB->GlobalLockObj) );
    OSI_RET_OK_CHECK( sl_LockObjDelete(&g_pCB->ProtectionLockObj) );
        
 #ifndef SL_TINY_EXT
    for (Idx = 0; Idx < MAX_CONCURRENT_ACTIONS; Idx++)
 #endif
    {
	OSI_RET_OK_CHECK( sl_SyncObjDelete(&g_pCB->ObjPool[Idx].SyncObj) );   
    }

    g_pCB->FreePoolIdx = 0;
    g_pCB->PendingPoolIdx = MAX_CONCURRENT_ACTIONS;
    g_pCB->ActivePoolIdx = MAX_CONCURRENT_ACTIONS;

#if (SL_MEMORY_MGMT == SL_MEMORY_MGMT_DYNAMIC)
    sl_Free(g_pCB);
#else
    g_pCB = NULL;
#endif


    g_pCB = NULL;
}

 
void _SlDrvRxIrqHandler(void *pValue)
{
    sl_IfMaskIntHdlr();

    RxIrqCnt++;

    if (TRUE == g_pCB->IsCmdRespWaited)
    {
        OSI_RET_OK_CHECK( sl_SyncObjSignalFromIRQ(&g_pCB->CmdSyncObj) );
    }
    else
    {
        sl_Spawn((_SlSpawnEntryFunc_t)_SlDrvMsgReadSpawnCtx, NULL, 0);
    }
}

 
_SlReturnVal_t _SlDrvCmdOp(
    _SlCmdCtrl_t  *pCmdCtrl ,
    void          *pTxRxDescBuff ,
    _SlCmdExt_t   *pCmdExt)
{
    _SlReturnVal_t RetVal;

    
    _SlDrvObjLockWaitForever(&g_pCB->GlobalLockObj);
    
    g_pCB->IsCmdRespWaited = TRUE;

    SL_TRACE0(DBG_MSG, MSG_312, "_SlDrvCmdOp: call _SlDrvMsgWrite");


     
    RetVal = _SlDrvMsgWrite(pCmdCtrl, pCmdExt, pTxRxDescBuff);

    if(SL_OS_RET_CODE_OK == RetVal)
    {

#ifndef SL_IF_TYPE_UART    
         
        if( 0 == gFirstCmdMode )
        {
            volatile _u32 CountVal = 0;
            gFirstCmdMode = 1;
            CountVal = CPU_FREQ_IN_MHZ*USEC_DELAY;
            while( CountVal-- );
        }   
#endif 
         
        RetVal = _SlDrvMsgReadCmdCtx();  
        SL_TRACE0(DBG_MSG, MSG_314, "_SlDrvCmdOp: exited _SlDrvMsgReadCmdCtx");
    }
    else
    {
        _SlDrvObjUnLock(&g_pCB->GlobalLockObj);
    }
    
    return RetVal;
}



 
_SlReturnVal_t _SlDrvDataReadOp(
    _SlSd_t             Sd,
    _SlCmdCtrl_t        *pCmdCtrl ,
    void                *pTxRxDescBuff ,
    _SlCmdExt_t         *pCmdExt)
{
    _SlReturnVal_t RetVal;
    _u8 ObjIdx = MAX_CONCURRENT_ACTIONS;
    _SlArgsData_t pArgsData;

     
    VERIFY_PROTOCOL(NULL != pCmdExt->pRxPayload);

     
     
    VERIFY_PROTOCOL(0 != pCmdExt->RxPayloadLen);

     
    if((Sd & BSD_SOCKET_ID_MASK) >= SL_MAX_SOCKETS)
    {
        return SL_EBADF;
    }

     
    ObjIdx = (_u8)_SlDrvWaitForPoolObj(RECV_ID, Sd & BSD_SOCKET_ID_MASK);

    if (MAX_CONCURRENT_ACTIONS == ObjIdx)
    {
        return SL_POOL_IS_EMPTY;
    }

    _SlDrvProtectionObjLockWaitForever();

    pArgsData.pData = pCmdExt->pRxPayload;
    pArgsData.pArgs =  (_u8 *)pTxRxDescBuff;
    g_pCB->ObjPool[ObjIdx].pRespArgs =  (_u8 *)&pArgsData;

    _SlDrvProtectionObjUnLock();


     
    _SlDrvObjLockWaitForever(&g_pCB->FlowContCB.TxLockObj);


     
     
     
    sl_SyncObjClear(&g_pCB->FlowContCB.TxSyncObj);

    if(g_pCB->FlowContCB.TxPoolCnt <= FLOW_CONT_MIN)
    {

         
        _SlDrvSyncObjWaitForever(&g_pCB->FlowContCB.TxSyncObj);
       
    }

    _SlDrvObjLockWaitForever(&g_pCB->GlobalLockObj);
    

    VERIFY_PROTOCOL(g_pCB->FlowContCB.TxPoolCnt > FLOW_CONT_MIN);
    g_pCB->FlowContCB.TxPoolCnt--;

    _SlDrvObjUnLock(&g_pCB->FlowContCB.TxLockObj);

     
    RetVal =  _SlDrvMsgWrite(pCmdCtrl, pCmdExt, (_u8 *)pTxRxDescBuff);

    _SlDrvObjUnLock(&g_pCB->GlobalLockObj);
    

    if(SL_OS_RET_CODE_OK == RetVal)
    {
         
        _SlDrvSyncObjWaitForever(&g_pCB->ObjPool[ObjIdx].SyncObj);
    }

    _SlDrvReleasePoolObj(ObjIdx);
    return RetVal;
}

 
 
 
_SlReturnVal_t _SlDrvDataWriteOp(
    _SlSd_t             Sd,
    _SlCmdCtrl_t  *pCmdCtrl ,
    void                *pTxRxDescBuff ,
    _SlCmdExt_t         *pCmdExt)
{
    _SlReturnVal_t  RetVal = SL_EAGAIN;  
    while( 1 )
    {
         
    _SlDrvObjLockWaitForever(&g_pCB->FlowContCB.TxLockObj);

         
         
         
        sl_SyncObjClear(&g_pCB->FlowContCB.TxSyncObj);

         
        if(g_pCB->SocketTXFailure & (1<<(Sd & BSD_SOCKET_ID_MASK)))
        {
		_SlDrvObjUnLock(&g_pCB->FlowContCB.TxLockObj);
            return SL_SOC_ERROR;
        }
        if(g_pCB->FlowContCB.TxPoolCnt <= FLOW_CONT_MIN + 1)
        {
             
             
            if( g_pCB->SocketNonBlocking & (1<< (Sd & BSD_SOCKET_ID_MASK)))
            {
            _SlDrvObjUnLock(&g_pCB->FlowContCB.TxLockObj);
                return RetVal;
            }
             
             
        _SlDrvSyncObjWaitForever(&g_pCB->FlowContCB.TxSyncObj);
        }
        if(g_pCB->FlowContCB.TxPoolCnt > FLOW_CONT_MIN + 1 )
        {
            break;
        }
        else
        {
			_SlDrvObjUnLock(&g_pCB->FlowContCB.TxLockObj);
        }
    }

    _SlDrvObjLockWaitForever(&g_pCB->GlobalLockObj);
    

    VERIFY_PROTOCOL(g_pCB->FlowContCB.TxPoolCnt > FLOW_CONT_MIN + 1 );
    g_pCB->FlowContCB.TxPoolCnt--;

    _SlDrvObjUnLock(&g_pCB->FlowContCB.TxLockObj);
    
     
    RetVal =  _SlDrvMsgWrite(pCmdCtrl, pCmdExt, pTxRxDescBuff);

    _SlDrvObjUnLock(&g_pCB->GlobalLockObj);

    return RetVal;
}

 
 
 
_SlReturnVal_t _SlDrvMsgWrite(_SlCmdCtrl_t  *pCmdCtrl,_SlCmdExt_t  *pCmdExt, _u8 *pTxRxDescBuff)
{
    _u8 sendRxPayload = FALSE;
    VERIFY_PROTOCOL(NULL != pCmdCtrl);

    g_pCB->FunctionParams.pCmdCtrl = pCmdCtrl;
    g_pCB->FunctionParams.pTxRxDescBuff = pTxRxDescBuff;
    g_pCB->FunctionParams.pCmdExt = pCmdExt;
    
    g_pCB->TempProtocolHeader.Opcode   = pCmdCtrl->Opcode;
    g_pCB->TempProtocolHeader.Len   = _SL_PROTOCOL_CALC_LEN(pCmdCtrl, pCmdExt);

    if (pCmdExt && pCmdExt->RxPayloadLen < 0 && pCmdExt->TxPayloadLen)
    {
        pCmdExt->RxPayloadLen = pCmdExt->RxPayloadLen * (-1);  
        sendRxPayload = TRUE;
        g_pCB->TempProtocolHeader.Len = g_pCB->TempProtocolHeader.Len + pCmdExt->RxPayloadLen;
    }

#ifdef SL_START_WRITE_STAT
    sl_IfStartWriteSequence(g_pCB->FD);
#endif

#ifdef SL_IF_TYPE_UART
     
    NWP_IF_WRITE_CHECK(g_pCB->FD, (_u8 *)&g_H2NSyncPattern.Long, 2*SYNC_PATTERN_LEN);
#else
     
    NWP_IF_WRITE_CHECK(g_pCB->FD, (_u8 *)&g_H2NSyncPattern.Short, SYNC_PATTERN_LEN);
#endif

     
    NWP_IF_WRITE_CHECK(g_pCB->FD, (_u8 *)&g_pCB->TempProtocolHeader, _SL_CMD_HDR_SIZE);

     
    if (pTxRxDescBuff && pCmdCtrl->TxDescLen > 0)
    {
    	NWP_IF_WRITE_CHECK(g_pCB->FD, pTxRxDescBuff, 
                           _SL_PROTOCOL_ALIGN_SIZE(pCmdCtrl->TxDescLen));
    }

     
     
     
    if (sendRxPayload == TRUE )
    {
     	NWP_IF_WRITE_CHECK(g_pCB->FD, pCmdExt->pRxPayload, 
                           _SL_PROTOCOL_ALIGN_SIZE(pCmdExt->RxPayloadLen));
    }

     
    if (pCmdExt && pCmdExt->TxPayloadLen > 0)
    {
         
         
        VERIFY_PROTOCOL(_SL_IS_PROTOCOL_ALIGNED_SIZE(pCmdCtrl->TxDescLen));

    	NWP_IF_WRITE_CHECK(g_pCB->FD, pCmdExt->pTxPayload, 
                           _SL_PROTOCOL_ALIGN_SIZE(pCmdExt->TxPayloadLen));
    }


    _SL_DBG_CNT_INC(MsgCnt.Write);

#ifdef SL_START_WRITE_STAT
    sl_IfEndWriteSequence(g_pCB->FD);
#endif

    return SL_OS_RET_CODE_OK;
}

 
 
 
_SlReturnVal_t _SlDrvMsgRead(void)
{
     
    union
    {      
      _u8             TempBuf[_SL_RESP_HDR_SIZE];
      _u32            DummyBuf[2];
    } uBuf;
    _u8               TailBuffer[4];
    _u16              LengthToCopy;
    _u16              AlignedLengthRecv;
    _u8               AlignSize;
    _u8               *pAsyncBuf = NULL;
    _u16              OpCode;
    _u16              RespPayloadLen;
    _u8               sd = SL_MAX_SOCKETS;
    _SlRxMsgClass_e   RxMsgClass;
    

     
    g_pCB->FunctionParams.AsyncExt.pAsyncBuf      = NULL;
    g_pCB->FunctionParams.AsyncExt.AsyncEvtHandler= NULL;

    
    VERIFY_RET_OK(_SlDrvRxHdrRead((_u8*)(uBuf.TempBuf), &AlignSize));

    OpCode = OPCODE(uBuf.TempBuf);
    RespPayloadLen = RSP_PAYLOAD_LEN(uBuf.TempBuf);


     
    if(SL_OPCODE_DEVICE_INITCOMPLETE != OpCode)
    {
        g_pCB->FlowContCB.TxPoolCnt = ((_SlResponseHeader_t *)uBuf.TempBuf)->TxPoolCnt;
        g_pCB->SocketNonBlocking = ((_SlResponseHeader_t *)uBuf.TempBuf)->SocketNonBlocking;
        g_pCB->SocketTXFailure = ((_SlResponseHeader_t *)uBuf.TempBuf)->SocketTXFailure;

        if(g_pCB->FlowContCB.TxPoolCnt > FLOW_CONT_MIN)
        {
            _SlDrvSyncObjSignal(&g_pCB->FlowContCB.TxSyncObj);
        }
    }

     
    _SlDrvClassifyRxMsg(OpCode);
    
    RxMsgClass = g_pCB->FunctionParams.AsyncExt.RxMsgClass;


    switch(RxMsgClass)
    {
    case ASYNC_EVT_CLASS:

            VERIFY_PROTOCOL(NULL == pAsyncBuf);

#if (SL_MEMORY_MGMT == SL_MEMORY_MGMT_DYNAMIC)
        g_pCB->FunctionParams.AsyncExt.pAsyncBuf = sl_Malloc(SL_ASYNC_MAX_MSG_LEN);
#else
        g_pCB->FunctionParams.AsyncExt.pAsyncBuf = g_StatMem.AsyncRespBuf;
#endif
             
            pAsyncBuf = g_pCB->FunctionParams.AsyncExt.pAsyncBuf;

             
            _SlDrvMemZero(pAsyncBuf, SL_ASYNC_MAX_MSG_LEN);
            
            MALLOC_OK_CHECK(pAsyncBuf);

            sl_Memcpy(pAsyncBuf, uBuf.TempBuf, _SL_RESP_HDR_SIZE);
			if (_SL_PROTOCOL_ALIGN_SIZE(RespPayloadLen) <= SL_ASYNC_MAX_PAYLOAD_LEN)
			{
				AlignedLengthRecv = _SL_PROTOCOL_ALIGN_SIZE(RespPayloadLen);
			}
			else
			{
				AlignedLengthRecv = _SL_PROTOCOL_ALIGN_SIZE(SL_ASYNC_MAX_PAYLOAD_LEN);
			}
            if (RespPayloadLen > 0)
            {
                NWP_IF_READ_CHECK(g_pCB->FD,
                                  pAsyncBuf + _SL_RESP_HDR_SIZE,
                                  AlignedLengthRecv);
        }
         
			if ((_SL_PROTOCOL_ALIGN_SIZE(RespPayloadLen) > SL_ASYNC_MAX_PAYLOAD_LEN))
        {
				AlignedLengthRecv = _SL_PROTOCOL_ALIGN_SIZE(RespPayloadLen) - SL_ASYNC_MAX_PAYLOAD_LEN;
            while (AlignedLengthRecv > 0)
            {
                NWP_IF_READ_CHECK(g_pCB->FD,TailBuffer,4);
                AlignedLengthRecv = AlignedLengthRecv - 4;
            }
        }
            
            _SlDrvProtectionObjLockWaitForever();
          
			if (
#ifndef SL_TINY_EXT               
                (SL_OPCODE_SOCKET_ACCEPTASYNCRESPONSE == OpCode) || 
                (SL_OPCODE_SOCKET_ACCEPTASYNCRESPONSE_V6 == OpCode) || 
#endif                
                (SL_OPCODE_SOCKET_CONNECTASYNCRESPONSE == OpCode)
               )
			{
				 
				sd = ((((_SocketResponse_t *)(pAsyncBuf + _SL_RESP_HDR_SIZE))->sd) & BSD_SOCKET_ID_MASK);
			}
			_SlFindAndSetActiveObj(OpCode, sd);

            _SlDrvProtectionObjUnLock();

            break;
    case RECV_RESP_CLASS:
        {
            _u8   ExpArgSize;  

                switch(OpCode)
            {
            case SL_OPCODE_SOCKET_RECVFROMASYNCRESPONSE:
                ExpArgSize = RECVFROM_IPV4_ARGS_SIZE;
                break;
#ifndef SL_TINY_EXT                        
            case SL_OPCODE_SOCKET_RECVFROMASYNCRESPONSE_V6:
                ExpArgSize = RECVFROM_IPV6_ARGS_SIZE;
                break;
#endif                        
            default:
                 
                ExpArgSize = RECV_ARGS_SIZE;
            }              

             
             
            NWP_IF_READ_CHECK(g_pCB->FD, &uBuf.TempBuf[4], RECV_ARGS_SIZE);

             
            VERIFY_PROTOCOL((SD(&uBuf.TempBuf[4])& BSD_SOCKET_ID_MASK) < SL_MAX_SOCKETS);

                 _SlDrvProtectionObjLockWaitForever();

             
				VERIFY_RET_OK(_SlFindAndSetActiveObj(OpCode,SD(&uBuf.TempBuf[4]) & BSD_SOCKET_ID_MASK));

             
            VERIFY_SOCKET_CB(NULL !=  ((_SlArgsData_t *)(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pData))->pArgs);	

            sl_Memcpy( ((_SlArgsData_t *)(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs))->pArgs, &uBuf.TempBuf[4], RECV_ARGS_SIZE);

            if(ExpArgSize > RECV_ARGS_SIZE)
            {
                NWP_IF_READ_CHECK(g_pCB->FD,
                    ((_SlArgsData_t *)(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs))->pArgs + RECV_ARGS_SIZE,
                    ExpArgSize - RECV_ARGS_SIZE);
            }

             
             
             
            if(ACT_DATA_SIZE(&uBuf.TempBuf[4]) > 0)
            {       
                VERIFY_SOCKET_CB(NULL != ((_SlArgsData_t *)(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs))->pData);

                 
                 
                 
                 
                LengthToCopy = ACT_DATA_SIZE(&uBuf.TempBuf[4]) & (3);
                AlignedLengthRecv = ACT_DATA_SIZE(&uBuf.TempBuf[4]) & (~3);
                if( AlignedLengthRecv >= 4)
                {
                    NWP_IF_READ_CHECK(g_pCB->FD,((_SlArgsData_t *)(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs))->pData,AlignedLengthRecv );                      
                }
                 
                if( LengthToCopy > 0) 
                {
                    NWP_IF_READ_CHECK(g_pCB->FD,TailBuffer,4);
                     
                    sl_Memcpy(((_SlArgsData_t *)(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs))->pData + AlignedLengthRecv,TailBuffer,LengthToCopy);                    
                }                  
            }
                 _SlDrvSyncObjSignal(&g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].SyncObj);
                 _SlDrvProtectionObjUnLock();
        }
        break;

    case CMD_RESP_CLASS:

         
         
         
         
         
         
        NWP_IF_READ_CHECK(g_pCB->FD,
            g_pCB->FunctionParams.pTxRxDescBuff,
            _SL_PROTOCOL_ALIGN_SIZE(g_pCB->FunctionParams.pCmdCtrl->RxDescLen));

        if((NULL != g_pCB->FunctionParams.pCmdExt) && (0 != g_pCB->FunctionParams.pCmdExt->RxPayloadLen))
        {
             
            _i16    ActDataSize = RSP_PAYLOAD_LEN(uBuf.TempBuf) - g_pCB->FunctionParams.pCmdCtrl->RxDescLen;

            g_pCB->FunctionParams.pCmdExt->ActualRxPayloadLen = ActDataSize;

             
            if(ActDataSize <= 0)
            {
                g_pCB->FunctionParams.pCmdExt->RxPayloadLen = 0;
            }
            else
            {
                 
                if (ActDataSize > g_pCB->FunctionParams.pCmdExt->RxPayloadLen)
                {
                    LengthToCopy = g_pCB->FunctionParams.pCmdExt->RxPayloadLen & (3);
                    AlignedLengthRecv = g_pCB->FunctionParams.pCmdExt->RxPayloadLen & (~3);
                }
                else
                {
                    LengthToCopy = ActDataSize & (3);
                    AlignedLengthRecv = ActDataSize & (~3);
                }
                 
                 
                 
                 

                if( AlignedLengthRecv >= 4)
                {
                    NWP_IF_READ_CHECK(g_pCB->FD,
                        g_pCB->FunctionParams.pCmdExt->pRxPayload,
                        AlignedLengthRecv );

                }
                 
                if( LengthToCopy > 0) 
                {
                    NWP_IF_READ_CHECK(g_pCB->FD,TailBuffer,4);
                     
                    sl_Memcpy(g_pCB->FunctionParams.pCmdExt->pRxPayload + AlignedLengthRecv,
                        TailBuffer,
                        LengthToCopy);
                    ActDataSize = ActDataSize-4;
                }
                 
                if (ActDataSize > g_pCB->FunctionParams.pCmdExt->RxPayloadLen)
                {
                     
                    AlignedLengthRecv = ActDataSize - (g_pCB->FunctionParams.pCmdExt->RxPayloadLen & (~3));
                    while( AlignedLengthRecv > 0)
                    {
                        NWP_IF_READ_CHECK(g_pCB->FD,TailBuffer, 4 );
                        AlignedLengthRecv = AlignedLengthRecv - 4;
                    }
                }
            }
        }
        break;

    default:
         
        break;
    }

    if(AlignSize > 0)
    {
        NWP_IF_READ_CHECK(g_pCB->FD, uBuf.TempBuf, AlignSize);
    }

    _SL_DBG_CNT_INC(MsgCnt.Read);

     
    sl_IfUnMaskIntHdlr();

    return SL_OS_RET_CODE_OK;
}


 
 
 
void _SlAsyncEventGenericHandler(void)
{
    _u32 SlAsyncEvent = 0;
    _u8  OpcodeFound = FALSE; 
    _u8  i;
    
    _u32* pEventLocation  = NULL;  
    _SlResponseHeader_t  *pHdr       = (_SlResponseHeader_t *)g_pCB->FunctionParams.AsyncExt.pAsyncBuf;


     
    if (g_pCB->FunctionParams.AsyncExt.AsyncEvtHandler == NULL)
        return;

     
    for (i=0; i< (sizeof(OpcodeTranslateTable) / sizeof(OpcodeKeyVal_t)); i++)
    {
        if (OpcodeTranslateTable[i].opcode == pHdr->GenHeader.Opcode)
        {
            SlAsyncEvent = OpcodeTranslateTable[i].event;
            OpcodeFound = TRUE;
            break;
        }
    }

     
    if (OpcodeFound == FALSE)
    {
         
        g_pCB->FunctionParams.AsyncExt.AsyncEvtHandler(g_pCB->FunctionParams.AsyncExt.pAsyncBuf);
    }
    else
    {
        
       pEventLocation = (_u32*)(g_pCB->FunctionParams.AsyncExt.pAsyncBuf + sizeof (_SlResponseHeader_t) - sizeof(SlAsyncEvent) );

        
       *pEventLocation = SlAsyncEvent;

        
       g_pCB->FunctionParams.AsyncExt.AsyncEvtHandler(pEventLocation);
    }

     
}


 
 
 
_SlReturnVal_t _SlDrvMsgReadCmdCtx(void)
{

     
     
     
     
     
     
    while (TRUE == g_pCB->IsCmdRespWaited)
    {
        if(_SL_PENDING_RX_MSG(g_pCB))
        {
            VERIFY_RET_OK(_SlDrvMsgRead());
            g_pCB->RxDoneCnt++;

            if (CMD_RESP_CLASS == g_pCB->FunctionParams.AsyncExt.RxMsgClass)
            {
                g_pCB->IsCmdRespWaited = FALSE;

                 
                 
                sl_SyncObjClear(&g_pCB->CmdSyncObj);
            }
            else if (ASYNC_EVT_CLASS == g_pCB->FunctionParams.AsyncExt.RxMsgClass)
            {
                 
                 
                 
                 
                 
                 
                _SlAsyncEventGenericHandler();
                
                
#if (SL_MEMORY_MGMT == SL_MEMORY_MGMT_DYNAMIC)
                sl_Free(g_pCB->FunctionParams.AsyncExt.pAsyncBuf);
#else
                g_pCB->FunctionParams.AsyncExt.pAsyncBuf = NULL;
#endif
            }
        }
        else
        {
             
             _SlDrvSyncObjWaitForever(&g_pCB->CmdSyncObj);
        }
    }

     
     
     
     
     

    _SlDrvObjUnLock(&g_pCB->GlobalLockObj);
    
    if(_SL_PENDING_RX_MSG(g_pCB))
    {
        sl_Spawn((_SlSpawnEntryFunc_t)_SlDrvMsgReadSpawnCtx, NULL, 0);
    }

    return SL_OS_RET_CODE_OK;
}

 
 
 
_SlReturnVal_t _SlDrvMsgReadSpawnCtx(void *pValue)
{
#ifdef SL_POLLING_MODE_USED
    _i16 retCode = OSI_OK;
     
    do
    {
        retCode = sl_LockObjLock(&g_pCB->GlobalLockObj, 0);
        if ( OSI_OK != retCode )
        {
            if (TRUE == g_pCB->IsCmdRespWaited)
            {
                _SlDrvSyncObjSignal(&g_pCB->CmdSyncObj);
                return SL_RET_CODE_OK;
            }
        }

    }
    while (OSI_OK != retCode);

#else
    _SlDrvObjLockWaitForever(&g_pCB->GlobalLockObj);
#endif


     
     
    if(FALSE == (_SL_PENDING_RX_MSG(g_pCB)))
    {
        _SlDrvObjUnLock(&g_pCB->GlobalLockObj);
        
        return SL_RET_CODE_OK;
    }

    VERIFY_RET_OK(_SlDrvMsgRead());

    g_pCB->RxDoneCnt++;

    switch(g_pCB->FunctionParams.AsyncExt.RxMsgClass)
    {
    case ASYNC_EVT_CLASS:
         
         
        VERIFY_PROTOCOL(NULL != g_pCB->FunctionParams.AsyncExt.pAsyncBuf);
   
        _SlAsyncEventGenericHandler();        
        
#if (SL_MEMORY_MGMT == SL_MEMORY_MGMT_DYNAMIC)
        sl_Free(g_pCB->FunctionParams.AsyncExt.pAsyncBuf);
#else
        g_pCB->FunctionParams.AsyncExt.pAsyncBuf = NULL;
#endif
        break;
    case DUMMY_MSG_CLASS:
    case RECV_RESP_CLASS:
         
        break;
    case CMD_RESP_CLASS:
         
         
    default:
        VERIFY_PROTOCOL(0);
    }

    _SlDrvObjUnLock(&g_pCB->GlobalLockObj);

    return(SL_RET_CODE_OK);
}



 

 
const _SlSpawnEntryFunc_t RxMsgClassLUT[] = {
    (_SlSpawnEntryFunc_t)_SlDrvDeviceEventHandler,  
#if defined(sl_WlanEvtHdlr) || defined(EXT_LIB_REGISTERED_WLAN_EVENTS)
    (_SlSpawnEntryFunc_t)_SlDrvHandleWlanEvents,            
#else
    NULL,
#endif
#if defined (sl_SockEvtHdlr) || defined(EXT_LIB_REGISTERED_SOCK_EVENTS)
    (_SlSpawnEntryFunc_t)_SlDrvHandleSockEvents,    
#else
    NULL,
#endif

#if defined(sl_NetAppEvtHdlr) || defined(EXT_LIB_REGISTERED_NETAPP_EVENTS)
    (_SlSpawnEntryFunc_t)_SlDrvHandleNetAppEvents,  
#else
    NULL,  
#endif
    NULL,                                           
    NULL,                                           
    NULL,
    NULL
};


 
 
 
void _SlDrvClassifyRxMsg(
    _SlOpcode_t         Opcode)
{
    _SlSpawnEntryFunc_t AsyncEvtHandler = NULL;
    _SlRxMsgClass_e     RxMsgClass  = CMD_RESP_CLASS;
    _u8               Silo;
    

	if (0 == (SL_OPCODE_SYNC & Opcode))
	{    
        
		if (SL_OPCODE_DEVICE_DEVICEASYNCDUMMY == Opcode)
		{ 
		    RxMsgClass = DUMMY_MSG_CLASS;
		}
		else if ( (SL_OPCODE_SOCKET_RECVASYNCRESPONSE == Opcode) || (SL_OPCODE_SOCKET_RECVFROMASYNCRESPONSE == Opcode) 
#ifndef SL_TINY_EXT                      
                    || (SL_OPCODE_SOCKET_RECVFROMASYNCRESPONSE_V6 == Opcode) 
#endif                    
                 ) 
		{
			RxMsgClass = RECV_RESP_CLASS;
		}
		else
		{
             
            RxMsgClass = ASYNC_EVT_CLASS;
        
		     
		    Silo = ((Opcode >> SL_OPCODE_SILO_OFFSET) & 0x7);

            VERIFY_PROTOCOL(Silo < (sizeof(RxMsgClassLUT)/sizeof(_SlSpawnEntryFunc_t)));

             
            AsyncEvtHandler = RxMsgClassLUT[Silo];
            
            if ((SL_OPCODE_NETAPP_HTTPGETTOKENVALUE == Opcode) || (SL_OPCODE_NETAPP_HTTPPOSTTOKENVALUE == Opcode))
            {
                AsyncEvtHandler = _SlDrvNetAppEventHandler;
            }
#ifndef SL_TINY_EXT            
            else if (SL_OPCODE_NETAPP_PINGREPORTREQUESTRESPONSE == Opcode)
            {
                AsyncEvtHandler = (_SlSpawnEntryFunc_t)_sl_HandleAsync_PingResponse;
            }
#endif
		}
	}

    g_pCB->FunctionParams.AsyncExt.RxMsgClass = RxMsgClass; 
    g_pCB->FunctionParams.AsyncExt.AsyncEvtHandler = AsyncEvtHandler;
    	 
}


 
 
 
_SlReturnVal_t   _SlDrvRxHdrRead(_u8 *pBuf, _u8 *pAlignSize)
{
     _u32       SyncCnt  = 0;
    _u8        ShiftIdx;  

#ifndef SL_IF_TYPE_UART
     
    NWP_IF_WRITE_CHECK(g_pCB->FD, (_u8 *)&g_H2NCnysPattern.Short, SYNC_PATTERN_LEN);
#endif

     
    NWP_IF_READ_CHECK(g_pCB->FD, &pBuf[0], 4);
    _SL_DBG_SYNC_LOG(SyncCnt,pBuf);

     
    while ( ! N2H_SYNC_PATTERN_MATCH(pBuf, g_pCB->TxSeqNum) )
    {
         
        VERIFY_PROTOCOL(SyncCnt < SL_SYNC_SCAN_THRESHOLD);

         
        if(0 == (SyncCnt % (_u32)SYNC_PATTERN_LEN))
        {
            NWP_IF_READ_CHECK(g_pCB->FD, &pBuf[4], 4);
            _SL_DBG_SYNC_LOG(SyncCnt,pBuf);
        }

         
        for(ShiftIdx = 0; ShiftIdx< 7; ShiftIdx++)
        {
            pBuf[ShiftIdx] = pBuf[ShiftIdx+1];
        }             
        pBuf[7] = 0;

        SyncCnt++;
    }

     
    SyncCnt %= SYNC_PATTERN_LEN;

    if(SyncCnt > 0)
    {
        *(_u32 *)&pBuf[0] = *(_u32 *)&pBuf[4];
        NWP_IF_READ_CHECK(g_pCB->FD, &pBuf[SYNC_PATTERN_LEN - SyncCnt], (_u16)SyncCnt);
    }
    else
    {
        NWP_IF_READ_CHECK(g_pCB->FD, &pBuf[0], 4);
    }

     
    while ( N2H_SYNC_PATTERN_MATCH(pBuf, g_pCB->TxSeqNum) )
    {
        _SL_DBG_CNT_INC(Work.DoubleSyncPattern);
        NWP_IF_READ_CHECK(g_pCB->FD, &pBuf[0], SYNC_PATTERN_LEN);
    }
    g_pCB->TxSeqNum++;

     
    NWP_IF_READ_CHECK(g_pCB->FD, &pBuf[SYNC_PATTERN_LEN], _SL_RESP_SPEC_HDR_SIZE);

     
     
    *pAlignSize = (_u8)((SyncCnt > 0) ? (SYNC_PATTERN_LEN - SyncCnt) : 0);

    return SL_RET_CODE_OK;
}

 
 
 
typedef union
{
    _BasicResponse_t	Rsp;
}_SlBasicCmdMsg_u;


#ifndef SL_TINY_EXT
_i16 _SlDrvBasicCmd(_SlOpcode_t Opcode)
{
    _SlBasicCmdMsg_u       Msg = {{0, 0}};
    _SlCmdCtrl_t           CmdCtrl;

    CmdCtrl.Opcode = Opcode;
    CmdCtrl.TxDescLen = 0;
    CmdCtrl.RxDescLen = sizeof(_BasicResponse_t);


    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&CmdCtrl, &Msg, NULL));

    return (_i16)Msg.Rsp.status;
}

 
_SlReturnVal_t _SlDrvCmdSend(
    _SlCmdCtrl_t  *pCmdCtrl ,
    void          *pTxRxDescBuff ,
    _SlCmdExt_t   *pCmdExt)
{
    _SlReturnVal_t RetVal;
    _u8            IsCmdRespWaitedOriginalVal;

    _SlFunctionParams_t originalFuncParms;

     
    IsCmdRespWaitedOriginalVal = g_pCB->IsCmdRespWaited;

     
    sl_Memcpy(&originalFuncParms,  &g_pCB->FunctionParams, sizeof(_SlFunctionParams_t));

    g_pCB->IsCmdRespWaited = FALSE;
  
    SL_TRACE0(DBG_MSG, MSG_312, "_SlDrvCmdSend: call _SlDrvMsgWrite");

     
    RetVal = _SlDrvMsgWrite(pCmdCtrl, pCmdExt, pTxRxDescBuff);

     
    g_pCB->IsCmdRespWaited = IsCmdRespWaitedOriginalVal;

     
    sl_Memcpy(&g_pCB->FunctionParams, &originalFuncParms, sizeof(_SlFunctionParams_t));

    return RetVal;


}
#endif

 
 
 
_u8 _SlDrvWaitForPoolObj(_u8 ActionID, _u8 SocketID)
{
    _u8 CurrObjIndex = MAX_CONCURRENT_ACTIONS;

     
            _SlDrvProtectionObjLockWaitForever();
    if (MAX_CONCURRENT_ACTIONS > g_pCB->FreePoolIdx)
    {
         
        CurrObjIndex = g_pCB->FreePoolIdx;
         
#ifndef SL_TINY_EXT
        if (MAX_CONCURRENT_ACTIONS > g_pCB->ObjPool[CurrObjIndex].NextIndex)
        {
            g_pCB->FreePoolIdx = g_pCB->ObjPool[CurrObjIndex].NextIndex;
        }
        else
#endif           
        {
             
            g_pCB->FreePoolIdx = MAX_CONCURRENT_ACTIONS;
        }
    }
    else
    {
		_SlDrvProtectionObjUnLock();
        return CurrObjIndex;
    }
    g_pCB->ObjPool[CurrObjIndex].ActionID = (_u8)ActionID;
    if (SL_MAX_SOCKETS > SocketID)
    {
        g_pCB->ObjPool[CurrObjIndex].AdditionalData = SocketID;
    }
#ifndef SL_TINY_EXT
     
	while ( ( (SL_MAX_SOCKETS > SocketID) && (g_pCB->ActiveActionsBitmap & (1<<SocketID)) ) || 
            ( (g_pCB->ActiveActionsBitmap & (1<<ActionID)) && (SL_MAX_SOCKETS == SocketID) ) )
    {
         
        g_pCB->ObjPool[CurrObjIndex].NextIndex = g_pCB->PendingPoolIdx;
        g_pCB->PendingPoolIdx = CurrObjIndex;
		_SlDrvProtectionObjUnLock();
        
         
        _SlDrvSyncObjWaitForever(&g_pCB->ObjPool[CurrObjIndex].SyncObj);
        
         
        _SlDrvProtectionObjLockWaitForever();
    }
#endif
     
    if (SL_MAX_SOCKETS > SocketID)
    {
        g_pCB->ActiveActionsBitmap |= (1<<SocketID);
    }
    else
    {
        g_pCB->ActiveActionsBitmap |= (1<<ActionID);
    }
     
    g_pCB->ObjPool[CurrObjIndex].NextIndex = g_pCB->ActivePoolIdx;
    g_pCB->ActivePoolIdx = CurrObjIndex;	
     
	_SlDrvProtectionObjUnLock();
    return CurrObjIndex;
}

 
 
 
void _SlDrvReleasePoolObj(_u8 ObjIdx)
{
#ifndef SL_TINY_EXT        
    _u8 PendingIndex;
#endif

     _SlDrvProtectionObjLockWaitForever();

       
#ifndef SL_TINY_EXT
     
	PendingIndex = g_pCB->PendingPoolIdx;
        
	while(MAX_CONCURRENT_ACTIONS > PendingIndex)
	{
		 
		if ( (g_pCB->ObjPool[PendingIndex].ActionID == g_pCB->ObjPool[ObjIdx].ActionID) && 
			( (SL_MAX_SOCKETS == (g_pCB->ObjPool[PendingIndex].AdditionalData & BSD_SOCKET_ID_MASK)) || 
			((SL_MAX_SOCKETS > (g_pCB->ObjPool[ObjIdx].AdditionalData & BSD_SOCKET_ID_MASK)) && ( (g_pCB->ObjPool[PendingIndex].AdditionalData & BSD_SOCKET_ID_MASK) == (g_pCB->ObjPool[ObjIdx].AdditionalData & BSD_SOCKET_ID_MASK) ))) )
		{
			 
			_SlRemoveFromList(&g_pCB->PendingPoolIdx, PendingIndex);
			 _SlDrvSyncObjSignal(&g_pCB->ObjPool[PendingIndex].SyncObj);
			 break;
		}
		PendingIndex = g_pCB->ObjPool[PendingIndex].NextIndex;
	}
#endif

		if (SL_MAX_SOCKETS > (g_pCB->ObjPool[ObjIdx].AdditionalData & BSD_SOCKET_ID_MASK))
		{
		 
			g_pCB->ActiveActionsBitmap &= ~(1<<(g_pCB->ObjPool[ObjIdx].AdditionalData & BSD_SOCKET_ID_MASK));
		}
		else
		{
		 
			g_pCB->ActiveActionsBitmap &= ~(1<<g_pCB->ObjPool[ObjIdx].ActionID);
		}	

     
    g_pCB->ObjPool[ObjIdx].pRespArgs = NULL;
    g_pCB->ObjPool[ObjIdx].ActionID = 0;
    g_pCB->ObjPool[ObjIdx].AdditionalData = SL_MAX_SOCKETS;

     
    _SlRemoveFromList(&g_pCB->ActivePoolIdx, ObjIdx);
     
    g_pCB->ObjPool[ObjIdx].NextIndex = g_pCB->FreePoolIdx;
    g_pCB->FreePoolIdx = ObjIdx;
	_SlDrvProtectionObjUnLock();
}


 
 
 
void _SlRemoveFromList(_u8 *ListIndex, _u8 ItemIndex)
{
#ifndef SL_TINY_EXT  
	_u8 Idx;
#endif        
        
    if (MAX_CONCURRENT_ACTIONS == g_pCB->ObjPool[*ListIndex].NextIndex)
    {
        *ListIndex = MAX_CONCURRENT_ACTIONS;
    }
     
#ifndef SL_TINY_EXT
	 
	else if (*ListIndex == ItemIndex)
	{
		*ListIndex = g_pCB->ObjPool[ItemIndex].NextIndex;
	}
	else
	{
              Idx = *ListIndex;
      
              while(MAX_CONCURRENT_ACTIONS > Idx)
              {
                   
                  if (g_pCB->ObjPool[Idx].NextIndex == ItemIndex)
                  {
                          g_pCB->ObjPool[Idx].NextIndex = g_pCB->ObjPool[ItemIndex].NextIndex;
                          break;
                  }

                  Idx = g_pCB->ObjPool[Idx].NextIndex;
              }
	}
#endif    
}


 
 
 
_SlReturnVal_t _SlFindAndSetActiveObj(_SlOpcode_t  Opcode, _u8 Sd)
{
    _u8 ActiveIndex;

    ActiveIndex = g_pCB->ActivePoolIdx;
     
#ifndef SL_TINY_EXT    
		while (MAX_CONCURRENT_ACTIONS > ActiveIndex)
#else
         
        if (MAX_CONCURRENT_ACTIONS > ActiveIndex)
#endif
    {
         
        if (g_pCB->ObjPool[ActiveIndex].AdditionalData & SL_NETAPP_FAMILY_MASK)
        {
            Opcode &= ~SL_OPCODE_IPV6;
        }

        if ((g_pCB->ObjPool[ActiveIndex].ActionID == RECV_ID) && (Sd == g_pCB->ObjPool[ActiveIndex].AdditionalData) && 
						( (SL_OPCODE_SOCKET_RECVASYNCRESPONSE == Opcode) || (SL_OPCODE_SOCKET_RECVFROMASYNCRESPONSE == Opcode)
#ifndef SL_TINY_EXT
                        || (SL_OPCODE_SOCKET_RECVFROMASYNCRESPONSE_V6 == Opcode) 
#endif
                          ) 

               )
        {
            g_pCB->FunctionParams.AsyncExt.ActionIndex = ActiveIndex;
            return SL_RET_CODE_OK;
        }
         
        if ( (_SlActionLookupTable[ g_pCB->ObjPool[ActiveIndex].ActionID - MAX_SOCKET_ENUM_IDX].ActionAsyncOpcode == Opcode) && 
            ( ((Sd == (g_pCB->ObjPool[ActiveIndex].AdditionalData & BSD_SOCKET_ID_MASK) ) && (SL_MAX_SOCKETS > Sd)) || (SL_MAX_SOCKETS == (g_pCB->ObjPool[ActiveIndex].AdditionalData & BSD_SOCKET_ID_MASK)) ) )
        {
             
            g_pCB->FunctionParams.AsyncExt.AsyncEvtHandler = _SlActionLookupTable[ g_pCB->ObjPool[ActiveIndex].ActionID - MAX_SOCKET_ENUM_IDX].AsyncEventHandler;
            g_pCB->FunctionParams.AsyncExt.ActionIndex = ActiveIndex;
            return SL_RET_CODE_OK;
        }
        ActiveIndex = g_pCB->ObjPool[ActiveIndex].NextIndex;
    }

    return SL_RET_CODE_SELF_ERROR;

 

}


 

void  _SlDrvSyncObjWaitForever(_SlSyncObj_t *pSyncObj)
{
    OSI_RET_OK_CHECK(sl_SyncObjWait(pSyncObj, SL_OS_WAIT_FOREVER));
}

void  _SlDrvSyncObjSignal(_SlSyncObj_t *pSyncObj)
{
    OSI_RET_OK_CHECK(sl_SyncObjSignal(pSyncObj));
}

void _SlDrvObjLockWaitForever(_SlLockObj_t *pLockObj)
{
    OSI_RET_OK_CHECK(sl_LockObjLock(pLockObj, SL_OS_WAIT_FOREVER));
}

void _SlDrvProtectionObjLockWaitForever()
{
    OSI_RET_OK_CHECK(sl_LockObjLock(&g_pCB->ProtectionLockObj, SL_OS_WAIT_FOREVER));

}

void _SlDrvObjUnLock(_SlLockObj_t *pLockObj)
{
    OSI_RET_OK_CHECK(sl_LockObjUnlock(pLockObj));

}

void _SlDrvProtectionObjUnLock()
{
    OSI_RET_OK_CHECK(sl_LockObjUnlock(&g_pCB->ProtectionLockObj));
}


void _SlDrvMemZero(void* Addr, _u16 size)
{
    sl_Memset(Addr, 0, size);
}


void _SlDrvResetCmdExt(_SlCmdExt_t* pCmdExt)
{
    _SlDrvMemZero(pCmdExt, sizeof (_SlCmdExt_t));
}




