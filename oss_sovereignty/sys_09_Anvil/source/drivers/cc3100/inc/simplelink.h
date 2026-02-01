 


 
    
    

#ifndef __SIMPLELINK_H__
#define    __SIMPLELINK_H__

#include "user.h"

#ifdef    __cplusplus
extern "C"
{
#endif


 

 


 
 
 
#define SL_DRIVER_VERSION   "1.0.0.10"
#define SL_MAJOR_VERSION_NUM    1L
#define SL_MINOR_VERSION_NUM    0L
#define SL_VERSION_NUM          0L
#define SL_SUB_VERSION_NUM      10L


 
 
 

#ifdef SL_TINY
#undef SL_INC_ARG_CHECK
#undef SL_INC_EXT_API
#undef SL_INC_SOCK_SERVER_SIDE_API
#undef SL_INC_WLAN_PKG
#undef SL_INC_NET_CFG_PKG
#undef SL_INC_FS_PKG
#undef SL_INC_SET_UART_MODE
#undef SL_INC_STD_BSD_API_NAMING
#undef SL_INC_SOCK_CLIENT_SIDE_API
#undef SL_INC_NET_APP_PKG
#undef SL_INC_SOCK_RECV_API
#undef SL_INC_SOCK_SEND_API
#undef SL_INC_SOCKET_PKG
#endif

#ifdef SL_SMALL
#undef SL_INC_EXT_API
#undef SL_INC_NET_APP_PKG
#undef SL_INC_NET_CFG_PKG
#undef SL_INC_FS_PKG
#define SL_INC_ARG_CHECK
#define SL_INC_WLAN_PKG
#define SL_INC_SOCKET_PKG
#define SL_INC_SOCK_CLIENT_SIDE_API
#define SL_INC_SOCK_SERVER_SIDE_API
#define SL_INC_SOCK_RECV_API
#define SL_INC_SOCK_SEND_API
#define SL_INC_SET_UART_MODE
#endif

#ifdef SL_FULL
#define SL_INC_EXT_API
#define SL_INC_NET_APP_PKG
#define SL_INC_NET_CFG_PKG
#define SL_INC_FS_PKG
#define SL_INC_ARG_CHECK
#define SL_INC_WLAN_PKG
#define SL_INC_SOCKET_PKG
#define SL_INC_SOCK_CLIENT_SIDE_API
#define SL_INC_SOCK_SERVER_SIDE_API
#define SL_INC_SOCK_RECV_API
#define SL_INC_SOCK_SEND_API
#define SL_INC_SET_UART_MODE
#endif

#define SL_RET_CODE_OK                          (0)
#define SL_RET_CODE_INVALID_INPUT               (-2)
#define SL_RET_CODE_SELF_ERROR                  (-3)
#define SL_RET_CODE_NWP_IF_ERROR                (-4)
#define SL_RET_CODE_MALLOC_ERROR                (-5)

#define sl_Memcpy       memcpy
#define sl_Memset       memset

#define sl_SyncObjClear(pObj)     sl_SyncObjWait(pObj,SL_OS_NO_WAIT)

#ifndef SL_TINY_EXT
#define SL_MAX_SOCKETS      (8)
#else
#define SL_MAX_SOCKETS      (2)
#endif


 
 
 
typedef void (*_SlSpawnEntryFunc_t)(void* pValue);

#ifndef NULL
#define NULL        (0)
#endif

#ifndef FALSE
#define FALSE       (0)
#endif

#ifndef TRUE
#define TRUE        (!FALSE)
#endif

#ifndef OK
#define OK          (0)
#endif

#ifndef _SL_USER_TYPES
      typedef unsigned char _u8;
      typedef signed char   _i8;
 
      typedef unsigned short _u16;
      typedef signed short   _i16;
 
      typedef unsigned long  _u32;
      typedef signed long    _i32;
      #define _volatile volatile
      #define _const    const
#endif

typedef _u16  _SlOpcode_t;
typedef _u8   _SlArgSize_t;
typedef _i16   _SlDataSize_t;
typedef _i16   _SlReturnVal_t;

#ifdef    __cplusplus
}
#endif  



 

 typedef enum {
 	EVENT_PROPAGATION_BLOCK = 0,
 	EVENT_PROPAGATION_CONTINUE

 } _SlEventPropogationStatus_e;






 
 
 

#ifdef SL_PLATFORM_MULTI_THREADED
    #include "spawn.h"
#else
    #include "nonos.h"
