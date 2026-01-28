




#include "simplelink.h"
    
#ifndef __WLAN_H__
#define    __WLAN_H__




#ifdef    __cplusplus
extern "C" {
#endif








#define SL_BSSID_LENGTH                                                                                    (6)
#define MAXIMAL_SSID_LENGTH                                                                                (32)

#define NUM_OF_RATE_INDEXES                                                                             (20)
#define SIZE_OF_RSSI_HISTOGRAM                                                                          (6)
 

#define  SL_DISCONNECT_RESERVED_0                                                                       (0)
#define  SL_DISCONNECT_UNSPECIFIED_REASON                                                               (1)
#define  SL_PREVIOUS_AUTHENTICATION_NO_LONGER_VALID                                                     (2)
#define  SL_DEAUTHENTICATED_BECAUSE_SENDING_STATION_IS_LEAVING                                          (3)
#define  SL_DISASSOCIATED_DUE_TO_INACTIVITY                                                             (4)
#define  SL_DISASSOCIATED_BECAUSE_AP_IS_UNABLE_TO_HANDLE_ALL_CURRENTLY_ASSOCIATED_STATIONS              (5)
#define  SL_CLASS_2_FRAME_RECEIVED_FROM_NONAUTHENTICATED_STATION                                        (6)
#define  SL_CLASS_3_FRAME_RECEIVED_FROM_NONASSOCIATED_STATION                                           (7)
#define  SL_DISASSOCIATED_BECAUSE_SENDING_STATION_IS_LEAVING_BSS                                        (8)
#define  SL_STATION_REQUESTING_ASSOCIATION_IS_NOT_AUTHENTICATED_WITH_RESPONDING_STATION                 (9)
#define  SL_DISASSOCIATED_BECAUSE_THE_INFORMATION_IN_THE_POWER_CAPABILITY_ELEMENT_IS_UNACCEPTABLE       (10)
#define  SL_DISASSOCIATED_BECAUSE_THE_INFORMATION_IN_THE_SUPPORTED_CHANNELS_ELEMENT_IS_UNACCEPTABLE     (11)
#define  SL_DISCONNECT_RESERVED_1                                                                       (12)
#define  SL_INVALID_INFORMATION_ELEMENT                                                                 (13)
#define  SL_MESSAGE_INTEGRITY_CODE_MIC_FAILURE                                                          (14)
#define  SL_FOUR_WAY_HANDSHAKE_TIMEOUT                                                                  (15)
#define  SL_GROUP_KEY_HANDSHAKE_TIMEOUT                                                                 (16)
#define  SL_RE_ASSOCIATION_REQUEST_PROBE_RESPONSE_BEACON_FRAME                                          (17)
#define  SL_INVALID_GROUP_CIPHER                                                                        (18)
#define  SL_INVALID_PAIRWISE_CIPHER                                                                     (19)
#define  SL_INVALID_AKMP                                                                                (20)
#define  SL_UNSUPPORTED_RSN_INFORMATION_ELEMENT_VERSION                                                 (21)
#define  SL_INVALID_RSN_INFORMATION_ELEMENT_CAPABILITIES                                                (22)
#define  SL_IEEE_802_1X_AUTHENTICATION_FAILED                                                           (23)
#define  SL_CIPHER_SUITE_REJECTED_BECAUSE_OF_THE_SECURITY_POLICY                                        (24)
#define  SL_DISCONNECT_RESERVED_2                                                                       (25)
#define  SL_DISCONNECT_RESERVED_3                                                                       (26)
#define  SL_DISCONNECT_RESERVED_4                                                                       (27)
#define  SL_DISCONNECT_RESERVED_5                                                                       (28)
#define  SL_DISCONNECT_RESERVED_6                                                                       (29)
#define  SL_DISCONNECT_RESERVED_7                                                                       (30)
#define  SL_DISCONNECT_RESERVED_8                                                                       (31)
#define  SL_USER_INITIATED_DISCONNECTION                                                                (200)


#define  SL_ERROR_KEY_ERROR                                                                             (-3)
#define  SL_ERROR_INVALID_ROLE                                                                          (-71)
#define  SL_ERROR_INVALID_SECURITY_TYPE                                                                 (-84)
#define  SL_ERROR_PASSPHRASE_TOO_LONG                                                                   (-85)
#define  SL_ERROR_WPS_NO_PIN_OR_WRONG_PIN_LEN                                                            (-87)
#define  SL_ERROR_EAP_WRONG_METHOD                                                                      (-88)
#define  SL_ERROR_PASSWORD_ERROR                                                                        (-89)
#define  SL_ERROR_EAP_ANONYMOUS_LEN_ERROR                                                               (-90)
#define  SL_ERROR_SSID_LEN_ERROR                                                                        (-91)
#define  SL_ERROR_USER_ID_LEN_ERROR                                                                     (-92)
#define  SL_ERROR_ILLEGAL_WEP_KEY_INDEX                                                                 (-95)
#define  SL_ERROR_INVALID_DWELL_TIME_VALUES                                                                (-96)
#define  SL_ERROR_INVALID_POLICY_TYPE                                                                   (-97)
#define  SL_ERROR_PM_POLICY_INVALID_OPTION                                                              (-98)
#define  SL_ERROR_PM_POLICY_INVALID_PARAMS                                                              (-99)
#define  SL_ERROR_WIFI_ALREADY_DISCONNECTED                                                             (-129)
#define  SL_ERROR_WIFI_NOT_CONNECTED                                                                    (-59)



#define SL_SEC_TYPE_OPEN                                                                                (0)
#define SL_SEC_TYPE_WEP                                                                                 (1)
#define SL_SEC_TYPE_WPA                                                                                 (2) 
#define SL_SEC_TYPE_WPA_WPA2                                                                            (2)
#define SL_SEC_TYPE_WPS_PBC                                                                             (3)
#define SL_SEC_TYPE_WPS_PIN                                                                             (4)
#define SL_SEC_TYPE_WPA_ENT                                                                             (5)
#define SL_SEC_TYPE_P2P_PBC                                                                             (6)
#define SL_SEC_TYPE_P2P_PIN_KEYPAD                                                                        (7)
#define SL_SEC_TYPE_P2P_PIN_DISPLAY                                                                        (8)
#define SL_SEC_TYPE_P2P_PIN_AUTO                                                                        (9) 


  
#define SL_SCAN_SEC_TYPE_OPEN                                                                           (0)
#define SL_SCAN_SEC_TYPE_WEP                                                                            (1)
#define SL_SCAN_SEC_TYPE_WPA                                                                            (2) 
#define SL_SCAN_SEC_TYPE_WPA2                                                                           (3)

  
  
#define TLS                                (0x1)
#define MSCHAP                             (0x0)
#define PSK                                (0x2) 
#define TTLS                               (0x10)
#define PEAP0                              (0x20)
#define PEAP1                              (0x40)
#define FAST                               (0x80)

#define FAST_AUTH_PROVISIONING             (0x02)
#define FAST_UNAUTH_PROVISIONING           (0x01)
#define FAST_NO_PROVISIONING               (0x00)

#define EAPMETHOD_PHASE2_SHIFT             (8)
#define EAPMETHOD_PAIRWISE_CIPHER_SHIFT    (19)
#define EAPMETHOD_GROUP_CIPHER_SHIFT       (27)

#define WPA_CIPHER_CCMP                    (0x1) 
#define WPA_CIPHER_TKIP                    (0x2)
#define CC31XX_DEFAULT_CIPHER              (WPA_CIPHER_CCMP | WPA_CIPHER_TKIP)

#define EAPMETHOD(phase1,phase2,pairwise_cipher,group_cipher) \
((phase1) | \
 ((phase2) << EAPMETHOD_PHASE2_SHIFT ) |\
 ((_u32)(pairwise_cipher) << EAPMETHOD_PAIRWISE_CIPHER_SHIFT ) |\
 ((_u32)(group_cipher) << EAPMETHOD_GROUP_CIPHER_SHIFT ))


#define SL_ENT_EAP_METHOD_TLS                       EAPMETHOD(TLS   , 0                        , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_TTLS_TLS                  EAPMETHOD(TTLS  , TLS                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_TTLS_MSCHAPv2             EAPMETHOD(TTLS  , MSCHAP                   , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_TTLS_PSK                  EAPMETHOD(TTLS  , PSK                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_PEAP0_TLS                 EAPMETHOD(PEAP0 , TLS                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_PEAP0_MSCHAPv2            EAPMETHOD(PEAP0 , MSCHAP                   , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER) 
#define SL_ENT_EAP_METHOD_PEAP0_PSK                 EAPMETHOD(PEAP0 , PSK                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_PEAP1_TLS                 EAPMETHOD(PEAP1 , TLS                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_PEAP1_MSCHAPv2            EAPMETHOD(PEAP1 , MSCHAP                   , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER) 
#define SL_ENT_EAP_METHOD_PEAP1_PSK                 EAPMETHOD(PEAP1 , PSK                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_FAST_AUTH_PROVISIONING    EAPMETHOD(FAST  , FAST_AUTH_PROVISIONING   , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_FAST_UNAUTH_PROVISIONING  EAPMETHOD(FAST  , FAST_UNAUTH_PROVISIONING , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_FAST_NO_PROVISIONING      EAPMETHOD(FAST  , FAST_NO_PROVISIONING     , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
 
#define SL_LONG_PREAMBLE                   (0)
#define SL_SHORT_PREAMBLE                   (1)

#define SL_RAW_RF_TX_PARAMS_CHANNEL_SHIFT  (0)
#define SL_RAW_RF_TX_PARAMS_RATE_SHIFT     (6)
#define SL_RAW_RF_TX_PARAMS_POWER_SHIFT    (11)
#define SL_RAW_RF_TX_PARAMS_PREAMBLE_SHIFT (15)

#define SL_RAW_RF_TX_PARAMS(chan,rate,power,preamble) \
    ((chan << SL_RAW_RF_TX_PARAMS_CHANNEL_SHIFT) | \
    (rate << SL_RAW_RF_TX_PARAMS_RATE_SHIFT) | \
    (power << SL_RAW_RF_TX_PARAMS_POWER_SHIFT) | \
    (preamble << SL_RAW_RF_TX_PARAMS_PREAMBLE_SHIFT))



#define SL_WLAN_CFG_AP_ID                    (0)
#define SL_WLAN_CFG_GENERAL_PARAM_ID         (1)
#define SL_WLAN_CFG_P2P_PARAM_ID              (2)


#define WLAN_AP_OPT_SSID                     (0)
#define WLAN_AP_OPT_CHANNEL                  (3)
#define WLAN_AP_OPT_HIDDEN_SSID              (4)
#define WLAN_AP_OPT_SECURITY_TYPE            (6)
#define WLAN_AP_OPT_PASSWORD                 (7)
#define WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE  (9)
#define WLAN_GENERAL_PARAM_OPT_STA_TX_POWER  (10)
#define WLAN_GENERAL_PARAM_OPT_AP_TX_POWER   (11)

#define WLAN_P2P_OPT_DEV_NAME                (12)
#define WLAN_P2P_OPT_DEV_TYPE                (13)
#define WLAN_P2P_OPT_CHANNEL_N_REGS             (14)
#define WLAN_GENERAL_PARAM_OPT_INFO_ELEMENT  (16)
#define WLAN_GENERAL_PARAM_OPT_SCAN_PARAMS   (18)      


#define SMART_CONFIG_CIPHER_SFLASH           (0)      
                                                      
#define SMART_CONFIG_CIPHER_AES              (1)      
#define SMART_CONFIG_CIPHER_NONE             (0xFF)   


#define SL_POLICY_CONNECTION                 (0x10)
#define SL_POLICY_SCAN                       (0x20)
#define SL_POLICY_PM                         (0x30)
#define SL_POLICY_P2P                         (0x40)

#define VAL_2_MASK(position,value)           ((1 & (value))<<(position))
#define MASK_2_VAL(position,mask)            (((1 << position) & (mask)) >> (position))

#define SL_CONNECTION_POLICY(Auto,Fast,Open,anyP2P,autoSmartConfig)                (VAL_2_MASK(0,Auto) | VAL_2_MASK(1,Fast) | VAL_2_MASK(2,Open) | VAL_2_MASK(3,anyP2P) | VAL_2_MASK(4,autoSmartConfig))
#define SL_SCAN_POLICY_EN(policy)            (MASK_2_VAL(0,policy))
#define SL_SCAN_POLICY(Enable)               (VAL_2_MASK(0,Enable))


#define SL_NORMAL_POLICY                    (0)
#define SL_LOW_LATENCY_POLICY               (1)
#define SL_LOW_POWER_POLICY                 (2)
#define SL_ALWAYS_ON_POLICY                 (3)
#define SL_LONG_SLEEP_INTERVAL_POLICY        (4)

#define SL_P2P_ROLE_NEGOTIATE                (3)
#define SL_P2P_ROLE_GROUP_OWNER             (15)
#define SL_P2P_ROLE_CLIENT                    (0)

#define SL_P2P_NEG_INITIATOR_ACTIVE            (0)
#define SL_P2P_NEG_INITIATOR_PASSIVE        (1)
#define SL_P2P_NEG_INITIATOR_RAND_BACKOFF   (2)

#define POLICY_VAL_2_OPTIONS(position,mask,policy)    ((mask & policy) << position )

#define SL_P2P_POLICY(p2pNegType,p2pNegInitiator)   (POLICY_VAL_2_OPTIONS(0,0xF,(p2pNegType > SL_P2P_ROLE_GROUP_OWNER ? SL_P2P_ROLE_GROUP_OWNER : p2pNegType)) | \
                                                     POLICY_VAL_2_OPTIONS(4,0x1,(p2pNegType > SL_P2P_ROLE_GROUP_OWNER ? 1:0)) | \
                                                     POLICY_VAL_2_OPTIONS(5,0x3, p2pNegInitiator))




