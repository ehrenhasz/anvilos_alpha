 
    


 
 
 
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"

 
 
 
#define NETAPP_MDNS_OPTIONS_ADD_SERVICE_BIT					 ((_u32)0x1 << 31)

#ifdef SL_TINY
#define NETAPP_MDNS_MAX_SERVICE_NAME_AND_TEXT_LENGTH         63
#else
#define NETAPP_MDNS_MAX_SERVICE_NAME_AND_TEXT_LENGTH         255
#endif


 
 
 
void _sl_HandleAsync_DnsGetHostByName(void *pVoidBuf);

#ifndef SL_TINY_EXT
void _sl_HandleAsync_DnsGetHostByService(void *pVoidBuf);
void _sl_HandleAsync_PingResponse(void *pVoidBuf);
#endif

void CopyPingResultsToReport(_PingReportResponse_t *pResults,SlPingReport_t *pReport);
_i16 sl_NetAppMDNSRegisterUnregisterService(const _i8* 		pServiceName, 
											const _u8   ServiceNameLen,
											const _i8* 		pText,
											const _u8   TextLen,
											const _u16  Port,
											const _u32    TTL,
											const _u32    Options);

#if defined(sl_HttpServerCallback) || defined(EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS)
_u16 _sl_NetAppSendTokenValue(slHttpServerData_t * Token);
#endif
typedef union
{
	_NetAppStartStopCommand_t       Cmd;
	_NetAppStartStopResponse_t   Rsp;
}_SlNetAppStartStopMsg_u;


#if _SL_INCLUDE_FUNC(sl_NetAppStart)

const _SlCmdCtrl_t _SlNetAppStartCtrl =
{
    SL_OPCODE_NETAPP_START_COMMAND,
    sizeof(_NetAppStartStopCommand_t),
    sizeof(_NetAppStartStopResponse_t)
};

_i16 sl_NetAppStart(const _u32 AppBitMap)
{
    _SlNetAppStartStopMsg_u Msg;
    Msg.Cmd.appId = AppBitMap;
    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlNetAppStartCtrl, &Msg, NULL));

    return Msg.Rsp.status;
}
#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppStop)


const _SlCmdCtrl_t _SlNetAppStopCtrl =
{
    SL_OPCODE_NETAPP_STOP_COMMAND,
    sizeof(_NetAppStartStopCommand_t),
    sizeof(_NetAppStartStopResponse_t)
};



_i16 sl_NetAppStop(const _u32 AppBitMap)
{
    _SlNetAppStartStopMsg_u Msg;
    Msg.Cmd.appId = AppBitMap;
    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlNetAppStopCtrl, &Msg, NULL));

    return Msg.Rsp.status;
}
#endif


 
 
 
typedef struct
{
    _u8  IndexOffest;
    _u8  MaxServiceCount;
    _u8  Flags;
    _i8  Padding;
}NetappGetServiceListCMD_t;

typedef union
{
	 NetappGetServiceListCMD_t      Cmd;
	_BasicResponse_t                Rsp;
}_SlNetappGetServiceListMsg_u;


#if _SL_INCLUDE_FUNC(sl_NetAppGetServiceList)

const _SlCmdCtrl_t _SlGetServiceListeCtrl =
{
    SL_OPCODE_NETAPP_NETAPP_MDNS_LOOKUP_SERVICE,
    sizeof(NetappGetServiceListCMD_t),
    sizeof(_BasicResponse_t)
};