#endif


 
#include "objInclusion.h"
#include "trace.h"
#include "fs.h"
#include "socket.h"
#include "netapp.h"
#include "wlan.h"
#include "device.h"
#include "netcfg.h"
#include "wlan_rx_filters.h"


  
#ifdef sl_GeneralEvtHdlr
#define _SlDrvHandleGeneralEvents sl_GeneralEvtHdlr
#endif

  
#ifdef sl_WlanEvtHdlr
#define _SlDrvHandleWlanEvents sl_WlanEvtHdlr
#endif

  
#ifdef sl_NetAppEvtHdlr
#define _SlDrvHandleNetAppEvents sl_NetAppEvtHdlr
#endif

  
#ifdef sl_HttpServerCallback
#define _SlDrvHandleHttpServerEvents sl_HttpServerCallback
#endif

  
#ifdef sl_SockEvtHdlr
#define _SlDrvHandleSockEvents sl_SockEvtHdlr
#endif


#ifndef __CONCAT
#define __CONCAT(x,y)	x ## y
#endif
#define __CONCAT2(x,y)	__CONCAT(x,y)


 
#ifdef SL_EXT_LIB_1

     
	#if __CONCAT2(SL_EXT_LIB_1, _NOTIFY_GENERAL_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_1, _GeneralEventHdl) (SlDeviceEvent_t *);
	#define SlExtLib1GeneralEventHandler   __CONCAT2(SL_EXT_LIB_1, _GeneralEventHdl)

	#undef EXT_LIB_REGISTERED_GENERAL_EVENTS
    #define EXT_LIB_REGISTERED_GENERAL_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_1, _NOTIFY_WLAN_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_1, _WlanEventHdl) (SlWlanEvent_t *);
	#define SlExtLib1WlanEventHandler   __CONCAT2(SL_EXT_LIB_1, _WlanEventHdl)

	#undef EXT_LIB_REGISTERED_WLAN_EVENTS
    #define EXT_LIB_REGISTERED_WLAN_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_1, _NOTIFY_NETAPP_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_1, _NetAppEventHdl) (SlNetAppEvent_t *);
	#define SlExtLib1NetAppEventHandler __CONCAT2(SL_EXT_LIB_1, _NetAppEventHdl)

	#undef EXT_LIB_REGISTERED_NETAPP_EVENTS
    #define EXT_LIB_REGISTERED_NETAPP_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_1, _NOTIFY_HTTP_SERVER_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_1, _HttpServerEventHdl) (SlHttpServerEvent_t* , SlHttpServerResponse_t*);
	#define SlExtLib1HttpServerEventHandler __CONCAT2(SL_EXT_LIB_1, _HttpServerEventHdl)

	#undef EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
    #define EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_1, _NOTIFY_SOCK_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_1, _SockEventHdl) (SlSockEvent_t *);
	#define SlExtLib1SockEventHandler __CONCAT2(SL_EXT_LIB_1, _SockEventHdl)

	#undef EXT_LIB_REGISTERED_SOCK_EVENTS
    #define EXT_LIB_REGISTERED_SOCK_EVENTS
	#endif

#endif


#ifdef SL_EXT_LIB_2

     
	#if __CONCAT2(SL_EXT_LIB_2, _NOTIFY_GENERAL_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_2, _GeneralEventHdl) (SlDeviceEvent_t *);
	#define SlExtLib2GeneralEventHandler   __CONCAT2(SL_EXT_LIB_2, _GeneralEventHdl)

	#undef EXT_LIB_REGISTERED_GENERAL_EVENTS
    #define EXT_LIB_REGISTERED_GENERAL_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_2, _NOTIFY_WLAN_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_2, _WlanEventHdl) (SlWlanEvent_t *);
	#define SlExtLib2WlanEventHandler   __CONCAT2(SL_EXT_LIB_2, _WlanEventHdl)

	#undef EXT_LIB_REGISTERED_WLAN_EVENTS
    #define EXT_LIB_REGISTERED_WLAN_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_2, _NOTIFY_NETAPP_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_2, _NetAppEventHdl) (SlNetAppEvent_t *);
	#define SlExtLib2NetAppEventHandler __CONCAT2(SL_EXT_LIB_2, _NetAppEventHdl)

	#undef EXT_LIB_REGISTERED_NETAPP_EVENTS
    #define EXT_LIB_REGISTERED_NETAPP_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_2, _NOTIFY_HTTP_SERVER_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_2, _HttpServerEventHdl) (SlHttpServerEvent_t* , SlHttpServerResponse_t*);
	#define SlExtLib2HttpServerEventHandler __CONCAT2(SL_EXT_LIB_2, _HttpServerEventHdl)

	#undef EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
    #define EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_2, _NOTIFY_SOCK_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_2, _SockEventHdl) (SlSockEvent_t *);
	#define SlExtLib2SockEventHandler __CONCAT2(SL_EXT_LIB_2, _SockEventHdl)

	#undef EXT_LIB_REGISTERED_SOCK_EVENTS
    #define EXT_LIB_REGISTERED_SOCK_EVENTS
	#endif