#define INFO_ELEMENT_DEFAULT_ID              (0) 


#define INFO_ELEMENT_MAX_SIZE                (252)


#define INFO_ELEMENT_MAX_TOTAL_LENGTH_AP     (300)

#define INFO_ELEMENT_MAX_TOTAL_LENGTH_P2P_GO (160)

#define INFO_ELEMENT_AP_ROLE                 (0)
#define INFO_ELEMENT_P2P_GO_ROLE             (1)


#define MAX_PRIVATE_INFO_ELEMENTS_SUPPROTED  (4)

#define INFO_ELEMENT_DEFAULT_OUI_0           (0x08)
#define INFO_ELEMENT_DEFAULT_OUI_1           (0x00)
#define INFO_ELEMENT_DEFAULT_OUI_2           (0x28)

#define INFO_ELEMENT_DEFAULT_OUI             (0x000000)  





typedef enum
{
    RATE_1M         = 1,
    RATE_2M         = 2,
    RATE_5_5M       = 3,
    RATE_11M        = 4,
    RATE_6M         = 6,
    RATE_9M         = 7,
    RATE_12M        = 8,
    RATE_18M        = 9,
    RATE_24M        = 10,
    RATE_36M        = 11,
    RATE_48M        = 12,
    RATE_54M        = 13,
    RATE_MCS_0      = 14,
    RATE_MCS_1      = 15,
    RATE_MCS_2      = 16,
    RATE_MCS_3      = 17,
    RATE_MCS_4      = 18,
    RATE_MCS_5      = 19,
    RATE_MCS_6      = 20,
    RATE_MCS_7      = 21,
    MAX_NUM_RATES   = 0xFF
}SlRateIndex_e;