_i16 sl_NetAppGetServiceList(const _u8  IndexOffest,
						     const _u8  MaxServiceCount,
							 const _u8  Flags,
						           _i8  *pBuffer,
							 const _u32  RxBufferLength
							)
{

    _i32 					 retVal= 0;
    _SlNetappGetServiceListMsg_u Msg;
    _SlCmdExt_t                  CmdExt;
	_u16               ServiceSize = 0;
	_u16               BufferSize = 0;

	 
    switch(Flags)
    {
        case SL_NET_APP_FULL_SERVICE_WITH_TEXT_IPV4_TYPE:
            ServiceSize =  sizeof(SlNetAppGetFullServiceWithTextIpv4List_t);
            break;

        case SL_NET_APP_FULL_SERVICE_IPV4_TYPE:
            ServiceSize =  sizeof(SlNetAppGetFullServiceIpv4List_t);
            break;

        case SL_NET_APP_SHORT_SERVICE_IPV4_TYPE:
            ServiceSize =  sizeof(SlNetAppGetShortServiceIpv4List_t);
            break;

        default:
			ServiceSize =  sizeof(_BasicResponse_t);
			break;
    }



	BufferSize =  MaxServiceCount * ServiceSize;

	 
	if(RxBufferLength <= BufferSize)
	{
		return SL_ERROR_NETAPP_RX_BUFFER_LENGTH_ERROR;
	}

    _SlDrvResetCmdExt(&CmdExt);
    CmdExt.RxPayloadLen = BufferSize;
    CmdExt.pRxPayload = (_u8 *)pBuffer; 

    Msg.Cmd.IndexOffest		= IndexOffest;
    Msg.Cmd.MaxServiceCount = MaxServiceCount;
    Msg.Cmd.Flags			= Flags;
    Msg.Cmd.Padding			= 0;

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlGetServiceListeCtrl, &Msg, &CmdExt));
    retVal = Msg.Rsp.status;

    return (_i16)retVal;
}

#endif

 
 
 
 


typedef struct
{
    _u8   ServiceNameLen;
    _u8   TextLen;
    _u16  Port;
    _u32   TTL;
    _u32   Options;
}NetappMdnsSetService_t;

typedef union
{
	 NetappMdnsSetService_t         Cmd;
	_BasicResponse_t                Rsp;
}_SlNetappMdnsRegisterServiceMsg_u;


#if _SL_INCLUDE_FUNC(sl_NetAppMDNSRegisterUnregisterService)

const _SlCmdCtrl_t _SlRegisterServiceCtrl =
{
    SL_OPCODE_NETAPP_MDNSREGISTERSERVICE,
    sizeof(NetappMdnsSetService_t),
    sizeof(_BasicResponse_t)
};

 
_i16 sl_NetAppMDNSRegisterUnregisterService(	const _i8* 		pServiceName, 
											const _u8   ServiceNameLen,
											const _i8* 		pText,
											const _u8   TextLen,
											const _u16  Port,
											const _u32   TTL,
											const _u32   Options)

{
    _SlNetappMdnsRegisterServiceMsg_u			Msg;
    _SlCmdExt_t									CmdExt ;
 _i8 									ServiceNameAndTextBuffer[NETAPP_MDNS_MAX_SERVICE_NAME_AND_TEXT_LENGTH];
 _i8 									*TextPtr;

	 

	 

	Msg.Cmd.ServiceNameLen	= ServiceNameLen;
	Msg.Cmd.Options			= Options;
	Msg.Cmd.Port			= Port;
	Msg.Cmd.TextLen			= TextLen;
	Msg.Cmd.TTL				= TTL;

	 
	if(TextLen + ServiceNameLen > (NETAPP_MDNS_MAX_SERVICE_NAME_AND_TEXT_LENGTH - 1 ))  
	{
		return -1;
	}

    _SlDrvMemZero(ServiceNameAndTextBuffer, NETAPP_MDNS_MAX_SERVICE_NAME_AND_TEXT_LENGTH);

	
	 
	sl_Memcpy(ServiceNameAndTextBuffer,
		      pServiceName,   
			  ServiceNameLen);

	if(TextLen > 0 )
	{
		
		TextPtr = &ServiceNameAndTextBuffer[ServiceNameLen];
		 
		sl_Memcpy(TextPtr,
				  pText,   
				  TextLen);

  
	}

    _SlDrvResetCmdExt(&CmdExt);
    CmdExt.TxPayloadLen = (TextLen + ServiceNameLen);
    CmdExt.pTxPayload   = (_u8 *)ServiceNameAndTextBuffer;

	
	VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlRegisterServiceCtrl, &Msg, &CmdExt));

	return (_i16)Msg.Rsp.status;

	
}
#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppMDNSRegisterService)