#endif


#ifdef SL_EXT_LIB_3

     
	#if __CONCAT2(SL_EXT_LIB_3, _NOTIFY_GENERAL_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_3, _GeneralEventHdl) (SlDeviceEvent_t *);
	#define SlExtLib3GeneralEventHandler   __CONCAT2(SL_EXT_LIB_3, _GeneralEventHdl)

	#undef EXT_LIB_REGISTERED_GENERAL_EVENTS
    #define EXT_LIB_REGISTERED_GENERAL_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_3, _NOTIFY_WLAN_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_3, _WlanEventHdl) (SlWlanEvent_t *);
	#define SlExtLib3WlanEventHandler   __CONCAT2(SL_EXT_LIB_3, _WlanEventHdl)

	#undef EXT_LIB_REGISTERED_WLAN_EVENTS
    #define EXT_LIB_REGISTERED_WLAN_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_3, _NOTIFY_NETAPP_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_3, _NetAppEventHdl) (SlNetAppEvent_t *);
	#define SlExtLib3NetAppEventHandler __CONCAT2(SL_EXT_LIB_3, _NetAppEventHdl)

	#undef EXT_LIB_REGISTERED_NETAPP_EVENTS
    #define EXT_LIB_REGISTERED_NETAPP_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_3, _NOTIFY_HTTP_SERVER_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_3, _HttpServerEventHdl) (SlHttpServerEvent_t* , SlHttpServerResponse_t*);
	#define SlExtLib3HttpServerEventHandler __CONCAT2(SL_EXT_LIB_3, _HttpServerEventHdl)

	#undef EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
    #define EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_3, _NOTIFY_SOCK_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_3, _SockEventHdl) (SlSockEvent_t *);
	#define SlExtLib3SockEventHandler __CONCAT2(SL_EXT_LIB_3, _SockEventHdl)

	#undef EXT_LIB_REGISTERED_SOCK_EVENTS
    #define EXT_LIB_REGISTERED_SOCK_EVENTS
	#endif

#endif


#ifdef SL_EXT_LIB_4

     
	#if __CONCAT2(SL_EXT_LIB_4, _NOTIFY_GENERAL_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_4, _GeneralEventHdl) (SlDeviceEvent_t *);
	#define SlExtLib4GeneralEventHandler   __CONCAT2(SL_EXT_LIB_4, _GeneralEventHdl)

	#undef EXT_LIB_REGISTERED_GENERAL_EVENTS
    #define EXT_LIB_REGISTERED_GENERAL_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_4, _NOTIFY_WLAN_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_4, _WlanEventHdl) (SlWlanEvent_t *);
	#define SlExtLib4WlanEventHandler   __CONCAT2(SL_EXT_LIB_4, _WlanEventHdl)

	#undef EXT_LIB_REGISTERED_WLAN_EVENTS
    #define EXT_LIB_REGISTERED_WLAN_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_4, _NOTIFY_NETAPP_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_4, _NetAppEventHdl) (SlNetAppEvent_t *);
	#define SlExtLib4NetAppEventHandler __CONCAT2(SL_EXT_LIB_4, _NetAppEventHdl)

	#undef EXT_LIB_REGISTERED_NETAPP_EVENTS
    #define EXT_LIB_REGISTERED_NETAPP_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_4, _NOTIFY_HTTP_SERVER_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_4, _HttpServerEventHdl) (SlHttpServerEvent_t* , SlHttpServerResponse_t*);
	#define SlExtLib4HttpServerEventHandler __CONCAT2(SL_EXT_LIB_4, _HttpServerEventHdl)

	#undef EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
    #define EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_4, _NOTIFY_SOCK_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_4, _SockEventHdl) (SlSockEvent_t *);
	#define SlExtLib4SockEventHandler __CONCAT2(SL_EXT_LIB_4, _SockEventHdl)

	#undef EXT_LIB_REGISTERED_SOCK_EVENTS
    #define EXT_LIB_REGISTERED_SOCK_EVENTS
	#endif