typedef enum {
    DEV_PW_DEFAULT=0,
    DEV_PW_PIN_KEYPAD=1,
    DEV_PW_PUSH_BUTTON=4,
    DEV_PW_PIN_DISPLAY=5
} sl_p2p_dev_password_method;


typedef struct
{
  _u32    status;
  _u32    ssid_len;
  _u8     ssid[32];
  _u32    private_token_len;
  _u8     private_token[32];
}slSmartConfigStartAsyncResponse_t;

typedef struct
{
  _u16    status;
  _u16    padding;
}slSmartConfigStopAsyncResponse_t;

typedef struct
{
  _u16    status;
  _u16    padding;
}slWlanConnFailureAsyncResponse_t;

typedef struct
{
  _u8     connection_type;
  _u8     ssid_len;
  _u8     ssid_name[32];
  _u8     go_peer_device_name_len;
  _u8     go_peer_device_name[32];
  _u8     bssid[6];
  _u8     reason_code;
  _u8     padding[2];
} slWlanConnectAsyncResponse_t;

typedef struct
{
  _u8     go_peer_device_name[32];
  _u8     mac[6];
  _u8     go_peer_device_name_len;
  _u8     wps_dev_password_id;
  _u8     own_ssid[32];
  _u8     own_ssid_len;
  _u8     padding[3];
}slPeerInfoAsyncResponse_t;