_i16 sl_NetAppMDNSRegisterService(	const _i8* 		pServiceName, 
									const _u8   ServiceNameLen,
									const _i8* 		pText,
									const _u8   TextLen,
									const _u16  Port,
									const _u32    TTL,
									     _u32    Options)

{

	 

	 
	Options |=  NETAPP_MDNS_OPTIONS_ADD_SERVICE_BIT;

    return  sl_NetAppMDNSRegisterUnregisterService(	pServiceName, 
											        ServiceNameLen,
													pText,
													TextLen,
													Port,
													TTL,
													Options);

	
}
#endif
 



 
#if _SL_INCLUDE_FUNC(sl_NetAppMDNSUnRegisterService)

_i16 sl_NetAppMDNSUnRegisterService(	const _i8* 		pServiceName, 
									const _u8   ServiceNameLen)


{
    _u32    Options = 0;

	 

	 
	
	Options &=  (~NETAPP_MDNS_OPTIONS_ADD_SERVICE_BIT);

    return  sl_NetAppMDNSRegisterUnregisterService(	pServiceName, 
											        ServiceNameLen,
													NULL,
													0,
													0,
													0,
													Options);

	
}
#endif
 



 
 
 
 

typedef struct 
{
	 _u8   ServiceLen;
	 _u8   AddrLen;
	 _u16  Padding;
}_GetHostByServiceCommand_t;



 
typedef struct 
{
	_u16   Status;
	_u16   TextLen;
	_u32    Port;
	_u32    Address;
}_GetHostByServiceIPv4AsyncResponse_t;


typedef struct 
{
	_u16   Status;
	_u16   TextLen;
	_u32    Port;
	_u32    Address[4];
}_GetHostByServiceIPv6AsyncResponse_t;


typedef union
{
    _GetHostByServiceIPv4AsyncResponse_t IpV4;
    _GetHostByServiceIPv6AsyncResponse_t IpV6;
}_GetHostByServiceAsyncResponseAttribute_u;

 
typedef struct
{
    _i16           Status;
	_u32   *out_pAddr;
	_u32   *out_pPort;
	_u16   *inout_TextLen;  
 _i8            *out_pText;
}_GetHostByServiceAsyncResponse_t;


typedef union
{
	_GetHostByServiceCommand_t      Cmd;
	_BasicResponse_t                Rsp;
}_SlGetHostByServiceMsg_u;


#if _SL_INCLUDE_FUNC(sl_NetAppDnsGetHostByService)

const _SlCmdCtrl_t _SlGetHostByServiceCtrl =
{
    SL_OPCODE_NETAPP_MDNSGETHOSTBYSERVICE,
    sizeof(_GetHostByServiceCommand_t),
    sizeof(_BasicResponse_t)
};

 