#endif


#ifdef SL_EXT_LIB_5

     
	#if __CONCAT2(SL_EXT_LIB_5, _NOTIFY_GENERAL_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_5, _GeneralEventHdl) (SlDeviceEvent_t *);
	#define SlExtLib5GeneralEventHandler   __CONCAT2(SL_EXT_LIB_5, _GeneralEventHdl)

	#undef EXT_LIB_REGISTERED_GENERAL_EVENTS
    #define EXT_LIB_REGISTERED_GENERAL_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_5, _NOTIFY_WLAN_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_5, _WlanEventHdl) (SlWlanEvent_t *);
	#define SlExtLib5WlanEventHandler   __CONCAT2(SL_EXT_LIB_5, _WlanEventHdl)

	#undef EXT_LIB_REGISTERED_WLAN_EVENTS
    #define EXT_LIB_REGISTERED_WLAN_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_5, _NOTIFY_NETAPP_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_5, _NetAppEventHdl) (SlNetAppEvent_t *);
	#define SlExtLib5NetAppEventHandler __CONCAT2(SL_EXT_LIB_5, _NetAppEventHdl)

	#undef EXT_LIB_REGISTERED_NETAPP_EVENTS
    #define EXT_LIB_REGISTERED_NETAPP_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_5, _NOTIFY_HTTP_SERVER_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_5, _HttpServerEventHdl) (SlHttpServerEvent_t* , SlHttpServerResponse_t*);
	#define SlExtLib5HttpServerEventHandler __CONCAT2(SL_EXT_LIB_5, _HttpServerEventHdl)

	#undef EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
    #define EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS
	#endif

	 
	#if __CONCAT2(SL_EXT_LIB_5, _NOTIFY_SOCK_EVENT)
	extern _SlEventPropogationStatus_e __CONCAT2(SL_EXT_LIB_5, _SockEventHdl) (SlSockEvent_t *);
	#define SlExtLib5SockEventHandler __CONCAT2(SL_EXT_LIB_5, _SockEventHdl)

	#undef EXT_LIB_REGISTERED_SOCK_EVENTS
    #define EXT_LIB_REGISTERED_SOCK_EVENTS
	#endif

#endif



#if defined(EXT_LIB_REGISTERED_GENERAL_EVENTS)
extern void _SlDrvHandleGeneralEvents(SlDeviceEvent_t *slGeneralEvent);
#endif

#if defined(EXT_LIB_REGISTERED_WLAN_EVENTS)
extern void _SlDrvHandleWlanEvents(SlWlanEvent_t *slWlanEvent);
#endif

#if defined (EXT_LIB_REGISTERED_NETAPP_EVENTS)
extern void _SlDrvHandleNetAppEvents(SlNetAppEvent_t *slNetAppEvent);
#endif

#if defined(EXT_LIB_REGISTERED_HTTP_SERVER_EVENTS)
extern void _SlDrvHandleHttpServerEvents(SlHttpServerEvent_t *slHttpServerEvent, SlHttpServerResponse_t *slHttpServerResponse);
#endif


#if defined(EXT_LIB_REGISTERED_SOCK_EVENTS)
extern void _SlDrvHandleSockEvents(SlSockEvent_t *slSockEvent);
#endif


typedef void (*_SlSpawnEntryFunc_t)(void* pValue);


 

 
#if (defined(sl_GeneralEvtHdlr))
extern void sl_GeneralEvtHdlr(SlDeviceEvent_t *pSlDeviceEvent);
#endif


 
#if (defined(sl_WlanEvtHdlr))
extern void sl_WlanEvtHdlr(SlWlanEvent_t* pSlWlanEvent);
#endif


 
#if (defined(sl_NetAppEvtHdlr))
extern void sl_NetAppEvtHdlr(SlNetAppEvent_t* pSlNetApp);
#endif

 
#if (defined(sl_SockEvtHdlr))
extern void sl_SockEvtHdlr(SlSockEvent_t* pSlSockEvent);
#endif

 
#if (defined(sl_HttpServerCallback))
extern void sl_HttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent, SlHttpServerResponse_t *pSlHttpServerResponse);
#endif
 
 
#ifdef  __cplusplus
}
#endif  

#endif     