typedef union
{
  slSmartConfigStartAsyncResponse_t        smartConfigStartResponse; 
  slSmartConfigStopAsyncResponse_t         smartConfigStopResponse;  
  slPeerInfoAsyncResponse_t                APModeStaConnected;          
  slPeerInfoAsyncResponse_t                APModestaDisconnected;     
  slWlanConnectAsyncResponse_t             STAandP2PModeWlanConnected;   
  slWlanConnectAsyncResponse_t             STAandP2PModeDisconnected;   
  slPeerInfoAsyncResponse_t                P2PModeDevFound;             
  slPeerInfoAsyncResponse_t                P2PModeNegReqReceived;       
  slWlanConnFailureAsyncResponse_t         P2PModewlanConnectionFailure;   

} SlWlanEventData_u;

typedef struct
{
   _u32     Event;
   SlWlanEventData_u        EventData;
} SlWlanEvent_t;


typedef struct 
{
    _u32  ReceivedValidPacketsNumber;                     
    _u32  ReceivedFcsErrorPacketsNumber;                   
    _u32  ReceivedAddressMismatchPacketsNumber;           
    _i16  AvarageDataCtrlRssi;                            
    _i16  AvarageMgMntRssi;                               
    _u16  RateHistogram[NUM_OF_RATE_INDEXES];             
    _u16  RssiHistogram[SIZE_OF_RSSI_HISTOGRAM];          
    _u32  StartTimeStamp;                                 
    _u32  GetTimeStamp;                                   
}SlGetRxStatResponse_t;