_i32 sl_NetAppDnsGetHostByService(_i8 		*pServiceName,	 
								  const _u8  ServiceLen,
								  const _u8  Family,			 
								  _u32  pAddr[], 
								  _u32  *pPort,
								  _u16 *pTextLen,  
								  _i8          *pText
						         )
{
    _SlGetHostByServiceMsg_u         Msg;
    _SlCmdExt_t                      CmdExt ;
    _GetHostByServiceAsyncResponse_t AsyncRsp;
	_u8 ObjIdx = MAX_CONCURRENT_ACTIONS;

 
	 

	Msg.Cmd.ServiceLen = ServiceLen;
	Msg.Cmd.AddrLen    = Family;

	 

    _SlDrvResetCmdExt(&CmdExt);
	CmdExt.TxPayloadLen = ServiceLen;
    CmdExt.pTxPayload   = (_u8 *)pServiceName;

	 
	AsyncRsp.out_pText     = pText;
	AsyncRsp.inout_TextLen = (_u16* )pTextLen;
	AsyncRsp.out_pPort     = pPort;
	AsyncRsp.out_pAddr     = (_u32 *)pAddr;


    ObjIdx = _SlDrvProtectAsyncRespSetting((_u8*)&AsyncRsp, GETHOSYBYSERVICE_ID, SL_MAX_SOCKETS);

    if (MAX_CONCURRENT_ACTIONS == ObjIdx)
    {
        return SL_POOL_IS_EMPTY;
    }

    
	if (SL_AF_INET6 == Family)  
	{
		g_pCB->ObjPool[ObjIdx].AdditionalData |= SL_NETAPP_FAMILY_MASK;
	}
     
	VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlGetHostByServiceCtrl, &Msg, &CmdExt));

 
	 
     
	if(SL_RET_CODE_OK == Msg.Rsp.status)
    {        
        _SlDrvSyncObjWaitForever(&g_pCB->ObjPool[ObjIdx].SyncObj);
        
		 
		
		Msg.Rsp.status = AsyncRsp.Status;
    }

    _SlDrvReleasePoolObj(ObjIdx);
    return Msg.Rsp.status;
}
#endif

 

 
#ifndef SL_TINY_EXT
void _sl_HandleAsync_DnsGetHostByService(void *pVoidBuf)
{

	_GetHostByServiceAsyncResponse_t* Res;
	_u16 				  TextLen;
	_u16 				  UserTextLen;


	 
    
	 
	_GetHostByServiceIPv4AsyncResponse_t   *pMsgArgs   = (_GetHostByServiceIPv4AsyncResponse_t *)_SL_RESP_ARGS_START(pVoidBuf);

    VERIFY_SOCKET_CB(NULL != g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs);

	 
	if(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].AdditionalData & SL_NETAPP_FAMILY_MASK)
	{
		return;
	}
	 
	else
	{
     
	TextLen = pMsgArgs->TextLen;
	
	 
		Res = (_GetHostByServiceAsyncResponse_t*)g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs;



	 
	Res->out_pAddr[0]	= pMsgArgs->Address;
	Res->out_pPort[0]	= pMsgArgs->Port;
	Res->Status			= pMsgArgs->Status;
	
	 
	UserTextLen			= Res->inout_TextLen[0];
    
	 
	UserTextLen = (TextLen <= UserTextLen) ? TextLen : UserTextLen;
	Res->inout_TextLen[0] = UserTextLen ;

     
	

	sl_Memcpy(Res->out_pText          ,
		     (_i8 *)(& pMsgArgs[1])  ,    
			 UserTextLen              );


     

		_SlDrvSyncObjSignal(&g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].SyncObj);
		return;
	}
}

 
 
 
void _sl_HandleAsync_DnsGetHostByAddr(void *pVoidBuf)
{
    SL_TRACE0(DBG_MSG, MSG_303, "STUB: _sl_HandleAsync_DnsGetHostByAddr not implemented yet!");
    return;
}
#endif

 
 
 
typedef union
{
    _GetHostByNameIPv4AsyncResponse_t IpV4;
    _GetHostByNameIPv6AsyncResponse_t IpV6;
}_GetHostByNameAsyncResponse_u;

typedef union
{
	_GetHostByNameCommand_t         Cmd;
	_BasicResponse_t                Rsp;
}_SlGetHostByNameMsg_u;


#if _SL_INCLUDE_FUNC(sl_NetAppDnsGetHostByName)
const _SlCmdCtrl_t _SlGetHostByNameCtrl =
{
    SL_OPCODE_NETAPP_DNSGETHOSTBYNAME,
    sizeof(_GetHostByNameCommand_t),
    sizeof(_BasicResponse_t)
};

