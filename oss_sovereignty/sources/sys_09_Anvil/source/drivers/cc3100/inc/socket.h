
 



#include "simplelink.h"

#ifndef __SL_SOCKET_H__
#define __SL_SOCKET_H__




#ifdef    __cplusplus
extern "C" {
#endif







#define SL_FD_SETSIZE                         SL_MAX_SOCKETS         
#define BSD_SOCKET_ID_MASK                     (0x0F)                 

#define SL_SOCK_STREAM                         (1)                       
#define SL_SOCK_DGRAM                          (2)                       
#define SL_SOCK_RAW                            (3)                       
#define SL_IPPROTO_TCP                         (6)                       
#define SL_IPPROTO_UDP                         (17)                      
#define SL_IPPROTO_RAW                         (255)                     
#define SL_SEC_SOCKET                          (100)                     


#define     SL_AF_INET                         (2)                       
#define     SL_AF_INET6                        (3)                       
#define     SL_AF_INET6_EUI_48                 (9)
#define     SL_AF_RF                           (6)                        
#define     SL_AF_PACKET                       (17)

#define     SL_PF_INET                         AF_INET
#define     SL_PF_INET6                        AF_INET6
#define     SL_INADDR_ANY                      (0)                       


#define SL_SOC_ERROR                          (-1)  
#define SL_SOC_OK                             ( 0)  
#define SL_INEXE                              (-8)   
#define SL_EBADF                              (-9)   
#define SL_ENSOCK                             (-10)  
#define SL_EAGAIN                             (-11)  
#define SL_EWOULDBLOCK                        SL_EAGAIN
#define SL_ENOMEM                             (-12)  
#define SL_EACCES                             (-13)  
#define SL_EFAULT                             (-14)  
#define SL_ECLOSE                             (-15)  
#define SL_EALREADY_ENABLED                   (-21)  
#define SL_EINVAL                             (-22)  
#define SL_EAUTO_CONNECT_OR_CONNECTING        (-69)  
#define SL_CONNECTION_PENDING                  (-72)  
#define SL_EUNSUPPORTED_ROLE                  (-86)  
#define SL_EDESTADDRREQ                       (-89)  
#define SL_EPROTOTYPE                         (-91)  
#define SL_ENOPROTOOPT                        (-92)  
#define SL_EPROTONOSUPPORT                    (-93)  
#define SL_ESOCKTNOSUPPORT                    (-94)  
#define SL_EOPNOTSUPP                         (-95)  
#define SL_EAFNOSUPPORT                       (-97)  
#define SL_EADDRINUSE                         (-98)  
#define SL_EADDRNOTAVAIL                      (-99)  
#define SL_ENETUNREACH                        (-101) 
#define SL_ENOBUFS                            (-105) 
#define SL_EOBUFF                             SL_ENOBUFS 
#define SL_EISCONN                            (-106) 
#define SL_ENOTCONN                           (-107) 
#define SL_ETIMEDOUT                          (-110) 
#define SL_ECONNREFUSED                       (-111) 
#define SL_EALREADY                           (-114)  

#define SL_ESEC_RSA_WRONG_TYPE_E              (-130)  
#define SL_ESEC_RSA_BUFFER_E                  (-131)  
#define SL_ESEC_BUFFER_E                      (-132)  
#define SL_ESEC_ALGO_ID_E                     (-133)  
#define SL_ESEC_PUBLIC_KEY_E                  (-134)  
#define SL_ESEC_DATE_E                        (-135)  
#define SL_ESEC_SUBJECT_E                     (-136)  
#define SL_ESEC_ISSUER_E                      (-137)  
#define SL_ESEC_CA_TRUE_E                     (-138)  
#define SL_ESEC_EXTENSIONS_E                  (-139)  
#define SL_ESEC_ASN_PARSE_E                   (-140)  
#define SL_ESEC_ASN_VERSION_E                 (-141)  
#define SL_ESEC_ASN_GETINT_E                  (-142)  
#define SL_ESEC_ASN_RSA_KEY_E                 (-143)  
#define SL_ESEC_ASN_OBJECT_ID_E               (-144)  
#define SL_ESEC_ASN_TAG_NULL_E                (-145)  
#define SL_ESEC_ASN_EXPECT_0_E                (-146)  
#define SL_ESEC_ASN_BITSTR_E                  (-147)  
#define SL_ESEC_ASN_UNKNOWN_OID_E             (-148)  
#define SL_ESEC_ASN_DATE_SZ_E                 (-149)  
#define SL_ESEC_ASN_BEFORE_DATE_E             (-150)  
#define SL_ESEC_ASN_AFTER_DATE_E              (-151)  
#define SL_ESEC_ASN_SIG_OID_E                 (-152)  
#define SL_ESEC_ASN_TIME_E                    (-153)  
#define SL_ESEC_ASN_INPUT_E                   (-154)  
#define SL_ESEC_ASN_SIG_CONFIRM_E             (-155)  
#define SL_ESEC_ASN_SIG_HASH_E                (-156)  
#define SL_ESEC_ASN_SIG_KEY_E                 (-157)  
#define SL_ESEC_ASN_DH_KEY_E                  (-158)  
#define SL_ESEC_ASN_NTRU_KEY_E                (-159)  
#define SL_ESEC_ECC_BAD_ARG_E                 (-170)  
#define SL_ESEC_ASN_ECC_KEY_E                 (-171)  
#define SL_ESEC_ECC_CURVE_OID_E               (-172)  
#define SL_ESEC_BAD_FUNC_ARG                  (-173)  
#define SL_ESEC_NOT_COMPILED_IN               (-174)  
#define SL_ESEC_UNICODE_SIZE_E                (-175)  
#define SL_ESEC_NO_PASSWORD                   (-176)  
#define SL_ESEC_ALT_NAME_E                    (-177)  
#define SL_ESEC_AES_GCM_AUTH_E                (-180)  
#define SL_ESEC_AES_CCM_AUTH_E                (-181)  
#define SL_SOCKET_ERROR_E                     (-208)  

#define SL_ESEC_MEMORY_ERROR                  (-203)  
#define SL_ESEC_VERIFY_FINISHED_ERROR         (-204)  
#define SL_ESEC_VERIFY_MAC_ERROR              (-205)  
#define SL_ESEC_UNKNOWN_HANDSHAKE_TYPE        (-207)  
#define SL_ESEC_SOCKET_ERROR_E                (-208)  
#define SL_ESEC_SOCKET_NODATA                 (-209)  
#define SL_ESEC_INCOMPLETE_DATA               (-210)  
#define SL_ESEC_UNKNOWN_RECORD_TYPE           (-211)  
#define SL_ESEC_FATAL_ERROR                   (-213)  
#define SL_ESEC_ENCRYPT_ERROR                 (-214)  
#define SL_ESEC_NO_PEER_KEY                   (-216)  
#define SL_ESEC_NO_PRIVATE_KEY                (-217)  
#define SL_ESEC_RSA_PRIVATE_ERROR             (-218)  
#define SL_ESEC_NO_DH_PARAMS                  (-219)  
#define SL_ESEC_BUILD_MSG_ERROR               (-220)  
#define SL_ESEC_BAD_HELLO                     (-221)  
#define SL_ESEC_DOMAIN_NAME_MISMATCH          (-222)  
#define SL_ESEC_WANT_READ                     (-223)  
#define SL_ESEC_NOT_READY_ERROR               (-224)  
#define SL_ESEC_PMS_VERSION_ERROR             (-225)  
#define SL_ESEC_VERSION_ERROR                 (-226)  
#define SL_ESEC_WANT_WRITE                    (-227)  
#define SL_ESEC_BUFFER_ERROR                  (-228)  
#define SL_ESEC_VERIFY_CERT_ERROR             (-229)  
#define SL_ESEC_VERIFY_SIGN_ERROR             (-230)  

#define SL_ESEC_LENGTH_ERROR                  (-241)  
#define SL_ESEC_PEER_KEY_ERROR                (-242)  
#define SL_ESEC_ZERO_RETURN                   (-243)  
#define SL_ESEC_SIDE_ERROR                    (-244)  
#define SL_ESEC_NO_PEER_CERT                  (-245)  
#define SL_ESEC_ECC_CURVETYPE_ERROR           (-250)  
#define SL_ESEC_ECC_CURVE_ERROR               (-251)  
#define SL_ESEC_ECC_PEERKEY_ERROR             (-252)  
#define SL_ESEC_ECC_MAKEKEY_ERROR             (-253)  
#define SL_ESEC_ECC_EXPORT_ERROR              (-254)  
#define SL_ESEC_ECC_SHARED_ERROR              (-255)  
#define SL_ESEC_NOT_CA_ERROR                  (-257)  
#define SL_ESEC_BAD_PATH_ERROR                (-258)  
#define SL_ESEC_BAD_CERT_MANAGER_ERROR        (-259)  
#define SL_ESEC_MAX_CHAIN_ERROR               (-268)  
#define SL_ESEC_SUITES_ERROR                  (-271)  
#define SL_ESEC_SSL_NO_PEM_HEADER             (-272)  
#define SL_ESEC_OUT_OF_ORDER_E                (-273)  
#define SL_ESEC_SANITY_CIPHER_E               (-275)  
#define SL_ESEC_GEN_COOKIE_E                  (-277)  
#define SL_ESEC_NO_PEER_VERIFY                (-278)  
#define SL_ESEC_UNKNOWN_SNI_HOST_NAME_E       (-281)  

#define SL_ESEC_UNSUPPORTED_SUITE     (-290)            
#define SL_ESEC_MATCH_SUITE_ERROR      (-291 )            


#define SL_ESEC_CLOSE_NOTIFY                  (-300)    
#define SL_ESEC_UNEXPECTED_MESSAGE            (-310)    
#define SL_ESEC_BAD_RECORD_MAC                (-320)                  
#define SL_ESEC_DECRYPTION_FAILED             (-321)    
#define SL_ESEC_RECORD_OVERFLOW               (-322)     
#define SL_ESEC_DECOMPRESSION_FAILURE         (-330)                  
#define SL_ESEC_HANDSHAKE_FAILURE             (-340)     
#define SL_ESEC_NO_CERTIFICATE                (-341)     
#define SL_ESEC_BAD_CERTIFICATE               (-342)           
#define SL_ESEC_UNSUPPORTED_CERTIFICATE       (-343)      
#define SL_ESEC_CERTIFICATE_REVOKED           (-344)                  
#define SL_ESEC_CERTIFICATE_EXPIRED           (-345)                  
#define SL_ESEC_CERTIFICATE_UNKNOWN           (-346)                  
#define SL_ESEC_ILLEGAL_PARAMETER             (-347)                  
#define SL_ESEC_UNKNOWN_CA                    (-348)                 
#define SL_ESEC_ACCESS_DENIED                 (-349)                 
#define SL_ESEC_DECODE_ERROR                  (-350)    
#define SL_ESEC_DECRYPT_ERROR                 (-351)    
#define SL_ESEC_EXPORT_RESTRICTION            (-360)     
#define SL_ESEC_PROTOCOL_VERSION              (-370)     
#define SL_ESEC_INSUFFICIENT_SECURITY         (-371)    
#define SL_ESEC_INTERNAL_ERROR                (-380)    
#define SL_ESEC_USER_CANCELLED                (-390)    
#define SL_ESEC_NO_RENEGOTIATION              (-400)    
#define SL_ESEC_UNSUPPORTED_EXTENSION         (-410)    
#define SL_ESEC_CERTIFICATE_UNOBTAINABLE      (-411)          
#define SL_ESEC_UNRECOGNIZED_NAME             (-412)    
#define SL_ESEC_BAD_CERTIFICATE_STATUS_RESPONSE  (-413)    
#define SL_ESEC_BAD_CERTIFICATE_HASH_VALUE    (-414)    

#define SL_ESECGENERAL                        (-450)  
#define SL_ESECDECRYPT                        (-451)  
#define SL_ESECCLOSED                         (-452)  
#define SL_ESECSNOVERIFY                      (-453)  
#define SL_ESECNOCAFILE                       (-454)  
#define SL_ESECMEMORY                         (-455)  
#define SL_ESECBADCAFILE                      (-456)  
#define SL_ESECBADCERTFILE                    (-457)  
#define SL_ESECBADPRIVATEFILE                 (-458)  
#define SL_ESECBADDHFILE                      (-459)  
#define SL_ESECT00MANYSSLOPENED               (-460)  
#define SL_ESECDATEERROR                      (-461)  
#define SL_ESECHANDSHAKETIMEDOUT              (-462)  




#define SL_SOCKET_PAYLOAD_TYPE_MASK            (0xF0)  
#define SL_SOCKET_PAYLOAD_TYPE_UDP_IPV4        (0x00)  
#define SL_SOCKET_PAYLOAD_TYPE_TCP_IPV4        (0x10)  
#define SL_SOCKET_PAYLOAD_TYPE_UDP_IPV6        (0x20)  
#define SL_SOCKET_PAYLOAD_TYPE_TCP_IPV6        (0x30)  
#define SL_SOCKET_PAYLOAD_TYPE_UDP_IPV4_SECURE (0x40)  
#define SL_SOCKET_PAYLOAD_TYPE_TCP_IPV4_SECURE (0x50)  
#define SL_SOCKET_PAYLOAD_TYPE_UDP_IPV6_SECURE (0x60)  
#define SL_SOCKET_PAYLOAD_TYPE_TCP_IPV6_SECURE (0x70)  
#define SL_SOCKET_PAYLOAD_TYPE_RAW_TRANCEIVER  (0x80)  
#define SL_SOCKET_PAYLOAD_TYPE_RAW_PACKET      (0x90)  
#define SL_SOCKET_PAYLOAD_TYPE_RAW_IP4         (0xa0)  
#define SL_SOCKET_PAYLOAD_TYPE_RAW_IP6         (SL_SOCKET_PAYLOAD_TYPE_RAW_IP4 )  

  

#define SL_SOL_SOCKET          (1)   
#define SL_IPPROTO_IP          (2)   
#define SL_SOL_PHY_OPT         (3)   

#define SL_SO_RCVBUF           (8)   
#define SL_SO_KEEPALIVE        (9)   
#define SL_SO_RCVTIMEO         (20)  
#define SL_SO_NONBLOCKING      (24)  
#define SL_SO_SECMETHOD        (25)  
#define SL_SO_SECURE_MASK      (26)  
#define SL_SO_SECURE_FILES     (27)  
#define SL_SO_CHANGE_CHANNEL   (28)  
#define SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME (30) 
#define SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME (31) 
#define SL_SO_SECURE_FILES_CA_FILE_NAME          (32) 
#define SL_SO_SECURE_FILES_DH_KEY_FILE_NAME      (33) 

#define SL_IP_MULTICAST_IF     (60) 
#define SL_IP_MULTICAST_TTL    (61) 
#define SL_IP_ADD_MEMBERSHIP   (65) 
#define SL_IP_DROP_MEMBERSHIP  (66) 
#define SL_IP_HDRINCL          (67) 
#define SL_IP_RAW_RX_NO_HEADER (68) 
#define SL_IP_RAW_IPV6_HDRINCL (69) 

#define SL_SO_PHY_RATE              (100)   
#define SL_SO_PHY_TX_POWER          (101)     
#define SL_SO_PHY_NUM_FRAMES_TO_TX  (102)   
#define SL_SO_PHY_PREAMBLE          (103)   

#define SL_SO_SEC_METHOD_SSLV3                             (0)  
#define SL_SO_SEC_METHOD_TLSV1                             (1)  
#define SL_SO_SEC_METHOD_TLSV1_1                           (2)  
#define SL_SO_SEC_METHOD_TLSV1_2                           (3)  
#define SL_SO_SEC_METHOD_SSLv3_TLSV1_2                     (4)  
#define SL_SO_SEC_METHOD_DLSV1                             (5)  

#define SL_SEC_MASK_SSL_RSA_WITH_RC4_128_SHA               (1 << 0)
#define SL_SEC_MASK_SSL_RSA_WITH_RC4_128_MD5               (1 << 1)
#define SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA           (1 << 2)
#define SL_SEC_MASK_TLS_DHE_RSA_WITH_AES_256_CBC_SHA       (1 << 3)
#define SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA     (1 << 4)
#define SL_SEC_MASK_TLS_ECDHE_RSA_WITH_RC4_128_SHA         (1 << 5)
#define SL_SEC_MASK_TLS_RSA_WITH_AES_128_CBC_SHA256              (1 << 6)
#define SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA256              (1 << 7)
#define SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256        (1 << 8)
#define SL_SEC_MASK_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256      (1 << 9)


#define SL_SEC_MASK_SECURE_DEFAULT                         ((SL_SEC_MASK_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256  <<  1)  -  1)

#define SL_MSG_DONTWAIT                                   (0x00000008)  


#define SL_IP_LEASE_PEER_RELEASE     (0)
#define SL_IP_LEASE_PEER_DECLINE     (1)
#define SL_IP_LEASE_EXPIRED          (2)


#define SSL_ACCEPT                                (1) 
#define RX_FRAGMENTATION_TOO_BIG                  (2) 
#define OTHER_SIDE_CLOSE_SSL_DATA_NOT_ENCRYPTED   (3) 



#ifdef SL_INC_STD_BSD_API_NAMING

#define FD_SETSIZE                          SL_FD_SETSIZE        
                                                                       
#define SOCK_STREAM                         SL_SOCK_STREAM        
#define SOCK_DGRAM                          SL_SOCK_DGRAM         
#define SOCK_RAW                            SL_SOCK_RAW           
#define IPPROTO_TCP                         SL_IPPROTO_TCP        
#define IPPROTO_UDP                         SL_IPPROTO_UDP        
#define IPPROTO_RAW                         SL_IPPROTO_RAW        
                                                                       
#define AF_INET                             SL_AF_INET            
#define AF_INET6                            SL_AF_INET6           
#define AF_INET6_EUI_48                     SL_AF_INET6_EUI_48
#define AF_RF                               SL_AF_RF              
#define AF_PACKET                           SL_AF_PACKET              
                                                                       
#define PF_INET                             SL_PF_INET            
#define PF_INET6                            SL_PF_INET6           
                                                                       
#define INADDR_ANY                          SL_INADDR_ANY                                                   
#define ERROR                               SL_SOC_ERROR                                                                                                                
#define INEXE                               SL_INEXE                 
#define EBADF                               SL_EBADF                 
#define ENSOCK                              SL_ENSOCK                
#define EAGAIN                              SL_EAGAIN                
#define EWOULDBLOCK                         SL_EWOULDBLOCK           
#define ENOMEM                              SL_ENOMEM                
#define EACCES                              SL_EACCES                
#define EFAULT                              SL_EFAULT                
#define EINVAL                              SL_EINVAL                
#define EDESTADDRREQ                        SL_EDESTADDRREQ          
#define EPROTOTYPE                          SL_EPROTOTYPE            
#define ENOPROTOOPT                         SL_ENOPROTOOPT           
#define EPROTONOSUPPORT                     SL_EPROTONOSUPPORT       
#define ESOCKTNOSUPPORT                     SL_ESOCKTNOSUPPORT       
#define EOPNOTSUPP                          SL_EOPNOTSUPP            
#define EAFNOSUPPORT                        SL_EAFNOSUPPORT          
#define EADDRINUSE                          SL_EADDRINUSE            
#define EADDRNOTAVAIL                       SL_EADDRNOTAVAIL         
#define ENETUNREACH                         SL_ENETUNREACH           
#define ENOBUFS                             SL_ENOBUFS               
#define EOBUFF                              SL_EOBUFF                
#define EISCONN                             SL_EISCONN               
#define ENOTCONN                            SL_ENOTCONN              
#define ETIMEDOUT                           SL_ETIMEDOUT             
#define ECONNREFUSED                        SL_ECONNREFUSED          

#define SOL_SOCKET                          SL_SOL_SOCKET         
#define IPPROTO_IP                          SL_IPPROTO_IP                     
#define SO_KEEPALIVE                        SL_SO_KEEPALIVE            
                                                                       
#define SO_RCVTIMEO                         SL_SO_RCVTIMEO        
#define SO_NONBLOCKING                      SL_SO_NONBLOCKING     
                                                                       
#define IP_MULTICAST_IF                     SL_IP_MULTICAST_IF    
#define IP_MULTICAST_TTL                    SL_IP_MULTICAST_TTL   
#define IP_ADD_MEMBERSHIP                   SL_IP_ADD_MEMBERSHIP  
#define IP_DROP_MEMBERSHIP                  SL_IP_DROP_MEMBERSHIP 
                                                                       
#define socklen_t                           SlSocklen_t
#define timeval                             SlTimeval_t
#define sockaddr                            SlSockAddr_t
#define in6_addr                            SlIn6Addr_t
#define sockaddr_in6                        SlSockAddrIn6_t
#define in_addr                             SlInAddr_t
#define sockaddr_in                         SlSockAddrIn_t
                                                                       
#define MSG_DONTWAIT                        SL_MSG_DONTWAIT       
                                                                       
#define FD_SET                              SL_FD_SET  
#define FD_CLR                              SL_FD_CLR  
#define FD_ISSET                            SL_FD_ISSET
#define FD_ZERO                             SL_FD_ZERO 
#define fd_set                              SlFdSet_t    

#define socket                              sl_Socket
#define close                               sl_Close
#define accept                              sl_Accept
#define bind                                sl_Bind
#define listen                              sl_Listen
#define connect                             sl_Connect
#define select                              sl_Select
#define setsockopt                          sl_SetSockOpt
#define getsockopt                          sl_GetSockOpt
#define recv                                sl_Recv
#define recvfrom                            sl_RecvFrom
#define write                               sl_Write
#define send                                sl_Send
#define sendto                              sl_SendTo
#define gethostbyname                       sl_NetAppDnsGetHostByName
#define htonl                               sl_Htonl
#define ntohl                               sl_Ntohl
#define htons                               sl_Htons
#define ntohs                               sl_Ntohs
#endif






typedef struct SlInAddr_t
{
#ifndef s_addr 
    _u32           s_addr;             
#else
    union S_un {
       struct { _u8 s_b1,s_b2,s_b3,s_b4; } S_un_b;
       struct { _u8 s_w1,s_w2; } S_un_w;
        _u32 S_addr;
    } S_un;
#endif
}SlInAddr_t;



typedef struct 
{
    _u32 KeepaliveEnabled; 
}SlSockKeepalive_t;

typedef struct 
{
    _u32 ReuseaddrEnabled; 
}SlSockReuseaddr_t;

typedef struct 
{
    _u32 Winsize;          
}SlSockWinsize_t;

typedef struct 
{
    _u32 NonblockingEnabled;
}SlSockNonblocking_t;


typedef struct
{
  _u8   sd;
  _u8   type;
  _i16  val;
  _u8*  pExtraInfo;
} SlSocketAsyncEvent_t;

typedef struct
{
  _i16       status;
  _u8        sd;
  _u8        padding;
} SlSockTxFailEventData_t;


typedef union
{
  SlSockTxFailEventData_t   SockTxFailData;
  SlSocketAsyncEvent_t      SockAsyncData;
} SlSockEventData_u;


typedef struct
{
   _u32                    Event;
   SlSockEventData_u       socketAsyncEvent;
} SlSockEvent_t;






typedef struct
{
    _u32    secureMask;
} SlSockSecureMask;

typedef struct
{
    _u8     secureMethod;
} SlSockSecureMethod;

typedef enum
{
  SL_BSD_SECURED_PRIVATE_KEY_IDX = 0,
  SL_BSD_SECURED_CERTIFICATE_IDX,
  SL_BSD_SECURED_CA_IDX,
  SL_BSD_SECURED_DH_IDX
}slBsd_secureSocketFilesIndex_e;

typedef struct 
{
    SlInAddr_t   imr_multiaddr;     
    SlInAddr_t   imr_interface;     
} SlSockIpMreq;



typedef _u32   SlTime_t;
typedef _u32   SlSuseconds_t;

typedef struct SlTimeval_t
{
    SlTime_t          tv_sec;             
    SlSuseconds_t     tv_usec;            
}SlTimeval_t;

typedef _u16 SlSocklen_t;


typedef struct SlSockAddr_t
{
    _u16          sa_family;     
    _u8           sa_data[14];  
}SlSockAddr_t;



typedef struct SlIn6Addr_t
{
    union 
    {
        _u8   _S6_u8[16];
        _u32  _S6_u32[4];
    } _S6_un;
}SlIn6Addr_t;

typedef struct SlSockAddrIn6_t
{
    _u16           sin6_family;                 
    _u16           sin6_port;                   
    _u32           sin6_flowinfo;               
    SlIn6Addr_t             sin6_addr;                   
    _u32           sin6_scope_id;               
}SlSockAddrIn6_t;



typedef struct SlSockAddrIn_t
{
    _u16              sin_family;         
    _u16              sin_port;           
    SlInAddr_t                  sin_addr;           
    _i8               sin_zero[8];        
}SlSockAddrIn_t;

typedef struct
{
    _u32 ip;
    _u32 gateway;
    _u32 dns;
}SlIpV4AcquiredAsync_t;

typedef struct  
{
    _u32 type;
    _u32 ip[4];
    _u32 gateway[4];
    _u32 dns[4];
}SlIpV6AcquiredAsync_t;

typedef struct
{
   _u32    ip_address;
   _u32    lease_time;
   _u8     mac[6];
   _u16    padding;
}SlIpLeasedAsync_t;

typedef struct
{
  _u32    ip_address;
  _u8     mac[6];
  _u16    reason;
}SlIpReleasedAsync_t;


typedef union
{
  SlIpV4AcquiredAsync_t    ipAcquiredV4; 
  SlIpV6AcquiredAsync_t    ipAcquiredV6; 
  _u32                      sd;            
  SlIpLeasedAsync_t        ipLeased;     
  SlIpReleasedAsync_t      ipReleased;   
} SlNetAppEventData_u;

typedef struct
{
   _u32                     Event;
   SlNetAppEventData_u       EventData;
}SlNetAppEvent_t;


typedef struct sock_secureFiles
{
    _u8                     secureFiles[4];
}SlSockSecureFiles_t;


typedef struct SlFdSet_t                    
{ 
   _u32        fd_array[(SL_FD_SETSIZE + 31)/32]; 
} SlFdSet_t;

typedef struct
{
    _u8   rate;               
    _u8   channel;            
    _i8    rssi;               
    _u8   padding;                                           
    _u32  timestamp;          
}SlTransceiverRxOverHead_t;








#if _SL_INCLUDE_FUNC(sl_Socket)
_i16 sl_Socket(_i16 Domain, _i16 Type, _i16 Protocol);
#endif


#if _SL_INCLUDE_FUNC(sl_Close)
_i16 sl_Close(_i16 sd);
#endif


#if _SL_INCLUDE_FUNC(sl_Accept)
_i16 sl_Accept(_i16 sd, SlSockAddr_t *addr, SlSocklen_t *addrlen);
#endif


#if _SL_INCLUDE_FUNC(sl_Bind)
_i16 sl_Bind(_i16 sd, const SlSockAddr_t *addr, _i16 addrlen);
#endif


#if _SL_INCLUDE_FUNC(sl_Listen)
_i16 sl_Listen(_i16 sd, _i16 backlog);
#endif


#if _SL_INCLUDE_FUNC(sl_Connect)
_i16 sl_Connect(_i16 sd, const SlSockAddr_t *addr, _i16 addrlen);
#endif


#if _SL_INCLUDE_FUNC(sl_Select)
_i16 sl_Select(_i16 nfds, SlFdSet_t *readsds, SlFdSet_t *writesds, SlFdSet_t *exceptsds, struct SlTimeval_t *timeout);



void SL_FD_SET(_i16 fd, SlFdSet_t *fdset);


void SL_FD_CLR(_i16 fd, SlFdSet_t *fdset);



_i16  SL_FD_ISSET(_i16 fd, SlFdSet_t *fdset);


void SL_FD_ZERO(SlFdSet_t *fdset);



#endif


#if _SL_INCLUDE_FUNC(sl_SetSockOpt)
_i16 sl_SetSockOpt(_i16 sd, _i16 level, _i16 optname, const void *optval, SlSocklen_t optlen);
#endif


#if _SL_INCLUDE_FUNC(sl_GetSockOpt)
_i16 sl_GetSockOpt(_i16 sd, _i16 level, _i16 optname, void *optval, SlSocklen_t *optlen);
#endif


#if _SL_INCLUDE_FUNC(sl_Recv)
_i16 sl_Recv(_i16 sd, void *buf, _i16 Len, _i16 flags);
#endif


#if _SL_INCLUDE_FUNC(sl_RecvFrom)
_i16 sl_RecvFrom(_i16 sd, void *buf, _i16 Len, _i16 flags, SlSockAddr_t *from, SlSocklen_t *fromlen);
#endif

 
#if _SL_INCLUDE_FUNC(sl_Send )
_i16 sl_Send(_i16 sd, const void *buf, _i16 Len, _i16 flags);
#endif


#if _SL_INCLUDE_FUNC(sl_SendTo)
_i16 sl_SendTo(_i16 sd, const void *buf, _i16 Len, _i16 flags, const SlSockAddr_t *to, SlSocklen_t tolen);
#endif


#if _SL_INCLUDE_FUNC(sl_Htonl )
_u32 sl_Htonl( _u32 val );

#define sl_Ntohl sl_Htonl  
#endif


#if _SL_INCLUDE_FUNC(sl_Htons )
_u16 sl_Htons( _u16 val );

#define sl_Ntohs sl_Htons   
#endif




#ifdef  __cplusplus
}
#endif 

#endif 