typedef struct
{
    _u8 ssid[MAXIMAL_SSID_LENGTH];
    _u8 ssid_len;
    _u8 sec_type;
    _u8 bssid[SL_BSSID_LENGTH];
    _i8 rssi;
    _i8 reserved[3];
}Sl_WlanNetworkEntry_t;

 
typedef struct 
{
    _u8   Type;
    _i8*  Key;
    _u8   KeyLen;
}SlSecParams_t;
 
typedef struct 
{
    _i8*  User;
    _u8   UserLen;
    _i8*  AnonUser;
    _u8   AnonUserLen;
    _u8   CertIndex;  
    _u32  EapMethod;
}SlSecParamsExt_t;

typedef struct 
{
    _i8   User[32];
    _u8   UserLen;
    _i8   AnonUser[32];
    _u8   AnonUserLen;
    _u8   CertIndex;  
    _u32  EapMethod;
}SlGetSecParamsExt_t;

typedef enum
{
    ROLE_STA   =   0,
    ROLE_AP    =   2,
    ROLE_P2P     =   3,
    ROLE_STA_ERR =  -1,         
    ROLE_AP_ERR  =  -ROLE_AP,   
    ROLE_P2P_ERR =  -ROLE_P2P   
}SlWlanMode_t;

typedef struct
{
    _u32   G_Channels_mask;
    _i32   rssiThershold;
}slWlanScanParamCommand_t;