_i16 sl_NetAppDnsGetHostByName(_i8 * hostname,const  _u16 usNameLen, _u32*  out_ip_addr,const _u8 family)
{
    _SlGetHostByNameMsg_u           Msg;
    _SlCmdExt_t                     ExtCtrl;
    _GetHostByNameAsyncResponse_u   AsyncRsp;
	_u8 ObjIdx = MAX_CONCURRENT_ACTIONS;


    _SlDrvResetCmdExt(&ExtCtrl);
    ExtCtrl.TxPayloadLen = usNameLen;
    ExtCtrl.pTxPayload = (_u8 *)hostname;

    Msg.Cmd.Len = usNameLen;
    Msg.Cmd.family = family;

	 
	ObjIdx = (_u8)_SlDrvWaitForPoolObj(GETHOSYBYNAME_ID,SL_MAX_SOCKETS);
	if (MAX_CONCURRENT_ACTIONS == ObjIdx)
	{
		return SL_POOL_IS_EMPTY;
	}

    _SlDrvProtectionObjLockWaitForever();

	g_pCB->ObjPool[ObjIdx].pRespArgs =  (_u8 *)&AsyncRsp;
	 
	if (SL_AF_INET6 == family)
	{
		g_pCB->ObjPool[ObjIdx].AdditionalData |= SL_NETAPP_FAMILY_MASK;
	}
	
    _SlDrvProtectionObjUnLock();

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlGetHostByNameCtrl, &Msg, &ExtCtrl));

    if(SL_RET_CODE_OK == Msg.Rsp.status)
    {
        _SlDrvSyncObjWaitForever(&g_pCB->ObjPool[ObjIdx].SyncObj);
        
        Msg.Rsp.status = AsyncRsp.IpV4.status;

        if(SL_OS_RET_CODE_OK == (_i16)Msg.Rsp.status)
        {
            sl_Memcpy((_i8 *)out_ip_addr,
                      (_i8 *)&AsyncRsp.IpV4.ip0, 
                      (SL_AF_INET == family) ? SL_IPV4_ADDRESS_SIZE : SL_IPV6_ADDRESS_SIZE);
        }
    }
    _SlDrvReleasePoolObj(ObjIdx);
    return Msg.Rsp.status;
}
#endif


 
 
 
void _sl_HandleAsync_DnsGetHostByName(void *pVoidBuf)
{
    _GetHostByNameIPv4AsyncResponse_t     *pMsgArgs   = (_GetHostByNameIPv4AsyncResponse_t *)_SL_RESP_ARGS_START(pVoidBuf);

   _SlDrvProtectionObjLockWaitForever();

    VERIFY_SOCKET_CB(NULL != g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs);

	 
	if(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].AdditionalData & SL_NETAPP_FAMILY_MASK)
	{
		sl_Memcpy(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs, pMsgArgs, sizeof(_GetHostByNameIPv6AsyncResponse_t));
	}
	 
	else
	{
		sl_Memcpy(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs, pMsgArgs, sizeof(_GetHostByNameIPv4AsyncResponse_t));
	}
	_SlDrvSyncObjSignal(&g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].SyncObj);
    _SlDrvProtectionObjUnLock();
    return;
}


#ifndef SL_TINY_EXT
void CopyPingResultsToReport(_PingReportResponse_t *pResults,SlPingReport_t *pReport)
{
    pReport->PacketsSent     = pResults->numSendsPings;
    pReport->PacketsReceived = pResults->numSuccsessPings;
    pReport->MinRoundTime    = pResults->rttMin;
    pReport->MaxRoundTime    = pResults->rttMax;
    pReport->AvgRoundTime    = pResults->rttAvg;
    pReport->TestTime        = pResults->testTime;
}

 
 
 
void _sl_HandleAsync_PingResponse(void *pVoidBuf)
{
    _PingReportResponse_t     *pMsgArgs   = (_PingReportResponse_t *)_SL_RESP_ARGS_START(pVoidBuf);
    SlPingReport_t            pingReport;
    
    if(pPingCallBackFunc)
    {
        CopyPingResultsToReport(pMsgArgs,&pingReport);
        pPingCallBackFunc(&pingReport);
    }
    else
    {
       
        _SlDrvProtectionObjLockWaitForever();
        
        VERIFY_SOCKET_CB(NULL != g_pCB->PingCB.PingAsync.pAsyncRsp);

		if (NULL != g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs)
		{
		   sl_Memcpy(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs, pMsgArgs, sizeof(_PingReportResponse_t));
		   _SlDrvSyncObjSignal(&g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].SyncObj);
		}
       _SlDrvProtectionObjUnLock();
    }
    return;
}
#endif

 
 
 
typedef union
{
	_PingStartCommand_t   Cmd;
	_PingReportResponse_t  Rsp;
}_SlPingStartMsg_u;


typedef enum
{
  CMD_PING_TEST_RUNNING = 0,
  CMD_PING_TEST_STOPPED
}_SlPingStatus_e;


