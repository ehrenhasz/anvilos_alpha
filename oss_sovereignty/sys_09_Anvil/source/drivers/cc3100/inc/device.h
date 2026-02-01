 

 
 
 
#include "simplelink.h"

#ifndef __DEVICE_H__
#define __DEVICE_H__



#ifdef __cplusplus
extern "C" {
#endif



 

 
 
 
     
     
 
         
 
#define SL_POOL_IS_EMPTY (-2000)
         
 
#define SL_ESMALLBUF     (-2001)

 
#define SL_EZEROLEN      (-2002)

 
#define SL_INVALPARAM    (-2003)


 
#define SL_BAD_INTERFACE    (-2004)

 



 
 
 

 
typedef enum
{
    SL_ERR_SENDER_HEALTH_MON,
    SL_ERR_SENDER_CLI_UART,
    SL_ERR_SENDER_SUPPLICANT,
    SL_ERR_SENDER_NETWORK_STACK,
    SL_ERR_SENDER_WLAN_DRV_IF,
    SL_ERR_SENDER_WILINK,
    SL_ERR_SENDER_INIT_APP,
    SL_ERR_SENDER_NETX,
    SL_ERR_SENDER_HOST_APD,
    SL_ERR_SENDER_MDNS,
    SL_ERR_SENDER_HTTP_SERVER,
    SL_ERR_SENDER_DHCP_SERVER,
    SL_ERR_SENDER_DHCP_CLIENT,
    SL_ERR_DISPATCHER,
    SL_ERR_NUM_SENDER_LAST=0xFF
}SlErrorSender_e; 


 
#define SL_ERROR_STATIC_ADDR_SUBNET_ERROR                   (-60)   
#define SL_ERROR_ILLEGAL_CHANNEL                            (-61)   
#define SL_ERROR_SUPPLICANT_ERROR                           (-72)   
#define SL_ERROR_HOSTAPD_INIT_FAIL                          (-73)   
#define SL_ERROR_HOSTAPD_INIT_IF_FAIL                       (-74)   
#define SL_ERROR_WLAN_DRV_INIT_FAIL                         (-75)   
#define SL_ERROR_WLAN_DRV_START_FAIL                        (-76)   
#define SL_ERROR_FS_FILE_TABLE_LOAD_FAILED                  (-77)   
#define SL_ERROR_PREFERRED_NETWORKS_FILE_LOAD_FAILED        (-78)   
#define SL_ERROR_HOSTAPD_BSSID_VALIDATION_ERROR             (-79)   
#define SL_ERROR_HOSTAPD_FAILED_TO_SETUP_INTERFACE          (-80)   
#define SL_ERROR_MDNS_ENABLE_FAIL                           (-81)   
#define SL_ERROR_HTTP_SERVER_ENABLE_FAILED                  (-82)   
#define SL_ERROR_DHCP_SERVER_ENABLE_FAILED                  (-83)   
#define SL_ERROR_PREFERRED_NETWORK_LIST_FULL                (-93)   
#define SL_ERROR_PREFERRED_NETWORKS_FILE_WRITE_FAILED       (-94)   
#define SL_ERROR_DHCP_CLIENT_RENEW_FAILED                   (-100)  
 
#define SL_ERROR_CON_MGMT_STATUS_UNSPECIFIED                (-102)  
#define SL_ERROR_CON_MGMT_STATUS_AUTH_REJECT                (-103)  
#define SL_ERROR_CON_MGMT_STATUS_ASSOC_REJECT               (-104)  
#define SL_ERROR_CON_MGMT_STATUS_SECURITY_FAILURE           (-105)  
#define SL_ERROR_CON_MGMT_STATUS_AP_DEAUTHENTICATE          (-106)  
#define SL_ERROR_CON_MGMT_STATUS_AP_DISASSOCIATE            (-107)  
#define SL_ERROR_CON_MGMT_STATUS_ROAMING_TRIGGER            (-108)  
#define SL_ERROR_CON_MGMT_STATUS_DISCONNECT_DURING_CONNECT  (-109)  
#define SL_ERROR_CON_MGMT_STATUS_SG_RESELECT                (-110)  
#define SL_ERROR_CON_MGMT_STATUS_ROC_FAILURE                (-111)  
#define SL_ERROR_CON_MGMT_STATUS_MIC_FAILURE                (-112)  
 
#define SL_ERROR_WAKELOCK_ERROR_PREFIX                      (-115)   
#define SL_ERROR_LENGTH_ERROR_PREFIX                        (-116)   
#define SL_ERROR_MDNS_CREATE_FAIL                           (-121)   
#define SL_ERROR_GENERAL_ERROR                              (-127)



#define SL_DEVICE_GENERAL_CONFIGURATION           (1)
#define SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME (11)
#define SL_DEVICE_GENERAL_VERSION                 (12)
#define SL_DEVICE_STATUS                          (2)

 
 
#define SL_WLAN_CONNECT_EVENT                     (1)
#define SL_WLAN_DISCONNECT_EVENT                  (2)
 
#define SL_WLAN_SMART_CONFIG_COMPLETE_EVENT       (3)
#define SL_WLAN_SMART_CONFIG_STOP_EVENT           (4)
 
#define SL_WLAN_STA_CONNECTED_EVENT               (5)
#define SL_WLAN_STA_DISCONNECTED_EVENT            (6)
 
#define SL_WLAN_P2P_DEV_FOUND_EVENT               (7)
#define    SL_WLAN_P2P_NEG_REQ_RECEIVED_EVENT     (8)
#define SL_WLAN_CONNECTION_FAILED_EVENT           (9)
 
#define SL_DEVICE_FATAL_ERROR_EVENT               (1)
#define SL_DEVICE_ABORT_ERROR_EVENT               (2)

                
#define    SL_SOCKET_TX_FAILED_EVENT              (1) 
#define SL_SOCKET_ASYNC_EVENT                     (2)
    
#define    SL_NETAPP_IPV4_IPACQUIRED_EVENT        (1)
#define    SL_NETAPP_IPV6_IPACQUIRED_EVENT        (2)
#define SL_NETAPP_IP_LEASED_EVENT                 (3)
#define SL_NETAPP_IP_RELEASED_EVENT               (4)

 
#define SL_NETAPP_HTTPGETTOKENVALUE_EVENT          (1)
#define SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT         (2)


 

 
#define SL_EVENT_CLASS_GLOBAL                     (0)
#define SL_EVENT_CLASS_DEVICE                     (1)
#define SL_EVENT_CLASS_WLAN                       (2)
#define SL_EVENT_CLASS_BSD                        (3)
#define SL_EVENT_CLASS_NETAPP                     (4)
#define SL_EVENT_CLASS_NETCFG                     (5)
#define SL_EVENT_CLASS_FS                         (6)

  
 
#define EVENT_DROPPED_DEVICE_ASYNC_GENERAL_ERROR          (0x00000001L)
#define STATUS_DEVICE_SMART_CONFIG_ACTIVE                 (0x80000000L)
  
 
#define EVENT_DROPPED_WLAN_WLANASYNCONNECTEDRESPONSE      (0x00000001L)
#define EVENT_DROPPED_WLAN_WLANASYNCDISCONNECTEDRESPONSE  (0x00000002L)
#define EVENT_DROPPED_WLAN_STA_CONNECTED                  (0x00000004L)
#define EVENT_DROPPED_WLAN_STA_DISCONNECTED               (0x00000008L)
#define STATUS_WLAN_STA_CONNECTED                         (0x80000000L)
                      
 
#define EVENT_DROPPED_NETAPP_IPACQUIRED                   (0x00000001L)
#define EVENT_DROPPED_NETAPP_IPACQUIRED_V6                (0x00000002L)
#define EVENT_DROPPED_NETAPP_IP_LEASED                    (0x00000004L)
#define EVENT_DROPPED_NETAPP_IP_RELEASED                  (0x00000008L)
                      
 
#define EVENT_DROPPED_SOCKET_TXFAILEDASYNCRESPONSE        (0x00000001L)
  
 
  


 
 
 

#define ROLE_UNKNOWN_ERR                      (-1)

#ifdef SL_IF_TYPE_UART
typedef struct  
{
    _u32             BaudRate;
    _u8              FlowControlEnable;
    _u8              CommPort;
} SlUartIfParams_t;
#endif

typedef struct
{
    _u32               ChipId;
    _u32               FwVersion[4];
    _u8                PhyVersion[4];
}_SlPartialVersion;

typedef struct
{
    _SlPartialVersion ChipFwAndPhyVersion;
    _u32               NwpVersion[4];
    _u16               RomVersion;
    _u16               Padding;
}SlVersionFull;


typedef struct
{
    _u32 				AbortType;
    _u32				AbortData;
}sl_DeviceReportAbort;


typedef struct
{
    _i8                status;
    SlErrorSender_e        sender;
}sl_DeviceReport;

typedef union
{
  sl_DeviceReport           deviceEvent; 
  sl_DeviceReportAbort		deviceReport;  
} _SlDeviceEventData_u;

typedef struct
{
   _u32                 Event;
   _SlDeviceEventData_u EventData;
} SlDeviceEvent_t;

typedef struct  
{
        
    _u32                sl_tm_sec;
    _u32                sl_tm_min;
    _u32                sl_tm_hour;
        
    _u32                sl_tm_day;  
    _u32                sl_tm_mon;  
    _u32                sl_tm_year;  
    _u32                sl_tm_week_day;  
    _u32                sl_tm_year_day;   
    _u32                reserved[3];  
}SlDateTime_t;


 
 
 
typedef void (*P_INIT_CALLBACK)(_u32 Status);

 
 
 

 
#if _SL_INCLUDE_FUNC(sl_Start)
_i16 sl_Start(const void* pIfHdl, _i8*  pDevName, const P_INIT_CALLBACK pInitCallBack);
#endif

 
#if _SL_INCLUDE_FUNC(sl_Stop)
_i16 sl_Stop(const _u16 timeout);
#endif


 
#if _SL_INCLUDE_FUNC(sl_DevSet)
_i32 sl_DevSet(const _u8 DeviceSetId ,const _u8 Option,const _u8 ConfigLen,const _u8 *pValues);
#endif

 
#if _SL_INCLUDE_FUNC(sl_DevGet)
_i32 sl_DevGet(const _u8 DeviceGetId,_u8 *pOption,_u8 *pConfigLen, _u8 *pValues);
#endif


 
#if _SL_INCLUDE_FUNC(sl_EventMaskSet)
_i16 sl_EventMaskSet(const _u8 EventClass ,const _u32 Mask);
#endif

 
#if _SL_INCLUDE_FUNC(sl_EventMaskGet)
_i16 sl_EventMaskGet(const _u8 EventClass,_u32 *pMask);
#endif


 
#if _SL_INCLUDE_FUNC(sl_Task)
void sl_Task(void);
#endif


 
#ifdef SL_IF_TYPE_UART
#if _SL_INCLUDE_FUNC(sl_UartSetMode)
_i16 sl_UartSetMode(const SlUartIfParams_t* pUartParams);
#endif
#endif

 


#ifdef  __cplusplus
}
#endif  

#endif   


