 

 
 
 

#include "simplelink.h"

#ifndef __NETAPP_H__
#define    __NETAPP_H__




#ifdef    __cplusplus
extern "C" {
#endif

 

 
 
 

 
#define SL_ERROR_NETAPP_RX_BUFFER_LENGTH_ERROR (-230)

 
#define MAX_INPUT_STRING                              (64)  

#define MAX_AUTH_NAME_LEN                             (20)
#define MAX_AUTH_PASSWORD_LEN                         (20)
#define MAX_AUTH_REALM_LEN                            (20)

#define MAX_DEVICE_URN_LEN (15+1)
#define MAX_DOMAIN_NAME_LEN    (24+1)

#define MAX_ACTION_LEN                                (30)
 
#define MAX_TOKEN_NAME_LEN                            (20)  
#define MAX_TOKEN_VALUE_LEN        MAX_INPUT_STRING

#define NETAPP_MAX_SERVICE_TEXT_SIZE                  (256)
#define NETAPP_MAX_SERVICE_NAME_SIZE                  (60)
#define NETAPP_MAX_SERVICE_HOST_NAME_SIZE             (64)


 
#define SL_NETAPP_RESPONSE_NONE                       (0)
#define SL_NETAPP_HTTPSETTOKENVALUE                   (1)

#define SL_NETAPP_FAMILY_MASK                         (0x80)

 
#define SL_NET_APP_MASK_IPP_TYPE_OF_SERVICE           (0x00000001)
#define SL_NET_APP_MASK_DEVICE_INFO_TYPE_OF_SERVICE   (0x00000002)
#define SL_NET_APP_MASK_HTTP_TYPE_OF_SERVICE          (0x00000004)
#define SL_NET_APP_MASK_HTTPS_TYPE_OF_SERVICE         (0x00000008)
#define SL_NET_APP_MASK_WORKSATION_TYPE_OF_SERVICE    (0x00000010)
#define SL_NET_APP_MASK_GUID_TYPE_OF_SERVICE          (0x00000020)
#define SL_NET_APP_MASK_H323_TYPE_OF_SERVICE          (0x00000040)
#define SL_NET_APP_MASK_NTP_TYPE_OF_SERVICE           (0x00000080)
#define SL_NET_APP_MASK_OBJECITVE_TYPE_OF_SERVICE     (0x00000100)
#define SL_NET_APP_MASK_RDP_TYPE_OF_SERVICE           (0x00000200)
#define SL_NET_APP_MASK_REMOTE_TYPE_OF_SERVICE        (0x00000400)
#define SL_NET_APP_MASK_RTSP_TYPE_OF_SERVICE          (0x00000800)
#define SL_NET_APP_MASK_SIP_TYPE_OF_SERVICE           (0x00001000)
#define SL_NET_APP_MASK_SMB_TYPE_OF_SERVICE           (0x00002000)
#define SL_NET_APP_MASK_SOAP_TYPE_OF_SERVICE          (0x00004000)
#define SL_NET_APP_MASK_SSH_TYPE_OF_SERVICE           (0x00008000)
#define SL_NET_APP_MASK_TELNET_TYPE_OF_SERVICE        (0x00010000)
#define SL_NET_APP_MASK_TFTP_TYPE_OF_SERVICE          (0x00020000)
#define SL_NET_APP_MASK_XMPP_CLIENT_TYPE_OF_SERVICE   (0x00040000)
#define SL_NET_APP_MASK_RAOP_TYPE_OF_SERVICE          (0x00080000)
#define SL_NET_APP_MASK_ALL_TYPE_OF_SERVICE           (0xFFFFFFFF)

 
 

#define SL_NET_APP_DNS_QUERY_NO_RESPONSE              (-159)    
#define SL_NET_APP_DNS_NO_SERVER                      (-161)    
#define SL_NET_APP_DNS_PARAM_ERROR                    (-162)   
#define SL_NET_APP_DNS_QUERY_FAILED                   (-163)    
#define SL_NET_APP_DNS_INTERNAL_1                     (-164)
#define SL_NET_APP_DNS_INTERNAL_2                     (-165)
#define SL_NET_APP_DNS_MALFORMED_PACKET               (-166)    
#define SL_NET_APP_DNS_INTERNAL_3                     (-167)
#define SL_NET_APP_DNS_INTERNAL_4                     (-168)
#define SL_NET_APP_DNS_INTERNAL_5                     (-169)
#define SL_NET_APP_DNS_INTERNAL_6                     (-170)
#define SL_NET_APP_DNS_INTERNAL_7                     (-171)
#define SL_NET_APP_DNS_INTERNAL_8                     (-172)
#define SL_NET_APP_DNS_INTERNAL_9                     (-173)
#define SL_NET_APP_DNS_MISMATCHED_RESPONSE            (-174)   
#define SL_NET_APP_DNS_INTERNAL_10                    (-175)
#define SL_NET_APP_DNS_INTERNAL_11                    (-176)
#define SL_NET_APP_DNS_NO_ANSWER                      (-177)   
#define SL_NET_APP_DNS_NO_KNOWN_ANSWER                (-178)   
#define SL_NET_APP_DNS_NAME_MISMATCH                  (-179)   
#define SL_NET_APP_DNS_NOT_STARTED                    (-180)   
#define SL_NET_APP_DNS_HOST_NAME_ERROR                (-181)   
#define SL_NET_APP_DNS_NO_MORE_ENTRIES                (-182)   
                                                      
#define SL_NET_APP_DNS_MAX_SERVICES_ERROR             (-200)   
#define SL_NET_APP_DNS_IDENTICAL_SERVICES_ERROR       (-201)   
#define SL_NET_APP_DNS_NOT_EXISTED_SERVICE_ERROR      (-203)   
#define SL_NET_APP_DNS_ERROR_SERVICE_NAME_ERROR       (-204)   
#define SL_NET_APP_DNS_RX_PACKET_ALLOCATION_ERROR     (-205)   
#define SL_NET_APP_DNS_BUFFER_SIZE_ERROR              (-206)   
#define SL_NET_APP_DNS_NET_APP_SET_ERROR              (-207)   
#define SL_NET_APP_DNS_GET_SERVICE_LIST_FLAG_ERROR    (-208)
#define SL_NET_APP_DNS_NO_CONFIGURATION_ERROR         (-209)

 
#define SL_ERROR_DEVICE_NAME_LEN_ERR                   (-117) 
#define SL_ERROR_DEVICE_NAME_INVALID                   (-118)
 
#define SL_ERROR_DOMAIN_NAME_LEN_ERR                   (-119)
#define SL_ERROR_DOMAIN_NAME_INVALID                   (-120)

 

 
#define SL_NET_APP_HTTP_SERVER_ID                     (1)
#define SL_NET_APP_DHCP_SERVER_ID                     (2)
#define SL_NET_APP_MDNS_ID                            (4)
#define SL_NET_APP_DNS_SERVER_ID                      (8)
#define SL_NET_APP_DEVICE_CONFIG_ID                   (16)
              
#define NETAPP_SET_DHCP_SRV_BASIC_OPT                 (0)             
                     
#define NETAPP_SET_GET_HTTP_OPT_PORT_NUMBER           (0)
#define NETAPP_SET_GET_HTTP_OPT_AUTH_CHECK            (1)
#define NETAPP_SET_GET_HTTP_OPT_AUTH_NAME             (2)
#define NETAPP_SET_GET_HTTP_OPT_AUTH_PASSWORD         (3)
#define NETAPP_SET_GET_HTTP_OPT_AUTH_REALM            (4)
#define NETAPP_SET_GET_HTTP_OPT_ROM_PAGES_ACCESS      (5)
                                                     
#define NETAPP_SET_GET_MDNS_CONT_QUERY_OPT            (1)
#define NETAPP_SET_GET_MDNS_QEVETN_MASK_OPT           (2)
#define NETAPP_SET_GET_MDNS_TIMING_PARAMS_OPT         (3)

 
#define NETAPP_SET_GET_DNS_OPT_DOMAIN_NAME            (0)

 
#define NETAPP_SET_GET_DEV_CONF_OPT_DEVICE_URN        (0)
#define NETAPP_SET_GET_DEV_CONF_OPT_DOMAIN_NAME       (1)


 
 
 

typedef struct
{
    _u32    PacketsSent;
    _u32    PacketsReceived;
    _u16    MinRoundTime;
    _u16    MaxRoundTime;
    _u16    AvgRoundTime;
    _u32    TestTime;
}SlPingReport_t;

typedef struct
{
    _u32    PingIntervalTime;        
    _u16    PingSize;                
    _u16    PingRequestTimeout;      
    _u32    TotalNumberOfAttempts;   
    _u32    Flags;                   
    _u32    Ip;                      
    _u32    Ip1OrPaadding;
    _u32    Ip2OrPaadding;
    _u32    Ip3OrPaadding;
}SlPingStartCommand_t;

typedef struct _slHttpServerString_t
{
    _u8     len;
    _u8     *data;
} slHttpServerString_t;

typedef struct _slHttpServerData_t
{
    _u8     value_len;
    _u8     name_len;
    _u8     *token_value;
    _u8     *token_name;
} slHttpServerData_t;

typedef struct _slHttpServerPostData_t
{
    slHttpServerString_t action;
    slHttpServerString_t  token_name;
    slHttpServerString_t token_value;
}slHttpServerPostData_t;

typedef union
{
  slHttpServerString_t  httpTokenName;  
  slHttpServerPostData_t   httpPostData;   
} SlHttpServerEventData_u;

typedef union
{
  slHttpServerString_t token_value;
} SlHttpServerResponsedata_u;

typedef struct
{
   _u32                    Event;
   SlHttpServerEventData_u EventData;
}SlHttpServerEvent_t;

typedef struct
{
   _u32                       Response;
   SlHttpServerResponsedata_u ResponseData;
}SlHttpServerResponse_t;


typedef struct
{
    _u32   lease_time;
    _u32   ipv4_addr_start;
    _u32   ipv4_addr_last;
}SlNetAppDhcpServerBasicOpt_t; 

 
typedef enum
{
    SL_NET_APP_FULL_SERVICE_WITH_TEXT_IPV4_TYPE = 1,
    SL_NET_APP_FULL_SERVICE_IPV4_TYPE,
    SL_NET_APP_SHORT_SERVICE_IPV4_TYPE
 
} SlNetAppGetServiceListType_e;

typedef struct
{
    _u32   service_ipv4;
    _u16   service_port;
    _u16   Reserved;
}SlNetAppGetShortServiceIpv4List_t;

typedef struct
{
    _u32   service_ipv4;
    _u16   service_port;
    _u16   Reserved;
    _u8    service_name[NETAPP_MAX_SERVICE_NAME_SIZE];
    _u8    service_host[NETAPP_MAX_SERVICE_HOST_NAME_SIZE];
}SlNetAppGetFullServiceIpv4List_t;

typedef struct
{
    _u32    service_ipv4;
    _u16    service_port;
    _u16    Reserved;
    _u8     service_name[NETAPP_MAX_SERVICE_NAME_SIZE];
    _u8     service_host[NETAPP_MAX_SERVICE_HOST_NAME_SIZE];
    _u8     service_text[NETAPP_MAX_SERVICE_TEXT_SIZE];
}SlNetAppGetFullServiceWithTextIpv4List_t;

typedef struct
{
     
    _u32    t;               
    _u32    p;               
    _u32    k;               
    _u32    RetransInterval; 
    _u32    Maxinterval;      
    _u32    max_time;        
}SlNetAppServiceAdvertiseTimingParameters_t;

 
 
 
typedef void (*P_SL_DEV_PING_CALLBACK)(SlPingReport_t*);

 
 
 


 
#if _SL_INCLUDE_FUNC(sl_NetAppStart)
_i16 sl_NetAppStart(const _u32 AppBitMap);
#endif
 
#if _SL_INCLUDE_FUNC(sl_NetAppStop)
_i16 sl_NetAppStop(const _u32 AppBitMap);
#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppDnsGetHostByName)
_i16 sl_NetAppDnsGetHostByName(_i8 * hostname,const  _u16 usNameLen, _u32*  out_ip_addr,const _u8 family );
#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppDnsGetHostByService)
_i32 sl_NetAppDnsGetHostByService(_i8  *pServiceName,  
                                  const _u8  ServiceLen,
                                  const _u8  Family,         
                                  _u32 pAddr[], 
                                  _u32 *pPort,
                                  _u16 *pTextLen,      
                                  _i8  *pText
                                 );

#endif

 

#if _SL_INCLUDE_FUNC(sl_NetAppGetServiceList)
_i16 sl_NetAppGetServiceList(const _u8   IndexOffest,
                             const _u8   MaxServiceCount,
                             const  _u8   Flags,
                                   _i8   *pBuffer,
                             const _u32  RxBufferLength
                            );

#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppMDNSUnRegisterService)
_i16 sl_NetAppMDNSUnRegisterService(const _i8 *pServiceName,const _u8 ServiceNameLen);
#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppMDNSRegisterService)
_i16 sl_NetAppMDNSRegisterService( const _i8*  pServiceName, 
                                   const _u8   ServiceNameLen,
                                   const _i8*  pText,
                                   const _u8   TextLen,
                                   const _u16  Port,
                                   const _u32  TTL,
                                         _u32  Options);
#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppPingStart)
_i16 sl_NetAppPingStart(const SlPingStartCommand_t* pPingParams,const _u8 family,SlPingReport_t *pReport,const P_SL_DEV_PING_CALLBACK pPingCallback);
#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppSet)
_i32 sl_NetAppSet(const _u8 AppId ,const _u8 Option,const _u8 OptionLen,const _u8 *pOptionValue);
#endif

 
#if _SL_INCLUDE_FUNC(sl_NetAppGet)
_i32 sl_NetAppGet(const _u8 AppId,const  _u8 Option,_u8 *pOptionLen, _u8 *pOptionValue);
#endif



 


#ifdef  __cplusplus
}
#endif  

#endif     