#if _SL_INCLUDE_FUNC(sl_NetAppPingStart)
_i16 sl_NetAppPingStart(const SlPingStartCommand_t* pPingParams,const _u8 family,SlPingReport_t *pReport,const P_SL_DEV_PING_CALLBACK pPingCallback)
{
    _SlCmdCtrl_t                CmdCtrl = {0, sizeof(_PingStartCommand_t), sizeof(_BasicResponse_t)};
    _SlPingStartMsg_u           Msg;
    _PingReportResponse_t       PingRsp;
    _u8 ObjIdx = MAX_CONCURRENT_ACTIONS;

    if( 0 == pPingParams->Ip ) 
    { 
       return _SlDrvBasicCmd(SL_OPCODE_NETAPP_PINGSTOP); 
    }

    if(SL_AF_INET == family)
    {
        CmdCtrl.Opcode = SL_OPCODE_NETAPP_PINGSTART;
        sl_Memcpy(&Msg.Cmd.ip0, &pPingParams->Ip, SL_IPV4_ADDRESS_SIZE);
    }
    else
    {
        CmdCtrl.Opcode = SL_OPCODE_NETAPP_PINGSTART_V6;
        sl_Memcpy(&Msg.Cmd.ip0, &pPingParams->Ip, SL_IPV6_ADDRESS_SIZE);
    }

    Msg.Cmd.pingIntervalTime        = pPingParams->PingIntervalTime;
    Msg.Cmd.PingSize                = pPingParams->PingSize;
    Msg.Cmd.pingRequestTimeout      = pPingParams->PingRequestTimeout;
    Msg.Cmd.totalNumberOfAttempts   = pPingParams->TotalNumberOfAttempts;
    Msg.Cmd.flags                   = pPingParams->Flags;

    
    if( pPingCallback )
    {	
       pPingCallBackFunc = pPingCallback;
    }
    else
    {
        
	   ObjIdx = (_u8)_SlDrvWaitForPoolObj(PING_ID,SL_MAX_SOCKETS);
	   if (MAX_CONCURRENT_ACTIONS == ObjIdx)
	   {
		  return SL_POOL_IS_EMPTY;
	   }
       OSI_RET_OK_CHECK(sl_LockObjLock(&g_pCB->ProtectionLockObj, SL_OS_WAIT_FOREVER));
         
       g_pCB->ObjPool[ObjIdx].pRespArgs = (_u8 *)&PingRsp;
       pPingCallBackFunc = NULL;
       OSI_RET_OK_CHECK(sl_LockObjUnlock(&g_pCB->ProtectionLockObj));
    }

    
    VERIFY_RET_OK(_SlDrvCmdOp(&CmdCtrl, &Msg, NULL));
	 
    if(CMD_PING_TEST_RUNNING == (_i16)Msg.Rsp.status || CMD_PING_TEST_STOPPED == (_i16)Msg.Rsp.status )
    {
         
        if( NULL == pPingCallback )
        {
            _SlDrvSyncObjWaitForever(&g_pCB->ObjPool[ObjIdx].SyncObj);

            if( SL_OS_RET_CODE_OK == (_i16)PingRsp.status )
            {
                CopyPingResultsToReport(&PingRsp,pReport);
            }
            _SlDrvReleasePoolObj(ObjIdx);
        }
    }
    else
    {    
        if( NULL == pPingCallback ) 
        {	
            _SlDrvReleasePoolObj(ObjIdx);
        }
    }

    return Msg.Rsp.status;
}
#endif

 
 
 
typedef union
{
    _NetAppSetGet_t    Cmd;
    _BasicResponse_t   Rsp;
}_SlNetAppMsgSet_u;


#if _SL_INCLUDE_FUNC(sl_NetAppSet)

const _SlCmdCtrl_t _SlNetAppSetCmdCtrl =
{
    SL_OPCODE_NETAPP_NETAPPSET,
    sizeof(_NetAppSetGet_t),
    sizeof(_BasicResponse_t)
};