typedef struct 
{
    _u8   id;
    _u8   oui[3];
    _u16  length;
    _u8   data[252];
} sl_protocol_InfoElement_t;

typedef struct 
{
    _u8                       index;  
    _u8                       role;   
    sl_protocol_InfoElement_t   ie;
} sl_protocol_WlanSetInfoElement_t;







 
#if _SL_INCLUDE_FUNC(sl_WlanConnect)
_i16 sl_WlanConnect(const _i8*  pName,const  _i16 NameLen,const _u8 *pMacAddr,const SlSecParams_t* pSecParams ,const SlSecParamsExt_t* pSecExtParams);
#endif


#if _SL_INCLUDE_FUNC(sl_WlanDisconnect)
_i16 sl_WlanDisconnect(void);
#endif


#if _SL_INCLUDE_FUNC(sl_WlanProfileAdd)
_i16 sl_WlanProfileAdd(const _i8*  pName,const  _i16 NameLen,const _u8 *pMacAddr,const SlSecParams_t* pSecParams ,const SlSecParamsExt_t* pSecExtParams,const _u32 Priority,const _u32  Options);
#endif


#if _SL_INCLUDE_FUNC(sl_WlanProfileGet)
_i16 sl_WlanProfileGet(const _i16 Index,_i8*  pName, _i16 *pNameLen, _u8 *pMacAddr, SlSecParams_t* pSecParams, SlGetSecParamsExt_t* pSecExtParams, _u32 *pPriority);
#endif


#if _SL_INCLUDE_FUNC(sl_WlanProfileDel)
_i16 sl_WlanProfileDel(const _i16 Index);
#endif


#if _SL_INCLUDE_FUNC(sl_WlanPolicySet)
_i16 sl_WlanPolicySet(const _u8 Type , const _u8 Policy, _u8 *pVal,const _u8 ValLen);
#endif

#if _SL_INCLUDE_FUNC(sl_WlanPolicyGet)
_i16 sl_WlanPolicyGet(const _u8 Type , _u8 Policy,_u8 *pVal,_u8 *pValLen);
#endif

#if _SL_INCLUDE_FUNC(sl_WlanGetNetworkList)
_i16 sl_WlanGetNetworkList(const _u8 Index,const  _u8 Count, Sl_WlanNetworkEntry_t *pEntries);
#endif


#if _SL_INCLUDE_FUNC(sl_WlanRxStatStart)
_i16 sl_WlanRxStatStart(void);
#endif



#if _SL_INCLUDE_FUNC(sl_WlanRxStatStop)
_i16 sl_WlanRxStatStop(void);
#endif



#if _SL_INCLUDE_FUNC(sl_WlanRxStatGet)
_i16 sl_WlanRxStatGet(SlGetRxStatResponse_t *pRxStat,const _u32 Flags);
#endif



#if _SL_INCLUDE_FUNC(sl_WlanSmartConfigStop)
_i16 sl_WlanSmartConfigStop(void);
#endif


#if _SL_INCLUDE_FUNC(sl_WlanSmartConfigStart)
_i16 sl_WlanSmartConfigStart(const _u32    groupIdBitmask,
                             const _u8    cipher,
                             const _u8    publicKeyLen,
                             const _u8    group1KeyLen,
                             const _u8    group2KeyLen,
                             const _u8*    publicKey,
                             const _u8*    group1Key,
                             const _u8*    group2Key);
#endif



#if _SL_INCLUDE_FUNC(sl_WlanSetMode)
_i16 sl_WlanSetMode(const _u8    mode);
#endif



#if _SL_INCLUDE_FUNC(sl_WlanSet)
_i16 sl_WlanSet(const _u16 ConfigId ,const _u16 ConfigOpt,const _u16 ConfigLen,const  _u8 *pValues);
#endif



#if _SL_INCLUDE_FUNC(sl_WlanGet)
_i16 sl_WlanGet(const _u16 ConfigId, _u16 *pConfigOpt,_u16 *pConfigLen, _u8 *pValues);
#endif



#ifdef  __cplusplus
}
#endif 

#endif    