_i32 sl_NetAppSet(const _u8 AppId ,const _u8 Option,const _u8 OptionLen,const  _u8 *pOptionValue)
{
    _SlNetAppMsgSet_u         Msg;
    _SlCmdExt_t               CmdExt;


    _SlDrvResetCmdExt(&CmdExt);
	CmdExt.TxPayloadLen = (OptionLen+3) & (~3);
    CmdExt.pTxPayload = (_u8 *)pOptionValue;


    Msg.Cmd.AppId    = AppId;
    Msg.Cmd.ConfigLen   = OptionLen;
	Msg.Cmd.ConfigOpt   = Option;

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlNetAppSetCmdCtrl, &Msg, &CmdExt));

    return (_i16)Msg.Rsp.status;
}
#endif

 
 
 
typedef union
{
    sl_NetAppHttpServerSendToken_t    Cmd;
    _BasicResponse_t   Rsp;
}_SlNetAppMsgSendTokenValue_u;



#if defined(sl_HttpServerCallback) || defined(EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS)
const _SlCmdCtrl_t _SlNetAppSendTokenValueCmdCtrl =
{
    SL_OPCODE_NETAPP_HTTPSENDTOKENVALUE,
    sizeof(sl_NetAppHttpServerSendToken_t),
    sizeof(_BasicResponse_t)
};

_u16 _sl_NetAppSendTokenValue(slHttpServerData_t * Token_value)
{
	_SlNetAppMsgSendTokenValue_u    Msg;
    _SlCmdExt_t						CmdExt;

	CmdExt.TxPayloadLen = (Token_value->value_len+3) & (~3);
    CmdExt.RxPayloadLen = 0;
	CmdExt.pTxPayload = (_u8 *) Token_value->token_value;
    CmdExt.pRxPayload = NULL;

	Msg.Cmd.token_value_len = Token_value->value_len;
	Msg.Cmd.token_name_len = Token_value->name_len;
	sl_Memcpy(&Msg.Cmd.token_name[0], Token_value->token_name, Token_value->name_len);
	

	VERIFY_RET_OK(_SlDrvCmdSend((_SlCmdCtrl_t *)&_SlNetAppSendTokenValueCmdCtrl, &Msg, &CmdExt));

	return Msg.Rsp.status;
}
#endif


 
 
 
typedef union
{
	_NetAppSetGet_t	    Cmd;
	_NetAppSetGet_t	    Rsp;
}_SlNetAppMsgGet_u;


#if _SL_INCLUDE_FUNC(sl_NetAppGet)
const _SlCmdCtrl_t _SlNetAppGetCmdCtrl =
{
    SL_OPCODE_NETAPP_NETAPPGET,
    sizeof(_NetAppSetGet_t),
    sizeof(_NetAppSetGet_t)
};

_i32 sl_NetAppGet(const _u8 AppId,const  _u8 Option,_u8 *pOptionLen, _u8 *pOptionValue)
{
    _SlNetAppMsgGet_u         Msg;
    _SlCmdExt_t               CmdExt;

       if (*pOptionLen == 0)
       {
              return SL_EZEROLEN;
       }

    _SlDrvResetCmdExt(&CmdExt);
    CmdExt.RxPayloadLen = *pOptionLen;
    CmdExt.pRxPayload = (_u8 *)pOptionValue;

    Msg.Cmd.AppId    = AppId;
    Msg.Cmd.ConfigOpt   = Option;
    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlNetAppGetCmdCtrl, &Msg, &CmdExt));
    

       if (CmdExt.RxPayloadLen < CmdExt.ActualRxPayloadLen) 
       {
              *pOptionLen = (_u8)CmdExt.RxPayloadLen;
              return SL_ESMALLBUF;
       }
       else
       {
              *pOptionLen = (_u8)CmdExt.ActualRxPayloadLen;
       }
  
    return (_i16)Msg.Rsp.Status;
}
#endif


 
 
 
void _SlDrvNetAppEventHandler(void* pArgs)
{
    _SlResponseHeader_t     *pHdr       = (_SlResponseHeader_t *)pArgs;
#if defined(sl_HttpServerCallback) || defined(EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS)
    SlHttpServerEvent_t		httpServerEvent;
    SlHttpServerResponse_t	httpServerResponse;
#endif
    
    switch(pHdr->GenHeader.Opcode)
    {
        case SL_OPCODE_NETAPP_DNSGETHOSTBYNAMEASYNCRESPONSE:
        case SL_OPCODE_NETAPP_DNSGETHOSTBYNAMEASYNCRESPONSE_V6:
            _sl_HandleAsync_DnsGetHostByName(pArgs);
            break;
#ifndef SL_TINY_EXT            
        case SL_OPCODE_NETAPP_MDNSGETHOSTBYSERVICEASYNCRESPONSE:
        case SL_OPCODE_NETAPP_MDNSGETHOSTBYSERVICEASYNCRESPONSE_V6:
            _sl_HandleAsync_DnsGetHostByService(pArgs);
            break;
        case SL_OPCODE_NETAPP_PINGREPORTREQUESTRESPONSE:
            _sl_HandleAsync_PingResponse(pArgs);
            break;
#endif

#if defined(sl_HttpServerCallback) || defined(EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS)
		case SL_OPCODE_NETAPP_HTTPGETTOKENVALUE:
		{              
			_u8 *pTokenName;
			slHttpServerData_t Token_value;
			sl_NetAppHttpServerGetToken_t *httpGetToken = (sl_NetAppHttpServerGetToken_t *)_SL_RESP_ARGS_START(pHdr);
                        pTokenName = (_u8 *)((sl_NetAppHttpServerGetToken_t *)httpGetToken + 1);

			httpServerResponse.Response = SL_NETAPP_HTTPSETTOKENVALUE;
			httpServerResponse.ResponseData.token_value.len = MAX_TOKEN_VALUE_LEN;

             
			httpServerResponse.ResponseData.token_value.data = (_u8 *)_SL_RESP_ARGS_START(pHdr) + MAX_TOKEN_NAME_LEN;

            httpServerEvent.Event = SL_NETAPP_HTTPGETTOKENVALUE_EVENT;
			httpServerEvent.EventData.httpTokenName.len = httpGetToken->token_name_len;
			httpServerEvent.EventData.httpTokenName.data = pTokenName;

			Token_value.token_name =  pTokenName;

            _SlDrvHandleHttpServerEvents (&httpServerEvent, &httpServerResponse);			

			Token_value.value_len = httpServerResponse.ResponseData.token_value.len;
			Token_value.name_len = httpServerEvent.EventData.httpTokenName.len;
			Token_value.token_value = httpServerResponse.ResponseData.token_value.data;
			    

			_sl_NetAppSendTokenValue(&Token_value);
		}
		break;

		case SL_OPCODE_NETAPP_HTTPPOSTTOKENVALUE:
		{
			_u8 *pPostParams;

			sl_NetAppHttpServerPostToken_t *httpPostTokenArgs = (sl_NetAppHttpServerPostToken_t *)_SL_RESP_ARGS_START(pHdr);
			pPostParams = (_u8 *)((sl_NetAppHttpServerPostToken_t *)httpPostTokenArgs + 1);

			httpServerEvent.Event = SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT;

			httpServerEvent.EventData.httpPostData.action.len = httpPostTokenArgs->post_action_len;
			httpServerEvent.EventData.httpPostData.action.data = pPostParams;
			pPostParams+=httpPostTokenArgs->post_action_len;

			httpServerEvent.EventData.httpPostData.token_name.len = httpPostTokenArgs->token_name_len;
			httpServerEvent.EventData.httpPostData.token_name.data = pPostParams;
			pPostParams+=httpPostTokenArgs->token_name_len;

			httpServerEvent.EventData.httpPostData.token_value.len = httpPostTokenArgs->token_value_len;
			httpServerEvent.EventData.httpPostData.token_value.data = pPostParams;

			httpServerResponse.Response = SL_NETAPP_RESPONSE_NONE;

            _SlDrvHandleHttpServerEvents (&httpServerEvent, &httpServerResponse);
			
		}
		break;
#endif

        
        default:
            SL_ERROR_TRACE2(MSG_305, "ASSERT: _SlDrvNetAppEventHandler : invalid opcode = 0x%x = %1", pHdr->GenHeader.Opcode, pHdr->GenHeader.Opcode);
            VERIFY_PROTOCOL(0);
    }
}

